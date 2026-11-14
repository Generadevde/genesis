// ---------------------------------------------------------------------------
// mundo.hpp — o mundo da simulação: a ilha e tudo que existe nela.
//
// Como o tempo funciona (simulação por EVENTOS):
//   (Desde a vida vegetal, as plantas — que podem ser milhares — guardam a
//   HORA em que algo vai acontecer, e essas horas ficam numa FILA ordenada
//   (fila_). Assim o motor acha o próximo acontecimento sem olhar planta
//   por planta. Os NPCs, que são poucos, continuam com relógios regressivos.)
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
#include <map>
#include <queue>
#include <random>
#include <string>
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
enum class Ocupante : std::uint8_t {
    Nada = 0, Macieira, Maca, Npc, Corpo,
    Muda,          // maçã que ninguém pegou em 6 h (vira macieira em 3 dias)
    MacieiraSeca   // macieira com mais de 7 dias: cinza, sem maçãs, some em 4 h
};

// O que o NPC está fazendo agora.
enum class EstadoNpc : std::uint8_t {
    Vagando = 0,   // andando aleatoriamente
    IndoComer,     // com fome < 70 e viu uma maçã: indo até ela
    Comendo,       // parado ao lado da maçã, comendo (10 s)
    Morto,         // fome ou sede chegou a 0; o corpo fica no mapa
    IndoBeber,     // com sede < 50 e viu água: indo até ela
    Bebendo,       // parado ao lado da água, bebendo (10 s)
    IndoPegar      // sem fome, viu uma maçã: indo pegar para carregar
};

// Uma macieira. Ela NUNCA sai da lista (o número dela é usado na memória
// dos NPCs e na origem das maçãs); quando some, fica com existe = false.
// (Os "quando_" são horas da simulação, em segundos desde o início.)
struct Macieira {
    Pos pos;
    double nasceu = 0.0;        // quando virou árvore
    double quando_queda = 0.0;  // quando cai a próxima maçã
    double quando_secar = 0.0;  // quando seca (7 dias de vida)
    double quando_sumir = 0.0;  // seca: quando some (4 horas depois)
    bool seca = false;
    bool existe = true;
};

// Uma muda (♣): nasce de uma maçã esquecida no chão.
struct Muda {
    int id = 0;
    Pos pos;
    double nasceu = 0.0;
    double quando_virar = 0.0;  // quando vira macieira (3 dias)
};

struct Npc {
    int id = 0;
    std::string nome;
    Pos pos;
    Pos anterior;              // de onde veio no último passo
    bool tem_anterior = false; // falso até dar o primeiro passo
    double hp = 0.0;           // vida: 0 a 100; morre com 0
    // O que estava no crítico quando morreu (soma): 1 = fome, 2 = sede, 4 = frio.
    int causa_morte = 0;
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

    // --- Carregar (pedido em 14/11/2026) ---------------------------------
    // Vendo uma maçã, mesmo sem fome, ele pega e leva consigo. Carrega no
    // máximo 1. Com fome, come a que está carregando.
    int macas_carregadas = 0;
    int origem_carregada = -1;     // macieira de onde veio a maçã na mão
    bool comendo_da_mao = false;   // está comendo a maçã que carregava

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
    // Relógio do jogo (1 h = 60 s; ver config.hpp). Dia começa em 1.
    struct Relogio { int dia; int hora; int minuto; };
    Relogio relogio() const;
    // Temperatura do mundo agora (°C) e se é dia (06:00–18:00).
    double temperatura() const { return temperatura_em(tempo_); }
    double temperatura_em(double t) const;
    bool e_dia() const;
    // Quanto de HP o NPC está perdendo (negativo) ou ganhando por segundo.
    double taxa_hp(const Npc& n) const;
    // Segundos até a necessidade ficar crítica (urgência; 0 = já está).
    double ate_critico_fome(const Npc& n) const;
    double ate_critico_sede(const Npc& n) const;
    Ocupante ocupante(int x, int y) const;
    const Mapa3D& mapa() const { return mapa_; }
    const std::vector<Macieira>& macieiras() const { return macieiras_; }
    std::vector<Muda> mudas() const;
    // Maçã no chão em (x, y): quantos segundos faltam para virar muda
    // (negativo se não há maçã ali).
    double falta_virar_muda(int x, int y) const;
    // Qual macieira / muda está em (x, y)? (-1 / nullptr se nenhuma)
    int macieira_em(int x, int y) const;
    const Muda* muda_em(int x, int y) const;
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
    void evento_maca_vira_muda(int indice_celula);
    void evento_muda_vira_arvore(int id_muda);
    void agendar(double quando, int tipo, int alvo);
    double hora_do_dia(double t) const;
    double proximo_marco_temperatura() const;
    void ocupar(int indice_celula, Ocupante novo);
    void contar_livres();
    void evento_macieira_seca(Macieira& m);
    void evento_macieira_some(Macieira& m);
    void por_maca_no_chao(int indice_celula, int origem);
    void tirar_maca_do_chao(int indice_celula);
    void nova_macieira(Pos p);

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
    bool tentar_pegar(Npc& n);
    void lembrar_macieira(Npc& n, int arvore);
    bool soltar_maca_perto(Pos onde, int origem);
    bool camada_livres(Pos centro, int r, std::vector<Pos>& livres) const;
    bool tentar_beber(Npc& n);
    void passo_em_direcao(Npc& n, Pos alvo);
    bool ir_a_macieira_lembrada(Npc& n);
    bool ir_a_agua_lembrada(Npc& n);
    void mover(Npc& n, Pos destino);
    void morrer(Npc& n);
    double sortear_queda();

    double tempo_ = 0.0;
    double falta_agua_ = 0.0;  // segundos até o próximo passo da água
    // Próximo cruzamento de 4°/5° ou 06:00 (guardado; recalculado quando passa).
    mutable double marco_temp_ = -1.0;
    std::mt19937_64 rng_;
    Mapa3D mapa_;
    std::vector<Ocupante> ocupacao_;
    // Para cada célula com maçã: de qual macieira ela caiu (-1 = nenhuma).
    // É assim que o NPC sabe qual macieira lembrar depois de comer.
    std::vector<int> origem_maca_;
    std::vector<Macieira> macieiras_;
    std::map<int, Muda> mudas_;  // por id (ordem fixa)
    int proximo_id_muda_ = 0;
    int livres_terra_ = 0;  // células de terra sem nada em cima (ver ocupar)
    // Maçã no chão (célula) -> quando vira muda (6 horas depois de cair).
    std::unordered_map<int, double> quando_muda_;

    // Fila de acontecimentos das plantas, a mais cedo primeiro.
    // Um acontecimento que ficou velho (a maçã foi pegada, a árvore já
    // secou...) é reconhecido na hora de sair da fila e ignorado.
    struct Evento {
        double quando;
        int tipo;   // ver Mundo::agendar
        int alvo;   // número da macieira, célula da maçã ou id da muda
        bool operator>(const Evento& o) const {
            if (quando != o.quando) return quando > o.quando;
            if (tipo != o.tipo) return tipo > o.tipo;
            return alvo > o.alvo;
        }
    };
    std::priority_queue<Evento, std::vector<Evento>, std::greater<Evento>> fila_;
    std::vector<Npc> npcs_;
    // Maçã reservada -> id do NPC que está indo comer/comendo.
    // Evita dois NPCs comendo a mesma maçã.
    std::unordered_map<int, int> reservas_;
};

}  // namespace genesis
