# Componente Platform Designer do medula (S11). Contrato: docs/20-Specs/S11-medula-fpga.md.
package require -exact qsys 16.1

set_module_property NAME medula
set_module_property VERSION 1.0
set_module_property DISPLAY_NAME "Medula: olhar, reflexo e watchdog (aracno S11)"
set_module_property GROUP "aracno"
set_module_property AUTHOR "aracno_jorge"
set_module_property EDITABLE false
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true

add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL medula
add_fileset_file medula.v VERILOG PATH medula.v TOP_LEVEL_FILE

add_interface clock clock end
set_interface_property clock clockRate 0
add_interface_port clock clk clk Input 1

add_interface reset reset end
set_interface_property reset associatedClock clock
set_interface_property reset synchronousEdges DEASSERT
add_interface_port reset reset_n reset_n Input 1

# Slave em palavras de 32 bits, latência de leitura fixa 1 (o módulo registra readdata).
add_interface s0 avalon end
set_interface_property s0 addressUnits WORDS
set_interface_property s0 associatedClock clock
set_interface_property s0 associatedReset reset
set_interface_property s0 readLatency 1
set_interface_property s0 readWaitTime 0
set_interface_property s0 writeWaitTime 0
set_interface_property s0 maximumPendingReadTransactions 0
add_interface_port s0 avs_address address Input 15
add_interface_port s0 avs_read read Input 1
add_interface_port s0 avs_write write Input 1
add_interface_port s0 avs_writedata writedata Input 32
add_interface_port s0 avs_readdata readdata Output 32

add_interface pwm conduit end
add_interface_port pwm pwm_in pwm_in Input 20
add_interface_port pwm pwm_out pwm_out Output 20
