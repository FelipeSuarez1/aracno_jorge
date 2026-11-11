# S11 — acrescenta o medula (olhar que segue, reflexo, watchdog de servos) entre os PWM e os pinos
# (aplicar DEPOIS do s09_add_camera.tcl). Nada existente muda. Endereço (contrato da S11): medula 0x20000.
# Uso: qsys-script --system-file=soc_system.qsys --script=s11_add_medula.tcl --search-path='$,.,ip/spider,ip/cam,ip/medula'
package require qsys
load_system soc_system.qsys

proc lw_slave {inst clkif iface base} {
    add_connection clk_50.clk $inst.$clkif
    add_connection clk_50.clk_reset $inst.reset
    add_connection hps_0.h2f_lw_axi_master $inst.$iface
    set_connection_parameter_value hps_0.h2f_lw_axi_master/$inst.$iface baseAddress $base
}

# fpga/medula/medula.v: pwm_in = saída das 20 Terasic; pwm_out = pinos (com corte do watchdog e cabeça própria).
add_instance medula medula
lw_slave medula clock s0 0x20000
add_interface medula_pwm conduit end
set_interface_property medula_pwm EXPORT_OF medula.pwm

save_system soc_system.qsys
