#pragma once
#include <stdint.h>
#include "aracno/types.h"
#include "motor/spider.h"

namespace aracno { namespace app {

// "Cérebro" mínimo da S03: só para exercitar laço, servos e segurança.
//   stand   → levanta (M_STAND) e fica parada
//   walk N  → levanta e anda N ciclos para frente, depois para
//   turn N  → levanta e gira N ciclos à esquerda
// A política aprendida (S05) substitui esta classe; a interface é a mesma: next().
class Demo {
public:
    enum Mode { STAND, WALK, TURN };
    Demo(Mode m, int cycles, uint8_t speed) : mode_(m), cycles_left_(cycles), speed_(speed), stood_(false), cycle_pending_(false) {}

    // Devolve o comando desejado dado o estado do stepper.
    Cmd next(const motor::Spider& sp) {
        Cmd c; c.action = STOP; c.speed = speed_;
        if (!stood_) {
            // Só conta como "em pé" se o M_STAND terminou todos os passos (abort/segurança não contam).
            if (sp.last_completed() == motor::M_STAND) stood_ = true;
            c.action = STAND_REQUEST;   // tratado pelo main: dispara M_STAND
            if (stood_) c.action = STOP;
            return c;
        }
        // Um ciclo só conta quando o stepper realmente partiu (a segurança pode ter barrado).
        if (cycle_pending_ && sp.busy()) { --cycles_left_; cycle_pending_ = false; }
        if (cycles_left_ > 0 && !sp.busy()) {
            cycle_pending_ = true;
            c.action = (mode_ == WALK) ? FORWARD : (mode_ == TURN ? TURN_LEFT : STOP);
        } else if (sp.busy()) {
            c.action = (mode_ == WALK) ? FORWARD : (mode_ == TURN ? TURN_LEFT : STOP);
        }
        return c;
    }
    bool done(const motor::Spider& sp) const { return stood_ && cycles_left_ == 0 && !sp.busy(); }
    bool stood() const { return stood_; }

    // Ação especial fora de aracno::Action, só do demo (não passa pela política).
    static const uint8_t STAND_REQUEST = 0xFE;

private:
    Mode mode_;
    int cycles_left_;
    uint8_t speed_;
    bool stood_;
    bool cycle_pending_;
};

}}  // namespace aracno::app
