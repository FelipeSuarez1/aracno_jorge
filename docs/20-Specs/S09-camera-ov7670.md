---
status: aprovada
origem: [[00-Inbox/camera-ov7670-e-ble]]; depende do aceite de [[10-Decisoes/ADR-010-camera-na-fpga]]
depende: [[20-Specs/S08-fpga-gpio0-display-cabeca]]
---
# S09 — Câmera OV7670 na FPGA: captura, configuração e "o que ela vê" no display

## Objetivo
A aranha passa a ter visão de baixa resolução: a FPGA captura quadros da OV7670 (ligada no GPIO_0), o ARM configura a câmera e lê os quadros, e o display mostra o que ela vê. Fase 2: detecção de movimento como entrada da política (resolve "pessoa vs. parede").

## Fora de escopo
Rede neural de detecção de pessoa (fase 3, se a fase 2 não bastar). Cor (a fase 1 usa só luminância Y). Gravar vídeo.

## Fatos de entrada
- Módulo OV7670 sem FIFO, 18 pinos, ligado no JP1 linhas 7–15 (tabela em [[00-Inbox/camera-ov7670-e-ble]]), montado pelo Luiz em 2026-10-28.
- Referência de software: https://github.com/danielmpinto/pico-ov7670-driver (MIT), adaptação do Adafruit_CircuitPython_OV7670 (MIT): lista de registradores de init, `frame_control` por resolução, SCCB a 100 kHz com pull-up 4,7 kΩ. Datasheet OV7670 (2006): https://web.mit.edu/6.111/www/f2016/tools/OV7670_2006.pdf
- SCCB: endereço 0x42/0x43 (escrita/leitura); PID (0x0A) = 0x76, VER (0x0B) = 0x73 (datasheet) — teste de presença.
- Memória interna do 5CSEMA4: ~2,7 Mbit (M10K); o design S08 usa pouco. Quadro QQVGA só Y = 160×120 = 19 200 B.
- Ponte LW: janela de 2 MiB; leitura 32 bits por acesso.

## Design
### FPGA (componente novo `cam_capture`, Verilog próprio + `_hw.tcl`, no Platform Designer em 0x10000, span 32 KiB)
- **XCLK**: 50 MHz / 4 = 12,5 MHz, gerado no topo (contador), saída GPIO_0[15]. Com `CLKRC` da câmera dividindo por 2, PCLK ≈ 6,25 MHz.
- **Captura**: VSYNC, HREF, PCLK e D[7:0] sincronizados no domínio de 50 MHz (2 flip-flops); borda de subida de PCLK detectada por amostragem (8 amostras por período de PCLK). Formato YUV422: guarda só os bytes Y (um sim, um não). Ao "armar", espera o início do quadro (VSYNC), grava 160×120 bytes numa RAM M10K de porta dupla e levanta `done`.
- **Registradores** (Avalon-MM, 32 bits): `CTRL` (bit0 arm), `STATUS` (bit0 done, bit1 overflow, contadores de linhas/pixels do último quadro para diagnóstico), `FRAMES` (contador); RAM do quadro mapeada a partir do offset 0x4000 (4 pixels por palavra).
- **SCCB e controle**: `pio_cam` (saída) bit0 SIOC puxa para 0, bit1 SIOD puxa para 0 (open-drain: 1 = drena, 0 = solta), bit2 RESET_n, bit3 PWDN; `pio_cam_in` (entrada) bit0 SIOD lido. Pull-ups 4,7 kΩ na protoboard.

### Firmware
- `hal/sccb`: bit-bang sobre os PIOs (~10 kHz, tempos do datasheet folgados), escrita/leitura de registrador, ACK verificado.
- `cam/ov7670`: reset, leitura de PID/VER, lista de init (QQVGA, YUV422, CLKRC) transcrita da referência com citação; `capture(buf, timeout)`.
- `--cam-demo`: mostra o quadro no display (Y → cinza RGB565, ampliado 2× para 320×240), FPS no console.
- Fase 2: `vision/motion`: diferença entre quadros, limiar, centroide → novos campos na `Obs` (mudança de contrato, S05 retreina).

## Contrato FPGA ↔ firmware (fixado 2026-10-28; fonte final: fpga/hps_0.h)
| Componente | Base LW | Span |
|---|---|---|
| `cam_capture` | **0x10000** | 0x10000 (64 KiB) |
| `pio_cam` (saída, 4 bits, reset 0) | **0x01C0** | 16 |
| `pio_cam_in` (entrada, 4 bits) | **0x01D0** | 16 |

`pio_cam` (saída): bit0 **SIOC_LOW** (1 = puxa SIOC para 0, 0 = solta → pull-up), bit1 **SIOD_LOW** (idem), bit2 **RESET_N** (0 = câmera em reset), bit3 **PWDN** (1 = desligada). Reset = 0 → câmera em reset até o firmware soltar.
`pio_cam_in` (entrada): bit0 **SIOD** (nível no pino), bit1 SIOC, bit2 VSYNC, bit3 HREF.

`cam_capture`, registradores de 32 bits (índice de palavra = offset/4):
| Palavra | Nome | Acesso | Conteúdo |
|---|---|---|---|
| 0 | CTRL | L/E | bit0 ARM (escrever 1 arma; volta a 0 sozinho), bit1 PHASE (0 = guarda bytes 0,2,4… da linha; 1 = guarda 1,3,5…) |
| 1 | STATUS | L | bit0 DONE (quadro gravado), bit1 BUSY (armado/capturando), bit2 OVERFLOW (linha > 320 bytes ou > 120 linhas: truncado) |
| 2 | FRAMES | L | bordas de subida de VSYNC desde o reset (câmera viva = contando) |
| 3 | LINES | L | linhas com HREF no último quadro capturado |
| 4 | BYTES_PER_LINE | L | bytes na última linha do último quadro (antes de descartar a fase) |
| 5 | ID | L | 0x43414D31 ("CAM1") |
| 0x2000… | FRAME | L | quadro Y 160×120: pixel p na palavra 0x2000 + p/4, byte p%4 (little-endian) |

Captura: ARM → espera a borda de subida de VSYNC → grava as linhas seguintes (HREF alto; um byte por borda de subida de PCLK; guarda os bytes da fase escolhida, até 160 por linha e 120 linhas) → na próxima borda de subida de VSYNC levanta DONE.

## Critérios de aceite
- [PC] `cam_capture` simulado com um testbench (fluxo sintético VSYNC/HREF/PCLK/D): quadro gravado bate com o gerado; HREF curto/longo e PCLK lento/rápido. (Simulador: Questa FSE se licenciado, senão Icarus Verilog — `[A VERIFICAR]`.)
- [PC] `tests/sccb.cpp`: sequência de bits de start/stop/escrita/ACK contra PIO falso; NACK detectado.
- [PC] Contrato de endereços: `tests/qsys.cpp` cobre os novos componentes.
- [LAB] Regressão: `stand` com 18 PWM e `--face-demo` continuam iguais com o `.rbf` novo.
- [LAB] `aracno --cam-id`: lê PID = 0x76 e VER = 0x73.
- [LAB] `--cam-demo`: imagem reconhecível no display (foto), ≥ 5 quadros/s.
- [LAB] Fase 2: com a aranha parada, pessoa cruzando a 1 m é detectada em ≥ 18 de 20 passagens; parede/objeto parado gera ≤ 1 falso positivo por minuto.

## Riscos
- PCLK num pino comum (não dedicado de clock): por isso amostragem a 50 MHz com PCLK ≤ 6,25 MHz. Se a imagem vier com bytes trocados/deslocados, reduzir PCLK (CLKRC) ou levar PCLK a um clock global.
- Cabos de 15–20 cm com 12,5 MHz: possível ruído; manter jumpers curtos e juntos do GND.
- Lista de registradores: a OV7670 é famosa por defaults ruins; usar a lista da Adafruit, que é testada.

## Plano
1. `núcleo` — `cam_capture.v` + testbench + `_hw.tcl`; script Platform Designer `s09_add_camera.tcl`; topo e pinos; compilar; conferir pinos/endereços antigos.
2. `periferia` — firmware: `hal/sccb`, `cam/ov7670`, `--cam-id`, `--cam-demo`; testes de host.
3. Revisão `reviewer-seguranca` (FPGA + firmware).
4. [LAB] troca do `.rbf`, regressão, `--cam-id`, `--cam-demo`.
5. Fase 2 em spec própria ou adendo, após a fase 1 funcionar.

## Evidência
(preencher)
