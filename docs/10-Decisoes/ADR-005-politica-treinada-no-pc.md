---
status: aceita
data: 2026-10-22
decide: Luiz, Felipe
---
# ADR-005 — Política pequena treinada no PC, inferida em C++ no HPS

## Contexto
Sem Python na placa (ADR-001). Observações de baixa dimensão (lux ×2, distância ×2, PIR, bateria, inclinação, tempo desde o último estímulo). O app Android original já gera comandos `AT…` por Bluetooth — fonte gratuita de dados de teleoperação.

## Decisão
- Modelo: Q-table com observação discretizada **ou** MLP de dezenas de neurônios. Exportado como `weights.h` (array C) com vetores de teste entrada→saída verificados no ARM na inicialização.
- Treino: (a) simulador 2D em Python (gymnasium) com RL — Q-learning tabular primeiro, PPO só se falhar; (b) clonagem de comportamento a partir de logs de teleoperação, com uma rodada DAgger.
- Baselines obrigatórios: máquina de estados à mão e política aleatória, nos mesmos cenários e sementes.
- Estado interno de "humor" (medo, curiosidade, cansaço) modula velocidade e expressão.

## Alternativas descartadas
- LLM/redes grandes — não cabem (ADR-001).
- TensorFlow Lite Micro — viável, mas desnecessário para este tamanho; fica como opção se entrar keyword spotting/visão (fora de escopo).

## Consequências
Pasta `sim/` com simulador, treino e exportador. Camada de segurança fixa acima da política (ADR-007 não cobre; ver [[20-Specs/S00-visao]]). Sem baseline, nenhuma afirmação de "a IA aprendeu".
