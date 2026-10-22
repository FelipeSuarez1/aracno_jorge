// Layout do registro binário (deve bater com tools/log2csv.py) e CRC-32 IEEE.
#include <cstdio>
#include <cstring>
#include <cstddef>
#include "aracno/log.h"

using namespace aracno;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FALHA linha %d: %s\n", __LINE__, msg); ++fails; } } while (0)

int main() {
    CHECK(sizeof(Obs) == 40, "sizeof(Obs) == 40 (log2csv OBS_SIZE)");
    CHECK(sizeof(LogRec) == 64, "sizeof(LogRec) == 64 (log2csv REC_SIZE)");
    CHECK(offsetof(LogRec, obs) == 8 && offsetof(LogRec, desired) == 48 && offsetof(LogRec, period_us) == 56 &&
          offsetof(LogRec, crc) == 60, "offsets do LogRec");
    CHECK(offsetof(Obs, acc_x_mg) == 14 && offsetof(Obs, t_since_stimulus_ms) == 20 && offsetof(Obs, age_ms) == 24 &&
          offsetof(Obs, installed) == 38, "offsets da Obs");
    CHECK(crc32_ieee("123456789", 9) == 0xCBF43926u, "CRC-32 IEEE known answer");

    // Gera um arquivo de 3 registros para o teste manual de tools/log2csv.py.
    FILE* f = std::fopen("/tmp/aracno_test.log", "wb");
    if (f) {
        for (int i = 0; i < 3; ++i) {
            LogRec r; std::memset(&r, 0, sizeof r);
            r.magic = LOG_MAGIC; r.version = LOG_VERSION; r.size = sizeof r;
            r.obs.t_ms = 1000 + 20 * i; r.obs.lux_l = 7; r.obs.lux_r = 300; r.obs.dist_front_mm = 450; r.obs.dist_floor_mm = 60;
            r.obs.acc_z_mg = 1000; r.obs.t_since_stimulus_ms = 5000; r.obs.installed = 0x7F;
            for (int s = 0; s < N_SENSORS; ++s) r.obs.age_ms[s] = 20;
            r.desired.action = FORWARD; r.desired.speed = 50; r.allowed = r.desired;
            r.motion = 2; r.step = i; r.period_us = 20000 + i;
            r.crc = crc32_ieee(&r, sizeof r - sizeof r.crc);
            std::fwrite(&r, sizeof r, 1, f);
        }
        std::fputs("lixo", f);  // bytes corrompidos no fim: o leitor deve ignorar
        std::fclose(f);
    }
    std::printf("log: %d falhas\n", fails);
    return fails ? 1 : 0;
}
