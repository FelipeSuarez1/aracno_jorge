# Lista de compras (rota HPS, ADR-009)

| Qtd | Item | Spec | Observação |
|---|---|---|---|
| ✅ | Display: Adafruit ILI9341 2,8" 320×240 com touch resistivo — **tem no lab** (também GC9A01 redondo e ST7789 1,3") | S04 | conferir jumpers IM1–3 e chip de touch |
| 2 | BH1750 (breakout GY-302) | S03 | um em 0x23, outro em 0x5C (pino ADDR) |
| 2 | VL53L1X (breakout com XSHUT exposto) | S03 | segundo endereço via XSHUT |
| 1 | PIR HC-SR501 | S03 | |
| 1 | PCF8574 (expansor I2C, 0x20) | S03 | XSHUT ×2, PIR, botão de parada, RST do display |
| 1 | PCA9685 (16 canais PWM I2C) | S06 | servos da cabeça |
| ✅/1 | Servos da cabeça: **2 MG996R reserva do kit**; falta o suporte pan/tilt para servo padrão | S06 | ou peça impressa no lab |
| 4 | Resistores 4,7 kΩ | S03 | pull-up I2C1 (SDA/SCL) na cabeça e no corpo |
| 1 | Botão NF para parada lógica + botão cogumelo NF (opcional) em série na bateria | S03 | |
| 1 | Cabo-fita 10 vias ≥ 30 cm + conectores 2x7 (header LTC) e Dupont | S04/S06 | |
| — | Pinça amperimétrica DC, luxímetro (ou app), trena, balança 1 g, paquímetro | S00 | lab do professor |
| — | Osciloscópio 2 canais + shunt 10–20 mΩ | S00 | lab do professor |

Já temos (2026-10-22): Spider, DE0-Nano-SoC, Servo Motor Card, 2 packs + carregador, fontes 12 V e 5 V, dongle BT, cabos USB.
