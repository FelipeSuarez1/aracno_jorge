import hashlib
import os
import re

import numpy as np

import export
import train_q
from aracno_sim import N_STATES, N_ACTIONS
from aracno_sim.discretize import DIST_FRONT_BINS, F_EDGE_MM, LUX_BINS, T_SINCE_BINS


def _train_and_hash(tmp_path, name, seed=1, episodes=40):
    q, curve = train_q.train(seed, episodes, curve_path=str(tmp_path / f"{name}.csv"))
    path = tmp_path / f"{name}.npy"
    np.save(path, q)
    assert q.shape == (N_STATES, N_ACTIONS)
    assert len(curve) == episodes
    return train_q.sha256_file(str(path)), path


def test_training_is_reproducible(tmp_path):
    h1, _ = _train_and_hash(tmp_path, "a")
    h2, _ = _train_and_hash(tmp_path, "b")
    assert h1 == h2
    h3, _ = _train_and_hash(tmp_path, "c", seed=2)
    assert h3 != h1


def test_export_is_deterministic_and_covers_boundaries(tmp_path):
    _, qpath = _train_and_hash(tmp_path, "q", episodes=10)
    root1, root2 = tmp_path / "r1", tmp_path / "r2"
    paths1, sha1 = export.export(str(qpath), str(root1))
    paths2, sha2 = export.export(str(qpath), str(root2))
    assert sha1 == sha2 == train_q.sha256_file(str(qpath))
    assert len(paths1) == 3
    for p1, p2 in zip(paths1, paths2):
        b1, b2 = open(p1, "rb").read(), open(p2, "rb").read()
        assert hashlib.sha256(b1).hexdigest() == hashlib.sha256(b2).hexdigest()
    vec_test = open(os.path.join(root1, "tests", "policy_vectors.h")).read()
    vec_fw = open(os.path.join(root1, "firmware", "src", "brain", "policy_vectors.h")).read()
    assert vec_test == vec_fw

    rows = re.findall(r"^\s+\{(\d+), (\d+), (\d+), (\d+), (\d+), (\d+)u, (\d+), (\d+)\},$", vec_test, re.M)
    assert len(rows) == 200
    rows = [tuple(int(x) for x in r) for r in rows]
    lux_l = {r[0] for r in rows}
    lux_r = {r[1] for r in rows}
    dist = {r[2] for r in rows}
    floor = {r[3] for r in rows}
    t = {r[5] for r in rows}
    for b in LUX_BINS:
        assert {b, b + 1} <= lux_l and {b, b + 1} <= lux_r
    for b in DIST_FRONT_BINS:
        assert {b, b + 1} <= dist
    assert {0, F_EDGE_MM, F_EDGE_MM + 1} <= floor and 0 in dist
    for b in T_SINCE_BINS:
        assert {b, b + 1} <= t
    assert {r[4] for r in rows} == {0, 1}
    assert all(0 <= r[6] < N_STATES and 0 <= r[7] < N_ACTIONS for r in rows)

    weights = open(os.path.join(root1, "firmware", "src", "brain", "weights.h")).read()
    assert f"WEIGHTS_SHA = \"{sha1}\"" in weights
    assert f"Q_ARGMAX[{N_STATES}]" in weights
