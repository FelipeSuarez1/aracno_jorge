#!/bin/sh
# Simula o medula (S11) contra o testbench (Questa FSE do Quartus Prime Lite 25.1std).
# Uso: fpga/medula/sim.sh   → uma rodada (~0,5 s simulado); código de saída 0 só com TB OK.
set -eu
Q=${QUESTA:-$HOME/altera_lite/25.1std/questa_fse/bin}
export LM_LICENSE_FILE=${LM_LICENSE_FILE:-$HOME/altera_lite/25.1std/license_questa/license.dat}
D=$(cd "$(dirname "$0")" && pwd)
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
cd "$W"
"$Q/vlib" work >/dev/null
"$Q/vlog" -quiet "$D/medula.v" "$D/tb_medula.v"
"$Q/vsim" -c -quiet tb_medula -do "run -all; quit -f" > sim.log 2>&1 || true
grep -E 'TB |FALHA|Error|LATENCY|watchdog' sim.log | head -40
grep -q 'TB OK' sim.log
