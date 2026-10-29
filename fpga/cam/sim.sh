#!/bin/sh
# Simula o cam_capture contra a câmera sintética (Questa FSE do Quartus Prime Lite 25.1std).
# Uso: fpga/cam/sim.sh   → roda 3 velocidades de PCLK; código de saída 0 só se as três derem TB OK.
set -eu
Q=${QUESTA:-$HOME/altera_lite/25.1std/questa_fse/bin}
export LM_LICENSE_FILE=${LM_LICENSE_FILE:-$HOME/altera_lite/25.1std/license_questa/license.dat}
D=$(cd "$(dirname "$0")" && pwd)
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
cd "$W"
"$Q/vlib" work >/dev/null
"$Q/vlog" -quiet "$D/cam_capture.v" "$D/tb_cam_capture.v"
# PCLK alinhado ao clock (80 ns) e dois assíncronos: ~1,56 MHz (320.3) e ~6,1 MHz (81.7, limite clk/8)
ok=0
for h in 80.0 320.3 81.7; do
    "$Q/vsim" -c -quiet -gHALF=$h tb_cam_capture -do "run -all; quit -f" > "sim_$h.log" 2>&1 || true
    grep -E 'TB |FALHA|Error' "sim_$h.log" | head -6
    grep -q 'TB OK' "sim_$h.log" || ok=1
done
exit $ok
