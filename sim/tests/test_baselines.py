import numpy as np

from aracno_sim import AracnoEnv, N_STATES, N_ACTIONS
from aracno_sim.types import Action, Obs
from baselines import FsmPolicy, RandomPolicy, TabularPolicy
from eval import run_episodes


def _rollout(policy, seed):
    env = AracnoEnv()
    policy.reset()
    s, info = env.reset(seed=seed)
    done = False
    actions = []
    while not done:
        a = policy.act(s, info["obs"])
        assert 0 <= int(a) < N_ACTIONS
        actions.append(int(a))
        s, r, term, trunc, info = env.step(a)
        done = term or trunc
    return actions, info


def test_fsm_and_random_run():
    for pol in (FsmPolicy(), RandomPolicy(seed=0), TabularPolicy(np.zeros((N_STATES, N_ACTIONS)))):
        for seed in range(3):
            actions, info = _rollout(pol, 1000 + seed)
            assert len(actions) > 0


def test_fsm_rules():
    fsm = FsmPolicy()
    fsm.reset()
    dark = Obs(lux_l=5, lux_r=5, dist_front_mm=2000, dist_floor_mm=80, pir=0, t_since_stimulus_ms=20000)
    assert fsm.act(0, dark) == Action.CROUCH
    edge = Obs(lux_l=5, lux_r=5, dist_front_mm=2000, dist_floor_mm=0, pir=0, t_since_stimulus_ms=20000)
    assert fsm.act(0, edge) == Action.BACKWARD
    lit_left = Obs(lux_l=300, lux_r=20, dist_front_mm=2000, dist_floor_mm=80, pir=0, t_since_stimulus_ms=20000)
    assert fsm.act(0, lit_left) == Action.TURN_RIGHT
    close = Obs(lux_l=5, lux_r=5, dist_front_mm=500, dist_floor_mm=80, pir=1, t_since_stimulus_ms=0)
    assert fsm.act(0, close) == Action.LUNGE
    assert fsm.act(0, dark) == Action.BACKWARD  # recuo após o bote


def test_eval_runs_small():
    sr, ret, dark = run_episodes(FsmPolicy(), layout_seed=1, n=3)
    assert 0.0 <= sr <= 1.0 and 0.0 <= dark <= 1.0
