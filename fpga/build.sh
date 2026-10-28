#!/bin/sh
# Reconstrói o soc_system.rbf da S08 a partir do projeto original da Terasic (docs/40-Fontes/spider-cd)
# + os arquivos deste diretório. Quartus Prime Lite 25.1std (QUARTUS_ROOTDIR ou ~/altera_lite/25.1std/quartus).
# Uso: fpga/build.sh [dir_de_trabalho]   (padrão ~/dev/aracno_fpga_work/build; ~15 min)
# Saída: <dir>/soc_system.rbf (copiar para a partição 1 do microSD) e <dir>/hps_0.h (deve ser igual a fpga/hps_0.h).
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
W=${1:-$HOME/dev/aracno_fpga_work/build}
Q=${QUARTUS_ROOTDIR:-$HOME/altera_lite/25.1std/quartus}
export PATH="$Q/bin:$Q/sopc_builder/bin:$PATH"
rm -rf "$W" && mkdir -p "$W"
cp -r "$REPO/docs/40-Fontes/spider-cd/DE0_NANO_SOC_SPIDER/." "$W/"
cp "$REPO/fpga/DE0_Nano_SoC_golden_top.v" "$REPO/fpga/DE0_Nano_SoC_golden_top.qsf" "$W/"
cd "$W"
qsys-script --system-file=soc_system.qsys --script="$REPO/fpga/s08_add_components.tcl" --search-path='$,.,ip/spider'
qsys-generate soc_system.qsys --synthesis=VERILOG --output-directory=soc_system --family="Cyclone V" \
    --part=5CSEMA4U23C6 --search-path='$,.,ip/spider'
quartus_sh --flow compile DE0_Nano_SoC_golden_top
quartus_cpf -c DE0_Nano_SoC_golden_top.sof soc_system.rbf     # sem compressão, como o sof_to_rbf.bat da Terasic
sopc-create-header-files soc_system.sopcinfo --single hps_0.h --module hps_0
tr -d '\r' < hps_0.h | diff -q - "$REPO/fpga/hps_0.h" && echo "hps_0.h igual ao do repo"
sha256sum soc_system.rbf
