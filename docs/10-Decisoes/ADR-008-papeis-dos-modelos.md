---
status: aceita
data: 2026-11-06 (Luiz aprovou o plano "medula-fpga": Fable aprova, Sonnet escreve, Opus revisa)
decide: Luiz, Felipe
---
# ADR-008 — Quem planeja, quem escreve, quem revisa

## Contexto
Todo código é escrito por IA. Proposta do Luiz: fable planeja e conversa; opus revisa; modelos rápidos escrevem. Princípio das práticas da Anthropic: quem escreve não avalia o próprio trabalho; a verificação vem de testes que devolvem pass/fail e de uma segunda opinião em contexto limpo.

## Decisão
| Papel | Modelo | Onde | Regra |
|---|---|---|---|
| Planejar, conversar e aprovar o plano | fable | sessão principal, plan mode | lê specs/ADRs/medições; propõe plano; um Fable em contexto limpo critica; humano aprova |
| Orquestrar a execução | modelo da sessão | sessão principal | não escreve código; delega, verifica, integra, commita e registra evidência |
| Escrever (periferia e núcleo) | sonnet | subagente `task` com o plano aprovado | RTL, firmware, testes, specs |
| Revisar | opus (diferente de quem escreveu) | agente somente-leitura `reviewer` com os critérios de `reviewer-seguranca.md` | bloqueia commit se achar violação de regra do CLAUDE.md, da spec ou de ADR aceita |
| Aprovar | Luiz, Felipe | plano/commit | lêem a evidência (saída de teste, log, foto), não o diff inteiro |

## Alternativas descartadas
- Um único modelo para tudo — sem segunda opinião, sem contexto limpo na revisão.
- Modelo rápido escrevendo tudo — barato, mas o núcleo tem pouca cobertura de teste automática (depende de lab).

## Consequências
Núcleo escrito por sonnet exige testbench com mutações e revisão opus antes do commit; `.claude/agents/reviewer-seguranca.md` mantém `model: opus`; os modelos dos subagentes ficam em `task.agentModelOverrides` da configuração do omp.
