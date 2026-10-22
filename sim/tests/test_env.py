import math

import numpy as np
from gymnasium.utils.env_checker import check_env

from aracno_sim import AracnoEnv, N_STATES, N_ACTIONS, state_index
from aracno_sim.discretize import (
    DIST_FRONT_BINS, F_EDGE_MM, LUX_BINS, T_SINCE_BINS, dist_front_level, floor_level,
    lux_level, t_since_level,
)
from aracno_sim.types import Action, Obs
from aracno_sim import env as envmod


def test_env_checker():
    check_env(AracnoEnv(), skip_render_check=True)


def test_discretize_boundaries_match_header():
    # Valores de firmware/include/aracno/discretize.h
    assert LUX_BINS == (10, 50, 200)
    assert DIST_FRONT_BINS == (150, 300, 600, 1200)
    assert F_EDGE_MM == 120
    assert T_SINCE_BINS == (2000, 10000)
    assert N_STATES == 960
    for i, b in enumerate(LUX_BINS):
        assert lux_level(b) == i and lux_level(b + 1) == i + 1
    for i, b in enumerate(DIST_FRONT_BINS):
        assert dist_front_level(b) == i and dist_front_level(b + 1) == i + 1
    assert dist_front_level(0) == 4
    assert floor_level(F_EDGE_MM) == 0 and floor_level(F_EDGE_MM + 1) == 1 and floor_level(0) == 1
    for i, b in enumerate(T_SINCE_BINS):
        assert t_since_level(b) == i and t_since_level(b + 1) == i + 1
    # fórmula do contrato: ((((lux_l·4 + lux_r)·5 + dist)·2 + floor)·2 + pir)·3 + t_since
    o = Obs(lux_l=300, lux_r=51, dist_front_mm=200, dist_floor_mm=0, pir=1, t_since_stimulus_ms=5000)
    assert state_index(o) == ((((3 * 4 + 2) * 5 + 1) * 2 + 1) * 2 + 1) * 3 + 1
    assert state_index(Obs(lux_l=65535, lux_r=65535, dist_front_mm=0, dist_floor_mm=0, pir=1,
                           t_since_stimulus_ms=0xFFFFFFFF)) == N_STATES - 1


def test_random_rollout_within_spaces():
    env = AracnoEnv()
    rng = np.random.default_rng(3)
    for ep in range(5):
        s, info = env.reset(seed=100 + ep)
        assert 0 <= s < N_STATES
        assert isinstance(info["obs"], Obs)
        done = False
        n = 0
        while not done:
            s, r, term, trunc, info = env.step(int(rng.integers(N_ACTIONS)))
            assert 0 <= s < N_STATES and s == state_index(info["obs"])
            assert math.isfinite(r)
            done = term or trunc
            n += 1
        assert n <= env.max_steps


def test_lunge_far_is_penalized_and_lunge_close_succeeds():
    env = AracnoEnv()
    env.reset(seed=5)
    # sem pessoa: bote longe
    env.p_present = False  # ainda não entrou (p_enter_ms >= 3000 > 200 ms)
    _, r, term, _, info = env.step(Action.LUNGE)
    assert r <= envmod.R_LUNGE_FAR + 1.0 and not info["lunged_ok"]
    # pessoa a 400 mm na frente do corpo: bote válido termina o episódio
    env.reset(seed=5)
    env.p_enter_ms = 0  # já entrou; fase 1 (parada) com p_stop_ms enorme: não se move
    env.p_present = True
    env.p_phase = 1
    env.p_phase_ms = 0
    env.p_stop_ms = 10**9
    env.p_face = env.heading + math.pi
    env.px = env.sx + 400 * math.cos(env.heading)
    env.py = env.sy + 400 * math.sin(env.heading)
    _, r, term, _, info = env.step(Action.LUNGE)
    assert term and info["lunged_ok"] and r >= envmod.R_LUNGE_OK - 2.0


def test_noise_dict_overrides():
    env = AracnoEnv(noise={"dist_dropout_p": 1.0})
    env.reset(seed=1)
    for _ in range(5):
        _, _, _, _, info = env.step(Action.STOP)
        assert info["obs"].dist_front_mm == 0
