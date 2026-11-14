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

    // 6 macieiras em células de grama livres, sorteadas ("espalhadas").
    for (int i = 0; i < cfg::NUM_MACIEIRAS; ++i) {
        Macieira m;
        m.pos = sortear_grama_livre();
        m.falta_queda = sortear_queda();
        ocupacao_[indice(m.pos)] = Ocupante::Macieira;
        macieiras_.push_back(m);
    }

    // 4 NPCs em células de grama livres, com fome inicial 100.
    for (int i = 0; i < cfg::NUM_NPCS; ++i) {
        Npc n;
        n.id = i;
        n.pos = sortear_grama_livre();
        n.fome = cfg::FOME_INICIAL;
        n.falta_passo = cfg::NPC_PASSO_S;
        n.falta_fome = cfg::FOME_INTERVALO_S;
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
            proximo = std::min({proximo, n.falta_fome, n.falta_passo});
            if (n.estado == EstadoNpc::Comendo)
                proximo = std::min(proximo, n.falta_comer);
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
            n.falta_passo -= passo;
            if (n.estado == EstadoNpc::Comendo) n.falta_comer -= passo;
        }
        for (Macieira& m : macieiras_) m.falta_queda -= passo;

        // 3) Dispara tudo que venceu, em ordem fixa (NPCs, depois macieiras).
        for (Npc& n : npcs_) {
            if (n.estado != EstadoNpc::Morto && n.falta_fome <= EPS) {
                n.falta_fome += cfg::FOME_INTERVALO_S;
                evento_fome(n);
            }
            if (n.estado == EstadoNpc::Comendo && n.falta_comer <= EPS)
                evento_fim_comer(n);
            if (n.estado != EstadoNpc::Morto && n.falta_passo <= EPS) {
                n.falta_passo += cfg::NPC_PASSO_S;
                evento_passo(n);
            }
        }
        for (Macieira& m : macieiras_) {
            if (m.falta_queda <= EPS) {
                m.falta_queda += sortear_queda();
                evento_queda_maca(m);
            }
        }
    }
}

// ===========================================================================
// Acontecimentos
// ===========================================================================

// A cada 5 s: fome abaixa 3. Chegou a 0 -> morre.
void Mundo::evento_fome(Npc& n) {
    n.fome -= cfg::FOME_QUEDA;
    if (n.fome <= cfg::FOME_MINIMA) {
        n.fome = cfg::FOME_MINIMA;
        morrer(n);
    }
}

// A cada 1 s: o NPC decide e dá (no máximo) um passo.
void Mundo::evento_passo(Npc& n) {
    if (n.estado == EstadoNpc::Comendo) return;  // comendo fica parado

    // --- Com fome (< 70): procura a maçã mais próxima que enxerga --------
    Pos maca;
    if (n.fome < cfg::FOME_LIMITE_COMER && procurar_maca(n, maca)) {
        n.estado = EstadoNpc::IndoComer;
        n.alvo = maca;
        n.tem_alvo = true;

        // Já está ao lado da maçã? Começa a comer (a maçã ocupa a célula
        // dela, então o NPC come de uma célula vizinha).
        if (distancia_passos(n.pos, maca) == 1) {
            reservas_[indice(maca)] = n.id;
            n.estado = EstadoNpc::Comendo;
            n.falta_comer = cfg::COMER_DURACAO_S;
            return;
        }

        // Senão, dá um passo para a vizinha livre que mais aproxima da maçã.
        bool achou = false;
        Pos melhor{};
        int melhor_passos = 0, melhor_d2 = 0;
        for (Pos v : VIZINHOS) {
            Pos p{n.pos.x + v.x, n.pos.y + v.y};
            if (!livre(p)) continue;
            const int ps = distancia_passos(p, maca), d2 = distancia2(p, maca);
            if (!achou || ps < melhor_passos ||
                (ps == melhor_passos && d2 < melhor_d2)) {
                achou = true;
                melhor = p;
                melhor_passos = ps;
                melhor_d2 = d2;
            }
        }
        if (achou) mover(n, melhor);
        return;  // sem vizinha livre: espera o próximo passo
    }

    // --- Sem fome, ou nenhuma maçã à vista: anda aleatoriamente ----------
    // Regra: não volta para a célula de onde acabou de sair.
    n.estado = EstadoNpc::Vagando;
    n.tem_alvo = false;
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

// Terminou os 10 s comendo: a maçã some e a fome sobe 12 (até 100).
void Mundo::evento_fim_comer(Npc& n) {
    const int i = indice(n.alvo);
    ocupacao_[i] = Ocupante::Nada;
    reservas_.erase(i);
    n.fome = std::min(cfg::FOME_MAXIMA, n.fome + cfg::MACA_RECUPERA);
    n.estado = EstadoNpc::Vagando;
    n.tem_alvo = false;
}

// Uma maçã cai em volta da macieira: na primeira camada que tiver célula
// de grama livre (camada 1 = as 8 vizinhas; se lotada, camada 2; e assim
// por diante). Dentro da camada, a célula é sorteada.
void Mundo::evento_queda_maca(Macieira& m) {
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
            ocupacao_[indice(opcoes[d(rng_)])] = Ocupante::Maca;
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

void Mundo::mover(Npc& n, Pos destino) {
    ocupacao_[indice(n.pos)] = Ocupante::Nada;
    n.anterior = n.pos;
    n.tem_anterior = true;
    n.pos = destino;
    ocupacao_[indice(n.pos)] = Ocupante::Npc;
}

// Morre: o corpo fica no mapa ocupando a célula. Se estava comendo, larga
// a maçã (ela continua lá para outro NPC).
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
