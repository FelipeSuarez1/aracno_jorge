# Saída do agente aprofundar — versão 2 (2026-10-22)

Análise que gerou o prompt mestre de [[20-Specs/S00-visao]]. Afirmações sobre datasheets não lidos nesta sessão são [INFERÊNCIA] do agente.

## 1. O que um profissional de ponta faria

- **Mediria o transitório, não a média.** Pinça DC e INA226 integram em ms–s; o que derruba o CPLD é o afundamento da linha de 6 V nos primeiros ~10–50 ms de um bote, quando 18 MG996R partem juntos. Um profissional captura corrente (shunt de baixo valor) **e** tensão do barramento no osciloscópio, em dois canais, ≥ 1 MS/s, e mede a resistência interna do pack NiMH (ΔV/ΔI) — pack cansado afunda mais, e é a tensão, não a corrente, que costuma disparar o corte por subtensão.
- **Casaria a janela de integração do INA230 com a do instrumento.** O INA230 tem tempo de conversão de 140 µs a 8,244 ms e média de 1 a 1024 amostras (TI SBOS601B). Comparar "INA230 vs. instrumento com erro < 10 %" sem saber a configuração que o CPLD gravou no INA230 é comparar grandezas diferentes. Ele lê a configuração no fonte do CPLD e ajusta o instrumento para a mesma janela.
- **Descobriria o limiar real de corte por ensaio, não só por leitura.** Fonte de bancada em 12 V/6,2 V descendo em degraus de 0,1 V até o CPLD desligar: limiar e histerese medidos, comparados às constantes do fonte do CPLD.
- **Caracterizaria cada sensor montado na aranha, na altura e nas superfícies reais, antes de escrever o simulador.** Desvio-padrão do VL53L1X por distância, refletância e lux ambiente (o DS12385 da ST tem tabela de alcance máximo vs. ambiente — o alcance cai muito com sol/luz forte); o sensor de chão a ~5–10 cm sobre carpete escuro vs. piso claro; saturação e constante de tempo do BH1750 (120 ms típicos em H-res); tempo de aquecimento (~60 s) e retrigger do HC-SR501. O modelo de ruído do simulador sai dessas tabelas, não de palpite.
- **Mediria a dinâmica real da marcha.** Velocidade (cm/s), taxa de giro (°/s), **distância de frenagem** após o comando de parar na velocidade máxima (inclui a rampa de velocidade em hardware do ip/spider) — 10 ensaios com trena. A distância mínima de parada do bote e da camada de segurança são derivadas disso, não escolhidas.
- **Mediria o jitter do laço de controle no HPS** (histograma de `clock_gettime(CLOCK_MONOTONIC)` com `SCHED_FIFO` + `mlockall`) e usaria um watchdog de hardware (o HPS do Cyclone V tem timers L4 de watchdog; checar `ls /dev/watchdog` e `dmesg`) para que travamento do processo = servos parados.
- **Trataria "sensor obsoleto" como falha**: I2C travado, VL53L1X desconectado, dongle BT removido → parada. Faria injeção de falhas com cronômetro de log.
- **Validaria o simulador contra logs reais** antes de confiar na política: distribuição por feature (sim vs. teleoperação) e acurácia offline da política clonada sobre os logs. Usaria **DAgger** (Ross, Gordon, Bagnell, AISTATS 2011) para coletar correções após o primeiro deploy, em vez de clonagem pura.
- **Fixaria baselines**: FSM à mão e política aleatória, nos mesmos cenários e sementes. Sem baseline, "70 % de ações iguais ao roteiro" não diz se a IA aprendeu algo.
- **Faria o teste do rosto como estudo com nível de chance**: 6 opções → chance = 16,7 %; teste binomial por expressão + matriz de confusão, participantes de fora, ordem aleatorizada, visto a 1 m no display real (não em PNG no monitor).
- **Faria uma análise de perigos curta (FMEA de uma página)** antes do primeiro bote com pessoas, com a assinatura do professor — é isso que ele assina, não um "diagrama de energia".
- **Congelaria o ambiente de build**: versão exata do cross-compiler em container, hash do commit, `uname -a` da imagem 2.0; outro aluno reproduz a partir do zero.

## 2. Material bruto

- **Spider CD-ROM 1.0.1** (conta Terasic gratuita, link no prompt): `CSpider.cpp/.h`, `CSpiderLeg.cpp`, `CMotor.cpp` (limites de ângulo, mapeamento ângulo→HIGH_DUR, taxa da marcha, espera no `CONTROL_STATUS` bit0), `BtSpp.cpp` (parser AT), **RTL do `ip/spider`** (`*.v`: resolução do contador a 50 MHz, como a rampa de velocidade incrementa por tick), **`hps_0.h`** gerado por `sopc-create-header-files` (base da ponte lightweight — no Cyclone V HPS TRM a `lwhps2fpga` começa em `0xFF200000` — e offsets de cada PWM), `.qsys/.sopcinfo` do projeto Quartus, `soc_system.dts`, e o **fonte do CPLD** da Servo Motor Card (constantes de subtensão/sobrecorrente, histerese, registrador de configuração escrito no INA230).
- **Esquemático da Servo Motor Card** (no CD ou no manual SMK V1.1.0): valor do **shunt** do INA230 — sem ele não há calibração nem comparação com instrumento.
- **Sistema vivo na UART**, além dos comandos do dia 1: `cat /proc/version`, `zcat /proc/config.gz` (se existir), `ls /proc/device-tree/soc/` (i2c1/spi1 habilitados?), `cat /sys/kernel/debug/gpio`, `dmesg`, `cat /proc/interrupts`, `/etc/inittab`, `ls /dev/watchdog*`, `df -h` e `cat /proc/mounts`.
- **Osciloscópio no servo**: forma de onda real de uma porta PWM (período, faixa de pulso) e **mapa pulso→ângulo de 3 MG996R** com transferidor — o MG996R não tem datasheet; só há a spec do TowerPro (9,4 kgf·cm @ 4,8 V / 11 @ 6 V), que deve ser tratada como alegação, não fato.
- **Datasheets primários**: INA230 TI SBOS601B; VL53L1X ST DS12385 (tabelas de alcance vs. ambiente e timing budget mínimo de 20 ms); BH1750 ROHM (modos e tempos); ST7789V Sitronix (AC characteristics da escrita serial); PCA9685 NXP; LTC2308 ADI; MG90S TowerPro (spec); HC-SR501 (doc do módulo, sem datasheet formal); Cyclone V HPS TRM (Intel 683126: mapa de endereços, I2C, SPI master, watchdog L4).
- **Medições próprias**: traços de osciloscópio (CSV), tabelas de caracterização de sensores, balança de 1 g, luxímetro, trena, vídeo com timestamp.
- **Logs de teleoperação** (sensores + comando AT + timestamp monotônico) — o dado bruto da clonagem e da validação do simulador.

## 3. Métodos e ferramentas

- **Energia**: shunt de 10–20 mΩ (ou sonda de corrente) + osciloscópio 2 canais ≥ 1 MS/s; fonte de bancada com limite de corrente para o ensaio de limiar; teste de resistência interna do pack (ΔV/ΔI em degrau de carga); INA226 só para média de autonomia. Comparação INA230 × instrumento com janela de integração igual à configurada pelo CPLD.
- **Torque**: massas medidas (balança), braços de alavanca medidos (paquímetro) nas juntas coxa/fêmur, torque estático calculado vs. spec do MG996R; prova empírica = marcha com lastro igual à cabeça medida, corrente comparada à marcha sem lastro.
- **Tempo real no HPS**: `sched_setscheduler(SCHED_FIFO)`, `mlockall`, histograma de período com `clock_gettime`; `cyclictest` (rt-tests) cross-compilado se couber; watchdog via `/dev/watchdog` (driver `dw_wdt`); período do laço = múltiplo do quadro de 20 ms dos servos e ≥ timing budget do VL53L1X (20 ms curto).
- **Build**: toolchain estática **musl** (musl.cc `arm-linux-musleabihf` ou musl-cross-make) em container, evitando os avisos de NSS da glibc estática; `-march=armv7-a -mfpu=neon -mfloat-abi=hard`; versão e hash fixados no README. Rootfs: `ro` no bootargs + tmpfs em `/var`/`/tmp`; overlayfs só se o kernel for ≥ 3.18 (foi mesclado em 3.18; a imagem 2.0 de 2015 provavelmente é 3.x — [INFERÊNCIA], confirmar no `/proc/version`); partição de dados ext4 separada com `sync`.
- **Rota FPGA (se escolhida)**: Quartus Prime Lite 21.1 (suporta Cyclone V) com Platform Designer; reutilizar o PWM do `ip/spider` para o pan/tilt; `sopc-create-header-files` para os novos offsets.
- **Display**: `spidev` com `SPI_IOC_MESSAGE` em rajadas; clock máximo = menor entre o AC spec do ST7789V e o máximo do SPI master do HPS (TRM); atualização parcial de regiões (olhos/boca/balão) em vez de quadro cheio de 115 kB.
- **Treino**: simulador em Python com API gymnasium; **Q-learning tabular** com observação discretizada primeiro; **PPO** (Stable-Baselines3) só se o tabular falhar na métrica; sementes fixas, `requirements.txt` pinado; exportação para `weights.h` com vetores de teste (entrada→saída calculados no PC e verificados no ARM na inicialização). Clonagem: regressão logística/MLP sobre logs; **DAgger** para a segunda rodada.
- **Rosto**: sprites nas dimensões de Kalegina et al. (HRI 2018; olhos, boca, sobrancelhas, cor), estudo de escolha forçada com teste binomial vs. 1/6 e matriz de confusão.
- **Segurança**: FMEA de uma página (modo de falha → efeito → detecção → ação), injeção de falhas cronometrada por log.

## 4. Verificação externa

- **Osciloscópio**: em 20 botes, mínimo da tensão do barramento ≥ limiar medido do CPLD + 0,2 V; pico de corrente registrado; zero desligamentos. Arquivo CSV dos traços anexado.
- **Limiar do CPLD**: valor medido na fonte de bancada coincide com a constante do fonte do CPLD dentro da resolução do INA230 (shunt conhecido).
- **INA230 × instrumento**: erro < 10 % **com janela de integração igual**, em 3 cargas (parado, marcha, bote).
- **Sensores**: tabela de σ do VL53L1X por distância × superfície × lux, com N ≥ 100 amostras por célula; limiar de borda escolhido com margem ≥ 5σ; BH1750 confere com luxímetro em 5 pontos do cômodo.
- **Frenagem**: distância de parada medida em 10 ensaios; distância mínima de bote = 2× a média + velocidade × latência do laço; parada por E-stop < 200 ms no log.
- **Jitter**: histograma com p99 do período do laço < 10 % do período nominal; teste de watchdog: `kill -STOP` no processo → servos param em < 1 s.
- **Injeção de falhas**: desconectar cada sensor, travar I2C, remover dongle, durante um bote — parada em < 200 ms em 10 de 10.
- **Simulador**: histogramas sim vs. logs reais por feature; acurácia offline da política clonada ≥ FSM à mão nos mesmos logs; taxa de sucesso em 1000 episódios com semente fixa, em layouts não vistos no treino.
- **Real**: os 20 cenários roteirizados do prompt, com FSM e política aleatória como baselines nos mesmos cenários.
- **Rosto**: ≥ 10 pessoas de fora, 6 opções, p < 0,05 vs. chance por expressão; balão legível a 1 m por todos.
- **Cabeça e borda**: os ensaios de 20 do prompt, filmados com timestamp.
- **Reprodutibilidade**: outro aluno, a partir do README e do container, compila o binário estático, grava a microSD e repete a demo sem ajuda; professor assina a FMEA antes do primeiro bote com pessoas.

## 5. Prompt reescrito

