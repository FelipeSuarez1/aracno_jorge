#include "cam/ov7670.h"
#include "cam/ov7670_regs.h"
#include "aracno/qsys.h"
#include "hal/clock.h"

namespace aracno { namespace cam {

bool Ov7670::present(hal::RegIo* io, uint32_t* id) {
    io_ = 0;
    if (!io) return false;
    const uint32_t v = io->read32(qsys::CAM_CAPTURE_BASE, ID);
    if (id) *id = v;
    if (v != ID_VALUE) return false;
    io_ = io;
    sccb_.attach(io, qsys::PIO_CAM_BASE, qsys::PIO_CAM_IN_BASE);
    return true;
}

// Sequência da referência ([PICO] ov7670_reset/ov7670_shutdown; [ADA] __init__): reset 1 ms em 0.
bool Ov7670::reset() {
    if (!io_) return false;
    sccb_.set_pwdn(false);
    sccb_.set_reset_n(false);
    hal::sleep_ms(1);
    sccb_.set_reset_n(true);
    hal::sleep_ms(10);
    return true;
}

bool Ov7670::identify() {
    pid_ = ver_ = 0;
    if (!io_) return false;
    if (!sccb_.write_reg(regs::COM7, regs::COM7_RESET)) return false;
    hal::sleep_ms(10);
    if (!sccb_.read_reg(regs::PID, &pid_) || !sccb_.read_reg(regs::VER, &ver_)) return false;
    return pid_ == regs::PID_VALUE && ver_ == regs::VER_VALUE;
}

// Como _write_list das referências: um registrador por vez, 1 ms entre escritas.
bool Ov7670::write_list(const regs::RegVal* list, unsigned n) {
    for (unsigned i = 0; i < n; ++i) {
        if (!sccb_.write_reg(list[i].reg, list[i].val)) return false;
        hal::sleep_ms(1);
    }
    return true;
}

bool Ov7670::init_qqvga_yuv() {
    if (!reset() || !identify()) return false;
    if (!write_list(regs::YUV, sizeof regs::YUV / sizeof regs::YUV[0])) return false;
    if (!write_list(regs::INIT, sizeof regs::INIT / sizeof regs::INIT[0])) return false;
    if (!write_list(regs::QQVGA_SCALE, sizeof regs::QQVGA_SCALE / sizeof regs::QQVGA_SCALE[0])) return false;
    // frame_control: preserva o bit7 (padrão de teste) de SCALING_XSC/YSC.
    uint8_t xsc = 0, ysc = 0;
    if (!sccb_.read_reg(regs::SCALING_XSC, &xsc) || !sccb_.read_reg(regs::SCALING_YSC, &ysc)) return false;
    if (!sccb_.write_reg(regs::SCALING_XSC, static_cast<uint8_t>((xsc & 0x80) | regs::QQVGA_XYSC))) return false;
    if (!sccb_.write_reg(regs::SCALING_YSC, static_cast<uint8_t>((ysc & 0x80) | regs::QQVGA_XYSC))) return false;
    if (!write_list(regs::QQVGA_WINDOW, sizeof regs::QQVGA_WINDOW / sizeof regs::QQVGA_WINDOW[0])) return false;
    return sccb_.write_reg(regs::CLKRC, regs::CLKRC_VALUE);
}

bool Ov7670::arm() {
    if (!io_) return false;
    io_->write32(qsys::CAM_CAPTURE_BASE, CTRL, CTRL_ARM | (phase_ ? CTRL_PHASE : 0u));
    return true;
}

bool Ov7670::collect(uint8_t* y, uint32_t timeout_ms, CaptureInfo* info) {
    if (!io_) return false;
    const uint32_t t0 = hal::now_ms();
    uint32_t st = io_->read32(qsys::CAM_CAPTURE_BASE, STATUS);
    while (!(st & ST_DONE) && hal::now_ms() - t0 < timeout_ms) {
        hal::sleep_ms(1);
        st = io_->read32(qsys::CAM_CAPTURE_BASE, STATUS);
    }
    if (info) {
        info->status = st;
        info->frames = io_->read32(qsys::CAM_CAPTURE_BASE, FRAMES);
        info->lines = io_->read32(qsys::CAM_CAPTURE_BASE, LINES);
        info->bytes_per_line = io_->read32(qsys::CAM_CAPTURE_BASE, BYTES_PER_LINE);
        info->overflow = (st & ST_OVERFLOW) != 0;
    }
    if (!(st & ST_DONE)) return false;
    if (y) {
        for (int w = 0; w < FRAME_WORDS; ++w) {
            const uint32_t v = io_->read32(qsys::CAM_CAPTURE_BASE, FRAME_WORD0 + w);
            y[4 * w + 0] = static_cast<uint8_t>(v);
            y[4 * w + 1] = static_cast<uint8_t>(v >> 8);
            y[4 * w + 2] = static_cast<uint8_t>(v >> 16);
            y[4 * w + 3] = static_cast<uint8_t>(v >> 24);
        }
    }
    return true;
}

bool Ov7670::capture(uint8_t* y, uint32_t timeout_ms, CaptureInfo* info) {
    return arm() && collect(y, timeout_ms, info);
}

uint32_t Ov7670::frames() const {
    return io_ ? io_->read32(qsys::CAM_CAPTURE_BASE, FRAMES) : 0;
}

void y_to_rgb565_2x(const uint8_t* y, uint16_t* out) {
    const int OW = 2 * Ov7670::W;
    for (int r = 0; r < Ov7670::H; ++r) {
        uint16_t* d0 = out + (2 * r) * OW;
        uint16_t* d1 = d0 + OW;
        const uint8_t* s = y + r * Ov7670::W;
        for (int c = 0; c < Ov7670::W; ++c) {
            const uint16_t g = gray565(s[c]);
            d0[2 * c] = d0[2 * c + 1] = d1[2 * c] = d1[2 * c + 1] = g;
        }
    }
}

}}  // namespace aracno::cam
