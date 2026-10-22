// Política tabular treinada no PC (S05): lookup em Q_ARGMAX[aracno::state_index(obs)].
// Pesos em brain/weights.h, vetores de autoteste em brain/policy_vectors.h — ambos gerados
// por sim/export.py; nunca editar à mão. A política nunca escreve nos servos: o Cmd passa
// pela camada de segurança (S03) antes.
#pragma once
#include "aracno/types.h"

namespace aracno {

class Policy {
public:
    // Confere todos os vetores de policy_vectors.h (estado e ação) e a faixa de Q_ARGMAX.
    // false = pesos/discretização divergem do Python: o app não deve rodar a política.
    bool self_test() const;

    // Sem alocação, sem estado. speed = 50; LUNGE usa 100.
    Cmd act(const Obs& o) const;

    // sha256 do q_table.npy que gerou weights.h (para o log).
    const char* weights_sha() const;
};

}  // namespace aracno
