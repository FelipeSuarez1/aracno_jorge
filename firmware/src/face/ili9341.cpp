#include "face/ili9341.h"

namespace aracno { namespace face {

Ili9341::Ili9341()
    : spi_(0), pins_(0), ok_(false), canvas_(0), row_(0), col_(0), pending_(0), failed_(false), bytes_sent_(0) {
    rect_.x = rect_.y = 0; rect_.w = rect_.h = 0;
}

bool Ili9341::cmd(uint8_t c) {
    if (!pins_->dc(false)) return false;
    return spi_->write(&c, 1);
}

bool Ili9341::data(const uint8_t* d, size_t n) {
    if (!pins_->dc(true)) return false;
    return spi_->write(d, n);
}

bool Ili9341::init(hal::SpiIo* spi, CtrlPins* pins, void (*sleep_ms)(uint32_t)) {
    spi_ = spi; pins_ = pins; ok_ = false;
    // Reset por hardware se o pino existe: RST baixo ≥ 10 µs, depois ≥ 120 ms (§15.4).
    if (pins_->has_reset()) {
        if (!pins_->reset(true)) return false;
        sleep_ms(10);
        if (!pins_->reset(false)) return false;
    }
    sleep_ms(120);   // com ou sem RST: cobre o power-on reset interno
    if (!cmd(SWRESET)) return false;
    sleep_ms(120);   // §8.2.2: após SWRESET, 5 ms em sleep-in; 120 ms se o display estava em sleep-out
    // Sequência mínima (sem ajustes de gamma/power da Adafruit: o display sobe com defaults).
    static const uint8_t pixfmt = 0x55;           // 16 bits/pixel RGB565 (§8.2.33)
    static const uint8_t madctl = MADCTL_LANDSCAPE;
    if (!cmd(PIXFMT) || !data(&pixfmt, 1)) return false;
    if (!cmd(MADCTL) || !data(&madctl, 1)) return false;
    if (!cmd(SLPOUT)) return false;
    sleep_ms(120);                                // §8.2.12: 120 ms até DISPON após SLPOUT
    // Tela preta antes de ligar (evita lixo da RAM).
    if (!window(0, 0, W - 1, H - 1)) return false;
    if (!cmd(RAMWR)) return false;
    if (!pins_->dc(true)) return false;
    for (size_t i = 0; i < STAGE_BYTES; ++i) stage_[i] = 0;
    size_t total = static_cast<size_t>(W) * H * 2;
    for (size_t off = 0; off < total; off += STAGE_BYTES) {
        size_t n = total - off; if (n > STAGE_BYTES) n = STAGE_BYTES;
        if (!spi_->write(stage_, n)) return false;
    }
    if (!cmd(DISPON)) return false;
    ok_ = true;
    return true;
}

bool Ili9341::window(int x0, int y0, int x1, int y1) {
    uint8_t c[4] = {static_cast<uint8_t>(x0 >> 8), static_cast<uint8_t>(x0), static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1)};
    uint8_t p[4] = {static_cast<uint8_t>(y0 >> 8), static_cast<uint8_t>(y0), static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1)};
    return cmd(CASET) && data(c, 4) && cmd(PASET) && data(p, 4);
}

bool Ili9341::begin_flush(const Canvas& c, const Rect& r) {
    if (!ok_ || r.w == 0 || r.h == 0) return false;
    // Recorta ao canvas.
    int x0 = r.x < 0 ? 0 : r.x, y0 = r.y < 0 ? 0 : r.y;
    int x1 = r.x + r.w - 1, y1 = r.y + r.h - 1;
    if (x1 >= W) x1 = W - 1;
    if (y1 >= H) y1 = H - 1;
    if (x0 > x1 || y0 > y1) return false;
    rect_.x = static_cast<int16_t>(x0); rect_.y = static_cast<int16_t>(y0);
    rect_.w = static_cast<uint16_t>(x1 - x0 + 1); rect_.h = static_cast<uint16_t>(y1 - y0 + 1);
    canvas_ = &c;
    row_ = 0; col_ = 0;
    failed_ = false;
    pending_ = static_cast<size_t>(rect_.w) * rect_.h * 2;
    if (!window(x0, y0, x1, y1) || !cmd(RAMWR) || !pins_->dc(true)) { pending_ = 0; failed_ = true; return false; }
    return true;
}

bool Ili9341::flush_step(size_t budget) {
    if (pending_ == 0) return false;
    if (budget > STAGE_BYTES) budget = STAGE_BYTES;
    if (budget < 2) budget = 2;                  // senão nunca avança
    size_t n = 0;
    // Preenche o estágio com pixels big-endian na ordem de varredura da janela.
    while (n + 2 <= budget && pending_ > 0) {
        uint16_t px = canvas_->at(rect_.x + col_, rect_.y + row_);
        stage_[n++] = static_cast<uint8_t>(px >> 8);
        stage_[n++] = static_cast<uint8_t>(px & 0xFF);
        pending_ -= 2;
        if (++col_ >= rect_.w) { col_ = 0; ++row_; }
    }
    if (n && !spi_->write(stage_, n)) { pending_ = 0; failed_ = true; return false; }
    bytes_sent_ += static_cast<uint32_t>(n);
    return pending_ > 0;
}

bool Ili9341::flush(const Canvas& c, const Rect& r) {
    if (!begin_flush(c, r)) return false;
    while (flush_step(STAGE_BYTES)) {}
    return !failed_;
}

}}  // namespace aracno::face
