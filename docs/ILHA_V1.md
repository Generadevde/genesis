# Primeira simulação: a ilha

Pedido e respostas do dono do projeto (14/11/2026).

## Definido

### Executável
- Abre direto na simulação, **sem menus**.
- Simulação em **tempo real** (por enquanto).

### Ilha
- **Quadrada**, **512 × 512** células (por enquanto).
- Chão de grama: `"` em **verde**. Água: `~` em **azul**.

### Macieiras 🌳
- **6 macieiras** espalhadas pela ilha.
- Cada macieira derruba **1 maçã por vez**, **em volta dela**.
- O tempo até a próxima queda é sorteado **entre 30 e 60 s**, **de novo a cada
  queda**, **em cada macieira separadamente**.
- **Sem limite** de maçãs no chão.
- **Maçã não estraga** (por enquanto).

### Maçãs 🍎
- Cada maçã recupera **12 de fome**.
- **Corpos têm física**: uma maçã não pode ficar dentro de outra.

### NPCs 🗿
- **4 NPCs**.
- **Fome** de **0 a 100**: 100 = satisfeito, 0 = faminto. **A cada 5 s, abaixa 3.**
- Fome chega a **0 → o NPC morre**.
- **Só come quando a fome está abaixo de 70.**
- Enxerga maçãs numa área de **32 × 32** células em volta dele.
- **Anda aleatoriamente, sem repetir por onde já foi.**

## Em aberto (perguntas ao dono do projeto)

1. Velocidade do NPC: quantas células por segundo?
2. A água: quantas células de água em volta da ilha de 512 × 512?
3. Fome inicial dos NPCs.
4. Tela: o terminal mostra só um pedaço do mapa de 512 × 512. Como mover a vista?
5. NPC morto: o corpo fica no mapa (com que símbolo) ou some?
6. Se todas as células em volta da macieira estiverem ocupadas, onde cai a maçã?
7. Comer: instantâneo ao chegar na maçã, ou leva tempo?
8. Os NPCs também não podem ocupar a mesma célula entre si, nem a da macieira?
