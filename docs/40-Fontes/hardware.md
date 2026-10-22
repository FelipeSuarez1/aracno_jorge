# Hardware identificado

Fatos confirmados em terasic.com.tw e nos manuais em [[40-Fontes]]. Não reidentificar.

| Item | Fato | Fonte |
|---|---|---|
| Kit | Terasic Spider, descontinuado ("Phased Out"); hexápode montado, 18 servos MG996R + 2 reserva, corpo de alumínio | [página do produto](http://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&CategoryNo=183&No=994); [[spider-user-manual]] cap. 1 |
| Placa principal | DE0-Nano-SoC — Cyclone V SE `5CSEMA4U23C6N` (FPGA 40K LE + HPS Cortex-A9 dual 925 MHz, 1 GB DDR3), Linux no microSD, GbE, USB OTG host/device (SMSC USB3300), UART-USB, acelerômetro em `HPS_I2C0`, 2× GPIO 2x20 (FPGA), header Arduino (FPGA), ADC LTC2308 (FPGA, SPI), header LTC 2x7 com `HPS_I2C1`, `HPS_SPIM`, 1 GPIO | [[de0-nano-soc-user-manual]] §2.1, §3.7.6, §3.7.8 |
| Módulo embaixo | Terasic Servo Motor Card: 24 portas PWM 3,3 V→5 V, servos a 6 V, fusível por porta; CPLD Altera + INA230 (tensão, corrente total, corte por subtensão/sobrecorrente); corte desliga servos **e** o DE0; chave física para os servos; entrada 6,2–12 V; saída 5 V para o host | [[smk-servo-motor-card-manual]] §2.1–2.7; [[spider-user-manual]] cap. 5 |
| Portas PWM usadas | 0–2 e 9–23 (18 servos); **3–8 livres** | [[spider-programming-guide]] Apêndice |
| Alimentação | Dois packs NiMH "two packs at once" ou fonte 12 V; DE0 alimentado pelo 5 V da Servo Motor Card; carregador 2 A no kit | [[spider-user-manual]] §2-1, cap. 4 |
| Controle original | Bluetooth SPP via dongle USB (dispositivo `socfpga-0`, PIN 0000) + app Android; comandos ASCII `ATFW ATBW ATTR ATTL ATTR= ATTL= ATSP= ATTTL ATTTR ATTTF ATTTB ATTTN ATALL` | [[spider-programming-guide]] §2.3; [[spider-user-manual]] cap. 3 |
| FPGA | 18 controladores PWM Qsys (`ip/spider`), registradores `TOTAL_DUR`, `HIGH_DUR`, `CONTROL_STATUS` (bit0 = PWM final enviado), tick 50 MHz, rampa de velocidade em hardware; LED7 heartbeat; Quartus 14.0 | [[spider-programming-guide]] cap. 3 |
| Linux/app | Yocto; app C++ `spider` em `/home/root`, iniciado por `/etc/inittab`; classes `CSpider → CSpiderLeg → CMotor`, `BtSpp` (RFCOMM), `CQueueCommand`, `CPIO_BUTTON/LED`; build com SoC EDS 14.1 | [[spider-programming-guide]] cap. 4 |
| Código-fonte | Spider CD-ROM 1.0.1 (2015-11-27) e Linux Image 2.0 (2015-10-23): download exige **conta Terasic gratuita** (redireciona para `member.pl`) | [lista de downloads](http://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&No=994&PartNo=4) |

## Ainda não lido (ler no dia 1 ou do CD)
- Versão do kernel, BlueZ, se o rootfs é somente leitura → [[20-Specs/S01-dia-1-uart]].
- Limiares de corte do CPLD, palavra de configuração do INA230, valor do shunt → código do CPLD e esquemático no CD.
- Limites de ângulo por servo, mapeamento ângulo→`HIGH_DUR`, espera no bit0 → `CMotor.cpp` no CD.
