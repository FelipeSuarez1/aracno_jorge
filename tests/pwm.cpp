// Conversão ângulo→HIGH_DUR igual ao CMotor da Terasic, divisor de velocidade,
// tempo de rampa do IP, tabelas de marcha e o avanço não bloqueante do Spider
// contra um banco de registradores simulado (modela o IP TERASIC_PWM_EX).
#include <cstdio>
#include <cstring>
#include "hal/lwbridge.h"
#include "motor/pwm.h"
#include "motor/spider.h"

using namespace aracno;
using namespace aracno::motor;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FALHA linha %d: %s\n", __LINE__, msg); ++fails; } } while (0)

// Modelo do IP: 18 canais × 4 registradores; HIGH_DUR alvo vs. high_dur_temp que
// "chega" quando o teste chama settle(); STATUS = (alvo == temp).
struct FakeRegs : public hal::RegIo {
    uint32_t reg[18][4];
    uint32_t temp[18];
    uint32_t writes;
    FakeRegs() : writes(0) { std::memset(reg, 0, sizeof reg); for (int i = 0; i < 18; ++i) temp[i] = 0; }
    static int ch(uint32_t base) { return static_cast<int>((0x110u - base) / 0x10u); }
    virtual void write32(uint32_t base, uint32_t idx, uint32_t v) { reg[ch(base)][idx] = v; ++writes; }
    virtual uint32_t read32(uint32_t base, uint32_t idx) const {
        int c = ch(base);
        if (idx == 2) return reg[c][1] == temp[c] ? 1u : 0u;   // STATUS
        return reg[c][idx];
    }
    void settle() { for (int i = 0; i < 18; ++i) temp[i] = reg[i][1]; }
    void settle(int c) { temp[c] = reg[c][1]; }
};

int main() {
    // --- ângulo → HIGH_DUR (CMotor.cpp: 0,5 ms=25000 ↔ −90°, 1,5 ms=75000 ↔ 0°, 2,5 ms=125000 ↔ +90°)
    CHECK(PwmBank::angle_to_high_dur(-90.f, false) == 25000, "-90 → 25000");
    CHECK(PwmBank::angle_to_high_dur(0.f, false) == 75000, "0 → 75000");
    CHECK(PwmBank::angle_to_high_dur(90.f, false) == 125000, "+90 → 125000");
    CHECK(PwmBank::angle_to_high_dur(45.f, false) == 100000, "+45 → 100000");
    CHECK(PwmBank::angle_to_high_dur(45.f, true) == 50000, "espelhado +45 → 50000 (= -45)");
    CHECK(PwmBank::angle_to_high_dur(200.f, false) == 125000, "satura em +90");
    CHECK(PwmBank::angle_to_high_dur(-200.f, false) == 25000, "satura em -90");
    CHECK(PwmBank::angle_to_high_dur(-200.f, true) == 125000, "espelhado satura em +90");

    // --- divisor (CMotor::SetSpeed): speed 0 → 1150, 50 → 850, 100 → 550; >100 satura em 99 → 556
    CHECK(PwmBank::divider_for_speed(0) == 1150, "speed 0 → 1150");
    CHECK(PwmBank::divider_for_speed(50) == 850, "speed 50 → 850");
    CHECK(PwmBank::divider_for_speed(100) == 550, "speed 100 → 550");
    CHECK(PwmBank::divider_for_speed(101) == 556, "speed 101 satura como 99 → 556");
    CHECK(PwmBank::divider_for_speed(-5) == 1150, "speed negativo → 1150");

    // --- rampa: Δ ticks × (div+1) / 50 MHz. 45° = 25000 ticks; div 850 → 425,5 ms
    CHECK(PwmBank::ramp_ms(75000, 100000, 850) == 425, "45° a speed 50 ≈ 425 ms");
    CHECK(PwmBank::ramp_ms(100000, 75000, 850) == 425, "rampa simétrica");
    CHECK(PwmBank::ramp_ms(25000, 125000, 550) == 1102, "180° a div 550 ≈ 1,1 s");
    CHECK(PwmBank::ramp_ms(70000, 70000, 850) == 0, "sem movimento → 0");

    // --- base dos canais (hps_0.h): PWM0 em 0x110, PWM17 em 0x0
    CHECK(PwmBank::base_of(0) == 0x110 && PwmBank::base_of(17) == 0x0 && PwmBank::base_of(9) == 0x80, "bases");

    // --- init escreve TOTAL_DUR, divisor padrão 1700, ABORT 0; NÃO escreve HIGH_DUR (não arma);
    //     semeia o alvo conhecido lendo high_dur_temp (canal 5 "deixado" a +45° por outro processo).
    FakeRegs regs;
    regs.reg[5][1] = 100000; regs.temp[5] = 100000;
    PwmBank pwm;
    pwm.init(&regs);
    for (int i = 0; i < 18; ++i) {
        CHECK(regs.reg[i][0] == 1000000, "TOTAL_DUR = 1 000 000");
        CHECK(regs.reg[i][2] == 1700, "ADJ_SPEED padrão 1700");
        CHECK(regs.reg[i][3] == 0, "ABORT 0");
        if (i != 5) CHECK(regs.reg[i][1] == 0, "init não escreve HIGH_DUR");
    }
    CHECK(pwm.target_deg(5) == -45.0f, "semente: canal 5 (espelhado) lido como -45°");
    CHECK(pwm.target_deg(9) == 0.0f, "semente: canal nunca escrito = 0°");
    // ETA do primeiro movimento usa a semente: 45° de rampa a 1700 → ~850 ms
    pwm.move(5, 0.f, 0);
    CHECK(pwm.eta_ms(5) > 800 && pwm.eta_ms(5) < 1000, "ETA a partir da posição real");

    // --- move espelha motores 0–8 e não os 9–17
    pwm.move(0, 30.f, 1000);
    pwm.move(9, 30.f, 1000);
    CHECK(regs.reg[0][1] == PwmBank::angle_to_high_dur(30.f, true), "motor 0 espelhado");
    CHECK(regs.reg[9][1] == PwmBank::angle_to_high_dur(30.f, false), "motor 9 direto");
    CHECK(!pwm.ready(0), "não pronto antes de chegar");
    regs.settle();
    CHECK(pwm.ready(0) && pwm.ready(9), "pronto após chegar");
    pwm.abort_all(true);
    for (int i = 0; i < 18; ++i) CHECK(regs.reg[i][3] == 1, "ABORT em todos");
    pwm.abort_all(false);

    // --- tabelas de marcha
    Step seq[Spider::MAX_STEPS];
    int n = Spider::build(M_FORWARD, seq);
    CHECK(n == 6, "MoveForward tem 6 passos");
    CHECK(seq[0].n == 3 && seq[1].n == 6 && seq[2].n == 3, "passos 1-3 do ciclo: 3, 6, 3 motores");
    // passo 1: tripé 1 (RF, LM, RB) joelhos a 60
    CHECK(seq[0].a[0].motor == 1 && seq[0].a[0].deg == 60, "RF joelho 60");
    CHECK(seq[0].a[1].motor == 13 && seq[0].a[1].deg == 60, "LM joelho 60");
    CHECK(seq[0].a[2].motor == 7 && seq[0].a[2].deg == 60, "RB joelho 60");
    // passo 2: quadris tripé 1 = base+20 (RF -20+20=0, LM 0+20=20, RB 20+20=40); tripé 2 = base-20
    CHECK(seq[1].a[0].motor == 0 && seq[1].a[0].deg == 0, "RF quadril 0");
    CHECK(seq[1].a[3].motor == 9 && seq[1].a[3].deg == -40, "LF quadril -40");
    int nb = Spider::build(M_BACKWARD, seq);
    CHECK(nb == 6 && seq[1].a[0].deg == -40 && seq[1].a[3].deg == 0, "backward inverte quadris");
    int ns = Spider::build(M_STAND, seq);
    CHECK(ns == 18 + 1 + 10 + 6, "STAND: 18 init + 1 quadris + 10 joelhos + 6 reset");
    CHECK(seq[0].pause_ms == 100 && seq[0].n == 1 && seq[0].a[0].deg == 0, "init: um motor por passo, 100 ms");
    for (Motion m = M_STAND; m < N_MOTIONS; m = static_cast<Motion>(m + 1)) {
        int k = Spider::build(m, seq);
        CHECK(k > 0 && k <= Spider::MAX_STEPS, "toda motion cabe em MAX_STEPS");
        for (int i = 0; i < k; ++i)
            for (int j = 0; j < seq[i].n; ++j) {
                CHECK(seq[i].a[j].motor < 18, "motor < 18");
                CHECK(seq[i].a[j].deg >= -90 && seq[i].a[j].deg <= 90, "ângulo em ±90");
            }
    }
    int nl = Spider::build(M_LUNGE, seq);
    CHECK(nl == 8, "LUNGE = tilt F + 6 + tilt N");

    // --- stepper não bloqueante
    Spider sp;
    sp.attach(&pwm);
    CHECK(!sp.busy(), "ocioso no início");
    uint32_t t = 10000;
    CHECK(sp.start(M_FORWARD, 2, t), "start forward ×2");
    CHECK(sp.busy() && sp.step_index() == 0, "passo 0 emitido");
    CHECK(!sp.start(M_BACKWARD, 1, t), "não aceita segundo start enquanto ocupado");
    sp.tick(t + 20);
    CHECK(sp.step_index() == 0, "sem STATUS não avança");
    regs.settle();
    sp.tick(t + 40);
    CHECK(sp.step_index() == 1, "avança quando pronto");
    int guard = 0;
    while (sp.busy() && guard++ < 100) { regs.settle(); t += 20; sp.tick(t); }
    CHECK(!sp.busy(), "termina os 2 ciclos");
    CHECK(sp.step_timeouts() == 0, "sem timeouts");

    // timeout: motor nunca "chega" → avança após eta+100 ms e conta
    sp.start(M_TILT_F, 1, t);
    uint32_t eta = 0;
    for (int i = 0; i < 18; ++i) if (pwm.eta_ms(i) > eta) eta = pwm.eta_ms(i);
    sp.tick(eta + 99);
    CHECK(sp.busy(), "antes do timeout continua");
    sp.tick(eta + 100);
    CHECK(!sp.busy() && sp.step_timeouts() == 1, "timeout avança e conta");

    // abort_now: ABORT=1 em todos e fica ocioso; wake religa perna a perna (testado adiante)
    regs.settle();
    sp.start(M_FORWARD, 5, t);
    sp.abort_now();
    CHECK(!sp.busy() && sp.aborted(), "abort_now esvazia");
    for (int i = 0; i < 18; ++i) CHECK(regs.reg[i][3] == 1, "ABORT=1 após abort_now");
    sp.wake();
    for (int i = 0; i < 6; ++i) { t += 100; sp.tick(t); }
    CHECK(!sp.aborted(), "religada após 6 ticks de 100 ms");
    for (int i = 0; i < 18; ++i) CHECK(regs.reg[i][3] == 0, "ABORT=0 após wake");

    // LUNGE força speed 100 e restaura
    sp.set_speed(40);
    sp.start(M_LUNGE, 1, t);
    CHECK(sp.speed() == 100 && regs.reg[0][2] == PwmBank::divider_for_speed(100), "bote a 100");
    guard = 0;
    while (sp.busy() && guard++ < 100) { regs.settle(); t += 20; sp.tick(t); }
    CHECK(sp.speed() == 40 && regs.reg[0][2] == PwmBank::divider_for_speed(40), "velocidade restaurada");

    // stop_after_cycle termina ao fim do ciclo corrente
    sp.start(M_FORWARD, 10, t);
    sp.stop_after_cycle();
    guard = 0;
    while (sp.busy() && guard++ < 100) { regs.settle(); t += 20; sp.tick(t); }
    CHECK(guard <= 8, "parou após um ciclo (6 passos)");

    // stop_after_step (veredito da segurança) termina no fim do passo corrente, não do ciclo
    regs.settle();
    sp.start(M_FORWARD, 10, t);
    regs.settle(); t += 20; sp.tick(t);         // passo 0 → 1
    CHECK(sp.step_index() == 1, "no passo 1");
    sp.stop_after_step();
    regs.settle(); t += 20; sp.tick(t);         // passo 1 concluído → termina
    CHECK(!sp.busy(), "parou após o passo corrente");
    CHECK(sp.step_timeouts() == 1, "sem timeouts novos");

    // bote interrompido por stop_after_step restaura a velocidade
    sp.set_speed(30);
    sp.start(M_LUNGE, 1, t);
    sp.stop_after_step();
    regs.settle(); t += 20; sp.tick(t);
    CHECK(!sp.busy() && sp.speed() == 30, "bote cortado restaura velocidade");

    // wake escalonado: uma perna a cada 100 ms; nada parte enquanto religa
    sp.abort_now();
    CHECK(!sp.start(M_FORWARD, 1, t), "abortado: start recusado");
    sp.wake();
    CHECK(sp.aborted() && sp.waking(), "religando");
    sp.tick(t);                                  // perna 0 imediatamente
    CHECK(regs.reg[0][3] == 0 && regs.reg[1][3] == 0 && regs.reg[2][3] == 0 && regs.reg[3][3] == 1, "perna 0 religada, perna 1 ainda não");
    sp.tick(t + 50);
    CHECK(regs.reg[3][3] == 1, "antes de 100 ms não religa a próxima");
    for (int i = 1; i < 6; ++i) { t += 100; sp.tick(t); }
    CHECK(!sp.aborted() && !sp.waking(), "6 pernas religadas");
    for (int i = 0; i < 18; ++i) CHECK(regs.reg[i][3] == 0, "ABORT=0 em todos após wake");
    CHECK(sp.start(M_FORWARD, 1, t), "aceita start após religar");
    guard = 0; while (sp.busy() && guard++ < 100) { regs.settle(); t += 20; sp.tick(t); }

    // rotação após marcha prefixa a pose de reset (6 passos) — CSpider::RotatelLeft chama Reset()
    sp.start(M_ROTATE_L, 1, t);
    CHECK(sp.step_count() == 12, "reset (6) + rotação (6) após marcha");
    guard = 0; while (sp.busy() && guard++ < 100) { regs.settle(); t += 20; sp.tick(t); }
    sp.start(M_ROTATE_R, 1, t);
    CHECK(sp.step_count() == 6, "rotação após rotação: sem reset");
    guard = 0; while (sp.busy() && guard++ < 100) { regs.settle(); t += 20; sp.tick(t); }

    // falha: 3 passos seguidos em timeout → fault, motion encerra; wake limpa
    uint32_t before = sp.step_timeouts();
    for (int k = 0; k < 18; ++k) regs.temp[k] = 1;   // servos "presos": STATUS nunca fica 1
    sp.start(M_FORWARD, 1, t);
    for (int i = 0; i < 3; ++i) {
        uint32_t e = 0; for (int k = 0; k < 18; ++k) if (pwm.eta_ms(k) > e) e = pwm.eta_ms(k);
        t = e + 100; sp.tick(t);
    }
    CHECK(sp.fault() && !sp.busy(), "3 timeouts seguidos → falha");
    CHECK(sp.step_timeouts() == before + 3, "timeouts contados");
    CHECK(!sp.start(M_FORWARD, 1, t), "em falha: start recusado");
    sp.abort_now(); sp.wake(); for (int i = 0; i < 6; ++i) { t += 100; sp.tick(t); }
    CHECK(!sp.fault(), "wake limpa a falha");

    std::printf("pwm/spider: %d falhas\n", fails);
    return fails ? 1 : 0;
}
