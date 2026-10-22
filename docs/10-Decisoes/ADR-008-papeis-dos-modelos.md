---
status: proposta
data: 2026-10-22
decide: Luiz, Felipe
---
# ADR-008 — Quem planeja, quem escreve, quem revisa

## Contexto
Todo código é escrito por IA. Proposta do Luiz: fable planeja e conversa; opus revisa; modelos rápidos escrevem. Princípio das práticas da Anthropic: quem escreve não avalia o próprio trabalho; a verificação vem de testes que devolvem pass/fail e de uma segunda opinião em contexto limpo.

## Decisão (proposta)
| Papel | Modelo | Onde | Regra |
|---|---|---|---|
| Planejar e conversar | fable (raciocínio forte) | sessão principal, plan mode | lê specs/ADRs/medições; propõe plano; humano aprova |
| Escrever — periferia | modelo rápido (sonnet/haiku-class) | subagente `task`/`sonic` com spec + plano | scripts, sprites, simulador, docs, testes de periferia |
| Escrever — núcleo | modelo forte (opus/fable) | sessão síncrona, humano acompanhando | camada de segurança, controle de servos, bote, FPGA, driver do display |
| Revisar | modelo forte **diferente** do que escreveu (opus) | agente somente-leitura `reviewer-seguranca`/`reviewer` | bloqueia commit se achar violação de regra do CLAUDE.md ou da spec |
| Aprovar | Luiz, Felipe | PR/commit | lêem a evidência (saída de teste, log, foto), não o diff inteiro |

Ajuste em relação à proposta original: **núcleo não é escrito por modelo rápido**. O custo de um bug na camada de segurança é um servo quebrado ou um bote em pessoa; modelo rápido escreve onde o teste de host pega o erro.

## Alternativas descartadas
- Um único modelo para tudo — sem segunda opinião, sem contexto limpo na revisão.
- Modelo rápido escrevendo tudo — barato, mas o núcleo tem pouca cobertura de teste automática (depende de lab).

## Consequências
`.claude/agents/reviewer-seguranca.md` com `model: opus` e ferramentas somente-leitura. Specs marcam cada tarefa como `periferia` ou `núcleo`. O planejador delega periferia em lote e acompanha núcleo passo a passo.
