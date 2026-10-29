// hal::Sccb contra o barramento open-drain e o escravo SCCB simulados em tests/fake_cam.h:
// sequência de bits de escrita, leitura SEM repeated start, NACK vira erro, RESET_N/PWDN preservados.
#include <cstdio>
#include <cstring>
#include "fake_cam.h"
#include "hal/sccb.h"

using namespace aracno;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FALHA linha %d: %s\n", __LINE__, msg); ++fails; } } while (0)
#define CHECK_TRACE(fake, want) do { if (std::strcmp((fake).trace, want)) { \
    std::printf("FALHA linha %d: rastro\n  obtido:   \"%s\"\n  esperado: \"%s\"\n", __LINE__, (fake).trace, want); ++fails; } } while (0)

static bool released(const FakeCamFpga& f) {
    return !(f.out & (1u << qsys::PIO_CAM_BIT_SIOC_LOW)) && !(f.out & (1u << qsys::PIO_CAM_BIT_SIOD_LOW));
}

int main() {
    // escrita: S 0x42 reg val P, todos com ACK; RESET_N = 1 e PWDN = 0 em toda escrita do pio_cam
    {
        FakeCamFpga f; hal::Sccb s;
        s.attach(&f, qsys::PIO_CAM_BASE, qsys::PIO_CAM_IN_BASE);
        s.set_reset_n(true);
        f.check_ctrl = true; f.ctrl_expect = 1;   // bits 3..2 = PWDN 0, RESET_N 1
        CHECK(s.write_reg(0x12, 0x80), "write_reg com ACK");
        CHECK_TRACE(f, "S 42+ 12+ 80+ P ");
        CHECK(f.regs[0x12] == 0x80, "escravo gravou COM7");
        CHECK(f.ctrl_violations == 0, "RESET_N/PWDN preservados durante a escrita");
        CHECK(f.glitches == 0, "SIOC e SIOD nunca mudam na mesma escrita");
        CHECK(f.repeated_starts == 0, "sem repeated start");
        CHECK(released(f), "barramento solto no fim");
        CHECK(s.last_error() == hal::Sccb::OK && s.errors() == 0, "sem erro");
    }
    // leitura: escrita do sub-endereço + STOP + START + leitura com NACK final; bits na ordem MSB primeiro
    {
        FakeCamFpga f; hal::Sccb s;
        s.attach(&f, qsys::PIO_CAM_BASE, qsys::PIO_CAM_IN_BASE);
        f.regs[0x0A] = 0x76; f.regs[0x0B] = 0x73; f.regs[0x55] = 0xA5; f.regs[0x56] = 0x01;
        uint8_t v = 0;
        CHECK(s.read_reg(0x0A, &v) && v == 0x76, "PID 0x76");
        CHECK_TRACE(f, "S 42+ 0A+ P S 43+ 76- P ");
        CHECK(f.repeated_starts == 0, "leitura sem repeated start");
        CHECK(f.stops == 2 && f.starts == 2, "duas transações completas");
        f.clear_trace();
        CHECK(s.read_reg(0x55, &v) && v == 0xA5, "0xA5 (bits alternados)");
        CHECK(s.read_reg(0x56, &v) && v == 0x01, "0x01 (só o LSB)");
        CHECK_TRACE(f, "S 42+ 55+ P S 43+ A5- P S 42+ 56+ P S 43+ 01- P ");
        CHECK(f.glitches == 0, "SIOC e SIOD nunca mudam na mesma escrita");
        CHECK(released(f), "barramento solto no fim");
    }
    // endereço sem ACK (câmera ausente/em reset): erro NACK_ADDR, STOP enviado, barramento solto
    {
        FakeCamFpga f; hal::Sccb s;
        f.addr7 = 0x30;
        s.attach(&f, qsys::PIO_CAM_BASE, qsys::PIO_CAM_IN_BASE);
        CHECK(!s.write_reg(0x12, 0x80), "write_reg sem ACK falha");
        CHECK(s.last_error() == hal::Sccb::NACK_ADDR, "erro NACK_ADDR");
        CHECK_TRACE(f, "S 42- P ");
        f.clear_trace();
        uint8_t v = 0x5A;
        CHECK(!s.read_reg(0x0A, &v), "read_reg sem ACK falha");
        CHECK(v == 0x5A, "valor não tocado em erro");
        CHECK_TRACE(f, "S 42- P ");
        CHECK(s.errors() == 2, "dois erros contados");
        CHECK(released(f), "barramento solto");
    }
    // dado sem ACK: erro NACK_DATA
    {
        FakeCamFpga f; hal::Sccb s;
        f.nack_data = true;
        s.attach(&f, qsys::PIO_CAM_BASE, qsys::PIO_CAM_IN_BASE);
        CHECK(!s.write_reg(0x12, 0x80), "dado sem ACK falha");
        CHECK(s.last_error() == hal::Sccb::NACK_DATA, "erro NACK_DATA");
        CHECK_TRACE(f, "S 42+ 12+ 80- P ");
        CHECK(s.write_reg(0x12, 0x80) == false && s.errors() == 2, "erro se repete");
    }
    // sem pull-up (SIOD/SIOC não sobem): BUS_BUSY sem gerar START; ACK falso não passa
    {
        FakeCamFpga f; hal::Sccb s;
        f.pullup = false;
        s.attach(&f, qsys::PIO_CAM_BASE, qsys::PIO_CAM_IN_BASE);
        uint8_t v = 0;
        CHECK(!s.write_reg(0x12, 0x80), "sem pull-up falha");
        CHECK(s.last_error() == hal::Sccb::BUS_BUSY, "erro BUS_BUSY");
        CHECK(!s.read_reg(0x0A, &v), "leitura sem pull-up falha");
        CHECK(f.starts == 0, "nenhum START com o barramento preso");
    }
    // sombra: attach lê o pio_cam (RESET_N já em 1, PWDN 1) e SCCB não mexe nesses bits
    {
        FakeCamFpga f; hal::Sccb s;
        f.out = (1u << qsys::PIO_CAM_BIT_RESET_N) | (1u << qsys::PIO_CAM_BIT_PWDN);
        s.attach(&f, qsys::PIO_CAM_BASE, qsys::PIO_CAM_IN_BASE);
        f.check_ctrl = true; f.ctrl_expect = 3;
        uint8_t v = 0;
        f.regs[0x0B] = 0x73;
        CHECK(s.write_reg(0x11, 0x01) && s.read_reg(0x0B, &v) && v == 0x73, "escrita e leitura");
        CHECK(f.ctrl_violations == 0, "RESET_N/PWDN semeados pela leitura e preservados");
        f.check_ctrl = false;
        s.set_pwdn(false);
        CHECK(f.out == (1u << qsys::PIO_CAM_BIT_RESET_N), "set_pwdn(false) só zera o bit3");
        s.set_reset_n(false);
        CHECK(f.out == 0, "set_reset_n(false) só zera o bit2");
    }
    // sem attach: falha sem acessar nada
    {
        hal::Sccb s; uint8_t v = 0;
        CHECK(!s.write_reg(0x12, 0x80) && !s.read_reg(0x0A, &v), "sem attach falha");
    }
    std::printf("sccb: %d falhas\n", fails);
    return fails ? 1 : 0;
}
