---
name: sessao-fim
description: Fecha a sessão de trabalho com diário em docs/50-Diario e propostas para o CLAUDE.md
disable-model-invocation: true
---
Encerre a sessão.

1. `git status` e `git log --oneline -15`: liste commits desta sessão; se há mudanças não commitadas, pergunte se commita.
2. Crie `docs/50-Diario/YYYY-MM-DD-<titulo-curto>.md` a partir de `docs/_templates/Diario.md`: feito (commits), verificado (comandos e saída resumida), pendente para humano ([LAB] e decisões), aprendido.
3. Atualize o `status:` das specs tocadas em `docs/20-Specs/`.
4. Se descobriu um gotcha que vai se repetir, proponha a linha exata para o `CLAUDE.md` — não edite o arquivo; mostre o diff e peça aprovação.
5. Nunca marque critério [LAB] como verificado.
