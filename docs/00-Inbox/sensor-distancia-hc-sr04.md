---
data: 2026-10-28
origem: Luiz — "o sensor de distância vai ser o HC-SR04"
---
# Sensor de distância: HC-SR04 em vez de VL53L1X

Impacto:
- S03 (`sense/vl53l1x`, XSHUT no PCF8574) e o contrato `Obs` (`dist_front_mm`, `dist_floor_mm`) continuam válidos; muda a origem: um bloco na FPGA mede a largura do Echo (µs → mm, ~5,8 µs/mm ida e volta) e expõe a distância num registrador.
- Elétrica: 5 V; **Echo em 5 V precisa de divisor** antes do GPIO_0 (3,3 V).
- Cone largo (~15°) e leituras falsas em superfícies inclinadas/macias: caracterizar como o VL53L1X estava planejado (S05, modelo de ruído).
- Borda de mesa: HC-SR04 apontado para o chão a ~5–10 cm fica perto do mínimo (2 cm, datasheet) — medir.
- Vira ADR (substitui a parte de ToF da ADR-007) quando os sensores forem ligados.
