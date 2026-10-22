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

## Lido do cartão e do CD (2026-10-22, detalhes em [[30-Medicoes/dia-1]])
- Kernel **3.13.0** (U-Boot 2013.01), Poky/Yocto 1.3, BusyBox 1.20.2, glibc 2.15, `CONFIG_PREEMPT_NONE`; sem gcc/python na placa; cartão = Linux Image 2.0.
- Ao vivo: `/dev/i2c-1`, `/dev/spidev1.0`, `/dev/watchdog`, `/dev/mem` existem; I2C0 tem o ADXL345 em 0x53; **I2C1 sem pull-up** (adicionar 2,2–4,7 kΩ).
- PWM: 4 registradores (`TOTAL_DUR`, `HIGH_DUR`, `ADJ_SPEED/STATUS`, `ABORT`), 20 ms, 0,5–2,5 ms ↔ −90…+90°, motores 0–8 espelhados ([[spider-cd/DE0_NANO_SOC_SPIDER_LINUX_APP/CMotor.cpp]]).
- CPLD: corte por `VBUS < 5,9 V` por 2 s ou `P > 65 W` por 0,2 s; INA230 cfg `0x4127`, cal 2560 (1 mA/LSB) ([[spider-cd/Motor_Board_MAXII_Power_Monitor/MAXII_Power_Monitor.v]]).
- Comandos extras: `ATST`, `ATVR=?`, `ATJS=`.

## Header LTC 2x7 (J17) — atenção ao multiplexador
- Confirmado no `DE0_Nano_SoC_golden_top.v` do CD: `HPS_SPIM_*` = **spim1/SS0** (`/dev/spidev1.0`), `HPS_I2C1_*` = i2c1 (`/dev/i2c-1`), `HPS_LTC_GPIO` = **HPS GPIO40** (gpiochip198 + 11 = **gpio209** no sysfs).
- A figura 3-27 do manual mostra um **TS3A5018 (U2)** cujo seletor é o próprio `HPS_LTC_GPIO`: ele escolhe se os pinos "MOSI_SDA"/"SCK_SCL" do header levam SPI ou I2C1. No esquemático da DE10-Nano (mesmo desenho, sheet 16): GPIO40 = 0 → SPI nos pinos 3/4/5/6; GPIO40 = 1 → I2C1 nos pinos 4/5; I2C1 também sai **direto** nos pinos 9/11 (0 Ω); pin 14 (GPIO) tem R105 **DNI**. `[INFERÊNCIA]` que a DE0-Nano-SoC é idêntica — **confirmar no esquemático do DE0-Nano-SoC CD-ROM** (login Terasic) e com multímetro.
- Consequências: (1) o firmware deve fixar GPIO40 = 0 (saída) para ter SPI no header; (2) **não há GPIO livre no header** → o D/C do display vem do PCF8574; (3) sensores I2C nos pinos 9/11.

## Ainda não verificado
- Pinagem física exata do J17 na DE0-Nano-SoC (esquemático no CD-ROM da placa) e o que é "VCC9" no pino 1/2.
- Versão do BlueZ (`bluetoothd -v` não respondeu); avisos de DMA do dwc2 com o dongle (ver [[30-Medicoes/dia-1]] parte B).
