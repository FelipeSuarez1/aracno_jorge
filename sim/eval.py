#!/usr/bin/env python3
"""Avalia política treinada, FSM à mão e aleatória em N episódios em layouts não vistos (S05).

Sucesso (spec S05): episódio termina com bote válido, sem colisão nem borda, e >= 70 % do
tempo no escuro. Alvo X: parâmetro --target (default 0,7, PROVISÓRIO até os autores fixarem
e registrarem em S05).

Layouts: sementes EVAL_SEED_BASE + layout_seed*10**6 + i, disjuntas das de treino (train_q.py).
"""

import argparse
import os
import sys

import numpy as np

from aracno_sim import AracnoEnv
from baselines import FsmPolicy, RandomPolicy, TabularPolicy
from train_q import EVAL_SEED_BASE


def run_episodes(policy, layout_seed: int, n: int, noise: dict = None):
    env = AracnoEnv(noise=noise)
    successes = 0
    returns = []
    dark = []
    for i in range(n):
        policy.reset()
        s, info = env.reset(seed=EVAL_SEED_BASE + layout_seed * 10**6 + i)
        total = 0.0
        done = False
        while not done:
            a = policy.act(s, info["obs"])
            s, r, term, trunc, info = env.step(a)
            total += r
            done = term or trunc
        successes += int(info["success"])
        returns.append(total)
        dark.append(info["dark_fraction"])
    return successes / n, float(np.mean(returns)), float(np.mean(dark))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--q", default="runs/q_table.npy")
    ap.add_argument("--episodes", type=int, default=1000)
    ap.add_argument("--layout-seed", type=int, default=7)
    ap.add_argument("--target", type=float, default=0.7, help="alvo X de taxa de sucesso (provisório)")
    args = ap.parse_args()

    policies = []
    if os.path.exists(args.q):
        policies.append(("Q-table (treinada)", TabularPolicy(np.load(args.q))))
    else:
        print(f"aviso: {args.q} não existe; rode train_q.py primeiro", file=sys.stderr)
    policies.append(("FSM à mão", FsmPolicy()))
    policies.append(("Aleatória", RandomPolicy(seed=args.layout_seed)))

    rows = []
    for name, pol in policies:
        rows.append((name,) + run_episodes(pol, args.layout_seed, args.episodes))

    print(f"N={args.episodes} episódios, layout_seed={args.layout_seed}, alvo X={args.target:.2f} (provisório)\n")
    print("| Política | Taxa de sucesso | Recompensa média | Fração no escuro |")
    print("|---|---:|---:|---:|")
    for name, sr, ret, dk in rows:
        print(f"| {name} | {sr:.1%} | {ret:.2f} | {dk:.1%} |")

    if rows[0][0].startswith("Q-table"):
        q_sr = rows[0][1]
        others = max(r[1] for r in rows[1:])
        print()
        print(f"alvo X: {'ATINGIDO' if q_sr >= args.target else 'NÃO atingido'} ({q_sr:.1%} vs {args.target:.0%})")
        print(f"supera baselines: {'SIM' if q_sr > others else 'NÃO'} ({q_sr:.1%} vs melhor baseline {others:.1%})")


if __name__ == "__main__":
    main()
