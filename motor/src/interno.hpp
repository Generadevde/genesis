// ---------------------------------------------------------------------------
// interno.hpp — coisas usadas por mundo.cpp e fauna.cpp (não é para fora do
// motor): os tipos de acontecimento da fila e alguns ajudantes pequenos.
// ---------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "mundo.hpp"

namespace genesis::interno {

// Tipos de acontecimento (na fila).
enum TipoEvento : int {
    QUEDA_MACA = 0,       // alvo = número da macieira
    MACIEIRA_SECA = 1,    // alvo = número da macieira
    MACIEIRA_SOME = 2,    // alvo = número da macieira
    MACA_VENCE = 3,       // alvo = número da maçã no chão (6 h)
    MUDA_VIRA_ARVORE = 4, // alvo = id da muda
    VIRA_ADULTO = 5,      // alvo = id do bebê
    VELHICE = 6,          // alvo = id do NPC
    NASCIMENTO = 7,       // alvo = id da mãe
    CONCEPCAO = 8,        // alvo = id da mãe (1 h juntos no abrigo)
    ESTACAO = 9,          // alvo = estação que começa (0 verão ... 3 primavera)
    // Fauna
    A_PASSO = 10,         // alvo = id do animal (próxima decisão)
    A_NEC = 11,           // alvo = id do animal (fome e sede caem)
    A_SEM_HP = 12,        // alvo = id do animal (HP chegou a 0)
    A_CRESCE = 13,        // alvo = id (60 dias, 90 dias, 3 anos)
    A_POSTURA = 14,       // alvo = id da galinha (hora de pôr o ovo)
    A_ECLOSAO = 15,       // alvo = id da galinha (21 dias chocando)
    OVOS_ESTRAGAM = 16,   // alvo = célula da pilha de ovos
    CARNE_ESTRAGA = 17,   // alvo = id da carcaça
    CORPO_SOME = 18       // alvo = id do animal (corpo some em 3 dias)
};

// Margem de arredondamento para comparar tempos em ponto flutuante.
// Não é regra do jogo: só evita que 0.9999999 deixe de contar como "venceu".
constexpr double EPS = 1e-9;

// Distância "de passos" entre duas células quando se anda em 8 direções:
// a maior diferença entre colunas ou linhas (distância de Chebyshev).
inline int distancia_passos(Pos a, Pos b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

// Distância ao quadrado (em linha reta). Usada só para desempatar.
inline int distancia2(Pos a, Pos b) {
    const int dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// As 8 células vizinhas (inclui diagonais).
inline constexpr Pos VIZINHOS[8] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0},
                                    {1, 0},   {-1, 1}, {0, 1},  {1, 1}};

}  // namespace genesis::interno
