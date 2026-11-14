// ---------------------------------------------------------------------------
// ponte.cpp — a ponte pybind11 entre o motor (C++) e a interface (Python).
//
// Este é o ÚNICO arquivo que conhece os dois lados. Tudo que o Python pode
// pedir ao motor precisa ser "exposto" aqui. Se algo não está aqui, o Python
// não enxerga.
//
// PYBIND11_MODULE(genesis_motor, m) cria o módulo que o Python importa com
// `import genesis_motor`. Cada `m.def(...)` publica uma função do motor.
// ---------------------------------------------------------------------------
#include <pybind11/pybind11.h>

#include "motor.hpp"

PYBIND11_MODULE(genesis_motor, m) {
    m.doc() = "Motor de simulação do Genesis (C++), exposto ao Python.";

    m.def("identificacao", &genesis::identificacao,
          "Nome e versão do motor.");
}
