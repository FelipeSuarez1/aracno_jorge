#pragma once
#include "face/ili9341.h"
#include "sense/i2c_devices.h"

namespace aracno { namespace face {

// D/C e RST do ILI9341 pelo PCF8574 (P4 = D/C, P5 = RST — mapa em sense/i2c_devices.h).
// Cada troca de D/C custa uma transação I2C (~0,3 ms a 100 kHz): comandos são raros e
// pixels fluem com D/C fixo em 1, então o custo por quadro é de poucos ms.
class PcfCtrlPins : public CtrlPins {
public:
    // dc_ começa conhecido: Pcf8574::init escreve 0xFF (D/C = 1).
    explicit PcfCtrlPins(sense::Pcf8574* pcf) : pcf_(pcf), dc_(true), known_(true), toggles_(0) {}
    virtual bool dc(bool data) {
        if (known_ && data == dc_) return true;   // evita transação se já está no nível certo
        if (!pcf_->write_bit(sense::Pcf8574::LCD_DC, data)) { known_ = false; return false; }
        dc_ = data; known_ = true; ++toggles_;
        return true;
    }
    virtual bool reset(bool low) { return pcf_->write_bit(sense::Pcf8574::LCD_RST, !low); }
    uint32_t toggles() const { return toggles_; }
    // Usar se o PCF8574 for reinicializado: o próximo dc() reescreve o pino.
    void invalidate() { known_ = false; }
private:
    sense::Pcf8574* pcf_;
    bool dc_;
    bool known_;
    uint32_t toggles_;
};

}}  // namespace aracno::face
