# sim — simulador 2D, treino da política e exportação (S05)

Spec: `docs/20-Specs/S05-simulador-e-politica.md`. Contrato Obs/Cmd/discretização: `docs/20-Specs/contrato-obs-cmd.md` (fonte de verdade em `firmware/include/aracno/{types,discretize}.h`, espelhados em `aracno_sim/{types,discretize}.py`).

## Instalar
```sh
cd sim
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt   # gymnasium, numpy, pytest pinados
```

## Rodar
```sh
.venv/bin/python train_q.py                      # seed 2, 10 000 episódios (~80 s) -> runs/q_table.npy (+ sha256) e runs/learning_curve_seed2.csv
.venv/bin/python train_q.py --seed 1 --episodes 2000   # verificação de reprodutibilidade: rodar 2x -> mesmo sha256
.venv/bin/python eval.py --episodes 1000 --target 0.7  # tabela Q-table / FSM / aleatória em layouts não vistos
.venv/bin/python export.py                             # -> firmware/src/brain/weights.h, firmware/src/brain/policy_vectors.h, tests/policy_vectors.h
make -C ../firmware test                               # tests/policy: C++ discretiza igual ao Python e Policy::self_test()
```
`runs/` é ignorado pelo git; os três headers gerados são commitados.

## Arquivos
- `aracno_sim/env.py` — `AracnoEnv` (gymnasium, `Discrete(960)` → `Discrete(9)`): cômodo com paredes, caixas, buracos (borda), 1–3 luzes com sombra, pessoa roteirizada (entra, para a 0,6–2 m da aranha, sai; 0,5–1,5 m/s), bateria. `info["obs"]` traz a `Obs` crua. Ruído em `DEFAULT_NOISE` (tudo `[A MEDIR]`, substituir pela caracterização de S03/S05).
- `baselines.py` — `FsmPolicy` (esconder → espreitar com PIR → bote a ≤ 600 mm → recuar), `RandomPolicy`, `TabularPolicy`; interface `act(state_index, obs) -> action`.
- `train_q.py` — Q-learning tabular (taxa `alpha/(1+n_sa)^alpha_decay`, default média amostral; γ = 0,95); sementes de episódio `seed*10**6 + ep`. Seed 2 é a melhor de {1,2,3} na validação (`--layout-seed 7`) e gera o `weights.h` commitado; o relatório final usa `--layout-seed 8`.
- `eval.py` — sementes de layout `10**9 + layout_seed*10**6 + i` (disjuntas do treino). Sucesso = bote válido, sem colisão nem borda, ≥ 70 % do tempo no escuro. Alvo X: `--target` (0,7 provisório, a fixar pelos autores em S05).
- `export.py` — `Q_ARGMAX[960]` + `WEIGHTS_SHA` e 200 vetores de teste cobrindo todos os limites de discretização ±1.
