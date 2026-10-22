#pragma once
#include <stdint.h>
#include "motor/pwm.h"

namespace aracno { namespace motor {

// Marcha da Terasic (CSpider.cpp) convertida em sequências de passos NÃO bloqueantes:
// cada passo escreve ângulos em até 18 motores e termina quando todos reportam
// STATUS bit0 (ou por timeout derivado da rampa). A camada de segurança pode
// interromper entre passos (stop) ou imediatamente (abort_now → ABORT em todos).
//
// Pernas (CSpider.h LEG_ID): RF=0 RM=1 RB=2 LF=3 LM=4 LB=5; juntas Hip=0 Knee=1 Ankle=2;
// motor = perna*3 + junta (CSpider::CSpider szMotorID). Tripé 1 = RF,LM,RB; tripé 2 = LF,RM,LB.
enum Motion {
    M_NONE = 0,
    M_STAND,        // CSpider::Init + Standup + Reset
    M_FORWARD,      // MoveForward
    M_BACKWARD,     // MoveBackward
    M_ROTATE_L,     // RotatelLeft
    M_ROTATE_R,     // RotatelRight
    M_TILT_F, M_TILT_B, M_TILT_L, M_TILT_R, M_TILT_N,
    M_CROUCH,       // corpo baixo [A AJUSTAR NO LAB]
    M_LUNGE,        // TiltForward + 1 ciclo de MoveForward + TiltNone, velocidade 100
    M_SLEEP_POSE,   // Reset (pose neutra em pé)
    N_MOTIONS
};

struct Assign { uint8_t motor; int8_t deg; };
struct Step {
    uint8_t n;          // nº de atribuições
    Assign a[18];
    uint16_t pause_ms;  // espera extra após ficar pronto (CSpider::Init usa 100 ms)
};

class Spider {
public:
    static const int MAX_STEPS = 48;

    Spider();
    void attach(PwmBank* pwm) { pwm_ = pwm; }
    // Inicia um movimento com `repeats` ciclos. false se já há movimento em curso.
    bool start(Motion m, uint8_t repeats, uint32_t now_ms);
    // Avança a sequência. Chamar a cada ciclo do laço (20 ms).
    void tick(uint32_t now_ms);
    bool busy() const { return motion_ != M_NONE; }
    Motion current() const { return motion_; }
    // Último movimento que terminou TODOS os passos (não abortado nem cortado pela segurança).
    Motion last_completed() const { return last_completed_; }
    // Termina ao fim do ciclo corrente (não inicia novo repeat). Para STOP vindo do cérebro.
    void stop_after_cycle() { repeats_ = 1; }
    // Termina ao fim do PASSO corrente (≤ 1 rampa). Para vereditos da segurança (borda, obstáculo).
    void stop_after_step() { stop_step_ = true; }
    // Corta na hora: ABORT=1 em todos os servos e descarta a sequência.
    void abort_now();
    // Inicia a reativação escalonada: uma perna (3 servos) por tick(), como CSpider::WakeUp.
    // aborted() só volta a false quando as 6 pernas foram religadas.
    void wake();
    bool aborted() const { return aborted_; }
    bool waking() const { return wake_leg_ < 6; }
    // ≥ FAULT_TIMEOUTS passos seguidos sem STATUS: FPGA/servo fora. Fica true até wake().
    bool fault() const { return fault_; }
    static const uint32_t FAULT_TIMEOUTS = 3;
    void set_speed(int speed_0_100);
    int  speed() const { return speed_; }
    uint32_t step_timeouts() const { return timeouts_; }
    int  step_index() const { return step_; }
    int  step_count() const { return nsteps_; }

    // Monta a sequência de um movimento em `out` (até MAX_STEPS). Exposto para testes.
    static int build(Motion m, Step* out);

private:
    void issue_step(uint32_t now_ms);
    bool step_done(uint32_t now_ms);

    PwmBank* pwm_;
    Motion   motion_;
    Step     seq_[MAX_STEPS];
    int      nsteps_;
    int      step_;
    uint8_t  repeats_;
    uint32_t eta_ms_;
    uint32_t pause_until_ms_;
    bool     aborted_;
    bool     stop_step_;
    int      wake_leg_;
    uint32_t wake_last_ms_;
    bool     fault_;
    uint32_t timeouts_row_;
    Motion   last_gait_;
    Motion   last_completed_;
    int      speed_;
    int      speed_saved_;
    uint32_t timeouts_;
};

}}  // namespace aracno::motor
