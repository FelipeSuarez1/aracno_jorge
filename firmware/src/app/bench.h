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

}}  // namespace aracno::app
