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
        .value("AREIA", Material::Areia)
        .value("GELO", Material::Gelo);

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
        .value("CONSTRUIR", Tarefa::Construir)
        .value("MACHADO", Tarefa::Machado);

    py::class_<Pedra>(m, "Pedra")
        .def_readonly("pos", &Pedra::pos)
        .def_readonly("lascas", &Pedra::lascas)
        .def_readonly("existe", &Pedra::existe);

    py::class_<Abrigo>(m, "Abrigo")
        .def_readonly("pos", &Abrigo::pos)
        .def_readonly("moradores", &Abrigo::moradores)
        .def_readonly("dentro", &Abrigo::dentro)
        .def_readonly("macas", &Abrigo::macas);

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
        .value("DORMINDO", EstadoNpc::Dormindo)
        .value("INDO_PROCRIAR", EstadoNpc::IndoProcriar)
        .value("PROCRIANDO", EstadoNpc::Procriando)
        .value("BEBE", EstadoNpc::Bebe)
        .value("INDO_GUARDAR", EstadoNpc::IndoGuardar);

    py::enum_<Sexo>(m, "Sexo")
        .value("HOMEM", Sexo::Homem)
        .value("MULHER", Sexo::Mulher);

    py::enum_<TipoRegistro>(m, "TipoRegistro")
        .value("INICIO", TipoRegistro::Inicio)
        .value("NASCIMENTO", TipoRegistro::Nascimento)
        .value("MORTE", TipoRegistro::Morte)
        .value("CASAL", TipoRegistro::Casal)
        .value("GRAVIDEZ", TipoRegistro::Gravidez)
        .value("VIROU_ADULTO", TipoRegistro::VirouAdulto)
        .value("ABRIGO_FEITO", TipoRegistro::AbrigoFeito)
        .value("MACHADO_FEITO", TipoRegistro::MachadoFeito)
        .value("ARVORE_CORTADA", TipoRegistro::ArvoreCortada)
        .value("MUDA_NASCEU", TipoRegistro::MudaNasceu)
        .value("ARVORE_NASCEU", TipoRegistro::ArvoreNasceu)
        .value("ARVORE_SECOU", TipoRegistro::ArvoreSecou)
        .value("ARVORE_SUMIU", TipoRegistro::ArvoreSumiu)
        .value("PEDRA_ESGOTOU", TipoRegistro::PedraEsgotou)
        .value("BEBE_PERDIDO", TipoRegistro::BebePerdido)
        .value("ESTACAO", TipoRegistro::Estacao)
        .value("CONGELOU", TipoRegistro::Congelou)
        .value("DERRETEU", TipoRegistro::Derreteu);

    py::enum_<Estacao>(m, "Estacao")
        .value("VERAO", Estacao::Verao)
        .value("OUTONO", Estacao::Outono)
        .value("INVERNO", Estacao::Inverno)
        .value("PRIMAVERA", Estacao::Primavera);

    py::class_<Data>(m, "Data")
        .def_readonly("ano", &Data::ano)
        .def_readonly("mes", &Data::mes)
        .def_readonly("dia", &Data::dia)
        .def_readonly("hora", &Data::hora)
        .def_readonly("minuto", &Data::minuto)
        .def_readonly("segundo", &Data::segundo);

    py::class_<Registro>(m, "Registro")
        .def_readonly("tempo", &Registro::tempo)
        .def_readonly("tipo", &Registro::tipo)
        .def_readonly("a", &Registro::a)
        .def_readonly("b", &Registro::b)
        .def_readonly("c", &Registro::c)
        .def_readonly("pos", &Registro::pos);

    py::class_<Macieira>(m, "Macieira")
        .def_readonly("pos", &Macieira::pos)
        .def_readonly("nasceu", &Macieira::nasceu)
        .def_readonly("quando_queda", &Macieira::quando_queda)
        .def_readonly("quando_secar", &Macieira::quando_secar)
        .def_readonly("quando_sumir", &Macieira::quando_sumir)
        .def_readonly("seca", &Macieira::seca)
        .def_readonly("existe", &Macieira::existe)
        .def_readonly("macas_verao", &Macieira::macas_verao);

    py::class_<Muda>(m, "Muda")
        .def_readonly("pos", &Muda::pos)
        .def_readonly("nasceu", &Muda::nasceu)
        .def_readonly("quando_virar", &Muda::quando_virar);

    py::class_<Npc>(m, "Npc")
        .def_readonly("id", &Npc::id)
        .def_readonly("nome", &Npc::nome)
        .def_readonly("sexo", &Npc::sexo)
        .def_readonly("bebe", &Npc::bebe)
        .def_readonly("nascimento", &Npc::nascimento)
        .def_readonly("mae", &Npc::mae)
        .def_readonly("pai", &Npc::pai)
        .def_readonly("filhos", &Npc::filhos)
        .def_readonly("parceiro", &Npc::parceiro)
        .def_readonly("gravida", &Npc::gravida)
        .def_readonly("gravidez_pai", &Npc::gravidez_pai)
        .def_readonly("quando_nascer", &Npc::quando_nascer)
        .def_readonly("pode_engravidar_em", &Npc::pode_engravidar_em)
        .def_readonly("procriando_desde", &Npc::procriando_desde)
        .def_readonly("morreu_em", &Npc::morreu_em)
        .def_readonly("quando_velhice", &Npc::quando_velhice)
        .def_readonly("quando_adulto", &Npc::quando_adulto)
        .def_readonly("macas_carregadas", &Npc::macas_carregadas)
        .def_readonly("comendo", &Npc::comendo)
        .def_readonly("comendo_de", &Npc::comendo_de)
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

    m.attr("SEGUNDOS_POR_HORA") = config::SEGUNDOS_POR_HORA;
    m.attr("SEGUNDOS_POR_DIA") = config::SEGUNDOS_POR_DIA;
    m.attr("TEMP_CRITICA") = config::TEMP_CRITICA;
    m.attr("HOMENS_PADRAO") = config::HOMENS_PADRAO;
    m.attr("MULHERES_PADRAO") = config::MULHERES_PADRAO;
    m.attr("SEGUNDOS_POR_ANO") = config::SEGUNDOS_POR_ANO;
    m.attr("SEGUNDOS_POR_MINUTO") = config::SEGUNDOS_POR_MINUTO;
    m.attr("HORA_INICIAL") = config::HORA_INICIAL;
    m.attr("MACAS_CARREGAVEIS") = config::MACAS_CARREGAVEIS;
    m.attr("ABRIGO_MACAS_MAX") = config::ABRIGO_MACAS_MAX;
    m.attr("MACAS_POR_PILHA") = config::MACAS_POR_PILHA;
    m.attr("FOME_INTERVALO_S") = config::FOME_INTERVALO_S;
    m.attr("SEDE_INTERVALO_S") = config::SEDE_INTERVALO_S;
    m.attr("COMER_DURACAO_S") = config::COMER_DURACAO_S;
    m.attr("BEBER_DURACAO_S") = config::BEBER_DURACAO_S;
    m.attr("NPC_SUBIR_S") = config::NPC_SUBIR_S;
    m.attr("AGUA_INTERVALO_S") = config::AGUA_INTERVALO_S;
    m.attr("BORDA_AGUA") = config::BORDA_AGUA;
    m.attr("TEMP_FRIO") = config::TEMP_FRIO;

    py::class_<Mundo>(m, "Mundo")
        .def(py::init<std::uint64_t, int, int>(), py::arg("semente"),
             py::arg("homens") = config::HOMENS_PADRAO,
             py::arg("mulheres") = config::MULHERES_PADRAO)
        .def("avancar", &Mundo::avancar, py::arg("dt"),
             "Anda o tempo da simulação em dt segundos.")
        .def_property_readonly("largura", &Mundo::largura)
        .def_property_readonly("altura", &Mundo::altura)
        .def_property_readonly("tempo", &Mundo::tempo)
        .def("data", &Mundo::data, "Data e hora de agora (calendário).")
        .def_static("data_de", &Mundo::data_de, py::arg("t"),
                    "Data e hora do instante t (segundos da simulação).")
        .def_static("tempo_de", &Mundo::tempo_de, py::arg("ano"), py::arg("mes"),
                    py::arg("dia"), "Segundos da simulação de uma data (00:00).")
        .def_static("somar_meses", &Mundo::somar_meses, py::arg("t"), py::arg("meses"))
        .def_static("estacao_de", &Mundo::estacao_de, py::arg("t"))
        .def("estacao", &Mundo::estacao)
        .def_static("horas_de_sol", &Mundo::horas_de_sol, py::arg("t"))
        .def_static("nascer_do_sol", &Mundo::nascer_do_sol, py::arg("t"))
        .def_static("por_do_sol", &Mundo::por_do_sol, py::arg("t"))
        .def_static("min_max_temperatura", [](double t) {
            double a = 0.0, b = 0.0;
            Mundo::min_max_temperatura(t, a, b);
            return py::make_tuple(a, b);
        }, py::arg("t"))
        .def_static("limites_estacao", [](double t) {
            double a = 0.0, b = 0.0;
            Mundo::limites_estacao(t, a, b);
            return py::make_tuple(a, b);
        }, py::arg("t"))
        .def("temperatura", &Mundo::temperatura, "Temperatura agora (°C).")
        .def("temperatura_em", &Mundo::temperatura_em, py::arg("t"))
        .def("sol", &Mundo::sol, "O sol está no céu?")
        .def("e_dia", &Mundo::e_dia, "É 'dia' para o abrigo (06:00–18:00)?")
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
        .def("macas_em", &Mundo::macas_em, py::arg("x"), py::arg("y"))
        .def("pilhas", &Mundo::pilhas, "Pilhas de maçãs: (x, y, quantas).")
        .def("macas_vencem", &Mundo::macas_vencem, py::arg("x"), py::arg("y"))
        .def("macas_por_cair", &Mundo::macas_por_cair, py::arg("macieira"))
        .def("mudas", &Mundo::mudas)
        .def("pedras", &Mundo::pedras)
        .def("idade_anos", &Mundo::idade_anos, py::arg("npc"))
        .def("total_historico", [](const Mundo& w) { return w.historico().size(); })
        .def("historico", [](const Mundo& w, size_t desde) {
            const auto& h = w.historico();
            return std::vector<Registro>(h.begin() + static_cast<long>(std::min(desde, h.size())),
                                         h.end());
        }, py::arg("desde") = 0,
           "Acontecimentos importantes a partir do número `desde`.")
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
        .def("teste_idade", &Mundo::teste_idade, py::arg("id"), py::arg("anos"),
             "SÓ PARA TESTES: muda a idade e reagenda a velhice.")
        .def("teste_macas", &Mundo::teste_macas, py::arg("id"), py::arg("quantas"),
             "SÓ PARA TESTES: maçãs na mão do NPC.")
        .def("teste_abrigo", &Mundo::teste_abrigo, py::arg("id"), py::arg("x"),
             py::arg("y"), py::arg("macas") = 0,
             "SÓ PARA TESTES: abrigo pronto em (x, y), com o NPC morando nele.")
        .def("teste_idade_macieira", &Mundo::teste_idade_macieira, py::arg("indice"),
             py::arg("anos"), "SÓ PARA TESTES: muda a idade de uma macieira.")
        .def("teste_adiantar_parto", &Mundo::teste_adiantar_parto, py::arg("mae"),
             py::arg("segundos"), "SÓ PARA TESTES: o bebê nasce daqui a `segundos`.")
        .def("teste_saltar", &Mundo::teste_saltar, py::arg("t"),
             "SÓ PARA TESTES: pula o relógio até t (ver mundo.hpp).")
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
