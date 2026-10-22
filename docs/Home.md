# aracno_jorge — mapa

Aranha Terasic Spider + DE0-Nano-SoC, cabeça pan/tilt com rosto, IA de decisão. Sem Pi, sem fala, sem SLAM.

## Como usar este vault
1. Ideia nova → nota em [[00-Inbox]] (texto livre).
2. A IA transforma em spec (`/spec`) em [[20-Specs]].
3. Decisão de arquitetura → ADR em [[10-Decisoes]] (só humano aprova).
4. Medição no lab → CSV + nota em [[30-Medicoes]].
5. Fim de sessão de IA → nota em [[50-Diario]].

## Estado atual
- Hardware identificado: [[40-Fontes/hardware]]
- Visão e prompt mestre: [[20-Specs/S00-visao]]
- Próximo passo humano: [[20-Specs/S01-dia-1-uart]]
- Próximo passo IA: [[20-Specs/S02-toolchain-hello-world]]

## Decisões
- [[10-Decisoes/ADR-001-sem-placa-companheira]]
- [[10-Decisoes/ADR-002-sem-fala-display-com-texto]]
- [[10-Decisoes/ADR-003-sem-camera-sem-geolocalizacao]]
- [[10-Decisoes/ADR-004-marcha-terasic-ia-decide]]
- [[10-Decisoes/ADR-005-politica-treinada-no-pc]]
- [[10-Decisoes/ADR-006-barramento-decidido-no-dia-1]]
- [[10-Decisoes/ADR-007-cabeca-pan-tilt]]
- [[10-Decisoes/ADR-008-papeis-dos-modelos]] (proposta)

## Fontes
- [[40-Fontes/hardware]] · [[40-Fontes/fontes-externas]]
- Manuais em texto: [[40-Fontes/spider-user-manual]] · [[40-Fontes/spider-programming-guide]] · [[40-Fontes/smk-servo-motor-card-manual]] · [[40-Fontes/de0-nano-soc-user-manual]]
