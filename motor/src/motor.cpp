// ---------------------------------------------------------------------------
// motor.cpp — implementação da porta de entrada do motor.
// ---------------------------------------------------------------------------
#include "motor.hpp"

namespace genesis {

std::string identificacao() {
    // Versão 0.8: HP, urgência e temperatura (docs/HP_TEMPERATURA.md).
    return "Genesis — motor 0.8 (HP, temperatura)";
}

}  // namespace genesis
