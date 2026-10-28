#pragma once
#include <stdint.h>
#include <stddef.h>
#include "hal/lwbridge.h"
#include "hal/spi.h"

namespace aracno { namespace hal {

// altera_avalon_spi em modo master (S08). Registradores de 32 bits, índice ×4 (Embedded
// Peripherals IP User Guide, "SPI Core", Register Map):
//   0 rxdata · 1 txdata · 2 status · 3 control · 5 slaveselect
//   status: bit3 ROE, bit5 TMT (shift vazio), bit6 TRDY (pode escrever), bit7 RRDY (há byte lido)
//   control: bit10 SSO (força SS_n ativo entre bytes)
// O core não tem FIFO: um byte por escrita em txdata, esperando TRDY. CS fica ativo do primeiro
// ao último byte de cada chamada (SSO), diferente do spidev.
class AvalonSpi : public SpiIo {
public:
    AvalonSpi() : io_(0), base_(0), timeouts_(0) {}
    void attach(RegIo* io, uint32_t base) { io_ = io; base_ = base; }
    virtual bool write(const uint8_t* data, size_t n) { return xfer(data, 0, n); }
    virtual bool xfer(const uint8_t* tx, uint8_t* rx, size_t n);
    uint32_t timeouts() const { return timeouts_; }

    enum { RXDATA = 0, TXDATA = 1, STATUS = 2, CONTROL = 3, SLAVESELECT = 5 };
    enum { ST_ROE = 1u << 3, ST_TOE = 1u << 4, ST_TMT = 1u << 5, ST_TRDY = 1u << 6, ST_RRDY = 1u << 7, ST_E = 1u << 8 };
    enum { CTL_SSO = 1u << 10 };
    // Limite de leituras de status por byte. A 12,5 MHz um byte leva 0,64 µs e cada leitura
    // pela ponte ~0,1–0,5 µs [INFERÊNCIA]: 10 000 leituras ≫ 1 byte. Estourou = core travado.
    static const uint32_t POLL_LIMIT = 10000;

private:
    bool wait(uint32_t mask, uint32_t* last = 0);
    RegIo* io_;
    uint32_t base_;
    uint32_t timeouts_;
};

// altera_avalon_pio: registrador 0 = data (escrita nas saídas / leitura das entradas).
class AvalonPio {
public:
    AvalonPio() : io_(0), base_(0), shadow_(0) {}
    // Semeia a sombra lendo o registrador (o PIO de saída permite leitura: devolve data_out).
    // Um único dono por PIO: dois AvalonPio no mesmo endereço sobrescrevem os bits um do outro.
    void attach(RegIo* io, uint32_t base) { io_ = io; base_ = base; shadow_ = io ? io->read32(base, 0) : 0; }
    void write_bit(int bit, bool v) {
        if (v) shadow_ |= 1u << bit; else shadow_ &= ~(1u << bit);
        if (io_) io_->write32(base_, 0, shadow_);
    }
    uint32_t read() const { return io_ ? io_->read32(base_, 0) : 0; }
    uint32_t shadow() const { return shadow_; }
private:
    RegIo* io_;
    uint32_t base_;
    uint32_t shadow_;
};

}}  // namespace aracno::hal
