---
status: rascunho
origem: [[20-Specs/S00-visao]] seção 5; [[10-Decisoes/ADR-007-cabeca-pan-tilt]]
depende: [[20-Specs/S03-firmware-base-e-seguranca]], [[20-Specs/S04-display-rosto-balao]]
---
# S06 — Cabeça pan/tilt: mecânica, servos, cabeamento e "olhar"

## Objetivo
Uma cabeça impressa em 3D com display, VL53L1X frontal e BH1750 ×2, movida por **2 MG996R (os reserva do kit)** (pan ±60°, tilt ±25°), que varre o ambiente com o ToF, vira para a pessoa detectada e acompanha quem cruza na frente.

## Fora de escopo
Expressões (S04), decisão de "olhar para onde" pela política (S05 — aqui só o seguimento reativo).

## Fatos de entrada
- Portas PWM 3–8 da Servo Motor Card estão livres, mas a FPGA só instancia 18 controladores — rota HPS escolhida ([[00-Inbox/proposta-ADR-009-rota-hps]]): **PCA9685** (0x40, I2C1) gera o PWM dos 2 MG996R. **Alimentação dos servos da cabeça NUNCA pelo header da DE0**: MG996R tem stall ~2,5 A cada; o `V+` do PCA9685 vai ao 6 V da Servo Motor Card (JP12 com J2 em curto, SMK §2.6) ou a uma fonte própria de 5–6 V ≥ 3 A. O PCA9685 continua com lógica em 3,3 V.
- Massa e torque: MG996R spec 9,4 kgf·cm @ 4,8 V / 11 @ 6 V `[alegação do fabricante]`; com o display Adafruit 2,8" (~40 g) + sensores + suporte (~100–120 g total `[INFERÊNCIA]`) a 4 cm do eixo → ~0,5 kgf·cm, margem > 10×. Pesar e medir o braço.
- I2C a ≥ 15 cm pelo pescoço: pull-ups 4,7 kΩ na ponta (cabeça) e cabo trançado SDA/GND, SCL/GND.

## Design
- Mecânica: suporte pan/tilt comercial para servo padrão (MG996R) ou peça impressa (STL em `docs/40-Fontes/cad/`); fixação no corpo de alumínio por 2 parafusos M3 nos furos existentes (fotografar). Cabo-fita de 10 vias (3V3, GND, SDA, SCL, SCK, MOSI, CS, D/C, RST, 5V) com folga para ±60°.
- Firmware `head/`: `pca9685.cpp` (50 Hz, pulso 0,5–2,5 ms como o MG996R no corpo; limites de software mais estreitos), `head.cpp` com `look(pan, tilt)` limitado por software, rampa de velocidade (≤ 120 °/s), varredura em "S" quando sem alvo, seguimento proporcional ao erro quando o ToF vê algo entre 30 e 150 cm.
- Segurança: o pan/tilt é "periférico" — nunca bloqueia a camada de segurança; se o I2C travar, a cabeça para onde está.

## Critérios de aceite
- [PC] `tests/head.cpp`: limites de ângulo, rampa (não excede 120 °/s em passos de 20 ms), conversão ângulo→registrador PCA9685 (0,5 ms = 102, 1,5 ms = 307, 2,5 ms = 512 em 50 Hz, 12 bits).
- [LAB] Massa da cabeça completa na balança; braço de alavanca medido; margem calculada ≥ 3×.
- [LAB] Marcha com a cabeça montada vs. sem: corrente média e inclinação (ADXL345) — diferença < 15 % e sem tombo em 5 min de marcha. Corrente dos 2 MG996R da cabeça medida separadamente (pinça no V+ do PCA9685).
- [LAB] Varredura: mapa polar de distâncias (−60…+60° em passos de 10°) de uma cena com 2 objetos bate com a trena em ± 5 cm.
- [LAB] Seguimento (S00): pessoa cruza a 1 m em 20 passagens; cabeça acompanha em ≥ 17; vídeo.
- [LAB] Cabo: 500 ciclos pan −60…+60 sem erro I2C/SPI no log.

## Verificação
- [PC]: `make -C firmware test`.
- [LAB]: `aracno --head-demo scan|track`; CSV do mapa polar; vídeo; `/medicao`.

## Plano
1. `periferia` — `pca9685.cpp`, `head.cpp`, `tests/head.cpp`.
2. [LAB] impressão/compra do suporte; montagem com display e sensores; pesagem.
3. `periferia` — `--head-demo`; mapa polar via log.
4. [LAB] testes de marcha com cabeça, cabo, seguimento.

## Evidência
(preencher)
