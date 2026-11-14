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
// Mundo em 3D (pedido em 14/11/2026):
//   O chão é um volume de células (x, y, z) — ver mapa3d.hpp e gerador.hpp.
//   NPCs, macieiras, maçãs e corpos ficam na SUPERFÍCIE: o z deles é o z da
//   grama do topo da coluna. Por isso a ocupação continua sendo por (x, y).
//   O NPC só passa para uma coluna vizinha se a diferença de altura for de
//   no máximo 1 nível; subir ou descer leva 5 s em vez de 1 s.
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

#include "mapa3d.hpp"

namespace genesis {

// Uma posição no mundo: coluna x, linha y e altura z.
struct Pos {
    int x = 0;
    int y = 0;
    int z = 0;
    bool operator==(const Pos&) const = default;
};

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
    Ocupante ocupante(int x, int y) const;
    const Mapa3D& mapa() const { return mapa_; }
    const std::vector<Macieira>& macieiras() const { return macieiras_; }
    std::vector<Pos> macas() const;
    // A ocupação inteira de uma vez (1 byte por coluna, linha por linha).
    const std::vector<Ocupante>& ocupacao_bruta() const { return ocupacao_; }
    // O que se vê olhando o nível z de cima, 1 byte por coluna:
    //   bits 0-3: material visto (Material)
    //   bit 4:    1 = está abaixo do nível z (hoje a tela mostra normal)
    //   bit 5:    1 = o ocupante da coluna (NPC, árvore...) aparece
    std::vector<std::uint8_t> visao(int z) const;

    // --- Só para TESTES: mexer no chão à mão (cavar, despejar água) --------
    void teste_definir(int x, int y, int z, Material m, int nivel_agua);
    const std::vector<Npc>& npcs() const { return npcs_; }

private:
    // --- Criação do mundo -------------------------------------------------
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
    bool livre(Pos p) const;   // terra firme e sem ocupante
    // Dá para passar de `de` para `para` (vizinhas)? Livre e degrau <= 1.
    bool pode_pisar(Pos de, Pos para) const;
    Pos com_z(Pos p) const;    // a mesma (x, y) com o z da superfície
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
    double falta_agua_ = 0.0;  // segundos até o próximo passo da água
    std::mt19937_64 rng_;
    Mapa3D mapa_;
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
