#pragma once
#include <stdint.h>
#include "hal/avalon.h"
#include "hal/lwbridge.h"

namespace aracno { namespace hal {

// Mestre SCCB (I2C de 3 fios da OmniVision) por bit-bang sobre o pio_cam/pio_cam_in da FPGA (S09).
// Open-drain emulado: escrever 1 no bit *_LOW drena o pino a 0; 0 solta e o pull-up de 4,7 kΩ sobe.
// SIOD é lido no pio_cam_in (bit0), SIOC no bit1 (só para conferir o barramento livre antes do start).
// Meio período padrão de 5 µs (≈100 kHz, como i2c_init(i2c0, 100 * 1000) da referência pico-ov7670-driver),
// em espera ativa sobre now_us.
// A OV7670 NÃO aceita repeated start: leitura = [S addr+W reg P] [S addr+R dado NACK P]
// (comentário em ov7670_read_register da referência; SCCB Functional Specification, OmniVision).
// Dono único do pio_cam: RESET_N e PWDN moram na mesma sombra (AvalonPio) e são preservados.
class Sccb {
public:
    enum Error { OK = 0, BUS_BUSY, NACK_ADDR, NACK_REG, NACK_DATA };

    Sccb() : io_(0), in_base_(0), addr7_(0x21), half_us_(5), err_(OK), errors_(0) {}
    // Sombra semeada lendo o pio_cam (reset da FPGA = 0: câmera em reset, linhas soltas).
    // Só lê: escrever nesses endereços exige ter conferido o ID do cam_capture antes.
    void attach(RegIo* io, uint32_t out_base, uint32_t in_base, uint8_t addr7 = 0x21);
    void set_half_period_us(uint32_t us) { half_us_ = us ? us : 1; }

    bool write_reg(uint8_t reg, uint8_t val);
    bool read_reg(uint8_t reg, uint8_t* val);

    // Pinos de controle da câmera, no mesmo PIO.
    void set_reset_n(bool v);
    void set_pwdn(bool v);
    uint32_t out_shadow() const { return pio_.shadow(); }
    uint32_t in_levels() const { return io_ ? io_->read32(in_base_, 0) : 0; }

    Error last_error() const { return err_; }
    uint32_t errors() const { return errors_; }
    static const char* error_name(Error e);

    // Primitivas do barramento (públicas para diagnóstico/testes).
    bool start();                  // false se SIOC/SIOD não sobem soltos (sem pull-up ou preso)
    void stop();
    bool write_byte(uint8_t b);    // true = escravo deu ACK (SIOD em 0 no 9º pulso)
    uint8_t read_byte(bool nack);  // nack = true no último byte

private:
    void scl(bool high);
    void sda(bool high);
    bool sda_in() const;
    bool scl_in() const;
    void half() const;
    bool fail(Error e);

    RegIo* io_;
    AvalonPio pio_;
    uint32_t in_base_;
    uint8_t addr7_;
    uint32_t half_us_;
    Error err_;
    uint32_t errors_;
};

}}  // namespace aracno::hal
