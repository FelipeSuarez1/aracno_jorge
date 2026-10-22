---
status: em-andamento
origem: [[20-Specs/S00-visao]] seção 4; [[10-Decisoes/ADR-005-politica-treinada-no-pc]]
depende: [[20-Specs/S03-firmware-base-e-seguranca]] (formato de Obs/Cmd e log)
---
# S05 — Simulador 2D, treino da política e exportação para C

## Objetivo
"Treinar a IA para ser aranha": um simulador 2D em Python onde a aranha aprende, por reforço, a esconder-se no escuro, espreitar e dar o bote; a política resultante vira `weights.h` e roda em C++ na placa, com baselines (FSM à mão e aleatória) para provar que aprendeu.

## Fora de escopo
Aprender a andar (ADR-004). Clonagem por teleoperação (S07, precisa do log real). Humor (entra como modulação em S07).

## Fatos de entrada
- Observação e ações são as do `struct Obs`/`Cmd` de S03. Dinâmica real (velocidade cm/s, °/s, distância de frenagem) **a medir** em S03 [LAB]; até lá o simulador usa valores com `[A MEDIR]` e o treino é refeito depois.
- Modelos de ruído dos sensores: σ do VL53L1X por distância × superfície × lux, tempo de aquecimento/retrigger do PIR, resposta do BH1750 — caracterização [LAB] desta spec.

## Design
- `sim/aracno_env.py`: ambiente **gymnasium**. Cômodo retangular com obstáculos, 1–3 fontes de luz (mapa de lux), pessoa com trajetória roteirizada (entra, para, sai; 0,5–1,5 m/s), bateria decrescente. Observação = `Obs` discretizada (lux esq/dir em 4 níveis, distância frente em 5, chão em 2, PIR, tempo desde estímulo em 3). Ações = {parar, avançar, virar esq, virar dir, abaixar, bote, recuar, virar cabeça esq/dir}. Recompensa: +escuro, +aproximar sem PIR da pessoa "ver" (modelo simples de campo de visão), +bote a < 60 cm, −colisão, −borda, −bateria baixa, −bote longe.
- `sim/train_q.py`: Q-learning tabular, sementes fixas, `requirements.txt` pinado; curva de aprendizado salva; avaliação em 1 000 episódios em layouts **não vistos**.
- `sim/train_ppo.py`: só se o tabular não atingir a métrica (Stable-Baselines3, MLP 2×32).
- `sim/baselines.py`: FSM à mão (a mesma lógica do `fsm_demo` de S03, estendida) e política aleatória.
- `sim/export.py`: Q-table ou MLP → `firmware/src/brain/weights.h` (arrays `const`, discretização idêntica) + `tests/policy_vectors.h` com 200 pares entrada→saída.
- `firmware/src/brain/policy.cpp`: inferência (lookup ou MLP com ReLU em ponto fixo/float), sem alocação; na inicialização verifica os 200 vetores e recusa rodar se divergir.

## Critérios de aceite
- [PC] Ambiente passa `gymnasium.utils.env_checker`.
- [PC] Treino reprodutível: duas execuções com a mesma semente geram `weights.h` idêntico (hash).
- [PC] Métrica em simulação, fixada antes de treinar: taxa de sucesso (episódio termina com bote válido sem colisão/borda e ≥ 70 % do tempo no escuro) ≥ **alvo X** em layouts não vistos, **superando FSM à mão e aleatória** no mesmo conjunto e sementes. X é definido pelos autores antes do primeiro treino e registrado aqui.
- [PC] `tests/policy.cpp`: os 200 vetores passam no C++; discretização em C++ = Python nos limites exatos.
- [LAB] Caracterização dos sensores para o simulador: σ do VL53L1X por distância (10–200 cm) × superfície (3) × lux (2), N ≥ 100 por célula; BH1750 vs. luxímetro em 5 pontos; PIR aquecimento e retrigger. CSVs em `30-Medicoes/`.
- [LAB] Validação sim vs. real: histogramas por feature dos logs reais (S07) vs. do simulador — divergência discutida por escrito.

## Verificação
- [PC]: `cd sim && pytest`; `python train_q.py --seed 1` duas vezes → mesmo hash; `python eval.py` imprime tabela política/FSM/aleatória.
- [LAB]: planilhas de caracterização; `/medicao`.

## Plano
1. `periferia` — `sim/aracno_env.py` + testes do ambiente.
2. `periferia` — baselines + `eval.py` com tabela.
3. `periferia` — `train_q.py`, `export.py`, `tests/policy.cpp`, `brain/policy.cpp`.
4. [LAB] caracterização dos sensores → atualizar modelos de ruído → retreinar.
5. `núcleo` — ligar `policy.cpp` no laço de S03 **abaixo** da camada de segurança; teste na aranha suspensa; depois no chão com cenários roteirizados (S00).

## Evidência
### 2026-10-22 — passos 1–3 [PC] feitos (agente `SimPolitica`, 7 commits)
- `cd sim && python -m pytest` → 10 passed. `train_q.py --seed 1 --episodes 2000` duas vezes → mesmo sha256 (`789a1253…321ed9`).
- `export.py` → `firmware/src/brain/weights.h` (sha `4340419e…65d7`), `policy_vectors.h` ×2; `tests/policy.cpp`: 200 vetores ok → C++ e Python discretizam igual.
- `eval.py --episodes 1000 --layout-seed 8` (layouts nunca vistos):

| Política | Sucesso | Recompensa média | Tempo no escuro |
|---|---:|---:|---:|
| Q-table (seed 2) | **6,6 %** | **4,64** | 44,9 % |
| FSM à mão | 5,9 % | −2,15 | 56,4 % |
| Aleatória | 0,3 % | −36,84 | 56,6 % |

- Ordem política > FSM > aleatória mantida, mas a vantagem em **sucesso** é marginal (σ ≈ 0,8 %). **Alvo X = 0,7 (provisório) não atingido** — e uma tabela reativa escrita à mão atinge só 13,7 % no mesmo simulador: é o teto de qualquer política tabular com esta `Obs`.
- Causa estrutural (relatório do agente): nada em `Obs` distingue **pessoa de parede** — só 5–8 % das leituras "PIR=1 e dist ≤ 600 mm" são pessoa. Decisão de modelagem tomada pelo agente e que os autores devem revisar: recompensa do bote mudada de +10/−2 para **+20/−1**, porque com a original a política ótima era "nunca dar o bote" (valor esperado negativo). Ver [[00-Inbox/politica-nao-distingue-pessoa-de-parede]].
- `Policy` ligada no laço (`aracno --policy`): `self_test()` na inicialização, recusa rodar se divergir; sha dos pesos no console.

### Pendente
- Autores fixam X e decidem sobre a recompensa do bote (Inbox).
- [LAB] caracterização dos sensores e da dinâmica (`V_FORWARD_MM_S`, `TURN_RAD_S`, `LUNGE_MM`, `FLOOR_HEIGHT_MM` em `env.py` marcados `[A MEDIR]`); retreinar.
