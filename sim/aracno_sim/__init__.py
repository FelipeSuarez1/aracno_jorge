"""Simulador 2D da aranha (S05). Espelha firmware/include/aracno/{types,discretize}.h."""

from .types import Action, Sensor, Obs, Cmd, N_ACTIONS, AGE_NEVER  # noqa: F401
from .discretize import N_STATES, state_index  # noqa: F401
from .env import AracnoEnv  # noqa: F401
