// ---------------------------------------------------------------------------
// mundo.cpp — implementação do mundo (ver explicação geral em mundo.hpp).
// ---------------------------------------------------------------------------
#include "mundo.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>

#include "config.hpp"

namespace genesis {

namespace cfg = config;

namespace {

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
    gerar_terreno();
    ocupacao_.assign(terreno_.size(), Ocupante::Nada);
    origem_maca_.assign(terreno_.size(), -1);

    // 6 macieiras em células de grama livres, sorteadas ("espalhadas").
    for (int i = 0; i < cfg::NUM_MACIEIRAS; ++i) {
        Macieira m;
        m.pos = sortear_grama_livre();
        m.falta_queda = sortear_queda();
        ocupacao_[indice(m.pos)] = Ocupante::Macieira;
        macieiras_.push_back(m);
    }

    // 4 NPCs em células de grama livres, com fome e sede iniciais 100.
    for (int i = 0; i < cfg::NUM_NPCS; ++i) {
        Npc n;
        n.id = i;
        n.pos = sortear_grama_livre();
        n.fome = cfg::FOME_INICIAL;
        n.sede = cfg::SEDE_INICIAL;
        n.falta_passo = cfg::NPC_PASSO_S;
        n.falta_fome = cfg::FOME_INTERVALO_S;
        n.falta_sede = cfg::SEDE_INTERVALO_S;
        ocupacao_[indice(n.pos)] = Ocupante::Npc;
        npcs_.push_back(n);
    }
}

// Ilha quadrada: água nas BORDA_AGUA células de cada borda, grama no meio.
void Mundo::gerar_terreno() {
    terreno_.assign(static_cast<size_t>(largura()) * altura(), Terreno::Agua);
    for (int y = cfg::BORDA_AGUA; y < altura() - cfg::BORDA_AGUA; ++y)
        for (int x = cfg::BORDA_AGUA; x < largura() - cfg::BORDA_AGUA; ++x)
            terreno_[indice({x, y})] = Terreno::Grama;
}

// Sorteia uma célula de grama ainda vazia (só a terra do meio é sorteada).
Pos Mundo::sortear_grama_livre() {
    std::uniform_int_distribution<int> dx(cfg::BORDA_AGUA,
                                          largura() - cfg::BORDA_AGUA - 1);
    std::uniform_int_distribution<int> dy(cfg::BORDA_AGUA,
                                          altura() - cfg::BORDA_AGUA - 1);
    while (true) {
        Pos p{dx(rng_), dy(rng_)};
        if (livre(p)) return p;
    }
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
        for (const Macieira& m : macieiras_)
            proximo = std::min(proximo, m.falta_queda);

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
        for (Macieira& m : macieiras_) m.falta_queda -= passo;

        // 3) Dispara tudo que venceu, em ordem fixa (NPCs, depois macieiras).
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
        for (int i = 0; i < static_cast<int>(macieiras_.size()); ++i) {
            Macieira& m = macieiras_[i];
            if (m.falta_queda <= EPS) {
                m.falta_queda += sortear_queda();
                evento_queda_maca(i);
            }
        }
    }
}

// ===========================================================================
// Acontecimentos
// ===========================================================================

// A cada 5 s: fome abaixa 3. Chegou a 0 -> morre.
// Enquanto está comendo, a fome NÃO abaixa (pedido do dono do projeto).
void Mundo::evento_fome(Npc& n) {
    if (n.estado == EstadoNpc::Comendo) return;
    n.fome -= cfg::FOME_QUEDA;
    if (n.fome <= cfg::FOME_MINIMA) {
        n.fome = cfg::FOME_MINIMA;
        morrer(n);
    }
}

// A cada 5 s: sede abaixa 4. Chegou a 0 -> morre.
// Enquanto está bebendo, a sede NÃO abaixa.
void Mundo::evento_sede(Npc& n) {
    if (n.estado == EstadoNpc::Bebendo) return;
    n.sede -= cfg::SEDE_QUEDA;
    if (n.sede <= cfg::SEDE_MINIMA) {
        n.sede = cfg::SEDE_MINIMA;
        morrer(n);
    }
}

// A cada 1 s: o NPC decide e dá (no máximo) um passo.
//
// Ordem de decisão (definida pelo dono do projeto):
//   - Com fome < 70 e sede < 50 ao mesmo tempo: tenta primeiro a MAIS BAIXA.
//     Se não achar o que ela pede (ex.: nenhuma água à vista), tenta a outra.
//   - Só uma baixa: tenta só ela.
//   - Nada à vista (ou nada baixo): anda aleatoriamente.
// Empate (fome == sede): a sede vem primeiro — PROVISÓRIO, aguardando o dono.
void Mundo::evento_passo(Npc& n) {
    if (n.estado == EstadoNpc::Comendo || n.estado == EstadoNpc::Bebendo)
        return;  // ocupado: fica parado

    const bool com_fome = n.fome < cfg::FOME_LIMITE_COMER;
    const bool com_sede = n.sede < cfg::SEDE_LIMITE_BEBER;
    if (com_fome && com_sede) {
        if (n.sede <= n.fome) {
            if (tentar_beber(n) || tentar_comer(n)) return;
        } else {
            if (tentar_comer(n) || tentar_beber(n)) return;
        }
    } else if (com_fome) {
        if (tentar_comer(n)) return;
    } else if (com_sede) {
        if (tentar_beber(n)) return;
    }

    // --- Nada a resolver, ou nada à vista: anda aleatoriamente ----------
    // Regra: não volta para a célula de onde acabou de sair.
    n.estado = EstadoNpc::Vagando;
    n.tem_alvo = false;
    n.pela_memoria = false;
    std::vector<Pos> opcoes;
    for (Pos v : VIZINHOS) {
        Pos p{n.pos.x + v.x, n.pos.y + v.y};
        if (!livre(p)) continue;
        if (n.tem_anterior && p == n.anterior) continue;
        opcoes.push_back(p);
    }
    if (opcoes.empty()) {
        // Única saída é voltar? Então volta (melhor que ficar preso).
        if (n.tem_anterior && livre(n.anterior)) mover(n, n.anterior);
        return;
    }
    std::uniform_int_distribution<size_t> d(0, opcoes.size() - 1);
    mover(n, opcoes[d(rng_)]);
}

// Procura a maçã mais próxima à vista. Se está ao lado, começa a comer;
// senão, dá um passo na direção dela. Devolve falso se não viu maçã.
bool Mundo::tentar_comer(Npc& n) {
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
    if (distancia_passos(n.pos, agua) == 1) {
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
        if (melhor_d <= 1) {  // chegou e não há maçã à vista: próxima
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
        Pos p{n.pos.x + v.x, n.pos.y + v.y};
        if (!livre(p)) continue;
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

// Terminou os 10 s comendo: a maçã some e a fome sobe 12 (até 100).
void Mundo::evento_fim_comer(Npc& n) {
    const int i = indice(n.alvo);
    // Memória: lembra a macieira de onde veio esta maçã (se ainda não lembra).
    const int arvore = origem_maca_[i];
    if (arvore >= 0 &&
        std::find(n.macieiras_lembradas.begin(), n.macieiras_lembradas.end(),
                  arvore) == n.macieiras_lembradas.end())
        n.macieiras_lembradas.push_back(arvore);
    n.macieiras_vistas_vazias.clear();  // comeu: a busca recomeça do zero
    ocupacao_[i] = Ocupante::Nada;
    origem_maca_[i] = -1;
    reservas_.erase(i);
    n.fome = std::min(cfg::FOME_MAXIMA, n.fome + cfg::MACA_RECUPERA);
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

// Uma maçã cai em volta da macieira: na primeira camada que tiver célula
// de grama livre (camada 1 = as 8 vizinhas; se lotada, camada 2; e assim
// por diante). Dentro da camada, a célula é sorteada.
void Mundo::evento_queda_maca(int indice_macieira) {
    const Macieira& m = macieiras_[indice_macieira];
    const int max_camada = std::max(largura(), altura());
    for (int r = 1; r <= max_camada; ++r) {
        std::vector<Pos> opcoes;
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                Pos p{m.pos.x + dx, m.pos.y + dy};
                if (livre(p)) opcoes.push_back(p);
            }
        if (!opcoes.empty()) {
            std::uniform_int_distribution<size_t> d(0, opcoes.size() - 1);
            const int i = indice(opcoes[d(rng_)]);
            ocupacao_[i] = Ocupante::Maca;
            origem_maca_[i] = indice_macieira;  // de onde ela caiu
            return;
        }
    }
}

// ===========================================================================
// Ajudantes
// ===========================================================================

int Mundo::largura() const { return cfg::LARGURA; }
int Mundo::altura() const { return cfg::ALTURA; }

bool Mundo::dentro(Pos p) const {
    return p.x >= 0 && p.y >= 0 && p.x < largura() && p.y < altura();
}

Terreno Mundo::terreno(int x, int y) const { return terreno_[indice({x, y})]; }
Ocupante Mundo::ocupante(int x, int y) const { return ocupacao_[indice({x, y})]; }

// Livre = dentro do mapa, é grama e não tem nada em cima.
bool Mundo::livre(Pos p) const {
    if (!dentro(p)) return false;
    const int i = indice(p);
    return terreno_[i] == Terreno::Grama && ocupacao_[i] == Ocupante::Nada;
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
            if (!dentro(p) || terreno_[indice(p)] != Terreno::Agua) continue;
            bool da_para_beber = false;
            for (Pos v : VIZINHOS) {
                Pos q{p.x + v.x, p.y + v.y};
                if (q == n.pos || livre(q)) { da_para_beber = true; break; }
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

void Mundo::mover(Npc& n, Pos destino) {
    ocupacao_[indice(n.pos)] = Ocupante::Nada;
    n.anterior = n.pos;
    n.tem_anterior = true;
    n.pos = destino;
    ocupacao_[indice(n.pos)] = Ocupante::Npc;
}

// Morre (de fome ou de sede): o corpo fica no mapa ocupando a célula.
// Se estava comendo, larga a maçã (ela continua lá para outro NPC).
void Mundo::morrer(Npc& n) {
    if (n.estado == EstadoNpc::Comendo) reservas_.erase(indice(n.alvo));
    n.estado = EstadoNpc::Morto;
    n.tem_alvo = false;
    ocupacao_[indice(n.pos)] = Ocupante::Corpo;
}

std::vector<Pos> Mundo::macas() const {
    std::vector<Pos> r;
    for (int y = 0; y < altura(); ++y)
        for (int x = 0; x < largura(); ++x)
            if (ocupacao_[indice({x, y})] == Ocupante::Maca) r.push_back({x, y});
    return r;
}

}  // namespace genesis
