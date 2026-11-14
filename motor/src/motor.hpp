// ---------------------------------------------------------------------------
// motor.hpp — porta de entrada do motor de simulação.
//
// Por enquanto o motor só se identifica. É o "esqueleto": serve para provar
// que a cadeia C++ -> pybind11 -> Python -> rich funciona de ponta a ponta.
// A simulação (ilha, NPCs, macieiras, fome) entra aqui depois que as
// perguntas pendentes forem respondidas pelo dono do projeto.
// ---------------------------------------------------------------------------
#pragma once

#include <string>

namespace genesis {

// Devolve o nome e a versão do motor, para a interface mostrar.
std::string identificacao();

}  // namespace genesis
