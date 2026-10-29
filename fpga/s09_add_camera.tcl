# S09 — acrescenta a captura da OV7670 e os PIOs de controle (aplicar DEPOIS do s08_add_components.tcl).
# Nada existente muda. Endereços (contrato da S09): pio_cam 0x01C0, pio_cam_in 0x01D0, cam_capture 0x10000.
# Uso: qsys-script --system-file=soc_system.qsys --script=s09_add_camera.tcl --search-path='$,.,ip/spider,ip/cam'
package require qsys
load_system soc_system.qsys

proc lw_slave {inst clkif iface base} {
    add_connection clk_50.clk $inst.$clkif
    add_connection clk_50.clk_reset $inst.reset
    add_connection hps_0.h2f_lw_axi_master $inst.$iface
    set_connection_parameter_value hps_0.h2f_lw_axi_master/$inst.$iface baseAddress $base
}

# Saída: bit0 SIOC_LOW, bit1 SIOD_LOW (open-drain no topo), bit2 RESET_N, bit3 PWDN. Reset 0 = câmera em reset.
add_instance pio_cam altera_avalon_pio
set_instance_parameter_value pio_cam width 4
set_instance_parameter_value pio_cam direction Output
set_instance_parameter_value pio_cam resetValue 0
lw_slave pio_cam clk s1 0x01C0
add_interface pio_cam_external_connection conduit end
set_interface_property pio_cam_external_connection EXPORT_OF pio_cam.external_connection

# Entrada: bit0 SIOD, bit1 SIOC, bit2 VSYNC, bit3 HREF (diagnóstico e leitura do SCCB).
add_instance pio_cam_in altera_avalon_pio
set_instance_parameter_value pio_cam_in width 4
set_instance_parameter_value pio_cam_in direction Input
lw_slave pio_cam_in clk s1 0x01D0
add_interface pio_cam_in_external_connection conduit end
set_interface_property pio_cam_in_external_connection EXPORT_OF pio_cam_in.external_connection

# Captura do quadro (fpga/cam/cam_capture.v).
add_instance cam_capture cam_capture
lw_slave cam_capture clock s0 0x10000
add_interface cam_capture_cam conduit end
set_interface_property cam_capture_cam EXPORT_OF cam_capture.cam

save_system soc_system.qsys
