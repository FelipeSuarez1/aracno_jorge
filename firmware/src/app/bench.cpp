#include "app/bench.h"
#include <cstdio>
#include <cstring>
#include "aracno/qsys.h"
#include "hal/avalon.h"
#include "hal/clock.h"
#include "hal/lwbridge.h"
#include "cam/ov7670.h"
#include "medula/medula.h"
#include "medula/scenes.h"
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
    std::printf("ov7670: QQVGA YUV, CLKRC ÷1, fase %d, init em %llu ms\n", cam.phase(),
                static_cast<unsigned long long>((hal::now_us() - t0) / 1000u));

    static uint8_t y[cam::Ov7670::FRAME_PIXELS];
    static uint16_t rgb[face::CANVAS_W * face::CANVAS_H];
    static face::Canvas canvas;   // 150 kB: fora da pilha
    face::Sprite spr; spr.x = 0; spr.y = 0; spr.w = face::CANVAS_W; spr.h = face::CANVAS_H; spr.px = rgb;
    face::Rect full; full.x = 0; full.y = 0; full.w = face::CANVAS_W; full.h = face::CANVAS_H;

    // Laço em pipeline: rearma logo depois de ler o quadro. O ARM só vale a partir da próxima borda de
    // VSYNC, então a FPGA grava o quadro SEGUINTE ao que está em curso (um em cada dois quadros da câmera)
    // enquanto o ARM converte e envia este ao display.
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

// ---- S11: bloco medula na FPGA ----------------------------------------------------------------------

// Abre a ponte e confere o ID do medula (só leitura). Sem isso, nenhuma escrita na faixa 0x20000
// (num .rbf antigo ela cairia no pwm17). 0 ok, 1 ponte, 3 medula ausente.
static int open_medula(hal::LwBridge& br, medula::Medula& med) {
    if (!br.open()) { std::fprintf(stderr, "ponte lightweight: /dev/mem falhou\n"); return 1; }
    if (!med.attach(&br)) {
        const uint32_t id = br.read32(qsys::MEDULA_BASE, medula::Medula::ID);
        std::fprintf(stderr, "medula não responde (ID=0x%08x, esperado 0x%08x): o soc_system.rbf do cartão é o da S11?\n",
                     id, medula::Medula::ID_VALUE);
        return 3;
    }
    return 0;
}

int medula_id() {
    hal::LwBridge br; medula::Medula med;
    const int rc = open_medula(br, med);
    if (rc) return rc;
    using medula::Medula;
    std::printf("medula: ID=0x%08x FRAMES=%u STATUS=0x%08x\n", br.read32(qsys::MEDULA_BASE, Medula::ID),
                br.read32(qsys::MEDULA_BASE, Medula::FRAMES), br.read32(qsys::MEDULA_BASE, Medula::STATUS));
    const uint32_t head = br.read32(qsys::MEDULA_BASE, Medula::HEAD_US);
    const uint32_t tgt = br.read32(qsys::MEDULA_BASE, Medula::HEAD_TARGET);
    std::printf("medula: HEAD_US pan=%u tilt=%u | HEAD_TARGET pan=%u tilt=%u | WD_CTRL=0x%x REFLEX_COUNT=%u\n",
                head & 0xFFFFu, head >> 16, tgt & 0xFFFFu, tgt >> 16, br.read32(qsys::MEDULA_BASE, Medula::WD_CTRL),
                br.read32(qsys::MEDULA_BASE, Medula::REFLEX_COUNT));
    return 0;
}

// Lista de campos divergentes de um quadro: "nome[hw/sw],…".
struct FieldDiff {
    char buf[192];
    size_t n;
    int count;
    FieldDiff() : n(0), count(0) { buf[0] = 0; }
    void check(const char* name, unsigned hw, unsigned sw) {
        if (hw == sw) return;
        const int w = std::snprintf(buf + n, sizeof buf - n, "%s%s[%u/%u]", count ? "," : "", name, hw, sw);
        if (w > 0 && n + static_cast<size_t>(w) < sizeof buf) n += static_cast<size_t>(w);
        ++count;
    }
};

int medula_test() {
    hal::LwBridge br; medula::Medula med;
    const int rc = open_medula(br, med);
    if (rc) return rc;
    std::printf("AVISO: SMK desligada (cabeça pode receber pulsos de 1 ms)\n");

    // Sequência F0..F19 da S11 (tb_medula.v): cena e BG_RESET de cada quadro.
    struct Step { int scene; bool bg_reset; };
    static const Step SEQ[20] = {
        {0, true},  {1, false}, {2, false}, {3, false}, {4, false},                                   // F0..F4
        {3, false}, {3, false}, {3, false}, {3, false}, {3, false}, {3, false}, {3, false},           // F5..F11
        {3, false}, {3, false}, {3, false}, {3, false},                                               // F12..F15
        {3, false},                                                                                   // F16
        {5, true},  {6, false}, {7, false}                                                            // F17..F19
    };
    const int NFRAMES = static_cast<int>(sizeof SEQ / sizeof SEQ[0]);

    medula::Params p = medula::default_params();
    p.hold_ms = 1;   // reflexo curto: a cabeça não fica presa no tilt máximo durante o teste
    med.configure(p);
    med.set_modes(false, true);
    static medula::Model model;
    static uint8_t scene[medula::N];
    model.set_frames(med.frames());

    int divergences = 0;
    bool have_prev = false;
    unsigned prev_pan = 0, prev_tilt = 0;   // HEAD_TARGET lido no quadro anterior
    for (int f = 0; f < NFRAMES; ++f) {
        medula::make_scene(SEQ[f].scene, scene);

        const uint64_t t0 = hal::now_us();
        medula::Result hw; std::memset(&hw, 0, sizeof hw);
        const bool io_ok = med.upload(scene) && med.start(SEQ[f].bg_reset) && med.wait_done(50) && med.read_result(&hw);
        const uint64_t t1 = hal::now_us();

        medula::Result sw; std::memset(&sw, 0, sizeof sw);
        const uint64_t t2 = hal::now_us();
        model.process(scene, SEQ[f].bg_reset, p, &sw);
        const uint64_t t3 = hal::now_us();

        FieldDiff d;
        if (!io_ok) {
            d.check("io", 0, 1);   // upload/GO/DONE/leitura falhou (BUSY, timeout de 50 ms)
        } else {
            d.check("target", hw.target, sw.target);
            d.check("count", hw.count, sw.count);
            d.check("cx", hw.cx, sw.cx);
            d.check("cy", hw.cy, sw.cy);
            d.check("min_depth", hw.min_depth, sw.min_depth);
            d.check("mean_depth", hw.mean_depth, sw.mean_depth);
            d.check("x0", hw.x0, sw.x0);
            d.check("y0", hw.y0, sw.y0);
            d.check("x1", hw.x1, sw.x1);
            d.check("y1", hw.y1, sw.y1);
            d.check("reflex", hw.reflex, sw.reflex);
            // O hardware guarda pan_t/tilt_t entre execuções e o modelo começa em 1500/1500: o alvo da
            // cabeça só se compara com alvo nos dois. Sem alvo, o hardware tem de manter o valor da leitura
            // anterior (no F0 da execução só registra). Regra válida para esta sequência: um quadro sem alvo
            // logo depois de um reflexo mudaria o tilt efetivo (1778 → tilt_t) e daria falso DIVERGE.
            if (hw.target && sw.target) {
                d.check("pan_target_us", hw.pan_target_us, sw.pan_target_us);
                d.check("tilt_target_us", hw.tilt_target_us, sw.tilt_target_us);
            } else if (!hw.target && have_prev) {
                d.check("pan_mantido", hw.pan_target_us, prev_pan);
                d.check("tilt_mantido", hw.tilt_target_us, prev_tilt);
            }
            prev_pan = hw.pan_target_us; prev_tilt = hw.tilt_target_us; have_prev = true;
        }
        std::printf("F%02d hw %u us (LATENCY/50) | ponta a ponta %u us | sw %u us | %s%s\n", f,
                    hw.latency_cycles / 50u, static_cast<unsigned>(t1 - t0), static_cast<unsigned>(t3 - t2),
                    d.count ? "DIVERGE campo=" : "ok", d.count ? d.buf : "");
        if (d.count) ++divergences;
    }
    std::printf("medula-test: %d divergências\n", divergences);
    return divergences == 0 ? 0 : 3;
}

int medula_wd() {
    hal::LwBridge br; medula::Medula med;
    const int rc = open_medula(br, med);
    if (rc) return rc;

    med.wd_disable();   // zera um TRIPPED de execução anterior
    med.wd_enable(100);
    std::printf("medula-wd: timeout 100 ms, kicks a cada 20 ms por 2 s\n");
    const uint32_t end = hal::now_ms() + 2000u;
    while (hal::now_ms() < end) {
        med.wd_kick();
        if (med.wd_tripped()) {
            med.wd_disable();
            std::printf("medula-wd: disparou com kicks em dia (sleep de 20 ms atrasou > 100 ms?)\n");
            return 3;
        }
        hal::sleep_ms(20);
    }

    // Para de chutar: o tempo medido parte do último kick.
    med.wd_kick();
    const uint64_t t0 = hal::now_us();
    uint64_t x_us = 0;
    bool tripped = false;
    while ((x_us = hal::now_us() - t0) < 1000000u) {
        if (med.wd_tripped()) { tripped = true; break; }
        hal::sleep_ms(1);
    }
    med.wd_disable();
    if (!tripped) {
        std::printf("medula-wd: não disparou em 1 s\n");
        return 3;
    }
    const unsigned x_ms = static_cast<unsigned>((x_us + 500u) / 1000u);   // ms inteiros mais próximos
    std::printf("medula-wd: disparou em %u ms (%u.%03u)\n", x_ms, static_cast<unsigned>(x_us / 1000u),
                static_cast<unsigned>(x_us % 1000u));
    return (x_ms >= 100 && x_ms <= 150) ? 0 : 3;
}

}}  // namespace aracno::app
