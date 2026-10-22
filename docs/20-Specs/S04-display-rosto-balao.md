---
status: rascunho
origem: [[20-Specs/S00-visao]] seção 5; [[10-Decisoes/ADR-002-sem-fala-display-com-texto]]
depende: [[20-Specs/S02-toolchain-hello-world]]
---
# S04 — Display: rosto com humor e balão de texto

## Objetivo
Um display SPI na cabeça mostra um rosto com expressões (6 básicas + neutro) que muda com o "humor", e um balão de texto legível a 1 m com o que a aranha "quer falar". Driver em C++ via `/dev/spidev1.0`, sem framebuffer do kernel.

## Fora de escopo
Pan/tilt (S06), escolha do humor pela política (S05). Áudio (ADR-002).

## Fatos de entrada
- `/dev/spidev1.0` existe (SPIM1, `dw-spi-mmio`, driver `spidev`) — [[30-Medicoes/dia-1]]. Pinos `HPS_SPIM_CLK/MISO/MOSI/SS` no header LTC — [[40-Fontes/de0-nano-soc-user-manual]] §3.7.8. **Ainda não confirmado** que o SPIM do header é o SPIM1 (único habilitado): teste de loopback é o passo 1.
- Display precisa de D/C e RESET além de SPI: usar `HPS_LTC_GPIO` para D/C e uma saída do PCF8574 (S03) para RESET. `[INFERÊNCIA]`: velocidade útil do SPI master do HPS ≥ 10 MHz; medir com osciloscópio ou pelo tempo de um quadro.
- Expressões: dimensões de Kalegina et al., HRI 2018 (olhos, boca, sobrancelhas) — [[40-Fontes/fontes-externas]].

## Hardware desta spec
ST7789 240×240 1,54" SPI (ou ILI9341 320×240 2,4"), 3,3 V. Fios: SCK, MOSI, CS, D/C, RST, VCC, GND (MISO não usado). Cabo ≥ 15 cm até a cabeça (S06): manter SPI ≤ 16 MHz.

## Design
- Sprites renderizados no PC (`tools/face/render.py`, Pillow): rosto = camadas olhos/sobrancelhas/boca, 7 expressões × 2 quadros (piscar), RGB565, exportados para `firmware/src/face/sprites.h` (arrays `const uint16_t` em `.rodata`).
- Balão: fonte bitmap 16 px (ASCII + acentos pt-BR) exportada para `font16.h`; quebra de linha automática; até 3 linhas.
- Atualização parcial por região (olhos ~60×30, boca ~80×40, balão 240×80) via `CASET/RASET/RAMWR`; quadro inteiro só na troca de expressão.
- API: `Face::set(Expr, intensity)`, `Face::say(const char*, ms)`, `Face::tick(now)` (piscar, timeout do balão). Sem alocação após init.

## Critérios de aceite
- [PC] `tests/face.cpp`: renderização por software para um buffer 240×240 e comparação pixel a pixel com PNGs de referência em `tests/ref/face_*.png` (gerados pelo mesmo `render.py`); quebra de linha do balão com 10 frases de teste.
- [PC] `tools/face/render.py` reproduz os sprites de forma determinística (hash fixo).
- [LAB] Loopback MISO↔MOSI em `/dev/spidev1.0`: `tools/uart.py run` com `spidev_test` cross-compilado devolve o padrão enviado → confirma SPIM1 = header LTC.
- [LAB] Display ligado: 7 expressões + balão "Olá, eu sou a Jorge" fotografados a 1 m; texto legível por 3 pessoas.
- [LAB] Tempo de atualização parcial (olhos) < 20 ms e quadro inteiro < 150 ms, medidos com `clock_gettime` e logados.
- [LAB] Estudo de reconhecimento (S00): ≥ 10 pessoas de fora, 6 opções, ≥ 70 % por expressão e p < 0,05 vs. 1/6 — só depois da cabeça montada (S06).

## Verificação
- [PC]: `make -C firmware test`.
- [LAB]: `tools/uart.py push firmware/build/aracno` + `aracno --face-demo`; fotos em `30-Medicoes/`.

## Plano
1. `periferia` — `tools/face/render.py` + sprites + fonte; PNGs de referência.
2. `periferia` — `tests/face.cpp` (renderizador de software compartilhado com o firmware).
3. `núcleo` — `hal/spi.cpp` (ioctl `SPI_IOC_MESSAGE`, rajadas de 4 kB), `face/st7789.cpp` (init, janelas, DMA-less), `face/face.cpp`.
4. `periferia` — `spidev_test` cross-compilado para o loopback; `--face-demo` no `main`.
5. [LAB] loopback → display → fotos.

## Evidência
(preencher)
