# aracno_jorge — mapa

Aranha Terasic Spider + DE0-Nano-SoC, cabeça pan/tilt com rosto, IA de decisão. Sem troca de Linux na DE0, sem voz, sem SLAM na DE0. Visão: Raspberry Pi 4 + RealSense R200, ROS 2 só no Pi; reconhecimento na FPGA; a DE0 decide e move, com firmware C++ estático (ADR-011). LiDAR Mapper: proposta no acréscimo da ADR-011, ainda não decidida (ADR-003 continua valendo para SLAM).

## Como usar este vault
1. Ideia nova → nota em [[00-Inbox]] (texto livre).
2. A IA transforma em spec (`/spec`) em [[20-Specs]].
3. Decisão de arquitetura → ADR em [[10-Decisoes]] (só humano aprova).
4. Medição no lab → CSV + nota em [[30-Medicoes]].
5. Fim de sessão de IA → nota em [[50-Diario]].

## Estado atual
- Hardware identificado: [[40-Fontes/hardware]]
- Visão e prompt mestre: [[20-Specs/S00-visao]]
- Dia 1 e toolchain: [[20-Specs/S01-dia-1-uart]] ✅ · [[20-Specs/S02-toolchain-hello-world]] ✅ · [[30-Medicoes/dia-1]]
- Próximo passo humano: aceitar [[00-Inbox/proposta-ADR-009-rota-hps]] e [[10-Decisoes/ADR-008-papeis-dos-modelos]]; comprar [[Compras]]
- Próximo passo IA (após aprovação das specs): [[20-Specs/S03-firmware-base-e-seguranca]]

## Specs
- [[20-Specs/S00-visao]] · [[20-Specs/S01-dia-1-uart]] · [[20-Specs/S02-toolchain-hello-world]]
- [[20-Specs/S03-firmware-base-e-seguranca]] (rascunho) · [[20-Specs/S04-display-rosto-balao]] (rascunho) · [[20-Specs/S05-simulador-e-politica]] (rascunho) · [[20-Specs/S06-cabeca-pan-tilt]] (rascunho)
- [[20-Specs/S08-fpga-gpio0-display-cabeca]] · [[20-Specs/S09-camera-ov7670]] (suspensa) · [[20-Specs/S10-enlace-pi-de0]] (rascunho) · [[20-Specs/S11-medula-fpga]] (aprovada)

## Decisões
- [[10-Decisoes/ADR-001-sem-placa-companheira]]
- [[10-Decisoes/ADR-002-sem-fala-display-com-texto]]
- [[10-Decisoes/ADR-003-sem-camera-sem-geolocalizacao]]
- [[10-Decisoes/ADR-004-marcha-terasic-ia-decide]]
- [[10-Decisoes/ADR-005-politica-treinada-no-pc]]
- [[10-Decisoes/ADR-006-barramento-decidido-no-dia-1]]
- [[10-Decisoes/ADR-007-cabeca-pan-tilt]]
- [[10-Decisoes/ADR-008-papeis-dos-modelos]]
- [[10-Decisoes/ADR-010-camera-na-fpga]]
- [[10-Decisoes/ADR-011-pi-ros-visao-na-fpga]]

## Fontes
- [[40-Fontes/hardware]] · [[40-Fontes/fontes-externas]]
- Manuais em texto: [[40-Fontes/spider-user-manual]] · [[40-Fontes/spider-programming-guide]] · [[40-Fontes/smk-servo-motor-card-manual]] · [[40-Fontes/de0-nano-soc-user-manual]]
