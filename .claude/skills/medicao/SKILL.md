---
name: medicao
description: Registra uma medição de laboratório em docs/30-Medicoes com CSV e nota, e atualiza as specs que dependem dela
disable-model-invocation: true
---
Registre a medição descrita em $ARGUMENTS (texto colado, CSV, foto ou caminho de arquivo).

1. Crie `docs/30-Medicoes/YYYY-MM-DD-<grandeza>.md` com: data, quem mediu, instrumento e configuração (janela de integração, escala), procedimento, tabela resumida (média, σ, N, mínimo, máximo) e link para o CSV bruto ao lado (`.csv`, mesmo nome).
2. Nunca "limpe" dados: outliers ficam no CSV, com coluna `obs`.
3. Procure em `docs/20-Specs/` critérios que esta medição prova ou derruba; cole o resultado na seção Evidência da spec e ajuste o status. Critério [LAB] só é atendido com a evidência colada.
4. Se o número contradiz uma ADR ou o prompt mestre (ex.: limiar do CPLD diferente do esperado), escreva uma nota em `docs/00-Inbox/` propondo a revisão — não edite `docs/10-Decisoes/`.
