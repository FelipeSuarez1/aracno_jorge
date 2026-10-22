// Tabela de casos da camada de segurança (S03). Cada caso: observação → comando
// desejado → comando permitido + motivo + abort. Limites: Limits() padrão
// (d_stop 300, f_edge 120, tilt 600, stale 200).
#include <cstdio>
#include <cstring>
#include "aracno/types.h"
#include "safety/safety.h"

using namespace aracno;
using namespace aracno::safety;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FALHA linha %d: %s\n", __LINE__, msg); ++fails; } } while (0)

static Obs fresh_all() {
    Obs o;
    std::memset(&o, 0, sizeof o);
    o.t_ms = 100000;
    o.lux_l = o.lux_r = 100;
    o.dist_front_mm = 1500;
    o.dist_floor_mm = 60;      // chão a 6 cm
    o.pir = 0; o.estop = 0;
    o.acc_x_mg = 0; o.acc_y_mg = 0; o.acc_z_mg = 1000;
    o.t_since_stimulus_ms = 5000;
    o.installed = 0x7F;        // todos
    for (int s = 0; s < N_SENSORS; ++s) o.age_ms[s] = 20;
    return o;
}

static Cmd cmd(Action a, uint8_t speed = 50) { Cmd c; c.action = static_cast<uint8_t>(a); c.speed = speed; return c; }

struct Case {
    const char* name;
    Obs obs;
    Cmd desired;
    Action expect_action;
    uint8_t expect_reason;
    bool expect_abort;
};

int main() {
    Obs base = fresh_all();

    // --- casos sem restrição --------------------------------------------------
    Case cases[64];
    int n = 0;
    {
        Action free_actions[] = {STOP, FORWARD, BACKWARD, TURN_LEFT, TURN_RIGHT, CROUCH, LUNGE, HEAD_LEFT, HEAD_RIGHT};
        for (unsigned i = 0; i < sizeof(free_actions) / sizeof(free_actions[0]); ++i) {
            Case c = {"livre", base, cmd(free_actions[i]), free_actions[i], RS_OK, false};
            cases[n++] = c;
        }
    }
    // --- obstáculo à frente -------------------------------------------------------
    {
        Obs o = base; o.dist_front_mm = 299;
        Case c1 = {"obstáculo 299: avançar vira parar", o, cmd(FORWARD), STOP, RS_OBSTACLE, false}; cases[n++] = c1;
        Case c2 = {"obstáculo 299: bote vira parar", o, cmd(LUNGE), STOP, RS_OBSTACLE, false}; cases[n++] = c2;
        Case c3 = {"obstáculo 299: recuar permitido", o, cmd(BACKWARD), BACKWARD, RS_OK, false}; cases[n++] = c3;
        Case c4 = {"obstáculo 299: virar permitido", o, cmd(TURN_LEFT), TURN_LEFT, RS_OK, false}; cases[n++] = c4;
        Case c5 = {"obstáculo 299: cabeça permitida", o, cmd(HEAD_RIGHT), HEAD_RIGHT, RS_OK, false}; cases[n++] = c5;
        Obs o2 = base; o2.dist_front_mm = 300;
        Case c6 = {"limite exato 300: avançar permitido", o2, cmd(FORWARD), FORWARD, RS_OK, false}; cases[n++] = c6;
        Obs o3 = base; o3.dist_front_mm = 0;   // inválido = sem obstáculo conhecido
        Case c7 = {"frontal inválido (0): avançar permitido", o3, cmd(FORWARD), FORWARD, RS_OK, false}; cases[n++] = c7;
    }
    // --- borda ----------------------------------------------------------------------
    {
        Obs o = base; o.dist_floor_mm = 121;
        Case c1 = {"borda 121: avançar vira parar", o, cmd(FORWARD), STOP, RS_EDGE, false}; cases[n++] = c1;
        Case c2 = {"borda: bote vira parar", o, cmd(LUNGE), STOP, RS_EDGE, false}; cases[n++] = c2;
        Case c3 = {"borda: virar vira parar", o, cmd(TURN_RIGHT), STOP, RS_EDGE, false}; cases[n++] = c3;
        Case c4 = {"borda: recuar permitido", o, cmd(BACKWARD), BACKWARD, RS_OK, false}; cases[n++] = c4;
        Case c5 = {"borda: agachar permitido", o, cmd(CROUCH), CROUCH, RS_OK, false}; cases[n++] = c5;
        Obs o2 = base; o2.dist_floor_mm = 120;
        Case c6 = {"chão exato 120: avançar permitido", o2, cmd(FORWARD), FORWARD, RS_OK, false}; cases[n++] = c6;
        Obs o3 = base; o3.dist_floor_mm = 0;   // ToF de chão instalado mas sem leitura válida = borda
        Case c7 = {"chão inválido (0): avançar vira parar", o3, cmd(FORWARD), STOP, RS_EDGE, false}; cases[n++] = c7;
        Obs o4 = base; o4.dist_floor_mm = 0; o4.installed = static_cast<uint8_t>(0x7F & ~(1u << S_DIST_FLOOR)); o4.age_ms[S_DIST_FLOOR] = AGE_NEVER;
        Case c8 = {"sem ToF de chão instalado: regra de borda não se aplica", o4, cmd(FORWARD), FORWARD, RS_OK, false}; cases[n++] = c8;
    }
    // --- sensor obsoleto --------------------------------------------------------------
    {
        Obs o = base; o.age_ms[S_DIST_FRONT] = 201;
        Case c1 = {"frontal obsoleto 201 ms: parar + abort", o, cmd(FORWARD), STOP, RS_STALE, true}; cases[n++] = c1;
        Obs o2 = base; o2.age_ms[S_DIST_FRONT] = 200;
        Case c2 = {"frontal 200 ms (limite): ok", o2, cmd(FORWARD), FORWARD, RS_OK, false}; cases[n++] = c2;
        Obs o3 = base; o3.age_ms[S_LUX_L] = AGE_NEVER;
        Case c3 = {"lux nunca lido mas instalado: parar + abort", o3, cmd(STOP), STOP, RS_STALE, true}; cases[n++] = c3;
        Obs o4 = base; o4.installed = static_cast<uint8_t>(0x7F & ~(1u << S_LUX_L)); o4.age_ms[S_LUX_L] = AGE_NEVER;
        Case c4 = {"lux não instalado: ignorado", o4, cmd(FORWARD), FORWARD, RS_OK, false}; cases[n++] = c4;
        Obs o5 = base; o5.age_ms[S_ACCEL] = 5000;
        Case c5 = {"acelerômetro obsoleto: parar + abort mesmo para STOP", o5, cmd(STOP), STOP, RS_STALE, true}; cases[n++] = c5;
        Obs o6 = base; o6.age_ms[S_PIR] = 1000;
        Case c6 = {"PIR obsoleto: parar + abort", o6, cmd(HEAD_LEFT), STOP, RS_STALE, true}; cases[n++] = c6;
    }
    // --- tombo: um único ciclo inclinado NÃO dispara (persistência testada em cenário abaixo) ----
    {
        Obs o = base; o.acc_x_mg = 601;
        Case c1 = {"inclinação x 601 por 1 ciclo: ainda ok", o, cmd(FORWARD), FORWARD, RS_OK, false}; cases[n++] = c1;
        Obs o3 = base; o3.acc_x_mg = 600;
        Case c3 = {"inclinação 600 (limite): ok", o3, cmd(FORWARD), FORWARD, RS_OK, false}; cases[n++] = c3;
    }
    // --- prioridade entre regras -----------------------------------------------------------
    {
        Obs o = base; o.dist_front_mm = 100; o.dist_floor_mm = 500;
        Case c1 = {"obstáculo + borda: motivo é borda (mais grave)", o, cmd(FORWARD), STOP, RS_EDGE, false}; cases[n++] = c1;
        Obs o3 = base; o3.acc_x_mg = 900; o3.age_ms[S_LUX_R] = 999;
        Case c3 = {"tombo + obsoleto: obsoleto vence (ambos abort)", o3, cmd(STOP), STOP, RS_STALE, true}; cases[n++] = c3;
    }
    // --- velocidade -----------------------------------------------------------------------------
    {
        Case c1 = {"velocidade preservada", base, cmd(FORWARD, 80), FORWARD, RS_OK, false}; cases[n++] = c1;
    }

    Safety s;
    for (int i = 0; i < n; ++i) {
        Safety fresh;  // cada caso independente (sem trava)
        Verdict v = fresh.filter(cases[i].obs, cases[i].desired);
        bool ok = v.cmd.action == cases[i].expect_action && v.reason == cases[i].expect_reason && v.abort == cases[i].expect_abort;
        if (!ok) {
            std::printf("FALHA caso %d (%s): esperado acao=%d motivo=%s abort=%d; obtido acao=%d motivo=%s abort=%d\n",
                        i, cases[i].name, cases[i].expect_action, reason_name(cases[i].expect_reason), cases[i].expect_abort,
                        v.cmd.action, reason_name(v.reason), v.abort);
            ++fails;
        }
        if (ok && v.reason == RS_OK) CHECK(v.cmd.speed == cases[i].desired.speed, "velocidade alterada sem motivo");
    }

    // --- botão de parada e trava -----------------------------------------------------------------
    {
        Obs o = base; o.estop = 1;
        Verdict v = s.filter(o, cmd(FORWARD));
        CHECK(v.cmd.action == STOP && v.reason == RS_ESTOP && v.abort, "estop: parar + abort");
        CHECK(s.latched(), "estop trava");
        Obs ok = base;
        v = s.filter(ok, cmd(FORWARD));
        CHECK(v.cmd.action == STOP && v.reason == RS_LATCHED && v.abort, "travado: continua parado após o botão soltar");
        s.reset_latch();
        v = s.filter(ok, cmd(FORWARD));
        CHECK(v.cmd.action == FORWARD && v.reason == RS_OK && !v.abort, "destravado pelo operador: volta a andar");
    }
    // tombo: precisa de fall_persist (10) ciclos seguidos; um tranco de 9 ciclos não trava
    {
        Safety t;
        Obs o = base; o.acc_y_mg = 800;
        Verdict v;
        for (int i = 0; i < 9; ++i) v = t.filter(o, cmd(FORWARD));
        CHECK(v.cmd.action == FORWARD && !t.latched(), "9 ciclos inclinada: ainda não é tombo");
        v = t.filter(base, cmd(FORWARD));
        CHECK(!t.latched(), "voltou ao nível: contador zera");
        for (int i = 0; i < 10; ++i) v = t.filter(o, cmd(FORWARD));
        CHECK(v.cmd.action == STOP && v.reason == RS_FALL && v.abort, "10 ciclos inclinada: tombo + abort");
        CHECK(t.latched() && t.latch_reason() == RS_FALL, "tombo trava com motivo RS_FALL");
        v = t.filter(base, cmd(FORWARD));
        CHECK(v.cmd.action == STOP && v.reason == RS_LATCHED && v.abort, "após tombo continua travado (a aranha caída não levanta sozinha)");
        // borda + tombo persistente: tombo vence
        Safety t2;
        Obs o2 = base; o2.dist_floor_mm = 500; o2.acc_x_mg = 900;
        for (int i = 0; i < 10; ++i) v = t2.filter(o2, cmd(BACKWARD));
        CHECK(v.cmd.action == STOP && v.reason == RS_FALL && v.abort, "borda + tombo: tombo vence (abort)");
    }
    // obsoleto NÃO trava: assim que o sensor volta, a aranha pode seguir
    {
        Safety t;
        Obs o = base; o.age_ms[S_DIST_FRONT] = 900;
        t.filter(o, cmd(FORWARD));
        CHECK(!t.latched(), "obsoleto não trava");
        Verdict v = t.filter(base, cmd(FORWARD));
        CHECK(v.cmd.action == FORWARD && v.reason == RS_OK, "sensor voltou: libera");
    }
    // sem nenhum sensor instalado (bancada): tudo permitido, nada obsoleto
    {
        Safety t;
        Obs o; std::memset(&o, 0, sizeof o); o.installed = 0;
        for (int k = 0; k < N_SENSORS; ++k) o.age_ms[k] = AGE_NEVER;
        Verdict v = t.filter(o, cmd(FORWARD));
        CHECK(v.cmd.action == FORWARD && v.reason == RS_OK && !v.abort, "bancada sem sensores: permitido");
    }

    std::printf("safety: %d casos de tabela + cenários de trava, %d falhas\n", n, fails);
    return fails ? 1 : 0;
}
