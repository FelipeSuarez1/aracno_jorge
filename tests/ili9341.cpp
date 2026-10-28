// Driver ILI9341 contra SPI e pinos falsos: sequência de init, janela CASET/PASET,
// ordem big-endian dos pixels, envio incremental e recorte.
#include <cstdio>
#include <cstring>
#include "hal/spi.h"
#include "face/ili9341.h"
#include "face/canvas.h"

using namespace aracno;
using namespace aracno::face;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FALHA linha %d: %s\n", __LINE__, msg); ++fails; } } while (0)

// Registra cada byte com o nível de D/C no momento do envio.
struct Ev { uint8_t b; bool data; };
static Ev evs[400000];
static size_t nev = 0;
static size_t max_write = 0;

struct FakePins : public CtrlPins {
    bool dc_level; int resets; bool rst_low; long reset_at_byte;
    FakePins() : dc_level(true), resets(0), rst_low(false), reset_at_byte(-1) {}
    virtual bool dc(bool d) { dc_level = d; return true; }
    virtual bool reset(bool low);
};
static FakePins* g_pins = 0;

bool FakePins::reset(bool low) { if (low) { ++resets; reset_at_byte = static_cast<long>(nev); } rst_low = low; return true; }

// fail_after: a escrita de número N (1-based) falha; 0 = nunca.
static unsigned writes = 0, fail_after = 0;
struct FakeSpi : public hal::SpiIo {
    virtual bool write(const uint8_t* d, size_t n) {
        ++writes;
        if (fail_after && writes == fail_after) return false;
        if (n > max_write) max_write = n;
        for (size_t i = 0; i < n && nev < sizeof evs / sizeof evs[0]; ++i) { evs[nev].b = d[i]; evs[nev].data = g_pins->dc_level; ++nev; }
        return true;
    }
    virtual bool xfer(const uint8_t* tx, uint8_t* rx, size_t n) { if (rx) std::memcpy(rx, tx, n); return write(tx, n); }
};

// Registra os sleeps (com a posição no fluxo de bytes) para conferir os tempos do datasheet.
static uint32_t sleeps[16]; static long sleep_at[16]; static int nsleep = 0;
static void rec_sleep(uint32_t ms) { if (nsleep < 16) { sleeps[nsleep] = ms; sleep_at[nsleep] = static_cast<long>(nev); ++nsleep; } }

// Procura a próxima ocorrência do comando c (byte com D/C = 0) a partir de i.
static long find_cmd(uint8_t c, size_t from) {
    for (size_t i = from; i < nev; ++i) if (!evs[i].data && evs[i].b == c) return static_cast<long>(i);
    return -1;
}

static Canvas canvas;

int main() {
    FakePins pins; g_pins = &pins;
    FakeSpi spi;
    Ili9341 lcd;

    CHECK(lcd.init(&spi, &pins, rec_sleep), "init ok");
    CHECK(pins.resets == 1 && !pins.rst_low, "um reset por hardware e RST liberado");
    long sw = find_cmd(Ili9341::SWRESET, 0), pf = find_cmd(Ili9341::PIXFMT, 0), md = find_cmd(Ili9341::MADCTL, 0);
    long so = find_cmd(Ili9341::SLPOUT, 0), on = find_cmd(Ili9341::DISPON, 0);
    CHECK(sw >= 0 && pf > sw && md > sw && so > md && on > so, "ordem SWRESET < PIXFMT/MADCTL < SLPOUT < DISPON");
    CHECK(evs[pf + 1].data && evs[pf + 1].b == 0x55, "PIXFMT = 0x55 (RGB565)");
    CHECK(evs[md + 1].data && evs[md + 1].b == 0xE8, "MADCTL = 0xE8 (MY|MX|MV|BGR, paisagem)");
    CHECK(pins.reset_at_byte == 0, "reset por hardware antes do primeiro byte SPI");
    // ≥120 ms entre RST liberado e o 1º comando; ≥120 ms entre SLPOUT e o próximo comando (§8.2.12)
    bool rst_wait = false, slp_wait = false;
    for (int i = 0; i < nsleep; ++i) {
        if (sleep_at[i] == 0 && sleeps[i] >= 120) rst_wait = true;
        if (sleep_at[i] == so + 1 && sleeps[i] >= 120) slp_wait = true;
    }
    CHECK(rst_wait, "espera ≥120 ms após reset antes de qualquer byte");
    CHECK(slp_wait, "espera ≥120 ms logo após SLPOUT");
    // tela preta: 320*240*2 bytes de dados zerados entre RAMWR e DISPON
    long rw = find_cmd(Ili9341::RAMWR, 0);
    CHECK(rw > so && rw < on, "RAMWR da tela preta antes de DISPON");
    CHECK(on - rw - 1 == 320 * 240 * 2, "tela preta = 153 600 bytes");
    {
        bool zeros = true;
        for (long i = rw + 1; i < on; ++i) if (evs[i].b != 0 || !evs[i].data) { zeros = false; break; }
        CHECK(zeros, "tela preta: todos os bytes 0 com D/C = dados");
    }
    CHECK(max_write <= Ili9341::STAGE_BYTES, "nenhuma escrita maior que 4096 (spidev bufsiz)");

    // flush de um retângulo 3x2 em (10, 20) com pixels conhecidos
    canvas.clear(0);
    canvas.rect(10, 20, 3, 2, 0xABCD, true);
    nev = 0;
    Rect r; r.x = 10; r.y = 20; r.w = 3; r.h = 2;
    CHECK(lcd.flush(canvas, r), "flush ok");
    long ca = find_cmd(Ili9341::CASET, 0), pa = find_cmd(Ili9341::PASET, 0), w2 = find_cmd(Ili9341::RAMWR, 0);
    CHECK(ca >= 0 && pa > ca && w2 > pa, "CASET, PASET, RAMWR");
    uint8_t c_exp[4] = {0, 10, 0, 12}, p_exp[4] = {0, 20, 0, 21};
    for (int i = 0; i < 4; ++i) {
        CHECK(evs[ca + 1 + i].data && evs[ca + 1 + i].b == c_exp[i], "colunas 10..12");
        CHECK(evs[pa + 1 + i].data && evs[pa + 1 + i].b == p_exp[i], "linhas 20..21");
    }
    CHECK(static_cast<long>(nev) - w2 - 1 == 3 * 2 * 2, "12 bytes de pixel");
    CHECK(evs[w2 + 1].b == 0xAB && evs[w2 + 2].b == 0xCD && evs[w2 + 1].data, "pixel big-endian, D/C = dados");

    // gradiente: px(x, y) = (y << 8) | x — detecta varredura por coluna e passo de linha errado
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 16; ++x) {
            uint16_t px = static_cast<uint16_t>((y << 8) | x);
            Sprite sp; sp.x = 0; sp.y = 0; sp.w = 1; sp.h = 1; sp.px = &px;
            canvas.blit(sp, 100 + x, y);
        }
    nev = 0;
    Rect rg; rg.x = 100; rg.y = 5; rg.w = 16; rg.h = 4;
    CHECK(lcd.flush(canvas, rg), "flush gradiente");
    w2 = find_cmd(Ili9341::RAMWR, 0);
    {
        bool ok = static_cast<long>(nev) - w2 - 1 == 16 * 4 * 2;
        long k = w2 + 1;
        for (int y = 5; y < 9 && ok; ++y)
            for (int x = 0; x < 16 && ok; ++x) {
                if (evs[k].b != y || evs[k + 1].b != x) ok = false;
                k += 2;
            }
        CHECK(ok, "gradiente: todos os pixels na ordem linha a linha, big-endian");
    }

    // coordenada > 255 vai em dois bytes: coluna 300
    nev = 0;
    Rect r2; r2.x = 300; r2.y = 0; r2.w = 20; r2.h = 1;
    lcd.flush(canvas, r2);
    ca = find_cmd(Ili9341::CASET, 0);
    CHECK(evs[ca + 1].b == 1 && evs[ca + 2].b == 44 && evs[ca + 3].b == 1 && evs[ca + 4].b == 63, "CASET 300..319 = 0x012C..0x013F");

    // recorte: retângulo saindo da tela
    nev = 0;
    Rect r3; r3.x = 310; r3.y = 230; r3.w = 50; r3.h = 50;
    CHECK(lcd.flush(canvas, r3), "flush recortado ok");
    w2 = find_cmd(Ili9341::RAMWR, 0);
    CHECK(static_cast<long>(nev) - w2 - 1 == 10 * 10 * 2, "recortado para 10x10");

    // incremental: quadro inteiro em passos de 8 kB → budget limitado ao estágio de 4 kB
    nev = 0;
    Rect full; full.x = 0; full.y = 0; full.w = 320; full.h = 240;
    CHECK(lcd.begin_flush(canvas, full), "begin_flush");
    int steps = 0;
    while (lcd.flush_step(8192)) ++steps;
    ++steps;
    CHECK(steps == 153600 / 4096 + 1, "38 passos de 4096 + resto");
    CHECK(!lcd.flushing(), "terminou");
    w2 = find_cmd(Ili9341::RAMWR, 0);
    CHECK(static_cast<long>(nev) - w2 - 1 == 153600, "quadro inteiro = 153 600 bytes");

    // budget ímpar não corta pixel ao meio
    nev = 0;
    lcd.begin_flush(canvas, r);
    lcd.flush_step(5);
    w2 = find_cmd(Ili9341::RAMWR, 0);
    CHECK(static_cast<long>(nev) - w2 - 1 == 4, "budget 5 → 2 pixels (4 bytes)");
    while (lcd.flush_step(5)) {}

    // falha de SPI no meio do quadro: flush devolve false e failed() fica true
    {
        Rect full2; full2.x = 0; full2.y = 0; full2.w = 320; full2.h = 240;
        writes = 0; fail_after = 10;
        CHECK(!lcd.flush(canvas, full2), "SPI falhando no meio: flush devolve false");
        CHECK(lcd.failed() && !lcd.flushing(), "failed() = true, nada pendente");
        fail_after = 0;
        CHECK(lcd.flush(canvas, full2) && !lcd.failed(), "próximo flush limpa o erro");
        writes = 0; fail_after = 2;   // falha já na janela (CASET)
        CHECK(!lcd.flush(canvas, full2), "falha na janela: flush devolve false");
        fail_after = 0;
    }
    // montagem sem RST: nenhum reset por hardware, SWRESET primeiro, ≥120 ms antes dele
    {
        struct NoRst : public FakePins { virtual bool has_reset() const { return false; } } nr;
        g_pins = &nr; nev = 0; nsleep = 0;
        Ili9341 lcd2;
        CHECK(lcd2.init(&spi, &nr, rec_sleep), "init sem RST ok");
        CHECK(nr.resets == 0, "sem RST: nenhum pulso de reset");
        CHECK(!evs[0].data && evs[0].b == Ili9341::SWRESET, "sem RST: primeiro byte é SWRESET");
        CHECK(nsleep > 0 && sleep_at[0] == 0 && sleeps[0] >= 120, "≥120 ms antes do SWRESET");
        g_pins = &pins;
    }
    // budget < 2 não trava
    {
        lcd.begin_flush(canvas, r);
        int guard = 0;
        while (lcd.flush_step(1) && guard < 100) ++guard;
        CHECK(guard < 100 && !lcd.flushing(), "budget 1 avança (sem laço infinito)");
    }

    std::printf("ili9341: %d falhas\n", fails);
    return fails ? 1 : 0;
}
