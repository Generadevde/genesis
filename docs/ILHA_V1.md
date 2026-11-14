# Primeira simulação: a ilha

Pedido do dono do projeto (14/11/2026).

## Definido

- Um executável para acessar, **sem menus**: abre direto na simulação.
- Uma **ilha**.
- **4 NPCs** na ilha.
- **6 macieiras** espalhadas.
- Variável **fome**, de **0 a 100**. **A cada 5 s, abaixa 3.**
- Maçãs **caem das macieiras a cada 30 a 60 s**.
- **Cada maçã recupera 12 de fome.**

## Em aberto (perguntas ao dono do projeto)

1. Tamanho da ilha (largura × altura do mapa) e formato.
2. Fome: 100 é satisfeito e 0 é faminto? O que acontece em 0?
3. Quando o NPC vai atrás de maçã: sempre, ou só abaixo de certa fome?
4. Como o NPC se move: velocidade, e o que faz enquanto não está com fome.
5. Queda das maçãs: o intervalo de 30 a 60 s é sorteado de novo a cada queda,
   em cada macieira separadamente? Cai uma maçã por vez? Onde ela cai?
   Existe limite de maçãs no chão?
6. Maçã no chão estraga com o tempo?
7. Velocidade da simulação: tempo real, ou com opção de acelerar/pausar?
8. Símbolos Unicode do mapa: NPC, macieira, maçã, terra, água.
