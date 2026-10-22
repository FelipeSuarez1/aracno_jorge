"""Espelho de firmware/include/aracno/discretize.h. Mesmos limites, mesma fórmula.
Limites inclusivos à direita no nível inferior: valor <= limite[i] -> nível i.
tests/policy_vectors.h (gerado por sim/export.py) prova a igualdade com o C++."""

from .types import Obs

# Lux: 4 níveis — escuro / penumbra / claro / muito claro.
LUX_BINS = (10, 50, 200)
LUX_LEVELS = 4

# Distância frontal: 5 níveis; 0 (inválido) conta como "longe" (nível 4).
DIST_FRONT_BINS = (150, 300, 600, 1200)
DIST_FRONT_LEVELS = 5

# Chão: 2 níveis — chão presente / borda (ou inválido). F_EDGE_MM é [A MEDIR] (S03/S05).
F_EDGE_MM = 120
FLOOR_LEVELS = 2

PIR_LEVELS = 2

# Tempo desde o último estímulo: 3 níveis.
T_SINCE_BINS = (2000, 10000)
T_SINCE_LEVELS = 3

N_STATES = LUX_LEVELS * LUX_LEVELS * DIST_FRONT_LEVELS * FLOOR_LEVELS * PIR_LEVELS * T_SINCE_LEVELS
assert N_STATES == 960


def lux_level(lux: int) -> int:
    level = 0
    while level < 3 and lux > LUX_BINS[level]:
        level += 1
    return level


def dist_front_level(mm: int) -> int:
    if mm == 0:
        return DIST_FRONT_LEVELS - 1
    level = 0
    while level < 4 and mm > DIST_FRONT_BINS[level]:
        level += 1
    return level


def floor_level(mm: int) -> int:
    return 1 if (mm == 0 or mm > F_EDGE_MM) else 0


def t_since_level(ms: int) -> int:
    level = 0
    while level < 2 and ms > T_SINCE_BINS[level]:
        level += 1
    return level


def state_index(o: Obs) -> int:
    """Ordem (lux_l, lux_r, dist_front, floor, pir, t_since), último varia mais rápido."""
    s = lux_level(o.lux_l)
    s = s * LUX_LEVELS + lux_level(o.lux_r)
    s = s * DIST_FRONT_LEVELS + dist_front_level(o.dist_front_mm)
    s = s * FLOOR_LEVELS + floor_level(o.dist_floor_mm)
    s = s * PIR_LEVELS + (1 if o.pir else 0)
    s = s * T_SINCE_LEVELS + t_since_level(o.t_since_stimulus_ms)
    return s


def state_levels(o: Obs):
    """Níveis individuais (útil para baselines e depuração)."""
    return (
        lux_level(o.lux_l),
        lux_level(o.lux_r),
        dist_front_level(o.dist_front_mm),
        floor_level(o.dist_floor_mm),
        1 if o.pir else 0,
        t_since_level(o.t_since_stimulus_ms),
    )
