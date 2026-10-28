#include "hal/avalon.h"

namespace aracno { namespace hal {

bool AvalonSpi::wait(uint32_t mask) {
    for (uint32_t i = 0; i < POLL_LIMIT; ++i)
        if (io_->read32(base_, STATUS) & mask) return true;
    ++timeouts_;
    return false;
}

bool AvalonSpi::xfer(const uint8_t* tx, uint8_t* rx, size_t n) {
    if (!io_) return false;
    if (n == 0) return true;
    // Descarta byte recebido pendente e limpa ROE (escrita em status limpa os erros).
    if (io_->read32(base_, STATUS) & ST_RRDY) (void)io_->read32(base_, RXDATA);
    io_->write32(base_, STATUS, 0);
    io_->write32(base_, SLAVESELECT, 1);
    io_->write32(base_, CONTROL, CTL_SSO);
    bool ok = true;
    for (size_t i = 0; i < n && ok; ++i) {
        if (!wait(ST_TRDY)) { ok = false; break; }
        io_->write32(base_, TXDATA, tx ? tx[i] : 0);
        if (rx) {
            if (!wait(ST_RRDY)) { ok = false; break; }
            rx[i] = static_cast<uint8_t>(io_->read32(base_, RXDATA));
        }
    }
    // Espera o último bit sair antes de soltar o CS.
    if (ok && !wait(ST_TMT)) ok = false;
    io_->write32(base_, CONTROL, 0);
    return ok;
}

}}  // namespace aracno::hal
