# fpga

Projeto Quartus da Terasic (Spider CD, `docs/40-Fontes/spider-cd/DE0_NANO_SOC_SPIDER`) com os acréscimos da S08:
SPI do display e PIOs no GPIO_0 (JP1), 2 PWM da cabeça nas portas 3/4 da Servo Motor Card.

| Arquivo | O quê |
|---|---|
| `s08_add_components.tcl` | Platform Designer: acrescenta pwm18/19, pio_lcd, pio_in, spi_lcd (nada existente muda) |
| `DE0_Nano_SoC_golden_top.v` | topo da Terasic + ligação das portas novas (bloco "S08") |
| `DE0_Nano_SoC_golden_top.qsf` | `.qsf` da Terasic + pull-ups fracos nas entradas do GPIO_0 |
| `hps_0.h` | endereços gerados; `tests/qsys.cpp` confere o firmware contra ele |
| `build.sh` | reconstrói o `.rbf` do zero (~15 min, Quartus Prime Lite 25.1std) |

Instalar: copiar `soc_system.rbf` para a partição FAT do microSD. O `.rbf` de fábrica já está guardado como `soc_system.rbf.orig` (SHA-256 `0c6f76f3…`): **não sobrescrever** — só criar o `.orig` se ele ainda não existir.
O build é reprodutível em função, não bit a bit: o `SYSID_QSYS_TIMESTAMP` (hora da geração) entra no `.rbf`.
Voltar ao de fábrica: `soc_system.rbf.orig` → `soc_system.rbf`. Binários (`.sof`, `.rbf`, `db/`) ficam fora do git.
