#pragma once
#include <stdint.h>
#include "hal/lwbridge.h"
#include "hal/sccb.h"

namespace aracno { namespace cam {

namespace regs { struct RegVal; }

// Resultado/diagnóstico de uma captura (registradores do cam_capture, S09 "Contrato").
struct CaptureInfo {
    uint32_t status;          // STATUS no fim da espera
    uint32_t frames;          // FRAMES: bordas de subida de VSYNC desde o reset da FPGA
    uint32_t lines;           // LINES: linhas com HREF no último quadro
    uint32_t bytes_per_line;  // BYTES_PER_LINE: bytes na última linha (antes de descartar a fase)
    bool overflow;            // STATUS.OVERFLOW: linha > 320 bytes ou > 120 linhas (truncado)
};

// OV7670 sem FIFO ligada ao cam_capture da FPGA (S09): o ARM configura pela SCCB (pio_cam) e lê o
// quadro Y 160×120 que a FPGA grava em M10K. Bases fixas em aracno/qsys.h.
class Ov7670 {
public:
    static const int W = 160, H = 120;
    static const int FRAME_PIXELS = W * H;
    static const int FRAME_WORDS = FRAME_PIXELS / 4;   // 4 pixels por palavra, little-endian

    // Registradores do cam_capture (índice de palavra = offset / 4).
    enum { CTRL = 0, STATUS = 1, FRAMES = 2, LINES = 3, BYTES_PER_LINE = 4, ID = 5, FRAME_WORD0 = 0x2000 };
    static const uint32_t CTRL_ARM = 1u << 0, CTRL_PHASE = 1u << 1;
    static const uint32_t ST_DONE = 1u << 0, ST_BUSY = 1u << 1, ST_OVERFLOW = 1u << 2;
    static const uint32_t ID_VALUE = 0x43414D31u;      // "CAM1"

    Ov7670() : io_(0), phase_(0), pid_(0), ver_(0) {}

    // Lê SÓ o ID do cam_capture. Se bater, liga o driver ao `io` (a SCCB semeia a sombra lendo o pio_cam)
    // e devolve true. Se não bater (.rbf sem a câmera: o endereço cai no pwm17), não escreve nada e
    // devolve false. `id` (opcional) recebe o valor lido.
    bool present(hal::RegIo* io, uint32_t* id = 0);

    // Solta a câmera: PWDN = 0, RESET_N = 0 por 1 ms, RESET_N = 1, espera 10 ms. Exige present().
    bool reset();
    // Software reset (COM7 = 0x80) + 10 ms, lê PID/VER. true só se PID = 0x76 e VER = 0x73.
    bool identify();
    uint8_t pid() const { return pid_; }
    uint8_t ver() const { return ver_; }
    // reset() + identify() + listas de cam/ov7670_regs.h (YUV, INIT, QQVGA, CLKRC).
    bool init_qqvga_yuv();

    // PHASE do CTRL: 0 = guarda os bytes 0,2,4… da linha (Y em YUYV); 1 = 1,3,5…
    void set_phase(int phase) { phase_ = phase ? 1 : 0; }
    int phase() const { return phase_; }

    // Arma a captura (CTRL = ARM | PHASE). Exige present().
    bool arm();
    // Espera STATUS.DONE por até timeout_ms (dormindo 1 ms entre leituras), preenche `info` e, só se
    // DONE, lê as 4800 palavras do quadro para y160x120. false em timeout (quadro não é lido).
    bool collect(uint8_t* y160x120, uint32_t timeout_ms, CaptureInfo* info);
    // arm() + collect().
    bool capture(uint8_t* y160x120, uint32_t timeout_ms, CaptureInfo* info);

    uint32_t frames() const;   // FRAMES (0 sem present())
    hal::Sccb& sccb() { return sccb_; }

private:
    bool write_list(const regs::RegVal* list, unsigned n);
    hal::RegIo* io_;
    hal::Sccb sccb_;
    int phase_;
    uint8_t pid_, ver_;
};

// Y → cinza RGB565 (R5 = G6/2 = B5 = Y >> 3, G6 = Y >> 2).
inline uint16_t gray565(uint8_t y) {
    return static_cast<uint16_t>(((y >> 3) << 11) | ((y >> 2) << 5) | (y >> 3));
}
// Quadro Y 160×120 → RGB565 320×240 ampliado 2× (pixel (x,y) → (2x..2x+1, 2y..2y+1)).
void y_to_rgb565_2x(const uint8_t* y160x120, uint16_t* out320x240);

}}  // namespace aracno::cam
