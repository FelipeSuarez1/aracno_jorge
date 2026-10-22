# tools/face — sprites do rosto, fonte do balão e referências (S04)

Gera, no PC, tudo que o firmware desenha no display (parte "verificável no PC" da
[[20-Specs/S04-display-rosto-balao]]). Nada aqui toca SPI/ILI9341: o `Canvas`
é um buffer `uint16_t[320*240]` RGB565 nativo; a ordem de bytes do fio é do driver.

## Regenerar
```sh
python3 -m venv tools/face/.venv
tools/face/.venv/bin/pip install -r tools/face/requirements.txt   # Pillow pinado
tools/face/.venv/bin/python tools/face/render.py
make -C firmware test                                              # tests/face
```
Saídas (todas commitadas):
- `firmware/src/face/sprites.h` — 7 expressões × 2 quadros (olhos abertos / piscando) × 5 camadas
  (olho esq/dir, sobrancelha esq/dir, boca): `static const uint16_t SPR_n[]` + tabela
  `SPRITES[expr][quadro][camada] = {x, y, w, h, px}` (px = 0 → camada ausente) + `EXPR_COLOR[]`.
- `firmware/src/face/font16.h` — fonte bitmap 8×16 monoespaçada, 1 bit/pixel, ASCII 32–126 +
  `áàâãéêíóôõúçÁÉÊÍÓÚÇ` (114 glifos), desenhada a partir de `fonts/LiberationMono-Regular.ttf`
  (13 px, linha de base em y=12; SIL OFL, licença em `fonts/`).
- `tests/ref/*.ppm` — 18 referências P6 320×240 (RGB888 derivado do RGB565): 7 expressões × 2 quadros,
  `face_alegria_i50_0` (intensidade 50) e `balao_1..3` (frases com acento e palavra > 38 colunas).
- `samples/*.png` — amostra de cada expressão com uma frase.
- `HASHES.txt` — sha256 de cada artefato + versão do Pillow/freetype + sha256 da fonte.

`render.py --compose EXPR QUADRO INTENSIDADE "frase" saida.ppm` compõe um quadro com o espelho
Python de `Canvas`/`Face` (mesmas constantes de `canvas.h`/`face.h`) — é esse espelho que gera as
referências; `tests/face.cpp` compara pixel a pixel.

## Tamanho (gerado em 2026-10-22)
| arquivo | texto | dados |
|---|---|---|
| `sprites.h` | 111 498 B | 53 458 B de pixels em 31 arrays únicos (camadas repetidas entre quadros são deduplicadas) |
| `font16.h` | 13 065 B | 114 × 16 B = 1 824 B + tabela de code points |
| `face.o` (-O2, host) | — | 58 995 B em `.text`+`.rodata` |

Abaixo de 200 kB: sem RLE. Se as caixas crescerem (olhos > 60×50), ativar RLE antes de aumentar
o supersample.

## Determinismo
Mesma entrada → mesmos bytes: fonte fixada no repo, Pillow pinado, desenho em inteiros,
anti-aliasing por `Image.reduce(4)` (média de caixa). `HASHES.txt` deve ficar idêntico após duas
execuções; se mudar sem alteração em `render.py`, a versão do Pillow/freetype mudou — repinar.

## Expressões
Variáveis de Kalegina et al. (HRI 2018): tamanho/altura dos olhos, presença e ângulo das sobrancelhas,
forma da boca (`PARAMS` em `render.py`). Mapeamento variável → emoção é [INFERÊNCIA]; o estudo de
reconhecimento [LAB] da S04 valida. `Face::set(expr, intensidade)` escala olhos e boca em 50–100 %
(vizinho mais próximo, inteiro); sobrancelhas não escalam. Cor do traço e do balão por humor
(`EXPR_COLOR`). Piscar: 150 ms a cada 3–5 s (xorshift32, semente fixa). Balão: até 3 linhas de 38
colunas, quebra em espaço, palavra maior que 38 cortada; `say(txt, ms)` expira em `ms` (0 = fixo).
