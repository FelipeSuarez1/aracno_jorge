#include "hal/sccb.h"
#include "aracno/qsys.h"
#include "hal/clock.h"

namespace aracno { namespace hal {

void Sccb::attach(RegIo* io, uint32_t out_base, uint32_t in_base, uint8_t addr7) {
    io_ = io;
    in_base_ = in_base;
    addr7_ = addr7;
    pio_.attach(io, out_base);
    err_ = OK;
}

void Sccb::set_reset_n(bool v) { pio_.write_bit(qsys::PIO_CAM_BIT_RESET_N, v); }
void Sccb::set_pwdn(bool v) { pio_.write_bit(qsys::PIO_CAM_BIT_PWDN, v); }

// Linha "alta" = soltar (bit *_LOW = 0); "baixa" = drenar (bit *_LOW = 1).
void Sccb::scl(bool high) { pio_.write_bit(qsys::PIO_CAM_BIT_SIOC_LOW, !high); }
void Sccb::sda(bool high) { pio_.write_bit(qsys::PIO_CAM_BIT_SIOD_LOW, !high); }
bool Sccb::sda_in() const { return (in_levels() >> qsys::PIO_CAM_IN_BIT_SIOD) & 1u; }
bool Sccb::scl_in() const { return (in_levels() >> qsys::PIO_CAM_IN_BIT_SIOC) & 1u; }

void Sccb::half() const {
    const uint64_t end = now_us() + half_us_;
    while (now_us() < end) {}
}

bool Sccb::fail(Error e) {
    err_ = e;
    ++errors_;
    return false;
}

// Solta SIOD com SIOC ainda baixo (não gera stop no meio de uma transação), depois SIOC; com os dois
// soltos e lidos em 1, SIOD desce com SIOC alto = START.
bool Sccb::start() {
    if (!io_) return fail(BUS_BUSY);
    sda(true);
    half();
    scl(true);
    half();
    if (!sda_in() || !scl_in()) return fail(BUS_BUSY);
    sda(false);
    half();
    scl(false);
    half();
    return true;
}

// SIOD baixo com SIOC baixo, sobe SIOC, depois SIOD = STOP. Termina com o barramento solto.
void Sccb::stop() {
    sda(false);
    half();
    scl(true);
    half();
    sda(true);
    half();
}

// MSB primeiro; SIOD muda só com SIOC baixo. 9º pulso: SIOD solto, escravo puxa a 0 = ACK.
bool Sccb::write_byte(uint8_t b) {
    for (int i = 7; i >= 0; --i) {
        sda((b >> i) & 1u);
        half();
        scl(true);
        half();
        scl(false);
    }
    sda(true);
    half();
    scl(true);
    half();
    const bool ack = !sda_in();
    scl(false);
    half();
    return ack;
}

uint8_t Sccb::read_byte(bool nack) {
    uint8_t b = 0;
    sda(true);
    for (int i = 0; i < 8; ++i) {
        half();
        scl(true);
        half();
        b = static_cast<uint8_t>((b << 1) | (sda_in() ? 1u : 0u));
        scl(false);
    }
    sda(nack);      // NACK = SIOD solto no 9º pulso (último byte); ACK = SIOD em 0
    half();
    scl(true);
    half();
    scl(false);
    sda(true);
    half();
    return b;
}

bool Sccb::write_reg(uint8_t reg, uint8_t val) {
    err_ = OK;
    if (!start()) return false;
    Error e = OK;
    if (!write_byte(static_cast<uint8_t>(addr7_ << 1))) e = NACK_ADDR;
    else if (!write_byte(reg)) e = NACK_REG;
    else if (!write_byte(val)) e = NACK_DATA;
    stop();
    return e == OK ? true : fail(e);
}

// Sem repeated start: fase de escrita do sub-endereço termina com STOP antes da fase de leitura.
bool Sccb::read_reg(uint8_t reg, uint8_t* val) {
    err_ = OK;
    if (!start()) return false;
    Error e = OK;
    if (!write_byte(static_cast<uint8_t>(addr7_ << 1))) e = NACK_ADDR;
    else if (!write_byte(reg)) e = NACK_REG;
    stop();
    if (e != OK) return fail(e);
    if (!start()) return false;
    if (!write_byte(static_cast<uint8_t>((addr7_ << 1) | 1u))) { stop(); return fail(NACK_ADDR); }
    const uint8_t v = read_byte(true);
    stop();
    if (val) *val = v;
    return true;
}

const char* Sccb::error_name(Error e) {
    switch (e) {
    case OK: return "ok";
    case BUS_BUSY: return "barramento preso (SIOC/SIOD não sobem: pull-up? fiação?)";
    case NACK_ADDR: return "sem ACK no endereço (câmera em reset, sem XCLK ou fora do barramento?)";
    case NACK_REG: return "sem ACK no registrador";
    case NACK_DATA: return "sem ACK no dado";
    }
    return "?";
}

}}  // namespace aracno::hal
