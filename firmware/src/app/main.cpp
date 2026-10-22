// aracno — laço de controle do HPS (S03).
//   sensores → cérebro (demo/política) → segurança → servos, a 50 Hz, com watchdog e log.
// Uso: aracno [--demo stand|walk N|turn N | --policy] [--sensors 0xMASK | --no-sensors] [--no-watchdog]
//             [--speed S] [--log ARQ] [--hz H] [--max-s S]
//   --policy: levanta (M_STAND) e depois entrega o comando à política aprendida (brain/weights.h).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <sched.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "aracno/types.h"
#include "aracno/log.h"
#include "hal/clock.h"
#include "hal/lwbridge.h"
#include "hal/watchdog.h"
#include "motor/pwm.h"
#include "motor/spider.h"
#include "sense/sensors.h"
#include "safety/safety.h"
#include "app/demo.h"
#include "brain/policy.h"

#ifndef ARACNO_GIT_REV
#define ARACNO_GIT_REV "unknown"
#endif

using namespace aracno;

namespace {

volatile sig_atomic_t g_stop = 0;
void on_signal(int) { g_stop = 1; }

// LEDs do DE0 via PIO Qsys (hps_0.h LED_PIO_BASE 0x120, 7 bits): bit0 heartbeat, bit5 abort neste ciclo, bit6 travado.
const uint32_t LED_PIO_BASE = 0x120;

motor::Motion motion_for(uint8_t action) {
    switch (action) {
    case FORWARD:    return motor::M_FORWARD;
    case BACKWARD:   return motor::M_BACKWARD;
    case TURN_LEFT:  return motor::M_ROTATE_L;
    case TURN_RIGHT: return motor::M_ROTATE_R;
    case CROUCH:     return motor::M_CROUCH;
    case LUNGE:      return motor::M_LUNGE;
    default:         return motor::M_NONE;   // STOP, HEAD_* (cabeça em S06)
    }
}

struct Hist {   // histograma do período do laço: faixas de 250 us até 50 ms, + estouro
    static const int NB = 200;
    uint32_t bin[NB + 1];
    uint32_t n, max_us;
    Hist() : n(0), max_us(0) { std::memset(bin, 0, sizeof bin); }
    void add(uint32_t us) { ++n; if (us > max_us) max_us = us; uint32_t b = us / 250; if (b > NB) b = NB; ++bin[b]; }
    uint32_t pct(double p) const {
        uint32_t acc = 0; for (int i = 0; i <= NB; ++i) { acc += bin[i]; if (acc >= p * n) return i * 250; } return NB * 250;
    }
};

}  // namespace

int main(int argc, char** argv) {
    app::Demo::Mode mode = app::Demo::STAND;
    int cycles = 0, speed = 50, hz = 50, max_s = 0;
    uint8_t sensors_mask = 0x7F;
    bool use_wd = false;   // PENDENTE S03: abrir /dev/watchdog reinicia o SoC (ver docs/00-Inbox/watchdog-dw-wdt-reinicia.md)
    bool use_policy = false;
    const char* log_path = "/home/root/aracno.log";
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--demo") && i + 1 < argc) {
            const char* m = argv[++i];
            if (!std::strcmp(m, "stand")) mode = app::Demo::STAND;
            else if (!std::strcmp(m, "walk")) { mode = app::Demo::WALK; cycles = (i + 1 < argc) ? std::atoi(argv[++i]) : 1; }
            else if (!std::strcmp(m, "turn")) { mode = app::Demo::TURN; cycles = (i + 1 < argc) ? std::atoi(argv[++i]) : 1; }
        } else if (!std::strcmp(argv[i], "--policy")) { use_policy = true; mode = app::Demo::STAND; }
        else if (!std::strcmp(argv[i], "--sensors") && i + 1 < argc) sensors_mask = static_cast<uint8_t>(std::strtoul(argv[++i], 0, 0));
        else if (!std::strcmp(argv[i], "--no-sensors")) sensors_mask = 0;
        else if (!std::strcmp(argv[i], "--no-watchdog")) use_wd = false;
        else if (!std::strcmp(argv[i], "--watchdog")) use_wd = true;
        else if (!std::strcmp(argv[i], "--speed") && i + 1 < argc) speed = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--hz") && i + 1 < argc) hz = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--max-s") && i + 1 < argc) max_s = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--log") && i + 1 < argc) log_path = argv[++i];
        else { std::fprintf(stderr, "argumento desconhecido: %s\n", argv[i]); return 2; }
    }
    if (hz < 10 || hz > 200) hz = 50;
    const uint64_t period_us = 1000000u / static_cast<uint64_t>(hz);

    std::printf("aracno rev=%s demo=%d ciclos=%d sensores=0x%02x watchdog=%d hz=%d\n",
                ARACNO_GIT_REV, mode, cycles, sensors_mask, use_wd, hz);

    // Sinais antes de tocar em hardware: qualquer saída passa pelo ABORT do fim.
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::signal(SIGHUP, on_signal);   // queda da sessão da UART

    // Tempo real: trava o que já está mapeado (código, pilha, dados). NUNCA MCL_FUTURE: ele faria o
    // kernel pré-tocar o mapeamento de /dev/mem e ler periféricos do HPS → reset do SoC (teste 1, dia 1).
    if (mlockall(MCL_CURRENT) != 0) std::perror("mlockall (continuando)");
    // musl devolve ENOSYS para sched_setscheduler; via syscall direta funciona (kernel PREEMPT_NONE:
    // o ganho é limitado; medimos no histograma).
    sched_param sp; sp.sched_priority = 50;
    if (syscall(SYS_sched_setscheduler, 0, SCHED_FIFO, &sp) != 0) std::perror("SCHED_FIFO (continuando)");

    hal::LwBridge bridge;
    if (!bridge.open()) { std::fprintf(stderr, "ponte lightweight: /dev/mem falhou\n"); return 1; }
    motor::PwmBank pwm;
    pwm.init(&bridge);
    motor::Spider spider;
    spider.attach(&pwm);
    spider.set_speed(speed);

    sense::SensorConfig scfg;
    scfg.installed = sensors_mask;
    sense::Sensors sensors;
    uint8_t alive = sensors.init(scfg);
    std::printf("sensores pedidos=0x%02x respondendo=0x%02x\n", sensors_mask, alive);

    safety::Safety safety;
    app::Demo demo(mode, cycles, static_cast<uint8_t>(speed));
    Policy policy;
    if (use_policy) {
        if (!policy.self_test()) { std::fprintf(stderr, "politica: self_test falhou (weights.h x policy_vectors.h)\n"); pwm.abort_all(true); return 1; }
        std::printf("politica: weights %s\n", policy.weights_sha());
        if (sensors_mask == 0) std::printf("AVISO: --policy sem sensores: a politica vai agir sobre zeros\n");
    }

    // Watchdog: a spec exige antes do bote perto de pessoas; hoje é opt-in (--watchdog) porque
    // abrir /dev/watchdog no kernel 3.13 reinicia o SoC — pendência documentada no Inbox.
    hal::Watchdog wd;
    if (use_wd) {
        if (!wd.open(5)) { std::fprintf(stderr, "/dev/watchdog indisponível\n"); pwm.abort_all(true); return 1; }
        std::printf("watchdog armado: %u s (clock %u Hz)\n", wd.timeout_s(), wd.clock_hz());
    } else {
        std::printf("AVISO: sem watchdog de hardware (pendencia S03b); processo travado = servos congelados\n");
    }

    FILE* log = std::fopen(log_path, "wb");
    if (!log) std::perror("log (continuando sem)");
    static char logbuf[1 << 16];
    if (log) std::setvbuf(log, logbuf, _IOFBF, sizeof logbuf);   // write() no microSD só a cada 64 KiB

    Obs obs; std::memset(&obs, 0, sizeof obs);
    for (int s = 0; s < N_SENSORS; ++s) obs.age_ms[s] = AGE_NEVER;
    Hist hist;
    uint64_t next = hal::now_us() + period_us;
    uint64_t last = hal::now_us();
    const uint64_t t_end = max_s ? last + static_cast<uint64_t>(max_s) * 1000000u : 0;
    uint32_t cycle = 0;
    uint8_t last_reason = safety::RS_OK;

    while (!g_stop) {
        hal::sleep_until_us(next);
        uint64_t now_us = hal::now_us();
        uint32_t now_ms = static_cast<uint32_t>(now_us / 1000u);
        uint32_t dt = static_cast<uint32_t>(now_us - last);
        last = now_us;
        next += period_us;
        if (now_us > next) next = now_us + period_us;   // atrasou mais de um período: ressincroniza
        if (cycle) hist.add(dt);
        ++cycle;

        // 1. sensores (I2C bloqueante nesta thread: se demorou mais que stale_ms, nada é fresco)
        sensors.sample(&obs, now_ms);
        if (sensors.last_sample_ms() > safety.limits().stale_ms)
            for (int s = 0; s < N_SENSORS; ++s) if (obs.age_ms[s] != AGE_NEVER) obs.age_ms[s] = 0xFFFE;

        // 2. cérebro: demo até levantar; depois a política, se pedida
        Cmd desired = (use_policy && demo.stood()) ? policy.act(obs) : demo.next(spider);
        bool want_stand = desired.action == app::Demo::STAND_REQUEST;
        Cmd probe = desired;
        if (want_stand) { probe.action = CROUCH; }   // levantar = movimento no lugar para a segurança

        // 3. segurança
        safety::Verdict v = safety.filter(obs, probe);
        if (v.reason != last_reason) {
            std::printf("[%u ms] seguranca: %s%s\n", now_ms, safety::reason_name(v.reason), v.abort ? " (ABORT)" : "");
            last_reason = v.reason;
        }

        // 4. atuação
        if (v.abort || spider.fault()) {
            if (spider.fault() && !v.abort) std::printf("[%u ms] stepper em falha (%u timeouts): ABORT\n", now_ms, spider.step_timeouts());
            spider.abort_now();
            if (spider.fault()) break;   // FPGA/servo fora: encerra com servos soltos
        } else if (spider.aborted()) {
            spider.wake();               // escalonado: uma perna por tick, nada parte até terminar
        } else if (v.reason != safety::RS_OK) {
            spider.stop_after_step();    // segurança restringiu: termina o passo corrente e para
        } else if (want_stand && v.cmd.action == CROUCH) {
            if (!spider.busy() && spider.current() != motor::M_STAND) spider.start(motor::M_STAND, 1, now_ms);
        } else {
            motor::Motion m = motion_for(v.cmd.action);
            if (m == motor::M_NONE) {
                if (spider.busy()) spider.stop_after_cycle();
            } else if (!spider.busy()) {
                spider.set_speed(v.cmd.speed);
                spider.start(m, 1, now_ms);
            } else if (spider.current() != m) {
                spider.stop_after_cycle();
            }
        }
        spider.tick(now_ms);

        // 5. sinais de vida
        uint32_t leds = ((now_ms / 500u) & 1u) | (safety.latched() ? 0x40u : 0u) | (v.abort ? 0x20u : 0u);
        bridge.write32(LED_PIO_BASE, 0, leds);
        wd.kick();

        // 6. log
        if (log) {
            LogRec r; std::memset(&r, 0, sizeof r);
            r.magic = LOG_MAGIC; r.version = LOG_VERSION; r.size = sizeof r;
            r.obs = obs; r.desired = desired; r.allowed = v.cmd; r.reason = v.reason; r.abort = v.abort ? 1 : 0;
            r.motion = static_cast<uint8_t>(spider.current()); r.step = static_cast<uint8_t>(spider.step_index());
            r.period_us = dt;
            r.crc = crc32_ieee(&r, sizeof r - sizeof r.crc);
            std::fwrite(&r, sizeof r, 1, log);
        }

        if (!use_policy && demo.done(spider) && !spider.busy()) { std::printf("demo concluída\n"); break; }
        if (t_end && now_us >= t_end) { std::printf("tempo máximo atingido\n"); break; }
    }

    // Saída: servos soltos, cão desarmado, log fechado.
    pwm.abort_all(true);
    bridge.write32(LED_PIO_BASE, 0, 0);
    wd.close_disarm();
    if (log) std::fclose(log);
    std::printf("ciclos=%u periodo nominal=%llu us p50=%u us p99=%u us max=%u us timeouts_passo=%u erros_i2c=%u\n",
                cycle, static_cast<unsigned long long>(period_us), hist.pct(0.50), hist.pct(0.99), hist.max_us,
                spider.step_timeouts(), sensors.i2c_errors());
    return 0;
}
