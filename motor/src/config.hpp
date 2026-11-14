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
// Mapa quadrado de 128 x 128 células (era 512 x 512; mudado em 14/11/2026).
inline constexpr int LARGURA = 128;
inline constexpr int ALTURA  = 128;
// Água: 8 células a partir de cada borda (era 48). Depois começa a terra.
inline constexpr int BORDA_AGUA = 8;

// --- Tempo do jogo (pedido em 14/11/2026) --------------------------------
// 1 hora do jogo = 60 s da simulação (valor "por enquanto" do dono).
// Logo, 1 minuto do jogo = 1 s. O dia tem 24 horas.
inline constexpr double SEGUNDOS_POR_HORA = 60.0;
inline constexpr int HORAS_POR_DIA = 24;
inline constexpr double SEGUNDOS_POR_DIA = SEGUNDOS_POR_HORA * HORAS_POR_DIA;
// O jogo começa no Dia 1, a esta hora (PROVISÓRIO).
inline constexpr int HORA_INICIAL = 0;

// --- Mundo em 3 dimensões (pedido em 14/11/2026) -------------------------
// Níveis z de -64 a +64; o nível 0 é o nível do mar.
inline constexpr int Z_MIN = -64;
inline constexpr int Z_MAX = 64;
inline constexpr int NIVEL_DO_MAR = 0;
// Superfície da terra (pedido em 14/11/2026): planícies, morros e vales.
//   - 30% da terra é vale, 30% é morro, o resto (40%) é planície;
//   - planície no +1 (PROVISÓRIO: era o chão mais baixo antes dos vales);
//   - morros sobem até o +4; vales descem até o -4.
// O topo é grama; abaixo, terra até o -3; dali para baixo, pedra.
// Se o fundo de um vale fica abaixo do -3 (no -4), o topo é pedra exposta.
inline constexpr int PLANICIE_Z = 1;            // PROVISÓRIO
inline constexpr int SUPERFICIE_MAX = 4;        // topo dos morros mais altos
inline constexpr int VALE_MIN_Z = -4;           // fundo dos vales mais fundos
inline constexpr double FRACAO_VALE = 0.30;
inline constexpr double FRACAO_MORRO = 0.30;
inline constexpr int TERRA_ATE_Z = -3;
// Topo da superfície em +4 (o máximo) vira neve no lugar da grama.
inline constexpr int NEVE_A_PARTIR_Z = 4;
// Mar (borda de BORDA_AGUA células): água do nível 0 até o -10; abaixo, pedra.
inline constexpr int MAR_FUNDO_Z = -10;

// Relevo "médio" (pedido do dono). Mais pra frente vira opção editável.
// PROVISÓRIO: tamanho médio de um morro, em células, e quantas camadas de
// detalhe se somam (cada uma com metade do tamanho e da força da anterior).
inline constexpr double RELEVO_TAMANHO_MORRO = 24.0;  // PROVISÓRIO
inline constexpr int RELEVO_CAMADAS = 3;               // PROVISÓRIO

// --- Água (física) ----------------------------------------------------------
// Cada célula de água tem nível de 1 a 10 (10% cada; 10 = cheia).
inline constexpr int AGUA_NIVEL_MAX = 10;
// A água se move a cada 3 s (valor de teste do dono do projeto).
inline constexpr double AGUA_INTERVALO_S = 3.0;

// --- Quantidades iniciais -------------------------------------------------
inline constexpr int NUM_NPCS      = 4;
// Nomes dos NPCs, na ordem (pedido em 14/11/2026; "por enquanto").
inline constexpr const char* NOMES_NPC[NUM_NPCS] = {"Jhon", "Elton", "Rose",
                                                    "Sycilia"};
// Quantas maçãs um NPC consegue carregar.
inline constexpr int MACAS_CARREGAVEIS = 1;
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

// --- Sede (pedida em 14/11/2026) -----------------------------------------
// Igual à fome: vai de 0 a 100, começa em 100, em 0 o NPC morre.
inline constexpr double SEDE_MAXIMA  = 100.0;
inline constexpr double SEDE_MINIMA  = 0.0;
inline constexpr double SEDE_INICIAL = 100.0;
// A cada 5 segundos, a sede abaixa 4.
inline constexpr double SEDE_INTERVALO_S = 5.0;
inline constexpr double SEDE_QUEDA       = 4.0;
// Abaixo de 50 o NPC tenta beber água (do mar mesmo).
inline constexpr double SEDE_LIMITE_BEBER = 50.0;
// Um gole recupera 15 e demora 10 s.
inline constexpr double GOLE_RECUPERA   = 15.0;
inline constexpr double BEBER_DURACAO_S = 10.0;

// --- Maçãs e macieiras ----------------------------------------------------
// Cada maçã recupera 12 de fome.
inline constexpr double MACA_RECUPERA = 12.0;
// ...e também 5 de sede (pedido em 14/11/2026).
inline constexpr double MACA_RECUPERA_SEDE = 5.0;
// Cada macieira derruba 1 maçã a cada 30 a 60 s (sorteado de novo a cada queda).
inline constexpr double QUEDA_MIN_S = 30.0;
inline constexpr double QUEDA_MAX_S = 60.0;
// --- Vida vegetal (pedida em 14/11/2026) -----------------------------------
// Maçã no chão que ninguém pega em 6 horas vira muda.
inline constexpr double MACA_VIRA_MUDA_H = 6.0;
// A muda vira macieira depois de 3 dias.
inline constexpr double MUDA_VIRA_ARVORE_DIAS = 3.0;
// A macieira vive 7 dias; depois fica seca (cinza, sem maçãs) por 4 horas
// e some.
inline constexpr double MACIEIRA_VIDA_DIAS = 7.0;
inline constexpr double MACIEIRA_SECA_H = 4.0;

// Comer uma maçã leva 10 s.
inline constexpr double COMER_DURACAO_S = 10.0;

// --- NPC ------------------------------------------------------------------
// Anda 1 célula por segundo -> 1 passo a cada 1 s.
inline constexpr double NPC_PASSO_S = 1.0;
// Subir ou descer 1 nível leva 5 s (em vez de 1 s). Mais de 1 nível, não sobe.
inline constexpr double NPC_SUBIR_S = 5.0;
inline constexpr int NPC_DEGRAU_MAX = 1;
// Enxerga maçãs e água numa área de 32 x 32 células em volta dele.
inline constexpr int VISAO_LADO = 32;

}  // namespace genesis::config
