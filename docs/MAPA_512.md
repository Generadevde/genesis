# Mapa 512 x 512, bosques, lagos e minimapa (motor 0.14)

Pedido pelo dono do projeto em 15/11/2026. Números em `motor/src/config.hpp`.

## Regras do dono do projeto
- Mapa de **512 x 512** células; mar numa borda de **32** células; uma ilha
  só. O relevo continua o mesmo (30% vale, 30% morro, 40% planície;
  morros até +4, vales até −4; morros com tamanho médio de 24 células).
- **Lagos:** 4 vales **sem ligação com o mar** (sorteados; tamanhos e
  fundos variados) enchem de água. Congelam no inverno, como os vales.
- **NPCs** nascem **juntos, no meio do mapa**.
- Na janela **Arquivo → Nova simulação** dá para mudar tudo (o padrão entre
  parênteses):

| Campo | Padrão | Como fica no mapa |
|---|---|---|
| Homens / Mulheres | 3 / 3 | juntos no meio |
| Macieiras | 48 | 9% soltas, 91% em **bosques de 6 a 28**; nunca em pedra nem areia; **idade sorteada** de muda (0 a 3 anos) até macieira seca (o último ano dos 44) |
| Galinhas / Galos | 28 / 3 | cada galo num lugar sorteado com um **bando de 4 a 8 galinhas**; as que sobram ficam **espalhadas, sozinhas** (procurando galo); todas **adultas**, idade sorteada de 90 dias a 3 anos |
| Pedras | 25 | 70% soltas, 30% em **grupos de 2 a 4**; em qualquer lugar; só dão lascas |

## Minimapa
No painel da direita, acima da Seleção: **1 pixel por célula** (o mapa
inteiro). Cores: grama, terra, pedra, água, neve, areia, gelo, macieira,
muda, maçã, pedra (⬟), abrigo, NPC (quadradinho rosa maior), animal, ovo,
carne. Um **retângulo branco** mostra o pedaço que está na tela. **Clicar no
minimapa leva a câmera para lá.**

## Como o código implementa

| Parte | Onde | Como |
|---|---|---|
| Tamanho | `config.hpp` | `LARGURA`, `ALTURA` = 512; `BORDA_AGUA` = 32. |
| Lagos | `gerador.cpp` (3b) | Acha os grupos de colunas com chão abaixo do 0 que ficaram secos (não ligados ao mar), embaralha e enche 4 até o nível 0. |
| Mundo inicial | `Mundo::criar_macieiras`, `criar_pedras`, `criar_galinhas` | Sorteio de lugares (`sortear_lugar`) e de vizinhos em volta de um centro (`lugar_perto`). |
| Minimapa | `Mundo::minimapa` (C++) → `minimapa_bytes` → `principal.py` (`desenhar_minimapa`) | 1 byte por célula com o "tipo" visto de cima; a interface vira isso numa imagem de 512 x 512 com uma paleta. Redesenha a cada 0,5 s real. |
| Desempenho | `nova_busca`, `procurar_recurso`, `caminho_ate` | Com o mapa 16 vezes maior: as buscas de caminho reaproveitam um rascunho (não criam tabelas do tamanho do mapa a cada passo); a busca de pedra/árvore para no primeiro que acha; a ida do animal à água lembrada usa A* (vai primeiro na direção da água). |

## Decisões minhas (PROVISÓRIAS, aguardando o dono)
- Painel da direita mais largo (536 px) para caber o minimapa de 512; o
  símbolo grande da Seleção diminuiu (128 → 64 px). Cores do minimapa;
  NPCs em quadradinhos de 3 x 3.
- O último bosque fica com o que sobrar (pode ter menos de 6).
- "Vale" para lago: grupo de colunas vizinhas (norte, sul, leste, oeste)
  com o chão abaixo do 0; enche até o 0.
- Animal que não acha caminho até a água lembrada só tenta de novo **1 hora
  depois** (explorando enquanto isso) — a busca no mapa inteiro a cada passo
  deixava tudo lento.

## O que os testes mostraram (para o dono decidir)
- **Água longe:** no meio do mapa, a água mais perto fica, em muitas ilhas,
  a 35–100 células (às vezes mais) — a visão do NPC é 32 x 32 e a da galinha
  16 x 16. NPCs e galinhas longe da água morrem de sede em ~4 horas.
- **Velocidade:** com 6 NPCs e ~30 galinhas, 1 dia do jogo leva ~1 s. O
  custo cresce com o número de animais (~0,9 µs por passo de animal): com
  150 animais, ~2 s por dia.
- Criar um mundo novo leva ~1 a 3 s (o gerador do mapa 512).

## Testes
`teste_mapa` (512, borda 32, até 4 lagos), `teste_mundo_inicial` (48
macieiras em bosques, 25 pedras, 3 galos com bandos de 4–8, NPCs no meio),
e os outros adaptados.
