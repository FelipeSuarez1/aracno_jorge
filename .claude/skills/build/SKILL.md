---
name: build
description: Compila o firmware cross estático no container e roda os testes de host; mostra evidência
disable-model-invocation: true
---
1. `make -C firmware test` — se falhar, corrija antes de qualquer cross-build.
2. `tools/cross-build.sh clean all` — faz o `docker build` se a imagem não existir.
3. A última linha deve conter `ARM` e `statically linked`; senão, falhou.
4. Mostre a saída dos passos 1 e 2 verbatim (sem resumir) como evidência.
