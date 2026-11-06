#include "medula/medula.h"
#include "aracno/qsys.h"
#include "hal/clock.h"

namespace aracno { namespace medula {

uint32_t Medula::rd(uint32_t idx) const { return io_->read32(qsys::MEDULA_BASE, idx); }
void Medula::wr(uint32_t idx, uint32_t v) { io_->write32(qsys::MEDULA_BASE, idx, v); }

bool Medula::attach(hal::RegIo* io) {
    io_ = 0;
    ctrl_shadow_ = 0;
    if (!io) return false;
    if (io->read32(qsys::MEDULA_BASE, ID) != ID_VALUE) return false;   // só leitura até aqui
    io_ = io;
    last_reflex_count_ = rd(REFLEX_COUNT);
    frames_at_attach_ = rd(FRAMES);
    return true;
}

uint32_t Medula::frames() const { return io_ ? rd(FRAMES) : 0; }

bool Medula::configure(const Params& p) {
    if (!io_) return false;
    wr(THRESH, static_cast<uint32_t>(p.fg_delta) | (static_cast<uint32_t>(p.min_count) << 8));
    wr(REFLEX_CFG, static_cast<uint32_t>(p.reflex_near) | (static_cast<uint32_t>(p.reflex_drop) << 8)
                       | (static_cast<uint32_t>(p.hold_ms) << 16));
    wr(PAN_GAIN, static_cast<uint16_t>(p.pan_gain));
    wr(TILT_GAIN, static_cast<uint16_t>(p.tilt_gain));
    wr(SLEW_US, p.slew_us);
    return true;
}

void Medula::set_modes(bool follow, bool reflex) {
    if (!io_) return;
    ctrl_shadow_ = (follow ? CTRL_FOLLOW_EN : 0u) | (reflex ? CTRL_REFLEX_EN : 0u);
    wr(CTRL, ctrl_shadow_);
}

bool Medula::upload(const uint8_t* depth8) {
    if (!io_ || !depth8) return false;
    if (rd(STATUS) & ST_BUSY) return false;
    for (int w = 0; w < FRAME_WORDS; ++w) {
        const uint8_t* q = depth8 + 4 * w;
        wr(FRAME_WORD0 + static_cast<uint32_t>(w),
           static_cast<uint32_t>(q[0]) | (static_cast<uint32_t>(q[1]) << 8) |
           (static_cast<uint32_t>(q[2]) << 16) | (static_cast<uint32_t>(q[3]) << 24));
    }
    return true;
}

bool Medula::start(bool bg_reset) {
    if (!io_) return false;
    if (rd(STATUS) & ST_BUSY) return false;   // um GO ignorado deixaria wait_done devolver o DONE do quadro anterior
    wr(CTRL, ctrl_shadow_ | CTRL_GO | (bg_reset ? CTRL_BG_RESET : 0u));
    return true;
}

bool Medula::wait_done(uint32_t timeout_ms) {
    if (!io_) return false;
    const uint32_t t0 = hal::now_ms();
    uint32_t st = rd(STATUS);
    while (!(st & ST_DONE) && hal::now_ms() - t0 < timeout_ms) {
        hal::sleep_ms(1);
        st = rd(STATUS);
    }
    return (st & ST_DONE) != 0;
}

bool Medula::read_result(Result* r) {
    if (!io_ || !r) return false;
    const uint32_t st = rd(STATUS);
    const uint32_t xy = rd(TARGET_XY);
    const uint32_t dp = rd(TARGET_DEPTH);
    const uint32_t bb = rd(BBOX);
    const uint32_t lat = rd(LATENCY);
    const uint32_t ht = rd(HEAD_TARGET);
    const uint32_t rc = rd(REFLEX_COUNT);
    r->target = (st & ST_TARGET) != 0;
    r->cx = static_cast<uint8_t>(xy);
    r->cy = static_cast<uint8_t>(xy >> 8);
    r->count = static_cast<uint16_t>(xy >> 16);
    r->min_depth = static_cast<uint8_t>(dp);
    r->mean_depth = static_cast<uint8_t>(dp >> 8);
    r->x0 = static_cast<uint8_t>(bb);
    r->y0 = static_cast<uint8_t>(bb >> 8);
    r->x1 = static_cast<uint8_t>(bb >> 16);
    r->y1 = static_cast<uint8_t>(bb >> 24);
    r->latency_cycles = lat;
    r->pan_target_us = static_cast<uint16_t>(ht);
    r->tilt_target_us = static_cast<uint16_t>(ht >> 16);
    r->reflex = rc != last_reflex_count_;
    last_reflex_count_ = rc;
    return true;
}

uint8_t Medula::background(int p) const {
    if (!io_ || p < 0 || p >= N) return 0;
    const uint32_t v = rd(BG_WORD0 + static_cast<uint32_t>(p >> 2));
    return static_cast<uint8_t>(v >> (8 * (p & 3)));
}

bool Medula::wd_enable(uint16_t timeout_ms) {
    if (!io_) return false;
    wr(WD_TIMEOUT_MS, timeout_ms);
    wr(WD_CTRL, 1);
    return true;
}

void Medula::wd_kick() {
    if (io_) wr(WD_KICK, 1);
}

void Medula::wd_disable() {
    if (io_) wr(WD_CTRL, 0);
}

bool Medula::wd_tripped() const {
    return io_ && (rd(STATUS) & ST_WD_TRIPPED) != 0;
}

}}  // namespace aracno::medula
