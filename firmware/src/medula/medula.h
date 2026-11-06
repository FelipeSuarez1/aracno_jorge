#pragma once
#include <stdint.h>
#include "hal/lwbridge.h"
#include "medula/model.h"

namespace aracno { namespace medula {

// Driver do bloco `medula` da FPGA (S11). Contrato de registradores em docs/20-Specs/S11-medula-fpga.md.
// Base fixa em aracno/qsys.h (MEDULA_BASE). Sem attach() bem-sucedido, tudo é no-op / false.
class Medula {
public:
    // Registradores (índice de palavra = offset / 4).
    enum Reg {
        CTRL = 0, STATUS, FRAMES, TARGET_XY, TARGET_DEPTH, ID, BBOX, LATENCY, THRESH, REFLEX_CFG,
        PAN_GAIN, TILT_GAIN, SLEW_US, HEAD_US, WD_CTRL, WD_KICK, WD_TIMEOUT_MS, REFLEX_COUNT, HEAD_TARGET
    };
    static const uint32_t FRAME_WORD0 = 0x2000;
    static const uint32_t BG_WORD0 = 0x4000;
    static const int FRAME_WORDS = N / 4;                    // 4 pixels por palavra, little-endian
    static const uint32_t ID_VALUE = 0x4D454431u;            // "MED1"
    static const uint32_t CTRL_GO = 1u << 0, CTRL_BG_RESET = 1u << 1, CTRL_FOLLOW_EN = 1u << 2, CTRL_REFLEX_EN = 1u << 3;
    static const uint32_t ST_DONE = 1u << 0, ST_BUSY = 1u << 1, ST_TARGET = 1u << 2, ST_REFLEX = 1u << 3,
                          ST_WD_TRIPPED = 1u << 4, ST_HEAD_OVERRIDE = 1u << 5, ST_WRITE_DROPPED = 1u << 6;

    Medula() : io_(0), ctrl_shadow_(0), last_reflex_count_(0), frames_at_attach_(0) {}

    // Lê SÓ o ID. Se bater, liga o driver ao `io`, guarda REFLEX_COUNT e FRAMES atuais e devolve true.
    // Se não bater (.rbf antigo: o endereço cai no pwm17), não escreve nada e devolve false.
    bool attach(hal::RegIo* io);
    bool present() const { return io_ != 0; }
    uint32_t frames() const;                                 // FRAMES (0 sem attach())
    uint32_t frames_at_attach() const { return frames_at_attach_; }

    // THRESH, REFLEX_CFG, PAN_GAIN, TILT_GAIN, SLEW_US. Ignora p.reflex_en (ver set_modes()).
    bool configure(const Params& p);
    // CTRL bits 2/3 (FOLLOW_EN, REFLEX_EN): sombra mantida pelo driver, escrita imediata.
    void set_modes(bool follow, bool reflex);

    // 4800 palavras no FRAME. false se ausente ou BUSY.
    bool upload(const uint8_t* depth8 /*N*/);
    // GO (e BG_RESET) junto com a sombra de modos.
    bool start(bool bg_reset);
    // Espera STATUS.DONE por até timeout_ms (1 ms entre leituras).
    bool wait_done(uint32_t timeout_ms);
    // Desempacota TARGET_XY, TARGET_DEPTH, BBOX, LATENCY e HEAD_TARGET; target = STATUS.TARGET;
    // reflex = REFLEX_COUNT mudou desde a leitura anterior.
    bool read_result(Result* r);
    uint8_t background(int p) const;                         // BG[p] (0 sem attach() ou se BUSY)

    // Watchdog de servos. Tudo false/no-op sem attach().
    bool wd_enable(uint16_t timeout_ms);                     // WD_TIMEOUT_MS e depois WD_CTRL = 1
    void wd_kick();
    void wd_disable();                                       // WD_CTRL = 0 (zera TRIPPED)
    bool wd_tripped() const;

private:
    uint32_t rd(uint32_t idx) const;
    void wr(uint32_t idx, uint32_t v);
    hal::RegIo* io_;
    uint32_t ctrl_shadow_;
    uint32_t last_reflex_count_;
    uint32_t frames_at_attach_;
};

}}  // namespace aracno::medula
