// ---------------------------------------------------------------------------
// mundo.cpp — implementação do mundo (ver explicação geral em mundo.hpp).
// ---------------------------------------------------------------------------
#include "mundo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "config.hpp"
#include "gerador.hpp"

namespace genesis {

namespace cfg = config;

namespace {

// Tipos de acontecimento das plantas (na fila).
enum TipoEvento : int {
    QUEDA_MACA = 0,      // alvo = número da macieira
    MACIEIRA_SECA = 1,   // alvo = número da macieira
    MACIEIRA_SOME = 2,   // alvo = número da macieira
    MACA_VIRA_MUDA = 3,  // alvo = célula da maçã
    MUDA_VIRA_ARVORE = 4, // alvo = id da muda
    VIRA_ADULTO = 5,      // alvo = id do bebê
    VELHICE = 6,          // alvo = id do NPC
    NASCIMENTO = 7,       // alvo = id da mãe
    CONCEPCAO = 8         // alvo = id da mãe (1 h juntos no abrigo)
};

// Margem de arredondamento para comparar tempos em ponto flutuante.
// Não é regra do jogo: só evita que 0.9999999 deixe de contar como "venceu".
constexpr double EPS = 1e-9;

// Distância "de passos" entre duas células quando se anda em 8 direções:
// a maior diferença entre colunas ou linhas (distância de Chebyshev).
int distancia_passos(Pos a, Pos b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

// Distância ao quadrado (em linha reta). Usada só para desempatar.
int distancia2(Pos a, Pos b) {
    const int dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// As 8 células vizinhas (inclui diagonais).
constexpr Pos VIZINHOS[8] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0},
                             {1, 0},   {-1, 1}, {0, 1},  {1, 1}};

}  // namespace

// ===========================================================================
// Criação do mundo
// ===========================================================================

Mundo::Mundo(std::uint64_t semente, int homens, int mulheres) : rng_(semente) {
    gerar_mundo(mapa_, semente);
    const size_t colunas = static_cast<size_t>(largura()) * altura();
    ocupacao_.assign(colunas, Ocupante::Nada);
    origem_maca_.assign(colunas, -1);
    contar_livres();
    falta_agua_ = cfg::AGUA_INTERVALO_S;

    // 6 macieiras em células de terra livres, sorteadas ("espalhadas").
    // Todas nascem com idade 0 (decisão do dono do projeto).
    for (int i = 0; i < cfg::NUM_MACIEIRAS; ++i) nova_macieira(sortear_grama_livre());

    // 3 pedras em células de terra livres, cada uma com 10 lascas.
    for (int i = 0; i < cfg::NUM_PEDRAS; ++i) {
        Pedra p;
        p.pos = sortear_grama_livre();
        p.lascas = cfg::PEDRA_LASCAS;
        ocupar(indice(p.pos), Ocupante::Pedra);
        pedras_.push_back(p);
    }

    // NPCs iniciais: `homens` homens e `mulheres` mulheres, em células de
    // terra livres, com 20 anos, fome e sede 100. Nomes "NPC 1", "NPC 2"...
    // e o sexo de cada número é sorteado. Se a terra lotar, para.
    std::vector<Sexo> sexos;
    for (int i = 0; i < homens; ++i) sexos.push_back(Sexo::Homem);
    for (int i = 0; i < mulheres; ++i) sexos.push_back(Sexo::Mulher);
    std::shuffle(sexos.begin(), sexos.end(), rng_);
    for (size_t i = 0; i < sexos.size() && livres_terra_ > 0; ++i) {
        Npc n;
        n.id = static_cast<int>(npcs_.size());
        n.nome = "NPC " + std::to_string(n.id + 1);
        n.sexo = sexos[i];
        n.idade_ref_anos = cfg::IDADE_INICIAL_ANOS;
        n.idade_ref_t = 0.0;
        n.hp = cfg::HP_INICIAL;
        n.fome = cfg::FOME_INICIAL;
        n.sede = cfg::SEDE_INICIAL;
        n.falta_passo = cfg::NPC_PASSO_S;
        n.falta_fome = cfg::FOME_INTERVALO_S;
        n.falta_sede = cfg::SEDE_INTERVALO_S;
        n.pos = sortear_grama_livre();
        ocupar(indice(n.pos), Ocupante::Npc);
        // Morre de velhice aos 60 anos.
        n.quando_velhice = (cfg::IDADE_MAXIMA_ANOS - cfg::IDADE_INICIAL_ANOS) *
                           cfg::SEGUNDOS_POR_ANO;
        agendar(n.quando_velhice, VELHICE, n.id);
        npcs_.push_back(n);
    }
    registrar(TipoRegistro::Inicio, homens, mulheres);
}

// Sorteia uma célula de grama ainda vazia (só a terra do meio é sorteada).
Pos Mundo::sortear_grama_livre() {
    std::uniform_int_distribution<int> dx(cfg::BORDA_AGUA,
                                          largura() - cfg::BORDA_AGUA - 1);
    std::uniform_int_distribution<int> dy(cfg::BORDA_AGUA,
                                          altura() - cfg::BORDA_AGUA - 1);
    while (true) {
        Pos p{dx(rng_), dy(rng_)};
        if (livre(p)) return com_z(p);
    }
}

// Uma macieira nova (do começo, ou que cresceu de uma muda): idade 0.
void Mundo::nova_macieira(Pos p) {
    Macieira m;
    m.pos = com_z(p);
    m.nasceu = tempo_;
    m.quando_queda = tempo_ + sortear_queda();
    m.quando_secar = tempo_ + cfg::MACIEIRA_VIDA_DIAS * cfg::SEGUNDOS_POR_DIA;
    ocupar(indice(m.pos), Ocupante::Macieira);
    const int i = static_cast<int>(macieiras_.size());
    macieiras_.push_back(m);
    agendar(m.quando_queda, QUEDA_MACA, i);
    agendar(m.quando_secar, MACIEIRA_SECA, i);
}

// Toda mudança de ocupante passa por aqui, para manter a conta de quantas
// células de terra estão livres (usada para não procurar lugar à toa
// quando a ilha está lotada).
void Mundo::ocupar(int i, Ocupante novo) {
    const int x = i % largura(), y = i / largura();
    const bool terra = mapa_.e_terra(x, y);
    if (terra && ocupacao_[i] == Ocupante::Nada) --livres_terra_;
    ocupacao_[i] = novo;
    if (terra && novo == Ocupante::Nada) ++livres_terra_;
}

void Mundo::contar_livres() {
    livres_terra_ = 0;
    for (int y = 0; y < altura(); ++y)
        for (int x = 0; x < largura(); ++x)
            if (livre({x, y})) ++livres_terra_;
}

void Mundo::agendar(double quando, int tipo, int alvo) {
    fila_.push({quando, tipo, alvo});
}

// Tempo até a próxima maçã: sorteado entre 30 e 60 s, a cada queda.
double Mundo::sortear_queda() {
    std::uniform_real_distribution<double> d(cfg::QUEDA_MIN_S, cfg::QUEDA_MAX_S);
    return d(rng_);
}

// ===========================================================================
// Passagem do tempo
// ===========================================================================

void Mundo::avancar(double dt) {
    double restante = dt;
    while (restante > 0.0) {
        // 1) Qual é o próximo acontecimento? (o relógio que vence primeiro)
        double proximo = std::numeric_limits<double>::infinity();
        for (const Npc& n : npcs_) {
            if (n.estado == EstadoNpc::Morto || n.bebe) continue;
            proximo = std::min({proximo, n.falta_fome, n.falta_sede,
                                n.falta_passo});
            if (n.estado == EstadoNpc::Comendo || n.estado == EstadoNpc::Bebendo ||
                n.estado == EstadoNpc::Trabalhando)
                proximo = std::min(proximo, n.falta_acao);
        }
        if (!fila_.empty()) proximo = std::min(proximo, fila_.top().quando - tempo_);
        proximo = std::min(proximo, falta_agua_);
        // A temperatura cruzando 4° / 5° (ou as 06:00) muda o HP e as
        // decisões: é um acontecimento também.
        proximo = std::min(proximo, proximo_marco_temperatura() - tempo_);
        // HP: quando alguém perdendo vida chega a 0.
        std::vector<double> taxas(npcs_.size(), 0.0);
        for (size_t k = 0; k < npcs_.size(); ++k) {
            const Npc& n = npcs_[k];
            if (n.estado == EstadoNpc::Morto || n.bebe) continue;
            taxas[k] = taxa_hp(n);
            if (taxas[k] < 0) proximo = std::min(proximo, n.hp / -taxas[k]);
        }
        proximo = std::max(proximo, 0.0);

        // 2) Anda o relógio até ele (ou até o fim do dt, o que vier antes).
        const double passo = std::min(proximo, restante);
        tempo_ += passo;
        restante -= passo;
        for (Npc& n : npcs_) {
            if (n.estado == EstadoNpc::Morto || n.bebe) continue;
            n.falta_fome -= passo;
            n.falta_sede -= passo;
            n.falta_passo -= passo;
            if (n.estado == EstadoNpc::Comendo || n.estado == EstadoNpc::Bebendo ||
                n.estado == EstadoNpc::Trabalhando)
                n.falta_acao -= passo;
        }
        for (size_t k = 0; k < npcs_.size(); ++k) {
            Npc& n = npcs_[k];
            if (n.estado == EstadoNpc::Morto || n.bebe) continue;
            n.hp = std::min(cfg::HP_MAXIMO, n.hp + taxas[k] * passo);
            if (n.hp <= EPS) {  // HP chegou a 0: morre
                n.hp = 0.0;
                morrer(n);
            }
        }
        falta_agua_ -= passo;

        // 3) Dispara tudo que venceu: NPCs, água, depois as plantas.
        for (size_t k = 0; k < npcs_.size(); ++k) {
            Npc& n = npcs_[k];
            if (n.bebe) continue;  // bebê: fica parado, sem necessidades
            if (n.estado != EstadoNpc::Morto && n.falta_fome <= EPS) {
                n.falta_fome += cfg::FOME_INTERVALO_S;
                evento_fome(n);
            }
            if (n.estado != EstadoNpc::Morto && n.falta_sede <= EPS) {
                n.falta_sede += cfg::SEDE_INTERVALO_S;
                evento_sede(n);
            }
            if (n.estado == EstadoNpc::Comendo && n.falta_acao <= EPS)
                evento_fim_comer(n);
            if (n.estado == EstadoNpc::Bebendo && n.falta_acao <= EPS)
                evento_fim_beber(n);
            if (n.estado == EstadoNpc::Trabalhando && n.falta_acao <= EPS)
                evento_fim_trabalho(n);
            if (n.estado != EstadoNpc::Morto && n.falta_passo <= EPS) {
                n.falta_passo += cfg::NPC_PASSO_S;
                evento_passo(n);
            }
        }
        // A cada 3 s: um passo da física da água.
        if (falta_agua_ <= EPS) {
            falta_agua_ += cfg::AGUA_INTERVALO_S;
            if (!mapa_.agua_parada()) {
                mapa_.passo_agua(rng_);
                contar_livres();  // a água pode cobrir ou descobrir terra
            }
        }
        // Plantas: tira da fila tudo que já venceu, na ordem da fila.
        while (!fila_.empty() && fila_.top().quando <= tempo_ + EPS) {
            const Evento e = fila_.top();
            fila_.pop();
            switch (e.tipo) {
                case QUEDA_MACA: {
                    Macieira& m = macieiras_[e.alvo];
                    if (!m.existe || m.seca || e.quando != m.quando_queda) break;
                    m.quando_queda += sortear_queda();
                    agendar(m.quando_queda, QUEDA_MACA, e.alvo);
                    evento_queda_maca(e.alvo);
                    break;
                }
                case MACIEIRA_SECA: {
                    Macieira& m = macieiras_[e.alvo];
                    if (!m.existe || m.seca) break;
                    evento_macieira_seca(m);  // seca não derruba mais maçã
                    agendar(m.quando_sumir, MACIEIRA_SOME, e.alvo);
                    break;
                }
                case MACIEIRA_SOME: {
                    Macieira& m = macieiras_[e.alvo];
                    if (!m.existe || !m.seca) break;
                    evento_macieira_some(m);
                    break;
                }
                case MACA_VIRA_MUDA: {
                    auto it = quando_muda_.find(e.alvo);
                    if (it == quando_muda_.end() || it->second != e.quando) break;
                    evento_maca_vira_muda(e.alvo);
                    break;
                }
                case MUDA_VIRA_ARVORE:
                    if (mudas_.count(e.alvo)) evento_muda_vira_arvore(e.alvo);
                    break;
                case VIRA_ADULTO:
                    evento_adulto(e.alvo);
                    break;
                case VELHICE:
                    if (e.quando == npcs_[e.alvo].quando_velhice) evento_velhice(e.alvo);
                    break;
                case NASCIMENTO: {
                    const Npc& m = npcs_[e.alvo];
                    if (m.estado != EstadoNpc::Morto && m.gravida &&
                        e.quando == m.quando_nascer)
                        evento_nascimento(e.alvo);
                    break;
                }
                case CONCEPCAO: {
                    const Npc& m = npcs_[e.alvo];
                    if (m.procriando_desde >= 0.0 &&
                        e.quando == m.procriando_desde + cfg::PROCRIAR_H * cfg::SEGUNDOS_POR_HORA)
                        evento_concepcao(e.alvo);
                    break;
                }
            }
        }
    }
}

// ===========================================================================
// Acontecimentos
// ===========================================================================

// A cada 5 s: fome abaixa 3, até 0.
// Enquanto está comendo, a fome NÃO abaixa (pedido do dono do projeto).
void Mundo::evento_fome(Npc& n) {
    // Comendo, ou dentro do abrigo ("dentro não faz fome"): não abaixa.
    if (n.estado == EstadoNpc::Comendo || n.dentro_abrigo >= 0) return;
    // Chegou a 0: fica em 0 (crítico — o NPC perde HP; ver taxa_hp).
    n.fome = std::max(cfg::FOME_MINIMA, n.fome - cfg::FOME_QUEDA);
}

// A cada 5 s: sede abaixa 4, até 0.
// Enquanto está bebendo, a sede NÃO abaixa.
void Mundo::evento_sede(Npc& n) {
    // Bebendo, a sede não abaixa; comendo maçã também não (a maçã mata a
    // sede um pouco — pedido do dono do projeto).
    if (n.estado == EstadoNpc::Bebendo || n.estado == EstadoNpc::Comendo ||
        n.dentro_abrigo >= 0)  // dentro do abrigo "não faz sede"
        return;
    // Chegou a 0: fica em 0 (crítico — o NPC perde HP; ver taxa_hp).
    n.sede = std::max(cfg::SEDE_MINIMA, n.sede - cfg::SEDE_QUEDA);
}

// A cada 1 s: o NPC decide e dá (no máximo) um passo.
//
// Ordem de decisão (definida pelo dono do projeto):
//   - Com fome < 70 e sede < 50 ao mesmo tempo: tenta primeiro a mais URGENTE
//     (a que fica crítica antes — tempo até chegar a 0).
//     Se não achar o que ela pede (ex.: nenhuma água à vista), tenta a outra.
//   - Só uma baixa: tenta só ela.
//   - Nada à vista (ou nada baixo): anda aleatoriamente.
// Empate: a sede vem primeiro — PROVISÓRIO, aguardando o dono.
void Mundo::evento_passo(Npc& n) {
    if (n.bebe) return;  // bebê: fica parado (por enquanto)

    // --- Dentro do abrigo: à noite dorme; de dia, só fica se está ali para
    //     procriar (esperando o parceiro ou junto com ele); senão sai -------
    if (n.dentro_abrigo >= 0) {
        // Se o casal está junto e a mulher passou a poder engravidar agora
        // (ex.: acabaram os 7 dias enquanto dormiam), começa a contar.
        ver_se_comeca_procriar(n.dentro_abrigo);
        if (!e_dia()) n.estado = EstadoNpc::Dormindo;
        else if (quer_procriar(n)) n.estado = EstadoNpc::Procriando;
        else sair_abrigo(n);
        return;
    }
    if (n.estado == EstadoNpc::Comendo || n.estado == EstadoNpc::Bebendo)
        return;  // ocupado: fica parado

    const bool com_fome = n.fome < cfg::FOME_LIMITE_COMER;
    const bool com_sede = n.sede < cfg::SEDE_LIMITE_BEBER;
    const bool com_frio = temperatura() <= cfg::TEMP_FRIO;

    // Trabalhando e apareceu uma necessidade: para, guarda o progresso e vai
    // cuidar dela; depois volta (decisão do dono do projeto).
    if (n.estado == EstadoNpc::Trabalhando) {
        if (!com_fome && !com_sede && !(com_frio && n.abrigo_casa >= 0)) return;
        pausar_trabalho(n);
    }

    conhecer_abrigos(n);
    formar_casal(n);

    // --- Necessidades, na ordem da URGÊNCIA (tempo até ficar crítica) -----
    // (frio só conta se ele tem abrigo para onde ir)
    struct Nec { double urgencia; int tipo; };  // 0 = comer, 1 = beber, 2 = abrigo
    std::vector<Nec> necs;
    if (com_fome) necs.push_back({ate_critico_fome(n), 0});
    if (com_sede) necs.push_back({ate_critico_sede(n), 1});
    if (com_frio && n.abrigo_casa >= 0) necs.push_back({ate_critico_frio(), 2});
    // Empate: abrigo, depois sede, depois fome — PROVISÓRIO.
    std::stable_sort(necs.begin(), necs.end(), [](const Nec& a, const Nec& b) {
        if (a.urgencia != b.urgencia) return a.urgencia < b.urgencia;
        return a.tipo > b.tipo;
    });
    for (const Nec& c : necs) {
        if (c.tipo == 0 && tentar_comer(n)) return;
        if (c.tipo == 1 && tentar_beber(n)) return;
        if (c.tipo == 2 && ir_para_abrigo(n)) return;
    }

    // --- Saciado (sem fome nem sede): sem abrigo, a necessidade é fazer um
    //     (por causa da temperatura). Também retoma trabalho que parou.
    if (!com_fome && !com_sede && cadeia_abrigo(n)) return;

    // --- Necessidade secundária: reprodução (o casal vai ao abrigo dele) ---
    if (tentar_procriar(n)) return;

    // --- Sem precisar comer/beber: se vê uma maçã e tem a mão livre, vai
    //     pegar para levar consigo (pedido do dono do projeto).
    if (tentar_pegar(n)) return;

    // --- Nada a resolver, ou nada à vista: explora ---------------------
    explorar(n);
}

// Explorar (pedido do dono do projeto; substitui o "andar à toa"):
//   - sem destino (ou chegou nele): sorteia uma distância de 2 a 20 e, entre
//     as células de terra que estão exatamente a essa distância, sorteia uma;
//   - anda 1 passo para a vizinha que mais aproxima do destino;
//   - se nenhum passo aproxima (barranco, água, alguém no caminho), larga o
//     destino e, no passo seguinte, sorteia outro.
void Mundo::explorar(Npc& n) {
    n.estado = EstadoNpc::Vagando;
    n.tem_alvo = false;
    n.pela_memoria = false;
    if (!n.tem_destino || distancia_passos(n.pos, n.destino) == 0) {
        if (!sortear_destino(n)) return;  // nenhum lugar possível: espera
    }
    const int agora = distancia_passos(n.pos, n.destino);
    bool achou = false;
    Pos melhor{};
    int melhor_passos = 0, melhor_d2 = 0;
    for (Pos v : VIZINHOS) {
        Pos p = com_z({n.pos.x + v.x, n.pos.y + v.y});
        if (!pode_pisar(n.pos, p)) continue;
        const int ps = distancia_passos(p, n.destino), d2 = distancia2(p, n.destino);
        if (!achou || ps < melhor_passos || (ps == melhor_passos && d2 < melhor_d2)) {
            achou = true;
            melhor = p;
            melhor_passos = ps;
            melhor_d2 = d2;
        }
    }
    if (!achou || melhor_passos >= agora) {
        n.tem_destino = false;  // não dá para chegar mais perto: outro destino
        return;
    }
    mover(n, melhor);
    if (distancia_passos(n.pos, n.destino) == 0) n.tem_destino = false;
}

bool Mundo::sortear_destino(Npc& n) {
    std::uniform_int_distribution<int> dist(cfg::EXPLORAR_DIST_MIN, cfg::EXPLORAR_DIST_MAX);
    const int r = dist(rng_);
    std::vector<Pos> opcoes;
    auto testar = [&](int x, int y) {
        Pos p{x, y};
        if (dentro(p) && mapa_.e_terra(x, y)) opcoes.push_back(com_z(p));
    };
    for (int dx = -r; dx <= r; ++dx) {
        testar(n.pos.x + dx, n.pos.y - r);
        testar(n.pos.x + dx, n.pos.y + r);
    }
    for (int dy = -r + 1; dy <= r - 1; ++dy) {
        testar(n.pos.x - r, n.pos.y + dy);
        testar(n.pos.x + r, n.pos.y + dy);
    }
    if (opcoes.empty()) return false;
    std::uniform_int_distribution<size_t> d(0, opcoes.size() - 1);
    n.destino = opcoes[d(rng_)];
    n.tem_destino = true;
    return true;
}

// Procura a maçã mais próxima à vista. Se está ao lado, começa a comer;
// senão, dá um passo na direção dela. Devolve falso se não viu maçã.
bool Mundo::tentar_comer(Npc& n) {
    // Carregando uma maçã? Come ela na hora, sem procurar (decisão do dono).
    if (n.macas_carregadas > 0) {
        n.estado = EstadoNpc::Comendo;
        n.comendo_da_mao = true;
        n.tem_alvo = false;
        n.pela_memoria = false;
        n.falta_acao = cfg::COMER_DURACAO_S;
        return true;
    }
    Pos maca;
    // Primeiro o que ele VÊ agora; se não vê maçã, usa a memória.
    if (!procurar_maca(n, maca)) return ir_a_macieira_lembrada(n);
    n.estado = EstadoNpc::IndoComer;
    n.alvo = maca;
    n.tem_alvo = true;
    n.pela_memoria = false;
    // A maçã ocupa a célula dela, então o NPC come de uma célula vizinha.
    if (distancia_passos(n.pos, maca) == 1) {
        reservas_[indice(maca)] = n.id;
        n.estado = EstadoNpc::Comendo;
        n.falta_acao = cfg::COMER_DURACAO_S;
        return true;
    }
    passo_em_direcao(n, maca);
    return true;
}

// Vê uma maçã e ainda pode carregar? Vai até ela; ao lado, pega na hora
// (a maçã sai do chão e vai para a mão). Ao pegar, já lembra a macieira de
// onde ela caiu (decisão do dono). Devolve falso se não vai pegar nada.
bool Mundo::tentar_pegar(Npc& n) {
    if (n.macas_carregadas >= cfg::MACAS_CARREGAVEIS) return false;
    Pos maca;
    if (!procurar_maca(n, maca)) return false;
    if (distancia_passos(n.pos, maca) == 1) {
        const int i = indice(maca);
        n.macas_carregadas += 1;
        n.origem_carregada = origem_maca_[i];
        tirar_maca_do_chao(i);
        lembrar_macieira(n, n.origem_carregada);
        n.estado = EstadoNpc::Vagando;
        n.tem_alvo = false;
        n.pela_memoria = false;
        return true;  // pegar gasta este passo
    }
    n.estado = EstadoNpc::IndoPegar;
    n.alvo = maca;
    n.tem_alvo = true;
    n.pela_memoria = false;
    passo_em_direcao(n, maca);
    return true;
}

void Mundo::lembrar_macieira(Npc& n, int arvore) {
    if (arvore >= 0 && macieiras_[arvore].existe &&
        std::find(n.macieiras_lembradas.begin(), n.macieiras_lembradas.end(),
                  arvore) == n.macieiras_lembradas.end())
        n.macieiras_lembradas.push_back(arvore);
}

// Igual ao comer, mas com água: o NPC não entra na água, bebe da célula
// de grama vizinha. A água não acaba e não é reservada.
bool Mundo::tentar_beber(Npc& n) {
    Pos agua;
    // Primeiro o que ele VÊ agora; se não vê água, usa a memória.
    if (!procurar_agua(n, agua)) return ir_a_agua_lembrada(n);
    n.estado = EstadoNpc::IndoBeber;
    n.alvo = agua;
    n.tem_alvo = true;
    n.pela_memoria = false;
    // Bebe da célula vizinha, se a água estiver no nível dele ou até 1 abaixo.
    const int desnivel = n.pos.z - agua.z;
    if (distancia_passos(n.pos, agua) == 1 && desnivel >= 0 &&
        desnivel <= cfg::NPC_DEGRAU_MAX) {
        n.estado = EstadoNpc::Bebendo;
        n.falta_acao = cfg::BEBER_DURACAO_S;
        return true;
    }
    passo_em_direcao(n, agua, Chegar::ParaBeber);
    return true;
}

// Com fome e sem maçã à vista: vai até a macieira lembrada mais próxima.
// Ao chegar ao lado dela, se continua sem ver maçã, marca a macieira como
// "vista vazia" e passa para a próxima lembrada (decisão do dono do projeto).
// Quando todas as lembradas estão vazias, esquece a lista de vazias e anda
// à toa neste passo; no próximo, recomeça pela mais próxima — PROVISÓRIO,
// aguardando o dono do projeto.
// Devolve falso se não lembra de nenhuma macieira (ou todas vazias agora).
bool Mundo::ir_a_macieira_lembrada(Npc& n) {
    while (true) {
        int melhor = -1, melhor_d = 0;
        for (int idx : n.macieiras_lembradas) {
            if (std::find(n.macieiras_vistas_vazias.begin(),
                          n.macieiras_vistas_vazias.end(),
                          idx) != n.macieiras_vistas_vazias.end())
                continue;
            const int d = distancia_passos(n.pos, macieiras_[idx].pos);
            if (melhor < 0 || d < melhor_d) { melhor = idx; melhor_d = d; }
        }
        if (melhor < 0) {  // nenhuma sobrando
            n.macieiras_vistas_vazias.clear();
            return false;
        }
        const Pos arvore = macieiras_[melhor].pos;
        if (melhor_d <= 1) {
            if (!macieiras_[melhor].existe) {
                // Chegou e a macieira não existe mais: esquece dela
                // (decisão do dono do projeto).
                n.macieiras_lembradas.erase(
                    std::find(n.macieiras_lembradas.begin(),
                              n.macieiras_lembradas.end(), melhor));
                continue;
            }
            // Chegou e não há maçã à vista: passa para a próxima.
            n.macieiras_vistas_vazias.push_back(melhor);
            continue;
        }
        n.estado = EstadoNpc::IndoComer;
        n.alvo = arvore;
        n.tem_alvo = true;
        n.pela_memoria = true;
        passo_em_direcao(n, arvore);
        return true;
    }
}

// Com sede e sem água à vista: vai até a água lembrada mais próxima.
// (A água não acaba; ao chegar ao lado, ele já a vê e bebe.)
bool Mundo::ir_a_agua_lembrada(Npc& n) {
    if (n.aguas_lembradas.empty()) return false;
    Pos melhor = n.aguas_lembradas.front();
    int melhor_d = distancia_passos(n.pos, melhor);
    for (Pos p : n.aguas_lembradas) {
        const int d = distancia_passos(n.pos, p);
        if (d < melhor_d) { melhor = p; melhor_d = d; }
    }
    n.estado = EstadoNpc::IndoBeber;
    n.alvo = melhor;
    n.tem_alvo = true;
    n.pela_memoria = true;
    passo_em_direcao(n, melhor, Chegar::ParaBeber);
    return true;
}

// --- Caminhos ---------------------------------------------------------------

bool Mundo::chegou(Pos q, Pos alvo, Chegar como) const {
    if (como == Chegar::NaCelula) return q.x == alvo.x && q.y == alvo.y;
    if (distancia_passos(q, alvo) != 1) return false;
    if (como == Chegar::ParaBeber) {
        const int desnivel = q.z - alvo.z;
        return desnivel >= 0 && desnivel <= cfg::NPC_DEGRAU_MAX;
    }
    return true;
}

// Busca em largura: a partir de onde o NPC está, visita as colunas vizinhas
// onde dá para pisar (livre e degrau de no máximo 1), em ordem de distância.
// O primeiro lugar que serve é o mais perto A PÉ; volta pelo caminho até o
// primeiro passo e dá esse passo.
bool Mundo::passo_em_direcao(Npc& n, Pos alvo, Chegar como) {
    if (chegou(n.pos, alvo, como)) return true;
    const int total = largura() * altura();
    std::vector<int> pai(static_cast<size_t>(total), -2);  // -2 = não visitado
    std::vector<Pos> fila;
    fila.reserve(256);
    const int ini = indice(n.pos);
    pai[ini] = -1;
    fila.push_back(n.pos);
    for (size_t k = 0; k < fila.size(); ++k) {
        const Pos cur = fila[k];
        if (k > 0 && chegou(cur, alvo, como)) {
            int i = indice(cur);
            while (pai[i] != ini) i = pai[i];  // volta até o 1º passo
            mover(n, com_z({i % largura(), i / largura()}));
            return true;
        }
        for (Pos v : VIZINHOS) {
            Pos p = com_z({cur.x + v.x, cur.y + v.y});
            if (!dentro(p) || pai[indice(p)] != -2 || !pode_pisar(cur, p)) continue;
            pai[indice(p)] = indice(cur);
            fila.push_back(p);
        }
    }
    // Sem caminho: dá o passo que mais aproxima em linha reta (como antes).
    bool achou = false;
    Pos melhor{};
    int melhor_passos = 0, melhor_d2 = 0;
    for (Pos v : VIZINHOS) {
        Pos p = com_z({n.pos.x + v.x, n.pos.y + v.y});
        if (!pode_pisar(n.pos, p)) continue;
        const int ps = distancia_passos(p, alvo), d2 = distancia2(p, alvo);
        if (!achou || ps < melhor_passos ||
            (ps == melhor_passos && d2 < melhor_d2)) {
            achou = true;
            melhor = p;
            melhor_passos = ps;
            melhor_d2 = d2;
        }
    }
    if (achou) mover(n, melhor);
    return false;
}

std::vector<int> Mundo::distancias(Pos de) const {
    std::vector<int> d(static_cast<size_t>(largura()) * altura(), -1);
    std::vector<Pos> fila{de};
    d[indice(de)] = 0;
    for (size_t k = 0; k < fila.size(); ++k) {
        const Pos cur = fila[k];
        for (Pos v : VIZINHOS) {
            Pos p = com_z({cur.x + v.x, cur.y + v.y});
            if (!dentro(p) || d[indice(p)] != -1 || !pode_pisar(cur, p)) continue;
            d[indice(p)] = d[indice(cur)] + 1;
            fila.push_back(p);
        }
    }
    return d;
}

// Terminou os 10 s comendo: a maçã some, a fome sobe 12 e a sede sobe 5
// (até 100).
void Mundo::evento_fim_comer(Npc& n) {
    if (n.comendo_da_mao) {  // comeu a maçã que carregava
        n.comendo_da_mao = false;
        n.macas_carregadas -= 1;
        n.origem_carregada = -1;
        n.macieiras_vistas_vazias.clear();
        n.fome = std::min(cfg::FOME_MAXIMA, n.fome + cfg::MACA_RECUPERA);
        n.sede = std::min(cfg::SEDE_MAXIMA, n.sede + cfg::MACA_RECUPERA_SEDE);
        n.estado = EstadoNpc::Vagando;
        n.tem_alvo = false;
        return;
    }
    const int i = indice(n.alvo);
    // Memória: lembra a macieira de onde veio esta maçã (se ainda não lembra).
    lembrar_macieira(n, origem_maca_[i]);
    n.macieiras_vistas_vazias.clear();  // comeu: a busca recomeça do zero
    reservas_.erase(i);
    tirar_maca_do_chao(i);
    n.fome = std::min(cfg::FOME_MAXIMA, n.fome + cfg::MACA_RECUPERA);
    n.sede = std::min(cfg::SEDE_MAXIMA, n.sede + cfg::MACA_RECUPERA_SEDE);
    n.estado = EstadoNpc::Vagando;
    n.tem_alvo = false;
}

// Terminou os 10 s bebendo: a sede sobe 15 (até 100). A água não acaba.
void Mundo::evento_fim_beber(Npc& n) {
    n.sede = std::min(cfg::SEDE_MAXIMA, n.sede + cfg::GOLE_RECUPERA);
    // Memória: lembra a célula de água onde bebeu (se ainda não lembra).
    if (std::find(n.aguas_lembradas.begin(), n.aguas_lembradas.end(),
                  n.alvo) == n.aguas_lembradas.end())
        n.aguas_lembradas.push_back(n.alvo);
    n.estado = EstadoNpc::Vagando;
    n.tem_alvo = false;
}

// Camada r em volta de `centro` = as células a exatamente r passos dele
// (r = 1: as 8 vizinhas). Põe em `livres` as que estão livres. Percorre só
// a BORDA da camada (8·r células), não o quadrado inteiro.
// Devolve falso quando a camada já está toda fora do mapa (não há mais
// onde procurar).
bool Mundo::camada_livres(Pos centro, int r, std::vector<Pos>& livres) const {
    livres.clear();
    if (livres_terra_ <= 0) return false;  // ilha lotada: não há onde cair
    if (centro.x - r < 0 && centro.y - r < 0 && centro.x + r >= largura() &&
        centro.y + r >= altura())
        return false;
    auto testar = [&](int x, int y) {
        Pos p{x, y};
        if (livre(p)) livres.push_back(p);
    };
    for (int dx = -r; dx <= r; ++dx) {         // linha de cima e de baixo
        testar(centro.x + dx, centro.y - r);
        testar(centro.x + dx, centro.y + r);
    }
    for (int dy = -r + 1; dy <= r - 1; ++dy) {  // lados (sem os cantos)
        testar(centro.x - r, centro.y + dy);
        testar(centro.x + r, centro.y + dy);
    }
    return true;
}

// --- Vida vegetal -------------------------------------------------------

// Toda maçã que vai para o chão passa por aqui: marca a célula, guarda de
// qual macieira veio e começa a contar as 6 horas para virar muda.
// (Maçã que foi pegada e solta de novo recomeça a contagem.)
void Mundo::por_maca_no_chao(int i, int origem) {
    ocupar(i, Ocupante::Maca);
    origem_maca_[i] = origem;
    quando_muda_[i] = tempo_ + cfg::MACA_VIRA_MUDA_H * cfg::SEGUNDOS_POR_HORA;
    agendar(quando_muda_[i], MACA_VIRA_MUDA, i);
}

// Toda maçã que sai do chão (pegada ou comida) passa por aqui.
void Mundo::tirar_maca_do_chao(int i) {
    ocupar(i, Ocupante::Nada);
    origem_maca_[i] = -1;
    quando_muda_.erase(i);
}

// 6 horas no chão sem ninguém pegar: a maçã vira muda na mesma célula.
// Se alguém está comendo ela neste momento, espera: ou ela é comida, ou
// (se quem comia morrer) vira muda nessa hora — ver Mundo::morrer.
void Mundo::evento_maca_vira_muda(int i) {
    if (reservas_.count(i)) return;
    quando_muda_.erase(i);
    origem_maca_[i] = -1;
    ocupar(i, Ocupante::Muda);
    Muda u;
    u.id = proximo_id_muda_++;
    u.pos = com_z({i % largura(), i / largura()});
    u.nasceu = tempo_;
    u.quando_virar = tempo_ + cfg::MUDA_VIRA_ARVORE_DIAS * cfg::SEGUNDOS_POR_DIA;
    mudas_[u.id] = u;
    agendar(u.quando_virar, MUDA_VIRA_ARVORE, u.id);
    registrar(TipoRegistro::MudaNasceu, -1, -1, -1, u.pos);
}

// 3 dias depois: a muda vira macieira (idade 0, derruba maçãs normalmente).
void Mundo::evento_muda_vira_arvore(int id) {
    const Pos p = mudas_.at(id).pos;
    mudas_.erase(id);
    nova_macieira(p);
    registrar(TipoRegistro::ArvoreNasceu, -1, -1, -1, p);
}

std::vector<Muda> Mundo::mudas() const {
    std::vector<Muda> r;
    r.reserve(mudas_.size());
    for (const auto& [id, u] : mudas_) r.push_back(u);
    return r;
}

// 7 dias de vida: a macieira seca (cinza) e para de derrubar maçãs.
void Mundo::evento_macieira_seca(Macieira& m) {
    m.seca = true;
    m.quando_sumir = tempo_ + cfg::MACIEIRA_SECA_H * cfg::SEGUNDOS_POR_HORA;
    ocupar(indice(m.pos), Ocupante::MacieiraSeca);
    registrar(TipoRegistro::ArvoreSecou, -1, -1, -1, m.pos);
}

// 4 horas seca: some. A célula fica livre. (Fica na lista com existe =
// false, para os números das outras macieiras não mudarem.)
void Mundo::evento_macieira_some(Macieira& m) {
    m.existe = false;
    ocupar(indice(m.pos), Ocupante::Nada);
    registrar(TipoRegistro::ArvoreSumiu, -1, -1, -1, m.pos);
}

int Mundo::macieira_em(int x, int y) const {
    for (int i = static_cast<int>(macieiras_.size()) - 1; i >= 0; --i)
        if (macieiras_[i].existe && macieiras_[i].pos.x == x && macieiras_[i].pos.y == y)
            return i;
    return -1;
}

const Muda* Mundo::muda_em(int x, int y) const {
    for (const auto& [id, u] : mudas_)
        if (u.pos.x == x && u.pos.y == y) return &u;
    return nullptr;
}

double Mundo::falta_virar_muda(int x, int y) const {
    auto it = quando_muda_.find(indice({x, y}));
    return it == quando_muda_.end() ? -1.0 : it->second - tempo_;
}

// ===========================================================================
// Abrigo e trabalho (pedra, galhos, machado, corte, construção)
// ===========================================================================

double Mundo::duracao_tarefa(Tarefa t) const {
    switch (t) {
        case Tarefa::Lascas: return cfg::TIRAR_LASCAS_S;
        case Tarefa::Galhos: return cfg::TIRAR_GALHOS_S;
        case Tarefa::Cortar: return cfg::CORTAR_H * cfg::SEGUNDOS_POR_HORA;
        case Tarefa::Construir: return cfg::CONSTRUIR_H * cfg::SEGUNDOS_POR_HORA;
        default: return 0.0;
    }
}

// Vê um abrigo com vaga de morador (até 2)? Vira morador dele. É o abrigo
// para onde ele vai quando faz frio.
void Mundo::conhecer_abrigos(Npc& n) {
    if (n.abrigo_casa >= 0) return;
    // Tem par com abrigo? Vai morar com ele (dormem juntos).
    if (n.parceiro >= 0 && npcs_[n.parceiro].abrigo_casa >= 0) {
        unir_abrigo_casal(n, npcs_[n.parceiro]);
        if (n.abrigo_casa >= 0) return;
    }
    const int metade = cfg::VISAO_LADO / 2;
    for (int a = 0; a < static_cast<int>(abrigos_.size()); ++a) {
        const Abrigo& ab = abrigos_[a];
        const int dx = ab.pos.x - n.pos.x, dy = ab.pos.y - n.pos.y;
        if (dx < -metade || dx >= metade || dy < -metade || dy >= metade) continue;
        if (static_cast<int>(ab.moradores.size()) >= cfg::ABRIGO_VAGAS) continue;
        abrigos_[a].moradores.push_back(n.id);
        n.abrigo_casa = a;
        if (n.parceiro >= 0) unir_abrigo_casal(n, npcs_[n.parceiro]);
        return;
    }
}

// Com frio: vai para o abrigo dele; ao lado, entra (se houver lugar).
bool Mundo::ir_para_abrigo(Npc& n) {
    if (n.abrigo_casa < 0) return false;
    const Abrigo& ab = abrigos_[n.abrigo_casa];
    const int desnivel = std::abs(ab.pos.z - n.pos.z);
    if (distancia_passos(n.pos, ab.pos) == 1 && desnivel <= cfg::NPC_DEGRAU_MAX &&
        static_cast<int>(ab.dentro.size()) < cfg::ABRIGO_VAGAS) {
        entrar_abrigo(n, n.abrigo_casa);
        return true;
    }
    n.estado = EstadoNpc::IndoAbrigo;
    n.tem_alvo = true;
    n.alvo = ab.pos;
    passo_em_direcao(n, ab.pos);
    return true;
}

void Mundo::entrar_abrigo(Npc& n, int a) {
    ocupar(indice(n.pos), Ocupante::Nada);  // sai da célula onde estava
    n.anterior = n.pos;
    n.pos = abrigos_[a].pos;
    n.dentro_abrigo = a;
    abrigos_[a].dentro.push_back(n.id);
    n.estado = e_dia() ? EstadoNpc::Procriando : EstadoNpc::Dormindo;
    n.tem_alvo = false;
    ver_se_comeca_procriar(a);
}

// Sai para uma célula livre ao lado do abrigo (se não houver, espera).
void Mundo::sair_abrigo(Npc& n) {
    Abrigo& ab = abrigos_[n.dentro_abrigo];
    for (Pos v : VIZINHOS) {
        Pos p = com_z({ab.pos.x + v.x, ab.pos.y + v.y});
        if (!pode_pisar(ab.pos, p)) continue;
        ab.dentro.erase(std::find(ab.dentro.begin(), ab.dentro.end(), n.id));
        n.dentro_abrigo = -1;
        // Saiu: se o casal estava junto procriando, a contagem para.
        n.procriando_desde = -1.0;
        if (n.parceiro >= 0) npcs_[n.parceiro].procriando_desde = -1.0;
        n.pos = p;
        ocupar(indice(p), Ocupante::Npc);
        n.estado = EstadoNpc::Vagando;
        return;
    }
}

// Segundos até a temperatura ficar abaixo de 4° (0 se já está).
double Mundo::ate_critico_frio() const {
    if (temperatura() < cfg::TEMP_CRITICA) return 0.0;
    const double passo = cfg::SEGUNDOS_POR_HORA / 60.0;  // 1 minuto do jogo
    for (double t = passo; t <= cfg::SEGUNDOS_POR_DIA; t += passo)
        if (temperatura_em(tempo_ + t) < cfg::TEMP_CRITICA) return t;
    return cfg::SEGUNDOS_POR_DIA;
}

// A "necessidade de abrigo" (saciado e sem abrigo), ou um trabalho parado.
bool Mundo::cadeia_abrigo(Npc& n) {
    if (n.tarefa == Tarefa::Nenhuma) {
        if (n.abrigo_casa >= 0) return false;  // já tem abrigo
        if (!escolher_tarefa(n)) return false;  // nada à vista: explora
    }
    return trabalhar(n);
}

// Qual o próximo trabalho para chegar ao abrigo?
//   lenha >= 20 -> construir aqui;  machado -> cortar uma árvore;
//   senão: lascas (até 6) e galhos (até 2) para o machado — o que estiver
//   mais perto entre os dois que faltam.
bool Mundo::escolher_tarefa(Npc& n) {
    fazer_machado(n);
    if (n.lenha >= cfg::ABRIGO_LENHA) {
        n.tarefa = Tarefa::Construir;
        n.tarefa_pos = n.pos;
        n.tarefa_alvo = -1;
        n.tarefa_restante = 0.0;
        return true;
    }
    int idx_pedra = -1, idx_arvore = -1;
    Pos p_pedra{}, p_arvore{};
    const bool quer_lascas = !n.machado && n.lascas < cfg::MACHADO_LASCAS;
    const bool quer_arvore = n.machado || n.galhos < cfg::MACHADO_GALHOS;
    const bool tem_pedra = quer_lascas && procurar_recurso(n, true, idx_pedra, p_pedra);
    const bool tem_arvore = quer_arvore && procurar_recurso(n, false, idx_arvore, p_arvore);
    bool usar_pedra = tem_pedra;
    if (tem_pedra && tem_arvore)
        usar_pedra = distancia_passos(n.pos, p_pedra) <= distancia_passos(n.pos, p_arvore);
    if (usar_pedra) {
        n.tarefa = Tarefa::Lascas;
        n.tarefa_alvo = idx_pedra;
        n.tarefa_pos = p_pedra;
    } else if (tem_arvore) {
        n.tarefa = n.machado ? Tarefa::Cortar : Tarefa::Galhos;
        n.tarefa_alvo = idx_arvore;
        n.tarefa_pos = p_arvore;
    } else {
        return false;
    }
    n.tarefa_restante = 0.0;
    return true;
}

// Procura uma pedra (pedra = true) ou árvore: primeiro a mais próxima que
// ele VÊ (32 x 32); se não vê nenhuma, a mais próxima que ele LEMBRA.
bool Mundo::procurar_recurso(const Npc& n, bool pedra, int& indice_achado,
                             Pos& onde) const {
    // "Mais perto" = menos passos A PÉ até ficar ao lado (só os que dá para
    // alcançar). Primeiro os que ele VÊ (32 x 32); se nenhum, os que LEMBRA.
    const std::vector<int> d = distancias(n.pos);
    auto passos_ate = [&](Pos alvo) {
        int melhor = -1;
        for (Pos v : VIZINHOS) {
            Pos q{alvo.x + v.x, alvo.y + v.y};
            if (!dentro(q)) continue;
            const int dq = d[indice(q)];
            if (dq >= 0 && (melhor < 0 || dq < melhor)) melhor = dq;
        }
        return melhor;
    };
    const int metade = cfg::VISAO_LADO / 2;
    int melhor_d = -1;
    for (int dy = -metade; dy < metade; ++dy)
        for (int dx = -metade; dx < metade; ++dx) {
            Pos p{n.pos.x + dx, n.pos.y + dy};
            if (!dentro(p)) continue;
            const Ocupante o = ocupacao_[indice(p)];
            const bool serve = pedra ? (o == Ocupante::Pedra)
                                     : (o == Ocupante::Macieira || o == Ocupante::MacieiraSeca);
            if (!serve) continue;
            const int passos = passos_ate(p);
            if (passos < 0) continue;  // não dá para chegar
            if (melhor_d < 0 || passos < melhor_d) {
                melhor_d = passos;
                onde = com_z(p);
                indice_achado = pedra ? pedra_em(p.x, p.y) : macieira_em(p.x, p.y);
            }
        }
    if (melhor_d >= 0) return true;
    const std::vector<int>& lembrados = pedra ? n.pedras_lembradas : n.macieiras_lembradas;
    for (int idx : lembrados) {
        const bool existe = pedra ? pedras_[idx].existe : macieiras_[idx].existe;
        const Pos p = pedra ? pedras_[idx].pos : macieiras_[idx].pos;
        // Ele não sabe se ela ainda existe: vai até lá e descobre (ver
        // Mundo::trabalhar). Só pula as que ele já VÊ que não estão lá.
        const int dx = p.x - n.pos.x, dy = p.y - n.pos.y;
        const bool a_vista = dx >= -metade && dx < metade && dy >= -metade && dy < metade;
        if (!existe && a_vista) continue;
        const int passos = passos_ate(p);
        if (passos < 0) continue;
        if (melhor_d < 0 || passos < melhor_d) {
            melhor_d = passos;
            onde = p;
            indice_achado = idx;
        }
    }
    return melhor_d >= 0;
}

// Vai até o lugar do trabalho e trabalha. Se o alvo sumiu, desiste dele
// (e esquece, se era lembrado).
bool Mundo::trabalhar(Npc& n) {
    // O alvo ainda existe?
    bool existe = true;
    if (n.tarefa == Tarefa::Lascas) existe = pedras_[n.tarefa_alvo].existe;
    if (n.tarefa == Tarefa::Galhos || n.tarefa == Tarefa::Cortar)
        existe = macieiras_[n.tarefa_alvo].existe;
    const int d = distancia_passos(n.pos, n.tarefa_pos);
    if (!existe) {
        // Só descobre que sumiu chegando perto (ou vendo que não está lá).
        if (d <= 1 || d < cfg::VISAO_LADO / 2) {
            auto& lista = (n.tarefa == Tarefa::Lascas) ? n.pedras_lembradas
                                                       : n.macieiras_lembradas;
            auto it = std::find(lista.begin(), lista.end(), n.tarefa_alvo);
            if (it != lista.end()) lista.erase(it);
            n.tarefa = Tarefa::Nenhuma;
            n.tarefa_restante = 0.0;
            return false;
        }
    }
    // Construir é na própria célula da obra; o resto, ao lado do alvo.
    const bool chegou = (n.tarefa == Tarefa::Construir) ? (d == 0) : (d == 1);
    if (!chegou) {
        if (n.tarefa == Tarefa::Construir && !livre(n.tarefa_pos) && d != 0) {
            n.estado = EstadoNpc::IndoTrabalhar;  // a obra está ocupada: espera
            return true;
        }
        n.estado = EstadoNpc::IndoTrabalhar;
        n.tem_alvo = true;
        n.alvo = n.tarefa_pos;
        const Chegar como = (n.tarefa == Tarefa::Construir) ? Chegar::NaCelula
                                                            : Chegar::AoLado;
        if (!passo_em_direcao(n, n.tarefa_pos, como)) {
            // Não há caminho até lá: larga este trabalho (escolhe outro depois).
            n.tarefa = Tarefa::Nenhuma;
            n.tarefa_restante = 0.0;
        }
        return true;
    }
    n.estado = EstadoNpc::Trabalhando;
    n.tem_alvo = true;
    n.alvo = n.tarefa_pos;
    n.falta_acao = (n.tarefa_restante > 0.0) ? n.tarefa_restante
                                             : duracao_tarefa(n.tarefa);
    n.tarefa_restante = 0.0;
    return true;
}

// Fome, sede ou frio no meio do trabalho: guarda quanto falta.
void Mundo::pausar_trabalho(Npc& n) {
    n.tarefa_restante = std::max(n.falta_acao, 0.0);
    n.estado = EstadoNpc::Vagando;
}

void Mundo::fazer_machado(Npc& n) {
    if (n.machado || n.lascas < cfg::MACHADO_LASCAS || n.galhos < cfg::MACHADO_GALHOS)
        return;
    n.lascas -= cfg::MACHADO_LASCAS;
    n.galhos -= cfg::MACHADO_GALHOS;
    n.machado = true;  // feito na hora; dura para sempre
    registrar(TipoRegistro::MachadoFeito, n.id);
}

void Mundo::evento_fim_trabalho(Npc& n) {
    const Tarefa t = n.tarefa;
    n.tarefa = Tarefa::Nenhuma;
    n.tarefa_restante = 0.0;
    n.estado = EstadoNpc::Vagando;
    n.tem_alvo = false;
    if (t == Tarefa::Lascas) {
        Pedra& p = pedras_[n.tarefa_alvo];
        if (!p.existe) return;
        const int tirou = std::min(cfg::LASCAS_POR_VEZ, p.lascas);
        p.lascas -= tirou;
        n.lascas += tirou;
        if (std::find(n.pedras_lembradas.begin(), n.pedras_lembradas.end(),
                      n.tarefa_alvo) == n.pedras_lembradas.end())
            n.pedras_lembradas.push_back(n.tarefa_alvo);
        if (p.lascas <= 0) {  // esgotou: a pedra some
            p.existe = false;
            ocupar(indice(p.pos), Ocupante::Nada);
            registrar(TipoRegistro::PedraEsgotou, n.id, -1, -1, p.pos);
        }
        fazer_machado(n);
    } else if (t == Tarefa::Galhos) {
        if (!macieiras_[n.tarefa_alvo].existe) return;
        n.galhos += cfg::GALHOS_POR_VEZ;  // a árvore não se estraga
        lembrar_macieira(n, n.tarefa_alvo);
        fazer_machado(n);
    } else if (t == Tarefa::Cortar) {
        Macieira& m = macieiras_[n.tarefa_alvo];
        if (!m.existe) return;
        m.existe = false;  // a árvore (viva ou seca) some
        ocupar(indice(m.pos), Ocupante::Nada);
        n.lenha += cfg::LENHA_POR_ARVORE;
        registrar(TipoRegistro::ArvoreCortada, n.id, -1, -1, m.pos);
    } else if (t == Tarefa::Construir) {
        n.lenha -= cfg::ABRIGO_LENHA;
        Abrigo ab;
        ab.pos = n.pos;
        ab.moradores.push_back(n.id);
        ab.dentro.push_back(n.id);  // terminou a obra lá dentro
        const int a = static_cast<int>(abrigos_.size());
        abrigos_.push_back(ab);
        ocupar(indice(n.pos), Ocupante::Abrigo);
        sair_de_morador(n);
        n.abrigo_casa = a;
        n.dentro_abrigo = a;
        registrar(TipoRegistro::AbrigoFeito, n.id, -1, -1, n.pos);
        if (n.parceiro >= 0) unir_abrigo_casal(n, npcs_[n.parceiro]);
    }
}

int Mundo::pedra_em(int x, int y) const {
    for (int i = 0; i < static_cast<int>(pedras_.size()); ++i)
        if (pedras_[i].existe && pedras_[i].pos.x == x && pedras_[i].pos.y == y) return i;
    return -1;
}

int Mundo::abrigo_em(int x, int y) const {
    for (int i = 0; i < static_cast<int>(abrigos_.size()); ++i)
        if (abrigos_[i].pos.x == x && abrigos_[i].pos.y == y) return i;
    return -1;
}

// ===========================================================================
// Família: casal, procriação, gravidez, bebê, adulto, velhice
// ===========================================================================

void Mundo::registrar(TipoRegistro t, int a, int b, int c, Pos pos) {
    Registro r;
    r.tempo = tempo_;
    r.tipo = t;
    r.a = a;
    r.b = b;
    r.c = c;
    r.pos = pos;
    historico_.push_back(r);
}

double Mundo::idade_anos(const Npc& n) const {
    const double agora = n.morreu_em >= 0.0 ? n.morreu_em : tempo_;
    return n.idade_ref_anos + (agora - n.idade_ref_t) / cfg::SEGUNDOS_POR_ANO;
}

// Célula livre mais perto de `centro` (camadas em volta; sorteada na camada).
bool Mundo::lugar_livre_perto(Pos centro, Pos& achado) {
    std::vector<Pos> opcoes;
    for (int r = 1; camada_livres(centro, r, opcoes); ++r) {
        if (!opcoes.empty()) {
            std::uniform_int_distribution<size_t> d(0, opcoes.size() - 1);
            achado = opcoes[d(rng_)];
            return true;
        }
    }
    return false;
}

void Mundo::sair_de_morador(Npc& n) {
    if (n.abrigo_casa < 0) return;
    auto& mor = abrigos_[n.abrigo_casa].moradores;
    auto it = std::find(mor.begin(), mor.end(), n.id);
    if (it != mor.end()) mor.erase(it);
    n.abrigo_casa = -1;
}

// Casal: um homem e uma mulher adultos, os dois sem par, que se veem na
// mesma área (32 x 32), viram casal na hora (decisão do dono).
void Mundo::formar_casal(Npc& n) {
    if (n.bebe || n.parceiro >= 0) return;
    const int metade = cfg::VISAO_LADO / 2;
    int melhor = -1, melhor_d = 0;
    for (const Npc& o : npcs_) {
        if (o.id == n.id || o.bebe || o.estado == EstadoNpc::Morto) continue;
        if (o.sexo == n.sexo || o.parceiro >= 0) continue;
        const int dx = o.pos.x - n.pos.x, dy = o.pos.y - n.pos.y;
        if (dx < -metade || dx >= metade || dy < -metade || dy >= metade) continue;
        const int d = distancia_passos(n.pos, o.pos);
        if (melhor < 0 || d < melhor_d) { melhor = o.id; melhor_d = d; }
    }
    if (melhor < 0) return;
    Npc& o = npcs_[melhor];
    n.parceiro = o.id;
    o.parceiro = n.id;
    const Npc& h = (n.sexo == Sexo::Homem) ? n : o;
    const Npc& m = (n.sexo == Sexo::Homem) ? o : n;
    registrar(TipoRegistro::Casal, h.id, m.id);
    unir_abrigo_casal(n, o);
}

// O casal dorme junto: passa a ter o mesmo abrigo. Se os dois têm abrigo,
// fica o mais perto dos dois (PROVISÓRIO); se só um tem, o outro vai para
// ele; se nenhum tem, isso acontece quando um deles conseguir um.
void Mundo::unir_abrigo_casal(Npc& a, Npc& b) {
    if (a.abrigo_casa >= 0 && a.abrigo_casa == b.abrigo_casa) return;
    std::vector<int> opcoes;
    if (a.abrigo_casa >= 0) opcoes.push_back(a.abrigo_casa);
    if (b.abrigo_casa >= 0) opcoes.push_back(b.abrigo_casa);
    auto custo = [&](int ab) {
        return distancia_passos(a.pos, abrigos_[ab].pos) +
               distancia_passos(b.pos, abrigos_[ab].pos);
    };
    std::sort(opcoes.begin(), opcoes.end(),
              [&](int x, int y) { return custo(x) < custo(y); });
    for (int alvo : opcoes) {
        Npc& fora = (a.abrigo_casa == alvo) ? b : a;  // quem precisa mudar
        int outros = 0;
        for (int id : abrigos_[alvo].moradores)
            if (id != fora.id) ++outros;
        if (outros >= cfg::ABRIGO_VAGAS) continue;  // cheio: tenta o outro
        sair_de_morador(fora);
        abrigos_[alvo].moradores.push_back(fora.id);
        fora.abrigo_casa = alvo;
        return;
    }
}

// Quer procriar agora? Adulto, com par vivo, o casal com o mesmo abrigo,
// sem precisar comer nem beber, e a mulher podendo engravidar (não está
// grávida e já passaram 7 dias do último parto).
bool Mundo::quer_procriar(const Npc& n) const {
    if (n.bebe || n.parceiro < 0) return false;
    const Npc& p = npcs_[n.parceiro];
    if (p.estado == EstadoNpc::Morto || p.bebe) return false;
    if (n.abrigo_casa < 0 || n.abrigo_casa != p.abrigo_casa) return false;
    if (n.dentro_abrigo < 0 &&
        (n.fome < cfg::FOME_LIMITE_COMER || n.sede < cfg::SEDE_LIMITE_BEBER))
        return false;
    const Npc& m = (n.sexo == Sexo::Mulher) ? n : p;
    return !m.gravida && tempo_ >= m.pode_engravidar_em;
}

// Vai ao abrigo do casal e entra (a qualquer hora do dia). Se está cheio,
// espera ao lado.
bool Mundo::tentar_procriar(Npc& n) {
    if (!quer_procriar(n)) return false;
    const Abrigo& ab = abrigos_[n.abrigo_casa];
    const int desnivel = std::abs(ab.pos.z - n.pos.z);
    if (distancia_passos(n.pos, ab.pos) == 1 && desnivel <= cfg::NPC_DEGRAU_MAX) {
        if (static_cast<int>(ab.dentro.size()) < cfg::ABRIGO_VAGAS)
            entrar_abrigo(n, n.abrigo_casa);
        else
            n.estado = EstadoNpc::IndoProcriar;  // cheio: espera
        return true;
    }
    n.estado = EstadoNpc::IndoProcriar;
    n.tem_alvo = true;
    n.alvo = ab.pos;
    passo_em_direcao(n, ab.pos);
    return true;
}

// O casal está junto dentro do abrigo (de dia, ou dormindo à noite) e a
// mulher pode engravidar: começa a contar 1 hora.
void Mundo::ver_se_comeca_procriar(int a) {
    for (int id : abrigos_[a].dentro) {
        Npc& m = npcs_[id];
        if (m.sexo != Sexo::Mulher || m.parceiro < 0 || m.procriando_desde >= 0.0) continue;
        const Npc& h = npcs_[m.parceiro];
        if (h.dentro_abrigo != a) continue;
        if (m.gravida || tempo_ < m.pode_engravidar_em) continue;
        m.procriando_desde = tempo_;
        agendar(tempo_ + cfg::PROCRIAR_H * cfg::SEGUNDOS_POR_HORA, CONCEPCAO, m.id);
    }
}

// 1 hora juntos: a mulher engravida (sempre dá certo).
void Mundo::evento_concepcao(int mae) {
    Npc& m = npcs_[mae];
    m.procriando_desde = -1.0;
    if (m.estado == EstadoNpc::Morto || m.parceiro < 0) return;
    const Npc& h = npcs_[m.parceiro];
    if (m.dentro_abrigo < 0 || h.dentro_abrigo != m.dentro_abrigo) return;
    if (m.gravida || tempo_ < m.pode_engravidar_em) return;
    m.gravida = true;
    m.gravidez_pai = h.id;
    m.quando_nascer = tempo_ + cfg::GRAVIDEZ_DIAS * cfg::SEGUNDOS_POR_DIA;
    agendar(m.quando_nascer, NASCIMENTO, m.id);
    registrar(TipoRegistro::Gravidez, m.id, h.id);
}

// 3 dias depois: nasce um bebê ao lado da mãe (sexo meio a meio).
// Nome: "F." + nome da mãe; do 2º filho em diante, "2F.", "3F."...
void Mundo::evento_nascimento(int mae) {
    Pos onde;
    if (!lugar_livre_perto(npcs_[mae].pos, onde)) return;  // sem lugar (PROVISÓRIO)
    Npc& m = npcs_[mae];
    m.gravida = false;
    m.pode_engravidar_em = tempo_ + cfg::INTERVALO_FILHOS_DIAS * cfg::SEGUNDOS_POR_DIA;
    m.filhos += 1;
    Npc b;
    b.id = static_cast<int>(npcs_.size());
    b.nome = (m.filhos == 1 ? std::string("F.") : std::to_string(m.filhos) + "F.") + m.nome;
    std::uniform_int_distribution<int> moeda(0, 1);
    b.sexo = moeda(rng_) ? Sexo::Mulher : Sexo::Homem;
    b.bebe = true;
    b.estado = EstadoNpc::Bebe;
    b.nasceu = tempo_;
    b.idade_ref_anos = 0.0;
    b.idade_ref_t = tempo_;
    b.mae = m.id;
    b.pai = m.gravidez_pai;
    b.hp = cfg::HP_INICIAL;
    b.fome = cfg::FOME_INICIAL;
    b.sede = cfg::SEDE_INICIAL;
    b.pos = com_z(onde);
    const int pai = m.gravidez_pai;
    m.gravidez_pai = -1;
    ocupar(indice(b.pos), Ocupante::Npc);
    npcs_.push_back(b);  // (depois disso, "m" não vale mais: a lista cresceu)
    agendar(tempo_ + cfg::BEBE_VIRA_ADULTO_DIAS * cfg::SEGUNDOS_POR_DIA, VIRA_ADULTO, b.id);
    registrar(TipoRegistro::Nascimento, b.id, mae, pai);
}

// 10 dias de vida: vira adulto e "salta" para 20 anos. Morre aos 60.
void Mundo::evento_adulto(int id) {
    Npc& n = npcs_[id];
    if (n.estado == EstadoNpc::Morto || !n.bebe) return;
    n.bebe = false;
    n.estado = EstadoNpc::Vagando;
    n.idade_ref_anos = cfg::IDADE_AO_VIRAR_ADULTO;
    n.idade_ref_t = tempo_;
    n.falta_passo = cfg::NPC_PASSO_S;
    n.falta_fome = cfg::FOME_INTERVALO_S;
    n.falta_sede = cfg::SEDE_INTERVALO_S;
    n.quando_velhice = tempo_ + (cfg::IDADE_MAXIMA_ANOS - cfg::IDADE_AO_VIRAR_ADULTO) *
                                    cfg::SEGUNDOS_POR_ANO;
    agendar(n.quando_velhice, VELHICE, id);
    registrar(TipoRegistro::VirouAdulto, id);
}

// 60 anos: morre de velhice.
void Mundo::evento_velhice(int id) {
    Npc& n = npcs_[id];
    if (n.estado == EstadoNpc::Morto) return;
    morrer(n, 8);
}

// Uma maçã cai em volta da macieira: na primeira camada que tiver célula
// de grama livre (camada 1 = as 8 vizinhas; se lotada, camada 2; e assim
// por diante). Dentro da camada, a célula é sorteada.
void Mundo::evento_queda_maca(int indice_macieira) {
    const Macieira& m = macieiras_[indice_macieira];
    std::vector<Pos> opcoes;
    for (int r = 1; camada_livres(m.pos, r, opcoes); ++r) {
        if (!opcoes.empty()) {
            std::uniform_int_distribution<size_t> d(0, opcoes.size() - 1);
            por_maca_no_chao(indice(opcoes[d(rng_)]), indice_macieira);
            return;
        }
    }
}

// ===========================================================================
// Ajudantes
// ===========================================================================

// Converte os segundos da simulação em dia, hora e minuto do jogo.
Mundo::Relogio Mundo::relogio() const {
    const double total = tempo_ + cfg::HORA_INICIAL * cfg::SEGUNDOS_POR_HORA;
    const long long minutos =
        static_cast<long long>(total / (cfg::SEGUNDOS_POR_HORA / 60.0) + 1e-9);
    const long long minutos_por_dia = 60LL * cfg::HORAS_POR_DIA;
    Relogio r;
    r.dia = static_cast<int>(minutos / minutos_por_dia) + 1;
    r.hora = static_cast<int>((minutos % minutos_por_dia) / 60);
    r.minuto = static_cast<int>(minutos % 60);
    return r;
}

// --- Dia, noite, temperatura, HP -------------------------------------------

// Hora do dia (0 a 24, com fração) no tempo t da simulação.
double Mundo::hora_do_dia(double t) const {
    const double total_h = t / cfg::SEGUNDOS_POR_HORA + cfg::HORA_INICIAL;
    return std::fmod(total_h, static_cast<double>(cfg::HORAS_POR_DIA));
}

bool Mundo::e_dia() const {
    const double h = hora_do_dia(tempo_);
    return h >= cfg::DIA_COMECA_H && h < cfg::NOITE_COMECA_H;
}

// Temperatura em linha reta entre os pontos do config.hpp (dando a volta
// na meia-noite: depois do último ponto vem o primeiro do dia seguinte).
double Mundo::temperatura_em(double t) const {
    const double h = hora_do_dia(t);
    for (int k = 0; k < cfg::TEMP_PONTOS; ++k) {
        const int prox = (k + 1) % cfg::TEMP_PONTOS;
        double h0 = cfg::TEMP_HORAS[k], h1 = cfg::TEMP_HORAS[prox];
        double hh = h;
        if (h1 <= h0) {  // trecho que passa da meia-noite
            h1 += cfg::HORAS_POR_DIA;
            if (hh < h0) hh += cfg::HORAS_POR_DIA;
        }
        if (hh >= h0 && hh < h1) {
            const double f = (hh - h0) / (h1 - h0);
            return cfg::TEMP_GRAUS[k] + f * (cfg::TEMP_GRAUS[prox] - cfg::TEMP_GRAUS[k]);
        }
    }
    return cfg::TEMP_GRAUS[0];
}

// Próximo momento em que a temperatura cruza 4° ou 5°, ou que dá 06:00.
// Procura de minuto em minuto do jogo e afina no fim (a curva é feita de
// retas, então isso acha o cruzamento com folga de precisão).
double Mundo::proximo_marco_temperatura() const {
    if (marco_temp_ > tempo_ + 1e-9) return marco_temp_;  // ainda vale
    const double passo = cfg::SEGUNDOS_POR_HORA / 60.0;  // 1 minuto do jogo
    auto lado = [&](double t, double limite) { return temperatura_em(t) < limite; };
    // 06:00 (quando o NPC sai do abrigo)
    double h = hora_do_dia(tempo_);
    double faltam_h = cfg::DIA_COMECA_H - h;
    if (faltam_h <= 1e-9) faltam_h += cfg::HORAS_POR_DIA;
    double melhor = tempo_ + faltam_h * cfg::SEGUNDOS_POR_HORA;
    for (double limite : {cfg::TEMP_CRITICA, cfg::TEMP_FRIO}) {
        const bool agora = lado(tempo_ + 1e-6, limite);
        for (double t = tempo_ + passo; t < melhor; t += passo) {
            if (lado(t, limite) != agora) {
                double a = t - passo, b = t;  // afina por bisseção
                for (int i = 0; i < 40; ++i) {
                    const double m = (a + b) / 2;
                    (lado(m, limite) != agora ? b : a) = m;
                }
                melhor = std::min(melhor, b);
                break;
            }
        }
    }
    marco_temp_ = melhor;
    return melhor;
}

// HP: −5/s por necessidade no crítico (fome 0, sede 0, frio < 4°).
double Mundo::taxa_hp(const Npc& n) const {
    if (n.bebe) return 0.0;  // bebê: sem necessidades (por enquanto)
    // Dentro do abrigo: nada é crítico (nem frio, nem fome, nem sede) e o
    // HP se recupera 1 por segundo, até 100.
    if (n.dentro_abrigo >= 0)
        return n.hp < cfg::HP_MAXIMO ? cfg::HP_RECUPERA_ABRIGO_S : 0.0;
    int criticos = 0;
    if (n.fome <= cfg::FOME_MINIMA) ++criticos;
    if (n.sede <= cfg::SEDE_MINIMA) ++criticos;
    if (temperatura() < cfg::TEMP_CRITICA) ++criticos;
    return -cfg::HP_DANO_POR_CRITICO_S * criticos;
}

double Mundo::ate_critico_fome(const Npc& n) const {
    return n.fome / (cfg::FOME_QUEDA / cfg::FOME_INTERVALO_S);
}

double Mundo::ate_critico_sede(const Npc& n) const {
    return n.sede / (cfg::SEDE_QUEDA / cfg::SEDE_INTERVALO_S);
}

int Mundo::largura() const { return cfg::LARGURA; }
int Mundo::altura() const { return cfg::ALTURA; }

bool Mundo::dentro(Pos p) const {
    return p.x >= 0 && p.y >= 0 && p.x < largura() && p.y < altura();
}

Ocupante Mundo::ocupante(int x, int y) const { return ocupacao_[indice({x, y})]; }

Pos Mundo::com_z(Pos p) const {
    if (dentro(p)) p.z = mapa_.superficie(p.x, p.y);
    return p;
}

// Livre = dentro do mapa, terra firme (não é água) e sem nada em cima.
bool Mundo::livre(Pos p) const {
    if (!dentro(p)) return false;
    return mapa_.e_terra(p.x, p.y) && ocupacao_[indice(p)] == Ocupante::Nada;
}

// Passar de uma coluna para a vizinha: precisa estar livre e o degrau
// (diferença de altura) ser de no máximo 1 nível.
bool Mundo::pode_pisar(Pos de, Pos para) const {
    if (!livre(para)) return false;
    const int degrau = std::abs(mapa_.superficie(para.x, para.y) - de.z);
    return degrau <= cfg::NPC_DEGRAU_MAX;
}

// A maçã já está sendo comida por outro NPC?
bool Mundo::maca_reservada(Pos p, int por_quem_nao) const {
    auto it = reservas_.find(indice(p));
    return it != reservas_.end() && it->second != por_quem_nao;
}

// Procura a maçã mais próxima dentro da área de visão (32 x 32 em volta do
// NPC: 16 células para um lado e 15 para o outro, somando 32 com a dele).
// Maçãs que outro NPC já está comendo não contam.
bool Mundo::procurar_maca(const Npc& n, Pos& achada) const {
    const int metade = cfg::VISAO_LADO / 2;
    bool achou = false;
    int melhor_passos = 0, melhor_d2 = 0;
    for (int dy = -metade; dy < metade; ++dy)
        for (int dx = -metade; dx < metade; ++dx) {
            Pos p{n.pos.x + dx, n.pos.y + dy};
            if (!dentro(p) || ocupacao_[indice(p)] != Ocupante::Maca) continue;
            if (maca_reservada(p, n.id)) continue;
            const int ps = distancia_passos(n.pos, p), d2 = distancia2(n.pos, p);
            if (!achou || ps < melhor_passos ||
                (ps == melhor_passos && d2 < melhor_d2)) {
                achou = true;
                achada = p;
                melhor_passos = ps;
                melhor_d2 = d2;
            }
        }
    return achou;
}

// Procura a célula de água mais próxima na mesma área de visão (32 x 32).
// Só conta água que tenha ao lado uma célula de onde dá para beber: grama
// livre, ou a própria célula onde o NPC já está.
bool Mundo::procurar_agua(const Npc& n, Pos& achada) const {
    const int metade = cfg::VISAO_LADO / 2;
    bool achou = false;
    int melhor_passos = 0, melhor_d2 = 0;
    for (int dy = -metade; dy < metade; ++dy)
        for (int dx = -metade; dx < metade; ++dx) {
            Pos p{n.pos.x + dx, n.pos.y + dy};
            if (!dentro(p)) continue;
            p = com_z(p);
            // Água à vista = o topo da coluna é água.
            if (mapa_.material(p.x, p.y, p.z) != Material::Agua) continue;
            // Precisa ter ao lado uma célula de terra de onde dá para beber:
            // no nível da água ou até 1 acima.
            bool da_para_beber = false;
            for (Pos v : VIZINHOS) {
                Pos q = com_z({p.x + v.x, p.y + v.y});
                if (!dentro(q) || !(q == n.pos || livre(q))) continue;
                const int desnivel = q.z - p.z;
                if (desnivel >= 0 && desnivel <= cfg::NPC_DEGRAU_MAX) {
                    da_para_beber = true;
                    break;
                }
            }
            if (!da_para_beber) continue;
            const int ps = distancia_passos(n.pos, p), d2 = distancia2(n.pos, p);
            if (!achou || ps < melhor_passos ||
                (ps == melhor_passos && d2 < melhor_d2)) {
                achou = true;
                achada = p;
                melhor_passos = ps;
                melhor_d2 = d2;
            }
        }
    return achou;
}

// Move para a coluna vizinha. Se mudou de nível (subiu ou desceu 1), o
// próximo passo só vem depois de 5 s em vez de 1 s.
void Mundo::mover(Npc& n, Pos destino) {
    destino = com_z(destino);
    ocupar(indice(n.pos), Ocupante::Nada);
    if (destino.z != n.pos.z)
        n.falta_passo += cfg::NPC_SUBIR_S - cfg::NPC_PASSO_S;
    n.anterior = n.pos;
    n.tem_anterior = true;
    n.pos = destino;
    ocupar(indice(n.pos), Ocupante::Npc);
}

// Morre (de fome ou de sede): o corpo fica no mapa ocupando a célula.
// Se estava comendo, larga a maçã (ela continua lá para outro NPC).
// Morre (HP 0, ou velhice). O corpo fica no mapa ocupando a célula.
// - Se carregava uma maçã (mesmo comendo ela), a maçã cai ao lado do corpo.
// - Se estava grávida, o bebê morre junto (decisão do dono).
// - O par fica livre para formar outro casal; a vaga de morador abre.
// - Morreu dentro do abrigo: o corpo fica numa célula livre ao lado.
void Mundo::morrer(Npc& n, int causa_extra) {
    if (n.estado == EstadoNpc::Comendo && !n.comendo_da_mao) {
        const int i = indice(n.alvo);
        reservas_.erase(i);
        // Se o prazo de virar muda já passou enquanto ele comia, vira agora.
        auto it = quando_muda_.find(i);
        if (it != quando_muda_.end() && it->second <= tempo_ + EPS)
            evento_maca_vira_muda(i);
    }
    n.causa_morte = causa_extra;
    n.morreu_em = tempo_;
    if (!n.bebe && n.dentro_abrigo < 0) {
        n.causa_morte |= (n.fome <= cfg::FOME_MINIMA ? 1 : 0) |
                         (n.sede <= cfg::SEDE_MINIMA ? 2 : 0) |
                         (temperatura() < cfg::TEMP_CRITICA ? 4 : 0);
    }
    n.estado = EstadoNpc::Morto;
    n.tem_alvo = false;
    n.comendo_da_mao = false;
    if (n.gravida) {
        n.gravida = false;
        registrar(TipoRegistro::BebePerdido, n.id);
    }
    if (n.parceiro >= 0) {
        npcs_[n.parceiro].parceiro = -1;
        npcs_[n.parceiro].procriando_desde = -1.0;
        n.parceiro = -1;
    }
    n.procriando_desde = -1.0;
    sair_de_morador(n);
    bool tem_corpo = true;
    if (n.dentro_abrigo >= 0) {
        Abrigo& ab = abrigos_[n.dentro_abrigo];
        ab.dentro.erase(std::find(ab.dentro.begin(), ab.dentro.end(), n.id));
        n.dentro_abrigo = -1;
        Pos fora;
        tem_corpo = lugar_livre_perto(ab.pos, fora);
        if (tem_corpo) n.pos = com_z(fora);
    }
    if (tem_corpo) ocupar(indice(n.pos), Ocupante::Corpo);
    while (n.macas_carregadas > 0 && soltar_maca_perto(n.pos, n.origem_carregada))
        n.macas_carregadas -= 1;
    n.origem_carregada = -1;
    registrar(TipoRegistro::Morte, n.id, n.causa_morte);
}

// Põe uma maçã no chão na célula livre mais perto de `onde` (camada 1 =
// as 8 vizinhas; se lotada, a próxima camada), sorteada dentro da camada.
bool Mundo::soltar_maca_perto(Pos onde, int origem) {
    std::vector<Pos> opcoes;
    for (int r = 1; camada_livres(onde, r, opcoes); ++r) {
        if (!opcoes.empty()) {
            std::uniform_int_distribution<size_t> d(0, opcoes.size() - 1);
            por_maca_no_chao(indice(opcoes[d(rng_)]), origem);
            return true;
        }
    }
    return false;
}

std::vector<Pos> Mundo::macas() const {
    std::vector<Pos> r;
    for (int y = 0; y < altura(); ++y)
        for (int x = 0; x < largura(); ++x)
            if (ocupacao_[indice({x, y})] == Ocupante::Maca)
                r.push_back(com_z({x, y}));
    return r;
}

std::vector<std::uint8_t> Mundo::visao(int z) const {
    std::vector<std::uint8_t> r(static_cast<size_t>(largura()) * altura());
    for (int y = 0; y < altura(); ++y)
        for (int x = 0; x < largura(); ++x) {
            const Mapa3D::Visto v = mapa_.ver(x, y, z);
            std::uint8_t b = static_cast<std::uint8_t>(v.material);
            if (v.profundidade > 0) b |= 1u << 4;
            // O ocupante fica na superfície: aparece se é ela que se vê.
            if (ocupacao_[indice({x, y})] != Ocupante::Nada &&
                v.z == mapa_.superficie(x, y))
                b |= 1u << 5;
            r[indice({x, y})] = b;
        }
    return r;
}

void Mundo::teste_npc(int id, double fome, double sede, double hp) {
    npcs_[id].fome = fome;
    npcs_[id].sede = sede;
    npcs_[id].hp = hp;
}

void Mundo::teste_dar(int id, int lascas, int galhos, int lenha, bool machado) {
    npcs_[id].lascas = lascas;
    npcs_[id].galhos = galhos;
    npcs_[id].lenha = lenha;
    npcs_[id].machado = machado;
}

// Só para testes: põe o NPC numa célula livre (fora do abrigo).
bool Mundo::teste_mover(int id, int x, int y) {
    Npc& n = npcs_[id];
    Pos p{x, y};
    if (n.dentro_abrigo >= 0 || !livre(p)) return false;
    ocupar(indice(n.pos), Ocupante::Nada);
    n.pos = com_z(p);
    ocupar(indice(n.pos), Ocupante::Npc);
    n.tem_destino = false;
    return true;
}

// Só para testes: muda a idade (anos) e reagenda a velhice a partir dela.
void Mundo::teste_idade(int id, double anos) {
    Npc& n = npcs_[id];
    n.idade_ref_anos = anos;
    n.idade_ref_t = tempo_;
    n.quando_velhice = tempo_ + (cfg::IDADE_MAXIMA_ANOS - anos) * cfg::SEGUNDOS_POR_ANO;
    agendar(n.quando_velhice, VELHICE, id);
}

void Mundo::teste_definir(int x, int y, int z, Material m, int nivel_agua) {
    mapa_.definir(x, y, z, m, nivel_agua);
    mapa_.recalcular_superficie(x, y);
    contar_livres();
}

}  // namespace genesis
