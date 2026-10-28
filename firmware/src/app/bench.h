#pragma once

namespace aracno { namespace app {

// Modos de bancada. Não tocam nos PWM dos servos (podem rodar com a SMK desligada).
//
// --spi-loopback: gpio209 = 0 (mux do header LTC em SPI), envia um padrão de 256 bytes em
//   /dev/spidev1.0 e confere o que volta. Exige um jumper J17 pino 5 (MISO) ↔ pino 7 (MOSI)
//   e o display DESCONECTADO. Prova o mux e a pinagem.
//
// --face-demo: PCF8574 em /dev/i2c-1 (0x20; P4 = D/C, P5 = RST), ILI9341 em /dev/spidev1.0;
//   mostra as 7 expressões com balão, mede e imprime o tempo de quadro inteiro e de
//   atualização parcial (olhos piscando).
int spi_loopback(unsigned hz);
int face_demo(unsigned hz, unsigned seconds_per_expr);

}}  // namespace aracno::app
