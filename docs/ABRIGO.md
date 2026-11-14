# Pedra, ferramentas, lenha e abrigo (motor 0.10)

Pedido pelo dono do projeto em 14/11/2026.

## Regras definidas pelo dono do projeto

### Pedra ⬟ (cinza escuro)
- **3 pedras** por mapa, na superfície. Ocupam a célula (têm física).
- Cada pedra tem **10 lascas**; **esgotou, some**.

### Coletar
| O quê | De onde | Tempo | Dá |
|---|---|---|---|
| Lascas | pedra (ao lado) | 30 s | 2 lascas |
| Galhos | árvore viva ou seca (ao lado) — não estraga a árvore | 30 s | 1 galho |
| Lenha (cortar) | árvore viva ou seca, **com machado** — a árvore **some** | 30 min do jogo | 30 lenhas |

- **Machado primitivo**: quando tem **6 lascas e 2 galhos**, faz **na hora**
  (gasta os dois) e ele **dura para sempre**.
- Carrega **sem limite** (por enquanto): maçã (1), lascas, galhos, lenha,
  machado. O painel mostra tudo.

### Abrigo 🏠
- Gasta **20 lenhas**, leva **2 horas** para construir, **na célula onde o
  NPC está**. Ocupa a célula, **cabe 2 NPCs**, **qualquer um pode usar**,
  **dura para sempre**.
- **Dentro não faz frio, nem fome, nem sede**; **dormindo, recupera 1 de
  HP por segundo**.
- **Entra quando a temperatura chega a 5° ou menos; sai às 06:00.**

### Quando ele faz isso
- **Saciado** (sem fome e sem sede), **sem abrigo**: a necessidade passa a
  ser **fazer o abrigo** (por causa da temperatura). Enquanto explora, se
  acha pedra ou árvore, já começa.
- **Com fome ou sede no meio do trabalho**, ele **para, vai cuidar e depois
  volta**; o trabalho fica **guardado** (de onde parou).

## Como o código implementa

| Parte | Onde | Como |
|---|---|---|
| Números | `config.hpp` | `NUM_PEDRAS`, `PEDRA_LASCAS`, `TIRAR_LASCAS_S`, `LASCAS_POR_VEZ`, `TIRAR_GALHOS_S`, `GALHOS_POR_VEZ`, `MACHADO_LASCAS`, `MACHADO_GALHOS`, `CORTAR_H`, `LENHA_POR_ARVORE`, `ABRIGO_LENHA`, `CONSTRUIR_H`, `ABRIGO_VAGAS`. |
| Ordem de decisão | `Mundo::evento_passo` | 1) Dentro do abrigo: dorme até as 06:00. 2) Trabalhando e surgiu fome/sede/frio: pausa (guarda o que falta). 3) Necessidades pela **urgência**: comer, beber e (com frio, se tem abrigo) ir para o abrigo. 4) Saciado: **cadeia do abrigo** (ou retomar o trabalho parado). 5) Pegar maçã. 6) Explorar. |
| Escolher o trabalho | `Mundo::escolher_tarefa` | Lenha ≥ 20 → construir aqui. Machado → cortar uma árvore. Senão → lascas (até 6) e galhos (até 2), o que estiver mais perto. |
| Achar pedra/árvore | `Mundo::procurar_recurso` | Primeiro o que ele **vê** (32 × 32), senão o que **lembra**; "mais perto" = menos passos **a pé**, só alvos que dá para alcançar. |
| Trabalhar | `Mundo::trabalhar`, `evento_fim_trabalho` | Vai até o alvo (ao lado; a obra, na própria célula), trabalha o tempo da tarefa, recebe o resultado. Alvo que sumiu: desiste (e esquece, se era lembrado). |
| Morador | `Mundo::conhecer_abrigos` | Quem constrói vira morador. Quem vê um abrigo com vaga de morador (até 2) passa a ser morador dele — é para lá que vai com frio. |
| Entrar / sair | `ir_para_abrigo`, `entrar_abrigo`, `sair_abrigo` | Ao lado do abrigo dele, com lugar, entra (o NPC sai da célula e fica "dentro"). Às 06:00 sai para uma célula livre ao lado. |
| Dentro | `taxa_hp`, `evento_fome`, `evento_sede` | HP +1/s; fome e sede não abaixam; frio não conta. |
| Caminho | `Mundo::passo_em_direcao` | **Busca de caminho de verdade** (em largura): o NPC contorna barrancos, água e ocupados. Vale para tudo (comida, água, trabalho, abrigo). Se não há caminho, dá o passo "em linha reta" de antes. |

### Decisões minhas (PROVISÓRIAS, aguardando o dono)
- **Morador**: a ideia de "morador" (2 por abrigo) para saber para qual
  abrigo cada um vai e quem ainda precisa construir.
- **Lembrar a macieira ao tirar galhos** (é "usar" a árvore, como comer).
- Empate de urgência: abrigo, depois sede, depois fome.
- Na tela, prioridade no zoom afastado: abrigo logo abaixo de NPC/corpo;
  pedra junto da muda.

## Testes
- `teste_trabalho`: 3 pedras com 10 lascas; lascas (30 s, +2), galhos
  (30 s, +1), machado (6 + 2, na hora), corte (30 min, +30 lenhas, árvore
  some), abrigo (2 h, −20 lenhas, vira morador); até 2 moradores/dentro.
- `teste_noite_no_abrigo`: trabalho parado pela fome guarda o progresso e é
  retomado; dentro do abrigo dorme, sem frio/fome/sede, HP +1/s, sai às 06:00.
- Funções **só para testes**: `teste_npc` (fome/sede/HP), `teste_dar`
  (itens), `teste_mover` (pôr o NPC numa célula).

## Observado (para o dono do projeto avaliar)

20 ilhas, 80 NPCs, do Dia 1 às 07:00 do Dia 2:

| | Sem ajuda | Com comida e água garantidas (só no teste) |
|---|---|---|
| Vivos às 21:30 | 1 | 80 |
| Abrigos construídos | 0 | 28 |
| Com abrigo (morador) | 0 | 45 |
| Mortos de frio | 1 | 65 |
| Vivos às 07:00 do Dia 2 | 0 | 15 |

- **Sem ajuda**, quase ninguém chega vivo à noite (sede/fome antes).
- **Com comida garantida**, os abrigos saem, mas **a maioria morre de frio
  no caminho**: o NPC só vai para o abrigo com **5°** (~21:32), e com
  **menos de 4°** (~21:42) já perde 5 HP/s — morre em 20 s. São **~30 s**
  para chegar: só quem está a poucas células do abrigo consegue.
- Não mudei nada: é o resultado das regras atuais.
