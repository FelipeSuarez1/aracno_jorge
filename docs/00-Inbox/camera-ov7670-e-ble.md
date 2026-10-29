---
data: 2026-10-28
origem: conversa no fim da sessão de 2026-10-28
decide: Luiz, Felipe, professor
---
# Câmera OV7670 na FPGA e Bluetooth para "pessoa vs. parede" — reabre ADR-003

## Problema que motiva
S05: com a `Obs` atual nada distingue pessoa de parede; teto da política tabular ~14 % ([[00-Inbox/politica-nao-distingue-pessoa-de-parede]]).

## Ideia 1 — OV7670 no GPIO_0 (o lab tem o módulo)
- FPGA gera XCLK (25 MHz = 50 MHz/2), captura PCLK/HREF/VSYNC/D0–D7 e grava 160×120 (RGB565, 38 kB) ou 320×240 cinza (77 kB) em M10K interna (5CSEMA4 tem ~330 kB).
- HPS lê o quadro pela ponte LW; configura a câmera (SCCB, ~100 registradores) por 2 bits de PIO.
- Detecção inicial: **diferença entre quadros (movimento)** → pessoa se move, parede não. Saída para a `Obs`: movimento sim/não + coluna do centroide (guia o pan da cabeça). Depois, opcional: rede pequena de detecção de pessoa.
- ~16 pinos do GPIO_0 (há ~26 livres). Spec nova S09. **Falta saber: módulo com ou sem FIFO (AL422B).**

## Ideia 2 — BLE do dongle CSR8510
- Escutar anúncios BLE de celulares, RSSI → "há celular a ~1–3 m". Sem direção; RSSI ±5–10 dB. Feature barata extra para a `Obs`.
- Primeiro passo: `hcitool lescan` na placa para ver se a BlueZ da imagem 2.0 faz BLE. [INFERÊNCIA]

## Decisão necessária
Revisar ADR-003 ("sem câmera"): câmera **na FPGA** não viola ADR-001 (sem placa extra). Se aprovado: ADR-010 + S09.

## Atualização 2026-10-28 (noite) — módulo no lab, pinagem proposta
Fonte de referência: https://github.com/danielmpinto/pico-ov7670-driver (MIT; adaptação do Adafruit_CircuitPython_OV7670). Útil: lista de registradores de init e `frame_control` por resolução; MCLK 16 MHz; SCCB a 100 kHz com **pull-up externo 4,7 kΩ**. A captura por polling do Pico não se aplica (aqui captura a FPGA).

Módulo sem FIFO (18 pinos, 2×9). JP1 (GPIO_0) linhas 7–15, orientação de [[20-Specs/S08-fpga-gpio0-display-cabeca]]:

| JP1 | GPIO_0 | OV7670 |
|---|---|---|
| 13 / 14 | [10] / [11] | SIOC / SIOD (SCCB por PIO open-drain, pull-up 4,7 kΩ ao 3,3 V) |
| 15 / 16 | [12] / [13] | VSYNC / HREF |
| 17 / 18 | [14] / [15] | PCLK / XCLK (FPGA gera; 12,5 MHz = 50/4, ajustar CLKRC) |
| 19 … 26 | [16] … [23] | D0 … D7 |
| 27 / 28 | [24] / [25] | RESET (ativo baixo) / PWDN (ativo alto) |
| 29 / 30 | — | 3,3 V / GND → trilhos da protoboard → câmera 3V3/GND e pull-ups |

Pendente para a S09: confirmar 18 pinos sem AL422B; PCLK entra num pino comum (não dedicado de clock) — amostrar com 50 MHz se PCLK ≤ ~12 MHz ou usar como clock global (Quartus promove); medir.
