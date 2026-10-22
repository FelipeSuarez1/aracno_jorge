"""Espelho de firmware/include/aracno/types.h. Qualquer mudança lá exige mudança aqui
(ver docs/20-Specs/contrato-obs-cmd.md)."""

from dataclasses import dataclass, field
from enum import IntEnum
from typing import List


class Action(IntEnum):
    """Mesma ordem numérica de `enum Action` em types.h."""

    STOP = 0
    FORWARD = 1
    BACKWARD = 2
    TURN_LEFT = 3
    TURN_RIGHT = 4
    CROUCH = 5
    LUNGE = 6
    HEAD_LEFT = 7
    HEAD_RIGHT = 8


N_ACTIONS = 9
assert len(Action) == N_ACTIONS


class Sensor(IntEnum):
    """Mesma ordem de `enum Sensor` em types.h (índices de age_ms[] e installed)."""

    S_LUX_L = 0
    S_LUX_R = 1
    S_DIST_FRONT = 2
    S_DIST_FLOOR = 3
    S_PIR = 4
    S_ACCEL = 5
    S_ESTOP = 6


N_SENSORS = 7
AGE_NEVER = 0xFFFF
ALL_INSTALLED = (1 << N_SENSORS) - 1

# Faixa de Cmd.speed (CSpider::SetSpeed satura em 10..100).
SPEED_MIN = 10
SPEED_MAX = 100


@dataclass
class Obs:
    """Espelho de `struct Obs`. Unidades idênticas: lux inteiro, mm, ms, mg."""

    t_ms: int = 0
    lux_l: int = 0
    lux_r: int = 0
    dist_front_mm: int = 0  # 0 = sem leitura válida
    dist_floor_mm: int = 0  # 0 = sem leitura válida
    pir: int = 0
    estop: int = 0
    acc_x_mg: int = 0
    acc_y_mg: int = 0
    acc_z_mg: int = 0
    t_since_stimulus_ms: int = 0
    age_ms: List[int] = field(default_factory=lambda: [0] * N_SENSORS)
    installed: int = ALL_INSTALLED

    def __post_init__(self):
        assert 0 <= self.lux_l <= 0xFFFF and 0 <= self.lux_r <= 0xFFFF
        assert 0 <= self.dist_front_mm <= 0xFFFF and 0 <= self.dist_floor_mm <= 0xFFFF
        assert self.pir in (0, 1) and self.estop in (0, 1)
        assert 0 <= self.t_since_stimulus_ms <= 0xFFFFFFFF
        assert len(self.age_ms) == N_SENSORS


@dataclass
class Cmd:
    action: int
    speed: int  # 10..100

    def __post_init__(self):
        assert 0 <= self.action < N_ACTIONS
        assert SPEED_MIN <= self.speed <= SPEED_MAX
