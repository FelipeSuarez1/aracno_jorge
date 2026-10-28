// AvalonSpi/AvalonPio contra um modelo do altera_avalon_spi: um byte em transmissão por vez,
// TRDY volta após `delay` leituras de status, RRDY com o byte recebido (loopback), TMT no fim.
#include <cstdio>
#include <cstring>
#include "hal/avalon.h"
#include "face/pio_ctrl.h"

using namespace aracno;
using namespace aracno::hal;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FALHA linha %d: %s\n", __LINE__, msg); ++fails; } } while (0)

struct FakeSpiCore : public RegIo {
    uint32_t control, slaveselect, delay, busy, rx, ss_writes;
    bool rrdy, stuck, tx_while_ss_inactive, invert_rx;
    uint8_t sent[1024]; size_t nsent;
    FakeSpiCore() : control(0), slaveselect(0), delay(3), busy(0), rx(0), ss_writes(0), rrdy(false), stuck(false),
                    tx_while_ss_inactive(false), invert_rx(false), nsent(0) {}
    virtual void write32(uint32_t, uint32_t idx, uint32_t v) {
        if (idx == AvalonSpi::TXDATA) {
            if (!(control & AvalonSpi::CTL_SSO) || slaveselect != 1) tx_while_ss_inactive = true;
            if (nsent < sizeof sent) sent[nsent++] = static_cast<uint8_t>(v);
            busy = delay; rx = invert_rx ? (~v & 0xFF) : (v & 0xFF);
        } else if (idx == AvalonSpi::CONTROL) control = v;
        else if (idx == AvalonSpi::SLAVESELECT) { slaveselect = v; ++ss_writes; }
    }
    virtual uint32_t read32(uint32_t, uint32_t idx) const {
        FakeSpiCore* self = const_cast<FakeSpiCore*>(this);
        if (idx == AvalonSpi::STATUS) {
            if (stuck) return 0;
            if (self->busy) { if (--self->busy == 0) self->rrdy = true; return 0; }
            return static_cast<uint32_t>(AvalonSpi::ST_TRDY | AvalonSpi::ST_TMT) | (rrdy ? static_cast<uint32_t>(AvalonSpi::ST_RRDY) : 0u);
        }
        if (idx == AvalonSpi::RXDATA) { self->rrdy = false; return rx; }
        return 0;
    }
};

struct FakePio : public RegIo {
    uint32_t data, writes;
    FakePio() : data(0), writes(0) {}
    virtual void write32(uint32_t, uint32_t idx, uint32_t v) { if (idx == 0) { data = v; ++writes; } }
    virtual uint32_t read32(uint32_t, uint32_t) const { return data; }
};

int main() {
    // escrita: bytes na ordem, SS ativo (SSO + slave 1) durante todo byte, CS solto no fim
    {
        FakeSpiCore core; AvalonSpi spi; spi.attach(&core, 0x1A0);
        uint8_t d[5] = {0x2A, 0x00, 0x0A, 0x01, 0x3F};
        CHECK(spi.write(d, 5), "write ok");
        CHECK(core.nsent == 5 && std::memcmp(core.sent, d, 5) == 0, "5 bytes na ordem");
        CHECK(!core.tx_while_ss_inactive, "nenhum byte com SS inativo");
        CHECK((core.control & AvalonSpi::CTL_SSO) == 0, "SSO desligado no fim (CS solto)");
        CHECK(spi.timeouts() == 0, "sem timeouts");
    }
    // xfer com loopback devolve os mesmos bytes; com MISO invertido detecta diferença
    {
        FakeSpiCore core; AvalonSpi spi; spi.attach(&core, 0x1A0);
        uint8_t tx[64], rx[64];
        for (int i = 0; i < 64; ++i) tx[i] = static_cast<uint8_t>(i * 7 + 1);
        std::memset(rx, 0, sizeof rx);
        CHECK(spi.xfer(tx, rx, 64) && std::memcmp(tx, rx, 64) == 0, "loopback: rx == tx");
        core.invert_rx = true;
        CHECK(spi.xfer(tx, rx, 64) && rx[0] == static_cast<uint8_t>(~tx[0]), "rx vem do core, não do tx");
    }
    // core travado (TRDY nunca sobe): falha em tempo limitado, CS solto, timeout contado
    {
        FakeSpiCore core; core.stuck = true; AvalonSpi spi; spi.attach(&core, 0x1A0);
        uint8_t d[3] = {1, 2, 3};
        CHECK(!spi.write(d, 3), "core travado: write devolve false");
        CHECK(core.nsent == 0, "nenhum byte enviado");
        CHECK(spi.timeouts() == 1, "um timeout contado");
        CHECK((core.control & AvalonSpi::CTL_SSO) == 0, "CS solto mesmo em falha");
    }
    // sem attach: falha sem acessar nada
    {
        AvalonSpi spi; uint8_t d = 0;
        CHECK(!spi.write(&d, 1), "sem attach: false");
    }
    // n = 0: ok sem mexer no CS
    {
        FakeSpiCore core; AvalonSpi spi; spi.attach(&core, 0x1A0);
        CHECK(spi.write(0, 0) && core.ss_writes == 0, "n=0 não seleciona o escravo");
    }
    // PIO + pinos do display: D/C no bit0, RST ativo baixo no bit1, preserva os outros bits
    {
        FakePio io; AvalonPio pio; pio.attach(&io, 0x180, 0x2);
        pio.write_bit(2, true);                      // XSHUT frontal ligado
        face::PioCtrlPins pins(&pio);
        pins.dc(false); CHECK((io.data & 1) == 0, "dc(false) → bit0 = 0 (comando)");
        pins.dc(true);  CHECK((io.data & 1) == 1, "dc(true) → bit0 = 1 (dados)");
        pins.reset(true);  CHECK((io.data & 2) == 0, "reset(true) → RST baixo");
        pins.reset(false); CHECK((io.data & 2) == 2, "reset(false) → RST alto");
        CHECK((io.data & 4) == 4, "bit2 (XSHUT) preservado");
        CHECK(pins.has_reset(), "RST ligado no JP1 pino 5");
    }
    std::printf("avalon: %d falhas\n", fails);
    return fails ? 1 : 0;
}
