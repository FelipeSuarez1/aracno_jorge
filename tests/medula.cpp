// S11: modelo de referência da medula (sequência F0..F19 do testbench, valores da spec) e driver
// medula::Medula contra um RegIo falso: nenhuma escrita antes do ID conferir; upload little-endian;
// CTRL = sombra | GO | BG_RESET; wait_done com timeout; ordem WD_TIMEOUT_MS → WD_CTRL; desempacotamento.
#include <cstdio>
#include <cstring>
#include "aracno/qsys.h"
#include "hal/clock.h"
#include "hal/lwbridge.h"
#include "medula/medula.h"
#include "medula/model.h"
#include "medula/scenes.h"

using namespace aracno;
using medula::Medula;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FALHA linha %d: %s\n", __LINE__, msg); ++fails; } } while (0)

// Compara um campo do quadro `f` e imprime o valor obtido × esperado se divergir.
static void expect(int f, const char* field, long got, long want) {
    if (got != want) {
        std::printf("FALHA F%02d: %s = %ld, esperado %ld\n", f, field, got, want);
        ++fails;
    }
}

static uint8_t scene[medula::N];
static medula::Model model;

// Uma linha da tabela do passo 4: cena e valores esperados do quadro.
struct Frame {
    int scene;
    bool bg_reset;
    bool target;
    int count, cx, cy, mn, mean, x0, y0, x1, y1;
    bool reflex;
    int pan, tilt;
};

static const Frame frames[20] = {
    //  cena reset  target count cx cy min mean  bbox            reflexo pan   tilt
    {0, true,  false,   0,  0,  0,  0,  0,  0,  0,  0,  0, false, 1500, 1500},   // F0
    {1, false, true,  551, 70, 55, 50, 50, 61, 41, 79, 69, false, 1458, 1522},   // F1
    {2, false, true,  551, 70, 55, 40, 40, 61, 41, 79, 69, false, 1458, 1522},   // F2
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, true,  1458, 1778},   // F3
    {4, false, true,  551, 70, 55, 36, 36, 61, 41, 79, 69, false, 1458, 1522},   // F4
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, false, 1458, 1522},   // F5
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, false, 1458, 1522},   // F6
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, false, 1458, 1522},   // F7
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, false, 1458, 1522},   // F8
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, false, 1458, 1522},   // F9
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, false, 1458, 1522},   // F10
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, false, 1458, 1522},   // F11
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, false, 1458, 1522},   // F12
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, false, 1458, 1522},   // F13
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, false, 1458, 1522},   // F14
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, false, 1458, 1522},   // F15
    {3, false, true,  551, 70, 55, 34, 34, 61, 41, 79, 69, false, 1458, 1522},   // F16
    {5, true,  false,   0,  0,  0,  0,  0,  0,  0,  0,  0, false, 1458, 1522},   // F17
    {6, false, true,  551, 70, 55, 50, 50, 61, 41, 79, 69, false, 1458, 1522},   // F18
    {7, false, false,  16,  0,  0,  0,  0,  0,  0,  0,  0, false, 1458, 1522},   // F19
};

static int bg_at(int x, int y) { return model.background(y * medula::W + x); }

// RegIo falso do bloco: registradores programáveis, FRAME gravado, log de todas as escritas.
class FakeMedula : public hal::RegIo {
public:
    struct Wr { uint32_t base, idx, val; };
    static const int LOG_MAX = 6000;
    FakeMedula() : writes_total(0), nlog(0), frame_writes(0), bad_base(0) {
        std::memset(reg, 0, sizeof reg);
        std::memset(frame, 0, sizeof frame);
        std::memset(bg, 0, sizeof bg);
        reg[Medula::ID] = Medula::ID_VALUE;
        reg[Medula::STATUS] = Medula::ST_DONE;
    }
    virtual void write32(uint32_t base, uint32_t idx, uint32_t v) {
        ++writes_total;
        if (base != qsys::MEDULA_BASE) ++bad_base;
        if (nlog < LOG_MAX) { log[nlog].base = base; log[nlog].idx = idx; log[nlog].val = v; ++nlog; }
        if (idx >= Medula::FRAME_WORD0 && idx < Medula::FRAME_WORD0 + 4800) {
            frame[idx - Medula::FRAME_WORD0] = v;
            ++frame_writes;
        }
    }
    virtual uint32_t read32(uint32_t base, uint32_t idx) const {
        if (base != qsys::MEDULA_BASE) return 0xDEADBEEFu;
        if (idx < 32) return reg[idx];
        if (idx >= Medula::BG_WORD0 && idx < Medula::BG_WORD0 + 4800) return bg[idx - Medula::BG_WORD0];
        return 0;
    }
    // Índice da última escrita no registrador `idx` (-1 se nenhuma) e a primeira.
    int last_write(uint32_t idx) const {
        for (int i = nlog - 1; i >= 0; --i) if (log[i].idx == idx) return i;
        return -1;
    }
    int first_write(uint32_t idx) const {
        for (int i = 0; i < nlog; ++i) if (log[i].idx == idx) return i;
        return -1;
    }

    uint32_t reg[32];
    uint32_t frame[4800];
    uint32_t bg[4800];
    Wr log[LOG_MAX];
    uint32_t writes_total;
    int nlog;
    uint32_t frame_writes;
    uint32_t bad_base;
};

static FakeMedula fake;

int main() {
    // ---- modelo: sequência F0..F19 do testbench ----
    {
        const medula::Params p = medula::default_params();
        CHECK(p.fg_delta == 8 && p.min_count == 40 && p.reflex_near == 38 && p.reflex_drop == 4 &&
              p.hold_ms == 1500 && p.pan_gain == 66 && p.tilt_gain == 68 && p.slew_us == 26 && p.reflex_en,
              "default_params = resets do contrato");
        model.set_frames(0);
        int reflexes = 0;
        for (int f = 0; f < 20; ++f) {
            const Frame& e = frames[f];
            medula::make_scene(e.scene, scene);
            medula::Result r;
            model.process(scene, e.bg_reset, p, &r);
            expect(f, "target", r.target, e.target);
            expect(f, "count", r.count, e.count);
            expect(f, "cx", r.cx, e.cx);
            expect(f, "cy", r.cy, e.cy);
            expect(f, "min_depth", r.min_depth, e.mn);
            expect(f, "mean_depth", r.mean_depth, e.mean);
            expect(f, "x0", r.x0, e.x0);
            expect(f, "y0", r.y0, e.y0);
            expect(f, "x1", r.x1, e.x1);
            expect(f, "y1", r.y1, e.y1);
            expect(f, "reflex", r.reflex, e.reflex);
            expect(f, "pan_target_us", r.pan_target_us, e.pan);
            expect(f, "tilt_target_us", r.tilt_target_us, e.tilt);
            expect(f, "latency_cycles", r.latency_cycles, 0);
            if (r.reflex) ++reflexes;
            expect(f, "frames()", model.frames(), f + 1);
            if (f == 15) expect(f, "BG(65,50)", bg_at(65, 50), 150);
            if (f == 16) expect(f, "BG(65,50)", bg_at(65, 50), 149);
            if (f == 18) {
                expect(f, "BG(5,5)", bg_at(5, 5), 100);
                expect(f, "BG(65,50)", bg_at(65, 50), 150);
            }
            if (f == 3) expect(f, "reflexos acumulados", reflexes, 1);
        }
        expect(19, "reflexos acumulados", reflexes, 1);
    }
    // sem REFLEX_EN o modelo nunca dispara, mas continua acompanhando prev_target/prev_min
    {
        medula::Params p = medula::default_params();
        p.reflex_en = false;
        medula::Model m;
        m.set_frames(0);
        medula::Result r;
        int n = 0;
        for (int i = 0; i < 4; ++i) {              // cenas 0..3: o susto da F3 seria disparado com reflex_en
            medula::make_scene(i, scene);
            m.process(scene, i == 0, p, &r);
            if (r.reflex) ++n;
        }
        CHECK(n == 0 && r.tilt_target_us == 1522, "sem reflex_en: nenhum reflexo, tilt efetivo = tilt_t");
        // ligado depois: prev_min já vale 34 (atualizado com o reflexo desligado); cena 4 (36) não é queda
        medula::make_scene(4, scene);
        p.reflex_en = true;
        m.process(scene, false, p, &r);
        CHECK(!r.reflex, "afastou: sem reflexo");
    }
    // asr4 (floor): pan do F1 = 1500 + floor(-660/16) = 1500 - 42; tilt = 1500 - floor(-340/16) = 1500 + 22 (via cena 1)
    {
        medula::Model m;
        m.set_frames(0);
        medula::Result r;
        medula::Params p = medula::default_params();
        medula::make_scene(0, scene);
        m.process(scene, true, p, &r);
        medula::make_scene(1, scene);
        m.process(scene, false, p, &r);
        CHECK(r.pan_target_us == 1458 && r.tilt_target_us == 1522, "asr4 por floor (valores negativos)");
    }

    // ---- driver: ID errado (.rbf antigo: 0x20000 cai no pwm17) ----
    {
        FakeMedula f; Medula m;
        f.reg[Medula::ID] = 0x00000001u;
        medula::Result r;
        CHECK(!m.attach(&f), "attach() falso com ID errado");
        CHECK(!m.present(), "present() falso");
        CHECK(!m.configure(medula::default_params()) && !m.upload(scene) && !m.start(true) &&
              !m.wait_done(1) && !m.read_result(&r) && !m.wd_enable(100) && !m.wd_tripped(),
              "nada funciona sem attach()");
        m.set_modes(true, true); m.wd_kick(); m.wd_disable();
        CHECK(m.frames() == 0 && m.background(0) == 0, "frames()/background() = 0 sem attach()");
        CHECK(f.writes_total == 0, "nenhuma escrita com ID errado");
    }
    // ID certo: attach() só lê
    {
        FakeMedula f; Medula m;
        f.reg[Medula::REFLEX_COUNT] = 7; f.reg[Medula::FRAMES] = 9;
        CHECK(m.attach(&f) && m.present(), "attach() com MED1");
        CHECK(f.writes_total == 0, "attach() não escreve");
        CHECK(m.frames() == 9 && m.frames_at_attach() == 9, "frames()");
    }
    // configure: THRESH, REFLEX_CFG, ganhos e SLEW; ignora reflex_en
    {
        FakeMedula f; Medula m;
        CHECK(m.attach(&f), "attach");
        medula::Params p = medula::default_params();
        p.hold_ms = 1; p.pan_gain = -66;
        CHECK(m.configure(p), "configure");
        CHECK(f.log[f.last_write(Medula::THRESH)].val == (8u | (40u << 8)), "THRESH");
        CHECK(f.log[f.last_write(Medula::REFLEX_CFG)].val == (38u | (4u << 8) | (1u << 16)), "REFLEX_CFG");
        CHECK(f.log[f.last_write(Medula::PAN_GAIN)].val == 0xFFBEu, "PAN_GAIN com sinal em 16 bits");
        CHECK(f.log[f.last_write(Medula::TILT_GAIN)].val == 68u, "TILT_GAIN");
        CHECK(f.log[f.last_write(Medula::SLEW_US)].val == 26u, "SLEW_US");
        CHECK(f.last_write(Medula::CTRL) == -1, "configure não toca CTRL");
        CHECK(f.writes_total == 5 && f.bad_base == 0, "5 escritas, todas na base da medula");
    }
    // upload: 4 pixels por palavra, little-endian; cena 1 palavra 0x2000+4002 = pixels x=8..11, y=100
    {
        FakeMedula f; Medula m;
        CHECK(m.attach(&f), "attach");
        medula::make_scene(1, scene);
        CHECK(m.upload(scene), "upload");
        CHECK(f.frame_writes == 4800 && f.writes_total == 4800, "4800 palavras escritas");
        CHECK(f.frame[4002] == 0x96329696u, "palavra 0x2000+4002 = 0x96329696");
        CHECK(f.frame[0] == 0x96969696u, "palavra 0 = 150 ×4");
        // BUSY: não escreve nada
        FakeMedula g; Medula n;
        CHECK(n.attach(&g), "attach");
        g.reg[Medula::STATUS] = Medula::ST_BUSY;
        CHECK(!n.upload(scene) && g.writes_total == 0, "upload falso e sem escrita com BUSY");
    }
    // start: CTRL = sombra | GO | BG_RESET
    {
        FakeMedula f; Medula m;
        CHECK(m.attach(&f), "attach");
        m.set_modes(true, true);
        CHECK(f.log[f.last_write(Medula::CTRL)].val == 0xCu, "set_modes escreve FOLLOW|REFLEX = bits 2/3");
        CHECK(m.start(true), "start(true)");
        CHECK(f.log[f.last_write(Medula::CTRL)].val == 0xFu, "start(true): CTRL = sombra|3");
        m.set_modes(false, true);
        CHECK(m.start(false), "start(false)");
        CHECK(f.log[f.last_write(Medula::CTRL)].val == 0x9u, "start(false): CTRL = REFLEX|GO");
        // BUSY: um GO seria ignorado pelo hardware; start() devolve false e não escreve em CTRL
        FakeMedula g; Medula n;
        CHECK(n.attach(&g), "attach");
        g.reg[Medula::STATUS] = Medula::ST_BUSY;
        CHECK(!n.start(false), "start(false) com BUSY → false");
        CHECK(g.last_write(Medula::CTRL) == -1 && g.writes_total == 0, "start() com BUSY não escreve em CTRL");
    }
    // wait_done: DONE nunca alto → false depois de ~timeout; DONE alto → true
    {
        FakeMedula f; Medula m;
        CHECK(m.attach(&f), "attach");
        f.reg[Medula::STATUS] = Medula::ST_BUSY;
        const uint64_t t0 = hal::now_us();
        CHECK(!m.wait_done(5), "wait_done(5) sem DONE → false");
        const uint64_t dt = hal::now_us() - t0;
        CHECK(dt >= 4000 && dt < 500000, "espera ~timeout e não trava");
        f.reg[Medula::STATUS] = Medula::ST_DONE;
        CHECK(m.wait_done(5), "wait_done com DONE → true");
    }
    // watchdog: WD_TIMEOUT_MS antes de WD_CTRL
    {
        FakeMedula f; Medula m;
        CHECK(m.attach(&f), "attach");
        CHECK(m.wd_enable(100), "wd_enable");
        const int it = f.first_write(Medula::WD_TIMEOUT_MS), ic = f.first_write(Medula::WD_CTRL);
        CHECK(it >= 0 && ic >= 0 && it < ic, "WD_TIMEOUT_MS escrito antes de WD_CTRL");
        CHECK(f.log[it].val == 100u && f.log[ic].val == 1u, "valores: timeout 100, ENABLE = 1");
        m.wd_kick();
        CHECK(f.last_write(Medula::WD_KICK) > ic, "wd_kick escreve WD_KICK");
        m.wd_disable();
        CHECK(f.log[f.last_write(Medula::WD_CTRL)].val == 0u, "wd_disable: WD_CTRL = 0");
        CHECK(!m.wd_tripped(), "wd_tripped falso");
        f.reg[Medula::STATUS] = Medula::ST_WD_TRIPPED;
        CHECK(m.wd_tripped(), "wd_tripped lê STATUS bit4");
    }
    // read_result: desempacota; reflex só na leitura em que REFLEX_COUNT mudou
    {
        FakeMedula f; Medula m;
        f.reg[Medula::REFLEX_COUNT] = 3;
        CHECK(m.attach(&f), "attach");
        f.reg[Medula::STATUS] = Medula::ST_DONE | Medula::ST_TARGET;
        f.reg[Medula::TARGET_XY] = 0x02273746u;
        f.reg[Medula::TARGET_DEPTH] = 0x3232u;
        f.reg[Medula::BBOX] = 0x454F293Du;
        f.reg[Medula::LATENCY] = 19300u;
        f.reg[Medula::HEAD_TARGET] = 0x05F205B2u;
        medula::Result r;
        CHECK(m.read_result(&r), "read_result");
        CHECK(r.target && r.count == 551 && r.cy == 55 && r.cx == 70, "TARGET_XY → count 551, cy 55, cx 70");
        CHECK(r.min_depth == 50 && r.mean_depth == 50, "TARGET_DEPTH → 50/50");
        CHECK(r.x0 == 61 && r.y0 == 41 && r.x1 == 79 && r.y1 == 69, "BBOX → (61,41,79,69)");
        CHECK(r.latency_cycles == 19300u, "LATENCY");
        CHECK(r.pan_target_us == 1458 && r.tilt_target_us == 1522, "HEAD_TARGET → 1458/1522");
        CHECK(!r.reflex, "reflex falso: REFLEX_COUNT igual ao do attach");
        f.reg[Medula::REFLEX_COUNT] = 4;
        CHECK(m.read_result(&r) && r.reflex, "reflex verdadeiro: REFLEX_COUNT mudou");
        CHECK(m.read_result(&r) && !r.reflex, "reflex falso na leitura seguinte");
        f.reg[Medula::STATUS] = Medula::ST_DONE;
        CHECK(m.read_result(&r) && !r.target, "target falso sem STATUS.TARGET");
        CHECK(f.writes_total == 0, "read_result não escreve");
    }
    // background(): byte p%4 da palavra BG_WORD0 + p/4
    {
        FakeMedula f; Medula m;
        CHECK(m.attach(&f), "attach");
        f.bg[2] = 0x44332211u;                       // pixels 8..11
        CHECK(m.background(8) == 0x11 && m.background(9) == 0x22 && m.background(10) == 0x33 &&
              m.background(11) == 0x44, "background: byte p%4 da palavra p/4");
        CHECK(m.background(-1) == 0 && m.background(medula::N) == 0, "background fora da faixa = 0");
    }
    // as constantes do contrato
    CHECK(qsys::MEDULA_BASE == 0x20000u && qsys::MEDULA_SPAN == 0x20000u, "MEDULA_BASE/SPAN");
    CHECK(Medula::ID_VALUE == 0x4D454431u && Medula::FRAME_WORD0 == 0x2000u && Medula::BG_WORD0 == 0x4000u,
          "ID e faixas");

    std::printf("medula: %d falhas\n", fails);
    return fails ? 1 : 0;
}
