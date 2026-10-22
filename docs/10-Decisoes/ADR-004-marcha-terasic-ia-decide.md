---
status: aceita
data: 2026-10-22
decide: Luiz, Felipe (com o professor)
---
# ADR-004 — A marcha é a da Terasic; a IA aprende a decidir, não a andar

## Contexto
Os MG996R não têm leitura de posição; aprender locomoção por RL com sim-to-real em servo barato é pesquisa, não TCC. A marcha tripé da Terasic já funciona (`CSpider`, [[40-Fontes/spider-programming-guide]] §2.1, cap. 4).

## Decisão
Reutilizar `CSpider`/`CSpiderLeg`/`CMotor` e os 18 controladores PWM da FPGA. "Treinar a IA para ser aranha" = treinar a **política de decisão** (esconder, espreitar, bote, recuar, virar, abaixar, virar cabeça), não a marcha.

## Alternativas descartadas
- RL de locomoção em simulador físico — sem feedback de posição, sem tempo.
- Cinemática inversa própria — só se a marcha pronta não permitir "abaixar corpo"/bote; o guia já tem `ATTTF` (inclinar para frente) e `ATSP=` (velocidade).

## Consequências
Ações da política mapeiam 1:1 para métodos de `CSpider` ou comandos `AT…`. Qualquer movimento novo (ex.: bote) é composição de primitivas existentes antes de ser movimento novo.
