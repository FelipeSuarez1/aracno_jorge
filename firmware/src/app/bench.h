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

// Modos de bancada do bloco `medula` (S11: núcleo na FPGA, base qsys::MEDULA_BASE). Não escrevem em nenhum
// registrador PWM. Todos leem o ID ANTES de qualquer escrita (num .rbf antigo 0x20000 cai no pwm17).
// Código de saída: 0 ok, 1 FPGA/ponte, 3 medula ausente/divergente.
//
// --medula-id: imprime ID, FRAMES, STATUS, HEAD_US, HEAD_TARGET, WD_CTRL e REFLEX_COUNT. 3 se ausente.
// --medula-test: roda as cenas F0..F19 da S11 no hardware e no modelo de referência e compara campo a
//   campo; imprime a latência do hardware, a ponta a ponta (upload+GO+DONE+leitura) e a do modelo em
//   software. 0 só com 0 divergências. A SMK deve estar desligada: a cabeça pode receber pulsos de 1 ms.
// --medula-wd: kicks a cada 20 ms por 2 s (TRIPPED deve ficar 0); depois para e mede o tempo até
//   TRIPPED = 1. 0 só se ficar entre 100 e 150 ms.
int medula_id();
int medula_test();
int medula_wd();

}}  // namespace aracno::app
