# Lista de procura no lab (rota FPGA, S08/S09) — atualizada 2026-10-28

Fluxo: Luiz procura no lab → manda nome/foto → IA diz onde ligar e o que habilita → vira spec.

## Já temos / já ligado
| Item | Onde | Spec |
|---|---|---|
| Adafruit ILI9341 2,8" com touch | GPIO_0 (JP1) pinos 1–5, 11, 12 — **rosto funcionando** | S04/S08 |
| Câmera OV7670 (sem FIFO) | GPIO_0 (JP1) pinos 13–30 — aguardando S09 | S09 |
| 2× MG996R reserva (cabeça) | Servo Motor Card portas 3/4 (PWM18/19 já na FPGA) | S06/S08 |
| Meia protoboard colada no acrílico | — | — |

## Pinos livres
- GPIO_0: pinos 31–40 (10) + 7–10 reservados (XSHUT ×2, PIR, botão)
- Header Arduino: 16 I/O · Header ADC (LTC2308): 8 entradas analógicas · SMK: portas PWM 5–8

## Procurar — 1. necessário (sem isso a IA não vê o mundo)
| Item | Para quê | Substituto |
|---|---|---|
| VL53L0X/VL53L1X ×2 | obstáculo à frente e borda de mesa | HC-SR04 (FPGA mede o eco) |
| BH1750 ×2 | achar o escuro | LDR + resistor no ADC |
| PIR HC-SR501 | presença | (câmera ajuda) |
| Botão NF | parada por software | botão comum |

## 2. Cabeça
| Suporte pan/tilt para servo padrão (ou impressão 3D) | cabeça que vira |
|---|---|

## 3. Personalidade (baratos)
| Item | Efeito | Liga em |
|---|---|---|
| Buzzer/piezo | chiado no bote, sons de susto (não é voz, ADR-002) | 1 pino FPGA |
| Fita WS2812 / LED RGB | olhos que brilham no escuro, cor do humor | 1 pino FPGA |
| Microfone analógico (KY-037, MAX4466) | reage a palma/barulho | ADC |
| 2 resistores (divisor) | tensão da bateria → humor "cansada" | ADC |
| Receptor IR + controle | comando sem celular | 1 pino FPGA |

## 4. Montagem e medição
Placa perfurada · cabo flat IDC 2x20 · espaçadores M3 · multímetro · osciloscópio · pinça amperimétrica DC · balança 1 g · luxímetro (ou app).

**Não precisa mais:** PCF8574 e PCA9685 (a FPGA faz o papel deles).
