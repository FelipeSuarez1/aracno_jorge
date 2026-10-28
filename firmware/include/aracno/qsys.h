// Endereços dos componentes Qsys na ponte lightweight (offset a partir de 0xFF200000).
// Fonte única: fpga/hps_0.h, gerado por sopc-create-header-files a partir do .sopcinfo da S08.
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

}}  // namespace aracno::qsys
