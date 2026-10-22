---
data: 2026-10-22
quem: Luiz (cartão no PC), IA (leitura)
spec: [[20-Specs/S01-dia-1-uart]]
---
# Dia 1 — parte A: leitura da microSD original (sem ligar a placa)

## Backup
- Dispositivo: `/dev/mmcblk0`, 7 864 320 000 bytes (7,3 GiB).
- Imagem: `~/dev/aracno_backup/spider-sd-original.img` (fora do repo).
- SHA-256: `44254bf27ea653c628a2cf9bf903227a2f6b28bb8f5b4b00c0c034b67dc28c6d`.
- Partições (`fdisk -l` na imagem): p3 raw 1 MiB @ setor 2048 (tipo A2, preloader/U-Boot); p2 ext3 1 GiB @ 14336 (rootfs); p1 FAT32 819 MiB @ 2121728 (boot).

## Partição de boot (p1)
| Arquivo | Data | Tamanho |
|---|---|---|
| `soc_system.rbf` | 2015-10-16 | 4 244 820 B |
| `zImage` | 2015-01-07 | 4 111 208 B |
| `socfpga.dtb` | 2014-12-29 | 17 900 B |
| `u-boot.scr` | 2015-04-20 | 176 B — `fatload mmc 0:1 $fpgadata de0_nano_soc.rbf; fpga load 0 $fpgadata $filesize; run bridge_enable_handoff;` (nome diverge do arquivo presente; o `bootcmd` do ambiente U-Boot deve carregar `soc_system.rbf` — `[INFERÊNCIA]`, confirmar no log de boot) |

Cópias em [[40-Fontes/spider-sd]].

## Device tree (`socfpga.dtb`, decodificado)
| Nó | Driver | Status |
|---|---|---|
| `i2c@ffc04000` (I2C0, acelerômetro) | `snps,designware-i2c` | okay |
| `i2c@ffc05000` (I2C1, header LTC) | `snps,designware-i2c` | **okay** |
| `i2c@ffc06000`, `i2c@ffc07000` | | disabled |
| `spi@fff00000` (SPIM0) | `snps,dw-spi-mmio` | disabled |
| `spi@fff01000` (SPIM1) + `spidev@0` | `snps,dw-spi-mmio` / `spidev` | habilitado (sem `status`) |
| `wd@ffd02000` | `snps,dw-wdt` | okay |
| `usb@ffb40000` (OTG) | `snps,dwc2` | okay |
| `gpio@ff708000/9000/a000` | `snps,dw-gpio` | okay |
| `serial0@ffc02000` | `snps,dw-apb-uart` | okay (console) |
| `fpgabridge@0/1/2` | hps2fpga, lwhps2fpga, fpga2hps | presentes |
| `ethernet@ff702000` | stmmac | okay |

## Rootfs (p2, lido com `debugfs`, sem montar)
- Distro: **Poky 8.0 (Yocto Project 1.3)**, hostname `socfpga`.
- Kernel: `/lib/modules/3.9.0` no rootfs, mas o kernel que roda é **3.13.0** (ver parte B). Sem overlayfs (entrou em 3.18) → rootfs read-only será `ro` + tmpfs.
- glibc **2.15**, libstdc++ 6.0.17; compilador usado no `spider`: Linaro GCC 4.7.3 (2013.03). **Sem gcc/g++ na placa.** Há `make`, `perl`, `gdbserver`, `strace`.
- Bluetooth: `bluetoothd`, `hciconfig`, `hcitool`, `rfcomm`, `sdptool` presentes; `/home/root/installBlueZ.run` (18 MB).
- Drivers **built-in** (`modules.builtin`): `i2c-dev`, `i2c-designware-platform`, `spidev`, `spi-dw-mmio`, `dw_wdt`, `dwc` (OTG), `usbcore`, `usb-storage`, `usbhid`, `gpio-dw`, `leds-gpio`. Módulos carregáveis: só `g_mass_storage`, `libcomposite`.
- `/dev` estático contém `i2c-0..3`, `mem`; há `udev` (`/etc/init.d/udev`) → `spidev1.0`, `watchdog`, `rfcomm*` devem aparecer no boot (`[INFERÊNCIA]` até ver na UART).
- `/etc/inittab`: `dbus-1`, `bluetoothd`, `hciconfig hci0 up`, `hciconfig hci piscan`, getty com **autologin em ttyS0 115200**, e `21:5:wait:/home/root/./spider` (o app inicia em runlevel 5; `wait` = init espera ele terminar).
- `/etc/fstab`: `rootfs / auto defaults`, tmpfs em `/var/volatile` e `/media/ram`.
- `/home/root`: `spider` (89 118 B, **não stripado, com debug_info**), `bt_test`, `bt_test.c`, `installBlueZ.run`, `.led_blink`.

## App `spider` (do binário, sem fonte)
- Classes: `CMMap` (`map/unmap/Reg32_Read/Reg32_Write/CheckQsysID`), `CMotor` (`Move(float)`, `SetSpeed`, `IsReady`, `ReadyTime`, `Sleep/WakeUp/Reset`), `CSpiderLeg` (`MoveJoint`, `JOINT_ID`), `CSpider` (`MoveForward/Backward`, `RotatelLeft/Right`, `MoveParallelL/R`, `TiltForward/Backward/Left/Right/None`, `BodyForward/Backward/UpDown`, `SetFootY`, `Standup`, `Stomp`, `DEMO_Dance`, `DEMO_Rollover`, `ByeBye`, `Abort`, `WaitReady`, `MoveTripod`), `CBtSpp`, `CBtSppCommand::CommandParsing`, `CQueueCommand`, `CPIO_LED/BUTTON`. Lista completa: [[40-Fontes/spider-sd/spider-symbols.txt]].
- Comandos reconhecidos (strings + enum DWARF): `ATFW ATBW ATTR ATTL ATTR= ATTL= ATST(stop) ATSP= ATTTL ATTTR ATTTF ATTTB ATTTN ATVR=?(versão) ATJS=(joystick) ATALL`, mais `CMD_IDLE`. Três a mais que o Programming Guide (`ATST`, `ATVR=?`, `ATJS=`).
- `szPWM_Base[18]` = `{0x110, 0x100, 0xF0, …, 0x10, 0x00}`: 18 controladores PWM a cada 0x10 bytes, em ordem decrescente por motor. Base da ponte lightweight não aparece como string — vem do CD (`hps_0.h`).
- `CMotor::CMotor(int)` carrega a constante float 180.0 (0x43340000) e o inteiro 1800 — provavelmente faixa de ângulo e algo de velocidade/ticks `[INFERÊNCIA]`, confirmar em `CMotor.cpp` do CD.
- Abre `/dev/mem` (`ERROR: could not open "/dev/mem"...`), serviço SDP "Terasic RFCOMM".

## Conclusão para ADR-006
Rota **HPS** está habilitada no DTB e no kernel: I2C1, SPIM1+spidev, watchdog, OTG. Falta só a confirmação ao vivo (`ls /dev/i2c* /dev/spidev* /dev/watchdog*`) na parte B.

## Parte B (pendente): UART com a placa ligada
Comandos da [[20-Specs/S01-dia-1-uart]] §3; colar em `dia-1-uart.log`.

# Dia 1 — parte C: Spider CD-ROM 1.0.1 e Linux Image 2.0

## Downloads (fora do repo, `~/dev/aracno_backup/`)
- `Spider_v.1.0.1_SystemCD.zip` 43 462 385 B, SHA-256 `20089ed17ff325272abe4b010ef2c9d5fb1e5aeb78139bac17a79b390eb7f2e0`.
- `Spider_image_V2_0.zip` 233 658 564 B, SHA-256 `24ec4edac53463f49191ecea5175029f419e093a6a476ef73fc0497f0783bc41` → `Spider_V2_0.img` (4 004 511 744 B).
- **O cartão da aranha é a Linux Image 2.0**: mesmo serial FAT `F725-1429`, mesmos 4 arquivos em p1, p3 idêntico (SHA-256), `spider`, `.led_blink`, `inittab`, `modules.dep` idênticos em p2 (p2 difere só por metadados/journal). Nenhuma atualização a buscar.

## Conteúdo do CD importado em [[40-Fontes/spider-cd]] (só fonte, 1,4 MB)
- `DE0_NANO_SOC_SPIDER_LINUX_APP/`: `Main.cpp`, `CSpider.cpp/.h`, `CSpiderLeg.cpp/.h`, `CMotor.cpp/.h`, `BtSpp*.cpp/.h`, `Queue*.cpp/.h`, `PIO_*.cpp/.h`, `mmap.cpp/.h`, `terasic_os.cpp/.h`, `hps_0.h`, `Makefile`, `autologin`, `bt/inc/` (headers BlueZ). O binário `spider` do CD é o mesmo do cartão.
- `DE0_NANO_SOC_SPIDER/`: `DE0_Nano_SoC_golden_top.v/.qsf/.qpf`, `soc_system.qsys`, `ip/spider/TERASIC_PWM_EX.v` + `PWM_Delay.v`, `TERASIC_PWM_EX_hw.tcl`, `hps_0.h`, `hps_isw_handoff/`. Fora do git: `.sof`, `.rbf`, `.jic`, `soc_system/` (HDL gerado), `.sopcinfo`.
- `Motor_Board_MAXII_Power_Monitor/`: `MAXII_Power_Monitor.v` (topo), `V/POWER_MONITOR.v`, `V/I2C_*.v`, `.qsf/.qpf/.sdc`. Fora: `.pof`, `output_files/`.
- `DE0_NANO_SOC_SPIDER_ANDROID/DEMO SPIDER/src/` + `AndroidManifest.xml`. Fora: `res/`, `bin/`, `gen/`.
- Manuais do CD = os já importados (User Manual, Programming Guide).

## Fatos extraídos (fonte primária)
### Controlador PWM (`ip/spider/TERASIC_PWM_EX.v`, `CMotor.cpp`)
- 4 registradores de 32 bits por canal: `0 TOTAL_DUR`, `1 HIGH_DUR`, `2 ADJ_SPEED` (escrita) / `STATUS` (leitura, bit0 = PWM final enviado), **`3 ABORT`** (1 = desliga o PWM; é o "power saving" do manual). O Programming Guide omite o registrador 3.
- Canais em `hps_0.h`: `PWM17_BASE 0x0` … `PWM0_BASE 0x110`, 16 bytes cada, na ponte lightweight (`CMMap::MAP_H2F_LW_BRIDGE`, `ALT_STM_OFST`+span 0x04000000 — `mmap.cpp`).
- `DUR_CLOCK_NUM = 50 000 000/50 = 1 000 000` ticks = 20 ms. Pulso: `PWM_DEGREE_MIN = 25 000` (0,5 ms) ↔ −90°, `PWM_DEGREE_MAX = 125 000` (2,5 ms) ↔ +90°; 1,5 ms = 0°. **Faixa de ângulo −90…+90°** (`DEGREE_MIN/MAX`). Motores 0–8 são espelhados (`m_nMotorID < 9 → −ângulo`).
- Velocidade: `SetSpeed(0..100)` → divisor em `ADJ_SPEED` de `1150` (lento, 0) a `550` (rápido, 100); o construtor escreve `1700` (`PWM_SPEED_MAX+PWM_SPEED_MIN`, mais lento ainda) até o primeiro `SetSpeed`. `m_Speed(1800)` só alimenta a estimativa de tempo: `0,17 s/60° × m_Speed × 5` por grau (comentário "0.17sec/60degree(4.8v)").
- `IsReady()` lê `STATUS == 1`; `ReadyTime()` é estimativa em ticks do SO.

### CPLD da Servo Motor Card (`MAXII_Power_Monitor.v`)
- INA230 em `0x80` (8 bits, = 0x40 7 bits). `CONFIGURATION = 0x4127` → decodificação pelo layout do datasheet `[INFERÊNCIA]`: AVG = 128 amostras, VBUSCT = 140 µs, VSHCT = 1,1 ms, modo contínuo shunt+bus → janela ≈ 128 × 1,24 ms ≈ **159 ms**.
- `CALIBRATION = 2560`, `CURRENT_LSB = 1 mA` → shunt = 0,00512/(0,001 × 2560) = **2 mΩ** `[INFERÊNCIA, fórmula do datasheet]`. Potência LSB 25 mW. Tensão LSB 1,25 mV.
- **Corte por bateria baixa: `VOLTAGE_BUS < 5,9 V` sustentado por 2 s** (`LOWBAT_TIMEOUT = 24 MHz × 2`).
- **Corte por sobrecarga: potência > 65 W sustentada por 0,2 s** (`MAX_POWER = 65 000 mW`, `OVERPOWER_TIMEOUT = 24 MHz/5`). A 6 V ≈ 10,8 A médios na janela do INA230.
- LEDs de bateria: 3 acesos > 6,35 V; 2 > 6,12 V; 1 > 5,9 V. Faixa compatível com **dois packs NiMH de 5 células em paralelo** (6 V nominal) — não em série `[INFERÊNCIA]`.
- `SOC_SYS_PDN_n` = 0 em qualquer alarme: desliga servos e o 5 V do DE0 (confirma o manual). Sem rearme automático: só desligar/ligar a chave.

### Comandos (`BtSppCommand.cpp`, `Main.cpp`)
- Além do manual: `ATST` (parar), `ATVR=?` (versão), `ATJS=` (joystick). Parser em `CBtSppCommand::CommandParsing`.

# Dia 1 — parte B: placa ligada, UART (2026-10-22, log completo em `dia-1-uart.log`)

Setup: fonte 5 V no jack do DE0, Servo Motor Card OFF, dongle Bluetooth CSR8510 no OTG, cabo no Mini-B **UART** (o outro Mini-B é o USB-Blaster II, `09fb:6810`). FT232R `0403:6001`, 115200 8N1, `/dev/ttyUSB1` no PC (grupo `plugdev`). Login automático como root; o app `spider` sobe sozinho e imprime `===== Spider Demo =====`.

## Correção à parte A
- **Kernel real: `3.13.0-00298-g3c7cbb9-dirty` (2015-01-07, gcc 4.6.3 Sourcery CodeBench 2012.03)**. O diretório `/lib/modules/3.9.0` do rootfs é resto de imagem anterior — ignorar.
- U-Boot SPL/U-Boot 2013.01.01 (2014-12-29/30). `bootcmd` lê `zImage`, `socfpga.dtb`, `soc_system.rbf` da p1 (o `u-boot.scr` com `de0_nano_soc.rbf` não é usado).
- `cmdline`: `console=ttyS0,115200 root=/dev/mmcblk0p2 rw rootwait`. CMA reserva 512 MiB; `free` mostra 1 030 232 kB total, ~1 010 MB livres.

## Nós de dispositivo (confirma a rota HPS — ADR-009)
```
/dev/i2c-0  /dev/i2c-1  /dev/spidev1.0  /dev/watchdog (10,130)  /dev/mem  /dev/ttyS0  /dev/ttyS1
```
- `CONFIG_I2C_CHARDEV=y`, `CONFIG_SPI_SPIDEV=y`, `CONFIG_DW_WATCHDOG=y`, `CONFIG_BT_RFCOMM=y`, `CONFIG_USB_DWC2_DUAL_ROLE=y`.
- **`CONFIG_PREEMPT_NONE=y`** (sem preempção): jitter do laço de controle vai depender disso; medir (S00). `sched_rt_runtime_us = 950000`.
- `/sys/bus/spi/devices/spi1.0` → driver `spidev`. Interrupções: `dw_spi1` (187), `ffc04000.i2c` (190), `ffc05000.i2c` (191), `fpga-mgr` (207).
- GPIO do HPS: `gpiochip171` (gpio@ff70a000), `gpiochip198` (gpio@ff709000), `gpiochip227` (gpio@ff708000), nenhum exportado.
- `i2cdetect -y -r 0`: **0x53** (acelerômetro ADXL345) — barramento I2C0 funciona.
- `i2cdetect -y -r 1`: dezenas de endereços falsos + `controller timed out` + `lost arbitration` → **I2C1 está sem pull-up** (SDA/SCL flutuando no header LTC). Qualquer módulo ligado ali precisa de pull-up 2,2–4,7 kΩ para 3,3 V (a maioria dos breakouts BH1750/VL53L1X/PCA9685 já traz). Sem sensor ligado, o barramento é inutilizável — comportamento esperado, não defeito.
- `i2cdetect -y N` (quick write) não é suportado pelo driver DesignWare; usar `-r`.

## Ferramentas na placa
- BusyBox 1.20.2 (`head -N` não existe; usar `head -n N`). Sem `gcc`, `python`, `ldd`. Tem `i2cdetect`, `strace`, `gdbserver`, `make`, `perl`, `sshd`, `lighttpd`, `udhcpc` (sem lease sem cabo).
- Bluetooth: `hci0` UP RUNNING PSCAN ISCAN, BD `00:1A:7D:DA:71:13`. **Aviso recorrente**: `256 KiB atomic DMA coherent pool is too small!` e `dwc2 … Failed to allocate memory to handle non-dword aligned buffer` (21 ocorrências) e `hci0 command 0x0c14 tx timeout`. O demo original funciona assim; se a teleoperação por BT falhar, acrescentar `coherent_pool=1M` ao `bootargs` do U-Boot (`[INFERÊNCIA]`, testar).

## Conclusão
S01 completa. Rota HPS confirmada ao vivo: I2C1, SPI1 (spidev), watchdog, OTG. ADR-009 pode ser aceita com a condição dos pull-ups.
