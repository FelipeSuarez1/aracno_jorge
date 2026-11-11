# S11 (aracno_jorge): o projeto da Terasic não tinha SDC. Clocks de entrada de 50 MHz; o GPIO_0 (display, PIR,
# botão, câmera) é assíncrono ao relógio de 50 MHz e é amostrado por sincronizadores/registradores próprios.
create_clock -name clk50_1 -period 20 [get_ports FPGA_CLK1_50]
create_clock -name clk50_2 -period 20 [get_ports FPGA_CLK2_50]
create_clock -name clk50_3 -period 20 [get_ports FPGA_CLK3_50]
derive_clock_uncertainty
set_false_path -from [get_ports {GPIO_0[*]}]
