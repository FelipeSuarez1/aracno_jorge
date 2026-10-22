#!/usr/bin/env python3
"""Q-learning tabular (960 estados x 9 ações) no AracnoEnv (S05).

Taxa de aprendizado alpha/(1+n_sa)^alpha_decay (n_sa = visitas de (s,a)); o default
alpha=1, alpha_decay=1 é a média amostral clássica — nas varreduras de S05 deu retorno e
taxa de sucesso mais estáveis entre sementes do que alpha fixo ou traços de elegibilidade.

Reprodutível: mesma --seed -> mesmo runs/q_table.npy (sha256 igual). Toda aleatoriedade vem
de um único numpy.random.Generator(seed) e das sementes de episódio derivadas dele.

Sementes de episódio de treino: TRAIN_SEED_BASE + seed*10**6 + ep. As de avaliação (eval.py)
ficam em outra faixa (EVAL_SEED_BASE), portanto os layouts avaliados nunca foram vistos.
"""

import argparse
import csv
import hashlib
import os

import numpy as np

from aracno_sim import AracnoEnv, N_STATES, N_ACTIONS

TRAIN_SEED_BASE = 0
EVAL_SEED_BASE = 10**9


def episode_seed(seed: int, ep: int) -> int:
    assert 0 <= seed < 1000 and 0 <= ep < 10**6
    return TRAIN_SEED_BASE + seed * 10**6 + ep


def train(seed: int, episodes: int, alpha: float = 1.0, gamma: float = 0.95,
          alpha_decay: float = 1.0, eps_start: float = 1.0, eps_end: float = 0.05,
          eps_decay_frac: float = 0.8, curve_path: str = None, noise: dict = None):
    """Devolve (q_table float64 [N_STATES, N_ACTIONS], lista de (ep, retorno, sucesso, passos))."""
    rng = np.random.default_rng(seed)
    env = AracnoEnv(noise=noise)
    q = np.zeros((N_STATES, N_ACTIONS), dtype=np.float64)
    visits = np.zeros((N_STATES, N_ACTIONS), dtype=np.int64)
    curve = []
    decay_eps = max(1, int(episodes * eps_decay_frac))
    for ep in range(episodes):
        eps = eps_start + (eps_end - eps_start) * min(1.0, ep / decay_eps)
        s, info = env.reset(seed=episode_seed(seed, ep))
        total = 0.0
        steps = 0
        done = False
        while not done:
            row = q[s]
            best = np.flatnonzero(row == row.max())
            greedy = int(best[0]) if len(best) == 1 else int(best[rng.integers(len(best))])
            if rng.random() < eps:
                a = int(rng.integers(N_ACTIONS))
            else:
                a = greedy
            s2, r, term, trunc, info = env.step(a)
            target = r if term else r + gamma * q[s2].max()
            visits[s, a] += 1
            a_eff = alpha / (1.0 + visits[s, a]) ** alpha_decay if alpha_decay > 0.0 else alpha
            q[s, a] += a_eff * (target - q[s, a])
            total += r
            steps += 1
            s = s2
            done = term or trunc
        curve.append((ep, total, int(info["success"]), steps))
    if curve_path:
        os.makedirs(os.path.dirname(curve_path) or ".", exist_ok=True)
        with open(curve_path, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["episode", "return", "success", "steps"])
            w.writerows(curve)
    return q, curve


def sha256_file(path: str) -> str:
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    # seed 2: melhor de {1,2,3} na validação (layout_seed=7, 1000 episódios); gera o weights.h commitado
    ap.add_argument("--seed", type=int, default=2)
    ap.add_argument("--episodes", type=int, default=10000)
    ap.add_argument("--alpha", type=float, default=1.0)
    ap.add_argument("--gamma", type=float, default=0.95)
    ap.add_argument("--alpha-decay", type=float, default=1.0, help="expoente p em alpha/(1+n_sa)^p (0 = alpha fixo)")
    ap.add_argument("--out", default="runs/q_table.npy")
    ap.add_argument("--curve", default=None, help="CSV da curva (default runs/learning_curve_seed<N>.csv)")
    args = ap.parse_args()
    curve = args.curve or f"runs/learning_curve_seed{args.seed}.csv"

    q, hist = train(args.seed, args.episodes, alpha=args.alpha, gamma=args.gamma,
                    alpha_decay=args.alpha_decay, curve_path=curve)
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    np.save(args.out, q)

    last = hist[-max(1, len(hist) // 10):]
    print(f"episódios={args.episodes} seed={args.seed}")
    print(f"últimos {len(last)}: retorno médio={np.mean([h[1] for h in last]):.2f} "
          f"sucesso={np.mean([h[2] for h in last]):.2%}")
    print(f"q_table: {args.out} sha256={sha256_file(args.out)}")
    print(f"curva: {curve}")


if __name__ == "__main__":
    main()
