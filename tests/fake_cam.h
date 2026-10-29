// Modelo de host da FPGA da S09 para tests/sccb.cpp e tests/ov7670.cpp:
//   pio_cam (saída) + pio_cam_in (entrada) com barramento open-drain (linha = AND de mestre, escravo e
//   pull-up) e um escravo SCCB da OV7670 que decodifica START/STOP/bits na borda do SIOC, dá ACK, grava
//   e devolve registradores; cam_capture com ID, STATUS, contadores e RAM do quadro.
// O escravo trata repeated start como a OV7670 (que não o aceita): ignora a transação → NACK.
// Rastro legível: "S 42+ 12+ 80+ P " (byte e '+' ACK / '-' NACK; "Sr" = repeated start).
#pragma once
#include <cstdio>
#include <cstring>
#include "aracno/qsys.h"
#include "hal/lwbridge.h"

struct FakeCamState {
    // pio_cam / barramento
    uint32_t out;
    bool scl, sda_m, slave_low, pullup;
    // escravo SCCB
    uint8_t addr7, regs[256], ptr, shift, tx;
    int st, bit, byte_idx;
    bool reading, last_ack, nack_addr, nack_data;
    unsigned starts, stops, repeated_starts, glitches;
    char trace[8192]; size_t tlen;
    struct W { uint8_t reg, val; } wlog[1024]; unsigned nw;
    // RESET_N/PWDN: cada escrita no pio_cam confere os bits 2..3 contra `ctrl_expect` (se check_ctrl)
    bool check_ctrl; uint32_t ctrl_expect; unsigned ctrl_violations;
    // cam_capture
    uint32_t id, status, frames, lines, bpl, ctrl_last, ctrl_writes, frame[4800];
    mutable uint32_t frame_reads;
    // contadores gerais
    unsigned writes_total;
    unsigned stray_writes;
};

struct FakeCamFpga : public aracno::hal::RegIo, public FakeCamState {
    enum St { IDLE, RX, ACK_OUT, TX, ACK_IN, IGNORE };

    FakeCamFpga() : FakeCamState() { scl = sda_m = pullup = true; addr7 = 0x21; id = 0x43414D31u; }
    virtual ~FakeCamFpga() {}

    void log(const char* s) {
        size_t n = std::strlen(s);
        if (tlen + n < sizeof trace) { std::memcpy(trace + tlen, s, n + 1); tlen += n; }
    }
    void clear_trace() { tlen = 0; trace[0] = 0; }
    bool line_sda() const { return pullup && sda_m && !slave_low; }

    void on_start() {
        ++starts;
        if (st != IDLE) { ++repeated_starts; log("Sr "); st = IGNORE; slave_low = false; return; }
        log("S ");
        st = RX; bit = 0; shift = 0; byte_idx = 0; reading = false;
    }
    void on_stop() { ++stops; log("P "); st = IDLE; slave_low = false; }
    void on_rise() {
        if (st == RX) { shift = static_cast<uint8_t>((shift << 1) | (line_sda() ? 1 : 0)); ++bit; }
        else if (st == ACK_IN) last_ack = !line_sda();
    }
    void drive_tx_bit() { slave_low = !((tx >> (7 - bit)) & 1); }
    void on_fall() {
        char b[16];
        switch (st) {
        case RX:
            if (bit < 8) break;
            last_ack = true;
            if (byte_idx == 0) {
                if ((shift >> 1) != addr7 || nack_addr) last_ack = false;
                reading = shift & 1;
            } else if (byte_idx == 1 && !reading) {
                ptr = shift;
            } else if (nack_data) {
                last_ack = false;
            } else {
                regs[ptr] = shift;
                if (nw < 1024) { wlog[nw].reg = ptr; wlog[nw].val = shift; ++nw; }
                ++ptr;
            }
            std::snprintf(b, sizeof b, "%02X%c ", shift, last_ack ? '+' : '-');
            log(b);
            ++byte_idx;
            st = ACK_OUT; slave_low = last_ack;
            break;
        case ACK_OUT:
            slave_low = false;
            if (!last_ack) { st = IGNORE; break; }
            if (reading) { st = TX; bit = 0; tx = regs[ptr]; drive_tx_bit(); }
            else { st = RX; bit = 0; shift = 0; }
            break;
        case TX:
            if (++bit == 8) { slave_low = false; st = ACK_IN; } else drive_tx_bit();
            break;
        case ACK_IN:
            std::snprintf(b, sizeof b, "%02X%c ", tx, last_ack ? '+' : '-');
            log(b);
            if (last_ack) { ++ptr; st = TX; bit = 0; tx = regs[ptr]; drive_tx_bit(); } else st = IGNORE;
            break;
        default: break;
        }
    }

    void pio_write(uint32_t v) {
        using namespace aracno::qsys;
        if (check_ctrl && ((v >> PIO_CAM_BIT_RESET_N) & 3u) != ctrl_expect) ++ctrl_violations;
        const bool nscl = !((v >> PIO_CAM_BIT_SIOC_LOW) & 1u);
        const bool nsda = !((v >> PIO_CAM_BIT_SIOD_LOW) & 1u);
        out = v;
        if (nscl != scl && nsda != sda_m) ++glitches;
        if (scl && nscl && nsda != sda_m) {
            const bool before = line_sda();
            sda_m = nsda;
            const bool after = line_sda();
            if (before && !after) on_start();
            else if (!before && after) on_stop();
        }
        sda_m = nsda;
        if (!scl && nscl) { scl = true; on_rise(); }
        else if (scl && !nscl) { scl = false; on_fall(); }
    }

    virtual void write32(uint32_t base, uint32_t idx, uint32_t v) {
        ++writes_total;
        if (base == aracno::qsys::PIO_CAM_BASE && idx == 0) pio_write(v);
        else if (base == aracno::qsys::CAM_CAPTURE_BASE && idx == 0) { ctrl_last = v; ++ctrl_writes; }
        else ++stray_writes;   // qualquer outro endereço (PWM, display...) — a câmera nunca deve escrever lá
    }
    virtual uint32_t read32(uint32_t base, uint32_t idx) const {
        using namespace aracno::qsys;
        if (base == PIO_CAM_BASE) return out;
        if (base == PIO_CAM_IN_BASE)
            return (line_sda() ? 1u << PIO_CAM_IN_BIT_SIOD : 0u) | ((scl && pullup) ? 1u << PIO_CAM_IN_BIT_SIOC : 0u);
        if (base == CAM_CAPTURE_BASE) {
            switch (idx) {
            case 1: return status;
            case 2: return frames;
            case 3: return lines;
            case 4: return bpl;
            case 5: return id;
            default: break;
            }
            if (idx >= 0x2000 && idx < 0x2000 + 4800) {
                ++frame_reads;
                return frame[idx - 0x2000];
            }
        }
        return 0;
    }
};
