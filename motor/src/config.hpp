// ---------------------------------------------------------------------------
// config.hpp — TODOS os números da simulação ficam aqui, num lugar só.
//
// Regra do projeto: nenhum número é inventado. Cada valor abaixo foi dado
// pelo dono do projeto (14 e 15/11/2026); os que eu escolhi estão marcados
// PROVISÓRIO. Para mudar
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

// --- Tempo e calendário (pedido em 15/11/2026; substitui o de 14/11) -----
// 1 segundo do jogo = 1 segundo real (na velocidade normal).
// 1 minuto = 60 s; 1 hora = 60 min; 1 dia = 24 h.
inline constexpr double SEGUNDOS_POR_MINUTO = 60.0;
inline constexpr double SEGUNDOS_POR_HORA = 60.0 * SEGUNDOS_POR_MINUTO;
inline constexpr int HORAS_POR_DIA = 24;
inline constexpr double SEGUNDOS_POR_DIA = SEGUNDOS_POR_HORA * HORAS_POR_DIA;
// Calendário: 12 meses com os dias do calendário de verdade (ano de 365
// dias, sem ano bissexto). A semana tem 7 dias (só para referência).
inline constexpr int MESES_POR_ANO = 12;
inline constexpr int DIAS_DO_MES[MESES_POR_ANO] = {31, 28, 31, 30, 31, 30,
                                                   31, 31, 30, 31, 30, 31};
inline constexpr int DIAS_POR_ANO = 365;
inline constexpr int DIAS_POR_SEMANA = 7;
inline constexpr double SEGUNDOS_POR_ANO = SEGUNDOS_POR_DIA * DIAS_POR_ANO;
// A simulação começa em 01/01/0001, às 07:00.
inline constexpr int ANO_INICIAL = 1;
inline constexpr int HORA_INICIAL = 7;

// --- Estações (cada uma com 3 meses; meses contados de 1 a 12) ------------
//   verão: 12, 1, 2      (formação de frutos)  dias mais longos
//   outono: 3, 4, 5      (folhas secas)        dia = noite
//   inverno: 6, 7, 8     (repouso e frio)      dias mais curtos
//   primavera: 9, 10, 11 (florescimento)       dia = noite
// O começo de cada estação (mês em que começa):
inline constexpr int VERAO_MES = 12;
inline constexpr int OUTONO_MES = 3;
inline constexpr int INVERNO_MES = 6;
inline constexpr int PRIMAVERA_MES = 9;

// --- Dia e noite --------------------------------------------------------------
// Padrão: 12 h de dia e 12 h de noite (outono e primavera).
// No verão o dia cresce aos poucos (em curva) até 13 h 30 no meio da
// estação e volta; no inverno encolhe até 11 h no meio e volta.
// O dia fica centrado no meio-dia (nasce às 12 h − dia/2).
inline constexpr double DIA_PADRAO_H = 12.0;
inline constexpr double DIA_MAX_VERAO_H = 13.5;
inline constexpr double DIA_MIN_INVERNO_H = 11.0;
// Horário do abrigo (regra antiga, mantida): de 06:00 às 18:00 é "dia" para
// quem está no abrigo (sai às 06:00 se não está procriando).
inline constexpr double DIA_COMECA_H = 6.0;
inline constexpr double NOITE_COMECA_H = 18.0;

// --- Temperatura (média do mundo todo; um dia vai depender de região/bioma)
// Mínima e máxima de cada estação, em °C (no meio da estação; entre uma e
// outra a temperatura muda aos poucos, em curva).
inline constexpr double VERAO_MIN = 20.0, VERAO_MAX = 35.0;
inline constexpr double OUTONO_MIN = 18.0, OUTONO_MAX = 28.0;
inline constexpr double INVERNO_MIN = 4.0, INVERNO_MAX = 18.0;
inline constexpr double PRIMAVERA_MIN = 20.0, PRIMAVERA_MAX = 30.0;
// Forma do dia (a mesma de antes, mas presa ao nascer e ao pôr do sol):
//   mínima 2 h antes de o sol nascer; sobe até a máxima 8 h depois de
//   nascer; fica na máxima até o pôr do sol; cai até a mínima 4 h depois
//   do pôr do sol; fica na mínima até a manhã seguinte.
// (Com o dia de 12 h isso dá exatamente a curva antiga: 04h, 14h, 18h, 22h.)
inline constexpr double TEMP_MIN_ANTES_NASCER_H = 2.0;
inline constexpr double TEMP_MAX_DEPOIS_NASCER_H = 8.0;
inline constexpr double TEMP_MIN_DEPOIS_POR_H = 4.0;
// A partir de 5° é "frio": o NPC vai para o abrigo.
inline constexpr double TEMP_FRIO = 5.0;
// Abaixo de 4° é crítico: quem está fora do abrigo perde HP.
inline constexpr double TEMP_CRITICA = 4.0;

// --- Família ----------------------------------------------------------------
// Os NPCs iniciais começam com 20 anos; morrem de velhice aos 100 anos.
inline constexpr double IDADE_INICIAL_ANOS = 20.0;
inline constexpr double IDADE_MAXIMA_ANOS = 100.0;
// Procriar: dos 16 anos até 45 (mulheres) e 60 (homens).
inline constexpr double IDADE_MIN_PROCRIAR = 16.0;
inline constexpr double IDADE_MAX_PROCRIAR_MULHER = 45.0;
inline constexpr double IDADE_MAX_PROCRIAR_HOMEM = 60.0;
// O casal fica 1 hora junto no abrigo; sempre dá certo.
inline constexpr double PROCRIAR_H = 1.0;
// Gravidez: 9 meses do calendário. Depois de nascer, a mãe só pode ter
// outro 2 meses depois.
inline constexpr int GRAVIDEZ_MESES = 9;
inline constexpr int INTERVALO_FILHOS_MESES = 2;
// O bebê vira adulto aos 15 anos (fica bebê até lá, por enquanto).
inline constexpr double BEBE_VIRA_ADULTO_ANOS = 15.0;

// --- HP (vida) --------------------------------------------------------------
// 0 a 100, começa em 100. Morre com HP 0.
inline constexpr double HP_MAXIMO = 100.0;
inline constexpr double HP_INICIAL = 100.0;
// Cada necessidade no crítico (fome 0, sede 0, temperatura < 4° fora do
// abrigo) tira 5 de HP por minuto; as que estão no crítico se somam.
inline constexpr double HP_DANO_POR_CRITICO_MIN = 5.0;
// Dormindo no abrigo, recupera 1 de HP por minuto.
inline constexpr double HP_RECUPERA_ABRIGO_MIN = 1.0;

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
// A água se move a cada 5 s.
inline constexpr double AGUA_INTERVALO_S = 5.0;
// Gelo (pedido em 15/11/2026): no inverno a camada de cima da água dos
// VALES congela (o mar não); derrete quando o inverno acaba. Dá para andar
// por cima; não dá para beber.

// --- Quantidades iniciais -------------------------------------------------
// NPCs iniciais (pedido em 14/11/2026): o jogador escolhe quantos homens
// e quantas mulheres em Arquivo -> Nova simulação (sem máximo). Ao abrir o
// programa direto, começa com 3 homens e 3 mulheres. Nomes: "NPC 1",
// "NPC 2"...; o sexo de cada número é sorteado.
inline constexpr int HOMENS_PADRAO = 3;
inline constexpr int MULHERES_PADRAO = 3;
// --- Inventário (pedido em 15/11/2026): o que o NPC carrega é medido em
// PESO, com qualquer mistura. Na mão: até 8 kg; no abrigo: até 80 kg.
inline constexpr double INVENTARIO_KG = 8.0;
inline constexpr double ABRIGO_KG = 80.0;
inline constexpr double PESO_MACA_KG = 0.5;
inline constexpr double PESO_OVO_KG = 1.0;
// Ovo: o NPC come em 5 min e recupera 30 de fome (não mexe na sede).
inline constexpr double OVO_RECUPERA = 30.0;
inline constexpr double COMER_OVO_S = 5.0 * 60.0;
// A memória da macieira vazia: chegou e não viu maçã -> não volta lá por
// comida por 2 a 5 dias (sorteado a cada vez).
inline constexpr double MACIEIRA_VAZIA_MIN_S = 2.0 * 86400.0;
inline constexpr double MACIEIRA_VAZIA_MAX_S = 5.0 * 86400.0;
// Caça: o NPC com fome e sem outra comida mata animais. Dano base 10 por
// ataque; um ataque a cada 5 s (para todos, por enquanto).
inline constexpr double NPC_DANO = 10.0;
inline constexpr double ATAQUE_S = 5.0;
inline constexpr int NUM_MACIEIRAS = 6;

// --- Fome -----------------------------------------------------------------
// Vai de 0 (faminto) a 100 (satisfeito). Em 0 é crítico: o NPC perde HP
// (antes do HP, morria em 0).
inline constexpr double FOME_MAXIMA   = 100.0;
inline constexpr double FOME_MINIMA   = 0.0;
inline constexpr double FOME_INICIAL  = 100.0;
// A cada 15 minutos, a fome abaixa 3.
inline constexpr double FOME_INTERVALO_S = 15.0 * 60.0;
inline constexpr double FOME_QUEDA       = 3.0;
// O NPC só come quando a fome está abaixo de 70.
inline constexpr double FOME_LIMITE_COMER = 70.0;

// --- Sede (pedida em 14/11/2026) -----------------------------------------
// Igual à fome: vai de 0 a 100, começa em 100, em 0 é crítico (perde HP).
inline constexpr double SEDE_MAXIMA  = 100.0;
inline constexpr double SEDE_MINIMA  = 0.0;
inline constexpr double SEDE_INICIAL = 100.0;
// A cada 10 minutos, a sede abaixa 4.
inline constexpr double SEDE_INTERVALO_S = 10.0 * 60.0;
inline constexpr double SEDE_QUEDA       = 4.0;
// Abaixo de 50 o NPC tenta beber água (do mar mesmo).
inline constexpr double SEDE_LIMITE_BEBER = 50.0;
// Um gole recupera 15 e demora 20 s.
inline constexpr double GOLE_RECUPERA   = 15.0;
inline constexpr double BEBER_DURACAO_S = 20.0;

// --- Maçãs e macieiras ----------------------------------------------------
// Cada maçã recupera 12 de fome.
inline constexpr double MACA_RECUPERA = 12.0;
// ...e também 5 de sede (pedido em 14/11/2026).
inline constexpr double MACA_RECUPERA_SEDE = 5.0;
// Ciclo da macieira no ano: floresce na primavera, dá frutos no verão,
// folhas secas no outono, repouso no inverno.
// No verão cada macieira (viva) derruba de 80 a 300 maçãs (sorteado por
// árvore, a cada verão). Começam a cair no 2º dia do verão e vão até o fim
// dele: poucas no começo, mais e mais até o pico no meio, depois menos.
inline constexpr int MACAS_POR_VERAO_MIN = 80;
inline constexpr int MACAS_POR_VERAO_MAX = 300;
inline constexpr int QUEDA_COMECA_DIA_DO_VERAO = 2;
// No chão, as maçãs se juntam: até 32 na mesma célula (uma pilha).
inline constexpr int MACAS_POR_PILHA = 32;
// --- Vida vegetal -----------------------------------------------------------
// Cada maçã no chão que ninguém pega em 6 horas: 40% de chance de virar
// muda; senão (60%) apodrece e some. Na mão do NPC e no abrigo ela não muda.
inline constexpr double MACA_VIRA_MUDA_H = 6.0;
inline constexpr double MACA_CHANCE_MUDA = 0.40;
// A muda vira macieira depois de 3 anos.
inline constexpr double MUDA_VIRA_ARVORE_ANOS = 3.0;
// A macieira vive 40 anos; depois fica seca (cinza, sem maçãs) por 1 ano e
// some. As macieiras do começo já têm 2 anos.
inline constexpr double MACIEIRA_VIDA_ANOS = 40.0;
inline constexpr double MACIEIRA_SECA_ANOS = 1.0;
inline constexpr double MACIEIRA_IDADE_INICIAL_ANOS = 2.0;

// --- Pedras, ferramentas, lenha e abrigo (pedido em 14/11/2026) ------------
// 3 pedras por mapa; cada uma tem 10 lascas e some quando esgota.
inline constexpr int NUM_PEDRAS = 3;
inline constexpr int PEDRA_LASCAS = 10;
// Cada trabalho leva um tempo sorteado (a cada vez) entre o mínimo e o
// máximo, em segundos.
// Tirar lascas: 15 a 20 min, dá 2 lascas. Tirar galhos de uma árvore (viva
// ou seca): 5 a 15 min, dá 1 galho, não estraga a árvore.
inline constexpr double LASCAS_MIN_S = 15.0 * 60.0, LASCAS_MAX_S = 20.0 * 60.0;
inline constexpr int LASCAS_POR_VEZ = 2;
inline constexpr double GALHOS_MIN_S = 5.0 * 60.0, GALHOS_MAX_S = 15.0 * 60.0;
inline constexpr int GALHOS_POR_VEZ = 1;
// Machado primitivo: gasta 6 lascas e 2 galhos; leva 1 a 3 h (onde ele
// estiver); dura para sempre.
inline constexpr int MACHADO_LASCAS = 6;
inline constexpr int MACHADO_GALHOS = 2;
inline constexpr double MACHADO_MIN_S = 1.0 * 3600.0, MACHADO_MAX_S = 3.0 * 3600.0;
// Cortar uma árvore (viva ou seca) com o machado: 2 a 6 h; a árvore some e
// dá 30 lenhas.
inline constexpr double CORTAR_MIN_S = 2.0 * 3600.0, CORTAR_MAX_S = 6.0 * 3600.0;
inline constexpr int LENHA_POR_ARVORE = 30;
// Abrigo 🏠: gasta 20 lenhas, leva 2 a 4 dias para construir (na célula onde
// o NPC está; continua de noite), ocupa a célula, cabe 2 NPCs e dura para
// sempre. Guarda até 80 kg (ver INVENTARIO); lá nada estraga.
inline constexpr int ABRIGO_LENHA = 20;
inline constexpr double CONSTRUIR_MIN_S = 2.0 * 86400.0, CONSTRUIR_MAX_S = 4.0 * 86400.0;
inline constexpr int ABRIGO_VAGAS = 2;

// Comer uma maçã leva 3 minutos.
inline constexpr double COMER_DURACAO_S = 3.0 * 60.0;

// --- NPC ------------------------------------------------------------------
// Andar 1 célula leva 5 s.
inline constexpr double NPC_PASSO_S = 5.0;
// Subir ou descer 1 nível leva 10 s (em vez de 5 s). Mais de 1 nível, não sobe.
inline constexpr double NPC_SUBIR_S = 10.0;
inline constexpr int NPC_DEGRAU_MAX = 1;
// Explorar (pedido em 14/11/2026): sem nada a resolver, o NPC escolhe um
// destino a uma distância sorteada de 2 a 20 células e anda até lá.
inline constexpr int EXPLORAR_DIST_MIN = 2;
inline constexpr int EXPLORAR_DIST_MAX = 20;
// Enxerga maçãs e água numa área de 32 x 32 células em volta dele.
inline constexpr int VISAO_LADO = 32;

// ===========================================================================
// FAUNA (pedido em 15/11/2026). Cada espécie tem a sua ficha (o "molde"):
// os mesmos campos para todas, com os valores dela. Hoje só a galinha.
// ===========================================================================
struct FichaEspecie {
    // Fome e sede: 0 a 100; caem `queda` a cada `intervalo_s`.
    double fome_intervalo_s, fome_queda, sede_intervalo_s, sede_queda;
    double fome_limite, sede_limite;     // abaixo disso vai comer / beber
    double comer_s, comer_ganho;         // uma "comida" (galinha: ciscar a grama)
    double beber_s, beber_ganho;         // um gole (galinha: uma bicada)
    // Movimento, exploração e visão.
    double passo_s, subir_s;
    int degrau_max;
    int explorar_min, explorar_max;      // distância do destino
    int visao_lado;                      // área que enxerga (lado do quadrado)
    // HP de cada fase; dano por necessidade crítica e recuperação dormindo.
    double hp_filhote, hp_femea, hp_macho;
    double dano_critico_min, recupera_dormindo_min;
    // Sono (hora de dormir e de acordar).
    double dorme_h, acorda_h;
    // Idade: filhote segue a mãe (sem fome/sede) até `independente_dias`;
    // vira adulto em `adulto_dias`; morre de velhice em `velhice_anos`.
    double independente_dias, adulto_dias, velhice_anos;
    // Bando: os adultos seguem o macho; os filhotes, a mãe. Cada um fica
    // a uma distância sorteada entre min e max de quem segue. Sozinha, a
    // fêmea procura um macho numa área de `busca_macho_lado`.
    int bando_dist_min, bando_dist_max, busca_macho_lado;
    // Luta e fuga: dano por ataque; chance de revidar (macho; mãe com
    // filhotes ou chocando); distância de fuga e tempo arisco (sorteados
    // para cada animal).
    double dano_filhote, dano_femea, dano_macho;
    double revida_macho, revida_mae;
    int fuga_min, fuga_max;
    double arisco_min_s, arisco_max_s;
    // Corpo: quem morre sozinho some em `corpo_some_dias`; morto por NPC
    // vira carne (peso, quanto recupera, quanto tempo leva para comer), que
    // estraga no chão em `carne_estraga_dias` (na mão e no abrigo, não).
    double corpo_some_dias, carne_estraga_dias;
    double peso_filhote, peso_femea, peso_macho;
    double carne_filhote_ganho, carne_filhote_s, carne_adulto_ganho, carne_adulto_s;
    // Reprodução (galinha): macho e fêmea juntos `namoro_min`; a fêmea põe
    // 1 ovo por dia (entre postura_ini_h e postura_fim_h), fica no ninho
    // `ninho_fica_h`, de ovos_min a ovos_max ovos; depois choca
    // `chocar_dias`; de falha_min a falha_max dos ovos não prestam; ovo sem
    // contato estraga em `ovo_estraga_dias`; ela volta a pôr
    // `volta_a_por_dias` depois de os filhotes nascerem.
    double namoro_min;
    double postura_ini_h, postura_fim_h, ninho_fica_h;
    int ovos_min, ovos_max;
    double chocar_dias, falha_min, falha_max, ovo_estraga_dias, volta_a_por_dias;
};

inline constexpr FichaEspecie GALINHA = {
    /* fome */ 10.0 * 60.0, 3.0, /* sede */ 10.0 * 60.0, 4.0,
    /* limites (os mesmos do NPC) */ 70.0, 50.0,
    /* ciscar */ 10.0, 10.0, /* bicada na água */ 5.0, 5.0,
    /* 0,5 célula por 3 s = 1 célula a cada 6 s; subir 10 s */ 6.0, 10.0, 1,
    /* explorar */ 1, 7, /* visão */ 16,
    /* HP */ 10.0, 40.0, 60.0, /* −5/min por crítico, +5/min dormindo */ 5.0, 5.0,
    /* dorme 19:00 – 05:00 */ 19.0, 5.0,
    /* pintinho segue a mãe até 60 dias; adulto aos 90; morre com 3 anos */
    60.0, 90.0, 3.0,
    /* bando 3 a 5 células; galinha sozinha procura galo em 64 x 64 */ 3, 5, 64,
    /* dano */ 0.0, 2.0, 2.5, /* revida: galo 20%, mãe 80% */ 0.20, 0.80,
    /* fuga 8 a 15 células; arisca de 2 semanas a 3 anos */ 8, 15,
    14.0 * 86400.0, 3.0 * 365.0 * 86400.0,
    /* corpo some 3 dias; carne estraga 2 dias */ 3.0, 2.0,
    /* peso: pintinho 1, galinha 4, galo 5 kg */ 1.0, 4.0, 5.0,
    /* carne: pintinho +10 em 5 min; galinha/galo +80 em 30 min */
    10.0, 5.0 * 60.0, 80.0, 30.0 * 60.0,
    /* namoro 15 min */ 15.0,
    /* põe de manhã (06:00–10:00) e fica 1 h */ 6.0, 10.0, 1.0,
    /* 12 a 14 ovos */ 12, 14,
    /* choca 21 dias; 5–20% não prestam; ovo estraga 1 dia; volta 60 dias */
    21.0, 0.05, 0.20, 1.0, 60.0,
};

// No começo: 10 galinhas e 2 galos, em 2 bandos (1 galo + 5 galinhas),
// todos com 91 dias.
inline constexpr int BANDOS_INICIAIS = 2;
inline constexpr int GALINHAS_POR_BANDO = 5;
inline constexpr double IDADE_INICIAL_GALINHA_DIAS = 91.0;

}  // namespace genesis::config
