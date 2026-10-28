#include "app/bench.h"
#include <cstdio>
#include <cstring>
#include "aracno/qsys.h"
#include "hal/avalon.h"
#include "hal/clock.h"
#include "hal/lwbridge.h"
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

}}  // namespace aracno::app
