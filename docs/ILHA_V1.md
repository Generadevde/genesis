# Primeira simulação: a ilha (motor 0.2)

Pedido e respostas do dono do projeto (14/11/2026). Todos os números estão
em `motor/src/config.hpp`, cada um com o comentário de onde veio.

## Regras definidas pelo dono do projeto

### Executável e tela
- `./genesis` abre direto na simulação, **sem menus de início**, em **tempo real**.
- A tela é a interface própria estilo Windows 98: ver [INTERFACE.md](INTERFACE.md)
  (menu Arquivo, abas Mapa/NPCs, painel de seleção, câmera nas setas,
  zoom 0,5x / 1x / 2x / 4x / 6x com a prioridade NPC > corpo > maçã >
  macieira > grama > água).

### Mapa
- **128 × 128** células, quadrado (por enquanto). Era 512 × 512: a tela do
  dono do projeto mostrava só água.
- **8 células de água** a partir de cada borda (era 48); depois começa a
  terra. A terra fica com **112 × 112**.
- Grama: `"` em **verde**. Água: `~` em **azul**.

### Macieiras 🌳
- **6 macieiras** espalhadas (posições sorteadas na terra).
- Derruba **1 maçã por vez**, **em volta dela**.
- Tempo até a próxima queda: sorteado **entre 30 e 60 s**, **de novo a cada
  queda**, **em cada macieira separadamente**.
- Se a volta estiver lotada, a maçã cai **na próxima camada**.
- **Sem limite** de maçãs no chão. **Não estraga** (por enquanto).

### Maçãs 🍎
- Cada maçã recupera **12 de fome** (sem passar de 100).
- Comer **leva 10 s**.

### NPCs 🗿
- **4 NPCs**, fome inicial **100**.
- **Fome 0 a 100**: 100 = satisfeito, 0 = faminto. **A cada 5 s, abaixa 3.**
- Fome **0 → morre**, e **o corpo fica** no mapa, marcado com uma cruz **✝** (U+271D).
- **Só come com fome abaixo de 70.**
- Enxerga maçãs numa área de **32 × 32** em volta dele.
  Com várias à vista, vai na **mais próxima**.
- Anda **1 célula por segundo**, aleatoriamente, **sem voltar para a célula
  de onde acabou de sair** (último movimento).

### Sede 💧 (pedida em 14/11/2026)
- Igual à fome: **0 a 100**, começa em **100**. **A cada 5 s, abaixa 4.**
- Sede **0 → morre** (o corpo fica, como na fome).
- **Abaixo de 50**, o NPC tenta beber **água do mar**.
- Enxerga água na mesma área de **32 × 32**. Vai na mais próxima.
- Não entra na água: **para na célula vizinha** e bebe dali.
- **Um gole recupera 15** (sem passar de 100) e **demora 10 s**. A água não acaba.

### Durante a ação
- **Enquanto come, a fome não abaixa. Enquanto bebe, a sede não abaixa.**
  (A outra necessidade continua abaixando normalmente.)

### Fome e sede ao mesmo tempo
- Com fome < 70 **e** sede < 50, ele tenta primeiro **a mais baixa**.
- Se não achar o que ela pede, mas achar o da outra (que também está
  baixa), vai na outra.

### Física ("corpos têm física")
- Cada célula tem **no máximo uma coisa**: macieira, maçã, NPC ou corpo.
- NPCs não ocupam a mesma célula entre si nem a da macieira.
- O corpo do NPC morto também ocupa a célula: ninguém passa por cima (confirmado).
- NPC não entra na água (confirmado).

## Como o código implementa cada regra

| Regra | Onde | Como |
|---|---|---|
| Tempo real | `interface/principal.py` | A cada quadro, mede quanto tempo passou no relógio e chama `mundo.avancar(dt)` com esse valor. (O resto da tela está em INTERFACE.md.) |
| Passagem do tempo | `Mundo::avancar` | Simulação por eventos: cada NPC e cada macieira tem um relógio regressivo. O motor anda até o próximo que vence, dispara, e segue. Não há "passo interno" inventado. |
| Fome −3 a cada 5 s | `Mundo::evento_fome` | Relógio de 5 s por NPC. Chegou a 0 → `morrer`. |
| Morte | `Mundo::morrer` | Estado vira Morto; a célula passa a ter um Corpo (continua ocupada). Se estava comendo, a maçã fica livre para outro. |
| Sede −4 a cada 5 s | `Mundo::evento_sede` | Relógio de 5 s por NPC, igual à fome. Chegou a 0 → `morrer`. Pulado enquanto está bebendo. |
| Pausa durante a ação | `Mundo::evento_fome` / `evento_sede` | O relógio continua, mas a queda é pulada se o NPC está comendo (fome) ou bebendo (sede). |
| Fome e sede juntas | `Mundo::evento_passo` | Se as duas estão baixas: tenta a menor (`tentar_beber` / `tentar_comer`); se não achar nada para ela, tenta a outra. |
| Achar água | `Mundo::procurar_agua` | Na área 32 × 32, a célula de água mais próxima que tenha ao lado uma célula de onde dá para beber. |
| Beber 10 s | `Mundo::tentar_beber` / `evento_fim_beber` | Ao lado da água, fica Bebendo por 10 s; no fim, sede +15. |
| Passo de 1 s | `Mundo::evento_passo` | Relógio de 1 s por NPC. Anda em 8 direções (diagonais contam como 1 célula). |
| Andar sem voltar | `Mundo::evento_passo` | Sorteia entre as vizinhas livres, menos a de onde veio. Se a única saída for voltar, volta (para não ficar preso). |
| Visão 32 × 32 | `Mundo::procurar_maca` | Olha 16 células para um lado e 15 para o outro (32 com a dele), nas duas direções. |
| Ir até a maçã | `Mundo::evento_passo` | Com fome < 70 e maçã à vista: passo para a vizinha livre que mais aproxima. A maçã ocupa a célula dela, então o NPC come **de uma célula vizinha**. |
| Comer 10 s | `Mundo::evento_fim_comer` | Ao começar, a maçã é reservada para ele (ninguém mais come). Depois de 10 s: a maçã some e a fome sobe 12. |
| Queda da maçã | `Mundo::evento_queda_maca` | Camada 1 = as 8 vizinhas da árvore. Sorteia uma livre; se não houver, tenta a camada 2, e assim por diante. Só cai em grama. |

## Testes (`testes/teste_ilha.py`)

Rodam sem tela, acelerando o tempo. Conferem: mapa e borda de água; 4 NPCs
com fome 100, sede 100 e 6 macieiras; fome −3 e sede −4 a cada 5 s; maçãs
só depois de 30 s e em volta das árvores; morte aos 125 s sem beber, com o
corpo ficando; comer e beber levam 10 s, com ganho de 12 e 15; durante a
ação a necessidade não abaixa; só vai comer com fome < 70 e só vai beber
com sede < 50; física (uma coisa por célula, NPC só em grama) em 50 ilhas
× 1 hora simulada.

## Observado nos testes (para o dono do projeto avaliar)

Com fome e sede (50 ilhas × 1 hora simulada, 200 NPCs):
- **79 chegaram a comer**, **107 chegaram a beber**.
- Depois de 1 hora, **só 1 dos 200 continuava vivo**.
- Sem beber, o NPC morre aos **125 s** (antes dos 170 s da fome). Ele só
  vê a água se estiver a até 16 células dela, e a terra tem 112 de largura:
  quem nasce no meio da ilha não enxerga o mar.
- Não mudei nada: é o resultado das regras atuais.

## Em aberto

- **Empate**: se fome e sede estiverem com o mesmo valor (as duas baixas),
  hoje ele tenta a **sede** primeiro — PROVISÓRIO, aguardando o dono.

