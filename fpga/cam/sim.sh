#!/bin/sh
# Simula o cam_capture contra a câmera sintética (Questa FSE do Quartus Prime Lite 25.1std).
# Uso: fpga/cam/sim.sh   → imprime "TB OK" ou "TB FALHOU"; código de saída 0 só se OK.
set -eu
Q=${QUESTA:-$HOME/altera_lite/25.1std/questa_fse/bin}
export LM_LICENSE_FILE=${LM_LICENSE_FILE:-$HOME/altera_lite/25.1std/license_questa/license.dat}
D=$(cd "$(dirname "$0")" && pwd)
W=$(mktemp -d)
cd "$W"
"$Q/vlib" work >/dev/null
"$Q/vlog" -quiet "$D/cam_capture.v" "$D/tb_cam_capture.v"
"$Q/vsim" -c -quiet tb_cam_capture -do "run -all; quit -f" | tee sim.log | grep -E 'TB |FALHA|Error' || true
grep -q 'TB OK' sim.log
