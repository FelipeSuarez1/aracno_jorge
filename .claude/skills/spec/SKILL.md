---
name: spec
description: Transforma uma nota do Inbox em spec numerada em docs/20-Specs seguindo o template
disable-model-invocation: true
---
Transforme a nota $ARGUMENTS (caminho em docs/00-Inbox/ ou texto livre) em uma spec.

1. Leia `docs/_templates/Spec.md`, `docs/20-Specs/S00-visao.md` e as ADRs em `docs/10-Decisoes/`. Se a ideia contraria uma ADR aceita, pare e diga qual.
2. Escolha o próximo número `Sxx` livre em `docs/20-Specs/`.
3. Preencha todas as seções do template. Critérios de aceite numéricos; cada um marcado [PC] ou [LAB]. Reaproveite critérios de S00 quando couber.
4. Marque cada tarefa do plano como `periferia` ou `núcleo` (ADR-008).
5. Cite fatos só de `docs/40-Fontes/` ou `docs/30-Medicoes/`; o resto é [INFERÊNCIA].
6. Mova a nota de origem para `docs/00-Inbox/_consumidas/` e aponte `origem:` para ela.
7. Adicione a spec em `docs/Home.md`. Não implemente nada; termine listando o que o humano precisa aprovar.
