---
status: aprovada
origem: conversa 2026-10-28 — jumpers não prendem no J17 (box header); decisão de usar o GPIO_0 da FPGA
depende: [[20-Specs/S03-firmware-base-e-seguranca]], [[20-Specs/S04-display-rosto-balao]]
---
# S08 — FPGA: display e periféricos no GPIO_0, PWM da cabeça na Servo Motor Card

## Objetivo
Recompilar o projeto Quartus da Terasic acrescentando, **sem alterar o que já existe**, um SPI master e GPIOs ligados ao header GPIO_0 (pinos abertos, jumpers firmes) e 2 controladores PWM nas portas livres da Servo Motor Card. Resultado: display ligado direto na FPGA (sem PCF8574) e cabeça pan/tilt sem PCA9685.

## Fora de escopo
- Qualquer mudança no componente HPS do Platform Designer (pinos, DDR, pontes) — mudaria o preloader (U-Boot SPL de 2014 no cartão), que não vamos regenerar.
- I2C dos sensores na FPGA: fase 2 desta spec, quando os sensores chegarem (decidir entre `altera_avalon_i2c` na FPGA e J17 com 4 fios).
- Watchdog (pendência S03b).

## Fatos de entrada
- Quartus Prime Lite **25.1std** em `~/altera_lite/25.1std` com Cyclone V (verificado 2026-10-28).
- Projeto original: `Program/DE0_NANO_SOC_SPIDER` do Spider CD (Quartus 14.0), cópia de trabalho fora do git em `~/dev/aracno_fpga_work/`. Fonte versionada em [[40-Fontes/spider-cd]].
- `DE0_Nano_SoC_golden_top.v`: os 18 PWM saem em **GPIO_1** (cabo flat da SMK); **GPIO_0 não é usado**; header Arduino não é usado.
- Mapa SMK ↔ GPIO_1: porta PWM*n* da SMK = `GPIO_1[n]` (apêndice do [[40-Fontes/spider-programming-guide]]); portas **3–8 livres** = `GPIO_1[3..8]`, já com level-shift para 5 V, 6 V e fusível na SMK.
- GPIO_0 (JP1): pinos 1–10 e 13–28, 31–40 = I/O 3,3 V; **11 = 5 V, 12 = GND, 29 = 3,3 V, 30 = GND** ([[40-Fontes/de0-nano-soc-user-manual]] Fig. 3-18).
- Display Adafruit ILI9341 aceita Vin 3–5 V e lógica 3,3 V (regulador + level-shifter na placa).

## Alocação proposta
| Função | Sinal FPGA | GPIO_0 pino | Observação |
|---|---|---|---|
| Display `CLK` | GPIO_0[0] | 1 | SPI master |
| Display `MOSI` | GPIO_0[1] | 2 | |
| Display `CS` | GPIO_0[2] | 3 | |
| Display `D/C` | GPIO_0[3] | 4 | PIO saída |
| Display `RST` | GPIO_0[4] | 5 | PIO saída |
| Display `MISO` | GPIO_0[5] | 6 | opcional |
| Display `Vin` / `GND` | — | 11 (5 V) / 12 | |
| XSHUT ToF frontal / chão | GPIO_0[6] / [7] | 7 / 8 | PIO saída (fase 2) |
| PIR / botão de parada | GPIO_0[8] / [9] | 9 / 10 | PIO entrada (fase 2) |
| Pan / tilt da cabeça | GPIO_1[3] / [4] | SMK porta 3 / 4 | 2 × `TERASIC_PWM_EX` novos (pwm18, pwm19) |

## Componentes novos no Platform Designer (ponte lightweight, endereços acima de 0x160)
- `spi_lcd`: `altera_avalon_spi` (SPI 3-wire), master, 8 bits, MSB primeiro, modo 0, SCLK **12,5 MHz** (50 MHz/4; o core só divide por pares e 20 MHz não é exato), 1 slave select.
- `pio_lcd`: `altera_avalon_pio`, 8 bits saída (D/C, RST, XSHUT ×2, reservas).
- `pio_in`: `altera_avalon_pio`, 4 bits entrada (PIR, ESTOP, reservas).
- `pwm18`, `pwm19`: `TERASIC_PWM_EX` (o mesmo IP do Spider, `ip/spider`).
- Endereços exatos gerados em `hps_0.h` por `sopc-create-header-files`; o firmware os lê do header (nunca número mágico).

## Critérios de aceite
- [PC] **Fase 0 (baseline):** projeto original, sem mudanças, gera `.rbf` no Quartus 25.1 sem erro. Endereços dos 18 PWM, LED_PIO, BUTTON_PIO e SYSID em `hps_0.h` idênticos ao original.
- [LAB] Fase 0: `.rbf` do baseline no cartão (original guardado como `soc_system.rbf.orig`), `aracno --demo stand --sensors 0x20` com SMK desligada completa os 35 passos lendo STATUS da FPGA (igual ao `.rbf` de fábrica).
- [PC] Fase 1: projeto com os componentes novos compila; `hps_0.h` novo mantém todos os endereços antigos e acrescenta os novos; timing fechado (sem slack negativo).
- [PC] Firmware: `hal/avalon_spi` + `hal/avalon_pio` com testes de host contra registradores falsos; `Ili9341` usa `SpiIo`/`CtrlPins` novos sem mudar o resto.
- [LAB] Fase 1: o `.rbf` novo mantém o `stand` funcionando (regressão dos 18 PWM); `aracno --face-demo` mostra as 7 expressões; tempo de quadro inteiro < 200 ms.
- [LAB] Fase 1: `aracno --head-demo` move pan e tilt nas portas 3/4 da SMK.
- Volta ao original documentada: copiar `soc_system.rbf.orig` sobre `soc_system.rbf` na p1 do cartão.

## Riscos
- Platform Designer 25.1 pode atualizar o componente HPS ao abrir um `.qsys` de 14.0 e mudar algo que dependa do preloader antigo → mitigação: fase 0 testa exatamente isso antes de qualquer acréscimo.
- `altera_avalon_spi` não tem FIFO: um byte por acesso à ponte. Estimativa `[INFERÊNCIA]` 0,3–0,6 µs por byte → 50–100 ms por quadro. Se não bastar, trocar por um streamer de pixels próprio (fase 3).
- MG996R da cabeça puxam até ~2,5 A de stall cada, somando-se ao orçamento de 65 W do CPLD.

## Plano
1. `núcleo` — fase 0: regenerar e compilar o baseline; comparar `hps_0.h`; teste [LAB] com o `.rbf` novo.
2. `núcleo` — fase 1: acrescentar componentes, pinos (`.qsf`) e ligações no `golden_top.v`; compilar.
3. `periferia` — firmware: drivers Avalon SPI/PIO + testes; `Ili9341` sobre eles; `--head-demo`.
4. Revisão `reviewer-seguranca` (FPGA + firmware) antes do [LAB].
5. [LAB] regressão do `stand`, display, cabeça.

## Evidência
### 2026-10-28 — fase 0 [PC]: baseline no Quartus 25.1
- `qsys-generate` (Platform Designer 25.1) do `soc_system.qsys` original: ok em 1 min 43 s; avisos = IPs atualizados de 14.0 para 25.1 e clock h2f_user0 97,37 MHz (pedido 100; não usado pelo projeto).
- `quartus_sh --flow compile`: 0 erros, 12 min 59 s. 2 avisos críticos = 72 pinos da DDR3 do HPS sem posição no `.qsf` (igual ao original).
- `.pin` novo × original (2015): **262 sinais, todos na mesma posição** (inclui os 72 da DDR3).
- `hps_0.h` novo × original: **23 endereços idênticos** (PWM0..17, LED_PIO, BUTTON_PIO, DIPSW_PIO, JTAG_UART, SYSID).
- `quartus_cpf -c` (sem compressão, como o `sof_to_rbf.bat` da Terasic): `soc_system_baseline.rbf` 4 244 820 B (= tamanho do `.rbf` de fábrica), SHA-256 `affcefbb…0f96b76`. Cópia de trabalho em `~/dev/aracno_fpga_work/baseline` (fora do git).
- Montagem física já feita pelo Luiz: display no GPIO_0 (JP1) conforme a tabela de alocação; orientação pela serigrafia "11" na 6ª linha (coluna da protoboard = ímpares, pino 1 junto ao parafuso).
### 2026-10-28 — fase 0 [LAB] ✅
- Cartão: original guardado como `soc_system.rbf.orig` (SHA-256 `0c6f76f3…`), baseline copiado como `soc_system.rbf` (`affcefbb…`).
- Boot: U-Boot lê 4 244 820 B, FPGA configura, kernel 3.13 sobe normalmente.
- `aracno --demo stand --sensors 0x20 --max-s 15` (SMK desligada): STAND completo, **0 timeouts de passo** nos 18 PWM, 750 ciclos, p50 = p99 = 20 000 µs, max 20 007 µs, `exit=0`.
- Conclusão: o projeto da Terasic recompilado no Quartus 25.1 é equivalente ao de fábrica. Liberada a fase 1.
