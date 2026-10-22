// Teste de host do rosto (S04): composição pixel-exata contra tests/ref/*.ppm
// (gerados por tools/face/render.py), quebra de linha do balão, dirty-rect,
// piscar e timeout. Roda a partir de firmware/ (make -C firmware test) ou da
// raiz do repositório.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "face/face.h"

using namespace aracno::face;

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { \
    std::printf("FALHA %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); \
    std::printf("\n"); ++g_fail; } } while (0)

static Face g_face;  // 150 kB: fora da pilha

// ---- PPM ------------------------------------------------------------------

static unsigned char g_ppm[CANVAS_W * CANVAS_H * 3];

static bool read_ppm(const char* name) {
    static const char* dirs[] = { "../tests/ref/", "tests/ref/" };
    FILE* f = 0;
    char path[256];
    for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]) && !f; ++i) {
        std::snprintf(path, sizeof path, "%s%s", dirs[i], name);
        f = std::fopen(path, "rb");
    }
    if (!f) { std::printf("não abriu %s\n", name); return false; }
    int w = 0, h = 0, maxv = 0;
    char magic[3] = { 0, 0, 0 };
    if (std::fscanf(f, "%2s %d %d %d", magic, &w, &h, &maxv) != 4 ||
        std::strcmp(magic, "P6") != 0 || w != CANVAS_W || h != CANVAS_H || maxv != 255) {
        std::printf("cabeçalho inválido em %s\n", name);
        std::fclose(f);
        return false;
    }
    std::fgetc(f);  // único whitespace após maxval
    size_t n = std::fread(g_ppm, 1, sizeof g_ppm, f);
    std::fclose(f);
    return n == sizeof g_ppm;
}

static void rgb565_to_888(uint16_t v, unsigned char* out) {
    unsigned r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
    out[0] = static_cast<unsigned char>((r << 3) | (r >> 2));
    out[1] = static_cast<unsigned char>((g << 2) | (g >> 4));
    out[2] = static_cast<unsigned char>((b << 3) | (b >> 2));
}

// Compara o canvas com o PPM; devolve número de pixels diferentes.
static int diff_canvas(const Canvas& cv) {
    int bad = 0, first_x = -1, first_y = -1;
    for (int y = 0; y < CANVAS_H; ++y) {
        for (int x = 0; x < CANVAS_W; ++x) {
            unsigned char rgb[3];
            rgb565_to_888(cv.at(x, y), rgb);
            const unsigned char* ref = g_ppm + (y * CANVAS_W + x) * 3;
            if (std::memcmp(rgb, ref, 3) != 0) {
                if (bad == 0) { first_x = x; first_y = y; }
                ++bad;
            }
        }
    }
    if (bad) std::printf("  %d pixels diferentes, primeiro em (%d,%d)\n", bad, first_x, first_y);
    return bad;
}

// Avança o relógio em passos de 10 ms até o estado de piscar desejado.
static uint32_t drive_blink(Face& f, uint32_t t, bool want) {
    for (int i = 0; i < 1000 && f.blinking() != want; ++i) { t += 10; f.tick(t); }
    return t;
}

struct RefCase { const char* file; Expr e; int frame; uint8_t intensity; const char* phrase; };

static uint32_t g_t = 0;  // relógio simulado, monotônico entre os testes

static void test_refs() {
    static const RefCase cases[] = {
        { "face_neutro_0.ppm",      NEUTRO,      0, 100, "" },
        { "face_neutro_1.ppm",      NEUTRO,      1, 100, "" },
        { "face_alegria_0.ppm",     ALEGRIA,     0, 100, "" },
        { "face_alegria_1.ppm",     ALEGRIA,     1, 100, "" },
        { "face_medo_0.ppm",        MEDO,        0, 100, "" },
        { "face_medo_1.ppm",        MEDO,        1, 100, "" },
        { "face_raiva_0.ppm",       RAIVA,       0, 100, "" },
        { "face_raiva_1.ppm",       RAIVA,       1, 100, "" },
        { "face_surpresa_0.ppm",    SURPRESA,    0, 100, "" },
        { "face_surpresa_1.ppm",    SURPRESA,    1, 100, "" },
        { "face_tristeza_0.ppm",    TRISTEZA,    0, 100, "" },
        { "face_tristeza_1.ppm",    TRISTEZA,    1, 100, "" },
        { "face_curiosidade_0.ppm", CURIOSIDADE, 0, 100, "" },
        { "face_curiosidade_1.ppm", CURIOSIDADE, 1, 100, "" },
        { "face_alegria_i50_0.ppm", ALEGRIA,     0,  50, "" },
        { "balao_1.ppm", NEUTRO, 0, 100, "Olá, eu sou a Jorge" },
        { "balao_2.ppm", MEDO,   0, 100,
          "Está escuro aqui... vou me esconder e esperar você chegar perto!" },
        { "balao_3.ppm", RAIVA,  0, 100,
          "Pneumoultramicroscopicossilicovulcanoconiótico! Não cabe." },
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const RefCase& c = cases[i];
        std::printf("ref %s\n", c.file);
        if (!read_ppm(c.file)) { ++g_fail; continue; }
        g_face.set(c.e, c.intensity);
        g_face.say(c.phrase, 0);
        g_t = drive_blink(g_face, g_t, c.frame == 1);
        CHECK(g_face.blinking() == (c.frame == 1), "não chegou ao quadro %d", c.frame);
        CHECK(diff_canvas(g_face.canvas()) == 0, "%s difere da referência", c.file);
    }
}

// ---- quebra de linha --------------------------------------------------------

static void test_wrap() {
    struct W { const char* in; int n; const char* l[3]; };
    static const W cases[] = {
        { "Olá, eu sou a Jorge", 1, { "Olá, eu sou a Jorge", "", "" } },
        { "", 0, { "", "", "" } },
        { "   espaços   múltiplos   no   começo e fim   ", 1,
          { "espaços múltiplos no começo e fim", "", "" } },
        { "Está escuro aqui... vou me esconder e esperar você chegar perto!", 2,
          { "Está escuro aqui... vou me esconder e", "esperar você chegar perto!", "" } },
        { "Pneumoultramicroscopicossilicovulcanoconiótico! Não cabe.", 2,
          { "Pneumoultramicroscopicossilicovulcanoc", "oniótico! Não cabe.", "" } },
        { "aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeeeffffffffffgggggggggghhhhhhhhhhiiiiiiiiii", 3,
          { "aaaaaaaaaabbbbbbbbbbccccccccccdddddddd", "ddeeeeeeeeeeffffffffffgggggggggghhhhhh",
            "hhhhiiiiiiiiii" } },
        { "uma frase bem longa que precisa de mais de três linhas para caber inteira no balão e por isso será cortada no fim", 3,
          { "uma frase bem longa que precisa de", "mais de três linhas para caber inteira",
            "no balão e por isso será cortada no" } },
        { "ÁÉÊÍÓÚÇ áàâãéêíóôõúç 1234567890123456789012345", 2,
          { "ÁÉÊÍÓÚÇ áàâãéêíóôõúç", "1234567890123456789012345", "" } },
        { "12345678901234567890123456789012345678 x", 2,
          { "12345678901234567890123456789012345678", "x", "" } },
        { "1234567890123456789012345678901234567 xy", 2,
          { "1234567890123456789012345678901234567", "xy", "" } },
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char out[BALLOON_LINES][BALLOON_LINE_BYTES];
        int n = wrap_lines(cases[i].in, out);
        CHECK(n == cases[i].n, "frase %u: %d linhas, esperava %d", i, n, cases[i].n);
        for (int k = 0; k < n && k < cases[i].n; ++k)
            CHECK(std::strcmp(out[k], cases[i].l[k]) == 0,
                  "frase %u linha %d: '%s' != '%s'", i, k, out[k], cases[i].l[k]);
    }
}

// ---- dirty-rect, piscar, timeout --------------------------------------------

static bool rect_eq(const Rect& r, int x, int y, int w, int h) {
    return r.x == x && r.y == y && r.w == w && r.h == h;
}

static void test_dirty_and_timing() {
    Face& f = g_face;
    Rect r;
    f.set(NEUTRO, 100);
    CHECK(f.dirty(&r) && rect_eq(r, 0, 0, CANVAS_W, CANVAS_H), "set() deve sujar o quadro inteiro");
    f.flushed();
    CHECK(!f.dirty(&r), "flushed() deve limpar");
    // alinha o relógio logo após uma piscada: próxima só daqui a ≥ 3 s
    g_t = drive_blink(f, g_t, true);
    g_t = drive_blink(f, g_t, false);
    f.flushed();

    f.say("oi", 1000);
    CHECK(f.dirty(&r) && rect_eq(r, 0, BALLOON_Y, CANVAS_W, BALLOON_H), "say() deve sujar só o balão");
    f.flushed();

    // 1º tick após say() fixa o prazo: timeout em t0+1000
    uint32_t t0 = g_t + 10;
    f.tick(t0);
    CHECK(f.lines() == 1 && !f.dirty(&r), "nada muda no 1º tick");
    f.tick(t0 + 999);
    CHECK(f.lines() == 1, "balão ainda visível antes do timeout");
    f.tick(t0 + 1000);
    CHECK(f.lines() == 0, "balão some no timeout");
    CHECK(f.dirty(&r) && rect_eq(r, 0, BALLOON_Y, CANVAS_W, BALLOON_H), "timeout suja só o balão");
    f.flushed();
    for (int y = BALLOON_Y; y < CANVAS_H; ++y)
        for (int x = 0; x < CANVAS_W; ++x)
            if (f.canvas().at(x, y) != 0) { CHECK(false, "balão não ficou preto em (%d,%d)", x, y); y = CANVAS_H; break; }

    // piscar: região só dos olhos (dentro do rosto, estreita)
    uint32_t t = drive_blink(f, t0 + 1000, true);
    CHECK(f.blinking(), "deveria piscar em até 10 s");
    CHECK(f.dirty(&r), "piscar deve sujar");
    CHECK(r.y + r.h <= FACE_H && r.h <= 60 && r.w <= 200 && r.x > 40,
          "dirty do piscar fora do esperado: %d,%d %ux%u", r.x, r.y, r.w, r.h);
    f.flushed();
    t = drive_blink(f, t, false);
    CHECK(!f.blinking() && f.dirty(&r) && r.y + r.h <= FACE_H, "fim do piscar deve sujar os olhos");
    f.flushed();

    // intervalos entre piscadas em 3–5 s e duração BLINK_MS, por 60 s
    uint32_t last_open = t, blink_start = 0;
    int n_blinks = 0;
    for (uint32_t now = t + 10; now < t + 60000; now += 10) {
        bool was = f.blinking();
        f.tick(now);
        if (!was && f.blinking()) {
            uint32_t gap = now - last_open;
            CHECK(gap >= BLINK_MIN_GAP_MS && gap <= BLINK_MAX_GAP_MS + 10,
                  "intervalo entre piscadas %u ms fora de 3–5 s", gap);
            blink_start = now;
            ++n_blinks;
        } else if (was && !f.blinking()) {
            uint32_t dur = now - blink_start;
            CHECK(dur >= BLINK_MS && dur <= BLINK_MS + 10, "piscada durou %u ms", dur);
            last_open = now;
        }
    }
    CHECK(n_blinks >= 10 && n_blinks <= 21, "%d piscadas em 60 s", n_blinks);

    // say() sem timeout persiste; set() mantém o texto e troca a cor
    f.say("fica", 0);
    for (uint32_t now = t + 60000; now < t + 90000; now += 100) f.tick(now);
    CHECK(f.lines() == 1 && std::strcmp(f.line(0), "fica") == 0, "say(ms=0) não pode expirar");
    f.flushed();
    f.set(RAIVA, 30);
    CHECK(f.dirty(&r) && rect_eq(r, 0, 0, CANVAS_W, CANVAS_H), "troca de expressão suja tudo");
    CHECK(f.lines() == 1, "set() não apaga o balão");
    // RAIVA = (255,40,40) → RGB565 0xF945 (render.py PARAMS)
    CHECK(f.canvas().at(0, BALLOON_Y) == 0xF945, "contorno do balão deve seguir a cor do humor");
}

// ---- Canvas básico ---------------------------------------------------------

static void test_canvas() {
    static Canvas cv;
    cv.clear(0x1234);
    CHECK(cv.at(0, 0) == 0x1234 && cv.at(319, 239) == 0x1234, "clear");
    cv.clear(0);
    cv.rect(-5, -5, 10, 10, 0xFFFF, true);     // recorte
    CHECK(cv.at(4, 4) == 0xFFFF && cv.at(5, 5) == 0, "rect recortado");
    cv.rect(10, 10, 4, 3, 0x07E0, false);
    CHECK(cv.at(10, 10) == 0x07E0 && cv.at(13, 12) == 0x07E0 && cv.at(11, 11) == 0, "contorno");
    static const uint16_t px[4] = { 1, 2, 3, 4 };
    Sprite s = { 0, 0, 2, 2, px };
    cv.blit(s, 318, 238);
    CHECK(cv.at(318, 238) == 1 && cv.at(319, 239) == 4, "blit no canto");
    cv.blit(s, 319, 239);
    CHECK(cv.at(319, 239) == 1, "blit recortado à direita/baixo");
    cv.blit_scaled(s, 100, 100, 50);           // 1×1 centrado: (100,100)
    CHECK(cv.at(100, 100) == 1 && cv.at(101, 101) == 0, "blit_scaled 50 %%");
    cv.text(0, 0, "A", 0xFFFF);
    int lit = 0;
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 8; ++x) if (cv.at(x, y) == 0xFFFF) ++lit;
    CHECK(lit > 10 && lit < 80, "glifo 'A' com %d pixels", lit);
    // UTF-8: acento conhecido ≠ '?', byte inválido → '?'
    cv.clear(0);
    cv.text(0, 0, "ç", 0xFFFF);
    int lit_c = 0;
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 8; ++x) if (cv.at(x, y)) ++lit_c;
    cv.clear(0);
    cv.text(0, 0, "?", 0xFFFF);
    int lit_q = 0;
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 8; ++x) if (cv.at(x, y)) ++lit_q;
    cv.clear(0);
    cv.text(0, 0, "\xFF", 0xFFFF);
    int lit_bad = 0;
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 8; ++x) if (cv.at(x, y)) ++lit_bad;
    CHECK(lit_c > 0 && lit_c != lit_q, "'ç' tem glifo próprio");
    CHECK(lit_bad == lit_q, "UTF-8 inválido vira '?'");
    const char* p = "é";
    CHECK(utf8_next(&p) == 0xE9 && *p == 0, "utf8_next");
}

int main() {
    test_canvas();
    test_wrap();
    test_refs();
    test_dirty_and_timing();
    if (g_fail) { std::printf("face: %d falha(s)\n", g_fail); return 1; }
    std::printf("face: ok\n");
    return 0;
}
