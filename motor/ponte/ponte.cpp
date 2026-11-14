// ---------------------------------------------------------------------------
// ponte.cpp — a ponte pybind11 entre o motor (C++) e a interface (Python).
//
// Este é o ÚNICO arquivo que conhece os dois lados. Tudo que o Python pode
// pedir ao motor precisa ser "exposto" aqui. Se algo não está aqui, o Python
// não enxerga.
//
// PYBIND11_MODULE(genesis_motor, m) cria o módulo que o Python importa com
// `import genesis_motor`. Cada `def` publica uma função; cada `class_`
// publica um tipo do motor.
//
// A interface só LÊ o estado (def_readonly); quem muda o mundo é o motor.
// ---------------------------------------------------------------------------
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>  // converte std::vector em lista do Python

#include "config.hpp"
#include "motor.hpp"
#include "mundo.hpp"

namespace py = pybind11;
using namespace genesis;

PYBIND11_MODULE(genesis_motor, m) {
    m.doc() = "Motor de simulação do Genesis (C++), exposto ao Python.";

    m.def("identificacao", &identificacao, "Nome e versão do motor.");

    // A interface precisa saber de quanto em quanto tempo algo se mexe,
    // para atualizar a tela no mesmo ritmo. Vem do config.hpp, para o número
    // existir num lugar só.
    m.attr("NPC_PASSO_S") = config::NPC_PASSO_S;
    // Limites do mundo 3D, para a interface saber até onde subir/descer.
    m.attr("Z_MIN") = config::Z_MIN;
    m.attr("Z_MAX") = config::Z_MAX;
    m.attr("SUPERFICIE_MAX") = config::SUPERFICIE_MAX;
    m.attr("AGUA_NIVEL_MAX") = config::AGUA_NIVEL_MAX;

    py::class_<Pos>(m, "Pos")
        .def_readonly("x", &Pos::x)
        .def_readonly("y", &Pos::y)
        .def_readonly("z", &Pos::z);

    py::enum_<Material>(m, "Material")
        .value("AR", Material::Ar)
        .value("GRAMA", Material::Grama)
        .value("TERRA", Material::Terra)
        .value("PEDRA", Material::Pedra)
        .value("AGUA", Material::Agua);

    py::enum_<Ocupante>(m, "Ocupante")
        .value("NADA", Ocupante::Nada)
        .value("MACIEIRA", Ocupante::Macieira)
        .value("MACA", Ocupante::Maca)
        .value("NPC", Ocupante::Npc)
        .value("CORPO", Ocupante::Corpo);

    py::enum_<EstadoNpc>(m, "EstadoNpc")
        .value("VAGANDO", EstadoNpc::Vagando)
        .value("INDO_COMER", EstadoNpc::IndoComer)
        .value("COMENDO", EstadoNpc::Comendo)
        .value("MORTO", EstadoNpc::Morto)
        .value("INDO_BEBER", EstadoNpc::IndoBeber)
        .value("BEBENDO", EstadoNpc::Bebendo);

    py::class_<Macieira>(m, "Macieira")
        .def_readonly("pos", &Macieira::pos)
        .def_readonly("falta_queda", &Macieira::falta_queda);

    py::class_<Npc>(m, "Npc")
        .def_readonly("id", &Npc::id)
        .def_readonly("pos", &Npc::pos)
        .def_readonly("fome", &Npc::fome)
        .def_readonly("sede", &Npc::sede)
        .def_readonly("estado", &Npc::estado)
        .def_readonly("falta_acao", &Npc::falta_acao)
        .def_readonly("pela_memoria", &Npc::pela_memoria)
        .def_readonly("alvo", &Npc::alvo)
        .def_readonly("macieiras_lembradas", &Npc::macieiras_lembradas)
        .def_readonly("aguas_lembradas", &Npc::aguas_lembradas);

    py::class_<Mundo>(m, "Mundo")
        .def(py::init<std::uint64_t>(), py::arg("semente"))
        .def("avancar", &Mundo::avancar, py::arg("dt"),
             "Anda o tempo da simulação em dt segundos.")
        .def_property_readonly("largura", &Mundo::largura)
        .def_property_readonly("altura", &Mundo::altura)
        .def_property_readonly("tempo", &Mundo::tempo)
        .def("ocupante", &Mundo::ocupante, py::arg("x"), py::arg("y"))
        .def("material", [](const Mundo& w, int x, int y, int z) {
            return w.mapa().material(x, y, z);
        }, py::arg("x"), py::arg("y"), py::arg("z"))
        .def("nivel_agua", [](const Mundo& w, int x, int y, int z) {
            return w.mapa().nivel_agua(x, y, z);
        }, py::arg("x"), py::arg("y"), py::arg("z"))
        .def("superficie", [](const Mundo& w, int x, int y) {
            return w.mapa().superficie(x, y);
        }, py::arg("x"), py::arg("y"))
        .def("agua_total", [](const Mundo& w) { return w.mapa().agua_total(); })
        .def("visao_bytes", [](const Mundo& w, int z) {
            const auto v = w.visao(z);
            return py::bytes(reinterpret_cast<const char*>(v.data()), v.size());
        }, py::arg("z"),
           "O que se vê olhando o nível z de cima (1 byte por coluna; ver mundo.hpp).")
        .def("teste_definir", &Mundo::teste_definir,
             py::arg("x"), py::arg("y"), py::arg("z"), py::arg("material"),
             py::arg("nivel_agua") = 0,
             "SÓ PARA TESTES: muda uma célula do chão à mão.")
        .def("macieiras", &Mundo::macieiras)
        .def("macas", &Mundo::macas)
        .def("npcs", &Mundo::npcs)
        // Mapa inteiro como bytes (1 byte por célula, linha por linha).
        // O valor de cada byte é o número do enum (Terreno / Ocupante).
        .def("ocupacao_bytes", [](const Mundo& w) {
            const auto& v = w.ocupacao_bruta();
            return py::bytes(reinterpret_cast<const char*>(v.data()), v.size());
        });
}
