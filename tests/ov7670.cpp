// cam::Ov7670 contra o modelo da FPGA da S09 (tests/fake_cam.h): present() não escreve nada com ID
// errado; capture() com timeout não lê o quadro; desempacota 4 pixels por palavra (little-endian);
// Y → RGB565 cinza ampliado 2×; init_qqvga_yuv() deixa os registradores de QQVGA/YUV/CLKRC no escravo.
#include <cstdio>
#include <cstring>
#include "fake_cam.h"
#include "cam/ov7670.h"
#include "hal/clock.h"

using namespace aracno;
using cam::Ov7670;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FALHA linha %d: %s\n", __LINE__, msg); ++fails; } } while (0)

static uint8_t y[Ov7670::FRAME_PIXELS];
static uint16_t rgb[320 * 240];

int main() {
    // ID errado (.rbf sem câmera): present() só lê, nada é escrito em nenhum endereço depois disso
    {
        FakeCamFpga f; Ov7670 c;
        f.id = 0x00000001u;   // pwm17 respondendo no lugar
        uint32_t id = 0;
        CHECK(!c.present(&f, &id), "present() falso com ID errado");
        CHECK(id == 0x00000001u, "ID lido devolvido");
        cam::CaptureInfo info;
        CHECK(!c.reset() && !c.identify() && !c.init_qqvga_yuv() && !c.arm() && !c.capture(y, 1, &info),
              "nada funciona sem present()");
        CHECK(c.frames() == 0, "frames() = 0 sem present()");
        CHECK(f.writes_total == 0, "nenhuma escrita com ID errado");
    }
    // ID certo: present() ainda não escreve (só lê ID e semeia a sombra do pio_cam)
    {
        FakeCamFpga f; Ov7670 c;
        uint32_t id = 0;
        CHECK(c.present(&f, &id) && id == Ov7670::ID_VALUE, "present() com CAM1");
        CHECK(f.writes_total == 0, "present() não escreve");
    }
    // timeout: CTRL = ARM, devolve false depois de ~timeout, sem ler o quadro
    {
        FakeCamFpga f; Ov7670 c;
        CHECK(c.present(&f), "present");
        f.status = Ov7670::ST_BUSY; f.lines = 7; f.frames = 3;
        std::memset(y, 0xEE, sizeof y);
        cam::CaptureInfo info;
        const uint64_t t0 = hal::now_us();
        CHECK(!c.capture(y, 20, &info), "timeout devolve false");
        const uint64_t dt = hal::now_us() - t0;
        CHECK(dt >= 19000 && dt < 500000, "espera ~timeout_ms e não trava");
        CHECK(f.ctrl_writes == 1 && f.ctrl_last == Ov7670::CTRL_ARM, "CTRL = ARM (fase 0)");
        CHECK(f.frame_reads == 0, "quadro não lido em timeout");
        CHECK(y[0] == 0xEE && y[Ov7670::FRAME_PIXELS - 1] == 0xEE, "buffer intacto");
        CHECK(info.status == Ov7670::ST_BUSY && info.lines == 7 && info.frames == 3 && !info.overflow,
              "info preenchida mesmo em timeout");
    }
    // quadro pronto: 4 pixels por palavra, byte p%4 da palavra 0x2000 + p/4 (little-endian)
    {
        FakeCamFpga f; Ov7670 c;
        CHECK(c.present(&f), "present");
        for (int w = 0; w < Ov7670::FRAME_WORDS; ++w) {
            uint32_t v = 0;
            for (int b = 0; b < 4; ++b) v |= static_cast<uint32_t>(((4 * w + b) * 7 + 3) & 0xFF) << (8 * b);
            f.frame[w] = v;
        }
        f.frame[0] = 0x44332211u;
        f.status = Ov7670::ST_DONE | Ov7670::ST_OVERFLOW; f.frames = 42; f.lines = 120; f.bpl = 320;
        c.set_phase(1);
        cam::CaptureInfo info;
        CHECK(c.capture(y, 100, &info), "captura com DONE");
        CHECK(f.ctrl_last == (Ov7670::CTRL_ARM | Ov7670::CTRL_PHASE), "CTRL = ARM | PHASE");
        CHECK(y[0] == 0x11 && y[1] == 0x22 && y[2] == 0x33 && y[3] == 0x44, "palavra 0 little-endian");
        int bad = 0;
        for (int p = 4; p < Ov7670::FRAME_PIXELS; ++p) if (y[p] != ((p * 7 + 3) & 0xFF)) ++bad;
        CHECK(bad == 0, "todos os pixels na ordem certa");
        CHECK(f.frame_reads == static_cast<uint32_t>(Ov7670::FRAME_WORDS), "4800 palavras lidas");
        CHECK(info.frames == 42 && info.lines == 120 && info.bytes_per_line == 320 && info.overflow, "info");
        CHECK(c.frames() == 42, "frames()");
    }
    // Y → RGB565 cinza
    {
        CHECK(cam::gray565(0) == 0x0000, "preto");
        CHECK(cam::gray565(255) == 0xFFFF, "branco");
        CHECK(cam::gray565(0x80) == 0x8410, "cinza médio: R=16 G=32 B=16");
        CHECK(cam::gray565(0x08) == 0x0841, "0x08: R=1 G=2 B=1");
        CHECK(cam::gray565(0x04) == 0x0020, "0x04: só G=1");
    }
    // ampliação 2×: pixel (x,y) → (2x..2x+1, 2y..2y+1)
    {
        for (int p = 0; p < Ov7670::FRAME_PIXELS; ++p) y[p] = static_cast<uint8_t>((p * 13 + p / 160) & 0xFF);
        std::memset(rgb, 0xAB, sizeof rgb);
        cam::y_to_rgb565_2x(y, rgb);
        int bad = 0;
        for (int yy = 0; yy < 240; ++yy)
            for (int xx = 0; xx < 320; ++xx)
                if (rgb[yy * 320 + xx] != cam::gray565(y[(yy / 2) * 160 + xx / 2])) ++bad;
        CHECK(bad == 0, "cada pixel de saída vem de (x/2, y/2)");
        y[119 * 160 + 159] = 0xFF; y[0] = 0x00; y[1] = 0x80;
        cam::y_to_rgb565_2x(y, rgb);
        CHECK(rgb[238 * 320 + 318] == 0xFFFF && rgb[239 * 320 + 319] == 0xFFFF, "canto inferior direito");
        CHECK(rgb[0] == 0 && rgb[1] == 0 && rgb[320] == 0 && rgb[321] == 0, "pixel (0,0) → 2×2");
        CHECK(rgb[2] == 0x8410 && rgb[3] == 0x8410 && rgb[322] == 0x8410 && rgb[323] == 0x8410, "pixel (1,0) → 2×2");
    }
    // init_qqvga_yuv: reset por hardware, COM7 reset, PID/VER, listas; registradores finais no escravo
    {
        FakeCamFpga f; Ov7670 c;
        f.regs[0x0A] = 0x76; f.regs[0x0B] = 0x73;
        f.regs[0x70] = 0x80 | 0x3A;   // padrão de teste ligado em XSC: bit7 tem que sobreviver
        f.regs[0x71] = 0x4A;
        CHECK(c.present(&f), "present");
        CHECK(c.init_qqvga_yuv(), "init completo");
        CHECK(c.pid() == 0x76 && c.ver() == 0x73, "PID/VER lidos");
        CHECK(f.nw > 100 && f.wlog[0].reg == 0x12 && f.wlog[0].val == 0x80, "primeira escrita: COM7 = reset");
        CHECK(f.wlog[f.nw - 1].reg == 0x11 && f.wlog[f.nw - 1].val == 0x01, "última escrita: CLKRC = 0x01 (÷2)");
        struct { uint8_t reg, val; const char* nome; } want[] = {
            {0x12, 0x00, "COM7 YUV"}, {0x40, 0xC0, "COM15 00..FF"}, {0x3A, 0x04, "TSLB"}, {0x15, 0x00, "COM10"},
            {0x0C, 0x04, "COM3 DCW"}, {0x3E, 0x1A, "COM14 PCLK÷4"}, {0x72, 0x22, "DCWCTR 1:4"},
            {0x73, 0xF2, "PCLK_DIV ÷4"}, {0x70, 0xA0, "XSC (bit7 preservado)"}, {0x71, 0x20, "YSC"},
            {0x17, 0x17, "HSTART"}, {0x18, 0x05, "HSTOP"}, {0x32, 0x92, "HREF"}, {0x19, 0x02, "VSTART"},
            {0x1A, 0x7A, "VSTOP"}, {0x03, 0x0F, "VREF"}, {0xA2, 0x02, "PCLK_DELAY"}, {0x11, 0x01, "CLKRC"},
            {0x13, 0xE5, "COM8 AGC/AEC"}, {0x10, 0x00, "AECH (COM2_SSLEEP da referência)"},
        };
        for (unsigned i = 0; i < sizeof want / sizeof want[0]; ++i) {
            if (f.regs[want[i].reg] != want[i].val) {
                std::printf("FALHA: reg 0x%02X (%s) = 0x%02X, esperado 0x%02X\n", want[i].reg, want[i].nome,
                            f.regs[want[i].reg], want[i].val);
                ++fails;
            }
        }
        CHECK(f.repeated_starts == 0 && f.glitches == 0, "barramento limpo");
        CHECK(f.out == (1u << qsys::PIO_CAM_BIT_RESET_N), "fim: RESET_N = 1, PWDN = 0, SIOC/SIOD soltos");
    }
    // PID errado: identify() falha e nenhuma lista é escrita
    {
        FakeCamFpga f; Ov7670 c;
        f.regs[0x0A] = 0x77; f.regs[0x0B] = 0x73;
        CHECK(c.present(&f), "present");
        CHECK(!c.init_qqvga_yuv(), "init falha com PID errado");
        CHECK(c.pid() == 0x77, "PID lido devolvido para o diagnóstico");
        CHECK(f.nw == 1, "só o COM7 reset foi escrito");
    }
    // câmera muda (sem ACK): identify() falha com NACK_ADDR
    {
        FakeCamFpga f; Ov7670 c;
        f.nack_addr = true;
        CHECK(c.present(&f), "present");
        CHECK(c.reset() && !c.identify(), "identify falha sem ACK");
        CHECK(c.sccb().last_error() == hal::Sccb::NACK_ADDR, "NACK_ADDR");
    }
    std::printf("ov7670: %d falhas\n", fails);
    return fails ? 1 : 0;
}
