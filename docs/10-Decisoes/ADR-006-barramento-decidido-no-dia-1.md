---
status: aceita
data: 2026-10-22
decide: Luiz, Felipe
---
# ADR-006 — Rota de barramento (HPS ou FPGA) decidida pelo teste do dia 1

## Contexto
Sensores e display precisam de I2C/SPI. O HPS expõe `HPS_I2C1`, `HPS_SPIM` e 1 GPIO no header LTC 2x7 ([[40-Fontes/de0-nano-soc-user-manual]] §3.7.8) — mas só se a imagem Linux 2.0 tiver os drivers habilitados (`/dev/i2c-1`, `/dev/spidev*`). Os headers 2x20 e Arduino são pinos da FPGA.

## Decisão
Não assumir nenhuma rota até rodar [[20-Specs/S01-dia-1-uart]].
- **Rota HPS** (se `/dev/i2c-1` e `/dev/spidev` existirem): display e sensores no header LTC; pan/tilt via PCA9685 no mesmo I2C. Zero Quartus.
- **Rota FPGA** (caso contrário, ou para pan/tilt pela Servo Motor Card): I2C/SPI master + 2 controladores PWM (portas 3–8 da SMK) adicionados no Platform Designer com Quartus Prime Lite; acesso via `/dev/mem` como os PWM existentes.

## Alternativas descartadas
- Bit-bang de I2C/SPI por GPIO do HPS — só 1 GPIO livre no header LTC.

## Consequências
A spec S02+ que tocar em periféricos declara qual rota usa. A escolha vira ADR-009 após o dia 1.
