# Vida vegetal (motor 0.7)

Pedida pelo dono do projeto em 14/11/2026. Usa o [sistema de tempo](TEMPO.md)
(1 hora do jogo = 60 s).

## Regras definidas pelo dono do projeto

| Fase | Duração | O que acontece |
|---|---|---|
| Maçã no chão 🍎 | **6 horas** sem ninguém pegar | vira **muda ♣** (verde-claro) na mesma célula |
| Muda ♣ | **3 dias** | vira **macieira** 🌳 |
| Macieira 🌳 | **7 dias** | derruba maçãs normalmente (a cada 30–60 s); depois **seca** |
| Macieira seca (cinza) | **4 horas** | **não derruba maçãs**; depois **some** (a célula fica livre) |

- A **muda ocupa a célula** e **só fica lá** (ninguém faz nada com ela).
- A **macieira que nasce da muda** é igual às outras (derruba maçãs, envelhece).
- As **6 macieiras do começo nascem com idade 0** (então secam juntas no dia 8).
- **Maçã pegada e solta de novo** (ex.: caiu do NPC que morreu) **recomeça**
  as 6 horas.
- **Memória**: o NPC vai à macieira que lembra; se chegar lá e ela **não
  existir mais**, ele **esquece** dela. (Macieira seca ainda existe: ele
  só passa para a próxima, como quando não há maçã.)

## Como o código implementa

| Parte | Onde | Como |
|---|---|---|
| Números | `config.hpp` | `MACA_VIRA_MUDA_H = 6`, `MUDA_VIRA_ARVORE_DIAS = 3`, `MACIEIRA_VIDA_DIAS = 7`, `MACIEIRA_SECA_H = 4` — convertidos para segundos com `SEGUNDOS_POR_HORA` / `SEGUNDOS_POR_DIA`. |
| Ocupantes novos | `mundo.hpp` | `Muda` e `MacieiraSeca`. |
| Contar as 6 h | `por_maca_no_chao` / `tirar_maca_do_chao` | Toda maçã que vai para o chão marca a hora em que vira muda; toda maçã que sai do chão (pegada ou comida) apaga essa marca. |
| Maçã sendo comida | `evento_maca_vira_muda`, `morrer` | Se, na hora de virar muda, alguém está comendo a maçã, ela não vira: ou é comida, ou (se quem comia morrer) vira muda nessa hora. |
| Muda → macieira | `evento_muda_vira_arvore` | A muda sai e entra uma macieira nova com idade 0 (`nova_macieira`). |
| Secar e sumir | `evento_macieira_seca` / `evento_macieira_some` | Seca: vira `MacieiraSeca` e para de derrubar maçãs. Some: célula livre; a macieira continua na lista com `existe = false` (os números das outras não mudam — a memória dos NPCs e a origem das maçãs usam esses números). |
| Esquecer | `ir_a_macieira_lembrada` | Chegando ao lado de uma macieira lembrada que não existe mais, tira ela da memória. |
| Só lembra o que existe | `lembrar_macieira` | Pegar uma maçã de uma macieira que já sumiu não cria lembrança. |

### Desempenho (sem mudar regras)
Com a vida vegetal a ilha pode ter milhares de plantas. Para não ficar
lento:
- **Fila de acontecimentos**: as plantas guardam a **hora** em que algo vai
  acontecer, numa fila ordenada; o motor pega sempre o mais cedo, sem olhar
  planta por planta.
- **Camadas pela borda**: ao procurar onde a maçã cai, cada camada em volta
  da árvore é percorrida só pela borda.
- **Conta de células livres**: o motor sabe quantas células de terra estão
  livres; com a ilha lotada, não procura lugar à toa.

Resultado: 21 dias simulados em ~2,6 s.

## Na tela

- **Muda** `♣` verde-claro; **macieira seca** 🌳 em cinza.
- Painel da direita: **macieira** (idade, "seca em" / "some em"),
  **muda** (idade, "vira macieira em"), **maçã** ("vira muda em").
- Barra de baixo: "Muda", "Macieira seca".

## Testes (`teste_vegetal`)

Uma maçã esquecida vira muda exatamente em 6 h; a muda vira macieira
(idade 0) exatamente em 3 dias; as 6 macieiras do começo secam exatamente
aos 7 dias, ficam 4 h secas e somem.

O "esquecer macieira que sumiu" não aparece nos testes automáticos: hoje
os NPCs morrem em poucos minutos e nenhum chega vivo ao dia 8.

## Observado (para o dono do projeto avaliar)

Numa ilha de teste, como os NPCs morrem cedo e ninguém pega as maçãs, a
ilha **lota de plantas**: dia 4 → 573 mudas; dia 8 → 8.412 mudas e 1.426
macieiras; dia 11 → ~9.800 macieiras (quase toda a terra). Depois disso
quase não há lugar para maçã cair. Não mudei nada: é o resultado das
regras atuais (não há limite).
