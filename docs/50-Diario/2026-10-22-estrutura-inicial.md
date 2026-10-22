---
data: 2026-10-22
modelo: fable (planejamento e escrita desta sessão)
specs: [[20-Specs/S00-visao]], [[20-Specs/S01-dia-1-uart]], [[20-Specs/S02-toolchain-hello-world]]
---
# Sessão 2026-10-22 — estrutura inicial do repositório

## Feito
- Layout do repo: `docs/` (vault Obsidian), `firmware/`, `fpga/`, `sim/`, `tools/`, `tests/`, `.claude/`.
- `CLAUDE.md` (34 linhas), templates ADR/Spec/Diário, `Home.md`.
- ADR-001..007 aceitas (decisões da sessão de planejamento), ADR-008 (papéis dos modelos) como proposta.
- S00 visão + prompt mestre; S01 dia 1 na UART (humano); S02 toolchain — parte [PC] concluída.
- Manuais Terasic (Spider UM, Programming Guide, SMK, DE0-Nano-SoC) importados em texto para `40-Fontes/`, mais `hardware.md`, `fontes-externas.md` e a análise `aprofundar-v2.md`.
- `tools/Dockerfile` (musl.cc GCC 11.2.1, SHA-256 fixado), `tools/cross-build.sh`, `firmware/Makefile`, `firmware/src/main.cpp` (hello world estático).
- `.claude/`: skills `spec`, `sessao-fim`, `medicao`, `build`; agentes `reviewer-seguranca` (opus, somente leitura) e `pesquisador-fontes`; hook que roda `make -C firmware test` após edição em `firmware/` ou `tests/`; permissão negada para editar `docs/10-Decisoes/`.

## Verificado
- `docker build -t aracno-tc tools/` → exit 0.
- `tools/cross-build.sh clean all` → `ELF 32-bit LSB executable, ARM, EABI5, statically linked, stripped`, 21 736 bytes.
- `make -C firmware test` → "0 testes (aceitável até S03)", exit 0.

## Pendente para humano
- S01 inteira (backup da microSD, comandos na UART, conta Terasic + CD → `40-Fontes/spider-cd/`).
- S02 [LAB]: rodar `aracno` na placa e colar a saída.
- Aprovar ou ajustar ADR-008 (quem planeja/escreve/revisa).
- Abrir `docs/` no Obsidian e conferir os links.

## Aprendido
- Fedora + docker: bind mount exige `:z`; sem `--user` os artefatos ficam de root. Embutido no script.
- musl estático gera `static-pie` por padrão; `-no-pie` resolve. Não há binfmt ARM neste PC; execução só na placa.
