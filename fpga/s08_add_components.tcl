# S08 — acrescenta ao soc_system.qsys da Terasic (Spider CD) os componentes do display e da cabeça,
# SEM alterar nada que já existe (hps_0, pwm0..17, PIOs, sysid: mesmos parâmetros e endereços).
# Uso (na cópia de trabalho do projeto):
#   qsys-script --system-file=soc_system.qsys --script=s08_add_components.tcl
# Endereços na ponte lightweight (os existentes vão de 0x000 a 0x15F):
#   pwm18 0x160, pwm19 0x170 (16 B cada) · pio_lcd 0x180 · pio_in 0x190 (16 B) · spi_lcd 0x1A0 (32 B)
package require qsys
load_system soc_system.qsys

proc lw_slave {inst clkif iface base} {
    add_connection clk_50.clk $inst.$clkif
    add_connection clk_50.clk_reset $inst.reset
    add_connection hps_0.h2f_lw_axi_master $inst.$iface
    set_connection_parameter_value hps_0.h2f_lw_axi_master/$inst.$iface baseAddress $base
}

# --- 2 PWM para a cabeça (mesmo IP dos 18 da marcha: ip/spider/TERASIC_PWM_EX.v) -------------------
foreach {name base} {pwm18 0x0160 pwm19 0x0170} {
    add_instance $name TERASIC_PWM_EX
    lw_slave $name clock avalon_slave_0 $base
    add_interface ${name}_conduit_end conduit end
    set_interface_property ${name}_conduit_end EXPORT_OF $name.conduit_end
}

# --- PIO de saída: bit0 D/C, bit1 RST (ativo baixo → reset em 1 = display liberado), bit2/3 XSHUT ToF -----
add_instance pio_lcd altera_avalon_pio
set_instance_parameter_value pio_lcd width 8
set_instance_parameter_value pio_lcd direction Output
set_instance_parameter_value pio_lcd resetValue 2
lw_slave pio_lcd clk s1 0x0180
add_interface pio_lcd_external_connection conduit end
set_interface_property pio_lcd_external_connection EXPORT_OF pio_lcd.external_connection

# --- PIO de entrada: bit0 PIR, bit1 botão de parada (NF, 1 = acionado), bits 2-3 reserva --------------------
add_instance pio_in altera_avalon_pio
set_instance_parameter_value pio_in width 4
set_instance_parameter_value pio_in direction Input
lw_slave pio_in clk s1 0x0190
add_interface pio_in_external_connection conduit end
set_interface_property pio_in_external_connection EXPORT_OF pio_in.external_connection

# --- SPI master do display (altera_avalon_spi, "SPI (3 Wire Serial)") -----------------------------------
# 50 MHz / 4 = 12,5 MHz (o core divide por número par; ILI9341 nominal ~10 MHz de escrita, mas aceita
# mais na prática — começamos em 12,5 MHz e medimos). Modo 0, 8 bits, MSB primeiro, 1 slave select.
add_instance spi_lcd altera_avalon_spi
set_instance_parameter_value spi_lcd masterSPI true
set_instance_parameter_value spi_lcd dataWidth 8
set_instance_parameter_value spi_lcd lsbOrderedFirst false
set_instance_parameter_value spi_lcd clockPolarity 0
set_instance_parameter_value spi_lcd clockPhase 0
set_instance_parameter_value spi_lcd numberOfSlaves 1
set_instance_parameter_value spi_lcd targetClockRate 12500000
lw_slave spi_lcd clk spi_control_port 0x01A0
add_interface spi_lcd_external conduit end
set_interface_property spi_lcd_external EXPORT_OF spi_lcd.external

save_system soc_system.qsys
