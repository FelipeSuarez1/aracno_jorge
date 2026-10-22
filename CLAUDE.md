# aracno_jorge — aranha Terasic Spider com cabeça e IA de decisão

Hexápode Terasic Spider (18× MG996R) + DE0-Nano-SoC (Cyclone V: FPGA + ARM Cortex-A9 dual, Linux Yocto 2015) + Servo Motor Card.
Todo código é escrito pela IA. Luiz e Felipe decidem, medem no lab e revisam. Responda sempre em português.

## Decisões fechadas (não reabrir; detalhes em docs/10-Decisoes/)
- Sem placa companheira, sem troca de Linux, sem câmera, sem SLAM, sem voz. Tudo roda no HPS em C++ estático.
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
- Placa pela UART (sem rede): `tools/uart.py run "cmd"`, `tools/uart.py push local /home/root/x`, `tools/uart.py boot`. Servo Motor Card OFF em teste de software.

## Regras de trabalho
- Fluxo: Inbox → `/spec` → plan mode (humano aprova) → implementar → testes/build → commit pequeno → `/sessao-fim`.
- Toda spec separa "verificável no PC" de "exige lab". Nunca marque critério de lab como verificado sem log/foto colado pelo humano.
- NUNCA edite `docs/10-Decisoes/`; proponha um ADR novo em `docs/00-Inbox/` e peça aprovação.
- Camada de segurança, controle de servos e bote: trabalho síncrono (humano acompanhando) e revisão por agente `reviewer-seguranca` antes do commit.
- Código C++: C++11 (sem depender de C++17), sem exceções/RTTI, sem alocação no laço de controle, `-static`, flags em `firmware/Makefile`.
- Cite a fonte primária (arquivo/linha do CD, seção do manual em `docs/40-Fontes/`, CSV de medição). O que não foi lido nem medido: marque `[INFERÊNCIA]`.
- Commits em português, imperativo, prefixo da área: `firmware:`, `sim:`, `docs:`, `tools:`, `fpga:`.

## Papéis dos modelos
- Planeja e conversa com Luiz/Felipe: modelo de raciocínio forte (fable), em plan mode.
- Escreve: modelo rápido para periferia (scripts, sprites, sim, docs); modelo forte para núcleo (segurança, servos, bote, FPGA).
- Revisa: modelo forte diferente do que escreveu, em agente somente-leitura. Quem escreve nunca aprova o próprio código.
