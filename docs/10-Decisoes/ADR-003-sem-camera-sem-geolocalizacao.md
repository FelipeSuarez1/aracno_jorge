---
status: aceita
data: 2026-10-22
decide: Luiz, Felipe (com o professor)
---
# ADR-003 — Sem câmera, sem geolocalização/SLAM

## Contexto
O professor sugeriu "se geolocalizar". SLAM exige lidar/câmera + ROS numa placa companheira (contraria ADR-001). Com sensores de baixa dimensão a aranha consegue navegar de forma reativa.

## Decisão
Nenhuma câmera, nenhum mapa, nenhuma localização absoluta. "Ver" = VL53L1X frontal na cabeça (varredura por pan) + VL53L1X para o chão (borda) + PIR (presença) + BH1750 ×2 (gradiente de luz). "Casa" = o lugar mais escuro que ela encontra.

## Alternativas descartadas
- Lidar 2D + SLAM — placa extra.
- IMU com bússola + contagem de passos — possível para orientação; fica no Inbox como feature futura, não é requisito.

## Consequências
Navegação reativa (evitar obstáculo, evitar borda, seguir gradiente de luz). O "ataque" é na direção que a cabeça já aponta. O que reabriria: professor exigir mapa.
