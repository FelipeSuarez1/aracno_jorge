#pragma once
#include <stdint.h>
#include "hal/spi.h"
#include "face/canvas.h"

namespace aracno { namespace face {

// Pino de controle D/C e RST do display, abstraído: hoje saem do PCF8574 (I2C), porque o
// header LTC não tem GPIO livre (docs/40-Fontes/hardware.md); amanhã pode ser um GPIO.
class CtrlPins {
public:
    virtual ~CtrlPins() {}
    virtual bool dc(bool data) = 0;     // true = dados, false = comando
    virtual bool reset(bool low) = 0;   // true = RST em nível baixo
    // false se RST não está ligado (pull-up da placa Adafruit): init usa só SWRESET.
    virtual bool has_reset() const { return true; }
};

// ILI9341 em SPI 4 fios (Adafruit 2,8" #1770, jumpers IM1-3 fechados), 320×240 paisagem,
// RGB565 big-endian. Datasheet ILI9341 V1.11 (ILITEK): comandos §8.2.
// A atualização de quadro inteiro (153 600 B) leva ~150 ms a 8 MHz — não cabe num ciclo de
// 20 ms do laço de controle; por isso o envio é INCREMENTAL: begin_flush() define a janela e
// flush_step() envia até `budget` bytes por chamada. Mesmo assim cada passo bloqueia (~8 ms a
// 4 MHz, spidev sem timeout; troca de D/C via I2C pode bloquear ~1 s): na integração com o laço
// de controle o display roda numa thread própria, com I2C protegido por trava (pendente S04/S06).
class Ili9341 {
public:
    static const int W = CANVAS_W, H = CANVAS_H;
    static const size_t STAGE_BYTES = 4096;   // = spidev bufsiz padrão

    Ili9341();
    // Reset (RST por hardware se ligado, senão só SWRESET), init, tela preta. ~250 ms.
    bool init(hal::SpiIo* spi, CtrlPins* pins, void (*sleep_ms)(uint32_t));
    bool ok() const { return ok_; }

    // Envia a região `r` do canvas de uma vez (bloqueante). Para demos/medições.
    bool flush(const Canvas& c, const Rect& r);
    // Envio incremental: begin define janela e estado; cada step envia ≤ budget bytes de pixels.
    // Devolve true enquanto houver pixels pendentes.
    bool begin_flush(const Canvas& c, const Rect& r);
    bool flush_step(size_t budget);
    bool flushing() const { return pending_ > 0; }
    bool failed() const { return failed_; }     // última transferência abortou por erro de SPI/I2C
    uint32_t bytes_sent() const { return bytes_sent_; }

    // Comandos (ILI9341 §8.2)
    enum Cmd {
        SWRESET = 0x01, SLPOUT = 0x11, DISPOFF = 0x28, DISPON = 0x29,
        CASET = 0x2A, PASET = 0x2B, RAMWR = 0x2C, MADCTL = 0x36, PIXFMT = 0x3A
    };
    // MADCTL (§8.2.29): MY=1, MX=1, MV=1, BGR=1 → 0xE8 = paisagem 320×240 (= setRotation(3) da
    // Adafruit_ILI9341). De cabeça para baixo: 0x28 (MV|BGR). Espelhado: 0x68 ou 0xA8.
    // 0x48/0x88 têm MV=0 (retrato 240×320) e NÃO servem com W = 320. [A VALIDAR na bancada]
    static const uint8_t MADCTL_LANDSCAPE = 0xE8;

private:
    bool cmd(uint8_t c);
    bool data(const uint8_t* d, size_t n);
    bool window(int x0, int y0, int x1, int y1);

    hal::SpiIo* spi_;
    CtrlPins* pins_;
    bool ok_;
    const Canvas* canvas_;
    Rect rect_;
    int row_, col_;          // próximo pixel a enviar dentro de rect_
    size_t pending_;         // bytes de pixel ainda não enviados
    bool failed_;
    uint32_t bytes_sent_;
    uint8_t stage_[STAGE_BYTES];
};

}}  // namespace aracno::face
