// ---------------------------------------------------------------------------
// mundo.hpp — o mundo da simulação: a ilha e tudo que existe nela.
//
// Como o tempo funciona (simulação por EVENTOS):
//   Cada coisa que acontece de tempos em tempos tem um "relógio" que conta
//   quanto falta para o próximo acontecimento:
//     - cada NPC: próximo passo (1 s), próxima queda de fome (5 s),
//       próxima queda de sede (5 s) e, se estiver comendo ou bebendo,
//       quanto falta para terminar (10 s);
//     - cada macieira: quanto falta para a próxima maçã cair (30 a 60 s).
//   avancar(dt) anda o tempo `dt` segundos, disparando os acontecimentos na
//   ordem certa em que vencem. Assim não existe "passo interno" inventado:
//   o tempo anda exatamente o que a interface pedir.
//
// Física (regra "corpos têm física"):
//   Cada célula guarda no máximo UMA coisa: macieira, maçã, NPC ou corpo.
//   Nada entra numa célula ocupada. Água não é andável.
//
// Memória do NPC (pedida em 14/11/2026):
//   O NPC lembra SÓ dos lugares que usou: a macieira de onde veio a maçã que
//   ele comeu, e a célula de água onde bebeu. Lembra todos e nunca esquece.
// ---------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

namespace genesis {

// Uma posição no mapa (coluna x, linha y).
struct Pos {
    int x = 0;
    int y = 0;
    bool operator==(const Pos&) const = default;
};

// Tipo de chão de cada célula.
enum class Terreno : std::uint8_t { Agua = 0, Grama = 1 };

// O que ocupa uma célula (no máximo uma coisa por célula).
enum class Ocupante : std::uint8_t { Nada = 0, Macieira, Maca, Npc, Corpo };

// O que o NPC está fazendo agora.
enum class EstadoNpc : std::uint8_t {
    Vagando = 0,   // andando aleatoriamente
    IndoComer,     // com fome < 70 e viu uma maçã: indo até ela
    Comendo,       // parado ao lado da maçã, comendo (10 s)
    Morto,         // fome ou sede chegou a 0; o corpo fica no mapa
    IndoBeber,     // com sede < 50 e viu água: indo até ela
    Bebendo        // parado ao lado da água, bebendo (10 s)
};

struct Macieira {
    Pos pos;
    double falta_queda = 0.0;  // segundos até a próxima maçã cair
};

struct Npc {
    int id = 0;
    Pos pos;
    Pos anterior;              // de onde veio no último passo
    bool tem_anterior = false; // falso até dar o primeiro passo
    double fome = 0.0;
    double sede = 0.0;
    double falta_passo = 0.0;  // segundos até o próximo passo
    double falta_fome = 0.0;   // segundos até a próxima queda de fome
    double falta_sede = 0.0;   // segundos até a próxima queda de sede
    double falta_acao = 0.0;   // segundos até terminar de comer/beber
    EstadoNpc estado = EstadoNpc::Vagando;
    Pos alvo;                  // maçã, água ou macieira do alvo atual
    bool tem_alvo = false;
    bool pela_memoria = false; // indo a um lugar lembrado (não visto agora)

    // --- Memória ---------------------------------------------------------
    std::vector<int> macieiras_lembradas;  // índices em Mundo::macieiras()
    std::vector<Pos> aguas_lembradas;      // células de água onde já bebeu
    // Macieiras lembradas que ele já foi ver nesta busca e estavam sem
    // maçã. Ele passa para a próxima; ver Mundo::tentar_comer.
    std::vector<int> macieiras_vistas_vazias;
};

class Mundo {
public:
    // `semente` define o sorteio. A mesma semente gera sempre o mesmo mundo
    // (útil para testes). Sem semente, a interface sorteia uma.
    explicit Mundo(std::uint64_t semente);

    // Anda o tempo da simulação em `dt` segundos.
    void avancar(double dt);

    // --- Leitura do estado (a interface só lê, nunca altera) -------------
    int largura() const;
    int altura() const;
    double tempo() const { return tempo_; }
    Terreno terreno(int x, int y) const;
    Ocupante ocupante(int x, int y) const;
    const std::vector<Macieira>& macieiras() const { return macieiras_; }
    std::vector<Pos> macas() const;
    // O mapa inteiro de uma vez (1 byte por célula, linha por linha).
    // A interface usa isso para desenhar rápido, sem perguntar célula a célula.
    const std::vector<Terreno>& terreno_bruto() const { return terreno_; }
    const std::vector<Ocupante>& ocupacao_bruta() const { return ocupacao_; }
    const std::vector<Npc>& npcs() const { return npcs_; }

private:
    // --- Criação do mundo -------------------------------------------------
    void gerar_terreno();
    Pos sortear_grama_livre();

    // --- Acontecimentos ---------------------------------------------------
    void evento_fome(Npc& n);
    void evento_sede(Npc& n);
    void evento_passo(Npc& n);
    void evento_fim_comer(Npc& n);
    void evento_fim_beber(Npc& n);
    void evento_queda_maca(int indice_macieira);

    // --- Ajudantes --------------------------------------------------------
    bool dentro(Pos p) const;
    int indice(Pos p) const { return p.y * largura() + p.x; }
    bool livre(Pos p) const;   // grama e sem ocupante
    bool maca_reservada(Pos p, int por_quem_nao) const;
    bool procurar_maca(const Npc& n, Pos& achada) const;
    bool procurar_agua(const Npc& n, Pos& achada) const;
    bool tentar_comer(Npc& n);
    bool tentar_beber(Npc& n);
    void passo_em_direcao(Npc& n, Pos alvo);
    bool ir_a_macieira_lembrada(Npc& n);
    bool ir_a_agua_lembrada(Npc& n);
    void mover(Npc& n, Pos destino);
    void morrer(Npc& n);
    double sortear_queda();

    double tempo_ = 0.0;
    std::mt19937_64 rng_;
    std::vector<Terreno> terreno_;
    std::vector<Ocupante> ocupacao_;
    // Para cada célula com maçã: de qual macieira ela caiu (-1 = nenhuma).
    // É assim que o NPC sabe qual macieira lembrar depois de comer.
    std::vector<int> origem_maca_;
    std::vector<Macieira> macieiras_;
    std::vector<Npc> npcs_;
    // Maçã reservada -> id do NPC que está indo comer/comendo.
    // Evita dois NPCs comendo a mesma maçã.
    std::unordered_map<int, int> reservas_;
};

}  // namespace genesis
