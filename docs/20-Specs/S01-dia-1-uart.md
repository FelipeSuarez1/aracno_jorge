---
status: aprovada
origem: [[20-Specs/S00-visao]] seção 1
depende: 
---
# S01 — Dia 1: levantamento da placa viva pela UART

## Objetivo
Saber o que a imagem Linux 2.0 original oferece (kernel, drivers I2C/SPI, compilador, watchdog, montagem do rootfs) para fechar ADR-006 (rota HPS ou FPGA) e definir a toolchain de S02.

## Fora de escopo
Nenhuma modificação na microSD além da cópia de backup. Nenhum código novo.

## Fatos de entrada
- UART-USB mini-B, 115200 8N1 ([[40-Fontes/de0-nano-soc-user-manual]] §3.7). Login `root` sem senha na imagem Terasic `[INFERÊNCIA]`.
- App `spider` sobe pelo `/etc/inittab` e ocupa os servos ([[40-Fontes/spider-programming-guide]] §4.1): fazer o levantamento com a chave da Servo Motor Card **desligada** ou com a aranha suspensa.

## Critérios de aceite
- [LAB] Imagem de backup da microSD gravada em disco (`dd`) com SHA-256 anotado em `30-Medicoes/dia-1.md`.
- [LAB] Saída completa dos comandos abaixo colada em `30-Medicoes/dia-1.md`.
- [LAB] Conta Terasic criada; `Spider CD-ROM 1.0.1` e `Linux Image 2.0` baixados; conteúdo do CD (pastas `Program/DE0_NANO_SOC_SPIDER*`, código do CPLD, esquemático) copiado para `docs/40-Fontes/spider-cd/` (sem binários grandes; `.rbf`/imagens ficam fora do git).
- [PC] Com a saída colada, a IA escreve ADR-009 (rota escolhida) como proposta no Inbox.

## Verificação
Procedimento (humano):
1. Chave da Servo Motor Card OFF. Ligar o DE0 pela fonte 5 V. Cabo mini-USB na UART. `screen /dev/ttyUSB0 115200` (ou PuTTY).
2. Rodar e copiar tudo:
```sh
uname -a; cat /proc/version; cat /etc/os-release 2>/dev/null; cat /etc/issue
gcc --version; g++ --version; python --version; python3 --version; ldd --version | head -1
free -m; df -h; cat /proc/mounts; cat /proc/cpuinfo | head -12
ls -l /dev/i2c* /dev/spidev* /dev/mem /dev/watchdog* /dev/ttyS* /dev/rfcomm* 2>&1
ls /proc/device-tree/soc/ 2>&1; ls /sys/bus/i2c/devices/ 2>&1; ls /sys/class/gpio/ 2>&1
zcat /proc/config.gz 2>/dev/null | grep -E 'I2C_DESIGNWARE|SPI_CADENCE|SPI_DW|SPIDEV|I2C_CHARDEV|DW_WATCHDOG|BT_RFCOMM|USB_DWC2|SND_USB' 
dmesg | head -80; cat /proc/interrupts
cat /etc/inittab; ls -l /home/root; cat /sys/kernel/debug/gpio 2>&1 | head -40
lsusb 2>&1; hciconfig -a 2>&1; bluetoothctl --version 2>&1
```
3. Desligar, retirar a microSD, no PC: `sudo dd if=/dev/sdX of=spider-sd-original.img bs=4M status=progress && sha256sum spider-sd-original.img`. Guardar fora do repo (tamanho de GB).
4. Terasic: criar conta em terasic.com.tw, baixar CD-ROM e Linux Image da página de Resources do Spider.

Formato de `30-Medicoes/dia-1.md`: cabeçalho com data, quem fez, hash do backup; depois um bloco ```text``` por comando.

## Plano
Humano executa; IA consome a saída e propõe ADR-009 + ajusta S02 (toolchain) se `gcc` existir na placa.

## Evidência
(colar aqui ou linkar `30-Medicoes/dia-1.md`)
