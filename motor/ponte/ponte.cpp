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
        .value("AGUA", Material::Agua)
        .value("NEVE", Material::Neve)
        .value("AREIA", Material::Areia);

    py::enum_<Ocupante>(m, "Ocupante")
        .value("NADA", Ocupante::Nada)
        .value("MACIEIRA", Ocupante::Macieira)
        .value("MACA", Ocupante::Maca)
        .value("NPC", Ocupante::Npc)
        .value("CORPO", Ocupante::Corpo)
        .value("MUDA", Ocupante::Muda)
        .value("MACIEIRA_SECA", Ocupante::MacieiraSeca)
        .value("PEDRA", Ocupante::Pedra)
        .value("ABRIGO", Ocupante::Abrigo);

    py::enum_<Tarefa>(m, "Tarefa")
        .value("NENHUMA", Tarefa::Nenhuma)
        .value("LASCAS", Tarefa::Lascas)
        .value("GALHOS", Tarefa::Galhos)
        .value("CORTAR", Tarefa::Cortar)
        .value("CONSTRUIR", Tarefa::Construir);

    py::class_<Pedra>(m, "Pedra")
        .def_readonly("pos", &Pedra::pos)
        .def_readonly("lascas", &Pedra::lascas)
        .def_readonly("existe", &Pedra::existe);

    py::class_<Abrigo>(m, "Abrigo")
        .def_readonly("pos", &Abrigo::pos)
        .def_readonly("moradores", &Abrigo::moradores)
        .def_readonly("dentro", &Abrigo::dentro);

    py::enum_<EstadoNpc>(m, "EstadoNpc")
        .value("VAGANDO", EstadoNpc::Vagando)
        .value("INDO_COMER", EstadoNpc::IndoComer)
        .value("COMENDO", EstadoNpc::Comendo)
        .value("MORTO", EstadoNpc::Morto)
        .value("INDO_BEBER", EstadoNpc::IndoBeber)
        .value("BEBENDO", EstadoNpc::Bebendo)
        .value("INDO_PEGAR", EstadoNpc::IndoPegar)
        .value("INDO_TRABALHAR", EstadoNpc::IndoTrabalhar)
        .value("TRABALHANDO", EstadoNpc::Trabalhando)
        .value("INDO_ABRIGO", EstadoNpc::IndoAbrigo)
        .value("DORMINDO", EstadoNpc::Dormindo);

    py::class_<Macieira>(m, "Macieira")
        .def_readonly("pos", &Macieira::pos)
        .def_readonly("nasceu", &Macieira::nasceu)
        .def_readonly("quando_queda", &Macieira::quando_queda)
        .def_readonly("quando_secar", &Macieira::quando_secar)
        .def_readonly("quando_sumir", &Macieira::quando_sumir)
        .def_readonly("seca", &Macieira::seca)
        .def_readonly("existe", &Macieira::existe);

    py::class_<Muda>(m, "Muda")
        .def_readonly("pos", &Muda::pos)
        .def_readonly("nasceu", &Muda::nasceu)
        .def_readonly("quando_virar", &Muda::quando_virar);

    py::class_<Npc>(m, "Npc")
        .def_readonly("id", &Npc::id)
        .def_readonly("nome", &Npc::nome)
        .def_readonly("macas_carregadas", &Npc::macas_carregadas)
        .def_readonly("comendo_da_mao", &Npc::comendo_da_mao)
        .def_readonly("lascas", &Npc::lascas)
        .def_readonly("galhos", &Npc::galhos)
        .def_readonly("lenha", &Npc::lenha)
        .def_readonly("machado", &Npc::machado)
        .def_readonly("tarefa", &Npc::tarefa)
        .def_readonly("tarefa_pos", &Npc::tarefa_pos)
        .def_readonly("tarefa_restante", &Npc::tarefa_restante)
        .def_readonly("abrigo_casa", &Npc::abrigo_casa)
        .def_readonly("dentro_abrigo", &Npc::dentro_abrigo)
        .def_readonly("pedras_lembradas", &Npc::pedras_lembradas)
        .def_readonly("pos", &Npc::pos)
        .def_readonly("hp", &Npc::hp)
        .def_readonly("causa_morte", &Npc::causa_morte)
        .def_readonly("fome", &Npc::fome)
        .def_readonly("sede", &Npc::sede)
        .def_readonly("estado", &Npc::estado)
        .def_readonly("falta_acao", &Npc::falta_acao)
        .def_readonly("pela_memoria", &Npc::pela_memoria)
        .def_readonly("destino", &Npc::destino)
        .def_readonly("tem_destino", &Npc::tem_destino)
        .def_readonly("alvo", &Npc::alvo)
        .def_readonly("macieiras_lembradas", &Npc::macieiras_lembradas)
        .def_readonly("aguas_lembradas", &Npc::aguas_lembradas);

    py::class_<Mundo::Relogio>(m, "Relogio")
        .def_readonly("dia", &Mundo::Relogio::dia)
        .def_readonly("hora", &Mundo::Relogio::hora)
        .def_readonly("minuto", &Mundo::Relogio::minuto);

    m.attr("SEGUNDOS_POR_HORA") = config::SEGUNDOS_POR_HORA;
    m.attr("SEGUNDOS_POR_DIA") = config::SEGUNDOS_POR_DIA;
    m.attr("TEMP_CRITICA") = config::TEMP_CRITICA;
    m.attr("TEMP_FRIO") = config::TEMP_FRIO;

    py::class_<Mundo>(m, "Mundo")
        .def(py::init<std::uint64_t>(), py::arg("semente"))
        .def("avancar", &Mundo::avancar, py::arg("dt"),
             "Anda o tempo da simulação em dt segundos.")
        .def_property_readonly("largura", &Mundo::largura)
        .def_property_readonly("altura", &Mundo::altura)
        .def_property_readonly("tempo", &Mundo::tempo)
        .def("relogio", &Mundo::relogio, "Dia, hora e minuto do jogo.")
        .def("temperatura", &Mundo::temperatura, "Temperatura agora (°C).")
        .def("e_dia", &Mundo::e_dia, "É dia (06:00–18:00)?")
        .def("taxa_hp", &Mundo::taxa_hp, py::arg("npc"))
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
        .def("mudas", &Mundo::mudas)
        .def("pedras", &Mundo::pedras)
        .def("abrigos", &Mundo::abrigos)
        .def("pedra_em", &Mundo::pedra_em, py::arg("x"), py::arg("y"))
        .def("abrigo_em", &Mundo::abrigo_em, py::arg("x"), py::arg("y"))
        .def("teste_npc", &Mundo::teste_npc, py::arg("id"), py::arg("fome"),
             py::arg("sede"), py::arg("hp"), "SÓ PARA TESTES: muda fome/sede/HP.")
        .def("teste_dar", &Mundo::teste_dar, py::arg("id"), py::arg("lascas"),
             py::arg("galhos"), py::arg("lenha"), py::arg("machado"),
             "SÓ PARA TESTES: dá itens ao NPC.")
        .def("teste_mover", &Mundo::teste_mover, py::arg("id"), py::arg("x"),
             py::arg("y"), "SÓ PARA TESTES: põe o NPC numa célula livre.")
        .def("macieira_em", &Mundo::macieira_em, py::arg("x"), py::arg("y"))
        .def("muda_em", [](const Mundo& w, int x, int y) -> py::object {
            const Muda* u = w.muda_em(x, y);
            if (!u) return py::none();
            return py::cast(*u);
        }, py::arg("x"), py::arg("y"))
        .def("falta_virar_muda", &Mundo::falta_virar_muda, py::arg("x"), py::arg("y"))
        .def("npcs", &Mundo::npcs)
        // Mapa inteiro como bytes (1 byte por célula, linha por linha).
        // O valor de cada byte é o número do enum (Terreno / Ocupante).
        .def("ocupacao_bytes", [](const Mundo& w) {
            const auto& v = w.ocupacao_bruta();
            return py::bytes(reinterpret_cast<const char*>(v.data()), v.size());
        });
}
