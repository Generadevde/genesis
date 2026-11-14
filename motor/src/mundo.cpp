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
    MUDA_VIRA_ARVORE = 4 // alvo = id da muda
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

Mundo::Mundo(std::uint64_t semente) : rng_(semente) {
    gerar_mundo(mapa_, semente);
    const size_t colunas = static_cast<size_t>(largura()) * altura();
    ocupacao_.assign(colunas, Ocupante::Nada);
    origem_maca_.assign(colunas, -1);
    contar_livres();
    falta_agua_ = cfg::AGUA_INTERVALO_S;

    // 6 macieiras em células de terra livres, sorteadas ("espalhadas").
    // Todas nascem com idade 0 (decisão do dono do projeto).
    for (int i = 0; i < cfg::NUM_MACIEIRAS; ++i) nova_macieira(sortear_grama_livre());

    // 4 NPCs em células de grama livres, com fome e sede iniciais 100.
    for (int i = 0; i < cfg::NUM_NPCS; ++i) {
        Npc n;
        n.id = i;
        n.nome = cfg::NOMES_NPC[i];
        n.pos = sortear_grama_livre();
        n.hp = cfg::HP_INICIAL;
        n.fome = cfg::FOME_INICIAL;
        n.sede = cfg::SEDE_INICIAL;
        n.falta_passo = cfg::NPC_PASSO_S;
        n.falta_fome = cfg::FOME_INTERVALO_S;
        n.falta_sede = cfg::SEDE_INTERVALO_S;
        ocupar(indice(n.pos), Ocupante::Npc);
        npcs_.push_back(n);
    }
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
            if (n.estado == EstadoNpc::Morto) continue;
            proximo = std::min({proximo, n.falta_fome, n.falta_sede,
                                n.falta_passo});
            if (n.estado == EstadoNpc::Comendo || n.estado == EstadoNpc::Bebendo)
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
            if (n.estado == EstadoNpc::Morto) continue;
            taxas[k] = taxa_hp(n);
            if (taxas[k] < 0) proximo = std::min(proximo, n.hp / -taxas[k]);
        }
        proximo = std::max(proximo, 0.0);

        // 2) Anda o relógio até ele (ou até o fim do dt, o que vier antes).
        const double passo = std::min(proximo, restante);
        tempo_ += passo;
        restante -= passo;
        for (Npc& n : npcs_) {
            if (n.estado == EstadoNpc::Morto) continue;
            n.falta_fome -= passo;
            n.falta_sede -= passo;
            n.falta_passo -= passo;
            if (n.estado == EstadoNpc::Comendo || n.estado == EstadoNpc::Bebendo)
                n.falta_acao -= passo;
        }
        for (size_t k = 0; k < npcs_.size(); ++k) {
            Npc& n = npcs_[k];
            if (n.estado == EstadoNpc::Morto) continue;
            n.hp = std::min(cfg::HP_MAXIMO, n.hp + taxas[k] * passo);
            if (n.hp <= EPS) {  // HP chegou a 0: morre
                n.hp = 0.0;
                morrer(n);
            }
        }
        falta_agua_ -= passo;

        // 3) Dispara tudo que venceu: NPCs, água, depois as plantas.
        for (Npc& n : npcs_) {
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
    if (n.estado == EstadoNpc::Comendo) return;
    // Chegou a 0: fica em 0 (crítico — o NPC perde HP; ver taxa_hp).
    n.fome = std::max(cfg::FOME_MINIMA, n.fome - cfg::FOME_QUEDA);
}

// A cada 5 s: sede abaixa 4, até 0.
// Enquanto está bebendo, a sede NÃO abaixa.
void Mundo::evento_sede(Npc& n) {
    // Bebendo, a sede não abaixa; comendo maçã também não (a maçã mata a
    // sede um pouco — pedido do dono do projeto).
    if (n.estado == EstadoNpc::Bebendo || n.estado == EstadoNpc::Comendo) return;
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
    if (n.estado == EstadoNpc::Comendo || n.estado == EstadoNpc::Bebendo)
        return;  // ocupado: fica parado

    const bool com_fome = n.fome < cfg::FOME_LIMITE_COMER;
    const bool com_sede = n.sede < cfg::SEDE_LIMITE_BEBER;
    if (com_fome && com_sede) {
        // Prioridade pela URGÊNCIA: a que vai ficar crítica antes vem
        // primeiro (decisão do dono do projeto).
        if (ate_critico_sede(n) <= ate_critico_fome(n)) {
            if (tentar_beber(n) || tentar_comer(n)) return;
        } else {
            if (tentar_comer(n) || tentar_beber(n)) return;
        }
    } else if (com_fome) {
        if (tentar_comer(n)) return;
    } else if (com_sede) {
        if (tentar_beber(n)) return;
    }

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
    passo_em_direcao(n, agua);
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
    passo_em_direcao(n, melhor);
    return true;
}

// Um passo para a vizinha livre que mais aproxima do alvo.
// Sem vizinha livre: fica parado e tenta de novo no próximo passo.
void Mundo::passo_em_direcao(Npc& n, Pos alvo) {
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
}

// 3 dias depois: a muda vira macieira (idade 0, derruba maçãs normalmente).
void Mundo::evento_muda_vira_arvore(int id) {
    const Pos p = mudas_.at(id).pos;
    mudas_.erase(id);
    nova_macieira(p);
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
}

// 4 horas seca: some. A célula fica livre. (Fica na lista com existe =
// false, para os números das outras macieiras não mudarem.)
void Mundo::evento_macieira_some(Macieira& m) {
    m.existe = false;
    ocupar(indice(m.pos), Ocupante::Nada);
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
// Se carregava uma maçã (mesmo se estava comendo ela), a maçã cai numa
// célula livre ao lado do corpo (decisão do dono do projeto).
void Mundo::morrer(Npc& n) {
    if (n.estado == EstadoNpc::Comendo && !n.comendo_da_mao) {
        const int i = indice(n.alvo);
        reservas_.erase(i);
        // Se o prazo de virar muda já passou enquanto ele comia, vira agora.
        auto it = quando_muda_.find(i);
        if (it != quando_muda_.end() && it->second <= tempo_ + EPS)
            evento_maca_vira_muda(i);
    }
    n.causa_morte = (n.fome <= cfg::FOME_MINIMA ? 1 : 0) |
                    (n.sede <= cfg::SEDE_MINIMA ? 2 : 0) |
                    (temperatura() < cfg::TEMP_CRITICA ? 4 : 0);
    n.estado = EstadoNpc::Morto;
    n.tem_alvo = false;
    n.comendo_da_mao = false;
    ocupar(indice(n.pos), Ocupante::Corpo);
    while (n.macas_carregadas > 0 && soltar_maca_perto(n.pos, n.origem_carregada))
        n.macas_carregadas -= 1;
    n.origem_carregada = -1;
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

void Mundo::teste_definir(int x, int y, int z, Material m, int nivel_agua) {
    mapa_.definir(x, y, z, m, nivel_agua);
    mapa_.recalcular_superficie(x, y);
    contar_livres();
}

}  // namespace genesis
