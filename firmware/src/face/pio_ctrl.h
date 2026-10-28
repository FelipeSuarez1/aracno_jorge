#pragma once
#include "face/ili9341.h"
#include "hal/avalon.h"

namespace aracno { namespace face {

// D/C e RST do ILI9341 pelo PIO da FPGA (pio_lcd, S08): bit0 = D/C, bit1 = RST (ativo baixo).
// Escrita direta num registrador da ponte: não falha e não bloqueia.
class PioCtrlPins : public CtrlPins {
public:
    enum { BIT_DC = 0, BIT_RST = 1 };
    explicit PioCtrlPins(hal::AvalonPio* pio) : pio_(pio) {}
    virtual bool dc(bool data) { pio_->write_bit(BIT_DC, data); return true; }
    virtual bool reset(bool low) { pio_->write_bit(BIT_RST, !low); return true; }
private:
    hal::AvalonPio* pio_;
};

}}  // namespace aracno::face
