---
status: aceita
data: 2026-10-22
decide: Luiz, Felipe (com o professor)
---
# ADR-007 — Cabeça pan/tilt com display, ToF e sensores de luz

## Contexto
O professor aprovou a cabeça que vira. A Servo Motor Card tem 6 portas PWM livres (3–8), mas a FPGA só instancia 18 controladores ([[40-Fontes/spider-programming-guide]] cap. 3).

## Decisão
Cabeça em pan/tilt com 2× MG90S carregando: display SPI (ST7789 240×240 ou ILI9341 320×240), VL53L1X frontal, BH1750 ×2 (esquerda/direita). Segundo VL53L1X fixo no corpo apontado ao chão (borda de mesa). PIR HC-SR501 no corpo. Suporte impresso em 3D no lab do professor.

## Alternativas descartadas
- Cabeça fixa — perde o "olhar" e a varredura do ToF.
- SSD1306 128×64 — pequeno demais para rosto + balão de texto (ADR-002).

## Consequências
Margem de torque ≥ 3× sobre a spec do MG90S, massa da cabeça medida na balança e testada como lastro na marcha antes da montagem. Cabeamento pelo pescoço com I2C ≥ 15 cm (pull-ups dimensionados). Acionamento dos servos da cabeça segue ADR-006.
