// Registradores e listas de inicialização da OV7670 (QQVGA 160×120, YUV422).
//
// Transcrito de (ambos sob licença MIT, texto da licença abaixo):
//   [PICO] https://github.com/danielmpinto/pico-ov7670-driver  (commit b8da3ae)
//          lib_ov7670/ov7670.h (nomes/endereços) e lib_ov7670/ov7670.c (listas, _window, frame_control).
//          Copyright (c) 2025 Daniel Messias da Silva Pinto.
//   [ADA]  https://github.com/adafruit/Adafruit_CircuitPython_OV7670  (commit e1dd879), adafruit_ov7670.py,
//          de onde o [PICO] foi adaptado. SPDX-FileCopyrightText: 2017 Scott Shawcroft, written for
//          Adafruit Industries; Copyright (c) 2021 Jeff Epler for Adafruit Industries.
//
//   Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
//   associated documentation files (the "Software"), to deal in the Software without restriction,
//   including without limitation the rights to use, copy, modify, merge, publish, distribute,
//   sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
//   furnished to do so, subject to the following conditions: The above copyright notice and this
//   permission notice shall be included in all copies or substantial portions of the Software.
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//   NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
//   OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
//   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Ordem de aplicação (cam/ov7670.cpp, como OV7670.__init__ do [ADA]): YUV → INIT → frame_control(QQVGA)
// → CLKRC. Diferenças em relação às referências estão marcadas com "ARACNO:".
#pragma once
#include <stdint.h>

namespace aracno { namespace cam { namespace regs {

struct RegVal { uint8_t reg; uint8_t val; };

// Endereços ([PICO] ov7670.h, "start ov7670 register defines"; = [ADA] _OV7670_REG_*).
enum : uint8_t {
    GAIN = 0x00, VREF = 0x03, AECH = 0x10, PID = 0x0A, VER = 0x0B, COM3 = 0x0C, COM4 = 0x0D,
    COM5 = 0x0E, COM6 = 0x0F, CLKRC = 0x11, COM7 = 0x12, COM8 = 0x13, COM9 = 0x14, COM10 = 0x15,
    HSTART = 0x17, HSTOP = 0x18, VSTART = 0x19, VSTOP = 0x1A, MIDH = 0x1C, MIDL = 0x1D, MVFP = 0x1E,
    ADCCTR1 = 0x21, ADCCTR2 = 0x22, AEW = 0x24, AEB = 0x25, VPT = 0x26, HREF = 0x32, CHLF = 0x33,
    ADC = 0x37, ACOM = 0x38, OFON = 0x39, TSLB = 0x3A, COM12 = 0x3C, COM14 = 0x3E, COM15 = 0x40,
    AWBC1 = 0x43, AWBC2 = 0x44, AWBC3 = 0x45, AWBC4 = 0x46, AWBC5 = 0x47, AWBC6 = 0x48,
    MTX1 = 0x4F, MTX2 = 0x50, MTX3 = 0x51, MTX4 = 0x52, MTX5 = 0x53, MTX6 = 0x54,
    BRIGHT = 0x55, CONTRAS = 0x56, CONTRAS_CENTER = 0x57,
    LCC3 = 0x64, LCC4 = 0x65, LCC5 = 0x66, GFIX = 0x69, AWBCTR3 = 0x6C, AWBCTR2 = 0x6D,
    AWBCTR1 = 0x6E, AWBCTR0 = 0x6F, SCALING_XSC = 0x70, SCALING_YSC = 0x71, SCALING_DCWCTR = 0x72,
    SCALING_PCLK_DIV = 0x73, REG74 = 0x74, SLOP = 0x7A, GAM_BASE = 0x7B, DM_LNL = 0x92,
    LCC6 = 0x94, LCC7 = 0x95, HAECC1 = 0x9F, HAECC2 = 0xA0, SCALING_PCLK_DELAY = 0xA2,
    BD50MAX = 0xA5, HAECC3 = 0xA6, HAECC4 = 0xA7, HAECC5 = 0xA8, HAECC6 = 0xA9, HAECC7 = 0xAA,
    BD60MAX = 0xAB, ABLC1 = 0xB1, THL_ST = 0xB3
};

// Bits ([PICO] ov7670.h).
enum : uint8_t {
    COM7_RESET = 0x80, COM7_YUV = 0x00,
    COM3_DCWEN = 0x04,
    COM8_FASTAEC = 0x80, COM8_AECSTEP = 0x40, COM8_BANDING = 0x20, COM8_AGC = 0x04, COM8_AEC = 0x01,
    COM15_R00FF = 0xC0,
    TSLB_YLAST = 0x04
};

// Valores esperados nos registradores de identificação (S09 "Fatos de entrada"; [ADA] product_id /
// product_version: "The expected value is 0x76" / "0x73").
static const uint8_t PID_VALUE = 0x76;
static const uint8_t VER_VALUE = 0x73;

// [PICO] ov7670.c _OV7670_yuv (= [ADA] _OV7670_yuv): "Manual output format, YUV, use full output range".
static const RegVal YUV[] = {
    {COM7, COM7_YUV},
    {COM15, COM15_R00FF},
};

// [PICO] ov7670.c _OV7670_inits (= [ADA] _OV7670_init), na mesma ordem. Comentários entre aspas são
// das referências.
static const RegVal INIT[] = {
    {TSLB, TSLB_YLAST},             // "No auto window"
    // ARACNO: COM10 = 0x00 (VSYNC ativo alto, padrão) como no [PICO]; o [ADA] usa COM10_VS_NEG
    // ("req by SAMD PCC"). O cam_capture conta a borda de SUBIDA de VSYNC como início/fim de quadro.
    {COM10, 0x00},
    {SLOP, 0x20},
    {GAM_BASE + 0, 0x1C}, {GAM_BASE + 1, 0x28}, {GAM_BASE + 2, 0x3C}, {GAM_BASE + 3, 0x55},
    {GAM_BASE + 4, 0x68}, {GAM_BASE + 5, 0x76}, {GAM_BASE + 6, 0x80}, {GAM_BASE + 7, 0x88},
    {GAM_BASE + 8, 0x8F}, {GAM_BASE + 9, 0x96}, {GAM_BASE + 10, 0xA3}, {GAM_BASE + 11, 0xAF},
    {GAM_BASE + 12, 0xC4}, {GAM_BASE + 13, 0xD7}, {GAM_BASE + 14, 0xE8},
    {COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BANDING},
    {GAIN, 0x00},
    // Nas duas referências esta linha é `_OV7670_COM2_SSLEEP, 0x00`: a constante de BIT (0x10) usada como
    // endereço, ou seja, escreve AECH (0x10) = 0. Mantido igual à lista testada.
    {AECH, 0x00},
    {COM4, 0x00},
    {COM9, 0x20},                   // "Max AGC value"
    {BD50MAX, 0x05}, {BD60MAX, 0x07}, {AEW, 0x75}, {AEB, 0x63}, {VPT, 0xA5},
    {HAECC1, 0x78}, {HAECC2, 0x68},
    {0xA1, 0x03},                   // "Reserved register?"
    {HAECC3, 0xDF},                 // "Histogram-based AEC/AGC setup"
    {HAECC4, 0xDF}, {HAECC5, 0xF0}, {HAECC6, 0x90}, {HAECC7, 0x94},
    {COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BANDING | COM8_AGC | COM8_AEC},
    {COM5, 0x61}, {COM6, 0x4B},
    {0x16, 0x02},                   // "Reserved register?"
    {MVFP, 0x07},
    {ADCCTR1, 0x02}, {ADCCTR2, 0x91},
    {0x29, 0x07},                   // "Reserved register?"
    {CHLF, 0x0B},
    {0x35, 0x0B},                   // "Reserved register?"
    {ADC, 0x1D}, {ACOM, 0x71}, {OFON, 0x2A}, {COM12, 0x78},
    {0x4D, 0x40},                   // "Reserved register?"
    {0x4E, 0x20},                   // "Reserved register?"
    {GFIX, 0x5D}, {REG74, 0x19},
    {0x8D, 0x4F}, {0x8E, 0x00}, {0x8F, 0x00}, {0x90, 0x00}, {0x91, 0x00},   // "Reserved register?"
    {DM_LNL, 0x00},
    {0x96, 0x00}, {0x9A, 0x80}, {0xB0, 0x84},                               // "Reserved register?"
    {ABLC1, 0x0C},
    {0xB2, 0x0E},                   // "Reserved register?"
    {THL_ST, 0x82},
    {0xB8, 0x0A},                   // "Reserved register?"
    {AWBC1, 0x14}, {AWBC2, 0xF0}, {AWBC3, 0x34}, {AWBC4, 0x58}, {AWBC5, 0x28}, {AWBC6, 0x3A},
    {0x59, 0x88}, {0x5A, 0x88}, {0x5B, 0x44}, {0x5C, 0x67}, {0x5D, 0x49}, {0x5E, 0x0E},  // "Reserved register?"
    {LCC3, 0x04}, {LCC4, 0x20}, {LCC5, 0x05}, {LCC6, 0x04}, {LCC7, 0x08},
    {AWBCTR3, 0x0A}, {AWBCTR2, 0x55},
    {MTX1, 0x80}, {MTX2, 0x80}, {MTX3, 0x00}, {MTX4, 0x22}, {MTX5, 0x5E},
    {MTX6, 0x80},                   // "0x40?"
    {AWBCTR1, 0x11},
    {AWBCTR0, 0x9F},                // "Or use 0x9E for advance AWB"
    {BRIGHT, 0x00}, {CONTRAS, 0x40},
    {CONTRAS_CENTER, 0x80},         // "0x40?"
    // ARACNO: o [PICO] termina com CLKRC = 0x50 (relógio externo direto, XCLK de 16 MHz); aqui o CLKRC
    // é escrito à parte, no fim (CLKRC_VALUE abaixo).
};

// frame_control para QQVGA: [PICO] _window[OV7670_SIZE_DIV4] = {11, 186, 2, 2} (= [ADA] _window[2])
// = vstart, hstart, edge_offset, pclk_delay; contas de ov7670_frame_control ([ADA] _frame_control)
// com size = OV7670_SIZE_DIV4 = 2.
static const int QQVGA_SIZE = 2;
static const int QQVGA_VSTART = 11, QQVGA_HSTART = 186, QQVGA_EDGE_OFFSET = 2, QQVGA_PCLK_DELAY = 2;
static const int QQVGA_VSTOP = QQVGA_VSTART + 480;              // "vstop = vstart + 480"
static const int QQVGA_HSTOP = (QQVGA_HSTART + 640) % 784;      // "hstop = (hstart + 640) % 784"

// Primeira parte (antes do read-modify-write de SCALING_XSC/YSC).
static const RegVal QQVGA_SCALE[] = {
    {COM3, COM3_DCWEN},                                         // downsampling (size > DIV1), sem zoom
    {COM14, 0x18 + QQVGA_SIZE},                                 // "Enable PCLK division if sub-VGA": ÷4
    {SCALING_DCWCTR, QQVGA_SIZE * 0x11},                        // downsample H e V 1:4
    {SCALING_PCLK_DIV, 0xF0 + QQVGA_SIZE},                      // "Pixel clock divider if sub-VGA": ÷4
};
// SCALING_XSC/YSC: bits 6:0 = 0x20 ("1.0", sem zoom), bit7 (padrão de teste) preservado lendo antes.
static const uint8_t QQVGA_XYSC = 0x20;

// Segunda parte: janela ("Window size is scattered across multiple registers").
static const RegVal QQVGA_WINDOW[] = {
    {HSTART, QQVGA_HSTART >> 3},
    {HSTOP, QQVGA_HSTOP >> 3},
    {HREF, (QQVGA_EDGE_OFFSET << 6) | ((QQVGA_HSTOP & 7) << 3) | (QQVGA_HSTART & 7)},
    {VSTART, QQVGA_VSTART >> 2},
    {VSTOP, QQVGA_VSTOP >> 2},
    {VREF, ((QQVGA_VSTOP & 3) << 2) | (QQVGA_VSTART & 3)},
    {SCALING_PCLK_DELAY, QQVGA_PCLK_DELAY},
};

// ARACNO: CLKRC (reg 0x11; [PICO] ov7670.h: CLK_EXT 0x40 "Use ext clock directly", CLK_SCALE 0x3F "Int clock
// prescale mask"): bit6 = 0 usa o prescaler, F_interno = XCLK / (bits[5:0] + 1) [INFERÊNCIA: fórmula do datasheet,
// não conferida no PDF neste commit].
// XCLK = 12,5 MHz (FPGA, 50 MHz / 4) ÷ 2 → 6,25 MHz interno; em QQVGA COM14/SCALING_PCLK_DIV dividem o PCLK
// por mais 4 → PCLK ≈ 1,56 MHz, folgado para a amostragem a 50 MHz do cam_capture (limite ~6,25 MHz).
// Quadros/s ≈ 30 × 6,25 / 24 ≈ 7,8 [INFERÊNCIA: 30 fps em VGA com relógio interno de 24 MHz; medir no LAB].
static const uint8_t CLKRC_VALUE = 0x01;

}}}  // namespace aracno::cam::regs
