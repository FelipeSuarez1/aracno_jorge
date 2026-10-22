# Contrato Obs / Cmd / discretização (S03 ↔ S05 ↔ S07)

Fonte de verdade: `firmware/include/aracno/types.h` e `firmware/include/aracno/discretize.h`. O Python em `sim/aracno_sim/` espelha os dois; `tests/policy.cpp` prova a igualdade com vetores gerados pelo Python.

## Obs (uma amostra por ciclo de 20 ms)
| Campo | Tipo | Unidade | Origem |
|---|---|---|---|
| `t_ms` | u32 | ms monotônico | `clock_gettime` |
| `lux_l`, `lux_r` | u16 | lx | BH1750 ×2 (I2C1 0x23 / 0x5C) |
| `dist_front_mm` | u16 | mm, 0 = inválido | VL53L1X cabeça |
| `dist_floor_mm` | u16 | mm, 0 = inválido | VL53L1X chão |
| `pir` | u8 | 0/1 | HC-SR501 via PCF8574 |
| `estop` | u8 | 0/1 | botão NF via PCF8574 |
| `acc_{x,y,z}_mg` | i16 | mg | ADXL345 (I2C0 0x53) |
| `t_since_stimulus_ms` | u32 | ms | calculado: último `pir=1` ou objeto < 1 m |
| `age_ms[N_SENSORS]` | u16 | ms; `0xFFFF` nunca | idade da última leitura válida |
| `installed` | u8 | bitmask | configuração da montagem |

## Cmd
`action` ∈ {STOP, FORWARD, BACKWARD, TURN_LEFT, TURN_RIGHT, CROUCH, LUNGE, HEAD_LEFT, HEAD_RIGHT}; `speed` 10–100.

## Discretização (política tabular)
| Feature | Limites | Níveis |
|---|---|---|
| lux_l, lux_r | ≤10, ≤50, ≤200, >200 | 4 |
| dist_front | ≤150, ≤300, ≤600, ≤1200, >1200 ou inválido | 5 |
| floor | ≤ `F_EDGE_MM` (120, **a medir**) = chão; senão/inválido = borda | 2 |
| pir | 0/1 | 2 |
| t_since | ≤2 s, ≤10 s, >10 s | 3 |

`state = ((((lux_l·4 + lux_r)·5 + dist)·2 + floor)·2 + pir)·3 + t_since` → 960 estados.

## Regras
- Camada de segurança (S03) recebe `Cmd` desejado e devolve `Cmd` permitido + motivo; a política nunca escreve nos servos.
- Mudança em qualquer limite = mudar os dois lados + regenerar `tests/policy_vectors.h` + retreinar.
