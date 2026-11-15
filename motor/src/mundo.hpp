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
//     - cada NPC: próximo passo (5 s), próxima queda de fome (15 min),
//       próxima queda de sede (10 min) e, se estiver comendo, bebendo ou
//       trabalhando, quanto falta para terminar;
//     - na fila: quedas de maçã (só no verão), cada maçã no chão (6 h),
//       mudas, macieiras, estações, gelo, família.
//   O tempo é em SEGUNDOS desde o começo (01/01/0001, 07:00). O calendário
//   (dia, mês, ano, estação) é calculado a partir dele — ver Mundo::data_de.
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

#include <array>
#include <cstdint>
#include <map>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.hpp"
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
    Nada = 0, Macieira, Maca /* pilha de 1 a 32 */, Npc, Corpo,
    Muda,          // maçã esquecida 6 h (40%); vira macieira em 3 anos
    MacieiraSeca,  // macieira com 40 anos: cinza, sem maçãs, some em 1 ano
    Pedra,         // ⬟ dá lascas (10 no total), some quando esgota
    Abrigo         // 🏠 cabe 2 NPCs e 300 maçãs; dentro não faz frio, fome nem sede
};

// O que o NPC está fazendo agora.
enum class EstadoNpc : std::uint8_t {
    Vagando = 0,   // andando aleatoriamente
    IndoComer,     // com fome < 70 e viu uma maçã: indo até ela
    Comendo,       // parado, comendo uma maçã (3 min)
    Morto,         // fome ou sede chegou a 0; o corpo fica no mapa
    IndoBeber,     // com sede < 50 e viu água: indo até ela
    Bebendo,       // parado ao lado da água, bebendo um gole (20 s)
    IndoPegar,     // saciado, viu maçãs: indo pegar para levar ao abrigo
    IndoTrabalhar, // indo até a pedra / árvore / lugar da obra
    Trabalhando,   // tirando lascas, tirando galhos, cortando ou construindo
    IndoAbrigo,    // com frio: indo para o abrigo dele
    Dormindo,      // dentro do abrigo, à noite
    IndoProcriar,  // indo para o abrigo do casal, para procriar
    Procriando,    // dentro do abrigo do casal (esperando o parceiro ou junto)
    Bebe,          // bebê: fica parado, sem necessidades (por enquanto)
    IndoGuardar    // levando as maçãs que pegou para o abrigo dele
};

enum class Sexo : std::uint8_t { Homem = 0, Mulher = 1 };

// Histórico do mundo: só os acontecimentos importantes.
enum class TipoRegistro : std::uint8_t {
    Inicio = 0,     // a = homens, b = mulheres
    Nascimento,     // a = filho, b = mãe, c = pai
    Morte,          // a = NPC, b = causa (1 fome, 2 sede, 4 frio, 8 velhice)
    Casal,          // a = homem, b = mulher
    Gravidez,       // a = mãe, b = pai
    VirouAdulto,    // a = NPC
    AbrigoFeito,    // a = NPC, pos
    MachadoFeito,   // a = NPC
    ArvoreCortada,  // a = NPC, pos
    MudaNasceu,     // pos
    ArvoreNasceu,   // pos (muda que virou macieira)
    ArvoreSecou,    // pos
    ArvoreSumiu,    // pos
    PedraEsgotou,   // pos
    BebePerdido,    // a = mãe (morreu grávida)
    Estacao,        // a = estação que começou (0 verão, 1 outono, 2 inverno, 3 primavera)
    Congelou,       // a = quantas colunas de água viraram gelo
    Derreteu        // a = quantas colunas de gelo viraram água
};

// Estações (na ordem do ano começando pelo verão, que começa em dezembro).
enum class Estacao : std::uint8_t { Verao = 0, Outono, Inverno, Primavera };

// Uma data do calendário (mês 1–12, dia 1–31).
struct Data {
    int ano = 1, mes = 1, dia = 1;
    int hora = 0, minuto = 0, segundo = 0;
};

struct Registro {
    double tempo = 0.0;
    TipoRegistro tipo = TipoRegistro::Inicio;
    int a = -1, b = -1, c = -1;
    Pos pos;
};

// O trabalho do NPC (para fazer o abrigo). Se ele para no meio (fome, sede,
// frio), o trabalho fica guardado e ele volta depois.
enum class Tarefa : std::uint8_t { Nenhuma = 0, Lascas, Galhos, Cortar, Construir, Machado };

struct Pedra {
    Pos pos;
    int lascas = 0;     // quantas ainda tem
    bool existe = true;
};

struct Abrigo {
    Pos pos;
    std::vector<int> moradores;  // NPCs que têm este como abrigo (até 2)
    std::vector<int> dentro;     // NPCs dentro agora
    int macas = 0;               // maçãs guardadas (até 300; não estragam)
};

// Uma macieira. Ela NUNCA sai da lista (o número dela é usado na memória
// dos NPCs e na origem das maçãs); quando some, fica com existe = false.
// (Os "quando_" são horas da simulação, em segundos desde o início.)
struct Macieira {
    Pos pos;
    double nasceu = 0.0;        // quando virou árvore (pode ser antes do começo)
    double quando_queda = -1.0; // quando cai a próxima maçã (-1 = nenhuma)
    double quando_secar = 0.0;  // quando seca (40 anos de vida)
    double quando_sumir = 0.0;  // seca: quando some (1 ano depois)
    bool seca = false;
    bool existe = true;
    // Verão: quantas maçãs ela derruba neste verão (80 a 300) e as horas
    // das quedas que ainda faltam (em segundos depois de `quedas_base`,
    // em ordem). Fora do verão fica vazio.
    int macas_verao = 0;
    double quedas_base = 0.0;
    std::vector<float> quedas;
    size_t proxima_queda = 0;
};

// Uma muda (♣): nasce de uma maçã esquecida no chão.
struct Muda {
    int id = 0;
    Pos pos;
    double nasceu = 0.0;
    double quando_virar = 0.0;  // quando vira macieira (3 anos)
};

// Uma maçã no chão. Cada uma tem as suas 6 horas (mesmo numa pilha).
struct MacaChao {
    int celula = 0;       // onde está (as maçãs da mesma célula formam a pilha)
    int origem = -1;      // de qual macieira caiu (-1 = nenhuma / não se sabe)
    double quando = 0.0;  // quando vence (vira muda ou apodrece)
};

struct Npc {
    int id = 0;
    std::string nome;
    Sexo sexo = Sexo::Homem;
    // --- Idade e família ---------------------------------------------------
    bool bebe = false;
    // Quando nasceu (segundos da simulação; os do começo nasceram 20 anos
    // antes dele, então é negativo). A idade conta pelo aniversário.
    double nascimento = 0.0;
    int mae = -1, pai = -1;
    int filhos = 0;                // quantos filhos (para o nome: F., 2F., ...)
    int parceiro = -1;             // o casal (-1 = sem par)
    bool gravida = false;
    int gravidez_pai = -1;
    double quando_nascer = 0.0;
    double pode_engravidar_em = 0.0;   // 2 meses depois do último parto
    double procriando_desde = -1.0;    // casal junto no abrigo desde (-1 = não)
    double morreu_em = -1.0;           // quando morreu (a idade para aí)
    double quando_velhice = -1.0;      // quando chega aos 100 anos
    double quando_adulto = -1.0;       // bebê: quando faz 15 anos
    Pos pos;
    Pos anterior;              // de onde veio no último passo
    bool tem_anterior = false; // falso até dar o primeiro passo
    double hp = 0.0;           // vida: 0 a 100; morre com 0
    // O que estava no crítico quando morreu (soma): 1 = fome, 2 = sede,
    // 4 = frio, 8 = velhice.
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
    // Explorar: destino escolhido (2 a 20 células) e se ainda está valendo.
    Pos destino;
    bool tem_destino = false;

    // --- Carregar (pedido em 14 e 15/11/2026) -----------------------------
    // Saciado e vendo maçãs, pega as da pilha (até 16 na mão) e leva para o
    // abrigo dele. Com fome, come primeiro as que carrega, depois as de um
    // abrigo, depois as do chão.
    int macas_carregadas = 0;
    // Comendo: a maçã já saiu de onde estava (mão, abrigo ou chão) e está
    // "na boca" até terminar os 3 minutos.
    bool comendo = false;
    int comendo_de = 0;            // 0 = da mão, 1 = de um abrigo, 2 = do chão
    // Mais coisas que ele carrega (sem limite, por enquanto).
    int lascas = 0;
    int galhos = 0;
    int lenha = 0;
    bool machado = false;

    // --- Trabalho e abrigo ------------------------------------------------
    Tarefa tarefa = Tarefa::Nenhuma;
    int tarefa_alvo = -1;          // número da pedra / macieira
    Pos tarefa_pos;                // onde fica o alvo (ou a obra)
    double tarefa_restante = 0.0;  // segundos que faltam (se parou no meio)
    int abrigo_casa = -1;          // abrigo do qual ele é morador
    int dentro_abrigo = -1;        // abrigo onde está agora (-1 = fora)
    std::vector<int> pedras_lembradas;  // pedras onde já tirou lascas

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
    Mundo(std::uint64_t semente, int homens, int mulheres);

    // Anda o tempo da simulação em `dt` segundos.
    void avancar(double dt);

    // --- Leitura do estado (a interface só lê, nunca altera) -------------
    int largura() const { return config::LARGURA; }
    int altura() const { return config::ALTURA; }
    double tempo() const { return tempo_; }
    // Calendário (ver config.hpp): a data agora e a de um instante t.
    Data data() const { return data_de(tempo_); }
    static Data data_de(double t);
    // Segundos da simulação da data (00:00 do dia) — pode ser negativo.
    static double tempo_de(int ano, int mes, int dia);
    // t mais `meses` meses do calendário (mesmo dia; se o mês é mais curto,
    // o último dia dele) e mesma hora.
    static double somar_meses(double t, int meses);
    static Estacao estacao_de(double t);
    Estacao estacao() const { return estacao_de(tempo_); }
    // Começo e fim (segundos) da estação que contém t.
    static void limites_estacao(double t, double& inicio, double& fim);
    // Quantas horas de sol tem o dia de t, e as horas do nascer / pôr.
    static double horas_de_sol(double t);
    static double nascer_do_sol(double t) { return 12.0 - horas_de_sol(t) / 2.0; }
    static double por_do_sol(double t) { return 12.0 + horas_de_sol(t) / 2.0; }
    // Mínima e máxima do dia (média do mundo), mudando aos poucos no ano.
    static void min_max_temperatura(double t, double& minima, double& maxima);
    // Temperatura do mundo agora (°C).
    double temperatura() const { return temperatura_em(tempo_); }
    double temperatura_em(double t) const;
    // O sol está no céu?
    bool sol() const;
    // "Dia" para quem está no abrigo (06:00–18:00; regra antiga, mantida).
    bool e_dia() const;
    // Quanto de HP o NPC está perdendo (negativo) ou ganhando por segundo.
    double taxa_hp(const Npc& n) const;
    // Segundos até a necessidade ficar crítica (urgência; 0 = já está).
    double ate_critico_fome(const Npc& n) const;
    double ate_critico_sede(const Npc& n) const;
    Ocupante ocupante(int x, int y) const;
    const Mapa3D& mapa() const { return mapa_; }
    const std::vector<Macieira>& macieiras() const { return macieiras_; }
    const std::vector<Pedra>& pedras() const { return pedras_; }
    const std::vector<Registro>& historico() const { return historico_; }
    double idade_anos(const Npc& n) const;   // bebê: fração de ano
    const std::vector<Abrigo>& abrigos() const { return abrigos_; }
    int pedra_em(int x, int y) const;
    int abrigo_em(int x, int y) const;
    std::vector<Muda> mudas() const;
    // Maçã no chão em (x, y): segundos até a próxima vencer (−1 se não há).
    double falta_virar_muda(int x, int y) const;
    // Qual macieira / muda está em (x, y)? (-1 / nullptr se nenhuma)
    int macieira_em(int x, int y) const;
    const Muda* muda_em(int x, int y) const;
    std::vector<Pos> macas() const;
    // Quantas maçãs há na célula (x, y), e as pilhas: (x, y, quantas).
    int macas_em(int x, int y) const;
    std::vector<std::array<int, 3>> pilhas() const;
    // Segundos até cada maçã da célula vencer (6 h), em ordem.
    std::vector<double> macas_vencem(int x, int y) const;
    // Quantas maçãs a macieira ainda vai derrubar neste verão.
    int macas_por_cair(int indice_macieira) const;
    // A ocupação inteira de uma vez (1 byte por coluna, linha por linha).
    const std::vector<Ocupante>& ocupacao_bruta() const { return ocupacao_; }
    // O que se vê olhando o nível z de cima, 1 byte por coluna:
    //   bits 0-3: material visto (Material)
    //   bit 4:    1 = está abaixo do nível z (hoje a tela mostra normal)
    //   bit 5:    1 = o ocupante da coluna (NPC, árvore...) aparece
    std::vector<std::uint8_t> visao(int z) const;

    // --- Só para TESTES: mexer no chão à mão (cavar, despejar água) --------
    void teste_definir(int x, int y, int z, Material m, int nivel_agua);
    void teste_npc(int id, double fome, double sede, double hp);
    void teste_dar(int id, int lascas, int galhos, int lenha, bool machado);
    bool teste_mover(int id, int x, int y);
    void teste_idade(int id, double anos);
    void teste_macas(int id, int quantas);       // maçãs na mão
    // Abrigo pronto em (x, y) (célula livre), com o NPC morando nele.
    int teste_abrigo(int id, int x, int y, int macas);
    void teste_idade_macieira(int indice, double anos);
    void teste_adiantar_parto(int mae, double segundos);
    // Pula o relógio até t (só para testes): as maçãs do chão somem, as
    // quedas e as estações são refeitas para a data nova; o resto (idades,
    // gravidez, plantas) fica nas mesmas horas marcadas — o que já passou
    // acontece logo no próximo avancar.
    void teste_saltar(double t);
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
    void evento_maca_vence(int id_maca);
    void evento_muda_vira_arvore(int id_muda);
    void agendar(double quando, int tipo, int alvo);
    double hora_do_dia(double t) const;
    double proximo_marco_temperatura() const;
    void ocupar(int indice_celula, Ocupante novo);
    void contar_livres();
    void evento_macieira_seca(Macieira& m);
    void evento_macieira_some(Macieira& m);
    // Põe uma maçã na pilha da célula (nova: vence daqui a 6 h).
    void por_maca_no_chao(int indice_celula, int origem);
    void por_maca_existente(int id, int indice_celula);
    // Tira uma maçã da pilha (a de cima). Devolve a origem dela.
    int tirar_maca_do_chao(int indice_celula);
    void nova_macieira(Pos p, double idade_anos);
    // Estações, calendário e gelo
    void evento_estacao(int estacao);
    void comecar_verao(double inicio, double fim);
    void gerar_quedas(int indice_macieira, double inicio, double fim);
    void congelar_vales();
    void degelar_vales();
    // Lugar para uma maçã em volta de `centro`: célula de terra vazia ou
    // pilha com menos de 32. Devolve -1 se não há.
    int lugar_para_maca(Pos centro);

    // --- Ajudantes --------------------------------------------------------
    bool dentro(Pos p) const {
        return p.x >= 0 && p.y >= 0 && p.x < largura() && p.y < altura();
    }
    int indice(Pos p) const { return p.y * largura() + p.x; }
    bool livre(Pos p) const;   // terra firme (não gelo) e sem ocupante
    bool pisavel(Pos p) const; // terra ou gelo, sem ocupante (para andar)
    bool pode_ter_filho(const Npc& n) const;  // idade de procriar
    // Dá para passar de `de` para `para` (vizinhas)? Livre e degrau <= 1.
    bool pode_pisar(Pos de, Pos para) const;
    // A mesma (x, y) com o z da superfície.
    Pos com_z(Pos p) const {
        if (dentro(p)) p.z = mapa_.superficie(p.x, p.y);
        return p;
    }
    bool procurar_maca(const Npc& n, Pos& achada) const;
    bool procurar_abrigo_com_macas(const Npc& n, int& abrigo) const;
    bool tentar_guardar(Npc& n);
    void comecar_a_comer(Npc& n, int de);
    bool procurar_agua(const Npc& n, Pos& achada) const;
    bool tentar_comer(Npc& n);
    bool tentar_pegar(Npc& n);
    // Abrigo e trabalho
    bool cadeia_abrigo(Npc& n);
    // Família
    void formar_casal(Npc& n);
    void unir_abrigo_casal(Npc& a, Npc& b);
    bool tentar_procriar(Npc& n);
    bool quer_procriar(const Npc& n) const;
    void ver_se_comeca_procriar(int abrigo);
    void evento_concepcao(int mae);
    void evento_nascimento(int mae);
    void evento_adulto(int id);
    void evento_velhice(int id);
    void registrar(TipoRegistro t, int a = -1, int b = -1, int c = -1, Pos pos = {});
    bool lugar_livre_perto(Pos centro, Pos& achado);
    void sair_de_morador(Npc& n);
    bool trabalhar(Npc& n);
    bool escolher_tarefa(Npc& n);
    void evento_fim_trabalho(Npc& n);
    void pausar_trabalho(Npc& n);
    double sortear(double minimo, double maximo);
    bool procurar_recurso(const Npc& n, bool pedra, int& indice, Pos& onde) const;
    void conhecer_abrigos(Npc& n);
    bool ir_para_abrigo(Npc& n);
    void entrar_abrigo(Npc& n, int a);
    void sair_abrigo(Npc& n);
    double ate_critico_frio() const;
    double duracao_tarefa(Tarefa t);
    void explorar(Npc& n);
    bool sortear_destino(Npc& n);
    void lembrar_macieira(Npc& n, int arvore);
    bool soltar_maca_perto(Pos onde, int origem);
    void tirar_de_cima_do_gelo(const std::vector<int>& colunas);
    bool camada_livres(Pos centro, int r, std::vector<Pos>& livres) const;
    bool tentar_beber(Npc& n);
    // Como chegar: ao lado do alvo, na própria célula, ou ao lado de uma
    // água de onde dá para beber (no nível dela ou 1 acima).
    enum class Chegar { AoLado, NaCelula, ParaBeber };
    // Dá 1 passo pelo CAMINHO até o alvo (contornando barrancos, água e
    // ocupados). Devolve falso se não há caminho; aí dá o passo "em linha
    // reta" de antes (para não ficar parado).
    bool passo_em_direcao(Npc& n, Pos alvo, Chegar como = Chegar::AoLado);
    bool chegou(Pos q, Pos alvo, Chegar como) const;
    // Distância a pé (em passos) de `de` até cada coluna; -1 = não chega.
    std::vector<int> distancias(Pos de) const;
    bool ir_a_macieira_lembrada(Npc& n);
    bool ir_a_agua_lembrada(Npc& n);
    void mover(Npc& n, Pos destino);
    void morrer(Npc& n, int causa_extra = 0);

    double tempo_ = 0.0;
    double falta_agua_ = 0.0;  // segundos até o próximo passo da água
    // Próximo cruzamento de 4°/5° ou 06:00 (guardado; recalculado quando passa).
    mutable double marco_temp_ = -1.0;
    // Guardado de ate_critico_frio: quando fica crítico e até quando vale.
    mutable double critico_em_ = 0.0;
    mutable double critico_valido_ate_ = -1.0;
    std::mt19937_64 rng_;
    Mapa3D mapa_;
    std::vector<Ocupante> ocupacao_;
    // Maçãs no chão: cada uma por número; e, por célula, os números das
    // maçãs da pilha (até 32).
    std::unordered_map<int, MacaChao> macas_chao_;
    std::vector<std::vector<int>> pilha_;
    int proximo_id_maca_ = 0;
    std::vector<Macieira> macieiras_;
    std::map<int, Muda> mudas_;  // por id (ordem fixa)
    std::vector<Pedra> pedras_;   // nunca sai da lista (existe = false)
    std::vector<Abrigo> abrigos_;
    std::vector<Registro> historico_;
    int proximo_id_muda_ = 0;
    int livres_terra_ = 0;  // células de terra sem nada em cima (ver ocupar)

    // Fila de acontecimentos das plantas, a mais cedo primeiro.
    // Um acontecimento que ficou velho (a maçã foi pegada, a árvore já
    // secou...) é reconhecido na hora de sair da fila e ignorado.
    struct Evento {
        double quando;
        int tipo;   // ver Mundo::agendar
        int alvo;   // número da macieira, da maçã, da muda, do NPC...
        bool operator>(const Evento& o) const {
            if (quando != o.quando) return quando > o.quando;
            if (tipo != o.tipo) return tipo > o.tipo;
            return alvo > o.alvo;
        }
    };
    std::priority_queue<Evento, std::vector<Evento>, std::greater<Evento>> fila_;
    std::vector<Npc> npcs_;
};

}  // namespace genesis
