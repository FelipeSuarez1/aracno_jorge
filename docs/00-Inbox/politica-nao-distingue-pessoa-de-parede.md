---
data: 2026-10-22
origem: relatório do agente SimPolitica (S05)
decide: Luiz, Felipe
---
# A política não consegue distinguir pessoa de parede — o que fazer?

**Fato (simulação):** com a `Obs` atual (lux ×2, distância frontal, chão, PIR, tempo desde estímulo), só 5–8 % das situações "PIR=1 e objeto ≤ 600 mm" são pessoa; o resto é parede/caixa. Resultado: a melhor política tabular possível chega a ~14 % de sucesso no simulador; a treinada ficou em 6,6 % (FSM 5,9 %, aleatória 0,3 %). O alvo provisório X = 70 % é inatingível assim.

**Decisão já tomada pelo agente, a revisar:** recompensa do bote +10/−2 → +20/−1 (`sim/aracno_sim/env.py`). Sem isso a política aprendia "nunca dar o bote". Se preferirem a original, o resultado é uma aranha que só se esconde.

**Opções para os autores:**
1. Nova feature em `Obs`: `dist_front` **diminuindo** nos últimos N ciclos (coisa que se aproxima = pessoa; parede não anda). Barato: só firmware + contrato + retreino.
2. Usar a varredura da cabeça (S06) como feature: objeto estreito com fundo longe = pessoa/perna; objeto largo = parede.
3. Aceitar X baixo e medir a diferença política vs. FSM no real (20 cenários roteirizados da S00) — o "aprendeu" vira a comparação relativa, não o valor absoluto.
4. Combinar 1+2.

Recomendação da IA: 1 agora (muda `types.h`/`discretize.h`, `env.py`, vetores; um dia de trabalho) e 2 quando a cabeça existir. Fixar X só depois de retreinar com a feature nova.
