---
status: aprovada
origem: [[10-Decisoes/ADR-011-pi-ros-visao-na-fpga]]
depende: [[20-Specs/S08-fpga-gpio0-display-cabeca]], [[20-Specs/S10-enlace-pi-de0]]
---
# S11 — "Medula" na FPGA: olhar que segue, reflexo de susto e watchdog de servos

## Objetivo
A FPGA ganha um bloco `medula` (Verilog próprio, no Platform Designer) que, a partir do mapa de profundidade da R200 (160×120, 8 bits), faz três coisas sem depender do ARM:
- **A — olhar que segue:** a cabeça pan/tilt acompanha a pessoa (PWM próprio da cabeça, com limite de velocidade);
- **B — reflexo de susto:** se a pessoa se aproxima de repente, a cabeça sobe ao limite de tilt em hardware, com latência medida;
- **E — watchdog de servos:** se o ARM parar de dar sinal de vida, os 20 PWM de servo são cortados na saída.

Os PWM da Terasic (`TERASIC_PWM_EX`) ficam **inalterados**; a `medula` só se coloca entre eles e os pinos.

## Fora de escopo
- Reação das pernas ao reflexo (o reflexo só mexe na cabeça).
- Enlace de rede Pi↔DE0 ([[20-Specs/S10-enlace-pi-de0]]).
- Rede neural de detecção.
- HC-SR04.

## Fatos de entrada
- FOV de profundidade da R200: **59° × 46°** (datasheet Intel 334616-001, Tabela 3-2). Formatos e profundidade medidos no Pi: [[30-Medicoes/2026-11-06-raspberry-pi-4]].
- PWM `TERASIC_PWM_EX`: **50 000 ticks por ms**, centro **75 000** ticks (`firmware/src/motor/pwm.h`). O PWM fica em 0 até a primeira escrita de `HIGH_DUR == 75000`; o ABORT só vale no fim do período (latch `AbortEnable` em `PwmEnd`), em `docs/40-Fontes/spider-cd/DE0_NANO_SOC_SPIDER/ip/spider/TERASIC_PWM_EX.v`. Pulso máximo de um servo no firmware: 2,5 ms (`HIGH_MAX`, `firmware/src/motor/pwm.h`).
- Uso atual da FPGA: **10 706 / 15 880 ALMs** e **36 / 270 M10K** (`~/dev/aracno_fpga_work/build/DE0_Nano_SoC_golden_top.fit.summary`, build S09).
- Limites de software da cabeça ([[20-Specs/S06-cabeca-pan-tilt]]): pan ±60°, tilt ±25°; 11,11 µs/° dá os limites fixos do RTL (ver Contrato).
- O `reset_n` do Qsys nunca é ativado depois da configuração (`.reset_reset_n(1'b1)` em `fpga/DE0_Nano_SoC_golden_top.v`); o U-Boot reconfigura a FPGA a cada boot.
- Padrão de RAM em 4 lanes sem reset (inferência de M10K), leitura síncrona e `_hw.tcl`: `fpga/cam/cam_capture.v` e `fpga/cam/cam_capture_hw.tcl` (S09).
- Não há divisor no projeto para reaproveitar.
- `[INFERÊNCIA]` Sentido da cabeça: µs maior = pan para a direita e tilt para cima. Não foi medido; se estiver invertido, basta o sinal dos ganhos (`PAN_GAIN`/`TILT_GAIN` têm sinal) e a fórmula do tilt, mas o contrato abaixo fixa os sinais como escritos.
- `[INFERÊNCIA]` A R200 fica fixa no corpo da aranha, olhando para a frente; a imagem não gira com a cabeça. Por isso "seguir" é controle em malha aberta por posição (erro de pixel → ângulo → µs), sem realimentação pela imagem.

## Design
### Visão geral
`medula` é um componente Avalon-MM no barramento LW (base `0x20000`) com a conduit `pwm` (`pwm_in[19:0]` vindo dos 20 PWM da Terasic, `pwm_out[19:0]` para os pinos). No topo, os `assign GPIO_1[...]` passam a ler `pwm_out` (`pwm_safe`), com o mesmo mapeamento de pinos atual.

O ARM (ou o enlace da S10) escreve um quadro de profundidade de 160×120 bytes na RAM FRAME, escreve GO em CTRL e lê o resultado (alvo, profundidade, caixa, reflexo, alvo da cabeça). A FPGA processa 1 pixel por ciclo (19 200 ciclos ≈ 384 µs a 50 MHz), mantém o fundo (BG) em RAM, e atualiza sozinha o PWM da cabeça.

### Detecção (algoritmo por quadro)
Primeiro plano = pixel mais perto que o fundo em pelo menos `FG_DELTA`, com profundidade válida (≠ 0) e erosão por 2 vizinhos (esquerda e acima). O fundo acompanha a cena 1 passo por quadro (e 1 a cada 16 quadros onde há primeiro plano, para a pessoa parada não virar fundo depressa). O alvo é o centroide dos pixels de primeiro plano; a profundidade informada é a mínima e a média. Detalhes bit a bit no Contrato.

### Olhar que segue
Alvo da cabeça a partir do centroide (`cx`, `cy`), com os ganhos em µs por pixel × 16 (ponto fixo, 4 bits fracionários):
- `PAN_GAIN  = 59° / 160 px × 11,11 µs/° × 16 = 65,6 → 66`
- `TILT_GAIN = 46° / 120 px × 11,11 µs/° × 16 = 68,2 → 68`

(59/160 = 0,36875 °/px; × 11,11 = 4,097 µs/px; × 16 = 65,6. 46/120 = 0,3833 °/px; × 11,11 = 4,259 µs/px; × 16 = 68,2.) Os valores `66` e `68` são os resets do contrato. Exemplo conferido no testbench: `cx = 70` → `pan_t = 1500 + asr4((70-80)·66) = 1500 - 42 = 1458`; `cy = 55` → `tilt_t = 1500 - asr4((55-60)·68) = 1500 + 22 = 1522`.

O PWM da cabeça é próprio: período de 20 ms (1 000 000 ciclos) e a largura só muda no início do período, com limite de `SLEW_US` µs por período (reset 26 µs ≈ 2,3°/período ≈ 117°/s). `HEAD_OVERRIDE = FOLLOW_EN || reflex_active`: enquanto vale, os canais 18/19 vêm do PWM da cabeça; senão, do PWM da Terasic.

### Reflexo de susto
Dispara se a pessoa estava a uma distância e, de um quadro para o seguinte, a profundidade mínima caiu `DROP` ou mais e agora está em `NEAR` ou menos (valores em unidades de 16 mm). Ao disparar, o tilt vai direto (sem slew) ao limite superior por `HOLD_MS`, e depois volta ao alvo normal com slew. O contador `REFLEX_COUNT` e a latência (`LATENCY`, em ciclos de 50 MHz) permitem comparar o caminho em hardware com o caminho por software (o modelo C++ em `firmware/src/medula/model.cpp`).

### Watchdog de servos
Contador de ms próprio. O firmware faz `WD_KICK` no laço de controle; se passar `WD_TIMEOUT_MS` sem kick, `TRIPPED` fica em 1 e os 20 canais de `pwm_out` vão a 0 sem deformar pulso (fim do pulso corrente, ou corte forçado em 125 000 ciclos se a fonte estiver presa em 1). Só um CLEAR (ou desligar e religar o ENABLE) restaura.

### Firmware (`firmware/src/medula/`, `firmware/src/app/`)
- `model.h/.cpp`: modelo bit a bit do algoritmo (também é o "caminho por software" do reflexo).
- `scenes.h/.cpp`: cenas de teste 0..7 geradas em código.
- `medula.h/.cpp`: driver sobre `hal::RegIo`; **nenhuma escrita antes de conferir o ID** (num `.rbf` antigo, 0x20000 cai no pwm17).
- `main.cpp`: `attach` + `wd_enable(100)` imediatamente antes do laço; `wd_kick()` junto do `wd.kick()`; `wd_disable()` na saída depois de `abort_all` e 40 ms.
- Modos de bancada `--medula-id`, `--medula-test`, `--medula-wd` (nenhum escreve nos registradores PWM).

## Contrato FPGA ↔ firmware
(É também a especificação do RTL e do modelo.)

Componente `medula`: base LW `0x20000`, span `0x20000`, endereço de palavra de 15 bits, readLatency 1, ID `0x4D454431` ("MED1").

Registradores nas palavras 0x00–0x12. **Fora das faixas listadas (palavras 0x13–0x1FFF, 0x32C0–0x3FFF, 0x52C0–0x7FFF): leitura 0 e escrita ignorada.** Decodificação estrita (sem espelhos).

|Palavra|Nome|Escrita|Leitura|Reset|
|---|---|---|---|---|
|0|CTRL|bit0 GO (pulso; ignorado se BUSY), bit1 BG_RESET (só vale junto com GO, para o quadro desse GO), bit2 FOLLOW_EN, bit3 REFLEX_EN. Bits 2/3 valem a qualquer momento, inclusive durante BUSY|`{FOLLOW_EN, REFLEX_EN}` nos bits 2/3; bits 0/1 = 0|0|
|1|STATUS|W1C: bit3 REFLEX, bit6 WRITE_DROPPED|bit0 DONE (zera no GO aceito), bit1 BUSY, bit2 TARGET, bit3 REFLEX (fixa), bit4 WD_TRIPPED, bit5 HEAD_OVERRIDE, bit6 WRITE_DROPPED (fixa)|0|
|2|FRAMES|—|quadros processados (dá a volta em 2³²)|0|
|3|TARGET_XY|—|[7:0] cx, [15:8] cy, [31:16] count|0|
|4|TARGET_DEPTH|—|[7:0] min_depth, [15:8] mean_depth|0|
|5|ID|—|0x4D454431|—|
|6|BBOX|—|[7:0] x0, [15:8] y0, [23:16] x1, [31:24] y1|0|
|7|LATENCY|—|ciclos de 50 MHz entre o GO aceito e o DONE|0|
|8|THRESH|[7:0] FG_DELTA, [23:8] MIN_COUNT|idem|FG_DELTA 8, MIN_COUNT 40|
|9|REFLEX_CFG|[7:0] NEAR, [15:8] DROP, [31:16] HOLD_MS|idem|38, 4, 1500|
|10|PAN_GAIN|[15:0] com sinal, µs/px × 16|idem|66|
|11|TILT_GAIN|[15:0] com sinal|idem|68|
|12|SLEW_US|[15:0] µs por período de 20 ms|idem|26|
|13|HEAD_US|—|[15:0] pan_us atual (com slew), [31:16] tilt_us atual|1500/1500|
|14|WD_CTRL|toda escrita define ENABLE = bit0 (ENABLE=1 zera o contador; ENABLE=0 desliga e zera TRIPPED); bit1 CLEAR só vale com bit0 = 1: zera TRIPPED e o contador|bit0 ENABLE, bit1 TRIPPED|0|
|15|WD_KICK|qualquer valor zera o contador|ms desde o último kick (satura em 65535)|0|
|16|WD_TIMEOUT_MS|[15:0], limitado a 20..1000|valor efetivo|100|
|17|REFLEX_COUNT|—|número de reflexos desde o power-up|0|
|18|HEAD_TARGET|—|[15:0] pan_t, [31:16] tilt_t efetivo (= TILT_MAX_US se houve reflexo **neste** quadro); travados no DONE|1500/1500|
|0x2000–0x32BF|FRAME|palavra 0x2000+w (w < 4800) = pixels 4w..4w+3; pixel p no byte p%4 (little-endian). Ignorado se BUSY, e nesse caso marca WRITE_DROPPED|o mesmo; 0 se BUSY|—|
|0x4000–0x52BF|BG|—|fundo, mesmo empacotamento (w < 4800); 0 se BUSY|0 no power-up no hardware (M10K sem MIF); indefinido em simulação até o primeiro BG_RESET (o TB não lê BG antes do F0)|

THRESH, REFLEX_CFG, PAN_GAIN, TILT_GAIN e SLEW_US são copiados para registradores internos no GO aceito (mudanças durante BUSY só valem no quadro seguinte). Reset ≡ valor de power-up via `initial`/inicializador do `reg`; todo registrador de estado tem inicializador. A entrada `reset_n` existe na interface mas não é usada: o `reset_n` do Qsys nunca é ativado depois da configuração (`.reset_reset_n(1'b1)` em `DE0_Nano_SoC_golden_top.v`); o U-Boot reconfigura a FPGA a cada boot.

Constantes fixas no RTL (`localparam`), limites de software da S06 (pan ±60°, tilt ±25°) em 11,11 µs/°: `PAN_MIN_US 833`, `PAN_MAX_US 2167`, `TILT_MIN_US 1222`, `TILT_MAX_US 1778`, `CENTER_US 1500`; período 20 000 µs = 1 000 000 ciclos; pulso máximo possível 2167 µs = 108 350 ciclos.

**Algoritmo por quadro** (idêntico no RTL e no modelo C++; W=160, H=120, N=19200, `fi` = valor de FRAMES no GO):
1. Para cada p em ordem (x = p%160, y = p/160), com `cur` = FRAME[p] e `bg` = BG[p] antes da atualização:
   - `fg_raw = cur != 0 && bg != 0 && bg > cur && (bg - cur) >= FG_DELTA`
   - `fg_clean = fg_raw && fg_raw(x-1,y) && fg_raw(x,y-1)`; vizinho fora da imagem = 0.
   - Novo BG[p], na primeira regra que valer: (1) BG_RESET → `cur`; (2) `bg == 0` → `cur`; (3) `cur == 0` → `bg`; (4) `fg_raw` → `(fi & 15) == 0 ? step(bg,cur) : bg`; (5) senão → `step(bg,cur)`. `step(b,c) = b + (c > b) - (c < b)`.
   - Se `fg_clean`: `count++`, `sum_x += x`, `sum_y += y`, `sum_d += cur`, `min_d = min(min_d, cur)`, `bbox = min/max(x,y)`. Larguras: count 16 bits, sum_x/sum_y/sum_d 24 bits.
2. `target = count >= MIN_COUNT`. Se target: `cx = sum_x / count`, `cy = sum_y / count`, `mean_depth = sum_d / count` (divisão inteira truncada), `min_depth = min_d`, bbox preenchido. Senão `cx = cy = min_depth = mean_depth = bbox = 0`; `count` é sempre informado.
3. Alvo da cabeça: se target, `pan_t = clamp(1500 + asr4((cx - 80) * PAN_GAIN), 833, 2167)` e `tilt_t = clamp(1500 - asr4((cy - 60) * TILT_GAIN), 1222, 1778)`, com `asr4` = deslocamento aritmético de 4 bits (floor). Senão: manter o último `pan_t`/`tilt_t` (power-up 1500/1500).
4. Reflexo: dispara se `REFLEX_EN && target && prev_target && (prev_min >= min_depth + DROP) && min_depth <= NEAR` (a soma em 9 bits, **nunca** subtração sem sinal). `prev_target`/`prev_min` são do quadro anterior e são atualizados a todo quadro, com ou sem REFLEX_EN. Ao disparar: REFLEX=1, REFLEX_COUNT++, `reflex_active` por HOLD_MS (contador de ms com prescaler próprio; recarrega se disparar de novo); o alvo de tilt efetivo vira `TILT_MAX_US` enquanto `reflex_active`. HEAD_TARGET[31:16] recebe `TILT_MAX_US` só se o reflexo disparou neste quadro; senão `tilt_t`.
5. DONE=1, FRAMES++, LATENCY e HEAD_TARGET travados.

**Cabeça (PWM próprio)**
- Contador de período de 1 000 000 ciclos. No início de cada período: `pan_us += clamp(pan_t - pan_us, ±SLEW_US)` e o mesmo para tilt com o alvo efetivo; com `reflex_active`, o tilt vai direto ao alvo (sem slew); ao fim do `reflex_active`, volta ao `tilt_t` com slew normal; depois o clamp nos limites fixos. A largura só muda no início do período.
- Saída `head_pwm[k]` alta enquanto `tick < us × 50`.
- `HEAD_OVERRIDE = FOLLOW_EN || reflex_active`. Quando FOLLOW_EN passa de 0 para 1, `pan_us = tilt_us = 1500`.

**Saídas PWM, troca sem pulso deformado** (os períodos da Terasic não estão em fase com o do `medula`; a Terasic só aplica ABORT no fim do período pelo mesmo motivo):
- Canais 0–17: `src[k] = pwm_in[k]`. Canais 18/19: `src[k] = sel_head[k] ? head_pwm[k-18] : pwm_in[k]`, com `sel_head[k]` registrado que só muda para o valor de `HEAD_OVERRIDE` quando `pwm_in[k] == 0 && head_pwm[k-18] == 0`.
- `gate[k]` registrado, vale `!WD_TRIPPED`, e só muda quando `src[k] == 0` **ou** quando `src[k]` está em 1 há ≥ 125 000 ciclos (`HIGH_MAX` = 2,5 ms, `firmware/src/motor/pwm.h`; contador de 17 bits por canal, zerado quando `src[k] == 0`). Isso cobre uma fonte presa em 1 (ex.: Terasic com `total_dur = 0` depois do `init`: `tick` fica em 0 e a saída vale sempre). `pwm_out[k] <= gate[k] & src[k]` (registrado, 1 ciclo de atraso; as duas bordas atrasam igual).
- Consequência: após TRIPPED, cada canal zera em ≤ 125 000 ciclos (fim do pulso corrente, ou corte forçado se a fonte estiver presa); nenhuma troca produz pulso fora de [833, 2167] µs nos canais 18/19.

**Watchdog**
- Prescaler **próprio** de 50 000 ciclos conta ms em `wd_ms`. KICK, CLEAR e a escrita com ENABLE=1 zeram `wd_ms` **e** o prescaler (disparo exatamente entre TIMEOUT e TIMEOUT+1 ms).
- Com ENABLE e `wd_ms >= WD_TIMEOUT_MS`, TRIPPED=1 e fica fixo. ENABLE=0 desliga e zera TRIPPED.

## Critérios de aceite
- [PC] `fpga/medula/sim.sh` imprime `TB OK: medula …` (exit 0) e cada uma das 5 mutações do RTL faz o testbench falhar (seção Verificação).
- [PC] `make -C firmware test` → `medula: 0 falhas`, `testes ok`; a mutação do modelo (`cx - 80` → `cx - 79`) é acusada; `qsys: 30 endereços … 0 falhas`.
- [PC] Build `fpga/build.sh ~/dev/aracno_fpga_work/s11`: fit ok; pinos de usuário inalterados em relação ao build S09; slack de setup e hold ≥ 0 nos caminhos de/para `*u0|medula:*`.
- [PC] `tools/cross-build.sh` produz ELF ARM estático.
- [LAB] `./aracno --medula-test` → `0 divergências`, com `hw ≈ 390 µs` por quadro (LATENCY entre 19 200 e 19 400 ciclos).
- [LAB] `./aracno --medula-wd` dispara entre **100 e 150 ms** (critério novo da S11; a S03 exige "servos soltos em < 5 s" e a S00 "< 1 s", ambos cobertos).
- [LAB] `stand` + `kill -STOP` no processo → servos soltos em ≤ 150 ms.
- [LAB] `--face-demo 6` sem regressão.

## Verificação
Rodar em `/home/luis/dev/aracno_jorge`. O orquestrador executa cada item ao fim do passo correspondente.

1. **Passo 4 — RTL**: `fpga/medula/sim.sh` → `TB OK: medula …`, exit 0 (Questa FSE em `~/altera_lite/25.1std/questa_fse` e `LM_LICENSE_FILE`, com padrão no script). Mutações, uma de cada vez, restauradas depois; cada uma tem de fazer o TB falhar:
   1. tirar o termo `fg_raw(x,y-1)` da erosão → count ≠ 551;
   2. trocar `(fi & 15) == 0` por `== 1` → BG(65,50) não vira 149 em F16;
   3. aplicar o slew também com `reflex_active` → tilt após F3 = 1548, não 1778;
   4. `gate[k]` sem olhar TRIPPED (nunca fecha) → o teste do watchdog falha;
   5. reflexo com `prev_min - min_depth >= DROP` em 8 bits → F4 dispara (REFLEX_COUNT 2).
2. **Passo 6 — host**: `make -C firmware clean && make -C firmware test` → `medula: 0 falhas`, `testes ok` (qsys ainda "29 endereços" até o passo 5). Mutação no modelo: trocar `cx - 80` por `cx - 79` → `tests/medula.cpp` acusa pan ≠ 1458 em F1 (1462). Restaurar.
3. **Passo 5 — build FPGA**: `fpga/build.sh ~/dev/aracno_fpga_work/s11` sai com 1 na primeira vez (`hps_0.h` novo); atualizar `fpga/hps_0.h`, aplicar `tests/qsys.cpp` e rodar `make -C firmware test` → `qsys: 30 endereços … 0 falhas`. `grep -E 'MEDULA_BASE 0x20000|MEDULA_SPAN 131072' fpga/hps_0.h` → 2 linhas. Pinos inalterados: extrair `nome pino` dos sinais de usuário do `.pin` de `s11` e de `~/dev/aracno_fpga_work/build/DE0_Nano_SoC_golden_top.pin` (mesmo awk da S09) → nenhuma diferença. `.fit.summary`: fit ok. `.sta.rpt`: slack de setup e hold ≥ 0 em todos os caminhos de/para `*u0|medula:*`; caminhos negativos fora do `medula` vão para a Evidência.
4. **Passo 7 — cross**: `tools/cross-build.sh` → `ELF 32-bit … ARM … statically linked`.
5. **Passo 8**: `reviewer` termina com APROVADO.
6. **[LAB]** (Luiz, por último): `--medula-test` → `0 divergências` com `hw ≈ 390 us`; `--medula-wd` → 100 ≤ X ≤ 150 ms; `--face-demo 6` como antes.

### Cenas e sequência do testbench
Cenas geradas em Verilog (base 150 salvo indicação): **S0**: tudo 150. **S1**: retângulo x∈[60,79], y∈[40,69] em 50; ruído de 1 pixel em 50 em (10+7k, 100), k=0..19. **S2**: S1 com o retângulo em 40. **S3**: idem em 34. **S4**: idem em 36. **S5**: x<10 em 0, resto 150. **S6**: S1 com x<10 em 100. **S7**: quadrado x∈[100,104], y∈[20,24] em 50.

Sequência (antes do F1: FOLLOW_EN=1, REFLEX_EN=1, HOLD_MS=30, WD desligado):

|Quadro|Cena|Esperado (lidos após DONE)|
|---|---|---|
|F0|S0 + BG_RESET|TARGET 0, count 0|
|F1|S1|count 551, cx 70, cy 55, min 50, mean 50, bbox (61,41,79,69), REFLEX 0, HEAD_TARGET 1458/1522. Após a próxima borda de subida de `pwm_out[18]` + 2 ciclos: HEAD_US pan 1474, tilt 1522|
|F2|S2|min 40, REFLEX 0|
|F3|S3|REFLEX 1, REFLEX_COUNT 1, HEAD_TARGET 1458/1778. Na próxima borda de `pwm_out[18]` + 2: HEAD_US pan 1458, tilt 1778 (sem slew)|
|F4|S4|REFLEX_COUNT 1 (afastou: 34 ≥ 36+4 é falso)|
|F5..F15|S3|REFLEX_COUNT 1 (36 ≥ 34+4 é falso). Após F15: BG(65,50) = 150|
|F16|S3|BG(65,50) = 149 (`fi & 15 == 0`)|
|F17|S5 + BG_RESET|TARGET 0|
|F18|S6|count 551, cx 70, cy 55, BG(5,5) = 100, BG(65,50) = 150, REFLEX_COUNT 1|
|F19|S7|TARGET 0, count 16, HEAD_TARGET 1458/1522 (mantido)|

Regra que quem alterar a sequência deve manter: a checagem "HEAD_TARGET manteve o valor anterior" só vale em quadros sem alvo que **não** venham logo depois de um reflexo (depois do reflexo o tilt efetivo cai de 1778 para `tilt_t`, o que daria falso DIVERGE); a sequência F0..F19 não tem esse caso.

Demais checagens do testbench:
- `pwm_in` estimulado por um gerador no TB: canais 0–17 com pulso de 1500 µs; 18/19 com 1000 µs; período 20 ms, fase deslocada 7 ms em relação ao reset.
- Depois de F19: esperar 15 períodos (reflexo expirado há > 30 ms; tilt volta 1778→1522 em 10 períodos de 26 µs) e conferir HEAD_US = 1458/1522. Depois FOLLOW_EN=0, esperar STATUS bit5 = 0 **e mais 1 período (1 000 000 ciclos)**, porque `sel_head` só troca com as duas fontes em 0; então conferir `pwm_out[17:0] == pwm_in[17:0]` atrasado 1 ciclo e `pwm_out[19:18] == pwm_in[19:18]`. Um monitor contínuo no TB confere que nenhum pulso em 18/19 durante toda a simulação ficou fora de [833×50, 2167×50] ciclos.
- Largura de pulso de `pwm_out[18]` = `pan_us × 50` ciclos com FOLLOW_EN=1.
- Escrita no FRAME durante BUSY → WRITE_DROPPED; LATENCY entre 19 200 e 19 400 ciclos (o valor exato vai para a Evidência); palavra 0x0100 e 0x3300 leem 0; ID.
- Watchdog: WD_TIMEOUT_MS=20, kicks a cada 10 ms por 60 ms → nunca dispara; depois o gerador prende `pwm_in[5]` em 1; sem kick → TRIPPED em 20–21 ms e `pwm_out == 0` (os 20 canais, inclusive o 5) em ≤ 125 000 ciclos depois; CLEAR restaura (pwm_out volta a seguir pwm_in nos canais com pulso); ENABLE=0 zera TRIPPED; WD_TIMEOUT_MS=5 escrito → lê 20.

### Roteiro de lab (Luiz, por último)
1. Cartão no PC, montar.
2. Copiar `soc_system_s11.rbf` como `soc_system.rbf`, mantendo o `.orig`; `sync`.
3. `udisksctl unmount -b /dev/mmcblk0p1` (**nunca** `power-off`).
4. DE0 na fonte de 5 V, SMK desligada.
5. `python3 tools/uart.py push firmware/build/aracno /home/root/aracno`.
6. `./aracno --medula-id`, `--medula-test`, `--medula-wd`, `--face-demo 6`.
7. Com bateria carregada: `stand` e `kill -STOP` no processo → servos soltos em ≤ 150 ms (critério S11; cobre S03 "< 5 s" e S00 "< 1 s").

## Riscos
- **Fit falhar ou slack negativo dentro do `medula`:** registrar uma etapa de pipeline entre o cálculo de `fg_raw` e a acumulação, e recompilar. Se faltar ALM: tirar o `cam_capture`/`pio_cam*` do build num commit `fpga: retira S09 do build para caber a S11` (apagar a linha do s09 no `build.sh` e as constantes `CAM_CAPTURE_*`/`PIO_CAM*` de `qsys.h` e `tests/qsys.cpp`), deixando o código-fonte da S09 no repo (já marcada `suspensa`).
- **Inferência de M10K das lanes de BG falhar** (lógica explodir no `.fit.summary`): escrever cada lane num `always @(posedge clk)` próprio da forma `if (we[k]) laneK[waddr] <= wdata[k];`, sem `case` nem `initial`, e manter a leitura síncrona `rdata[k] <= laneK[raddr]`.
- **RAM BG não iniciar em zero no power-up:** o firmware sempre manda o primeiro quadro com BG_RESET; o contrato já exige isso. Sem outro tratamento.
- **Quartus 25.1 não aceitar o mesmo SDC para as três entradas de clock:** manter só `FPGA_CLK1_50` (clock do Qsys) e `FPGA_CLK2_50` (LED de heartbeat).
- **Edições em `CLAUDE.md` ou em `docs/10-Decisoes/` vetadas pelo Luiz:** não editar esses arquivos; deixar a ADR-011 e a tabela nova da ADR-008 em `docs/00-Inbox/` como propostas; o `reviewer` do passo 8 trata a ADR-008 proposta (Sonnet escreve, Opus revisa) como regra vigente e o Luiz acompanha os passos 4–7.

## Plano
Todos os passos são `núcleo` (ADR-008): código de FPGA, de segurança (watchdog) ou de controle de servos; Sonnet escreve, Opus revisa; nenhum commit de RTL ou de código de segurança antes do APROVADO do passo 8.
4. `núcleo` — `fpga/medula/medula.v` (+ `medula_div`), `tb_medula.v`, `sim.sh`; mutações do RTL.
5. `núcleo` — `fpga/medula/medula_hw.tcl`, `fpga/s11_add_medula.tcl`, topo (`pwm_raw`/`pwm_safe`), SDC, `build.sh`, `README.md`; compilar em `~/dev/aracno_fpga_work/s11`; `hps_0.h`; `tests/qsys.cpp` (30 endereços).
6. `núcleo` — firmware: `MEDULA_BASE`/`MEDULA_SPAN` em `qsys.h`, `medula/model`, `medula/scenes`, `medula/medula` (driver), `tests/medula.cpp`. Pode rodar em paralelo com o 4; não toca `tests/qsys.cpp`.
7. `núcleo` — firmware: watchdog no laço de `main.cpp` e modos `--medula-id`, `--medula-test`, `--medula-wd`.
8. `núcleo` — revisão independente (`reviewer`, opus, critérios de `reviewer-seguranca.md`) dos passos 4–7; REPROVADO → correção por `task` → rechecagem dos bloqueios.
9. `núcleo` — evidência [PC] e roteiro de lab nesta spec.

Ordem de commit: 4, 6, 5, 7 (depois do APROVADO do passo 8), depois 9.

## Pendente
(Sugestões não bloqueantes do passo 8 entram aqui.)

## Evidência
(preencher)
