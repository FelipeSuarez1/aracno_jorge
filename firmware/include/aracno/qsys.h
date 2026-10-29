// Endereços dos componentes Qsys na ponte lightweight (offset a partir de 0xFF200000).
// Fonte única: fpga/hps_0.h, gerado por sopc-create-header-files a partir do .sopcinfo (S08; câmera: S09).
// tests/qsys.cpp lê fpga/hps_0.h e falha se qualquer valor aqui divergir.
#pragma once
#include <stdint.h>

namespace aracno { namespace qsys {

static const uint32_t PWM0_BASE     = 0x110;   // PWMn_BASE = 0x110 - 0x10*n (n = 0..17)
static const uint32_t PWM18_BASE    = 0x160;   // S08: pan da cabeça (SMK porta 3)
static const uint32_t PWM19_BASE    = 0x170;   // S08: tilt da cabeça (SMK porta 4)
static const uint32_t LED_PIO_BASE  = 0x120;
static const uint32_t BUTTON_PIO_BASE = 0x140;
static const uint32_t SYSID_QSYS_BASE = 0x158;
static const uint32_t PIO_LCD_BASE  = 0x180;   // S08: bit0 D/C, bit1 RST (1 = liberado), bit2/3 XSHUT
static const uint32_t PIO_LCD_RESET_VALUE = 2; // RST liberado, D/C = 0, XSHUT = 0 ao configurar a FPGA
static const uint32_t PIO_IN_BASE   = 0x190;   // S08: bit0 PIR, bit1 botão de parada (1 = acionado)
static const uint32_t SPI_LCD_BASE  = 0x1A0;   // S08: altera_avalon_spi, master 8 bits, 12,5 MHz

// S09 (câmera OV7670). Contrato em docs/20-Specs/S09-camera-ov7670.md, seção "Contrato FPGA ↔ firmware".
// Num .rbf sem estes blocos os endereços caem no pwm17 (ver app/bench.cpp): NADA de escrita antes de
// conferir o registrador ID do cam_capture (cam::Ov7670::present).
static const uint32_t CAM_CAPTURE_BASE = 0x10000; // cam_capture: registradores + quadro (span 64 KiB)
static const uint32_t PIO_CAM_BASE    = 0x1C0;    // saída, 4 bits, reset 0 (câmera em reset)
static const uint32_t PIO_CAM_IN_BASE = 0x1D0;    // entrada, 4 bits
// pio_cam (saída), índice do bit. *_LOW: 1 = drena o pino a 0, 0 = solta (pull-up 4,7 kΩ sobe).
static const int PIO_CAM_BIT_SIOC_LOW = 0;
static const int PIO_CAM_BIT_SIOD_LOW = 1;
static const int PIO_CAM_BIT_RESET_N  = 2;        // 0 = câmera em reset
static const int PIO_CAM_BIT_PWDN     = 3;        // 1 = câmera desligada
// pio_cam_in (entrada), índice do bit: nível no pino.
static const int PIO_CAM_IN_BIT_SIOD  = 0;
static const int PIO_CAM_IN_BIT_SIOC  = 1;
static const int PIO_CAM_IN_BIT_VSYNC = 2;
static const int PIO_CAM_IN_BIT_HREF  = 3;

}}  // namespace aracno::qsys
