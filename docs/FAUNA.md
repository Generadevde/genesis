# Fauna: o molde dos animais e a galinha (motor 0.13)

Pedido pelo dono do projeto em 15/11/2026. Todos os números estão em
`motor/src/config.hpp` (`FichaEspecie` e `GALINHA`).

## O molde (vale para quase todos os animais)
Cada espécie tem uma **ficha** com os mesmos campos e os valores dela:
- **fome, sede e HP**, como os NPCs; **bebe água** do mesmo jeito (ao lado
  da água líquida, nunca do gelo);
- **explorar**, **velocidade** e **degrau** próprios;
- **visão** e **memória** (de onde bebeu) próprias;
- **dorme**; ainda **não sente frio**;
- **ocupa a célula** (tem física);
- **idade e velhice**; o corpo de quem morre sozinho **some em 3 dias**
  (os corpos dos NPCs continuam);
- **sexo e reprodução**, cada espécie do seu jeito;
- **bando** em umas espécies, em outras não.

## A galinha 🐔 / galo 🐓 / pintinho 🐤 / ovo 🥚

| O quê | Valor |
|---|---|
| No começo | 10 galinhas e 2 galos, em 2 bandos (1 galo + 5 galinhas), todos com 91 dias |
| Comer | cisca a grama (só grama; por enquanto não acaba): 10 s, +10 de fome |
| Beber | uma bicada na água: 5 s, +5 de sede |
| Fome / sede | 0 a 100; a cada 10 min caem 3 / 4; come abaixo de 70, bebe abaixo de 50 |
| Andar | 1 célula a cada 6 s (0,5 célula por 3 s); subir/descer 1 nível: 10 s |
| Explorar | destino a 1–7 células |
| Visão | 16 x 16 |
| HP | galinha 40, galo 60, pintinho 10; −5 por minuto por necessidade em 0; dormindo, +5 por minuto |
| Sono | das 19:00 às 05:00, onde estiver (o bando dorme junto) |
| Idade | pintinho de 0 a 90 dias; galinha/galo dos 90 dias aos 3 anos; morre com 3 anos |
| Bando | todos seguem o galo, a 3–5 células; os pintinhos seguem a mãe |
| Pintinho | até 60 dias anda com a mãe, sem fome nem sede; dos 60 aos 90 explora sozinho, com fome e sede; aos 90 vira galinha (entra no bando mais perto) ou galo (começa um bando) |
| Sozinha | a galinha sem bando procura um galo numa área de 64 x 64 |
| Namoro | galo e galinha juntos 15 min (❤️) → ela põe a ninhada inteira |
| Ovos | 1 por dia, de manhã (06:00–10:00), no ninho (onde pôs o 1º), e fica lá ~1 h; 12 a 14 ovos |
| Chocar | depois do último ovo, fica em cima deles 21 dias, com fome e sede travadas |
| Nascer | cada ovo tem uma chance (sorteada na ninhada, de 5% a 20%) de não prestar e sumir; o resto vira pintinho (meio a meio galo e galinha) |
| De novo | a galinha volta a pôr 60 dias depois de os pintinhos nascerem |
| Ovo estraga | se ficar mais de 1 dia sem contato com a galinha (no ninho) ou com um NPC |

## NPCs, ovos e carne

| O quê | Valor |
|---|---|
| Inventário (mão) | até **8 kg**, qualquer mistura |
| Abrigo | até **80 kg** |
| Pesos | maçã 0,5 kg; ovo 1 kg; pintinho 1 kg; galinha 4 kg; galo 5 kg |
| Comer ovo | 5 min, +30 de fome (não mexe na sede) |
| Comer carne | galinha ou galo: 30 min, +80; pintinho: 5 min, +10 |
| Carne | estraga em 2 dias no chão; na mão e no abrigo, não; só a de animal morto por NPC |
| Ordem para comer | o que carrega → um abrigo → o chão (maçã, ovo ou carne, o mais perto) → macieira lembrada → **caça** |

- Saciado, o NPC pega do chão (maçãs, ovos, carne) o que couber e leva ao
  abrigo dele, como já fazia com as maçãs.
- Tirar ovos de baixo da galinha que está chocando: ela **defende** com 80%
  de chance (revida até morrer).

## Caça, luta e fuga

| O quê | Valor |
|---|---|
| Quando o NPC caça | com fome e **sem nenhum outro recurso** |
| Ataque | um a cada 5 s, para todos; dano base: NPC 10, galinha 2, galo 2,5, pintinho 0 |
| Revidar (sorteado por ataque) | galo 20%; galinha com pintinhos (até 60 dias) ou chocando 80% — e luta **até um dos dois morrer** |
| Fugir | quem não revida foge; o **bando percebe e foge também** |
| Ariscos | com **todos** os NPCs, por 2 semanas a 3 anos (sorteado para cada um); fogem de NPC a 8–15 células (sorteado para cada um) |
| Morto por NPC | vira **carne** 🍗 |
| HP do NPC ferido | só volta dormindo no abrigo (como antes) |

## O bug da macieira (corrigido junto)
O NPC com fome que chega a uma macieira lembrada e não vê maçã agora a
marca como **vazia** e não volta lá por comida por **2 a 5 dias**
(sorteado). Antes ele ficava indo e voltando.

## Tela
- 🐔 galinha, 🐓 galo, 🐤 pintinho (metade do tamanho), 🥚 ovos (com `xN`),
  🍗 carne, 🐔 cinza = galinha morta; ❤️ em cima de quem está namorando e
  do abrigo onde um casal de NPCs está procriando.
- Aba nova **Fauna**: Animal, Espécie, Sexo, Idade, HP, Fome, Sede, Estado,
  Posição, Bando (clique seleciona).
- Painel do animal: idade, HP, fome/sede, estado, bando, mãe, ninhada,
  quando nascem os pintinhos, até quando fica arisco.
- O Histórico continua só de NPCs; entra "NPC 1 matou uma galinha".

## Como o código implementa

| Parte | Onde | Como |
|---|---|---|
| Ficha | `config.hpp` (`FichaEspecie`, `GALINHA`) | Todos os números da espécie. |
| Animal | `mundo.hpp` (`Animal`), `fauna.cpp` | Tudo pela **fila de eventos** (podem ser muitos): passo (`A_PASSO`), fome/sede (`A_NEC`), HP chegando a 0 (`A_SEM_HP`), idade (`A_CRESCE`), eclosão (`A_ECLOSAO`), ovos estragando, carne estragando, corpo sumindo. O HP é contínuo: valor num instante + taxa. |
| Decisão | `Mundo::evento_animal` | fim da ação → choca → dorme → luta → perigo (fugir) → pintinho segue a mãe → fome/sede → ninho → namoro (galo) → bando → explorar. |
| Caminho | `Mundo::buscar_animal` | Busca em largura dentro da visão (grama, água, ninho); no mapa todo para ir à água lembrada, guardando o caminho. |
| Ovos | `PilhaOvos` em `ovos_` | Todos os ovos de uma célula juntos; "com a galinha" ou "último contato" (+1 dia = estragam). |
| Inventário | `Inventario` (NPC e abrigo) | Contagem por tipo; o peso é calculado. |
| Caça | `tentar_cacar`, `lutar`, `animal_atacado`, `deixar_arisco` | Ver a tabela acima. |

## Decisões minhas (PROVISÓRIAS, aguardando o dono)
- **Dormindo, a fome e a sede da galinha não caem** (como o NPC no abrigo).
  Sem isso, com a sede caindo 4 a cada 10 min, ela chega a 0 no meio da
  noite e morre (40 de HP duram 8 minutos).
- **Comendo ou bebendo, continua até encher (100).** Sem isso ela volta a
  ter sede 10 min depois e refaz a viagem até a água o tempo todo (e o
  motor fica muito lento).
- **Fome e sede antes de pôr o ovo**; se passar das 10:00 sem conseguir
  pôr, fica para a manhã seguinte.
- **Com fome ou sede e nada à vista, explora livre do bando** (senão fica
  presa a 3–5 células do galo e morre de sede).
- Na idade adulta, o HP vai para o de adulto (cheio).
- O galo namora uma galinha de cada vez; só entram galinhas adultas, sem
  ninhada, sem pintinhos com menos de 60 dias, sem fome nem sede; o namoro
  começa a ninhada no dia seguinte.
- A galinha que **sai do ninho chocando** (fugiu ou lutou longe dele) larga
  a ninhada; se luta ao lado do ninho e ganha, volta a chocar. Se levam
  todos os ovos, ela para de chocar.
- **O NPC se defende**: se um animal está revidando contra ele, ele bate de
  volta (para o trabalho, se estava trabalhando).
- Comida na mão "sem ordem": come na ordem maçã, ovo, carne.
- O abrigo para comer é o dele; senão, o mais perto que ele vê.
- Pilha de ovos soltos: até 32 por célula (o mesmo das maçãs).
- Corpo/carne em cima de um ninho vai para a célula livre mais perto.
- Símbolos 🍗 (carne) e 🐔 cinza (galinha morta); tamanho do ❤️.
- "NPC matou uma galinha" entra no Histórico (é ação de NPC).
- Galinha morta por NPC = causa 16; NPC morto por animal = causa 16 também.

## Testes
`testes/teste_ilha.py`: `teste_macieira_vazia`, `teste_fauna_inicio`,
`teste_fauna_vida`, `teste_fauna_reproducao`, `teste_ovos_estragam`,
`teste_caca`, `teste_coracao`, `teste_fauna_desempenho`, e
`teste_macas_npc` (inventário por peso).
