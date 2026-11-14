# Memória dos NPCs (motor 0.3)

Pedida pelo dono do projeto em 14/11/2026: o NPC lembra onde fica uma
macieira e a água; quando fica com fome (ou sede), volta lá.

## Regras definidas pelo dono do projeto

- **Aprende só onde usou**:
  - a **macieira** de onde caiu a maçã que ele **comeu**;
  - a **célula de água** onde ele **bebeu**.
- Lembra **todos** esses lugares.
- **Nunca esquece.**
- Com fome, volta à macieira lembrada. **Se chegar lá e não houver maçã,
  vai para outra** macieira que lembra.
- Com sede, volta à água lembrada.

## Como o código implementa

| Regra | Onde | Como |
|---|---|---|
| Saber de qual árvore veio a maçã | `Mundo::evento_queda_maca` | Cada célula com maçã guarda o número da macieira de onde ela caiu (`origem_maca_`). |
| Aprender a macieira | `Mundo::evento_fim_comer` | Ao terminar de comer, adiciona a macieira de origem em `macieiras_lembradas` (sem repetir). |
| Aprender a água | `Mundo::evento_fim_beber` | Ao terminar de beber, adiciona a célula de água em `aguas_lembradas` (sem repetir). |
| Nunca esquecer | — | Nada no código apaga essas listas. |
| Ver vem antes de lembrar | `tentar_comer` / `tentar_beber` | Primeiro procura maçã/água na área 32 × 32. Só se não vir nada usa a memória. |
| Voltar à macieira | `Mundo::ir_a_macieira_lembrada` | Vai para a macieira lembrada mais próxima, um passo por segundo. |
| Sem maçã → outra | `Mundo::ir_a_macieira_lembrada` | Chegou ao lado da árvore e não vê maçã: marca como "vista vazia" e escolhe a próxima lembrada mais próxima. Ao comer, a lista de vazias é zerada. |
| Voltar à água | `Mundo::ir_a_agua_lembrada` | Vai para a água lembrada mais próxima. Ao chegar, já a vê e bebe. |

O NPC indo a um lugar lembrado aparece como **"indo comer (memória)"** ou
**"indo beber (memória)"**.

## Na tela

- **Painel da direita** (NPC selecionado): lista as macieiras lembradas
  (posição de cada uma) e quantos pontos de água ele lembra.
- **Aba NPCs**: colunas "Macieiras lembradas" e "Águas lembradas".

## Testes (`testes/teste_ilha.py`, `teste_memoria`)

Em 50 ilhas × 30 minutos simulados conferem: as listas só crescem (nunca
esquece); não há repetidos; água lembrada é água de verdade; uma macieira
nova só entra depois de comer e uma água nova só depois de beber; quando
vai por memória, o alvo é mesmo um lugar lembrado.

## Observado nos testes (para o dono do projeto avaliar)

50 ilhas × 1 hora, 200 NPCs: **1 vivo no fim**; **120 morreram de sede** e
**79 de fome**; metade morreu antes de **169 s**.

- **Sede**: a memória só ajuda depois do primeiro gole. Quem nasce longe do
  mar (mais de 16 células) não vê a água e morre aos 125 s sem nunca ter
  bebido — então não tem o que lembrar.
- **Fome**: falta comida na ilha. As 6 macieiras derrubam, em média, uma
  maçã a cada 45 s cada uma ≈ 8 maçãs por minuto ≈ 96 de fome por minuto.
  Os 4 NPCs gastam 36 por minuto cada = 144 por minuto. Mesmo comendo tudo,
  não dá para os 4.
- Não mudei nada: é o resultado das regras atuais.

## Em aberto

- **Todas as lembradas vazias**: hoje ele zera a lista de "vistas vazias",
  anda à toa por 1 passo e, no passo seguinte, recomeça a ronda pela
  macieira lembrada mais próxima — PROVISÓRIO, aguardando o dono.
