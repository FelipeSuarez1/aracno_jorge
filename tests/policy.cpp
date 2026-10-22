// Teste de host da política (S05): self_test() passa e cada vetor gerado pelo Python
// (tests/policy_vectors.h) bate com aracno::state_index e Policy::act em C++ — prova que a
// discretização do C++ é igual à do sim/aracno_sim/discretize.py nos limites exatos.
#include <cstdio>
#include "aracno/discretize.h"
#include "brain/policy.h"
#include "policy_vectors.h"

#define CHECK(cond, ...)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            std::printf("FALHOU %s:%d: ", __FILE__, __LINE__);        \
            std::printf(__VA_ARGS__);                                 \
            std::printf("\n");                                        \
            return 1;                                                 \
        }                                                             \
    } while (0)

int main() {
    using namespace aracno;
    Policy p;
    CHECK(N_POLICY_VECTORS == 200, "esperava 200 vetores, há %u", N_POLICY_VECTORS);
    CHECK(p.self_test(), "Policy::self_test() falhou (sha %s)", p.weights_sha());

    Obs o = Obs();
    o.installed = 0x7F;
    unsigned lunges = 0;
    for (uint16_t i = 0; i < N_POLICY_VECTORS; ++i) {
        const PolicyVector& v = POLICY_VECTORS[i];
        o.lux_l = v.lux_l;
        o.lux_r = v.lux_r;
        o.dist_front_mm = v.dist_front_mm;
        o.dist_floor_mm = v.dist_floor_mm;
        o.pir = v.pir;
        o.t_since_stimulus_ms = v.t_since_stimulus_ms;
        const uint16_t s = state_index(o);
        CHECK(s == v.state, "vetor %u: estado C++=%u Python=%u (lux %u/%u dist %u chao %u pir %u t %lu)",
              i, s, v.state, v.lux_l, v.lux_r, v.dist_front_mm, v.dist_floor_mm, v.pir,
              static_cast<unsigned long>(v.t_since_stimulus_ms));
        CHECK(s < N_STATES, "vetor %u: estado %u fora de N_STATES", i, s);
        const Cmd c = p.act(o);
        CHECK(c.action == v.action, "vetor %u: ação C++=%u Python=%u", i, c.action, v.action);
        CHECK(c.action < N_ACTIONS, "vetor %u: ação %u inválida", i, c.action);
        if (c.action == LUNGE) {
            ++lunges;
            CHECK(c.speed == 100, "vetor %u: LUNGE com speed %u", i, c.speed);
        } else {
            CHECK(c.speed == 50, "vetor %u: speed %u != 50", i, c.speed);
        }
    }

    // Limites exatos da discretização, independentes dos vetores gerados.
    CHECK(lux_level(10) == 0 && lux_level(11) == 1, "limite lux 10/11");
    CHECK(lux_level(50) == 1 && lux_level(51) == 2, "limite lux 50/51");
    CHECK(lux_level(200) == 2 && lux_level(201) == 3, "limite lux 200/201");
    CHECK(dist_front_level(0) == 4 && dist_front_level(1) == 0, "dist 0 inválido = longe");
    CHECK(dist_front_level(1200) == 3 && dist_front_level(1201) == 4, "limite dist 1200/1201");
    CHECK(floor_level(120) == 0 && floor_level(121) == 1 && floor_level(0) == 1, "limite chão");
    CHECK(t_since_level(2000) == 0 && t_since_level(2001) == 1 && t_since_level(10001) == 2, "limite t_since");

    std::printf("policy: %u vetores ok, %u LUNGE, weights %s\n", N_POLICY_VECTORS, lunges, p.weights_sha());
    return 0;
}
