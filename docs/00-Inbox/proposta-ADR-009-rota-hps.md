---
status: proposta
data: 2026-10-22
decide: Luiz, Felipe
---
# ADR-009 — Rota HPS para sensores, display e cabeça (proposta)

## Contexto
[[30-Medicoes/dia-1]] parte A: o `socfpga.dtb` original habilita `i2c@ffc05000` (I2C1), `spi@fff01000` (SPIM1) com `spidev@0`, `wd@ffd02000` e o OTG; o kernel 3.9.0 tem `i2c-dev`, `spidev`, `spi-dw-mmio` e `dw_wdt` built-in. Tudo que ADR-006 exigia para a rota HPS já está na imagem original — sem Quartus.

## Decisão (proposta)
Rota **HPS**: display SPI em `/dev/spidev1.0`, BH1750 ×2 / VL53L1X ×2 / PCA9685 em `/dev/i2c-1` (header LTC 2x7), watchdog em `/dev/watchdog`, pan/tilt pelo PCA9685. A FPGA continua intocada (só os 18 PWM originais). Rota FPGA fica como plano B para pan/tilt pela Servo Motor Card.

## Condições para aceitar
- Parte B do dia 1 confirma na UART: `ls /dev/i2c-1 /dev/spidev1.0 /dev/watchdog` existem.
- Confirmar que o `HPS_SPIM` do header LTC é o SPIM1 (manual não diz; teste: loopback MISO↔MOSI no header com `spidev_test` ou leitura do display).
- Endereços I2C dos sensores não colidem: BH1750 0x23/0x5C, VL53L1X 0x29 (um deles precisa de troca de endereço por XSHUT), PCA9685 0x40.

## Consequências
S03+ usam `spidev`/`i2c-dev` via ioctl em C++. Nenhuma spec precisa de Quartus até segunda ordem.
