# Primeira simulação: a ilha (motor 0.1)

Pedido e respostas do dono do projeto (14/11/2026). Todos os números estão
em `motor/src/config.hpp`, cada um com o comentário de onde veio.

## Regras definidas pelo dono do projeto

### Executável
- `./genesis` abre direto na simulação, **sem menus**. Sai com **Ctrl+C**.
- Simulação em **tempo real** (por enquanto).
- **Uma janela grande mostrando a ilha inteira**, sem câmera (por enquanto).

### Mapa
- **512 × 512** células, quadrado (por enquanto).
- **48 células de água** a partir de cada borda; depois começa a terra.
  A terra fica com **416 × 416**.
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
- Fome **0 → morre**, e **o corpo fica** no mapa.
- **Só come com fome abaixo de 70.**
- Enxerga maçãs numa área de **32 × 32** em volta dele.
  Com várias à vista, vai na **mais próxima**.
- Anda **1 célula por segundo**, aleatoriamente, **sem voltar para a célula
  de onde acabou de sair** (último movimento).

### Física ("corpos têm física")
- Cada célula tem **no máximo uma coisa**: macieira, maçã, NPC ou corpo.
- NPCs não ocupam a mesma célula entre si nem a da macieira.

## Como o código implementa cada regra

| Regra | Onde | Como |
|---|---|---|
| Tempo real | `interface/principal.py` | A cada quadro, mede quanto tempo passou no relógio e chama `mundo.avancar(dt)` com esse valor. |
| Passagem do tempo | `Mundo::avancar` | Simulação por eventos: cada NPC e cada macieira tem um relógio regressivo. O motor anda até o próximo que vence, dispara, e segue. Não há "passo interno" inventado. |
| Fome −3 a cada 5 s | `Mundo::evento_fome` | Relógio de 5 s por NPC. Chegou a 0 → `morrer`. |
| Morte | `Mundo::morrer` | Estado vira Morto; a célula passa a ter um Corpo (continua ocupada). Se estava comendo, a maçã fica livre para outro. |
| Passo de 1 s | `Mundo::evento_passo` | Relógio de 1 s por NPC. Anda em 8 direções (diagonais contam como 1 célula). |
| Andar sem voltar | `Mundo::evento_passo` | Sorteia entre as vizinhas livres, menos a de onde veio. Se a única saída for voltar, volta (para não ficar preso). |
| Visão 32 × 32 | `Mundo::procurar_maca` | Olha 16 células para um lado e 15 para o outro (32 com a dele), nas duas direções. |
| Ir até a maçã | `Mundo::evento_passo` | Com fome < 70 e maçã à vista: passo para a vizinha livre que mais aproxima. A maçã ocupa a célula dela, então o NPC come **de uma célula vizinha**. |
| Comer 10 s | `Mundo::evento_fim_comer` | Ao começar, a maçã é reservada para ele (ninguém mais come). Depois de 10 s: a maçã some e a fome sobe 12. |
| Queda da maçã | `Mundo::evento_queda_maca` | Camada 1 = as 8 vizinhas da árvore. Sorteia uma livre; se não houver, tenta a camada 2, e assim por diante. Só cai em grama. |

## Testes (`testes/teste_ilha.py`)

Rodam sem tela, acelerando o tempo. Conferem: mapa e borda de água; 4 NPCs
com fome 100 e 6 macieiras; fome −3 a cada 5 s; maçãs só depois de 30 s e
em volta das árvores; morte aos 170 s sem comer, com o corpo ficando; comer
leva 10 s e só com fome < 70; ganho de 12 por maçã; física (uma coisa por
célula, NPC só em grama) em 50 ilhas × 1 hora simulada.

## Observado nos testes (para o dono do projeto avaliar)

- Nas 50 ilhas × 1 hora, dos 200 NPCs, **só 11 chegaram a comer** e **189
  morreram sem comer** (aos 170 s). Motivo: a terra tem 416 × 416, são só 6
  macieiras e o NPC enxerga 32 × 32; andando ao acaso, quase nunca chega
  perto de uma árvore antes de morrer. Não mudei nada: é o resultado das
  regras atuais.
- A ilha inteira na tela ocupa **1024 colunas × 513 linhas** (cada célula
  tem 2 colunas por causa dos emojis). Para ver tudo, o terminal precisa
  estar maximizado com a fonte bem pequena; senão aparece só o canto de cima
  à esquerda.

## Em aberto

- Símbolo do corpo: está **💀, provisório**, aguardando a escolha do dono.
- O corpo bloqueia a célula (segue "corpos têm física") — confirmar.
- Água não é andável — confirmar.
