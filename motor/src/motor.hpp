// ---------------------------------------------------------------------------
// motor.hpp — porta de entrada do motor de simulação.
//
// Aqui fica só a identificação do motor. A simulação em si está em
// mundo.hpp / mundo.cpp, e todos os números dela em config.hpp.
// ---------------------------------------------------------------------------
#pragma once

#include <string>

namespace genesis {

// Devolve o nome e a versão do motor, para a interface mostrar.
std::string identificacao();

}  // namespace genesis
