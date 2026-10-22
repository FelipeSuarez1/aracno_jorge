---
status: aprovada
origem: [[20-Specs/S00-visao]] seção 3; [[30-Medicoes/dia-1]]
depende: [[20-Specs/S02-toolchain-hello-world]]
---
# S03 — Firmware base: laço de controle, servos e camada de segurança

## Objetivo
Um binário `aracno` que substitui o `spider` da Terasic: controla os 18 servos pela mesma FPGA, lê os sensores, roda um laço de controle de período fixo e **só move a aranha se a camada de segurança permitir**. Sem política ainda: o "cérebro" desta spec é uma máquina de estados mínima (parado ↔ andar para frente) usada só para testar.

## Fora de escopo
Política aprendida (S05), display/rosto (S04), pan/tilt (S06), teleoperação com gravação (S07), rootfs somente-leitura (S08).

## Fatos de entrada
- PWM: 4 registradores por canal, 20 ms, pulso 0,5–2,5 ms ↔ −90…+90°, `ABORT=1` desliga o pulso; canais `PWM17_BASE 0x0 … PWM0_BASE 0x110` na ponte lightweight — [[40-Fontes/spider-cd/DE0_NANO_SOC_SPIDER_LINUX_APP/CMotor.cpp]], `hps_0.h`, `TERASIC_PWM_EX.v`.
- Marcha, mapeamento perna/junta/servo, espelhamento dos motores 0–8 — `CSpider.cpp`, `CSpiderLeg.cpp`, apêndice do [[40-Fontes/spider-programming-guide]].
- Kernel 3.13 `PREEMPT_NONE`; `/dev/i2c-1` (sem pull-up), `/dev/spidev1.0`, `/dev/watchdog` (dw_wdt), `/dev/mem` — [[30-Medicoes/dia-1]].
- Corte do CPLD: < 5,9 V por 2 s ou > 65 W por 0,2 s; desliga DE0 junto — `MAXII_Power_Monitor.v`. O HPS **não lê** o INA230 sem mover R310/R311→R313/R314 na Servo Motor Card (SMK §2.5); fase 1 fica sem leitura de bateria.
- O `spider` original sobe pelo `/etc/inittab` e mapeia os mesmos registradores: os dois não podem rodar juntos.

## Hardware desta spec
BH1750 ×2 (0x23 e 0x5C via pino ADDR), VL53L1X ×2 (0x29; o segundo recebe novo endereço por XSHUT na inicialização), PIR HC-SR501, expansor **PCF8574** (0x20) para XSHUT ×2, PIR (entrada) e botão de parada lógica; pull-ups 4,7 kΩ no I2C1. Tudo no header LTC 2x7 (3,3 V, GND, `HPS_I2C1_SCLK/SDAT`).

## Arquitetura (C++11, sem exceções, sem alocação no laço)
```
firmware/src/
  hal/   mmap.cpp i2c.cpp gpio_pcf8574.cpp watchdog.cpp clock.cpp
  motor/ pwm.cpp (registradores) leg.cpp spider.cpp (marcha portada de CSpider/CSpiderLeg)
  sense/ bh1750.cpp vl53l1x.cpp pir.cpp  → struct Obs {lux_l, lux_r, dist_front_mm, dist_floor_mm, pir, tilt_x, tilt_y, t_since_stimulus, age_ms[]}
  safety/ safety.cpp  → Cmd filter(const Obs&, Cmd desejado): devolve Cmd permitido + motivo
  brain/  fsm_demo.cpp (esta spec) | policy.cpp (S05)
  app/    main.cpp (laço 50 Hz SCHED_FIFO + mlockall, log binário, watchdog)
```
Regras da camada de segurança (fixas, não configuráveis pela política):
| Condição | Ação |
|---|---|
| `dist_front_mm < D_STOP` (derivado da frenagem medida) | nenhum avanço; permite recuar/virar |
| `dist_floor_mm > F_EDGE` ou leitura inválida | parar; permite só recuar |
| sensor com `age_ms > 200` (obsoleto) ou erro I2C | parar + `ABORT` em todos os servos |
| `|tilt| > T_FALL` (acelerômetro ADXL345 em `/dev/i2c-0`, 0x53) | `ABORT` em todos os servos |
| botão de parada lógica (PCF8574) | `ABORT` + estado "travado" até reinício |
| processo morre / trava | watchdog reinicia o HPS; U-Boot reconfigura a FPGA → PWM zerados → servos soltos |
| ao sair (SIGTERM/SIGINT) | `ABORT` em todos os servos antes de encerrar |

## Critérios de aceite
- [PC] `tests/safety.cpp`: tabela de ≥ 30 casos (obs → cmd desejado → cmd permitido + motivo), incluindo limites exatos, sensor obsoleto, botão travado, recuperação. Passa com `make -C firmware test`.
- [PC] `tests/pwm.cpp`: ângulo→`HIGH_DUR` reproduz `CMotor::Move` (−90→25 000, 0→75 000, +90→125 000, espelhamento 0–8); fora da faixa satura.
- [PC] `tests/obs.cpp`: decodificação de quadros BH1750/VL53L1X a partir de bytes gravados e marcação de `age_ms`.
- [PC] Build estático limpo (`-Wall -Wextra -Werror`), binário < 300 kB.
- [LAB] Com `spider` desativado no `inittab` e **aranha suspensa** (pés sem tocar o chão), `aracno --demo stand` levanta e `aracno --demo walk 5` dá 5 passos; vídeo.
- [LAB] Jitter: histograma do período do laço em 10 min de marcha, p99 < 2 ms (10 % de 20 ms), gravado em `30-Medicoes/`.
- [LAB] Injeção de falha: desconectar o VL53L1X frontal durante a marcha → `ABORT` em < 200 ms (timestamp no log); `kill -STOP` → reinício pelo watchdog em < 5 s e servos soltos.
- [LAB] Botão de parada → servos soltos imediatamente; LED do DE0 indica estado "travado".

## Verificação
- [PC]: `make -C firmware test`; `tools/cross-build.sh`; saída colada.
- [LAB]: `tools/uart.py push` + `tools/uart.py run`; log binário convertido por `tools/log2csv.py`; vídeo e histograma anexados.

## Plano (aprovar antes de implementar)
1. `periferia` — portar `CMMap`, `CMotor`, `CSpiderLeg`, `CSpider` para `firmware/src/motor/` sem Bluetooth, sem `printf` no laço; manter constantes com comentário citando o arquivo original.
2. `periferia` — `hal/i2c.cpp` (ioctl `I2C_RDWR` com timeout), drivers BH1750, VL53L1X (sequência de init do ST API reduzida), PCF8574, ADXL345.
3. `núcleo` — `safety/safety.cpp` + `tests/safety.cpp` escritos **antes** do laço (TDD); constantes `D_STOP`, `F_EDGE`, `T_FALL` em `safety_limits.h` com `[A MEDIR]` até a medição de frenagem.
4. `núcleo` — `app/main.cpp`: laço 50 Hz, watchdog, log binário com CRC, tratamento de sinais, `--demo`.
5. `periferia` — `tools/log2csv.py`, `tools/inittab-aracno.sh` (troca `spider` por `aracno` no `/etc/inittab` com backup).
6. Revisão pelo agente `reviewer-seguranca` antes do primeiro teste na aranha; teste [LAB] com a aranha suspensa.

## Evidência
(preencher)
