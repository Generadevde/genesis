// ---------------------------------------------------------------------------
// config.hpp — TODOS os números da simulação ficam aqui, num lugar só.
//
// Regra do projeto: nenhum número é inventado. Cada valor abaixo foi dado
// pelo dono do projeto em 14/11/2026 (ver docs/ILHA_V1.md). Para mudar
// um comportamento, muda-se aqui — o resto do código só lê estes nomes.
// ---------------------------------------------------------------------------
#pragma once

namespace genesis::config {

// --- Mapa -----------------------------------------------------------------
// Mapa quadrado de 512 x 512 células.
inline constexpr int LARGURA = 512;
inline constexpr int ALTURA  = 512;
// Água: 48 células a partir de cada borda. Depois começa a terra (grama).
inline constexpr int BORDA_AGUA = 48;

// --- Quantidades iniciais -------------------------------------------------
inline constexpr int NUM_NPCS      = 4;
inline constexpr int NUM_MACIEIRAS = 6;

// --- Fome -----------------------------------------------------------------
// Vai de 0 (faminto) a 100 (satisfeito). Em 0 o NPC morre.
inline constexpr double FOME_MAXIMA   = 100.0;
inline constexpr double FOME_MINIMA   = 0.0;
inline constexpr double FOME_INICIAL  = 100.0;
// A cada 5 segundos, a fome abaixa 3.
inline constexpr double FOME_INTERVALO_S = 5.0;
inline constexpr double FOME_QUEDA       = 3.0;
// O NPC só come quando a fome está abaixo de 70.
inline constexpr double FOME_LIMITE_COMER = 70.0;

// --- Maçãs e macieiras ----------------------------------------------------
// Cada maçã recupera 12 de fome.
inline constexpr double MACA_RECUPERA = 12.0;
// Cada macieira derruba 1 maçã a cada 30 a 60 s (sorteado de novo a cada queda).
inline constexpr double QUEDA_MIN_S = 30.0;
inline constexpr double QUEDA_MAX_S = 60.0;
// Comer uma maçã leva 10 s.
inline constexpr double COMER_DURACAO_S = 10.0;

// --- NPC ------------------------------------------------------------------
// Anda 1 célula por segundo -> 1 passo a cada 1 s.
inline constexpr double NPC_PASSO_S = 1.0;
// Enxerga maçãs numa área de 32 x 32 células em volta dele.
inline constexpr int VISAO_LADO = 32;

}  // namespace genesis::config
