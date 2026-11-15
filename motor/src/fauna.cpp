// ---------------------------------------------------------------------------
// fauna.cpp — os animais (pedido do dono do projeto em 15/11/2026).
//
// O "molde": todo animal tem fome, sede, HP, idade, visão, memória (água),
// sono, explorar, bando e um jeito de se reproduzir. Os números de cada
// espécie ficam numa ficha no config.hpp (FichaEspecie). Hoje só existe a
// galinha (🐔 galinha, 🐓 galo, 🐤 pintinho, 🥚 ovo).
//
// Como o tempo anda para os animais: TUDO pela fila de eventos (podem ser
// muitos). Cada animal tem a hora do próximo passo (A_PASSO), da próxima
// queda de fome/sede (A_NEC) e, se o HP está caindo, a hora em que chega a
// 0 (A_SEM_HP). O HP é contínuo: guarda-se o valor num instante (hp, hp_t)
// e a taxa; muda-se a taxa quando algo muda (fome/sede em 0, dormir...).
//
// Ver docs/FAUNA.md.
// ---------------------------------------------------------------------------
#include <algorithm>
#include <cmath>

#include "config.hpp"
#include "interno.hpp"
#include "mundo.hpp"

namespace genesis {

namespace cfg = config;
using namespace interno;
using cfg::FichaEspecie;

namespace {
const FichaEspecie& ficha(const Animal&) { return cfg::GALINHA; }  // só galinha, por ora
constexpr double DIA = cfg::SEGUNDOS_POR_DIA;
}  // namespace

// ===========================================================================
// Criação
// ===========================================================================

// 2 bandos: em cada um, 1 galo num lugar sorteado e 5 galinhas em volta.
// Todos com 91 dias (decisão do dono).
void Mundo::criar_galinhas() {
    for (int b = 0; b < cfg::BANDOS_INICIAIS && livres_terra_ > 0; ++b) {
        const Pos p = sortear_grama_livre();
        const int galo = novo_animal(Sexo::Homem, cfg::IDADE_INICIAL_GALINHA_DIAS, p, -1, -1);
        animais_[galo].bando = galo;
        for (int k = 0; k < cfg::GALINHAS_POR_BANDO; ++k) {
            Pos q;
            if (!lugar_livre_perto(p, q)) break;
            novo_animal(Sexo::Mulher, cfg::IDADE_INICIAL_GALINHA_DIAS, com_z(q), -1, galo);
        }
    }
}

int Mundo::novo_animal(Sexo sexo, double idade_dias, Pos pos, int mae, int bando) {
    Animal a;
    const FichaEspecie& f = cfg::GALINHA;
    a.id = static_cast<int>(animais_.size());
    a.sexo = sexo;
    a.nascimento = tempo_ - idade_dias * DIA;
    a.fase = idade_dias >= f.adulto_dias ? 2 : idade_dias >= f.independente_dias ? 1 : 0;
    a.pos = com_z(pos);
    a.mae = mae;
    a.bando = bando;
    a.hp_t = tempo_;
    std::uniform_int_distribution<int> db(f.bando_dist_min, f.bando_dist_max);
    std::uniform_int_distribution<int> df(f.fuga_min, f.fuga_max);
    a.dist_bando = db(rng_);
    a.dist_fuga = df(rng_);
    animais_.push_back(a);
    Animal& x = animais_.back();
    x.hp = hp_max(x);
    ocupar(indice(x.pos), Ocupante::Animal);
    x.quando_nec = tempo_ + f.fome_intervalo_s;
    agendar(x.quando_nec, A_NEC, x.id);
    // Marcos da idade: 60 dias, 90 dias, 3 anos (o evento confere qual é).
    for (double d : {f.independente_dias, f.adulto_dias, f.velhice_anos * 365.0}) {
        const double t = x.nascimento + d * DIA;
        if (t > tempo_) agendar(t, A_CRESCE, x.id);
    }
    // O primeiro passo vem num instante sorteado (para não andarem todos juntos).
    agendar_passo(x, sortear(0.0, f.passo_s));
    recalcular_hp(x);
    return x.id;
}

// ===========================================================================
// Ajudantes
// ===========================================================================

double Mundo::idade_dias(const Animal& a) const {
    const double agora = a.morreu_em >= 0.0 ? a.morreu_em : tempo_;
    return (agora - a.nascimento) / DIA;
}

double Mundo::hp_max(const Animal& a) const {
    const FichaEspecie& f = ficha(a);
    if (a.fase < 2) return f.hp_filhote;
    return a.sexo == Sexo::Homem ? f.hp_macho : f.hp_femea;
}

// Dorme das 19:00 às 05:00, onde estiver.
// (A hora é tirada 1 ms à frente: às 05:00 em ponto, um arredondamento
// podia dar 04:59:59,999... e o animal "acordar" para sempre no mesmo
// instante.)
bool Mundo::dormindo_agora(const Animal& a) const {
    const double h = hora_do_dia(tempo_ + 1e-3);
    const FichaEspecie& f = ficha(a);
    return h >= f.dorme_h || h < f.acorda_h;
}

// Sem fome nem sede caindo: pintinho com a mãe (até 60 dias), galinha
// chocando (pedido do dono) e dormindo (PROVISÓRIO: como o NPC no abrigo;
// senão, com a sede caindo 4 a cada 10 min, todas morreriam toda noite).
bool Mundo::sem_necessidades(const Animal& a) const {
    return a.fase == 0 || a.estado == EstadoAnimal::Chocando ||
           a.estado == EstadoAnimal::Dormindo;
}

void Mundo::agendar_passo(Animal& a, double daqui) {
    a.quando_passo = tempo_ + daqui;
    agendar(a.quando_passo, A_PASSO, a.id);
}

double Mundo::hp_animal(const Animal& a) const {
    if (a.estado == EstadoAnimal::Morto) return 0.0;
    return std::clamp(a.hp + a.taxa_hp * (tempo_ - a.hp_t), 0.0, hp_max(a));
}

void Mundo::atualizar_hp(Animal& a) {
    a.hp = hp_animal(a);
    a.hp_t = tempo_;
}

// Taxa do HP: −5 por minuto por necessidade em 0 (somando); dormindo,
// +5 por minuto (até o máximo). Se cai, marca a hora em que chega a 0.
void Mundo::recalcular_hp(Animal& a) {
    atualizar_hp(a);
    const FichaEspecie& f = ficha(a);
    int criticos = 0;
    if (a.fase > 0 && a.fome <= 0.0) ++criticos;
    if (a.fase > 0 && a.sede <= 0.0) ++criticos;
    double por_min = -f.dano_critico_min * criticos;
    if (a.estado == EstadoAnimal::Dormindo && a.hp < hp_max(a)) por_min += f.recupera_dormindo_min;
    a.taxa_hp = por_min / cfg::SEGUNDOS_POR_MINUTO;
    if (a.taxa_hp < 0.0) {
        a.quando_sem_hp = tempo_ + a.hp / -a.taxa_hp;
        agendar(a.quando_sem_hp, A_SEM_HP, a.id);
    } else {
        a.quando_sem_hp = -1.0;
    }
}

int Mundo::animal_em(int x, int y) const {
    for (const Animal& a : animais_)
        if (a.estado != EstadoAnimal::Morto && a.pos.x == x && a.pos.y == y) return a.id;
    return -1;
}

// Pode o animal ir de `de` para `para` (vizinhas)? Célula livre (terra ou
// gelo) e degrau de no máximo 1. A galinha pode entrar no próprio ninho
// (a pilha de ovos dela).
bool Mundo::pode_pisar_animal(const Animal& a, Pos de, Pos para) const {
    if (!dentro(para)) return false;
    const int i = indice(para);
    const bool terreno = mapa_.e_terra(para.x, para.y) || mapa_.e_gelo(para.x, para.y);
    if (!terreno) return false;
    bool ok = ocupacao_[i] == Ocupante::Nada;
    if (!ok && ocupacao_[i] == Ocupante::Ovo && i == a.ninho) ok = true;
    if (!ok) return false;
    return std::abs(mapa_.superficie(para.x, para.y) - de.z) <= ficha(a).degrau_max;
}

void Mundo::mover_animal(Animal& a, Pos para) {
    const int de = indice(a.pos);
    auto it = ovos_.find(de);
    if (it != ovos_.end() && it->second.ovos > 0) {
        ocupar(de, Ocupante::Ovo);
        if (it->second.ninho_de == a.id) sair_do_ninho(a);
    } else {
        ocupar(de, Ocupante::Nada);
    }
    a.pos = com_z(para);
    const int i = indice(a.pos);
    ocupar(i, Ocupante::Animal);
    auto ov = ovos_.find(i);
    if (ov != ovos_.end() && ov->second.ninho_de == a.id) ov->second.com_galinha = true;
}

// A galinha saiu de cima dos ovos: o contato acaba agora; sem contato por
// 1 dia, eles estragam.
void Mundo::sair_do_ninho(Animal& a) {
    if (a.ninho < 0) return;
    auto it = ovos_.find(a.ninho);
    if (it == ovos_.end() || !it->second.com_galinha) return;
    it->second.com_galinha = false;
    it->second.contato = tempo_;
    it->second.estraga_em = tempo_ + ficha(a).ovo_estraga_dias * DIA;
    agendar(it->second.estraga_em, OVOS_ESTRAGAM, a.ninho);
    // Se estava chocando e saiu (fugiu, lutou), a choca acabou: os ovos
    // ficam abandonados — PROVISÓRIO.
    if (a.estado == EstadoAnimal::Chocando || a.eclosao > 0) {
        a.eclosao = -1.0;
        a.fertil = false;
        a.ovos_postos = 0;
        it->second.ninho_de = -1;
        a.ninho = -1;
    }
}

// Um passo de 1 célula: em direção a `alvo` (o vizinho que mais aproxima)
// ou, com `fugir`, para longe dele. Devolve se andou; subiu/desceu fica em
// `subiu_` (o próximo passo demora mais).
bool Mundo::passo_animal(Animal& a, Pos alvo, bool fugir) {
    const int agora = distancia_passos(a.pos, alvo);
    bool achou = false;
    Pos melhor{};
    int melhor_ps = 0, melhor_d2 = 0;
    for (Pos v : VIZINHOS) {
        Pos p = com_z({a.pos.x + v.x, a.pos.y + v.y});
        if (!pode_pisar_animal(a, a.pos, p)) continue;
        int ps = distancia_passos(p, alvo), d2 = distancia2(p, alvo);
        if (fugir) { ps = -ps; d2 = -d2; }
        if (!achou || ps < melhor_ps || (ps == melhor_ps && d2 < melhor_d2)) {
            achou = true;
            melhor = p;
            melhor_ps = ps;
            melhor_d2 = d2;
        }
    }
    if (!achou) return false;
    if (!fugir && melhor_ps >= agora) return false;  // não aproxima
    if (fugir && -melhor_ps < agora) return false;   // não afasta
    a_subiu_ = melhor.z != a.pos.z;
    mover_animal(a, melhor);
    return true;
}

int Mundo::buscar_animal(Animal& a, const std::function<bool(Pos)>& serve, bool mapa_todo) {
    if (serve(a.pos)) { a.caminho.clear(); return 1; }
    // No mapa todo (caro): segue o caminho guardado enquanto ele valer.
    if (mapa_todo && !a.caminho.empty() && serve(a.caminho.front())) {
        const Pos prox = com_z(a.caminho.back());
        if (pode_pisar_animal(a, a.pos, prox)) {
            a.caminho.pop_back();
            a_subiu_ = prox.z != a.pos.z;
            mover_animal(a, prox);
            return 2;
        }
    }
    a.caminho.clear();
    const int metade = ficha(a).visao_lado / 2;
    const size_t total = static_cast<size_t>(largura()) * altura();
    if (bfs_pai_.size() != total) {
        bfs_pai_.assign(total, -1);
        bfs_marca_.assign(total, 0);
    }
    const unsigned vez = ++bfs_vez_;
    auto visitado = [&](Pos p) { return bfs_marca_[indice(p)] == vez; };
    std::vector<Pos> fila{a.pos};
    bfs_marca_[indice(a.pos)] = vez;
    bfs_pai_[indice(a.pos)] = -1;
    for (size_t k = 0; k < fila.size(); ++k) {
        const Pos cur = fila[k];
        for (Pos v : VIZINHOS) {
            Pos p = com_z({cur.x + v.x, cur.y + v.y});
            if (!dentro(p)) continue;
            if (!mapa_todo && (p.x - a.pos.x < -metade || p.x - a.pos.x >= metade ||
                               p.y - a.pos.y < -metade || p.y - a.pos.y >= metade))
                continue;
            if (visitado(p) || !pode_pisar_animal(a, cur, p)) continue;
            bfs_marca_[indice(p)] = vez;
            bfs_pai_[indice(p)] = static_cast<int>(k);
            fila.push_back(p);
            if (serve(p)) {
                // volta pelo caminho até o primeiro passo (no mapa todo,
                // guarda o caminho inteiro para os próximos passos)
                size_t j = fila.size() - 1;
                if (mapa_todo) a.caminho.push_back(fila[j]);
                while (bfs_pai_[indice(fila[j])] != 0) {
                    j = static_cast<size_t>(bfs_pai_[indice(fila[j])]);
                    if (mapa_todo) a.caminho.push_back(fila[j]);
                }
                if (mapa_todo) a.caminho.pop_back();  // o 1º passo é dado agora
                a_subiu_ = fila[j].z != a.pos.z;
                mover_animal(a, fila[j]);
                return 2;
            }
        }
    }
    return 0;
}

// Explorar (cada espécie com a sua distância: galinha de 1 a 7). Quem segue
// alguém (bando / mãe) só escolhe destinos perto dele.
bool Mundo::explorar_animal(Animal& a, bool livre) {
    const FichaEspecie& f = ficha(a);
    int lider = -1;
    if (livre) lider = -1;
    else if (a.fase == 0) lider = a.mae;
    else if (a.fase == 2 && a.sexo == Sexo::Mulher) lider = a.bando;
    if (lider >= 0 && animais_[lider].estado == EstadoAnimal::Morto) lider = -1;
    if (!a.tem_destino || distancia_passos(a.pos, a.destino) == 0) {
        std::uniform_int_distribution<int> dist(f.explorar_min, f.explorar_max);
        const int r = dist(rng_);
        std::vector<Pos> opcoes;
        auto testar = [&](int x, int y) {
            Pos p{x, y};
            if (!dentro(p) || !(mapa_.e_terra(x, y) || mapa_.e_gelo(x, y))) return;
            if (lider >= 0 && distancia_passos(p, animais_[lider].pos) > a.dist_bando) return;
            opcoes.push_back(com_z(p));
        };
        for (int dx = -r; dx <= r; ++dx) {
            testar(a.pos.x + dx, a.pos.y - r);
            testar(a.pos.x + dx, a.pos.y + r);
        }
        for (int dy = -r + 1; dy <= r - 1; ++dy) {
            testar(a.pos.x - r, a.pos.y + dy);
            testar(a.pos.x + r, a.pos.y + dy);
        }
        if (opcoes.empty()) { a.tem_destino = false; return false; }
        std::uniform_int_distribution<size_t> d(0, opcoes.size() - 1);
        a.destino = opcoes[d(rng_)];
        a.tem_destino = true;
    }
    a.estado = EstadoAnimal::Vagando;
    if (!passo_animal(a, a.destino)) { a.tem_destino = false; return false; }
    if (distancia_passos(a.pos, a.destino) == 0) a.tem_destino = false;
    return true;
}

// O galo vivo mais perto numa área de `lado` x `lado`.
int Mundo::achar_galo(const Animal& a, int lado) const {
    const int metade = lado / 2;
    int melhor = -1, melhor_d = 0;
    for (const Animal& g : animais_) {
        if (g.estado == EstadoAnimal::Morto || g.sexo != Sexo::Homem || g.fase < 2) continue;
        const int dx = g.pos.x - a.pos.x, dy = g.pos.y - a.pos.y;
        if (dx < -metade || dx >= metade || dy < -metade || dy >= metade) continue;
        const int d = distancia_passos(a.pos, g.pos);
        if (melhor < 0 || d < melhor_d) { melhor = g.id; melhor_d = d; }
    }
    return melhor;
}

// ===========================================================================
// Acontecimentos
// ===========================================================================

// A cada 10 min: fome −3, sede −4 (até 0). Não cai para o pintinho com a
// mãe, para a galinha chocando, dormindo, nem durante a ação (ciscar não
// baixa a fome; beber não baixa a sede).
void Mundo::evento_animal_nec(int id) {
    Animal& a = animais_[id];
    if (a.estado == EstadoAnimal::Morto) return;
    const FichaEspecie& f = ficha(a);
    a.quando_nec = tempo_ + f.fome_intervalo_s;
    agendar(a.quando_nec, A_NEC, id);
    if (sem_necessidades(a)) return;
    if (a.estado != EstadoAnimal::Comendo) a.fome = std::max(0.0, a.fome - f.fome_queda);
    if (a.estado != EstadoAnimal::Bebendo) a.sede = std::max(0.0, a.sede - f.sede_queda);
    recalcular_hp(a);
}

void Mundo::evento_animal_sem_hp(int id) {
    Animal& a = animais_[id];
    if (a.estado == EstadoAnimal::Morto) return;
    a.hp = 0.0;
    a.hp_t = tempo_;
    morrer_animal(a, (a.fome <= 0 ? 1 : 0) | (a.sede <= 0 ? 2 : 0));
}

// 60 dias: o pintinho larga a mãe; 90 dias: vira galinha ou galo (o galo
// começa um bando novo; a galinha entra no bando mais perto); 3 anos: morre.
void Mundo::evento_animal_cresce(int id) {
    Animal& a = animais_[id];
    if (a.estado == EstadoAnimal::Morto) return;
    const FichaEspecie& f = ficha(a);
    const double dias = idade_dias(a) + 1e-6;
    if (dias >= f.velhice_anos * 365.0) {
        morrer_animal(a, 8);
        return;
    }
    if (a.fase < 2 && dias >= f.adulto_dias) {
        atualizar_hp(a);
        a.fase = 2;
        a.hp = hp_max(a);  // adulto: HP de adulto (PROVISÓRIO: cheio)
        if (a.sexo == Sexo::Homem) a.bando = a.id;
        else a.bando = achar_galo(a, f.busca_macho_lado);
        recalcular_hp(a);
    } else if (a.fase == 0 && dias >= f.independente_dias) {
        a.fase = 1;
        a.fome = 100.0;  // (estava parada em 100 enquanto seguia a mãe)
        a.sede = 100.0;
        recalcular_hp(a);
    }
}

// A decisão de cada passo do animal. Ordem:
//   fim de ação -> choca -> dorme -> luta -> perigo -> pintinho segue a mãe
//   -> fome/sede -> ninho (pôr o ovo) -> namoro (galo) -> bando -> explorar.
void Mundo::evento_animal(int id) {
    Animal& a = animais_[id];
    if (a.estado == EstadoAnimal::Morto) return;
    const FichaEspecie& f = ficha(a);
    a_subiu_ = false;
    auto andou = [&]() { agendar_passo(a, a_subiu_ ? f.subir_s : f.passo_s); };

    // --- fim do que estava fazendo -----------------------------------------
    // Já comendo ou bebendo, continua até encher (100) — PROVISÓRIO; senão
    // voltaria a ter fome/sede 10 min depois e faria a viagem de novo.
    if (a.estado == EstadoAnimal::Comendo) {
        a.fome = std::min(100.0, a.fome + f.comer_ganho);
        recalcular_hp(a);
        if (a.fome < 100.0 && !dormindo_agora(a)) { agendar_passo(a, f.comer_s); return; }
        a.estado = EstadoAnimal::Vagando;
    } else if (a.estado == EstadoAnimal::Bebendo) {
        a.sede = std::min(100.0, a.sede + f.beber_ganho);
        if (std::find(a.aguas.begin(), a.aguas.end(), a.alvo) == a.aguas.end())
            a.aguas.push_back(a.alvo);
        recalcular_hp(a);
        if (a.sede < 100.0 && !dormindo_agora(a)) { agendar_passo(a, f.beber_s); return; }
        a.estado = EstadoAnimal::Vagando;
    } else if (a.estado == EstadoAnimal::Namorando) {
        if (tempo_ + EPS < a.namoro_fim) { agendar_passo(a, a.namoro_fim - tempo_); return; }
        if (a.sexo == Sexo::Mulher) {
            // Namorou 15 min: põe a ninhada inteira (12 a 14 ovos), 1 por dia,
            // de manhã (06:00–10:00), começando no dia seguinte.
            std::uniform_int_distribution<int> q(f.ovos_min, f.ovos_max);
            a.fertil = true;
            a.ovos_ninhada = q(rng_);
            a.ovos_postos = 0;
            a.falha = sortear(f.falha_min, f.falha_max);
            const Data d = data_de(tempo_);
            a.proxima_postura = tempo_de(d.ano, d.mes, d.dia) + DIA +
                                sortear(f.postura_ini_h, f.postura_fim_h) * 3600.0;
        }
        a.namoro_com = -1;
        a.estado = EstadoAnimal::Vagando;
    } else if (a.estado == EstadoAnimal::NoNinho) {
        if (tempo_ + EPS < a.sai_ninho) { agendar_passo(a, a.sai_ninho - tempo_); return; }
        a.estado = EstadoAnimal::Vagando;  // sai no próximo passo dado
    } else if (a.estado == EstadoAnimal::Chocando) {
        return;  // fica em cima dos ovos; a eclosão a acorda
    }

    // --- sono (19:00 – 05:00, onde estiver; o bando dorme junto) ----------
    if (dormindo_agora(a) && a.estado != EstadoAnimal::Lutando) {
        if (a.estado != EstadoAnimal::Dormindo) {
            a.estado = EstadoAnimal::Dormindo;
            recalcular_hp(a);
        }
        // Acorda às 05:00.
        double h = hora_do_dia(tempo_ + 1e-3);
        double falta = f.acorda_h - h;
        if (falta <= 0) falta += cfg::HORAS_POR_DIA;
        agendar_passo(a, std::max(falta * 3600.0, 1.0));
        return;
    }
    if (a.estado == EstadoAnimal::Dormindo) {
        a.estado = EstadoAnimal::Vagando;
        recalcular_hp(a);
    }

    // --- luta (revidando um NPC até um dos dois morrer) ---------------------
    if (a.estado == EstadoAnimal::Lutando) {
        const int k = a.lutando_com;
        const bool vale = k >= 0 && npcs_[k].estado != EstadoNpc::Morto &&
                          npcs_[k].dentro_abrigo < 0;
        if (vale) {
            if (distancia_passos(a.pos, npcs_[k].pos) <= 1) {
                const double dano = a.fase < 2 ? f.dano_filhote
                                  : a.sexo == Sexo::Homem ? f.dano_macho : f.dano_femea;
                npc_ferido(npcs_[k], dano, a.id);
                agendar_passo(a, cfg::ATAQUE_S);
                return;
            }
            passo_animal(a, npcs_[k].pos);
            andou();
            return;
        }
        a.lutando_com = -1;
        a.estado = EstadoAnimal::Vagando;
        // Defendia o ninho sem sair de cima dele: volta a chocar.
        if (a.eclosao > 0 && a.ninho == indice(a.pos)) {
            a.estado = EstadoAnimal::Chocando;
            recalcular_hp(a);
            return;
        }
    }

    // --- perigo: arisco, foge de NPC perto ---------------------------------
    if (perigo(a)) { andou(); return; }

    // --- pintinho (até 60 dias) segue a mãe ---------------------------------
    if (a.fase == 0 && a.mae >= 0 && animais_[a.mae].estado != EstadoAnimal::Morto) {
        const Animal& m = animais_[a.mae];
        if (distancia_passos(a.pos, m.pos) > a.dist_bando) {
            a.estado = EstadoAnimal::Seguindo;
            a.tem_destino = false;
            passo_animal(a, m.pos);
            andou();
            return;
        }
    }

    // Comer e beber vêm antes de pôr o ovo (primárias antes — PROVISÓRIO,
    // como nos NPCs).
    if (cuidar_necessidades(a)) return;
    // Com fome ou sede e sem achar grama/água à vista: explora LIVRE do bando
    // (senão ficaria preso a 3–5 células do galo) — PROVISÓRIO.
    const bool precisa = !sem_necessidades(a) &&
                         (a.fome < f.fome_limite || a.sede < f.sede_limite);
    if (precisa) {
        explorar_animal(a, true);
        andou();
        return;
    }
    if (cuidar_ninho(a)) return;
    if (namorar(a)) return;
    if (cuidar_bando(a)) { andou(); return; }
    explorar_animal(a);
    andou();
}

// Arisco (atacado há pouco) ou fugindo: se um NPC está perto (dentro da
// distância de fuga, 8 a 15), anda para longe dele.
bool Mundo::perigo(Animal& a) {
    const bool arisco = tempo_ < a.arisco_ate;
    if (!arisco && a.fugindo_de < 0) return false;
    int perto = -1, perto_d = 0;
    for (const Npc& n : npcs_) {
        if (n.estado == EstadoNpc::Morto || n.bebe || n.dentro_abrigo >= 0) continue;
        if (!arisco && n.id != a.fugindo_de) continue;
        const int d = distancia_passos(a.pos, n.pos);
        if (d <= a.dist_fuga && (perto < 0 || d < perto_d)) { perto = n.id; perto_d = d; }
    }
    if (perto < 0) {
        a.fugindo_de = -1;
        if (a.estado == EstadoAnimal::Fugindo) a.estado = EstadoAnimal::Vagando;
        return false;
    }
    if (a.estado == EstadoAnimal::NoNinho) a.estado = EstadoAnimal::Vagando;
    a.estado = EstadoAnimal::Fugindo;
    a.tem_destino = false;
    passo_animal(a, npcs_[perto].pos, true);
    return true;
}

// Fome (< 70): cisca a grama (10 s, +10) — só em grama, que por enquanto
// não acaba. Sede (< 50): bica a água ao lado (5 s, +5). A mais urgente
// primeiro (a que chega a 0 antes).
bool Mundo::cuidar_necessidades(Animal& a) {
    if (sem_necessidades(a)) return false;
    const FichaEspecie& f = ficha(a);
    const bool fome = a.fome < f.fome_limite, sede = a.sede < f.sede_limite;
    if (!fome && !sede) return false;
    const double t_fome = a.fome / (f.fome_queda / f.fome_intervalo_s);
    const double t_sede = a.sede / (f.sede_queda / f.sede_intervalo_s);
    const bool sede_primeiro = sede && (!fome || t_sede <= t_fome);
    auto e_grama = [&](Pos p) {
        return mapa_.material(p.x, p.y, p.z) == Material::Grama;
    };
    auto da_para_beber = [&](Pos q) {
        for (Pos v : VIZINHOS) {
            Pos w = com_z({q.x + v.x, q.y + v.y});
            if (!dentro(w) || mapa_.material(w.x, w.y, w.z) != Material::Agua) continue;
            const int desnivel = q.z - w.z;
            if (desnivel >= 0 && desnivel <= f.degrau_max) { a.alvo = w; return true; }
        }
        return false;
    };
    auto tentar_comer = [&]() {
        const int r = buscar_animal(a, e_grama);
        if (r == 1) {  // está na grama: cisca
            a.estado = EstadoAnimal::Comendo;
            agendar_passo(a, f.comer_s);
            return true;
        }
        if (r == 0) return false;
        a.estado = EstadoAnimal::IndoComer;
        agendar_passo(a, a_subiu_ ? f.subir_s : f.passo_s);
        return true;
    };
    auto tentar_beber = [&]() {
        const int r = buscar_animal(a, da_para_beber);
        if (r == 1) {  // água ao lado: bica
            da_para_beber(a.pos);
            a.estado = EstadoAnimal::Bebendo;
            agendar_passo(a, f.beber_s);
            return true;
        }
        if (r == 2) {
            a.estado = EstadoAnimal::IndoBeber;
            agendar_passo(a, a_subiu_ ? f.subir_s : f.passo_s);
            return true;
        }
        // Nada à vista: vai (pelo caminho, no mapa todo) a uma água lembrada.
        if (a.aguas.empty()) return false;
        auto lembrada = [&](Pos q) {
            for (Pos v : VIZINHOS) {
                Pos w = com_z({q.x + v.x, q.y + v.y});
                if (!dentro(w) || mapa_.material(w.x, w.y, w.z) != Material::Agua) continue;
                const int desnivel = q.z - w.z;
                if (desnivel < 0 || desnivel > f.degrau_max) continue;
                for (Pos m : a.aguas)
                    if (m.x == w.x && m.y == w.y) return true;
            }
            return false;
        };
        a_subiu_ = false;
        if (buscar_animal(a, lembrada, true) != 2) return false;
        a.estado = EstadoAnimal::IndoBeber;
        agendar_passo(a, a_subiu_ ? f.subir_s : f.passo_s);
        return true;
    };
    if (sede_primeiro) {
        if (tentar_beber()) return true;
        if (fome && tentar_comer()) return true;
    } else {
        if (tentar_comer()) return true;
        if (sede && tentar_beber()) return true;
    }
    return false;
}

// Galo: sem fome nem sede, procura uma galinha do bando dele que possa
// namorar (adulta, sem ninhada, sem pintinhos com menos de 60 dias); ao
// lado dela, os dois ficam juntos 15 min (❤️).
bool Mundo::namorar(Animal& a) {
    const FichaEspecie& f = ficha(a);
    if (a.sexo != Sexo::Homem || a.fase < 2) return false;
    if (a.fome < f.fome_limite || a.sede < f.sede_limite) return false;
    int melhor = -1, melhor_d = 0;
    for (const Animal& g : animais_) {
        if (g.sexo != Sexo::Mulher || g.fase < 2 || g.bando != a.id || g.fertil) continue;
        if (tempo_ < g.pode_por_em) continue;
        if (g.estado != EstadoAnimal::Vagando && g.estado != EstadoAnimal::Seguindo &&
            g.estado != EstadoAnimal::IndoComer && g.estado != EstadoAnimal::IndoBeber)
            continue;
        if (g.fome < f.fome_limite || g.sede < f.sede_limite) continue;
        const int d = distancia_passos(a.pos, g.pos);
        if (melhor < 0 || d < melhor_d) { melhor = g.id; melhor_d = d; }
    }
    if (melhor < 0) return false;
    Animal& g = animais_[melhor];
    if (melhor_d <= 1) {
        a.estado = g.estado = EstadoAnimal::Namorando;
        a.namoro_com = g.id;
        g.namoro_com = a.id;
        a.namoro_fim = g.namoro_fim = tempo_ + f.namoro_min * cfg::SEGUNDOS_POR_MINUTO;
        agendar_passo(a, a.namoro_fim - tempo_);
        agendar_passo(g, g.namoro_fim - tempo_);
        return true;
    }
    a.estado = EstadoAnimal::Seguindo;
    a.tem_destino = false;
    a_subiu_ = false;
    if (!passo_animal(a, g.pos)) return false;
    agendar_passo(a, a_subiu_ ? f.subir_s : f.passo_s);
    return true;
}

// Galinha fértil: todo dia, numa hora sorteada da manhã (06:00–10:00), vai
// ao ninho (o lugar do 1º ovo), põe 1 ovo e fica 1 h. Depois do último ovo
// (12 a 14), fica chocando 21 dias (sem fome nem sede).
bool Mundo::cuidar_ninho(Animal& a) {
    if (!a.fertil || a.proxima_postura < 0 || tempo_ + EPS < a.proxima_postura) return false;
    const FichaEspecie& f = ficha(a);
    // Passou da manhã (10:00) sem conseguir pôr: fica para a manhã seguinte
    // — PROVISÓRIO.
    if (hora_do_dia(tempo_) >= f.postura_fim_h) {
        const Data d = data_de(tempo_);
        a.proxima_postura = tempo_de(d.ano, d.mes, d.dia) + DIA +
                            sortear(f.postura_ini_h, f.postura_fim_h) * 3600.0;
        return false;
    }
    if (a.ninho < 0) {
        // O 1º ovo: aqui mesmo, se for terra (no gelo, não) — senão anda.
        if (!mapa_.e_terra(a.pos.x, a.pos.y)) return false;
        a.ninho = indice(a.pos);
        PilhaOvos p;
        p.ninho_de = a.id;
        p.com_galinha = true;
        ovos_[a.ninho] = p;
    }
    if (indice(a.pos) != a.ninho) {
        a.estado = EstadoAnimal::IndoNinho;
        a_subiu_ = false;
        const Pos alvo = com_z({a.ninho % largura(), a.ninho / largura()});
        // À vista: pelo caminho; longe: em linha reta.
        const int ninho = a.ninho;
        int r = buscar_animal(a, [&](Pos p) { return indice(p) == ninho; });
        if (r == 0 && !passo_animal(a, alvo)) return false;  // não chega: tenta depois
        agendar_passo(a, a_subiu_ ? f.subir_s : f.passo_s);
        return true;
    }
    PilhaOvos& p = ovos_[a.ninho];
    p.ovos += 1;
    p.ninho_de = a.id;
    p.com_galinha = true;
    a.ovos_postos += 1;
    if (a.ovos_postos >= a.ovos_ninhada) {
        a.estado = EstadoAnimal::Chocando;
        a.proxima_postura = -1.0;
        a.eclosao = tempo_ + f.chocar_dias * DIA;
        agendar(a.eclosao, A_ECLOSAO, a.id);
        recalcular_hp(a);
        return true;
    }
    a.estado = EstadoAnimal::NoNinho;
    a.sai_ninho = tempo_ + f.ninho_fica_h * 3600.0;
    const Data d = data_de(tempo_);
    a.proxima_postura = tempo_de(d.ano, d.mes, d.dia) + DIA +
                        sortear(f.postura_ini_h, f.postura_fim_h) * 3600.0;
    agendar_passo(a, a.sai_ninho - tempo_);
    return true;
}

// Bando: a galinha segue o galo dela (fica a 3–5 células). Sem bando (ou o
// galo morreu), procura um galo numa área de 64 x 64 e entra no bando dele.
bool Mundo::cuidar_bando(Animal& a) {
    const FichaEspecie& f = ficha(a);
    if (a.fase < 2 || a.sexo == Sexo::Homem) return false;
    if (a.bando >= 0 && animais_[a.bando].estado == EstadoAnimal::Morto) a.bando = -1;
    if (a.bando < 0) a.bando = achar_galo(a, f.busca_macho_lado);
    if (a.bando < 0) return false;  // sozinha: explora
    const Animal& g = animais_[a.bando];
    if (distancia_passos(a.pos, g.pos) <= a.dist_bando) return false;
    a.estado = EstadoAnimal::Seguindo;
    a.tem_destino = false;
    return passo_animal(a, g.pos);
}

// 21 dias chocando: nascem os pintinhos em volta dela. Cada ovo tem a chance
// sorteada da ninhada (5–20%) de não prestar: esse some. Galo ou galinha
// meio a meio. Ela só volta a pôr 60 dias depois.
void Mundo::evento_eclosao(int id) {
    Animal& a = animais_[id];
    if (a.estado == EstadoAnimal::Morto || a.ninho < 0) return;
    const FichaEspecie& f = ficha(a);
    auto it = ovos_.find(a.ninho);
    const int ovos = it == ovos_.end() ? 0 : it->second.ovos;
    if (it != ovos_.end()) ovos_.erase(it);
    std::bernoulli_distribution falha(a.falha), moeda(0.5);
    for (int k = 0; k < ovos; ++k) {
        if (falha(rng_)) continue;
        Pos q;
        if (!lugar_livre_perto(a.pos, q)) break;
        const Sexo s = moeda(rng_) ? Sexo::Homem : Sexo::Mulher;
        novo_animal(s, 0.0, com_z(q), a.id, a.bando);
    }
    Animal& m = animais_[id];  // (a lista pode ter crescido)
    m.estado = EstadoAnimal::Vagando;
    m.fertil = false;
    m.ovos_postos = 0;
    m.ninho = -1;
    m.eclosao = -1.0;
    m.pode_por_em = tempo_ + f.volta_a_por_dias * DIA;
    recalcular_hp(m);
    agendar_passo(m, f.passo_s);
}

// Ovos sem contato por 1 dia: estragam (somem).
void Mundo::evento_ovos_estragam(int celula) {
    auto it = ovos_.find(celula);
    const int dona = it->second.ninho_de;
    ovos_.erase(it);
    if (ocupacao_[celula] == Ocupante::Ovo) ocupar(celula, Ocupante::Nada);
    if (dona >= 0 && animais_[dona].ninho == celula) {
        Animal& a = animais_[dona];  // perdeu a ninhada: volta ao normal
        a.ninho = -1;
        a.fertil = false;
        a.ovos_postos = 0;
        a.proxima_postura = -1.0;
    }
}

// Morre: o corpo fica 3 dias e some. Morto por NPC (causa 16) vira carne.
void Mundo::morrer_animal(Animal& a, int causa) {
    atualizar_hp(a);
    if (a.ninho >= 0) {
        sair_do_ninho(a);
        auto it = ovos_.find(a.ninho);
        if (it != ovos_.end()) it->second.ninho_de = -1;
        a.ninho = -1;
    }
    if (a.namoro_com >= 0) {
        Animal& p = animais_[a.namoro_com];
        p.namoro_com = -1;
        if (p.estado == EstadoAnimal::Namorando) {
            p.estado = EstadoAnimal::Vagando;
            agendar_passo(p, ficha(p).passo_s);
        }
    }
    const int tipo = a.fase < 2 ? 0 : a.sexo == Sexo::Mulher ? 1 : 2;
    a.estado = EstadoAnimal::Morto;
    a.causa_morte = causa;
    a.morreu_em = tempo_;
    a.hp = 0.0;
    a.taxa_hp = 0.0;
    a.quando_passo = a.quando_nec = a.quando_sem_hp = -1.0;
    // O corpo fica na célula dele; se ali há ovos, na célula livre mais perto.
    int cel = indice(a.pos);
    ocupar(cel, ovos_.count(cel) ? Ocupante::Ovo : Ocupante::Nada);
    if (ovos_.count(cel)) {
        Pos q;
        if (!lugar_livre_perto(a.pos, q)) return;
        cel = indice(q);
        a.pos = com_z(q);
    }
    if (causa == 16) {
        por_carcaca(cel, tipo);
    } else {
        ocupar(cel, Ocupante::CorpoAnimal);
        a.corpo = cel;
        agendar(tempo_ + ficha(a).corpo_some_dias * DIA, CORPO_SOME, a.id);
    }
}

// ===========================================================================
// Ovos e carne no chão
// ===========================================================================

// Lugar para ovos soltos: célula vazia ou pilha de ovos solta (não ninho)
// com menos de 32 — PROVISÓRIO (o mesmo limite das maçãs).
int Mundo::lugar_para_ovo(Pos centro) {
    for (int r = 1; r < std::max(largura(), altura()); ++r) {
        std::vector<int> opcoes;
        auto testar = [&](int x, int y) {
            Pos p{x, y};
            if (!dentro(p)) return;
            const int i = indice(p);
            if (livre(p)) { opcoes.push_back(i); return; }
            auto it = ovos_.find(i);
            if (ocupacao_[i] == Ocupante::Ovo && it != ovos_.end() &&
                it->second.ninho_de < 0 && it->second.ovos < cfg::MACAS_POR_PILHA)
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
    return -1;
}

// Ovos postos no chão por um NPC: o contato é agora (estragam em 1 dia).
void Mundo::por_ovos(int celula, int quantos) {
    PilhaOvos& p = ovos_[celula];
    p.ovos += quantos;
    p.com_galinha = false;
    p.contato = tempo_;
    p.estraga_em = tempo_ + cfg::GALINHA.ovo_estraga_dias * DIA;
    agendar(p.estraga_em, OVOS_ESTRAGAM, celula);
    if (ocupacao_[celula] == Ocupante::Nada) ocupar(celula, Ocupante::Ovo);
}

// Um NPC tira ovos da pilha. A galinha chocando em cima defende (80%): ela
// revida até um dos dois morrer. Os que ficam têm contato com o NPC agora.
int Mundo::tirar_ovos(int celula, int quantos, int npc) {
    auto it = ovos_.find(celula);
    if (it == ovos_.end()) return 0;
    PilhaOvos& p = it->second;
    const int tira = std::min(quantos, p.ovos);
    p.ovos -= tira;
    const int dona = p.ninho_de;
    if (dona >= 0 && animais_[dona].estado == EstadoAnimal::Chocando && npc >= 0) {
        Animal& g = animais_[dona];
        std::bernoulli_distribution defende(ficha(g).revida_mae);
        if (defende(rng_)) {
            g.estado = EstadoAnimal::Lutando;
            g.lutando_com = npc;
            agendar_passo(g, cfg::ATAQUE_S);
        }
    }
    if (p.ovos <= 0) {
        ovos_.erase(it);
        if (ocupacao_[celula] == Ocupante::Ovo) ocupar(celula, Ocupante::Nada);
        if (dona >= 0 && animais_[dona].ninho == celula) {
            Animal& g = animais_[dona];  // levaram todos: a ninhada acabou
            g.ninho = -1;
            g.fertil = false;
            g.ovos_postos = 0;
            g.eclosao = -1.0;
            g.proxima_postura = -1.0;
            if (g.estado == EstadoAnimal::Chocando) {
                g.estado = EstadoAnimal::Vagando;
                recalcular_hp(g);
                agendar_passo(g, ficha(g).passo_s);
            }
        }
    } else if (!p.com_galinha) {
        p.contato = tempo_;
        p.estraga_em = tempo_ + cfg::GALINHA.ovo_estraga_dias * DIA;
        agendar(p.estraga_em, OVOS_ESTRAGAM, celula);
    }
    return tira;
}

void Mundo::por_carcaca(int celula, int tipo) {
    Carcaca c;
    c.celula = celula;
    c.tipo = tipo;
    c.estraga = tempo_ + cfg::GALINHA.carne_estraga_dias * DIA;
    const int id = proxima_carcaca_++;
    carcacas_[id] = c;
    ocupar(celula, Ocupante::Carne);
    agendar(c.estraga, CARNE_ESTRAGA, id);
}

int Mundo::carcaca_na_celula(int celula) const {
    for (const auto& [id, c] : carcacas_)
        if (c.celula == celula) return c.tipo;
    return -1;
}

int Mundo::tirar_carcaca(int celula) {
    for (auto it = carcacas_.begin(); it != carcacas_.end(); ++it)
        if (it->second.celula == celula) {
            const int tipo = it->second.tipo;
            carcacas_.erase(it);
            ocupar(celula, Ocupante::Nada);
            return 2 + tipo;
        }
    return 2;
}

// ===========================================================================
// Caça e luta
// ===========================================================================

// Um animal levou um golpe de um NPC (e não morreu).
//   - Já está lutando: continua até o fim.
//   - Senão, sorteia se revida (galo 20%; galinha com pintinhos de até 60
//     dias, 80%): revida até um dos dois morrer.
//   - Senão, foge; o bando (e os pintinhos) percebe e foge também: todos
//     ficam ariscos (com todos os NPCs) por 2 semanas a 3 anos.
void Mundo::animal_atacado(Animal& a, int npc) {
    const FichaEspecie& f = ficha(a);
    if (a.estado == EstadoAnimal::Lutando) return;
    double chance = 0.0;
    if (a.fase == 2 && a.sexo == Sexo::Homem) chance = f.revida_macho;
    if (a.fase == 2 && a.sexo == Sexo::Mulher) {
        for (const Animal& c : animais_)
            if (c.mae == a.id && c.fase == 0 && c.estado != EstadoAnimal::Morto) {
                chance = f.revida_mae;
                break;
            }
        if (a.estado == EstadoAnimal::Chocando) chance = f.revida_mae;
    }
    std::bernoulli_distribution revida(chance);
    if (chance > 0.0 && revida(rng_)) {
        if (a.estado == EstadoAnimal::Namorando && a.namoro_com >= 0) {
            animais_[a.namoro_com].estado = EstadoAnimal::Vagando;
            animais_[a.namoro_com].namoro_com = -1;
            agendar_passo(animais_[a.namoro_com], f.passo_s);
            a.namoro_com = -1;
        }
        a.estado = EstadoAnimal::Lutando;
        a.lutando_com = npc;
        agendar_passo(a, cfg::ATAQUE_S);
        return;
    }
    // Foge, e o bando junto.
    const int bando = a.fase == 2 ? a.bando : (a.mae >= 0 ? animais_[a.mae].bando : -1);
    for (Animal& b : animais_) {
        if (b.estado == EstadoAnimal::Morto || b.estado == EstadoAnimal::Lutando) continue;
        const int bb = b.fase == 2 ? b.bando : (b.mae >= 0 ? animais_[b.mae].bando : -1);
        const bool junto = b.id == a.id || (bando >= 0 && bb == bando) || b.mae == a.id;
        if (junto) deixar_arisco(b, npc);
    }
}

void Mundo::deixar_arisco(Animal& b, int npc) {
    const FichaEspecie& f = ficha(b);
    b.arisco_ate = std::max(b.arisco_ate, tempo_ + sortear(f.arisco_min_s, f.arisco_max_s));
    b.fugindo_de = npc;
    if (b.estado == EstadoAnimal::Namorando && b.namoro_com >= 0) {
        Animal& p = animais_[b.namoro_com];
        p.namoro_com = -1;
        if (p.estado == EstadoAnimal::Namorando) p.estado = EstadoAnimal::Vagando;
        b.namoro_com = -1;
    }
    if (b.estado == EstadoAnimal::Dormindo) recalcular_hp(b);
    const bool chocava = b.estado == EstadoAnimal::Chocando;
    b.estado = EstadoAnimal::Fugindo;
    if (chocava) recalcular_hp(b);
    agendar_passo(b, 0.0);  // reage já
}

void Mundo::npc_ferido(Npc& n, double dano, int /*animal*/) {
    n.hp -= dano;
    if (n.hp <= EPS) {
        n.hp = 0.0;
        morrer(n, 16);
    }
}

// Algum animal está revidando contra este NPC? (o mais perto)
int Mundo::atacado_por(const Npc& n) const {
    int melhor = -1, melhor_d = 0;
    for (const Animal& a : animais_) {
        if (a.estado != EstadoAnimal::Lutando || a.lutando_com != n.id) continue;
        const int d = distancia_passos(n.pos, a.pos);
        if (melhor < 0 || d < melhor_d) { melhor = a.id; melhor_d = d; }
    }
    return melhor;
}

// O NPC bate em quem está revidando contra ele (PROVISÓRIO: ele se defende).
bool Mundo::lutar(Npc& n) {
    if (n.dentro_abrigo >= 0) return false;
    const int k = atacado_por(n);
    if (k < 0) return false;
    Animal& a = animais_[k];
    n.estado = EstadoNpc::Lutando;
    n.presa = k;
    n.tem_alvo = true;
    n.alvo = a.pos;
    if (distancia_passos(n.pos, a.pos) <= 1) {
        atualizar_hp(a);
        a.hp -= cfg::NPC_DANO;
        if (a.hp <= EPS) {
            morrer_animal(a, 16);
            n.presa = -1;
            n.estado = EstadoNpc::Vagando;
            registrar(TipoRegistro::AnimalMorto, n.id, static_cast<int>(a.sexo), a.fase, a.pos);
        } else {
            recalcular_hp(a);
        }
        return true;
    }
    passo_em_direcao(n, a.pos);
    return true;
}

// Caça (com fome e sem nenhuma outra comida): vai atrás do animal vivo mais
// perto à vista; ao lado, ataca (10 de dano, 1 ataque por passo = 5 s).
bool Mundo::tentar_cacar(Npc& n) {
    const int metade = cfg::VISAO_LADO / 2;
    auto a_vista = [&](const Animal& a) {
        const int dx = a.pos.x - n.pos.x, dy = a.pos.y - n.pos.y;
        return a.estado != EstadoAnimal::Morto && dx >= -metade && dx < metade &&
               dy >= -metade && dy < metade;
    };
    int alvo = (n.presa >= 0 && a_vista(animais_[n.presa])) ? n.presa : -1;
    if (alvo < 0) {
        int melhor_d = 0;
        for (const Animal& a : animais_) {
            if (!a_vista(a)) continue;
            const int d = distancia_passos(n.pos, a.pos);
            if (alvo < 0 || d < melhor_d) { alvo = a.id; melhor_d = d; }
        }
    }
    if (alvo < 0) { n.presa = -1; return false; }
    Animal& a = animais_[alvo];
    n.presa = alvo;
    n.tem_alvo = true;
    n.alvo = a.pos;
    n.pela_memoria = false;
    if (distancia_passos(n.pos, a.pos) <= 1) {
        n.estado = EstadoNpc::Lutando;
        atualizar_hp(a);
        a.hp -= cfg::NPC_DANO;
        if (a.hp <= EPS) {
            morrer_animal(a, 16);
            n.presa = -1;
            registrar(TipoRegistro::AnimalMorto, n.id, static_cast<int>(a.sexo), a.fase, a.pos);
        } else {
            recalcular_hp(a);
            animal_atacado(a, n.id);
        }
        return true;
    }
    n.estado = EstadoNpc::Cacando;
    passo_em_direcao(n, a.pos);
    return true;
}

// ===========================================================================
// Leitura (para a interface) e ganchos de teste
// ===========================================================================

std::vector<std::array<int, 4>> Mundo::ovos() const {
    std::vector<std::array<int, 4>> r;
    for (const auto& [cel, p] : ovos_)
        r.push_back({cel % largura(), cel / largura(), p.ovos, p.ninho_de});
    return r;
}

std::vector<std::array<int, 3>> Mundo::carnes() const {
    std::vector<std::array<int, 3>> r;
    for (const auto& [id, c] : carcacas_)
        r.push_back({c.celula % largura(), c.celula / largura(), c.tipo});
    return r;
}

int Mundo::ovos_em(int x, int y) const {
    auto it = ovos_.find(indice({x, y}));
    return it == ovos_.end() ? 0 : it->second.ovos;
}

double Mundo::ovos_estragam_em(int x, int y) const {
    auto it = ovos_.find(indice({x, y}));
    if (it == ovos_.end() || it->second.com_galinha) return -1.0;
    return it->second.estraga_em - tempo_;
}

std::vector<Pos> Mundo::coracoes() const {
    std::vector<Pos> r;
    for (const Animal& a : animais_)
        if (a.estado == EstadoAnimal::Namorando) r.push_back(a.pos);
    for (const Npc& m : npcs_)
        if (m.procriando_desde >= 0.0 && m.dentro_abrigo >= 0)
            r.push_back(abrigos_[m.dentro_abrigo].pos);
    return r;
}

void Mundo::teste_animal(int id, double fome, double sede, double hp) {
    Animal& a = animais_[id];
    atualizar_hp(a);
    a.fome = fome;
    a.sede = sede;
    a.hp = hp;
    recalcular_hp(a);
}

void Mundo::teste_idade_animal(int id, double dias) {
    Animal& a = animais_[id];
    a.nascimento = tempo_ - dias * DIA;
    const FichaEspecie& f = ficha(a);
    for (double d : {f.independente_dias, f.adulto_dias, f.velhice_anos * 365.0}) {
        const double t = a.nascimento + d * DIA;
        if (t > tempo_) agendar(t, A_CRESCE, a.id);
    }
}

void Mundo::teste_mover_animal(int id, int x, int y) {
    Animal& a = animais_[id];
    Pos p{x, y};
    if (!livre(p)) return;
    mover_animal(a, p);
    a.tem_destino = false;
}

}  // namespace genesis
