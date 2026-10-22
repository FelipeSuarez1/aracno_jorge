#include "motor/spider.h"

namespace aracno { namespace motor {

// Constantes de CSpider.cpp (Spider CD), em graus.
static const int8_t KNEE_UP   = 60;   // Knee_Up_Base
static const int8_t KNEE_DOWN = 45;   // Knee_Down_Base
static const int8_t HIP_F     = -20;  // HipF_Base
static const int8_t HIP_M     = 0;    // HipM_Base
static const int8_t HIP_B     = 20;   // HipB_Base
static const int8_t ANKLE     = 45;   // Ankle_Base
static const int8_t DANCE     = 20;   // Dance_Angle
static const int8_t CROUCH_FOOT = 65; // [A AJUSTAR NO LAB]: SetFootY(45)=em pé, 90=no chão

enum { RF = 0, RM, RB, LF, LM, LB };
enum { HIP = 0, KNEE, ANKLE_J };
static inline uint8_t mot(int leg, int joint) { return static_cast<uint8_t>(leg * 3 + joint); }

namespace {

struct Builder {
    Step* out;
    int n;
    Builder(Step* o) : out(o), n(0) {}
    Step& cur() { return out[n - 1]; }
    void begin(uint16_t pause = 0) {
        if (n >= Spider::MAX_STEPS) return;
        out[n].n = 0;
        out[n].pause_ms = pause;
        ++n;
    }
    void set(int leg, int joint, int deg) {
        Step& s = cur();
        if (s.n >= 18) return;
        s.a[s.n].motor = mot(leg, joint);
        s.a[s.n].deg = static_cast<int8_t>(deg);
        ++s.n;
    }
    // CSpider::MoveTripod(tripod, joint, F, M, B): tripé 0 = RF,LM,RB; tripé 1 = LF,RM,LB.
    void tripod(int t, int joint, int f, int m, int b) {
        if (t == 0) { set(RF, joint, f); set(LM, joint, m); set(RB, joint, b); }
        else        { set(LF, joint, f); set(RM, joint, m); set(LB, joint, b); }
    }
    // CSpider::SetFootY(leg, a): joelho e tornozelo = a.
    void foot(int leg, int a) { set(leg, ANKLE_J, a); set(leg, KNEE, a); }
    void foot_all(int a) { for (int l = 0; l < 6; ++l) foot(l, a); }
};

// CSpider::MoveForward (6 passos por ciclo); backward inverte os sinais dos quadris.
void gait_cycle(Builder& b, int dir) {
    b.begin(); b.tripod(0, KNEE, KNEE_UP, KNEE_UP, KNEE_UP);
    b.begin(); b.tripod(0, HIP, HIP_F + 20 * dir, HIP_M + 20 * dir, HIP_B + 20 * dir);
               b.tripod(1, HIP, HIP_F - 20 * dir, HIP_M - 20 * dir, HIP_B - 20 * dir);
    b.begin(); b.tripod(0, KNEE, KNEE_DOWN, KNEE_DOWN, KNEE_DOWN);
    b.begin(); b.tripod(1, KNEE, KNEE_UP, KNEE_UP, KNEE_UP);
    b.begin(); b.tripod(0, HIP, HIP_F - 20 * dir, HIP_M - 20 * dir, HIP_B - 20 * dir);
               b.tripod(1, HIP, HIP_F + 20 * dir, HIP_M + 20 * dir, HIP_B + 20 * dir);
    b.begin(); b.tripod(1, KNEE, KNEE_DOWN, KNEE_DOWN, KNEE_DOWN);
}

// CSpider::RotatelRight (s=-1) / RotatelLeft (s=+1).
void rotate_cycle(Builder& b, int s) {
    b.begin(); b.tripod(0, KNEE, KNEE_UP, KNEE_UP, KNEE_UP);
    b.begin(); b.tripod(0, HIP, HIP_F + 30 * s, HIP_M - 30 * s, HIP_B + 30 * s);
    b.begin(); b.tripod(0, KNEE, KNEE_DOWN, KNEE_DOWN, KNEE_DOWN);
    b.begin(); b.tripod(1, KNEE, KNEE_UP, KNEE_UP, KNEE_UP);
    b.begin(); b.tripod(0, HIP, HIP_F, HIP_M, HIP_B);
    b.begin(); b.tripod(1, KNEE, KNEE_DOWN, KNEE_DOWN, KNEE_DOWN);
}

// CSpider::TiltForward/Backward/Left/Right/None.
void tilt(Builder& b, Motion m) {
    b.begin();
    switch (m) {
    case M_TILT_F:
        b.foot(LF, KNEE_DOWN + DANCE); b.foot(LM, KNEE_DOWN); b.foot(LB, KNEE_DOWN - DANCE);
        b.foot(RF, KNEE_DOWN + DANCE); b.foot(RM, KNEE_DOWN); b.foot(RB, KNEE_DOWN - DANCE);
        break;
    case M_TILT_B:
        b.foot(LF, KNEE_DOWN - DANCE); b.foot(LM, KNEE_DOWN); b.foot(LB, KNEE_DOWN + DANCE);
        b.foot(RF, KNEE_DOWN - DANCE); b.foot(RM, KNEE_DOWN); b.foot(RB, KNEE_DOWN + DANCE);
        break;
    case M_TILT_L:
        b.foot(LF, KNEE_DOWN + DANCE); b.foot(LM, KNEE_DOWN + DANCE); b.foot(LB, KNEE_DOWN + DANCE);
        b.foot(RF, KNEE_DOWN - DANCE); b.foot(RM, KNEE_DOWN - DANCE); b.foot(RB, KNEE_DOWN - DANCE);
        break;
    case M_TILT_R:
        b.foot(LF, KNEE_DOWN - DANCE); b.foot(LM, KNEE_DOWN - DANCE); b.foot(LB, KNEE_DOWN - DANCE);
        b.foot(RF, KNEE_DOWN + DANCE); b.foot(RM, KNEE_DOWN + DANCE); b.foot(RB, KNEE_DOWN + DANCE);
        break;
    default:
        b.foot_all(KNEE_DOWN);
        break;
    }
}

// CSpider::Reset: pares de pernas (i, 5-i): joelho 60 + quadril base + tornozelo 45; depois joelho 45.
void reset_pose(Builder& b) {
    static const int8_t hip0[6] = {HIP_F, HIP_M, HIP_B, HIP_F, HIP_M, HIP_B};
    for (int i = 0; i < 3; ++i) {
        int j = 5 - i;
        b.begin();
        b.set(i, KNEE, KNEE_UP); b.set(j, KNEE, KNEE_UP);
        b.set(i, HIP, hip0[i]);  b.set(j, HIP, hip0[j]);
        b.set(i, ANKLE_J, ANKLE); b.set(j, ANKLE_J, ANKLE);
        b.begin();
        b.set(i, KNEE, KNEE_DOWN); b.set(j, KNEE, KNEE_DOWN);
    }
}

}  // namespace

int Spider::build(Motion m, Step* out) {
    Builder b(out);
    switch (m) {
    case M_STAND: {
        // CSpider::Init: cada junta de cada perna vai a 0°, uma por vez, com 100 ms de pausa.
        for (int joint = 0; joint < 3; ++joint)
            for (int leg = 0; leg < 6; ++leg) { b.begin(100); b.set(leg, joint, 0); }
        // CSpider::Standup: quadris (−20, 0, 20) ×2; joelhos 90→45 em passos de 5°, tornozelo 45.
        b.begin();
        static const int8_t hip0[6] = {HIP_F, HIP_M, HIP_B, HIP_F, HIP_M, HIP_B};
        for (int leg = 0; leg < 6; ++leg) b.set(leg, HIP, hip0[leg]);
        for (int k = 90; k >= 45; k -= 5) {
            b.begin();
            for (int leg = 0; leg < 6; ++leg) { b.set(leg, KNEE, k); b.set(leg, ANKLE_J, ANKLE); }
        }
        reset_pose(b);
        break;
    }
    case M_FORWARD:  gait_cycle(b, +1); break;
    case M_BACKWARD: gait_cycle(b, -1); break;
    case M_ROTATE_L: rotate_cycle(b, +1); break;
    case M_ROTATE_R: rotate_cycle(b, -1); break;
    case M_TILT_F: case M_TILT_B: case M_TILT_L: case M_TILT_R: case M_TILT_N:
        tilt(b, m); break;
    case M_CROUCH:
        b.begin(); b.foot_all(CROUCH_FOOT); break;
    case M_LUNGE:
        tilt(b, M_TILT_F);
        gait_cycle(b, +1);
        tilt(b, M_TILT_N);
        break;
    case M_SLEEP_POSE:
        reset_pose(b); break;
    default:
        break;
    }
    return b.n;
}

Spider::Spider()
    : pwm_(0), motion_(M_NONE), nsteps_(0), step_(0), repeats_(0), eta_ms_(0),
      pause_until_ms_(0), aborted_(false), stop_step_(false), wake_leg_(6), wake_last_ms_(0),
      fault_(false), timeouts_row_(0), last_gait_(M_NONE), speed_(50), speed_saved_(50), timeouts_(0) {}

bool Spider::start(Motion m, uint8_t repeats, uint32_t now_ms) {
    if (motion_ != M_NONE || !pwm_ || m == M_NONE || m >= N_MOTIONS) return false;
    if (aborted_ || fault_) return false;   // religar primeiro (wake), de forma escalonada
    int n0 = 0;
    // CSpider::RotatelLeft/Right chamam Reset() se os quadris não estão na base (após marcha).
    if ((m == M_ROTATE_L || m == M_ROTATE_R) &&
        (last_gait_ == M_FORWARD || last_gait_ == M_BACKWARD || last_gait_ == M_LUNGE))
        n0 = build(M_SLEEP_POSE, seq_);
    int n1 = build(m, seq_ + n0);
    if (n1 == 0 || n0 + n1 > MAX_STEPS) return false;
    nsteps_ = n0 + n1;
    motion_ = m;
    repeats_ = repeats ? repeats : 1;
    step_ = 0;
    stop_step_ = false;
    if (m == M_FORWARD || m == M_BACKWARD || m == M_LUNGE || m == M_ROTATE_L || m == M_ROTATE_R) last_gait_ = m;
    if (m == M_LUNGE) { speed_saved_ = speed_; set_speed(100); }
    issue_step(now_ms);
    return true;
}

void Spider::issue_step(uint32_t now_ms) {
    const Step& s = seq_[step_];
    eta_ms_ = now_ms;
    for (uint8_t i = 0; i < s.n; ++i) {
        pwm_->move(s.a[i].motor, static_cast<float>(s.a[i].deg), now_ms);
        uint32_t e = pwm_->eta_ms(s.a[i].motor);
        if (e > eta_ms_) eta_ms_ = e;
    }
    pause_until_ms_ = 0;
}

bool Spider::step_done(uint32_t now_ms) {
    const Step& s = seq_[step_];
    bool all = true;
    for (uint8_t i = 0; i < s.n && all; ++i)
        if (!pwm_->ready(s.a[i].motor)) all = false;
    if (!all) {
        // Timeout: rampa estimada + 100 ms. Conta como anomalia (servo preso, FPGA fora).
        if (now_ms >= eta_ms_ + 100u) { ++timeouts_; ++timeouts_row_; return true; }
        return false;
    }
    timeouts_row_ = 0;
    return true;
}

void Spider::tick(uint32_t now_ms) {
    // Reativação escalonada: 3 servos (uma perna) a cada 100 ms, como CSpider::WakeUp.
    if (wake_leg_ < 6) {
        if (now_ms - wake_last_ms_ >= 100u || wake_last_ms_ == 0) {
            for (int j = 0; j < 3; ++j) pwm_->abort(wake_leg_ * 3 + j, false);
            wake_last_ms_ = now_ms;
            ++wake_leg_;
            if (wake_leg_ == 6) { aborted_ = false; fault_ = false; timeouts_row_ = 0; }
        }
        return;
    }
    if (motion_ == M_NONE) return;
    if (pause_until_ms_) {
        if (now_ms < pause_until_ms_) return;
        pause_until_ms_ = 0;
    } else {
        if (!step_done(now_ms)) return;
        if (timeouts_row_ >= FAULT_TIMEOUTS) {
            // FPGA fora ou servo preso: não insistir. Fica em falha até wake().
            fault_ = true;
            if (motion_ == M_LUNGE) set_speed(speed_saved_);
            motion_ = M_NONE;
            return;
        }
        if (seq_[step_].pause_ms) {
            pause_until_ms_ = now_ms + seq_[step_].pause_ms;
            return;
        }
    }
    ++step_;
    if (stop_step_ || step_ >= nsteps_) {
        if (!stop_step_ && repeats_ > 1) { --repeats_; step_ = 0; }
        else {
            if (motion_ == M_LUNGE) set_speed(speed_saved_);
            motion_ = M_NONE;
            stop_step_ = false;
            return;
        }
    }
    issue_step(now_ms);
}

void Spider::abort_now() {
    if (pwm_) pwm_->abort_all(true);
    aborted_ = true;
    wake_leg_ = 6;             // cancela reativação em curso
    if (motion_ == M_LUNGE) set_speed(speed_saved_);
    motion_ = M_NONE;
    stop_step_ = false;
}

void Spider::wake() {
    if (!pwm_ || !aborted_ || wake_leg_ < 6) return;   // já religado ou já religando
    wake_leg_ = 0;
    wake_last_ms_ = 0;
}

void Spider::set_speed(int speed) {
    // CSpider::SetSpeed satura em [10, 100].
    if (speed > 100) speed = 100;
    if (speed < 10) speed = 10;
    speed_ = speed;
    if (pwm_) pwm_->set_speed_all(speed);
}

}}  // namespace aracno::motor
