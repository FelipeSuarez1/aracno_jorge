---
status: aceita
data: 2026-10-28 (aceita pelo Luiz na conversa: "gostei de tudo... faça a câmera funcionar")
decide: Luiz, Felipe (com o professor)
substitui: parte de [[10-Decisoes/ADR-003-sem-camera-sem-geolocalizacao]]
---
# ADR-010 — Câmera OV7670 ligada na FPGA (revisa a ADR-003)

## Contexto
- ADR-003 proibiu câmera porque, na época, câmera implicava placa companheira (ADR-001).
- S05 mostrou que, sem visão, a política não distingue pessoa de parede (teto ~14 % em simulação).
- S08 abriu o GPIO_0 da FPGA (26 pinos livres) e provou o fluxo Quartus 25.1 → `.rbf` → placa.
- O lab tem uma OV7670 sem FIFO; já ligada no JP1 (linhas 7–15).

## Decisão (proposta)
Permitir **uma câmera OV7670 capturada pela FPGA**, sem placa extra e sem webcam USB. Continua proibido: placa companheira, SLAM/geolocalização, visão em Linux novo. A visão serve para (1) detectar movimento/pessoa como entrada da política e (2) guiar a cabeça.

## Alternativas descartadas
- Webcam USB: kernel 3.13 da imagem sem driver UVC; OTG já com sobrecorrente pelo dongle BT.
- Câmera inteligente (HuskyLens/OpenMV): funciona, mas a visão viria pronta; a OV7670 já está no lab e usa a FPGA de verdade.

## Consequências
Spec [[20-Specs/S09-camera-ov7670]]. A `Obs` ganha campos de visão na fase 2 (retreino da S05). ADR-003 continua valendo para SLAM e geolocalização.
