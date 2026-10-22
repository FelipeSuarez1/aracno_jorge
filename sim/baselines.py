"""Baselines para comparação com a política treinada (S05).

Interface comum: `act(state_index, obs) -> action` (int de aracno_sim.types.Action) e
`reset()` no início de cada episódio.
"""

import numpy as np

from aracno_sim.discretize import state_levels
from aracno_sim.types import Action, N_ACTIONS


class RandomPolicy:
    def __init__(self, seed: int = 0):
        self.rng = np.random.default_rng(seed)

    def reset(self):
        pass

    def act(self, state_index: int, obs) -> int:
        return int(self.rng.integers(N_ACTIONS))


class FsmPolicy:
    """FSM à mão: esconder no escuro -> espreitar quando PIR -> bote a <= 600 mm -> recuar.
    Mesma lógica do `fsm_demo` de S03, estendida com espreita e recuo.
    Só usa os níveis discretizados, como a política tabular."""

    HIDE, STALK, RETREAT = range(3)
    RETREAT_STEPS = 10   # 2 s recuando após o bote
    COOLDOWN_STEPS = 40  # 8 s ignorando o PIR após um bote (evita bote repetido na mesma parede)

    def __init__(self):
        self.reset()

    def reset(self):
        self.state = self.HIDE
        self.retreat_left = 0
        self.cooldown = 0

    def act(self, state_index: int, obs) -> int:
        lux_l, lux_r, dist, floor, pir, t_since = state_levels(obs)

        if floor == 1:                       # borda/inválido: só recuar
            return Action.BACKWARD

        if self.state == self.RETREAT:
            self.retreat_left -= 1
            if self.retreat_left <= 0:
                self.state = self.HIDE
                self.cooldown = self.COOLDOWN_STEPS
            return Action.BACKWARD

        if self.cooldown > 0:
            self.cooldown -= 1
            pir = 0

        # Só o PIR indica pessoa (t_since também zera com parede a < 1 m, contrato Obs).
        if pir:
            self.state = self.STALK
        elif t_since == 2 or self.cooldown > 0:  # > 10 s sem estímulo
            self.state = self.HIDE

        if self.state == self.STALK:
            if pir and dist in (1, 2):       # 150–600 mm à frente com PIR ativo: bote
                self.state = self.RETREAT
                self.retreat_left = self.RETREAT_STEPS
                return Action.LUNGE
            if dist == 0:                    # colado em algo: provavelmente parede
                return Action.TURN_LEFT if lux_l <= lux_r else Action.TURN_RIGHT
            if dist == 3:                    # algo a 600–1200 mm: aproximar
                return Action.FORWARD
            # nada perto à frente: o bote sai na direção do corpo (e o pan não está em Obs),
            # então varre girando o corpo, não a cabeça
            return Action.TURN_LEFT

        # HIDE
        if dist == 0:                        # obstáculo a <= 150 mm: virar para o lado mais escuro
            return Action.TURN_LEFT if lux_l <= lux_r else Action.TURN_RIGHT
        if lux_l == 0 and lux_r == 0:        # já no escuro: abaixar e ficar
            return Action.CROUCH
        if lux_l < lux_r:
            return Action.TURN_LEFT
        if lux_r < lux_l:
            return Action.TURN_RIGHT
        return Action.FORWARD


class TabularPolicy:
    """Política treinada: argmax da Q-table (o mesmo lookup de firmware/src/brain/policy.cpp)."""

    def __init__(self, q_table: np.ndarray):
        self.argmax = np.argmax(q_table, axis=1).astype(np.uint8)

    def reset(self):
        pass

    def act(self, state_index: int, obs) -> int:
        return int(self.argmax[state_index])
