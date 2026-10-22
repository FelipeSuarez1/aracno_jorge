# Fontes externas

## Terasic / Intel
- Spider: página do produto e downloads — http://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&CategoryNo=183&No=994 (aba Resources: `&PartNo=4`). CD-ROM e Linux Image exigem login.
- `cd-spider.terasic.com` redireciona para a página acima.
- Servo Motor Card manual V1.1.0 (2019-07-30) — https://www.mouser.com/datasheet/2/598/ervo_Motor_Kit-1020966.pdf (Mouser bloqueia robôs; abrir no navegador).
- DE0-Nano-SoC User Manual rev.D0 — https://www.terasic.com.tw/attachment/archive/941/DE0-Nano-SoC_User_manual_rev.D0.pdf
- Cyclone V HPS Technical Reference Manual — Intel doc 683126 (ponte lightweight `lwhps2fpga` em `0xFF200000`, I2C, SPI master, watchdog L4). `[INFERÊNCIA]` número do doc citado pelo agente; confirmar no site da Intel.
- Caminho de Debian armhf na DE0-Nano-SoC (referência, **não usar**: ADR-001) — https://forum.digikey.com/t/debian-getting-started-with-the-de0-nano-soc-kit/12434

## Datasheets a citar (não lidos ainda nesta sessão — `[INFERÊNCIA]` até abrir)
- INA230 — TI SBOS601B (tempos de conversão 140 µs–8,244 ms; média 1–1024).
- VL53L1X — ST DS12385 (alcance vs. luz ambiente; timing budget mínimo 20 ms).
- BH1750 — ROHM (modos H/L-res, ~120 ms em H-res).
- ST7789V — Sitronix (AC characteristics SPI).
- PCA9685 — NXP. LTC2308 — ADI. HC-SR501 — doc do módulo.
- MG996R / MG90S — apenas spec TowerPro (stall ~2,5 A @ 6 V; 9,4 kgf·cm @ 4,8 V / 11 @ 6 V): tratar como alegação, medir.

## Método
- Kalegina, Schroeder, Allchin, Berlin, Cakmak. "Characterizing the Design Space of Rendered Robot Faces". HRI 2018.
- Ross, Gordon, Bagnell. "A Reduction of Imitation Learning and Structured Prediction to No-Regret Online Learning" (DAgger). AISTATS 2011.
- Anthropic, "How Anthropic teams use Claude Code" — https://www-cdn.anthropic.com/58284b19e702b49db9302d5b6f135ad8871e7658.pdf
- Claude Code best practices — https://code.claude.com/docs/en/best-practices
