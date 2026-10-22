#!/usr/bin/env python3
"""Exporta a Q-table para C (S05):

- firmware/src/brain/weights.h: `Q_ARGMAX[960]` (melhor ação por estado), `WEIGHTS_SHA`
  (sha256 do q_table.npy), `WEIGHTS_N_STATES`/`WEIGHTS_N_ACTIONS`.
- tests/policy_vectors.h e firmware/src/brain/policy_vectors.h (conteúdo idêntico): 200 pares
  {campos de Obs, estado esperado, ação esperada} gerados pelo Python, cobrindo todos os
  limites de discretização ±1 (limite -> nível inferior, limite+1 -> nível superior) e o
  valor 0 (inválido) de dist_front/dist_floor; o restante é aleatório com semente fixa.

Determinístico: mesma q_table -> mesmos 3 arquivos (byte a byte).
"""

import argparse
import hashlib
import os

import numpy as np

from aracno_sim.discretize import (
    DIST_FRONT_BINS, DIST_FRONT_LEVELS, F_EDGE_MM, FLOOR_LEVELS, LUX_BINS, LUX_LEVELS, N_STATES,
    PIR_LEVELS, T_SINCE_BINS, T_SINCE_LEVELS, state_index,
)
from aracno_sim.types import N_ACTIONS, Obs

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
N_VECTORS = 200
VECTOR_SEED = 20261022

HEADER_NOTE = "// Gerado por sim/export.py — NÃO EDITAR À MÃO. Regenerar: cd sim && python export.py\n"


def boundary_values(bins, extra=()):
    vals = list(extra)
    for b in bins:
        vals.extend((b - 1, b, b + 1))
    return vals


def make_vectors(argmax: np.ndarray):
    rng = np.random.default_rng(VECTOR_SEED)
    base = dict(lux_l=5, lux_r=5, dist_front_mm=1000, dist_floor_mm=80, pir=0, t_since_stimulus_ms=500)
    sweeps = {
        "lux_l": boundary_values(LUX_BINS, (0, 65535)),
        "lux_r": boundary_values(LUX_BINS, (0, 65535)),
        "dist_front_mm": boundary_values(DIST_FRONT_BINS, (0, 1, 65535)),
        "dist_floor_mm": boundary_values((F_EDGE_MM,), (0, 1, 65535)),
        "pir": [0, 1],
        "t_since_stimulus_ms": boundary_values(T_SINCE_BINS, (0, 0xFFFFFFFF)),
    }
    fields = []
    for name, values in sweeps.items():
        for v in values:
            d = dict(base)
            d[name] = v
            fields.append(d)
    assert len(fields) <= N_VECTORS, len(fields)

    # Combinações de limites nas duas features de lux (cobre o produto lux_l x lux_r)
    for ll in boundary_values(LUX_BINS):
        for lr in (10, 11):
            if len(fields) >= N_VECTORS - 40:
                break
            d = dict(base)
            d["lux_l"], d["lux_r"] = ll, lr
            fields.append(d)

    # Uma entrada por ação presente em Q_ARGMAX (exercita o caminho LUNGE/speed=100 no C++):
    # o primeiro estado de cada ação, decodificado para valores representativos.
    lux_rep = (0, LUX_BINS[1], LUX_BINS[2], 65535)
    dist_rep = (1, DIST_FRONT_BINS[1], DIST_FRONT_BINS[2], DIST_FRONT_BINS[3], 0)
    floor_rep = (F_EDGE_MM, 0)
    t_rep = (0, T_SINCE_BINS[1], 0xFFFFFFFF)
    for action in range(N_ACTIONS):
        states = np.flatnonzero(argmax == action)
        if len(states) == 0:
            continue
        s = int(states[0])
        s, t = divmod(s, T_SINCE_LEVELS)
        s, pir = divmod(s, PIR_LEVELS)
        s, fl = divmod(s, FLOOR_LEVELS)
        s, dist = divmod(s, DIST_FRONT_LEVELS)
        ll, lr = divmod(s, LUX_LEVELS)
        fields.append(dict(lux_l=lux_rep[ll], lux_r=lux_rep[lr], dist_front_mm=dist_rep[dist],
                           dist_floor_mm=floor_rep[fl], pir=pir, t_since_stimulus_ms=t_rep[t]))

    lux_pool = boundary_values(LUX_BINS, (0, 300, 1000, 65535))
    dist_pool = boundary_values(DIST_FRONT_BINS, (0, 1, 2000, 65535))
    floor_pool = [0, 1, 50, F_EDGE_MM - 1, F_EDGE_MM, F_EDGE_MM + 1, 500, 65535]
    t_pool = boundary_values(T_SINCE_BINS, (0, 1, 60000, 0xFFFFFFFF))
    while len(fields) < N_VECTORS:
        fields.append(dict(
            lux_l=int(lux_pool[rng.integers(len(lux_pool))]),
            lux_r=int(lux_pool[rng.integers(len(lux_pool))]),
            dist_front_mm=int(dist_pool[rng.integers(len(dist_pool))]),
            dist_floor_mm=int(floor_pool[rng.integers(len(floor_pool))]),
            pir=int(rng.integers(2)),
            t_since_stimulus_ms=int(t_pool[rng.integers(len(t_pool))]),
        ))

    vectors = []
    for d in fields:
        s = state_index(Obs(**d))
        vectors.append((d, s, int(argmax[s])))
    return vectors


def render_weights(argmax: np.ndarray, sha: str) -> str:
    lines = [HEADER_NOTE,
             "// Política tabular: melhor ação por estado (índice = aracno::state_index).",
             "#pragma once", "#include <stdint.h>", "", "namespace aracno {", "",
             f"static const uint16_t WEIGHTS_N_STATES = {N_STATES};",
             f"static const uint8_t  WEIGHTS_N_ACTIONS = {N_ACTIONS};",
             f'static const char* const WEIGHTS_SHA = "{sha}";  // sha256 de q_table.npy', "",
             f"static const uint8_t Q_ARGMAX[{N_STATES}] = {{"]
    for i in range(0, N_STATES, 24):
        chunk = ", ".join(str(int(v)) for v in argmax[i:i + 24])
        lines.append(f"    {chunk},")
    lines += ["};", "", "}  // namespace aracno", ""]
    return "\n".join(lines)


def render_vectors(vectors, sha: str) -> str:
    lines = [HEADER_NOTE,
             "// Vetores entrada->saída da política: estado esperado (Python) e ação esperada",
             "// (Q_ARGMAX[estado]). Cobrem todos os limites de discretização ±1.",
             f"// weights sha256: {sha}",
             "#pragma once", "#include <stdint.h>", "", "namespace aracno {", "",
             "struct PolicyVector {",
             "    uint16_t lux_l, lux_r, dist_front_mm, dist_floor_mm;",
             "    uint8_t  pir;",
             "    uint32_t t_since_stimulus_ms;",
             "    uint16_t state;   // esperado de aracno::state_index",
             "    uint8_t  action;  // esperado de Policy::act().action",
             "};", "",
             f"static const uint16_t N_POLICY_VECTORS = {len(vectors)};",
             f"static const PolicyVector POLICY_VECTORS[{len(vectors)}] = {{"]
    for d, s, a in vectors:
        lines.append(f"    {{{d['lux_l']}, {d['lux_r']}, {d['dist_front_mm']}, {d['dist_floor_mm']}, "
                     f"{d['pir']}, {d['t_since_stimulus_ms']}u, {s}, {a}}},")
    lines += ["};", "", "}  // namespace aracno", ""]
    return "\n".join(lines)


def export(q_path: str, root: str = ROOT):
    """Escreve os 3 headers; devolve os caminhos."""
    with open(q_path, "rb") as f:
        raw = f.read()
    sha = hashlib.sha256(raw).hexdigest()
    q = np.load(q_path)
    assert q.shape == (N_STATES, N_ACTIONS), q.shape
    argmax = np.argmax(q, axis=1).astype(np.uint8)
    vectors = make_vectors(argmax)
    assert len(vectors) == N_VECTORS

    outs = {
        os.path.join(root, "firmware", "src", "brain", "weights.h"): render_weights(argmax, sha),
        os.path.join(root, "tests", "policy_vectors.h"): render_vectors(vectors, sha),
        os.path.join(root, "firmware", "src", "brain", "policy_vectors.h"): render_vectors(vectors, sha),
    }
    for path, text in outs.items():
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", newline="\n") as f:
            f.write(text)
    return list(outs), sha


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--q", default="runs/q_table.npy")
    ap.add_argument("--root", default=ROOT, help="raiz do repo (default: pai de sim/)")
    args = ap.parse_args()
    paths, sha = export(args.q, args.root)
    print(f"q_table sha256={sha}")
    for p in paths:
        print(f"escrito {os.path.relpath(p, args.root)}")


if __name__ == "__main__":
    main()
