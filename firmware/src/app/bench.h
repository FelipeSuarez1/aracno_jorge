#pragma once

namespace aracno { namespace app {

// Modos de bancada do display (S08: SPI e PIO na FPGA, GPIO_0/JP1). Não tocam nos PWM dos servos.
//
// --spi-loopback: envia 256 bytes pelo spi_lcd e confere o que volta. Exige um jumper
//   JP1 pino 2 (MOSI) ↔ pino 6 (MISO) e o display DESCONECTADO (com CS ativo ele receberia lixo).
//
// --face-demo: ILI9341 no SPI da FPGA, D/C e RST no pio_lcd; mostra as 7 expressões com balão e
//   mede o tempo de quadro inteiro e das atualizações parciais (olhos piscando).
int spi_loopback();
int face_demo(unsigned seconds_per_expr);

// Modos de bancada da câmera OV7670 (S09: cam_capture + pio_cam/pio_cam_in na FPGA). Também não tocam
// nos PWM. Todos leem o ID do cam_capture ANTES de qualquer escrita (num .rbf antigo esses endereços
// caem no pwm17). Código de saída: 0 ok, 1 FPGA/ponte/display, 2 câmera.
//
// --cam-id: solta o reset, COM7 reset, lê PID/VER pela SCCB; 0 só se PID = 0x76 e VER = 0x73.
// --cam-raw: só solta o reset (câmera nos padrões de fábrica); imprime níveis do pio_cam_in, FRAMES ao
//   longo de 2 s e LINES/BYTES_PER_LINE de uma captura — diagnóstico de fiação/XCLK/VSYNC/HREF/PCLK.
// --cam-demo [S] [--phase 0|1]: configura QQVGA/YUV, captura em laço por S segundos e mostra Y em cinza
//   ampliado 2× no ILI9341; imprime quadros/s e LINES/BYTES_PER_LINE do primeiro quadro.
int cam_id();
int cam_raw();
int cam_demo(unsigned seconds, int phase);

}}  // namespace aracno::app
