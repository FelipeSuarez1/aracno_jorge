#!/bin/bash
# Reconstrói o soc_system.rbf da S08 a partir do projeto original da Terasic (docs/40-Fontes/spider-cd)
# + os arquivos deste diretório. Quartus Prime Lite 25.1std (QUARTUS_ROOTDIR ou ~/altera_lite/25.1std/quartus).
# Uso: fpga/build.sh [dir_de_trabalho]   (padrão ~/dev/aracno_fpga_work/build; ~15 min)
# Saída: <dir>/soc_system.rbf (copiar para a partição 1 do microSD) e <dir>/hps_0.h (igual a fpga/hps_0.h exceto o
# timestamp do sysid — por isso o sha256 do .rbf muda a cada build: reprodutível em função, não bit a bit).
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
W=${1:-$HOME/dev/aracno_fpga_work/build}
Q=${QUARTUS_ROOTDIR:-$HOME/altera_lite/25.1std/quartus}
export PATH="$Q/bin:$Q/sopc_builder/bin:$PATH"
# Segurança: só apaga um diretório que seja claramente de build (vazio ou com marcador de build anterior).
if [ -e "$W" ] && [ -n "$(ls -A "$W")" ] && [ ! -f "$W/.aracno_fpga_build" ]; then
    echo "recusado: $W existe, não está vazio e não é um build anterior" >&2; exit 1
fi
rm -rf "$W" && mkdir -p "$W" && touch "$W/.aracno_fpga_build"
cp -r "$REPO/docs/40-Fontes/spider-cd/DE0_NANO_SOC_SPIDER/." "$W/"
cp "$REPO/fpga/DE0_Nano_SoC_golden_top.v" "$REPO/fpga/DE0_Nano_SoC_golden_top.qsf" "$W/"
cd "$W"
qsys-script --system-file=soc_system.qsys --script="$REPO/fpga/s08_add_components.tcl" --search-path='$,.,ip/spider'
qsys-generate soc_system.qsys --synthesis=VERILOG --output-directory=soc_system --family="Cyclone V" \
    --part=5CSEMA4U23C6 --search-path='$,.,ip/spider'
quartus_sh --flow compile DE0_Nano_SoC_golden_top
quartus_cpf -c DE0_Nano_SoC_golden_top.sof soc_system.rbf     # sem compressão, como o sof_to_rbf.bat da Terasic
sopc-create-header-files soc_system.sopcinfo --single hps_0.h --module hps_0
# SYSID_QSYS_TIMESTAMP é a hora da geração: o build se reproduz em função, não bit a bit.
if diff -q <(tr -d '\r' < hps_0.h | grep -v SYSID_QSYS_TIMESTAMP) <(grep -v SYSID_QSYS_TIMESTAMP "$REPO/fpga/hps_0.h"); then
    echo "hps_0.h igual ao do repo (exceto timestamp)"
else
    echo "ATENÇÃO: hps_0.h difere do repo — atualizar fpga/hps_0.h e rodar make -C firmware test" >&2; exit 1
fi
sha256sum soc_system.rbf
