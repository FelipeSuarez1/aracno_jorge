#include "app/bench.h"
#include <cstdio>
#include <cstring>
#include "aracno/qsys.h"
#include "hal/avalon.h"
#include "hal/clock.h"
#include "hal/lwbridge.h"
#include "cam/ov7670.h"
#include "face/face.h"
#include "face/ili9341.h"
#include "face/pio_ctrl.h"

namespace aracno { namespace app {

static bool open_fpga(hal::LwBridge& br, hal::AvalonSpi& spi, hal::AvalonPio& pio) {
    if (!br.open()) { std::fprintf(stderr, "ponte lightweight: /dev/mem falhou\n"); return false; }
    // Sem os blocos da S08 (.rbf de fábrica ou baseline), o interconector da ponte LW manda endereços sem
    // escravo para o pwm17 (DEFAULT_DESTID): ler 0x1A8 devolve o STATUS do pwm17 (só bit0 = done), sem
    // erro de barramento. Por isso a máscara TRDY|TMT (0x60) distingue os dois — NÃO afrouxar para `st != 0`.
    // E nenhuma ESCRITA nesta faixa antes do teste: num .rbf antigo ela iria para os registradores do servo 17.
    uint32_t st = br.read32(qsys::SPI_LCD_BASE, hal::AvalonSpi::STATUS);
    if (!(st & (hal::AvalonSpi::ST_TRDY | hal::AvalonSpi::ST_TMT))) {
        std::fprintf(stderr, "spi_lcd não responde (status=0x%x): o soc_system.rbf do cartão é o da S08?\n", st);
        return false;
    }
    spi.attach(&br, qsys::SPI_LCD_BASE);
    pio.attach(&br, qsys::PIO_LCD_BASE);         // sombra lida do hardware (reset = qsys::PIO_LCD_RESET_VALUE)
    std::printf("fpga: spi_lcd ok (status=0x%x), pio_lcd ok\n", st);
    return true;
}

int spi_loopback() {
    hal::LwBridge br; hal::AvalonSpi spi; hal::AvalonPio pio;
    if (!open_fpga(br, spi, pio)) return 1;
    uint8_t tx[256], rx[256];
    for (int i = 0; i < 256; ++i) tx[i] = static_cast<uint8_t>(i ^ 0x5A);
    std::memset(rx, 0, sizeof rx);
    uint64_t t0 = hal::now_us();
    if (!spi.xfer(tx, rx, sizeof tx)) { std::fprintf(stderr, "xfer falhou (timeouts=%u)\n", spi.timeouts()); return 1; }
    uint32_t us = static_cast<uint32_t>(hal::now_us() - t0);
    int bad = 0;
    for (int i = 0; i < 256; ++i) if (rx[i] != tx[i]) ++bad;
    std::printf("loopback: %d/256 bytes diferentes em %u us (rx[0..3] = %02x %02x %02x %02x)\n", bad, us, rx[0], rx[1], rx[2], rx[3]);
    if (bad == 0) std::printf("LOOPBACK OK: spi_lcd e JP1 pinos 2/6 confirmados\n");
    else if (rx[0] == 0xFF && rx[1] == 0xFF) std::printf("só 0xFF: MISO no pull-up — jumper 2-6 ausente?\n");
    return bad ? 2 : 0;
}

int face_demo(unsigned seconds_per_expr) {
    hal::LwBridge br; hal::AvalonSpi spi; hal::AvalonPio pio;
    if (!open_fpga(br, spi, pio)) return 1;
    face::PioCtrlPins pins(&pio);
    face::Ili9341 lcd;
    uint64_t t0 = hal::now_us();
    if (!lcd.init(&spi, &pins, hal::sleep_ms)) { std::fprintf(stderr, "init do ILI9341 falhou (timeouts=%u)\n", spi.timeouts()); return 1; }
    std::printf("ili9341: init + tela preta em %llu ms\n", static_cast<unsigned long long>((hal::now_us() - t0) / 1000u));

    static face::Face f;    // canvas de 150 kB: fora da pilha
    static const char* frases[face::N_EXPR] = {
        "Olá, eu sou a Jorge.", "Achei você!", "Tem luz demais aqui...", "Sai da minha toca!",
        "O que foi isso?", "Ninguém quer brincar comigo.", "Hmm... o que é aquilo?"};
    static const char* nomes[face::N_EXPR] = {"neutro", "alegria", "medo", "raiva", "surpresa", "tristeza", "curiosidade"};
    face::Rect full; full.x = 0; full.y = 0; full.w = face::CANVAS_W; full.h = face::CANVAS_H;

    for (int e = 0; e < face::N_EXPR; ++e) {
        f.set(static_cast<face::Expr>(e), 80);
        f.say(frases[e], 0);
        f.tick(hal::now_ms());
        t0 = hal::now_us();
        if (!lcd.flush(f.canvas(), full)) { std::fprintf(stderr, "[%s] flush do quadro inteiro falhou\n", nomes[e]); return 1; }
        f.flushed();
        uint32_t full_us = static_cast<uint32_t>(hal::now_us() - t0);
        std::printf("[%s] quadro inteiro %u ms (~%u kbit/s reais)\n", nomes[e], full_us / 1000u,
                    static_cast<unsigned>(153600ull * 8u * 1000u / (full_us ? full_us : 1)));

        uint32_t end = hal::now_ms() + seconds_per_expr * 1000u, worst = 0, parts = 0;
        while (hal::now_ms() < end) {
            f.tick(hal::now_ms());
            face::Rect r;
            if (f.dirty(&r)) {
                t0 = hal::now_us();
                if (!lcd.flush(f.canvas(), r)) { std::fprintf(stderr, "[%s] flush parcial falhou\n", nomes[e]); return 1; }
                f.flushed();
                uint32_t ms = static_cast<uint32_t>((hal::now_us() - t0) / 1000u);
                if (ms > worst) worst = ms;
                ++parts;
            }
            hal::sleep_ms(20);
        }
        if (parts) std::printf("[%s] %u atualizações parciais, pior %u ms\n", nomes[e], parts, worst);
    }
    std::printf("face-demo: fim (%u bytes enviados, %u timeouts de SPI)\n", lcd.bytes_sent(), spi.timeouts());
    return 0;
}

// Ponte aberta e ID do cam_capture conferido (só leitura). Sem isso, nenhuma escrita na faixa da câmera.
static bool open_cam(hal::LwBridge& br, cam::Ov7670& cam) {
    if (!br.is_open() && !br.open()) { std::fprintf(stderr, "ponte lightweight: /dev/mem falhou\n"); return false; }
    uint32_t id = 0;
    if (!cam.present(&br, &id)) {
        std::fprintf(stderr, "cam_capture não responde (ID=0x%08x, esperado 0x%08x): o soc_system.rbf do cartão é o da S09?\n",
                     id, cam::Ov7670::ID_VALUE);
        return false;
    }
    std::printf("fpga: cam_capture ok (ID=0x%08x), pio_cam=0x%x\n", id, cam.sccb().out_shadow());
    return true;
}

static void print_levels(cam::Ov7670& cam, const char* when) {
    const uint32_t v = cam.sccb().in_levels();
    std::printf("pio_cam_in %s: SIOD=%u SIOC=%u VSYNC=%u HREF=%u\n", when,
                (v >> qsys::PIO_CAM_IN_BIT_SIOD) & 1u, (v >> qsys::PIO_CAM_IN_BIT_SIOC) & 1u,
                (v >> qsys::PIO_CAM_IN_BIT_VSYNC) & 1u, (v >> qsys::PIO_CAM_IN_BIT_HREF) & 1u);
}

static void print_info(const char* tag, const cam::CaptureInfo& in) {
    std::printf("%s: STATUS=0x%x (done=%u busy=%u overflow=%u) FRAMES=%u LINES=%u BYTES_PER_LINE=%u\n", tag,
                in.status, in.status & cam::Ov7670::ST_DONE ? 1u : 0u, in.status & cam::Ov7670::ST_BUSY ? 1u : 0u,
                in.overflow ? 1u : 0u, in.frames, in.lines, in.bytes_per_line);
}

static void print_stats(const uint8_t* y) {
    unsigned mn = 255, mx = 0, sum = 0;
    for (int p = 0; p < cam::Ov7670::FRAME_PIXELS; ++p) {
        if (y[p] < mn) mn = y[p];
        if (y[p] > mx) mx = y[p];
        sum += y[p];
    }
    std::printf("quadro Y: min=%u média=%u max=%u\n", mn, sum / cam::Ov7670::FRAME_PIXELS, mx);
}

int cam_id() {
    hal::LwBridge br; cam::Ov7670 cam;
    if (!open_cam(br, cam)) return 1;
    print_levels(cam, "antes do reset");
    cam.reset();
    const bool ok = cam.identify();
    const hal::Sccb::Error e = cam.sccb().last_error();
    if (e != hal::Sccb::OK) {
        std::printf("SCCB: %s\n", hal::Sccb::error_name(e));
        print_levels(cam, "depois do erro");
        return 2;
    }
    std::printf("PID=0x%02x VER=0x%02x (esperado 0x76 0x73): %s\n", cam.pid(), cam.ver(), ok ? "CAM-ID OK" : "DIFERENTE");
    return ok ? 0 : 2;
}

int cam_raw() {
    hal::LwBridge br; cam::Ov7670 cam;
    if (!open_cam(br, cam)) return 1;
    print_levels(cam, "em reset");
    cam.reset();   // só solta RESET_N/PWDN: registradores nos padrões de fábrica (VGA YUV)
    print_levels(cam, "solta");
    const uint32_t f0 = cam.frames();
    uint32_t prev = f0;
    for (int i = 1; i <= 8; ++i) {
        hal::sleep_ms(250);
        const uint32_t f = cam.frames();
        std::printf("t=%4d ms FRAMES=%u (+%u)\n", i * 250, f, f - prev);
        prev = f;
    }
    const uint32_t vs = prev - f0;
    std::printf("VSYNC: %u bordas em 2 s (~%u.%u Hz)\n", vs, vs / 2, (vs % 2) * 5);
    if (vs == 0) std::printf("FRAMES parado: sem XCLK na câmera, sem alimentação ou VSYNC fora do pino?\n");
    cam::CaptureInfo info;
    static uint8_t y[cam::Ov7670::FRAME_PIXELS];
    const bool done = cam.capture(y, 1000, &info);
    print_info(done ? "captura" : "captura (timeout 1 s)", info);
    // [INFERÊNCIA] padrão de fábrica = VGA YUV: ~480 linhas de 1280 bytes → OVERFLOW esperado aqui.
    std::printf("(padrões de fábrica: VGA YUV, esperado ~480 linhas de 1280 bytes e OVERFLOW)\n");
    if (done) print_stats(y);
    return (vs > 0 && done) ? 0 : 2;
}

int cam_demo(unsigned seconds, int phase) {
    hal::LwBridge br; hal::AvalonSpi spi; hal::AvalonPio pio; cam::Ov7670 cam;
    if (!open_fpga(br, spi, pio)) return 1;
    if (!open_cam(br, cam)) return 1;
    face::PioCtrlPins pins(&pio);
    face::Ili9341 lcd;
    if (!lcd.init(&spi, &pins, hal::sleep_ms)) { std::fprintf(stderr, "init do ILI9341 falhou (timeouts=%u)\n", spi.timeouts()); return 1; }

    cam.set_phase(phase);
    uint64_t t0 = hal::now_us();
    if (!cam.init_qqvga_yuv()) {
        std::fprintf(stderr, "init da câmera falhou: PID=0x%02x VER=0x%02x, SCCB: %s\n", cam.pid(), cam.ver(),
                     hal::Sccb::error_name(cam.sccb().last_error()));
        return 2;
    }
    std::printf("ov7670: QQVGA YUV, CLKRC ÷2, fase %d, init em %llu ms\n", cam.phase(),
                static_cast<unsigned long long>((hal::now_us() - t0) / 1000u));

    static uint8_t y[cam::Ov7670::FRAME_PIXELS];
    static uint16_t rgb[face::CANVAS_W * face::CANVAS_H];
    static face::Canvas canvas;   // 150 kB: fora da pilha
    face::Sprite spr; spr.x = 0; spr.y = 0; spr.w = face::CANVAS_W; spr.h = face::CANVAS_H; spr.px = rgb;
    face::Rect full; full.x = 0; full.y = 0; full.w = face::CANVAS_W; full.h = face::CANVAS_H;

    // Laço em pipeline: rearma logo depois de ler o quadro, e a FPGA grava o próximo enquanto o ARM
    // converte e envia este ao display.
    unsigned shown = 0, timeouts = 0, overflows = 0;
    const uint32_t start = hal::now_ms(), end = start + seconds * 1000u;
    cam.arm();
    while (hal::now_ms() < end) {
        cam::CaptureInfo info;
        if (!cam.collect(y, 1000, &info)) {
            print_info("timeout de 1 s", info);
            if (++timeouts >= 3) { std::fprintf(stderr, "cam-demo: 3 timeouts seguidos, desistindo\n"); return 2; }
            cam.arm();
            continue;
        }
        timeouts = 0;
        if (info.overflow) ++overflows;
        if (shown == 0) { print_info("1º quadro", info); print_stats(y); }
        cam.arm();
        cam::y_to_rgb565_2x(y, rgb);
        canvas.blit(spr, 0, 0);
        if (!lcd.flush(canvas, full)) { std::fprintf(stderr, "flush do quadro falhou (timeouts=%u)\n", spi.timeouts()); return 1; }
        ++shown;
    }
    const uint32_t ms = hal::now_ms() - start;
    std::printf("cam-demo: %u quadros em %u ms = %u.%u quadros/s, %u com OVERFLOW, FRAMES=%u\n", shown, ms,
                ms ? shown * 1000u / ms : 0u, ms ? (shown * 10000u / ms) % 10u : 0u, overflows, cam.frames());
    return 0;
}

}}  // namespace aracno::app
