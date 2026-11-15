# Tempo, calendário e estações (motor 0.12)

Pedido pelo dono do projeto em 15/11/2026. **Substitui por completo** o
sistema de 14/11 (em que 1 hora do jogo durava 60 s e o ano tinha 14 dias).
Todos os números estão em `motor/src/config.hpp`.

## Escala
- **1 segundo do jogo = 1 segundo real** (na velocidade normal).
- 1 minuto = 60 s · 1 hora = 60 min · 1 dia = 24 h.
- Ano de **365 dias**, com **12 meses de verdade** e sem ano bissexto:

| Mês | Dias | Mês | Dias | Mês | Dias |
|---|---|---|---|---|---|
| Janeiro | 31 | Maio | 31 | Setembro | 30 |
| Fevereiro | 28 | Junho | 30 | Outubro | 31 |
| Março | 31 | Julho | 31 | Novembro | 30 |
| Abril | 30 | Agosto | 31 | Dezembro | 31 |

- A semana tem 7 dias (fica só como referência; nada usa ainda).
- A simulação começa em **01/01/0001, às 07:00**. Depois do ano 99 vem o 100.

## Velocidades (botões da barra de baixo)

| Botão | Velocidade |
|---|---|
| `\|\|` | pausa (também a tecla **Espaço**: pausa / continua) |
| `>` | normal: 1 s do jogo por segundo |
| `>>` | 1 minuto por segundo |
| `>>>` | 1 hora por segundo |
| `>>>>` | 1 dia por segundo |

Se o computador não der conta (muitos NPCs), o jogo anda o mais rápido que
conseguir e a barra de baixo mostra a **velocidade real** (decisão do dono).
Hoje, com 6 NPCs, 1 dia do jogo leva cerca de 0,25 s.

## Estações
Cada uma tem 3 meses (os meses contados de 1 a 12):

| Estação | Meses | O que acontece | Dia e noite |
|---|---|---|---|
| Verão | 12, 1, 2 | as macieiras dão frutos | dias mais longos (até **13 h 30** de sol no meio do verão; noite de 10 h 30) |
| Outono | 3, 4, 5 | folhas secas | dia = noite (12 h) |
| Inverno | 6, 7, 8 | repouso e frio; **a água dos vales congela** | dias mais curtos (até **11 h** de sol no meio do inverno; noite de 13 h) |
| Primavera | 9, 10, 11 | as macieiras florescem; o gelo derrete | dia = noite (12 h) |

- No verão e no inverno o dia muda **aos poucos, em curva**, com o pico no
  meio da estação, e volta a 12 h no fim dela.
- O dia fica **centrado no meio-dia**: com 13 h 30 de sol, o sol nasce às
  05:15 e se põe às 18:45.
- A barra de baixo mostra **data, hora e estação**:
  `15/01/0001 13:00:00 — Verão`.

## Temperatura (média do mundo; um dia vai depender de região e bioma)

| Estação | Mínima | Máxima |
|---|---|---|
| Verão | 20° | 35° |
| Outono | 18° | 28° |
| Inverno | 4° | 18° |
| Primavera | 20° | 30° |

- Esses valores valem **no meio** de cada estação; entre uma e outra a
  mínima e a máxima mudam **aos poucos, em curva**.
- Ao longo do dia, a forma é a de antes, **presa ao nascer e ao pôr do sol**:
  mínima 2 h antes de o sol nascer → sobe até a máxima 8 h depois de nascer
  → fica na máxima até o pôr do sol → cai até a mínima 4 h depois do pôr →
  fica na mínima até a manhã seguinte. Com o dia de 12 h isso dá exatamente
  a curva antiga (04h, 14h, 18h, 22h).
- O frio (5° ou menos: vai ao abrigo) e o frio crítico (abaixo de 4°: perde
  HP) continuam iguais. Como a mínima do inverno é 4°, **hoje ninguém sofre
  com o frio** — o dono vai criar outra necessidade para os abrigos.
- O abrigo continua com o horário antigo: sai às **06:00** (o dono vai mudar
  isso depois).

## Tabela de tempos (todos do dono)

| O quê | Tempo |
|---|---|
| Fome cai 3 | a cada 15 min |
| Sede cai 4 | a cada 10 min |
| Comer uma maçã (+12 fome, +5 sede) | 3 min |
| Beber um gole (+15 sede) | 20 s |
| Dano por necessidade crítica | −5 HP por minuto (somando) |
| Recuperar dormindo no abrigo | +1 HP por minuto |
| Andar 1 célula | 5 s |
| Subir/descer 1 nível | 10 s |
| Tirar lascas (+2) | 15 a 20 min |
| Tirar galhos (+1) | 5 a 15 min |
| Fazer o machado (6 lascas + 2 galhos) | 1 a 3 h (onde ele estiver) |
| Cortar árvore (+30 lenha) | 2 a 6 h |
| Construir abrigo (−20 lenha) | 2 a 4 dias (continua de noite) |
| Procriar (juntos no abrigo) | 1 h |
| Gravidez | 9 meses (do calendário) |
| Intervalo entre filhos | 2 meses (contados do nascimento) |
| Bebê vira adulto | 15 anos (continua bebê até lá, por enquanto) |
| Idade inicial / velhice | 20 anos / 100 anos |
| Idade para ter filhos | de 16 até 45 (mulheres) / 60 (homens) |
| Maçã esquecida no chão | 6 h: 40% vira muda, 60% apodrece e some |
| Muda vira macieira | 3 anos |
| Macieira vive | 40 anos; depois seca 1 ano e some |
| Água escorre 1 nível | a cada 5 s |

As faixas ("15 a 20 min") são sorteadas **a cada vez**.
As idades contam pelo **aniversário** (ano sempre de 365 dias) e os "meses"
da gravidez e do intervalo contam pelo **calendário** (engravidou em 10/03,
nasce em 10/12).

## Macieiras e maçãs (detalhes em [VIDA_VEGETAL.md](VIDA_VEGETAL.md))
- Primavera floresce, verão dá frutos, outono folhas secas, inverno repouso
  (aparece só no painel da direita).
- **A maçã só cai no verão** (o relógio antigo de 30–60 s foi apagado): cada
  macieira derruba **de 80 a 300** por verão, começando no **2º dia** do
  verão, poucas no começo, mais e mais até o **pico no meio**, depois menos,
  até o último dia.
- No chão as maçãs se juntam em **pilhas de até 32** na mesma célula; o mapa
  mostra um pequeno **`xN`** com a quantidade.
- Cada maçã tem as **suas** 6 horas (estão juntas, mas são separadas).

## Maçãs, NPCs e abrigo (detalhes em [CARREGAR.md](CARREGAR.md))

> Desde o motor 0.13 o limite é por **peso**: 8 kg na mão e 80 kg no abrigo (ver [FAUNA.md](FAUNA.md)).
- Saciado, o NPC pega **todas** as maçãs da pilha que couberem na mão
  (**até 16**), na hora, e **leva para o abrigo** dele (guardar é na hora).
- O abrigo guarda **até 300** maçãs; lá (e na mão) elas **não estragam**.
- Abrigo cheio: ele deixa as que carrega **no chão, onde está**, e não pega
  mais.
- Com fome, come **primeiro as que carrega**, depois as de **um abrigo**
  (qualquer um pode comer de qualquer abrigo), depois as do chão.

## Gelo (detalhes em [MUNDO_3D.md](MUNDO_3D.md))
- No começo do **inverno** (01/06), a **camada de cima** da água dos **vales**
  congela e vira **`g` azul-claro**. O mar não congela.
- Dá para **andar por cima**; **não dá para beber**.
- Derrete no começo da **primavera** (01/09) e fica derretido na primavera,
  no verão e no outono.

## Como o código implementa

| Parte | Onde | Como |
|---|---|---|
| Números | `config.hpp` | Tudo acima. |
| Calendário | `Mundo::data_de`, `tempo_de`, `somar_meses` | O tempo é contado em segundos desde 01/01/0001 07:00. Somando as 7 h, divide-se por 86 400 (dias) e por 365 (anos); o mês sai da tabela de dias. Antes do começo (os NPCs iniciais nasceram 20 anos antes) o ano fica 0 ou negativo. |
| Estações | `estacao_de`, `limites_estacao`, evento `ESTACAO` | Cada começo de estação é um evento na fila; quando acontece, registra no Histórico, faz o que a estação traz (verão: sorteia as maçãs; inverno: congela; primavera: derrete) e se agenda para o ano seguinte. |
| Sol | `horas_de_sol`, `nascer_do_sol`, `por_do_sol` | Calculado ao meio-dia de cada dia, com a curva sen² (pico no meio da estação). |
| Temperatura | `min_max_temperatura`, `temperatura_em` | Mínima/máxima dos meios das estações, ligadas pela curva (1 − cos)/2; a forma do dia é feita de retas presas ao sol. |
| Quedas de maçã | `comecar_verao`, `gerar_quedas`, evento `QUEDA_MACA` | No começo do verão, cada macieira sorteia quantas e as horas de cada queda (com chance proporcional à curva); a fila só guarda a próxima. |
| Pilhas | `por_maca_no_chao`, `tirar_maca_do_chao`, `lugar_para_maca`, evento `MACA_VENCE` | Cada maçã do chão tem número, célula, origem e hora de vencer; a célula guarda a lista das suas (a pilha). |
| Velocidade | `principal.py` (`andar_tempo`) | Cada quadro anda o motor em pedaços até gastar `ORCAMENTO_MOTOR_MS`; mede a velocidade de verdade. |
| Desempenho | `procurar_recurso` | Antes de calcular caminhos (caro), vê se há alguma pedra/árvore à vista ou lembrada. Isso levou 1 dia do jogo de 8 s para 0,25 s. |

## Decisões minhas (PROVISÓRIAS, aguardando o dono)
- A **forma da curva** do sol e das quedas de maçã: sen² (sobe devagar,
  pico no meio, desce devagar). A da temperatura entre estações:
  (1 − cos)/2.
- O **meio da estação** é o meio exato em tempo (ex.: verão de 01/12 a
  01/03 → meio em 15/01 00:00).
- A forma do dia da temperatura **presa ao sol** (mínima 2 h antes de
  nascer, etc.), para manter a curva antiga quando o dia tem 12 h.
- "Vale" = água **dentro da terra** (fora da borda de 8 células do mar).
  Os vales ligados ao mar também congelam (só a parte de dentro da terra).
- Quem está em cima do gelo quando ele derrete vai para a **terra livre mais
  perto**. No gelo não se põe nada (maçã, bebê, corpo, obra): o abrigo é
  construído na terra livre mais perto.
- Macieira que nasce de uma muda **no meio do verão** só dá maçãs no verão
  seguinte; ao abrir uma simulação no meio do verão, só cai o que faltava
  cair desse verão.
- A idade da macieira (40 anos) conta de quando ela **virou árvore** (as do
  começo já têm 2 anos de árvore).
- Casal: **16 anos** ou mais (a idade de ter filhos).
- "Até 45 anos" = enquanto ainda tem 45 anos completos.
- Abrigo para comer: o **dele** (se tem maçãs); senão, o mais perto que ele
  **vê**.
- Maçã que estava sendo comida quando o NPC morre cai no chão com as da mão.
- Os nascidos antes do começo aparecem no painel como "antes do ano 1".
- Tecla **Espaço** = pausa; cor do gelo; tamanho do `xN`; quanto tempo por
  quadro o motor pode gastar (12 ms).

## Testes
`testes/teste_ilha.py`: `teste_calendario` (datas, meses, estações, sol,
temperatura), `teste_quedas` (fome/sede), `teste_morte_sem_nada` e
`teste_hp` (−5/min), `teste_verao` (80–300 maçãs, pico no meio, pilhas,
40% muda), `teste_vegetal` (3 anos, 40 anos, 1 ano), `teste_gelo`,
`teste_acoes` (3 min / 20 s), `teste_macas_npc` (16 na mão, 300 no abrigo,
ordem para comer), `teste_trabalho` (faixas de tempo), `teste_noite_no_abrigo`
(inverno), `teste_familia` (9 meses, 2 meses, 15 anos, 16–45/60),
`teste_velhice` (100 anos), `teste_relevo_npc` (5 s / 10 s),
`teste_desempenho` (1 dia em menos de 1 s).

Coisas que levam meses ou anos são testadas com ganchos só de teste
(`teste_saltar`, `teste_idade`, `teste_adiantar_parto`,
`teste_idade_macieira`, `teste_abrigo`, `teste_macas`).

`testes/teste_interface.py`: botões `|| > >> >>> >>>>`, Espaço, barra com
data e estação, desenho das pilhas.
