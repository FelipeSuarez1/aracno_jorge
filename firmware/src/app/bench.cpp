#include "app/bench.h"
#include <cstdio>
#include <cstring>
#include "hal/clock.h"
#include "hal/gpio_sysfs.h"
#include "hal/i2c.h"
#include "hal/spi.h"
#include "sense/i2c_devices.h"
#include "face/face.h"
#include "face/ili9341.h"
#include "face/pcf_ctrl.h"

namespace aracno { namespace app {

static bool open_spi(hal::SpiDev& spi, unsigned hz) {
    if (!hal::gpio_set_output(hal::GPIO_LTC_MUX, false)) {
        std::fprintf(stderr, "gpio%d (mux do header LTC) falhou\n", hal::GPIO_LTC_MUX);
        return false;
    }
    if (!spi.open("/dev/spidev1.0", hz, 0, 8)) {
        std::fprintf(stderr, "/dev/spidev1.0 falhou\n");
        return false;
    }
    std::printf("spi: /dev/spidev1.0 pedido %u Hz, aceito %u Hz; gpio%d = 0\n", hz, spi.hz(), hal::GPIO_LTC_MUX);
    return true;
}

int spi_loopback(unsigned hz) {
    hal::SpiDev spi;
    if (!open_spi(spi, hz)) return 1;
    uint8_t tx[256], rx[256];
    for (int i = 0; i < 256; ++i) tx[i] = static_cast<uint8_t>(i ^ 0x5A);
    std::memset(rx, 0, sizeof rx);
    if (!spi.xfer(tx, rx, sizeof tx)) { std::fprintf(stderr, "xfer falhou\n"); return 1; }
    int bad = 0;
    for (int i = 0; i < 256; ++i) if (rx[i] != tx[i]) ++bad;
    std::printf("loopback: %d/256 bytes diferentes (rx[0..3] = %02x %02x %02x %02x)\n", bad, rx[0], rx[1], rx[2], rx[3]);
    if (bad == 0) std::printf("LOOPBACK OK: mux e pinos 5/7 do J17 confirmados\n");
    else if (rx[0] == 0 && rx[1] == 0) std::printf("só zeros: jumper 5-7 ausente ou mux não comutou\n");
    else if (rx[0] == 0xFF && rx[1] == 0xFF) std::printf("só 0xFF: MISO flutuando — jumper no pino errado?\n");
    return bad ? 2 : 0;
}

int face_demo(unsigned hz, unsigned seconds_per_expr, bool with_reset) {
    hal::I2cBus bus;
    if (!bus.open("/dev/i2c-1")) { std::fprintf(stderr, "/dev/i2c-1 falhou\n"); return 1; }
    sense::Pcf8574 pcf;
    if (!pcf.init(&bus, 0x20)) {
        std::fprintf(stderr, "PCF8574 não respondeu em 0x20 (A0-A2 no GND? pull-ups? pinos 9/11 do J17?)\n");
        return 1;
    }
    std::printf("pcf8574: ok em 0x20\n");
    hal::SpiDev spi;
    if (!open_spi(spi, hz)) return 1;

    face::PcfCtrlPins pins(&pcf, with_reset);
    std::printf("ili9341: RST %s\n", with_reset ? "pelo PCF8574 P5" : "não ligado (só SWRESET)");
    face::Ili9341 lcd;
    uint64_t t0 = hal::now_us();
    if (!lcd.init(&spi, &pins, hal::sleep_ms)) { std::fprintf(stderr, "init do ILI9341 falhou\n"); return 1; }
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
        uint32_t tog0 = pins.toggles();
        t0 = hal::now_us();
        if (!lcd.flush(f.canvas(), full)) { std::fprintf(stderr, "[%s] flush do quadro inteiro falhou\n", nomes[e]); return 1; }
        f.flushed();
        uint32_t full_us = static_cast<uint32_t>(hal::now_us() - t0);
        // Taxa real deduzida do tempo (o "aceito" do driver não é o divisor real).
        std::printf("[%s] quadro inteiro %u ms (~%u kbit/s reais, D/C %u trocas)\n", nomes[e], full_us / 1000u,
                    static_cast<unsigned>(153600ull * 8u * 1000u / (full_us ? full_us : 1)), pins.toggles() - tog0);

        // Fica na expressão, enviando só o que mudou (piscar).
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
    std::printf("face-demo: fim (%u bytes enviados)\n", lcd.bytes_sent());
    return 0;
}

}}  // namespace aracno::app
