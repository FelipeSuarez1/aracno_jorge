# fpga

Projeto Quartus da Terasic (Spider CD, `docs/40-Fontes/spider-cd/DE0_NANO_SOC_SPIDER`) com os acréscimos da S08:
SPI do display e PIOs no GPIO_0 (JP1), 2 PWM da cabeça nas portas 3/4 da Servo Motor Card; da S09: captura da OV7670
(suspensa, ADR-011); e da S11: o `medula` (olhar que segue, reflexo de susto, watchdog de servos) entre os 20 PWM e os pinos.

| Arquivo | O quê |
|---|---|
| `s08_add_components.tcl` | Platform Designer: acrescenta pwm18/19, pio_lcd, pio_in, spi_lcd (nada existente muda) |
| `s09_add_camera.tcl` | Platform Designer: pio_cam, pio_cam_in e `cam_capture` (base 0x10000) |
| `s11_add_medula.tcl` | Platform Designer: acrescenta o `medula` (base LW 0x20000, span 0x20000) e o conduit `medula_pwm` |
| `medula/` | RTL `medula.v`, testbench `tb_medula.v`, `sim.sh` (Questa FSE) e o componente `medula_hw.tcl` |
| `DE0_Nano_SoC_golden_top.v` | topo da Terasic + ligação das portas novas (S08, S09) + `pwm_raw` → `medula` → `pwm_safe` → pinos (S11) |
| `DE0_Nano_SoC_golden_top.qsf` | `.qsf` da Terasic + pull-ups fracos nas entradas do GPIO_0 + `SDC_FILE` |
| `DE0_Nano_SoC_golden_top.sdc` | clocks de 50 MHz e `GPIO_0` como false path (o projeto da Terasic não tinha SDC) |
| `hps_0.h` | endereços gerados; `tests/qsys.cpp` confere o firmware contra ele |
| `build.sh` | reconstrói o `.rbf` do zero (S08 + S09 + S11, ~18 min, Quartus Prime Lite 25.1std) |

Instalar: copiar `soc_system.rbf` para a partição FAT do microSD. O `.rbf` de fábrica já está guardado como `soc_system.rbf.orig` (SHA-256 `0c6f76f3…`): **não sobrescrever** — só criar o `.orig` se ele ainda não existir.
O build é reprodutível em função, não bit a bit: o `SYSID_QSYS_TIMESTAMP` (hora da geração) entra no `.rbf`.
Voltar ao de fábrica: `soc_system.rbf.orig` → `soc_system.rbf`. Binários (`.sof`, `.rbf`, `db/`) ficam fora do git.
