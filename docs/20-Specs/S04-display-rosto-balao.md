---
status: rascunho
origem: [[20-Specs/S00-visao]] seção 5; [[10-Decisoes/ADR-002-sem-fala-display-com-texto]]
depende: [[20-Specs/S02-toolchain-hello-world]]
---
# S04 — Display: rosto com humor e balão de texto

## Objetivo
Um display SPI na cabeça (Adafruit ILI9341 2,8" 320×240 com touch resistivo, já no lab) mostra um rosto com expressões (6 básicas + neutro) que muda com o "humor", e um balão de texto legível a 1 m com o que a aranha "quer falar". Driver em C++ via `/dev/spidev1.0`, sem framebuffer do kernel.

## Fora de escopo
Pan/tilt (S06), escolha do humor pela política (S05). Áudio (ADR-002).

## Fatos de entrada
- `/dev/spidev1.0` existe (SPIM1, `dw-spi-mmio`, driver `spidev`) — [[30-Medicoes/dia-1]]. Pinos `HPS_SPIM_CLK/MISO/MOSI/SS` no header LTC — [[40-Fontes/de0-nano-soc-user-manual]] §3.7.8. **Ainda não confirmado** que o SPIM do header é o SPIM1 (único habilitado): teste de loopback é o passo 1.
- Display precisa de D/C e RESET além de SPI. `HPS_LTC_GPIO` (gpio209) **não está livre**: é o seletor do mux TS3A5018 do header e deve ficar em 0 para o SPI aparecer ([[40-Fontes/hardware]]). D/C e RESET vêm de duas saídas do **PCF8574** (S03) — D/C muda só entre comando e dados (≈0,2 ms por troca a 400 kHz), aceitável porque pixels fluem com D/C fixo em 1. `[INFERÊNCIA]`: velocidade útil do SPI master do HPS ≥ 10 MHz; medir com osciloscópio ou pelo tempo de um quadro.
- Expressões: dimensões de Kalegina et al., HRI 2018 (olhos, boca, sobrancelhas) — [[40-Fontes/fontes-externas]].

## Hardware desta spec
**Adafruit 2,8" TFT ILI9341 320×240 com touch resistivo** (Product ID 1770; disponível no lab, 2026-10-22). Tem regulador e level-shifter: alimentar `VIN` com 3,3 V. Fios: `CLK, MOSI, CS, D/C, RST, VIN, GND` (MISO, SD e os pinos do touch resistivo analógico ficam soltos). Jumpers `IM1/IM2/IM3` fechados = modo SPI (conferir atrás). Controlador de touch: STMPE610 (I2C 0x41, placas antigas) ou TSC2007 (I2C 0x48, 2023+) — ler o chip atrás; vai no mesmo I2C1 dos sensores. Massa ~35–40 g (`[INFERÊNCIA]`, pesar) — por isso S06 usa MG996R. Cabo ≥ 15 cm até a cabeça: SPI ≤ 16 MHz.

Touch (uso futuro, fora dos critérios desta spec): estímulo tátil para o humor (toque longo acalma, toque curto assusta) e "acordar/dormir". Alternativas no lab: GC9A01 redondo 1,28" (bom como olho extra, ruim para texto), ST7789 1,3" (reserva).

## Design
- Sprites renderizados no PC (`tools/face/render.py`, Pillow): rosto = camadas olhos/sobrancelhas/boca, 7 expressões × 2 quadros (piscar), RGB565, exportados para `firmware/src/face/sprites.h` (arrays `const uint16_t` em `.rodata`). Layout 320×240 em paisagem: rosto centralizado nos 320×160 de cima, balão nos 320×80 de baixo.
- Balão: fonte bitmap 16 px (ASCII + acentos pt-BR) exportada para `font16.h`; quebra de linha automática; até 3 linhas.
- Atualização parcial por região (olhos ~60×30, boca ~80×40, balão 320×80) via `CASET/RASET/RAMWR`; quadro inteiro só na troca de expressão.
- API: `Face::set(Expr, intensity)`, `Face::say(const char*, ms)`, `Face::tick(now)` (piscar, timeout do balão). Sem alocação após init.

## Critérios de aceite
- [PC] `tests/face.cpp`: renderização por software para um buffer 320×240 e comparação pixel a pixel com PNGs de referência em `tests/ref/face_*.png` (gerados pelo mesmo `render.py`); quebra de linha do balão com 10 frases de teste.
- [PC] `tools/face/render.py` reproduz os sprites de forma determinística (hash fixo).
- [LAB] Loopback J17 pino 5 (MISO) ↔ pino 7 (MOSI) em `/dev/spidev1.0` com gpio209 = 0: `spidev_test` cross-compilado devolve o padrão enviado → confirma o mux. Pinagem do J17 em [[40-Fontes/hardware]]. Ligação do display: pino 4 → CLK, 7 → MOSI, 6 → CS, 2 → VIN, 3 → GND; D/C e RST → PCF8574 (pinos 9/11 = SDA/SCL).
- [LAB] Display ligado: 7 expressões + balão "Olá, eu sou a Jorge" fotografados a 1 m; texto legível por 3 pessoas.
- [LAB] Tempo de atualização parcial (olhos) < 20 ms e quadro inteiro < 200 ms (320×240×2 B = 154 kB a ≥ 8 MHz), medidos com `clock_gettime` e logados.
- [LAB] Estudo de reconhecimento (S00): ≥ 10 pessoas de fora, 6 opções, ≥ 70 % por expressão e p < 0,05 vs. 1/6 — só depois da cabeça montada (S06).

## Verificação
- [PC]: `make -C firmware test`.
- [LAB]: `tools/uart.py push firmware/build/aracno` + `aracno --face-demo`; fotos em `30-Medicoes/`.

## Plano
1. `periferia` — `tools/face/render.py` + sprites + fonte; PNGs de referência.
2. `periferia` — `tests/face.cpp` (renderizador de software compartilhado com o firmware).
3. `núcleo` — `hal/spi.cpp` (ioctl `SPI_IOC_MESSAGE`, rajadas de 4 kB), `face/ili9341.cpp` (init, janelas `CASET/PASET/RAMWR`, DMA-less), `face/face.cpp`.
4. `periferia` — `spidev_test` cross-compilado para o loopback; `--face-demo` no `main`.
5. [LAB] loopback → display → fotos.

## Evidência
(preencher)
