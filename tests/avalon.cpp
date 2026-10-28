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

// Modelo do core (soc_system_spi_lcd.v gerado): registrador de espera (tx_holding) + registrador de
// deslocamento. TRDY = ~(transmitting & primed): volta assim que o byte passa para o deslocamento.
// TMT = ~transmitting & ~primed: só quando o último bit saiu. Cada leitura de STATUS avança 1 "tempo".
struct FakeSpiCore : public RegIo {
    uint32_t control, slaveselect, delay, shift_left, rx, ss_writes;
    bool primed, transmitting, rrdy, stuck, invert_rx;
    bool tx_while_ss_inactive, overrun, released_busy;
    uint8_t holding, shifting;
    uint8_t sent[1024]; size_t nsent;
    FakeSpiCore() : control(0), slaveselect(0), delay(3), shift_left(0), rx(0), ss_writes(0), primed(false),
                    transmitting(false), rrdy(false), stuck(false), invert_rx(false), tx_while_ss_inactive(false),
                    overrun(false), released_busy(false), holding(0), shifting(0), nsent(0) {}
    void step() {
        if (transmitting && --shift_left == 0) {
            transmitting = false;
            rx = invert_rx ? (~shifting & 0xFFu) : shifting;
            rrdy = true;
        }
        if (!transmitting && primed) { shifting = holding; primed = false; transmitting = true; shift_left = delay; }
    }
    uint32_t status() const {
        uint32_t st = 0;
        if (!(transmitting && primed)) st |= AvalonSpi::ST_TRDY;
        if (!transmitting && !primed) st |= AvalonSpi::ST_TMT;
        if (rrdy) st |= AvalonSpi::ST_RRDY;
        return st;
    }
    virtual void write32(uint32_t, uint32_t idx, uint32_t v) {
        if (idx == AvalonSpi::TXDATA) {
            if (!(control & AvalonSpi::CTL_SSO) || slaveselect != 1) tx_while_ss_inactive = true;
            if (!(status() & AvalonSpi::ST_TRDY)) overrun = true;     // escreveu sem TRDY: byte perdido
            if (nsent < sizeof sent) sent[nsent++] = static_cast<uint8_t>(v);
            holding = static_cast<uint8_t>(v); primed = true;
            step();
        } else if (idx == AvalonSpi::CONTROL) {
            if (!(v & AvalonSpi::CTL_SSO) && (transmitting || primed)) released_busy = true;
            control = v;
        } else if (idx == AvalonSpi::SLAVESELECT) { slaveselect = v; ++ss_writes; }
    }
    virtual uint32_t read32(uint32_t, uint32_t idx) const {
        FakeSpiCore* self = const_cast<FakeSpiCore*>(this);
        if (idx == AvalonSpi::STATUS) {
            if (stuck) return 0;
            self->step();
            return status();
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
        CHECK(!core.overrun, "nenhuma escrita em TXDATA sem TRDY");
        CHECK(!core.released_busy, "CS só sobe depois do último bit (espera TMT)");
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
        FakePio io; io.data = 0x2;                   // resetValue do pio_lcd
        AvalonPio pio; pio.attach(&io, 0x180);
        CHECK(pio.shadow() == 0x2, "attach lê o valor atual do hardware");
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
