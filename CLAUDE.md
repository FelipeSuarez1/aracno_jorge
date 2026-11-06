# aracno_jorge — aranha Terasic Spider com cabeça e IA de decisão

Hexápode Terasic Spider (18× MG996R) + DE0-Nano-SoC (Cyclone V: FPGA + ARM Cortex-A9 dual, Linux Yocto 2015) + Servo Motor Card.
Todo código é escrito pela IA. Luiz e Felipe decidem, medem no lab e revisam. Responda sempre em português.

## Decisões fechadas (não reabrir; detalhes em docs/10-Decisoes/)
- Sem troca de Linux na DE0, sem voz, sem SLAM na DE0. Visão: Raspberry Pi 4 + RealSense R200, ROS 2 só no Pi; reconhecimento na FPGA; a DE0 decide e move, com firmware C++ estático (ADR-011). LiDAR Mapper: proposta no acréscimo da ADR-011, ainda não decidida (ADR-003 continua valendo para SLAM).
- Marcha é a da Terasic (CSpider). A IA aprende a DECIDIR, não a andar.
- IA = política pequena treinada no PC (sim/ + teleoperação), exportada como `weights.h`, inferida em C++.
- Camada de segurança fixa acima da política. Chave da Servo Motor Card = E-stop.
- Cabeça pan/tilt (2× MG90S) com display SPI, VL53L1X frontal, BH1750 ×2; VL53L1X para o chão; PIR no corpo.
- Rota de barramento (HPS vs FPGA) decidida pelo S01 (dia 1 na UART). Até lá, não assumir `/dev/i2c-1` nem `/dev/spidev`.

## Layout
- `docs/` vault Obsidian: `00-Inbox` ideias, `10-Decisoes` ADRs, `20-Specs` specs, `30-Medicoes` CSV, `40-Fontes` manuais, `50-Diario` sessões.
- `firmware/` C++ do HPS · `fpga/` Quartus · `sim/` simulador e treino · `tools/` build/flash · `tests/` testes de host.

## Comandos
- Build cross (estático, musl, em container): `tools/cross-build.sh` (faz `docker build` na primeira vez)
- Testes de host: `make -C firmware test`
- microSD no PC: só `udisksctl unmount -b /dev/mmcblk0p1`. NUNCA `udisksctl power-off`: desliga o leitor interno (porta USB 3-3) e só volta com root.
- Placa pela UART (sem rede): `tools/uart.py run "cmd"`, `tools/uart.py push local /home/root/x`, `tools/uart.py boot`. Servo Motor Card OFF em teste de software.

## Regras de trabalho
- Fluxo: Inbox → `/spec` → plan mode (humano aprova) → implementar → testes/build → commit pequeno → `/sessao-fim`.
- Toda spec separa "verificável no PC" de "exige lab". Nunca marque critério de lab como verificado sem log/foto colado pelo humano.
- NUNCA edite `docs/10-Decisoes/`; proponha um ADR novo em `docs/00-Inbox/` e peça aprovação.
- Camada de segurança, controle de servos, bote e FPGA: nenhum commit sem APROVADO do agente `reviewer` (opus, critérios de `reviewer-seguranca.md`); testbench/testes de host com mutações antes da revisão.
- Código C++: C++11 (sem depender de C++17), sem exceções/RTTI, sem alocação no laço de controle, `-static`, flags em `firmware/Makefile`.
- Cite a fonte primária (arquivo/linha do CD, seção do manual em `docs/40-Fontes/`, CSV de medição). O que não foi lido nem medido: marque `[INFERÊNCIA]`.
- Commits em português, imperativo, prefixo da área: `firmware:`, `sim:`, `docs:`, `tools:`, `fpga:`.

## Papéis dos modelos
- Planeja e aprova o plano: fable (plan mode + um fable em contexto limpo criticando).
- Escreve: sonnet em subagente `task`, a partir do plano aprovado (periferia e núcleo).
- Revisa: opus em agente somente-leitura. Quem escreve nunca aprova o próprio código.
