# HP, urgência, dia/noite e temperatura (motor 0.8)

Pedido pelo dono do projeto em 14/11/2026.

## Regras

### HP (vida)
- **0 a 100**, começa em **100**. **Morre com HP 0** (só assim).
- Fome **0**, sede **0** e temperatura **abaixo de 4°** (fora do abrigo)
  são **críticos**: cada um tira **5 de HP por segundo**, e eles **se somam**
  (ex.: fome 0 + frio = 10/s).
- Fome e sede não matam mais direto: ficam em 0 e o HP cai.
- **Dormindo no abrigo**, recupera **1 de HP por segundo** (entra junto
  com o abrigo).

### Urgência e prioridade
- A prioridade vem das necessidades mais críticas; a **urgência** é o
  **tempo até a necessidade ficar crítica**: fome 0, sede 0 (e, com o
  abrigo, o tempo até o frio). A mais urgente é atendida primeiro.

### Dia, noite e temperatura
- Mesmo relógio do jogo (1 h = 60 s). **Dia das 06:00 às 18:00**; o resto é noite.
- O jogo começa no **Dia 1, às 06:00** (era 00:00; à 00:00 já faz 2°).
- Temperatura **uma só para o mundo todo** (por enquanto), mudando aos poucos:

| Hora | Temperatura |
|---|---|
| 22:00 → 04:00 | 2° (o mais frio) |
| 04:00 → 14:00 | sobe de 2° até 28° |
| 14:00 → 18:00 | 28° (o mais quente) |
| 18:00 → 22:00 | cai de 28° até 2° |

- **5° ou menos é frio** (o NPC vai para o abrigo — entra junto com o abrigo);
  **abaixo de 4° é crítico**.
- A tela **não escurece** à noite.
- A **barra de baixo** mostra a temperatura e se é dia ou noite.

## Como o código implementa

| Parte | Onde | Como |
|---|---|---|
| Números | `config.hpp` | `HP_*`, `DIA_COMECA_H`, `NOITE_COMECA_H`, `TEMP_HORAS`/`TEMP_GRAUS` (os 4 pontos da curva), `TEMP_FRIO`, `TEMP_CRITICA`. |
| Temperatura | `Mundo::temperatura_em` | Reta entre os pontos da tabela, dando a volta na meia-noite. |
| HP | `Mundo::taxa_hp`, `Mundo::avancar` | A taxa (−5 × críticos) é constante entre acontecimentos; o motor anda o HP junto com o tempo e trata "HP chegou a 0" como um acontecimento (morre). |
| Temperatura como acontecimento | `Mundo::proximo_marco_temperatura` | O próximo cruzamento de 4° ou 5° (e as 06:00) entra na conta do "próximo acontecimento", para o HP e as decisões mudarem na hora certa. O resultado fica guardado até passar. |
| Urgência | `Mundo::evento_passo`, `ate_critico_*` | Fome e sede baixas ao mesmo tempo: vai primeiro na que chega a 0 antes (fome/0,6 por s; sede/0,8 por s). |
| Causa da morte | `Npc::causa_morte` | O que estava crítico na hora: fome, sede, frio (pode ser mais de um). |

## Testes
`teste_relogio` (curva de temperatura e dia/noite), `teste_morte_sem_nada`
(sede 0 aos 125 s → morre aos 145 s, causa: sede) e `teste_hp` (−5/s por
crítico, somando; HP entre 0 e 100).

## Observado
Sem abrigo, quem estiver vivo às ~21:20 (quando cai abaixo de 4°) perde
5 HP/s e morre em 20 s. O abrigo vem nas próximas etapas.
