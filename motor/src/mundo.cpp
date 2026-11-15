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

// Tipos de acontecimento (na fila).
enum TipoEvento : int {
    QUEDA_MACA = 0,      // alvo = número da macieira
    MACIEIRA_SECA = 1,   // alvo = número da macieira
    MACIEIRA_SOME = 2,   // alvo = número da macieira
    MACA_VENCE = 3,      // alvo = número da maçã no chão (6 h)
    MUDA_VIRA_ARVORE = 4, // alvo = id da muda
    VIRA_ADULTO = 5,      // alvo = id do bebê
    VELHICE = 6,          // alvo = id do NPC
    NASCIMENTO = 7,       // alvo = id da mãe
    CONCEPCAO = 8,        // alvo = id da mãe (1 h juntos no abrigo)
    ESTACAO = 9           // alvo = estação que começa (0 verão ... 3 primavera)
};

// Dia do ano (0 = 1º de janeiro) em que começa cada mês.
int dia_do_ano(int mes, int dia) {
    int d = 0;
    for (int k = 0; k < mes - 1; ++k) d += cfg::DIAS_DO_MES[k];
    return d + dia - 1;
}

// Mês (1–12) em que começa cada estação, na ordem do enum Estacao.
constexpr int MES_DA_ESTACAO[4] = {cfg::VERAO_MES, cfg::OUTONO_MES,
                                   cfg::INVERNO_MES, cfg::PRIMAVERA_MES};

// Curva suave de 0 a 1 e de volta a 0 (pico no meio): sen²(π·f).
// É a "curva gradual" do dono do projeto (forma escolhida: PROVISÓRIO).
double curva_pico(double f) {
    const double s = std::sin(M_PI * f);
    return s * s;
}

// Passagem suave de 0 a 1 (f de 0 a 1): (1 − cos(π·f)) / 2.
double curva_passagem(double f) { return (1.0 - std::cos(M_PI * f)) / 2.0; }

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
    pilha_.assign(colunas, {});
    contar_livres();
    falta_agua_ = cfg::AGUA_INTERVALO_S;

    // 6 macieiras em células de terra livres, sorteadas ("espalhadas").
    // Começam com 2 anos (decisão do dono do projeto).
    for (int i = 0; i < cfg::NUM_MACIEIRAS; ++i)
        nova_macieira(sortear_grama_livre(), cfg::MACIEIRA_IDADE_INICIAL_ANOS);

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
        // Nasceu exatamente 20 anos antes do começo (mesma data e hora).
        n.nascimento = -cfg::IDADE_INICIAL_ANOS * cfg::SEGUNDOS_POR_ANO;
        n.hp = cfg::HP_INICIAL;
        n.fome = cfg::FOME_INICIAL;
        n.sede = cfg::SEDE_INICIAL;
        n.falta_passo = cfg::NPC_PASSO_S;
        n.falta_fome = cfg::FOME_INTERVALO_S;
        n.falta_sede = cfg::SEDE_INTERVALO_S;
        n.pos = sortear_grama_livre();
        ocupar(indice(n.pos), Ocupante::Npc);
        // Morre de velhice aos 100 anos.
        n.quando_velhice = n.nascimento + cfg::IDADE_MAXIMA_ANOS * cfg::SEGUNDOS_POR_ANO;
        agendar(n.quando_velhice, VELHICE, n.id);
        npcs_.push_back(n);
    }
    registrar(TipoRegistro::Inicio, homens, mulheres);

    // Estações: o que vale na estação em que o mundo começa, e o começo de
    // cada uma das próximas (cada uma se reagenda para o ano seguinte).
    double ini = 0.0, fim = 0.0;
    limites_estacao(tempo_, ini, fim);
    const Estacao agora = estacao_de(tempo_);
    if (agora == Estacao::Verao) comecar_verao(ini, fim);
    if (agora == Estacao::Inverno) congelar_vales();
    for (int e = 0; e < 4; ++e) {
        const Data d = data_de(tempo_);
        double quando = tempo_de(d.ano, MES_DA_ESTACAO[e], 1);
        if (quando <= tempo_) quando = tempo_de(d.ano + 1, MES_DA_ESTACAO[e], 1);
        agendar(quando, ESTACAO, e);
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

// Uma macieira nova: as do começo têm 2 anos; as que crescem de uma muda,
// 0. Vive 40 anos (contados de quando virou árvore) e então seca.
// Uma macieira que nasce no meio do verão só dá maçãs no verão seguinte
// (PROVISÓRIO).
void Mundo::nova_macieira(Pos p, double idade_anos) {
    Macieira m;
    m.pos = com_z(p);
    m.nasceu = tempo_ - idade_anos * cfg::SEGUNDOS_POR_ANO;
    m.quando_secar = m.nasceu + cfg::MACIEIRA_VIDA_ANOS * cfg::SEGUNDOS_POR_ANO;
    ocupar(indice(m.pos), Ocupante::Macieira);
    const int i = static_cast<int>(macieiras_.size());
    macieiras_.push_back(m);
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

// Número sorteado entre `minimo` e `maximo` (durações dos trabalhos).
double Mundo::sortear(double minimo, double maximo) {
    std::uniform_real_distribution<double> d(minimo, maximo);
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
        // A cada 5 s: um passo da física da água.
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
                    // Próxima queda deste verão (se ainda houver).
                    m.proxima_queda += 1;
                    if (m.proxima_queda < m.quedas.size()) {
                        m.quando_queda = m.quedas_base + m.quedas[m.proxima_queda];
                        agendar(m.quando_queda, QUEDA_MACA, e.alvo);
                    } else {
                        m.quando_queda = -1.0;
                        m.quedas.clear();
                        m.proxima_queda = 0;
                    }
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
                case MACA_VENCE: {
                    auto it = macas_chao_.find(e.alvo);
                    if (it == macas_chao_.end() || it->second.quando != e.quando) break;
                    evento_maca_vence(e.alvo);
                    break;
                }
                case MUDA_VIRA_ARVORE:
                    if (mudas_.count(e.alvo)) evento_muda_vira_arvore(e.alvo);
                    break;
                case VIRA_ADULTO:
                    if (e.quando == npcs_[e.alvo].quando_adulto) evento_adulto(e.alvo);
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
                case ESTACAO:
                    evento_estacao(e.alvo);
                    break;
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

// A cada 15 min: fome abaixa 3, até 0.
// Enquanto está comendo, a fome NÃO abaixa (pedido do dono do projeto).
void Mundo::evento_fome(Npc& n) {
    // Comendo, ou dentro do abrigo ("dentro não faz fome"): não abaixa.
    if (n.estado == EstadoNpc::Comendo || n.dentro_abrigo >= 0) return;
    // Chegou a 0: fica em 0 (crítico — o NPC perde HP; ver taxa_hp).
    n.fome = std::max(cfg::FOME_MINIMA, n.fome - cfg::FOME_QUEDA);
}

// A cada 10 min: sede abaixa 4, até 0.
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

// A cada 5 s: o NPC decide e dá (no máximo) um passo.
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

    // --- Saciado com maçãs na mão: leva para o abrigo dele.
    if (tentar_guardar(n)) return;

    // --- Saciado: se vê maçãs e tem lugar na mão, vai pegar (pedido do
    //     dono do projeto) — para levar ao abrigo.
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
        if (dentro(p) && (mapa_.e_terra(x, y) || mapa_.e_gelo(x, y)))
            opcoes.push_back(com_z(p));
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

// A maçã sai de onde estava (mão, abrigo ou chão) e ele come por 3 min.
void Mundo::comecar_a_comer(Npc& n, int de) {
    n.estado = EstadoNpc::Comendo;
    n.comendo = true;
    n.comendo_de = de;
    n.tem_alvo = false;
    n.pela_memoria = false;
    n.falta_acao = cfg::COMER_DURACAO_S;
}

// Com fome (decisão do dono do projeto): come primeiro uma das maçãs que
// carrega; se não tem, a de um abrigo (qualquer um pode comer de qualquer
// abrigo); senão, uma do chão; senão, vai a uma macieira lembrada.
// Devolve falso se não achou nada para comer.
bool Mundo::tentar_comer(Npc& n) {
    if (n.macas_carregadas > 0) {
        n.macas_carregadas -= 1;
        comecar_a_comer(n, 0);
        return true;
    }
    int ab = -1;
    if (procurar_abrigo_com_macas(n, ab)) {
        const Abrigo& a = abrigos_[ab];
        const int desnivel = std::abs(a.pos.z - n.pos.z);
        if (distancia_passos(n.pos, a.pos) == 1 && desnivel <= cfg::NPC_DEGRAU_MAX) {
            abrigos_[ab].macas -= 1;
            comecar_a_comer(n, 1);
            return true;
        }
        n.estado = EstadoNpc::IndoComer;
        n.alvo = a.pos;
        n.tem_alvo = true;
        n.pela_memoria = false;
        if (passo_em_direcao(n, a.pos)) return true;
        // Sem caminho até o abrigo: tenta o chão.
    }
    Pos maca;
    // Primeiro o que ele VÊ agora; se não vê maçã, usa a memória.
    if (!procurar_maca(n, maca)) return ir_a_macieira_lembrada(n);
    n.estado = EstadoNpc::IndoComer;
    n.alvo = maca;
    n.tem_alvo = true;
    n.pela_memoria = false;
    // A pilha ocupa a célula dela, então o NPC come de uma célula vizinha.
    if (distancia_passos(n.pos, maca) == 1) {
        const int origem = tirar_maca_do_chao(indice(maca));
        lembrar_macieira(n, origem);
        n.macieiras_vistas_vazias.clear();
        comecar_a_comer(n, 2);
        return true;
    }
    passo_em_direcao(n, maca);
    return true;
}

// O abrigo (com maçãs guardadas) para ir comer: o dele, se tem maçãs; senão
// o mais perto que ele vê (32 x 32) — PROVISÓRIO.
bool Mundo::procurar_abrigo_com_macas(const Npc& n, int& abrigo) const {
    if (n.abrigo_casa >= 0 && abrigos_[n.abrigo_casa].macas > 0) {
        abrigo = n.abrigo_casa;
        return true;
    }
    const int metade = cfg::VISAO_LADO / 2;
    int melhor_d = -1;
    for (int a = 0; a < static_cast<int>(abrigos_.size()); ++a) {
        const Abrigo& ab = abrigos_[a];
        if (ab.macas <= 0) continue;
        const int dx = ab.pos.x - n.pos.x, dy = ab.pos.y - n.pos.y;
        if (dx < -metade || dx >= metade || dy < -metade || dy >= metade) continue;
        const int d = distancia_passos(n.pos, ab.pos);
        if (melhor_d < 0 || d < melhor_d) { melhor_d = d; abrigo = a; }
    }
    return melhor_d >= 0;
}

// Saciado e vendo maçãs (pedido do dono do projeto): vai até a pilha e, ao
// lado dela, pega na hora todas que couberem na mão (até 16) — mesmo que
// sejam menos. Depois leva para o abrigo (ver tentar_guardar). Se o abrigo
// dele está cheio (300), não pega. Sem abrigo, só carrega.
// Ao pegar, lembra a macieira de onde cada maçã caiu.
bool Mundo::tentar_pegar(Npc& n) {
    const int cabe = cfg::MACAS_CARREGAVEIS - n.macas_carregadas;
    if (cabe <= 0) return false;
    if (n.abrigo_casa >= 0 && abrigos_[n.abrigo_casa].macas >= cfg::ABRIGO_MACAS_MAX)
        return false;
    Pos maca;
    if (!procurar_maca(n, maca)) return false;
    if (distancia_passos(n.pos, maca) == 1) {
        const int i = indice(maca);
        for (int k = 0; k < cabe && !pilha_[i].empty(); ++k) {
            lembrar_macieira(n, tirar_maca_do_chao(i));
            n.macas_carregadas += 1;
        }
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

// Com maçãs na mão e um abrigo: leva para lá. Ao lado do abrigo, guarda na
// hora as que couberem (até 300); as que não couberem ele deixa no chão
// onde está (em volta dele).
bool Mundo::tentar_guardar(Npc& n) {
    if (n.macas_carregadas <= 0 || n.abrigo_casa < 0) return false;
    Abrigo& ab = abrigos_[n.abrigo_casa];
    const int desnivel = std::abs(ab.pos.z - n.pos.z);
    if (distancia_passos(n.pos, ab.pos) == 1 && desnivel <= cfg::NPC_DEGRAU_MAX) {
        const int guarda = std::min(n.macas_carregadas, cfg::ABRIGO_MACAS_MAX - ab.macas);
        ab.macas += guarda;
        n.macas_carregadas -= guarda;
        while (n.macas_carregadas > 0 && soltar_maca_perto(n.pos, -1))
            n.macas_carregadas -= 1;
        n.macas_carregadas = 0;  // se nem no chão couber, perdem-se
        n.estado = EstadoNpc::Vagando;
        n.tem_alvo = false;
        return true;
    }
    n.estado = EstadoNpc::IndoGuardar;
    n.tem_alvo = true;
    n.alvo = ab.pos;
    if (!passo_em_direcao(n, ab.pos)) return false;  // sem caminho: segue a vida
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

// Terminou os 3 min comendo: a fome sobe 12 e a sede sobe 5 (até 100).
void Mundo::evento_fim_comer(Npc& n) {
    n.comendo = false;
    n.fome = std::min(cfg::FOME_MAXIMA, n.fome + cfg::MACA_RECUPERA);
    n.sede = std::min(cfg::SEDE_MAXIMA, n.sede + cfg::MACA_RECUPERA_SEDE);
    n.estado = EstadoNpc::Vagando;
    n.tem_alvo = false;
}

// Terminou os 20 s bebendo: a sede sobe 15 (até 100). A água não acaba.
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

// Toda maçã NOVA no chão passa por aqui (caiu da árvore, ou alguém soltou):
// entra na pilha da célula e começa a contar as suas 6 horas.
// (Maçã pegada e solta de novo recomeça a contagem.)
void Mundo::por_maca_no_chao(int i, int origem) {
    MacaChao m;
    m.celula = i;
    m.origem = origem;
    m.quando = tempo_ + cfg::MACA_VIRA_MUDA_H * cfg::SEGUNDOS_POR_HORA;
    const int id = proximo_id_maca_++;
    macas_chao_[id] = m;
    pilha_[i].push_back(id);
    if (ocupacao_[i] != Ocupante::Maca) ocupar(i, Ocupante::Maca);
    agendar(m.quando, MACA_VENCE, id);
}

// Uma maçã que já existe muda de célula (a pilha virou muda): continua com
// as mesmas 6 horas de antes.
void Mundo::por_maca_existente(int id, int i) {
    macas_chao_[id].celula = i;
    pilha_[i].push_back(id);
    if (ocupacao_[i] != Ocupante::Maca) ocupar(i, Ocupante::Maca);
}

// Tira a maçã de cima da pilha. Se a pilha acabou, a célula fica livre.
int Mundo::tirar_maca_do_chao(int i) {
    if (pilha_[i].empty()) return -1;
    const int id = pilha_[i].back();
    pilha_[i].pop_back();
    const int origem = macas_chao_[id].origem;
    macas_chao_.erase(id);
    if (pilha_[i].empty()) ocupar(i, Ocupante::Nada);
    return origem;
}

// Lugar para uma maçã em volta de `centro`: na primeira camada (1 = as 8
// vizinhas; depois a 2...) que tenha célula de terra vazia ou pilha com
// menos de 32. Dentro da camada, a célula é sorteada.
int Mundo::lugar_para_maca(Pos centro) {
    std::vector<int> opcoes;
    for (int r = 1;; ++r) {
        if (centro.x - r < 0 && centro.y - r < 0 && centro.x + r >= largura() &&
            centro.y + r >= altura())
            return -1;  // passou do mapa todo
        opcoes.clear();
        auto testar = [&](int x, int y) {
            Pos p{x, y};
            if (!dentro(p)) return;
            const int i = indice(p);
            if (livre(p) || (ocupacao_[i] == Ocupante::Maca &&
                             static_cast<int>(pilha_[i].size()) < cfg::MACAS_POR_PILHA))
                opcoes.push_back(i);
        };
        for (int dx = -r; dx <= r; ++dx) {
            testar(centro.x + dx, centro.y - r);
            testar(centro.x + dx, centro.y + r);
        }
        for (int dy = -r + 1; dy <= r - 1; ++dy) {
            testar(centro.x - r, centro.y + dy);
            testar(centro.x + r, centro.y + dy);
        }
        if (!opcoes.empty()) {
            std::uniform_int_distribution<size_t> d(0, opcoes.size() - 1);
            return opcoes[d(rng_)];
        }
    }
}

// 6 horas no chão sem ninguém pegar (decisão do dono do projeto): 40% de
// chance de virar muda; senão apodrece e some. Se vira muda, ela fica na
// célula da pilha e as outras maçãs da pilha vão para as células vizinhas
// (cada uma com as suas 6 horas). Numa célula só cabe uma muda.
void Mundo::evento_maca_vence(int id) {
    const int i = macas_chao_.at(id).celula;
    auto& p = pilha_[i];
    p.erase(std::find(p.begin(), p.end(), id));
    macas_chao_.erase(id);
    std::bernoulli_distribution vira_muda(cfg::MACA_CHANCE_MUDA);
    if (!vira_muda(rng_)) {  // apodreceu
        if (p.empty()) ocupar(i, Ocupante::Nada);
        return;
    }
    // Vira muda: as outras saem da célula primeiro.
    const std::vector<int> resto = p;
    p.clear();
    ocupar(i, Ocupante::Muda);
    const Pos aqui = com_z({i % largura(), i / largura()});
    for (int outra : resto) {
        const int j = lugar_para_maca(aqui);
        if (j >= 0) por_maca_existente(outra, j);
        else macas_chao_.erase(outra);  // não coube em lugar nenhum
    }
    Muda u;
    u.id = proximo_id_muda_++;
    u.pos = aqui;
    u.nasceu = tempo_;
    u.quando_virar = tempo_ + cfg::MUDA_VIRA_ARVORE_ANOS * cfg::SEGUNDOS_POR_ANO;
    mudas_[u.id] = u;
    agendar(u.quando_virar, MUDA_VIRA_ARVORE, u.id);
    registrar(TipoRegistro::MudaNasceu, -1, -1, -1, u.pos);
}

// 3 anos depois: a muda vira macieira (idade 0).
void Mundo::evento_muda_vira_arvore(int id) {
    const Pos p = mudas_.at(id).pos;
    mudas_.erase(id);
    ocupar(indice(p), Ocupante::Nada);
    nova_macieira(p, 0.0);
    registrar(TipoRegistro::ArvoreNasceu, -1, -1, -1, p);
}

std::vector<Muda> Mundo::mudas() const {
    std::vector<Muda> r;
    r.reserve(mudas_.size());
    for (const auto& [id, u] : mudas_) r.push_back(u);
    return r;
}

// 40 anos de vida: a macieira seca (cinza) e não dá mais maçãs.
void Mundo::evento_macieira_seca(Macieira& m) {
    m.seca = true;
    m.quando_sumir = tempo_ + cfg::MACIEIRA_SECA_ANOS * cfg::SEGUNDOS_POR_ANO;
    m.quedas.clear();
    m.quando_queda = -1.0;
    ocupar(indice(m.pos), Ocupante::MacieiraSeca);
    registrar(TipoRegistro::ArvoreSecou, -1, -1, -1, m.pos);
}

// 1 ano seca: some. A célula fica livre. (Fica na lista com existe =
// false, para os números das outras macieiras não mudarem.)
void Mundo::evento_macieira_some(Macieira& m) {
    m.existe = false;
    m.quedas.clear();
    m.quando_queda = -1.0;
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
    const auto v = macas_vencem(x, y);
    return v.empty() ? -1.0 : v.front();
}

int Mundo::macas_em(int x, int y) const {
    return static_cast<int>(pilha_[indice({x, y})].size());
}

std::vector<std::array<int, 3>> Mundo::pilhas() const {
    std::vector<std::array<int, 3>> r;
    for (int i = 0; i < static_cast<int>(pilha_.size()); ++i)
        if (!pilha_[i].empty())
            r.push_back({i % largura(), i / largura(), static_cast<int>(pilha_[i].size())});
    return r;
}

std::vector<double> Mundo::macas_vencem(int x, int y) const {
    std::vector<double> r;
    for (int id : pilha_[indice({x, y})]) r.push_back(macas_chao_.at(id).quando - tempo_);
    std::sort(r.begin(), r.end());
    return r;
}

int Mundo::macas_por_cair(int k) const {
    const Macieira& m = macieiras_[k];
    if (!m.existe || m.seca || m.quedas.empty()) return 0;
    return static_cast<int>(m.quedas.size() - m.proxima_queda);
}

// ===========================================================================
// Estações, maçãs do verão e gelo
// ===========================================================================

// Começou uma estação: registra, faz o que ela traz e agenda a do ano que vem.
void Mundo::evento_estacao(int e) {
    registrar(TipoRegistro::Estacao, e);
    double ini = 0.0, fim = 0.0;
    limites_estacao(tempo_, ini, fim);
    if (e == static_cast<int>(Estacao::Verao)) comecar_verao(ini, fim);
    if (e == static_cast<int>(Estacao::Inverno)) congelar_vales();
    if (e == static_cast<int>(Estacao::Primavera)) degelar_vales();
    const Data d = data_de(tempo_);
    agendar(tempo_de(d.ano + 1, MES_DA_ESTACAO[e], 1), ESTACAO, e);
}

// Verão: cada macieira viva sorteia quantas maçãs vai derrubar (80 a 300) e
// quando cada uma cai: do 2º dia do verão até o fim dele, mais no meio
// (curva sen², a mesma do dia — PROVISÓRIO).
void Mundo::comecar_verao(double inicio, double fim) {
    for (int k = 0; k < static_cast<int>(macieiras_.size()); ++k)
        if (macieiras_[k].existe && !macieiras_[k].seca) gerar_quedas(k, inicio, fim);
}

void Mundo::gerar_quedas(int k, double inicio, double fim) {
    Macieira& m = macieiras_[k];
    std::uniform_int_distribution<int> quantas(cfg::MACAS_POR_VERAO_MIN,
                                               cfg::MACAS_POR_VERAO_MAX);
    m.macas_verao = quantas(rng_);
    // "Começam a cair no 2º dia do verão": a partir das 00:00 do dia 2.
    const double de = inicio + (cfg::QUEDA_COMECA_DIA_DO_VERAO - 1) * cfg::SEGUNDOS_POR_DIA;
    const double janela = fim - de;
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    m.quedas.clear();
    m.proxima_queda = 0;
    m.quedas_base = de;
    for (int q = 0; q < m.macas_verao; ++q) {
        double f;  // sorteia f com chance proporcional à curva (rejeição)
        do f = u01(rng_); while (u01(rng_) > curva_pico(f));
        const double t = de + f * janela;
        if (t > tempo_) m.quedas.push_back(static_cast<float>(f * janela));
    }
    std::sort(m.quedas.begin(), m.quedas.end());
    if (m.quedas.empty()) { m.quando_queda = -1.0; return; }
    m.quando_queda = m.quedas_base + m.quedas[0];
    agendar(m.quando_queda, QUEDA_MACA, k);
}

// Inverno: a camada de cima da água dos vales congela. "Vale" = água dentro
// da terra; o mar (a borda de 8 células) não congela — PROVISÓRIO.
void Mundo::congelar_vales() {
    const int b = cfg::BORDA_AGUA;
    const auto col = mapa_.congelar(b, b, largura() - b, altura() - b);
    if (!col.empty()) registrar(TipoRegistro::Congelou, static_cast<int>(col.size()));
}

// Fim do inverno: o gelo volta a ser água. Quem está em cima do gelo vai
// para a terra livre mais perto (PROVISÓRIO).
void Mundo::degelar_vales() {
    const auto col = mapa_.degelar();
    if (col.empty()) return;
    tirar_de_cima_do_gelo(col);
    contar_livres();
    registrar(TipoRegistro::Derreteu, static_cast<int>(col.size()));
}

void Mundo::tirar_de_cima_do_gelo(const std::vector<int>& colunas) {
    for (int i : colunas) {
        const Ocupante o = ocupacao_[i];
        if (o == Ocupante::Nada) continue;
        const Pos aqui = com_z({i % largura(), i / largura()});
        Pos fora;
        const bool tem = lugar_livre_perto(aqui, fora);
        ocupacao_[i] = Ocupante::Nada;  // (era gelo: não conta como terra livre)
        if (!tem) continue;
        fora = com_z(fora);
        for (Npc& n : npcs_)
            if (n.dentro_abrigo < 0 && n.pos.x == aqui.x && n.pos.y == aqui.y) {
                n.pos = fora;
                n.tem_destino = false;
            }
        ocupar(indice(fora), o);
    }
}

// ===========================================================================
// Abrigo e trabalho (pedra, galhos, machado, corte, construção)
// ===========================================================================

// Quanto tempo leva o trabalho: sorteado a cada vez, entre o mínimo e o
// máximo do dono do projeto (ver config.hpp).
double Mundo::duracao_tarefa(Tarefa t) {
    switch (t) {
        case Tarefa::Lascas: return sortear(cfg::LASCAS_MIN_S, cfg::LASCAS_MAX_S);
        case Tarefa::Galhos: return sortear(cfg::GALHOS_MIN_S, cfg::GALHOS_MAX_S);
        case Tarefa::Machado: return sortear(cfg::MACHADO_MIN_S, cfg::MACHADO_MAX_S);
        case Tarefa::Cortar: return sortear(cfg::CORTAR_MIN_S, cfg::CORTAR_MAX_S);
        case Tarefa::Construir: return sortear(cfg::CONSTRUIR_MIN_S, cfg::CONSTRUIR_MAX_S);
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

// Segundos até a temperatura ficar abaixo de 4° (0 se já está). Se não
// fica nas próximas 24 h, conta como 24 h. O resultado fica guardado (e
// vale até o momento achado, ou por 1 hora se não achou), porque procurar
// de minuto em minuto a cada passo do NPC sairia caro.
double Mundo::ate_critico_frio() const {
    if (temperatura() < cfg::TEMP_CRITICA) return 0.0;
    if (tempo_ < critico_valido_ate_) return std::max(0.0, critico_em_ - tempo_);
    const double passo = cfg::SEGUNDOS_POR_MINUTO;  // 1 minuto do jogo
    for (double t = passo; t <= cfg::SEGUNDOS_POR_DIA; t += passo)
        if (temperatura_em(tempo_ + t) < cfg::TEMP_CRITICA) {
            critico_em_ = tempo_ + t;
            critico_valido_ate_ = critico_em_;
            return t;
        }
    critico_em_ = tempo_ + cfg::SEGUNDOS_POR_DIA;
    critico_valido_ate_ = tempo_ + cfg::SEGUNDOS_POR_HORA;
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
//   lenha >= 20 -> construir aqui (no gelo não: na terra livre mais perto);
//   6 lascas e 2 galhos, sem machado -> fazer o machado (onde está, 1 a 3 h);
//   machado -> cortar uma árvore;
//   senão: lascas (até 6) e galhos (até 2) para o machado — o que estiver
//   mais perto entre os dois que faltam.
bool Mundo::escolher_tarefa(Npc& n) {
    if (n.lenha >= cfg::ABRIGO_LENHA) {
        n.tarefa = Tarefa::Construir;
        n.tarefa_pos = n.pos;
        if (mapa_.e_gelo(n.pos.x, n.pos.y)) {
            Pos terra;
            if (!lugar_livre_perto(n.pos, terra)) return false;
            n.tarefa_pos = com_z(terra);
        }
        n.tarefa_alvo = -1;
        n.tarefa_restante = 0.0;
        return true;
    }
    if (!n.machado && n.lascas >= cfg::MACHADO_LASCAS && n.galhos >= cfg::MACHADO_GALHOS) {
        n.tarefa = Tarefa::Machado;
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
    // Antes de calcular os caminhos (caro), vê se existe algum candidato.
    const int metade = cfg::VISAO_LADO / 2;
    {
        bool algum = false;
        for (int dy = -metade; dy < metade && !algum; ++dy)
            for (int dx = -metade; dx < metade; ++dx) {
                Pos p{n.pos.x + dx, n.pos.y + dy};
                if (!dentro(p)) continue;
                const Ocupante o = ocupacao_[indice(p)];
                if (pedra ? (o == Ocupante::Pedra)
                          : (o == Ocupante::Macieira || o == Ocupante::MacieiraSeca)) {
                    algum = true;
                    break;
                }
            }
        if (!algum && (pedra ? n.pedras_lembradas : n.macieiras_lembradas).empty())
            return false;
    }
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
    // Construir é na própria célula da obra; o machado, onde ele estiver;
    // o resto, ao lado do alvo.
    if (n.tarefa == Tarefa::Machado) n.tarefa_pos = n.pos;
    const bool chegou = (n.tarefa == Tarefa::Machado) ? true
                      : (n.tarefa == Tarefa::Construir) ? (d == 0) : (d == 1);
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
    } else if (t == Tarefa::Galhos) {
        if (!macieiras_[n.tarefa_alvo].existe) return;
        n.galhos += cfg::GALHOS_POR_VEZ;  // a árvore não se estraga
        lembrar_macieira(n, n.tarefa_alvo);
    } else if (t == Tarefa::Machado) {
        n.lascas -= cfg::MACHADO_LASCAS;
        n.galhos -= cfg::MACHADO_GALHOS;
        n.machado = true;  // dura para sempre
        registrar(TipoRegistro::MachadoFeito, n.id);
    } else if (t == Tarefa::Cortar) {
        Macieira& m = macieiras_[n.tarefa_alvo];
        if (!m.existe) return;
        m.existe = false;  // a árvore (viva ou seca) some
        m.quedas.clear();
        m.quando_queda = -1.0;
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

// Idade em anos (com fração). Como o ano tem sempre 365 dias, completar
// N anos é exatamente N × 365 dias depois do nascimento: o aniversário.
double Mundo::idade_anos(const Npc& n) const {
    const double agora = n.morreu_em >= 0.0 ? n.morreu_em : tempo_;
    return (agora - n.nascimento) / cfg::SEGUNDOS_POR_ANO;
}

// Idade de ter filhos (anos completos): dos 16 até os 45 (mulher) / 60
// (homem), inclusive — PROVISÓRIO ("até 45" = enquanto tem 45 anos).
bool Mundo::pode_ter_filho(const Npc& n) const {
    if (n.bebe || n.estado == EstadoNpc::Morto) return false;
    const double anos = std::floor(idade_anos(n) + 1e-9);
    const double max = (n.sexo == Sexo::Mulher) ? cfg::IDADE_MAX_PROCRIAR_MULHER
                                                : cfg::IDADE_MAX_PROCRIAR_HOMEM;
    return anos >= cfg::IDADE_MIN_PROCRIAR && anos <= max;
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
// Idade mínima para o casal: 16 anos (a de procriar) — PROVISÓRIO.
void Mundo::formar_casal(Npc& n) {
    if (n.bebe || n.parceiro >= 0) return;
    if (idade_anos(n) + 1e-9 < cfg::IDADE_MIN_PROCRIAR) return;
    const int metade = cfg::VISAO_LADO / 2;
    int melhor = -1, melhor_d = 0;
    for (const Npc& o : npcs_) {
        if (o.id == n.id || o.bebe || o.estado == EstadoNpc::Morto) continue;
        if (o.sexo == n.sexo || o.parceiro >= 0) continue;
        if (idade_anos(o) + 1e-9 < cfg::IDADE_MIN_PROCRIAR) continue;
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

// Quer procriar agora? Os dois na idade de ter filhos, o casal com o mesmo
// abrigo, sem precisar comer nem beber, e a mulher podendo engravidar (não
// está grávida e já passaram 2 meses do último parto).
bool Mundo::quer_procriar(const Npc& n) const {
    if (n.bebe || n.parceiro < 0) return false;
    const Npc& p = npcs_[n.parceiro];
    if (!pode_ter_filho(n) || !pode_ter_filho(p)) return false;
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
        if (!pode_ter_filho(m) || !pode_ter_filho(h)) continue;
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
    if (!pode_ter_filho(m) || !pode_ter_filho(h)) return;
    m.gravida = true;
    m.gravidez_pai = h.id;
    m.quando_nascer = somar_meses(tempo_, cfg::GRAVIDEZ_MESES);  // 9 meses
    agendar(m.quando_nascer, NASCIMENTO, m.id);
    registrar(TipoRegistro::Gravidez, m.id, h.id);
}

// 9 meses depois: nasce um bebê ao lado da mãe (sexo meio a meio).
// Nome: "F." + nome da mãe; do 2º filho em diante, "2F.", "3F."...
void Mundo::evento_nascimento(int mae) {
    Pos onde;
    if (!lugar_livre_perto(npcs_[mae].pos, onde)) return;  // sem lugar (PROVISÓRIO)
    Npc& m = npcs_[mae];
    m.gravida = false;
    m.pode_engravidar_em = somar_meses(tempo_, cfg::INTERVALO_FILHOS_MESES);
    m.filhos += 1;
    Npc b;
    b.id = static_cast<int>(npcs_.size());
    b.nome = (m.filhos == 1 ? std::string("F.") : std::to_string(m.filhos) + "F.") + m.nome;
    std::uniform_int_distribution<int> moeda(0, 1);
    b.sexo = moeda(rng_) ? Sexo::Mulher : Sexo::Homem;
    b.bebe = true;
    b.estado = EstadoNpc::Bebe;
    b.nascimento = tempo_;
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
    npcs_.back().quando_adulto = tempo_ + cfg::BEBE_VIRA_ADULTO_ANOS * cfg::SEGUNDOS_POR_ANO;
    agendar(npcs_.back().quando_adulto, VIRA_ADULTO, b.id);
    npcs_.back().quando_velhice = tempo_ + cfg::IDADE_MAXIMA_ANOS * cfg::SEGUNDOS_POR_ANO;
    agendar(npcs_.back().quando_velhice, VELHICE, b.id);
    registrar(TipoRegistro::Nascimento, b.id, mae, pai);
}

// 15 anos: vira adulto (a idade continua contando normalmente).
void Mundo::evento_adulto(int id) {
    Npc& n = npcs_[id];
    if (n.estado == EstadoNpc::Morto || !n.bebe) return;
    n.bebe = false;
    n.estado = EstadoNpc::Vagando;
    n.falta_passo = cfg::NPC_PASSO_S;
    n.falta_fome = cfg::FOME_INTERVALO_S;
    n.falta_sede = cfg::SEDE_INTERVALO_S;
    registrar(TipoRegistro::VirouAdulto, id);
}

// 100 anos: morre de velhice.
void Mundo::evento_velhice(int id) {
    Npc& n = npcs_[id];
    if (n.estado == EstadoNpc::Morto) return;
    morrer(n, 8);
}

// Uma maçã cai em volta da macieira: na primeira camada que tiver lugar
// (célula vazia ou pilha com menos de 32), sorteada dentro da camada.
void Mundo::evento_queda_maca(int indice_macieira) {
    const int i = lugar_para_maca(macieiras_[indice_macieira].pos);
    if (i >= 0) por_maca_no_chao(i, indice_macieira);
}

// ===========================================================================
// Ajudantes
// ===========================================================================

// --- Calendário -----------------------------------------------------------
// O tempo da simulação (tempo_) é contado em segundos desde 01/01/0001 às
// 07:00. Somando as 7 h, vira "segundos desde 01/01/0001 às 00:00"; daí
// sai o dia (÷ 86 400), o ano (÷ 365 dias) e o mês (pela tabela de dias).

Data Mundo::data_de(double t) {
    const double desde_zero = t + cfg::HORA_INICIAL * cfg::SEGUNDOS_POR_HORA;
    const long long seg = static_cast<long long>(std::floor(desde_zero + 1e-6));
    const long long por_dia = static_cast<long long>(cfg::SEGUNDOS_POR_DIA);
    long long dias = seg / por_dia, resto = seg % por_dia;
    if (resto < 0) { resto += por_dia; dias -= 1; }  // antes do começo
    long long ano0 = dias / cfg::DIAS_POR_ANO, dia_ano = dias % cfg::DIAS_POR_ANO;
    if (dia_ano < 0) { dia_ano += cfg::DIAS_POR_ANO; ano0 -= 1; }
    Data d;
    d.ano = static_cast<int>(ano0) + cfg::ANO_INICIAL;
    int mes = 0;
    while (dia_ano >= cfg::DIAS_DO_MES[mes]) dia_ano -= cfg::DIAS_DO_MES[mes++];
    d.mes = mes + 1;
    d.dia = static_cast<int>(dia_ano) + 1;
    d.hora = static_cast<int>(resto / 3600);
    d.minuto = static_cast<int>((resto % 3600) / 60);
    d.segundo = static_cast<int>(resto % 60);
    return d;
}

double Mundo::tempo_de(int ano, int mes, int dia) {
    const double dias = static_cast<double>(ano - cfg::ANO_INICIAL) * cfg::DIAS_POR_ANO +
                        dia_do_ano(mes, dia);
    return dias * cfg::SEGUNDOS_POR_DIA - cfg::HORA_INICIAL * cfg::SEGUNDOS_POR_HORA;
}

double Mundo::somar_meses(double t, int meses) {
    const Data d = data_de(t);
    const double hora = t - tempo_de(d.ano, d.mes, d.dia);  // segundos no dia
    int m0 = d.mes - 1 + meses;
    int ano = d.ano + m0 / 12;
    m0 %= 12;
    if (m0 < 0) { m0 += 12; ano -= 1; }
    const int dia = std::min(d.dia, cfg::DIAS_DO_MES[m0]);
    return tempo_de(ano, m0 + 1, dia) + hora;
}

Estacao Mundo::estacao_de(double t) {
    const int m = data_de(t).mes;
    if (m == 12 || m <= 2) return Estacao::Verao;
    if (m <= 5) return Estacao::Outono;
    if (m <= 8) return Estacao::Inverno;
    return Estacao::Primavera;
}

void Mundo::limites_estacao(double t, double& inicio, double& fim) {
    const Data d = data_de(t);
    const int e = static_cast<int>(estacao_de(t));
    const int mes_ini = MES_DA_ESTACAO[e];
    // O verão começa em dezembro do ano anterior quando estamos em jan/fev.
    const int ano_ini = (mes_ini > d.mes) ? d.ano - 1 : d.ano;
    inicio = tempo_de(ano_ini, mes_ini, 1);
    const int mes_fim = MES_DA_ESTACAO[(e + 1) % 4];
    const int ano_fim = (mes_fim < mes_ini) ? ano_ini + 1 : ano_ini;
    fim = tempo_de(ano_fim, mes_fim, 1);
}

// --- Dia, noite, temperatura, HP -------------------------------------------

// Horas de sol no dia de t (calculado ao meio-dia desse dia, para o nascer e
// o pôr do sol serem os mesmos o dia inteiro). Primavera e outono: 12 h.
// Verão: sobe em curva até 13 h 30 no meio da estação e volta; inverno:
// desce até 11 h no meio e volta.
double Mundo::horas_de_sol(double t) {
    const Data d = data_de(t);
    const double meio_dia = tempo_de(d.ano, d.mes, d.dia) + 12.0 * cfg::SEGUNDOS_POR_HORA;
    double ini = 0.0, fim = 0.0;
    limites_estacao(meio_dia, ini, fim);
    const double f = (meio_dia - ini) / (fim - ini);
    switch (estacao_de(meio_dia)) {
        case Estacao::Verao:
            return cfg::DIA_PADRAO_H + (cfg::DIA_MAX_VERAO_H - cfg::DIA_PADRAO_H) * curva_pico(f);
        case Estacao::Inverno:
            return cfg::DIA_PADRAO_H + (cfg::DIA_MIN_INVERNO_H - cfg::DIA_PADRAO_H) * curva_pico(f);
        default:
            return cfg::DIA_PADRAO_H;
    }
}

// Mínima e máxima da temperatura em t. Os valores do dono valem no MEIO de
// cada estação; entre o meio de uma e o meio da seguinte, mudam aos poucos
// (curva suave).
void Mundo::min_max_temperatura(double t, double& minima, double& maxima) {
    static constexpr double MIN[4] = {cfg::VERAO_MIN, cfg::OUTONO_MIN,
                                      cfg::INVERNO_MIN, cfg::PRIMAVERA_MIN};
    static constexpr double MAX[4] = {cfg::VERAO_MAX, cfg::OUTONO_MAX,
                                      cfg::INVERNO_MAX, cfg::PRIMAVERA_MAX};
    double ini = 0.0, fim = 0.0;
    limites_estacao(t, ini, fim);
    const int e = static_cast<int>(estacao_de(t));
    const double meio = (ini + fim) / 2.0;
    int a, b;            // estações de cada ponta do trecho
    double ta, tb;       // o meio de cada uma
    if (t >= meio) {     // do meio desta até o meio da próxima
        double i2 = 0.0, f2 = 0.0;
        limites_estacao(fim + 1.0, i2, f2);
        a = e; b = (e + 1) % 4; ta = meio; tb = (i2 + f2) / 2.0;
    } else {             // do meio da anterior até o meio desta
        double i0 = 0.0, f0 = 0.0;
        limites_estacao(ini - 1.0, i0, f0);
        a = (e + 3) % 4; b = e; ta = (i0 + f0) / 2.0; tb = meio;
    }
    const double w = curva_passagem((t - ta) / (tb - ta));
    minima = MIN[a] + (MIN[b] - MIN[a]) * w;
    maxima = MAX[a] + (MAX[b] - MAX[a]) * w;
}

// Hora do dia (0 a 24, com fração) no tempo t da simulação.
double Mundo::hora_do_dia(double t) const {
    const double total_h = t / cfg::SEGUNDOS_POR_HORA + cfg::HORA_INICIAL;
    double h = std::fmod(total_h, static_cast<double>(cfg::HORAS_POR_DIA));
    if (h < 0) h += cfg::HORAS_POR_DIA;
    return h;
}

// "Dia" do abrigo: 06:00–18:00 (regra antiga, o dono vai mudar depois).
bool Mundo::e_dia() const {
    const double h = hora_do_dia(tempo_);
    return h >= cfg::DIA_COMECA_H && h < cfg::NOITE_COMECA_H;
}

bool Mundo::sol() const {
    const double h = hora_do_dia(tempo_);
    return h >= nascer_do_sol(tempo_) && h < por_do_sol(tempo_);
}

// Temperatura em t: entre a mínima e a máxima do dia, com a forma do dia
// presa ao nascer e ao pôr do sol (ver config.hpp), em linha reta entre:
//   mínima (nascer − 2 h) -> máxima (nascer + 8 h) -> máxima (pôr do sol)
//   -> mínima (pôr + 4 h) -> mínima até a manhã seguinte.
double Mundo::temperatura_em(double t) const {
    double tmin = 0.0, tmax = 0.0;
    min_max_temperatura(t, tmin, tmax);
    const double h = hora_do_dia(t);
    const double nasce = nascer_do_sol(t), poe = por_do_sol(t);
    const double a = nasce - cfg::TEMP_MIN_ANTES_NASCER_H;
    const double b = nasce + cfg::TEMP_MAX_DEPOIS_NASCER_H;
    const double c = poe;
    const double d = poe + cfg::TEMP_MIN_DEPOIS_POR_H;
    if (h < a || h >= d) return tmin;
    if (h < b) return tmin + (tmax - tmin) * (h - a) / (b - a);
    if (h < c) return tmax;
    return tmax + (tmin - tmax) * (h - c) / (d - c);
}

// Próximo momento em que a temperatura cruza 4° ou 5°, ou que dá 06:00.
// Procura de minuto em minuto do jogo e afina no fim (a curva é feita de
// retas, então isso acha o cruzamento com folga de precisão).
double Mundo::proximo_marco_temperatura() const {
    if (marco_temp_ > tempo_ + 1e-9) return marco_temp_;  // ainda vale
    const double passo = cfg::SEGUNDOS_POR_MINUTO;  // 1 minuto do jogo
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

// HP: −5 por minuto por necessidade no crítico (fome 0, sede 0, frio < 4°).
double Mundo::taxa_hp(const Npc& n) const {
    if (n.bebe) return 0.0;  // bebê: sem necessidades (por enquanto)
    // Dentro do abrigo: nada é crítico (nem frio, nem fome, nem sede) e o
    // HP se recupera 1 por minuto, até 100.
    if (n.dentro_abrigo >= 0)
        return n.hp < cfg::HP_MAXIMO ? cfg::HP_RECUPERA_ABRIGO_MIN / cfg::SEGUNDOS_POR_MINUTO
                                     : 0.0;
    int criticos = 0;
    if (n.fome <= cfg::FOME_MINIMA) ++criticos;
    if (n.sede <= cfg::SEDE_MINIMA) ++criticos;
    if (temperatura() < cfg::TEMP_CRITICA) ++criticos;
    return -cfg::HP_DANO_POR_CRITICO_MIN / cfg::SEGUNDOS_POR_MINUTO * criticos;
}

double Mundo::ate_critico_fome(const Npc& n) const {
    return n.fome / (cfg::FOME_QUEDA / cfg::FOME_INTERVALO_S);
}

double Mundo::ate_critico_sede(const Npc& n) const {
    return n.sede / (cfg::SEDE_QUEDA / cfg::SEDE_INTERVALO_S);
}


Ocupante Mundo::ocupante(int x, int y) const { return ocupacao_[indice({x, y})]; }


// Livre = dentro do mapa, terra firme (não é água nem gelo) e sem nada em
// cima. É onde as coisas podem ser postas (maçã, bebê, corpo, abrigo).
bool Mundo::livre(Pos p) const {
    if (!dentro(p)) return false;
    return mapa_.e_terra(p.x, p.y) && ocupacao_[indice(p)] == Ocupante::Nada;
}

// Para ANDAR também vale o gelo (pedido do dono do projeto).
bool Mundo::pisavel(Pos p) const {
    if (!dentro(p)) return false;
    return (mapa_.e_terra(p.x, p.y) || mapa_.e_gelo(p.x, p.y)) &&
           ocupacao_[indice(p)] == Ocupante::Nada;
}

// Passar de uma coluna para a vizinha: precisa estar livre (terra ou gelo)
// e o degrau (diferença de altura) ser de no máximo 1 nível.
bool Mundo::pode_pisar(Pos de, Pos para) const {
    if (!pisavel(para)) return false;
    const int degrau = std::abs(mapa_.superficie(para.x, para.y) - de.z);
    return degrau <= cfg::NPC_DEGRAU_MAX;
}

// Procura a pilha de maçãs mais próxima dentro da área de visão (32 x 32 em
// volta do NPC: 16 células para um lado e 15 para o outro).
bool Mundo::procurar_maca(const Npc& n, Pos& achada) const {
    const int metade = cfg::VISAO_LADO / 2;
    bool achou = false;
    int melhor_passos = 0, melhor_d2 = 0;
    for (int dy = -metade; dy < metade; ++dy)
        for (int dx = -metade; dx < metade; ++dx) {
            Pos p{n.pos.x + dx, n.pos.y + dy};
            if (!dentro(p) || ocupacao_[indice(p)] != Ocupante::Maca) continue;
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
// próximo passo só vem depois de 10 s em vez de 5 s.
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
    // A maçã que ele estava comendo cai junto com as que carregava.
    if (n.comendo) {
        n.comendo = false;
        n.macas_carregadas += 1;
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
    while (n.macas_carregadas > 0 && soltar_maca_perto(n.pos, -1))
        n.macas_carregadas -= 1;
    n.macas_carregadas = 0;
    registrar(TipoRegistro::Morte, n.id, n.causa_morte);
}

// Põe uma maçã no chão perto de `onde` (célula vazia ou pilha com lugar,
// na camada mais perto), sorteada dentro da camada.
bool Mundo::soltar_maca_perto(Pos onde, int origem) {
    const int i = lugar_para_maca(onde);
    if (i < 0) return false;
    por_maca_no_chao(i, origem);
    return true;
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
    n.nascimento = tempo_ - anos * cfg::SEGUNDOS_POR_ANO;
    n.quando_velhice = n.nascimento + cfg::IDADE_MAXIMA_ANOS * cfg::SEGUNDOS_POR_ANO;
    agendar(n.quando_velhice, VELHICE, id);
    if (n.bebe) {
        n.quando_adulto = n.nascimento + cfg::BEBE_VIRA_ADULTO_ANOS * cfg::SEGUNDOS_POR_ANO;
        agendar(n.quando_adulto, VIRA_ADULTO, id);
    }
}

void Mundo::teste_macas(int id, int quantas) { npcs_[id].macas_carregadas = quantas; }

int Mundo::teste_abrigo(int id, int x, int y, int macas) {
    Pos p{x, y};
    if (!livre(p)) return -1;
    Abrigo ab;
    ab.pos = com_z(p);
    ab.macas = macas;
    const int a = static_cast<int>(abrigos_.size());
    abrigos_.push_back(ab);
    ocupar(indice(p), Ocupante::Abrigo);
    Npc& n = npcs_[id];
    sair_de_morador(n);
    abrigos_[a].moradores.push_back(id);
    n.abrigo_casa = a;
    return a;
}

void Mundo::teste_idade_macieira(int k, double anos) {
    Macieira& m = macieiras_[k];
    m.nasceu = tempo_ - anos * cfg::SEGUNDOS_POR_ANO;
    m.quando_secar = m.nasceu + cfg::MACIEIRA_VIDA_ANOS * cfg::SEGUNDOS_POR_ANO;
    agendar(m.quando_secar, MACIEIRA_SECA, k);
}

void Mundo::teste_adiantar_parto(int mae, double segundos) {
    Npc& m = npcs_[mae];
    if (!m.gravida) return;
    m.quando_nascer = tempo_ + segundos;
    agendar(m.quando_nascer, NASCIMENTO, mae);
}

void Mundo::teste_saltar(double t) {
    // Tira da fila o que é do calendário (quedas, maçãs, estações).
    std::vector<Evento> manter;
    while (!fila_.empty()) {
        const Evento e = fila_.top();
        fila_.pop();
        if (e.tipo != QUEDA_MACA && e.tipo != MACA_VENCE && e.tipo != ESTACAO)
            manter.push_back(e);
    }
    for (const Evento& e : manter) fila_.push(e);
    for (size_t i = 0; i < pilha_.size(); ++i)
        if (!pilha_[i].empty()) {
            pilha_[i].clear();
            ocupar(static_cast<int>(i), Ocupante::Nada);
        }
    macas_chao_.clear();
    for (Macieira& m : macieiras_) {
        m.quedas.clear();
        m.quando_queda = -1.0;
        m.proxima_queda = 0;
    }
    tempo_ = t;
    marco_temp_ = -1.0;
    critico_valido_ate_ = -1.0;
    // A estação da data nova.
    const auto gelo = mapa_.degelar();
    if (!gelo.empty()) { tirar_de_cima_do_gelo(gelo); contar_livres(); }
    double ini = 0.0, fim = 0.0;
    limites_estacao(tempo_, ini, fim);
    const Estacao agora = estacao_de(tempo_);
    if (agora == Estacao::Verao) comecar_verao(ini, fim);
    if (agora == Estacao::Inverno) congelar_vales();
    for (int e = 0; e < 4; ++e) {
        const Data d = data_de(tempo_);
        double quando = tempo_de(d.ano, MES_DA_ESTACAO[e], 1);
        if (quando <= tempo_) quando = tempo_de(d.ano + 1, MES_DA_ESTACAO[e], 1);
        agendar(quando, ESTACAO, e);
    }
}

void Mundo::teste_definir(int x, int y, int z, Material m, int nivel_agua) {
    mapa_.definir(x, y, z, m, nivel_agua);
    mapa_.recalcular_superficie(x, y);
    contar_livres();
}

}  // namespace genesis
