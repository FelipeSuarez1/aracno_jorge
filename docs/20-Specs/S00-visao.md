---
status: aprovada
origem: sessão de planejamento 2026-10-22 (agente aprofundar, versão 2)
depende: 
---
# S00 — Visão e prompt mestre

## Objetivo
Aranha autônoma com "cabeça humana": rosto com humor e balão de texto no display, cabeça que vira para olhar, comportamento aprendido (esconder-se no escuro, espreitar, bote em direção à pessoa, "falar" pelo balão, mostrar o rosto); anda sozinha sem bater e sem cair.

## Fora de escopo
Voz, câmera, SLAM, placa extra, aprender a andar (ADR-001..005).

## Critérios de aceite do projeto (fixados antes dos testes)
Todos **[LAB]** salvo indicação:
- Osciloscópio em 20 botes: tensão mínima do barramento ≥ limiar medido do CPLD + 0,2 V; zero desligamentos; CSV dos traços em `30-Medicoes/`.
- Limiar de corte medido na fonte de bancada coincide com a constante do código do CPLD; INA230 vs. instrumento erro < 10 % com a mesma janela de integração.
- Sensores: limiar de borda com margem ≥ 5σ sobre tabela medida (N ≥ 100 por célula).
- 20 botes contra alvo a 60 cm medidos com trena, zero contatos; parada por E-stop < 200 ms em log com timestamp.
- Jitter: p99 do período do laço < 10 % do nominal **[PC parcial: histograma gerado pelo firmware]**; `kill -STOP` → servos param em < 1 s pelo watchdog.
- Injeção de falhas: desconectar cada sensor, travar I2C, remover dongle BT durante um bote → parada < 200 ms em 10 de 10.
- Borda: 20 aproximações da beira de uma mesa com rede embaixo, zero quedas.
- Escuro: 20 ensaios claro + 20 escuro com luxímetro, ≥ 95 % corretos; em 10 partidas aleatórias chega ao ponto mais escuro em ≥ 8.
- Política: sucesso em simulação ≥ alvo fixado em layouts não vistos **[PC]**; no real, 20 cenários roteirizados com ≥ 70 % de ações iguais ao roteiro, superando FSM à mão e política aleatória.
- Rosto: ≥ 10 pessoas de fora, escolha forçada 6 opções, display real a 1 m, ≥ 70 % por expressão e p < 0,05 vs. 1/6; balão legível a 1 m.
- Cabeça: segue pessoa que cruza a 1 m em 20 passagens, perde ≤ 3.
- Professor assina a FMEA de uma página antes do primeiro bote com pessoas.
- Outro aluno clona o repo, constrói no container, grava a microSD e repete a demo sem ajuda **[PC parcial]**.

## Prompt mestre (saída do agente `aprofundar`, 2026-10-22)
Contexto completo para specs novas. Itens citados como datasheet ainda não lidos estão marcados em [[40-Fontes/fontes-externas]].

```
Contexto: somos dois alunos de Engenharia da Computação (Luiz e Felipe), orientados por um professor que aprovou a ideia e tem lab com impressora 3D, osciloscópio, fonte de bancada, balança e luxímetro. Temos um Terasic Spider (hexápode descontinuado, 18 servos MG996R + 2 reserva) com DE0-Nano-SoC (Cyclone V SE 5CSEMA4U23C6N: FPGA 40K LE + HPS Cortex-A9 dual 925 MHz, 1 GB DDR3, Yocto 2015 na microSD, USB OTG host, UART-USB, acelerômetro em HPS_I2C0, header LTC 2x7 com HPS_I2C1/HPS_SPIM/1 GPIO; headers 2x20 e Arduino são pinos da FPGA; ADC LTC2308 8 canais na FPGA) sobre uma Terasic Servo Motor Card (24 portas PWM a 6 V com fusível; servos nas portas 0–2 e 9–23, portas 3–8 livres; INA230 + CPLD com corte por subtensão/sobrecorrente que desliga servos E o DE0; chave física para os servos; entrada 6,2–12 V; dois packs NiMH ou fonte 12 V).

Fatos do Programming Guide v1007 (usar, não redescobrir): FPGA com 18 controladores PWM Qsys (ip/spider; registradores TOTAL_DUR, HIGH_DUR, CONTROL_STATUS, tick 50 MHz, rampa de velocidade em hardware); app C++ "spider" em /home/root iniciado por /etc/inittab, classes CSpider → CSpiderLeg → CMotor, acesso aos PWM via /dev/mem; marcha tripé pronta; comandos ASCII por Bluetooth SPP (ATFW, ATBW, ATTR=, ATTL=, ATSP=, ATTTF/ATTTB/ATTTL/ATTTR/ATTTN, ATALL) recebidos por BtSpp. Fontes no Spider CD-ROM 1.0.1 (conta Terasic gratuita, http://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&No=994&PartNo=4). Toolchain original Quartus 14.0 / SoC EDS 14.1 (descontinuado). Manual da Servo Motor Card (SMK V1.1.0, 2019-07-30): https://www.mouser.com/datasheet/2/598/ervo_Motor_Kit-1020966.pdf . DE0-Nano-SoC User Manual rev.D0: https://www.terasic.com.tw/attachment/archive/941/DE0-Nano-SoC_User_manual_rev.D0.pdf . Cyclone V HPS Technical Reference Manual (Intel 683126) para mapa de endereços (ponte lightweight lwhps2fpga em 0xFF200000), I2C, SPI master e watchdog L4. Caminho documentado de Debian na DE0-Nano-SoC (NÃO usar, só referência): https://forum.digikey.com/t/debian-getting-started-with-the-de0-nano-soc-kit/12434

Datasheets primários a citar: INA230 (TI SBOS601B: tempos de conversão 140 µs–8,244 ms, média 1–1024); VL53L1X (ST DS12385: tabela de alcance vs. luz ambiente, timing budget mínimo 20 ms); BH1750 (ROHM: modos H/L-res e tempos); ST7789V (Sitronix: AC characteristics da escrita serial); PCA9685 (NXP); LTC2308 (ADI); MG996R e MG90S (apenas spec do TowerPro, sem datasheet — tratar como alegação); HC-SR501 (doc do módulo).

Decisões fechadas (não reabrir):
- SEM placa companheira, SEM troca de Linux, SEM câmera, SEM geolocalização/SLAM, SEM reconhecimento ou síntese de voz. Tudo roda no HPS em C++ compilado com toolchain estática musl (musl.cc arm-linux-musleabihf ou musl-cross-make, versão e hash fixados em container) para não depender da glibc do Yocto 2015; flags -march=armv7-a -mfpu=neon -mfloat-abi=hard -static.
- A marcha é a da Terasic (CSpider). A IA NÃO aprende a andar; aprende a DECIDIR.
- IA = política pequena (MLP de dezenas de neurônios ou Q-table) treinada no PC e exportada como array C para inferência em C++. Duas fontes de treino: (a) simulador 2D em Python (API gymnasium; cômodo com luzes, pessoa que anda, bateria) com RL; (b) clonagem de comportamento a partir de teleoperação pelo app Android original com log de (sensores → comando AT), seguida de uma rodada DAgger (Ross, Gordon, Bagnell, AISTATS 2011) com correções humanas. Observação: lux esq, lux dir, distância frente, distância chão, PIR, bateria (INA230), inclinação (acelerômetro), tempo desde o último estímulo. Ações: parar, avançar, virar esq/dir, abaixar corpo, bote, recuar, virar cabeça. Recompensa: estar no escuro, aproximar-se sem ser detectada, bote a < 60 cm, penalidades por colisão/borda/bateria baixa. Baselines obrigatórios: FSM à mão e política aleatória nos mesmos cenários e sementes.
- Estado interno de "humor" (medo, curiosidade, cansaço) modulando velocidade e expressão.
- Camada de segurança fixa, acima da política: distância mínima de parada, borda de mesa, corrente, bateria, tombo, sensor obsoleto/I2C travado, watchdog de hardware; chave da Servo Motor Card = E-stop.
- Cabeça pan/tilt com 2× MG90S carregando: display, VL53L1X frontal, BH1750 ×2 (esq/dir). Segundo VL53L1X fixo apontado para o chão (borda). PIR HC-SR501 no corpo. Opcional: microfone analógico no ADC LTC2308 para reagir a barulho por amplitude.
- Display: ST7789 240×240 SPI (ou ILI9341 320×240); rosto por sprites gerados no PC (expressões nas dimensões de Kalegina et al., "Characterizing the Design Space of Rendered Robot Faces", HRI 2018) + balão de texto com o que ela "quer falar"; sequência: age → balão por N s → rosto.
- Barramento decidido no dia 1 pelo teste na UART. Rota HPS: sensores/display no header LTC (HPS_I2C1/HPS_SPIM) e pan/tilt via PCA9685. Rota FPGA: I2C/SPI master + 2 controladores PWM reaproveitados do ip/spider (portas 3–8 da SMK) adicionados no Platform Designer com Quartus Prime Lite 21.1, acessados via /dev/mem com offsets de hps_0.h regenerado.

Objetivo: aranha autônoma com "cabeça humana" — rosto com humor e balão de texto no display, cabeça que vira para olhar, comportamento aprendido: esconder-se no escuro, espreitar, dar um "bote" em direção à pessoa, "falar" pelo balão e mostrar o rosto; anda sozinha sem bater e sem cair.

Entregue, nesta ordem:

1. LEVANTAMENTO DO MATERIAL BRUTO. Do Spider CD: de CSpider.cpp/CSpiderLeg.cpp/CMotor.cpp, limites de ângulo por servo, mapeamento ângulo→HIGH_DUR, taxa de atualização da marcha e a espera em CONTROL_STATUS bit0; de BtSpp.cpp, o parser AT; do RTL em ip/spider (*.v), a resolução do contador e como a rampa de velocidade incrementa por tick; de hps_0.h e do .sopcinfo, base da ponte lightweight e offset de cada PWM; do fonte do CPLD, as constantes de subtensão/sobrecorrente, a histerese e a palavra de configuração gravada no INA230 (tempo de conversão e média); do esquemático da SMK, o valor do shunt do INA230. Do sistema vivo na UART: uname -a; cat /proc/version; gcc --version; ls /dev/i2c* /dev/spidev* /dev/mem /dev/watchdog*; ls /proc/device-tree/soc/; zcat /proc/config.gz; cat /proc/mounts; dmesg; cat /proc/interrupts; /etc/inittab. No osciloscópio: período e faixa de pulso real de uma porta PWM; mapa pulso→ângulo medido com transferidor em 3 MG996R. Cada fato com arquivo/linha, seção/página ou arquivo de medição.

2. ORÇAMENTO DE ENERGIA E PESO MEDIDO. Shunt de 10–20 mΩ no cabo dos packs + osciloscópio em 2 canais (corrente e tensão do barramento, ≥ 1 MS/s) em marcha tripé e em 20 botes (velocidade máx + inclinar corpo p/ frente + avançar); registrar mínimo de tensão e pico de corrente por bote, comparados ao limiar do CPLD. Limiar real de corte: fonte de bancada descendo em degraus de 0,1 V até o desligamento, com histerese; comparar à constante do fonte do CPLD. Resistência interna de cada pack (ΔV/ΔI) novo vs. após 3 ciclos. INA230 vs. instrumento com a MESMA janela de integração configurada pelo CPLD, em parado/marcha/bote. Autonomia até o corte em 3 ciclos com INA226. Massas na balança (display + 2 MG90S + sensores + suporte + cabos) e braços de alavanca com paquímetro nas juntas coxa/fêmur; torque estático calculado vs. spec do MG996R; marcha com lastro igual à cabeça medida, corrente e inclinação (acelerômetro) comparadas à marcha sem lastro.

3. ARQUITETURA EM C++ NO HPS. Laço de controle (sensores → política → segurança → CSpider/cabeça/display) com SCHED_FIFO + mlockall, período escolhido como múltiplo do quadro de 20 ms dos servos e ≥ timing budget do VL53L1X; histograma de jitter medido com clock_gettime(CLOCK_MONOTONIC); watchdog via /dev/watchdog (ou justificar ausência pelo dmesg) e regra "sensor obsoleto → parar". Log binário de sensores com timestamp monotônico e CRC, formato documentado; pesos exportados em weights.h com vetores de teste entrada→saída verificados no ARM na inicialização. Modo "teleoperação com gravação" reaproveitando BtSpp (comando AT + vetor de sensores no mesmo instante). Modo "autônomo". "Bote" definido numericamente a partir de medição: distância de frenagem em 10 ensaios na velocidade máxima; distância mínima = 2× média + velocidade × latência do laço; velocidade, condições de aborto e tempo máximo. Rootfs em somente leitura: ro no bootargs + tmpfs em /var e /tmp; overlayfs só se o kernel for ≥ 3.18 (confirmar em /proc/version); partição de dados separada com sync, para sobreviver ao corte do CPLD.

4. PIPELINE DE TREINO. Simulador 2D com modelos de ruído de sensor extraídos da caracterização medida (σ do VL53L1X por distância × superfície × lux com N ≥ 100 por célula; BH1750 conferido com luxímetro em 5 pontos do cômodo; aquecimento e retrigger do PIR), velocidade e taxa de giro reais da aranha medidas com trena e vídeo, pessoa a 0,5–1,5 m/s, bateria pelo ensaio de autonomia. Algoritmo: Q-learning tabular com observação discretizada; PPO (Stable-Baselines3) só se o tabular não atingir a métrica; sementes fixas, requirements.txt pinado. Validação do simulador: histogramas sim vs. logs de teleoperação por feature; acurácia offline da política clonada nos logs vs. FSM à mão. Métrica: taxa de sucesso em 1000 episódios em layouts não vistos no treino, alvo fixado antes. Protocolo de clonagem: número de sessões, minutos e cenários (pessoa entra/para/sai; claro/escuro; borda), depois uma rodada DAgger. Comparação política vs. FSM vs. aleatória nos mesmos cenários.

5. CABEÇA. Pan/tilt: ângulos, massa medida, braço de alavanca do tilt e margem de torque ≥ 3× sobre a spec do MG90S; cabeamento pelo pescoço com I2C de ≥ 15 cm (pull-ups dimensionados) e ciclos de flexão testados. Driver do display via spidev com SPI_IOC_MESSAGE em rajadas, clock = menor entre o AC spec do ST7789V e o máximo do SPI master do HPS (TRM), atualização parcial de regiões (olhos/boca/balão). Driver I2C dos sensores com timeout e detecção de barramento travado. Fixação ao corpo impressa em 3D, verificada após 20 botes sem afrouxar.

6. PLANO DE VERIFICAÇÃO EXTERNA, critérios fixados antes dos testes:
   - osciloscópio em 20 botes: tensão mínima do barramento ≥ limiar medido do CPLD + 0,2 V, zero desligamentos, CSV dos traços anexado; limiar medido na fonte coincide com a constante do CPLD dentro da resolução do INA230; INA230 vs. instrumento erro < 10 % com janela igual;
   - caracterização de sensores: limiar de borda com margem ≥ 5σ sobre a tabela medida;
   - 20 botes contra alvo a 60 cm medidos com trena, zero contatos; parada por E-stop < 200 ms em log com timestamp;
   - jitter: p99 do período do laço < 10 % do nominal; kill -STOP no processo → servos param em < 1 s pelo watchdog;
   - injeção de falhas: desconectar cada sensor, travar I2C, remover dongle BT durante um bote → parada < 200 ms em 10 de 10;
   - borda: 20 aproximações da beira de uma mesa com rede embaixo, zero quedas;
   - escuro: 20 ensaios claro + 20 escuro com luxímetro, ≥ 95 % de decisões corretas; em 10 partidas de pontos aleatórios do cômodo, chega ao ponto mais escuro em ≥ 8;
   - política: taxa de sucesso em simulação ≥ alvo fixado em layouts não vistos; no real, 20 cenários roteirizados (pessoa entra, para, sai) com ≥ 70 % de ações iguais às do roteiro, superando FSM à mão e política aleatória nos mesmos cenários;
   - rosto: ≥ 10 pessoas de fora, escolha forçada com 6 opções, no display real a 1 m, ordem aleatorizada; ≥ 70 % de acerto por expressão e p < 0,05 vs. chance (1/6) por teste binomial, matriz de confusão anexada; balão legível a 1 m por todos;
   - cabeça: segue uma pessoa que cruza a frente a 1 m em 20 passagens, perde o alvo em ≤ 3;
   - professor assina a FMEA de uma página (modo de falha → efeito → detecção → ação) antes do primeiro bote com pessoas;
   - outro aluno clona o repositório, constrói no container com toolchain fixada, grava a microSD e repete a demo sem ajuda.

Regras: não reidentifique o hardware; cite a fonte primária de cada afirmação técnica (manual, datasheet, arquivo/linha do CD, traço/tabela de medição); marque como [INFERÊNCIA] o que não puder ser lido ou medido; não adicione funcionalidades fora do objetivo (nada de voz, câmera, SLAM, placa extra); responda em português.
```
## Análise completa do agente
As seções "O que um profissional de ponta faria", "Material bruto", "Métodos e ferramentas" e "Verificação externa" estão em [[40-Fontes/aprofundar-v2]].
