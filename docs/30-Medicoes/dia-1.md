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
- Kernel: **3.9.0** (`/lib/modules/3.9.0`). Sem overlayfs (entrou em 3.18) → rootfs read-only será `ro` + tmpfs.
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
