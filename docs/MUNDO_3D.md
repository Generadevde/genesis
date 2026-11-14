# Gerador de mundos e mundo em 3D (motor 0.4)

Pedido pelo dono do projeto em 14/11/2026: a base sólida do gerador de
mundos, com as 3 dimensões **x, y, z**. Tamanho continua 128 × 128.

## Regras definidas pelo dono do projeto

### Dimensões
- **x** e **y**: 128 × 128 (como antes).
- **z**: de **−64 a +64**; **z = 0 é o nível do mar**.

### Materiais
| Material | Símbolo | Cor |
|---|---|---|
| Grama | `"` | verde |
| Neve | `n` | branco |
| Areia | `a` | amarelo claro |
| Terra | `d` | marrom |
| Pedra | `p` | cinza |
| Água | `~` | azul |
| Ar | (vazio) | — |

### Camadas (leitura A)
- **Terra** (o meio da ilha), desde 14/11/2026 com **planícies, morros e vales**:
  - **40% planície**, no **+1** (PROVISÓRIO: era o chão mais baixo antes dos vales);
  - **30% morro**, subindo até o **+4**;
  - **30% vale**, descendo até o **−4**.
  - O **topo é grama** — **neve** se o topo está em **+4**; **pedra exposta**
    se o fundo do vale fica abaixo do −3 (no −4). Abaixo do topo, **terra
    até o −3**; dali para baixo, **pedra**.
- **Vales e água**: um vale abaixo do nível do mar **enche de água só se
  estiver ligado ao mar**; pode encostar na costa (e o mar entra). Vale
  fundo isolado fica **seco**.
- **Areia** (pedido em 14/11/2026): o chão cujo topo fica **no nível 0** e
  **encosta no mar** (ou num vale alagado ligado a ele) vira **areia**.
  "Encostar" = alguma das 8 colunas vizinhas tem água no topo, no nível 0
  (PROVISÓRIO: as diagonais contam). Era o "bug" da terra no nível 0 ao lado
  da água: não era água faltando — a célula 0 é chão, na altura da água.
- **Mar** (a borda de 8 células, que continua): **água do 0 até o −10**;
  abaixo, **pedra**.
- Relevo **"médio"**. Mais pra frente vira opção editável.
- Cada "Nova simulação" gera um mundo **aleatório**.

Exemplo de coluna com morro de altura +3:
```
 +4  (ar)
 +3  "  grama   (topo)
 +2  d  terra
  …  d  terra
 -3  d  terra
 -4  p  pedra  … até -64
```

### Água (física)
- Cada célula de água tem **nível de 1 a 10** (10% cada; 10 = cheia).
- A água **sempre escorre para baixo** e **se espalha** para os lados.
- Ela se move **a cada 3 s** (valor de teste).

### NPCs
- Ficam **só na superfície**.
- Conseguem **subir ou descer 1 nível**; isso **leva 5 s** (em vez de 1 s).

### Tela
- **Z** desce um nível, **X** sobe um nível.
- O que está **abaixo** do nível visto aparece **com a cor normal**
  (antes ficava apagado; o dono do projeto pediu para deixar normal em
  14/11/2026).

## Como o código implementa

| Parte | Onde | Como |
|---|---|---|
| Volume do mundo | `motor/src/mapa3d.hpp/.cpp` (`Mapa3D`) | Um material e um nível de água para cada célula (x, y, z): 128 × 128 × 129 ≈ 2,1 milhões de células. Guarda também a **superfície** de cada coluna (z da célula mais alta que não é ar). |
| Gerador — relevo | `motor/src/gerador.hpp/.cpp` (`gerar_mundo`) | Mapa de alturas por **ruído suave** (valores sorteados numa grade espaçada, misturados suavemente entre os pontos, somando camadas de detalhe). A terra é **ordenada pelo valor do ruído**: os 30% mais baixos viram vale, os 30% mais altos viram morro, o meio vira planície — assim as porcentagens saem exatas. Dentro do vale, quanto mais baixo o ruído, mais fundo (até −4); dentro do morro, quanto mais alto, mais alto (até +4). Depois monta cada coluna com as camadas acima. |
| Gerador — vales alagados | `gerar_mundo` (parte 3) | Partindo da água do mar, preenche com água todo **ar** que ela alcança indo para os lados ou para baixo, do nível 0 para baixo. Feito na criação, o mundo já nasce em equilíbrio. |
| Água — cair | `Mapa3D::passo_agua` | De baixo para cima: cada célula de água passa para a de baixo tudo o que couber (se a de baixo é ar ou água não cheia). Indo de baixo para cima, cai **1 nível por passo**. |
| Água — espalhar | `Mapa3D::passo_agua` | Depois de cair, a água que sobrou passa **1 nível** para cada vizinho do mesmo z (norte, sul, leste, oeste) que tenha **pelo menos 2 níveis a menos**. A ordem é sorteada a cada passo, para não puxar sempre para o mesmo lado. Célula que acabou de receber água não repassa no mesmo passo. |
| Água nunca some | `Mapa3D::passo_agua` | Só muda água de lugar; o teste confere a soma total. |
| Água parada | `Mapa3D::agua_parada_` | Se num passo nada se moveu e ninguém mexeu no mapa, os próximos passos são pulados (o mar parado não gasta tempo). Não muda o resultado. |
| Passo da água a cada 3 s | `Mundo::avancar` | Mais um relógio de eventos, como os da fome e da sede. |
| NPC na superfície | `Mundo::com_z`, `Mundo::livre` | O z de NPCs, árvores, maçãs e corpos é o z da grama do topo da coluna. "Livre" = coluna de terra firme (não água) sem ninguém em cima. |
| Degrau de 1 nível | `Mundo::pode_pisar` | Só passa para a coluna vizinha se a diferença de altura for no máximo 1. |
| Subir leva 5 s | `Mundo::mover` | Se o passo mudou de nível, o próximo passo vem 5 s depois (em vez de 1 s). |
| Beber com relevo | `procurar_agua`, `tentar_beber` | Bebe da coluna vizinha se a água estiver no nível dele ou até 1 abaixo. |
| Visão de uma camada | `Mundo::visao(z)` | Para cada coluna, desce a partir do nível z até achar algo que não é ar; diz o material, se está abaixo (apagado) e se o ocupante aparece. |

## Testes (`testes/teste_ilha.py`)

- `teste_mapa`: camadas de toda coluna (mar e terra), alturas de +1 a +4.
- `teste_relevo_npc`: degraus de no máximo 1 nível; depois de subir ou
  descer, o próximo passo só vem 5 s depois.
- `teste_agua`: o mar parado não muda; num buraco cavado à mão, 10 de água
  soltos no ar caem 1 nível a cada 3 s, chegam ao fundo e se espalham;
  a quantidade total nunca muda.

## Observado nos testes (para o dono do projeto avaliar)

> Com planícies (+1) e vales (14/11/2026): em 50 ilhas × 1 hora,
> **141 de 200 NPCs beberam** (eram 32), **76 comeram**, **1 vivo no fim**.
> O texto abaixo é de antes das planícies.

- **Quase ninguém consegue beber.** O mar fica no nível 0 e o NPC só bebe
  se estiver no nível da água ou 1 acima. Numa ilha de teste, das **444**
  células de terra na costa, só **83** estão em +1; o resto é barranco
  (+2 a +4). Em 50 ilhas × 1 hora: **32 de 200 NPCs beberam**, **0 vivos no
  fim**, **175 morreram de sede** e **25 de fome**.
- Como quem bebe fica perto do mar (e continua vendo a água), a ida à
  **água lembrada** não aconteceu nesses testes.
- Não mudei nada: é o resultado das regras atuais.

## Valores PROVISÓRIOS (aguardando o dono do projeto)

| Onde | Nome | Valor | O que é |
|---|---|---|---|
| `config.hpp` | `RELEVO_TAMANHO_MORRO` | 24 | tamanho médio de um morro, em células |
| `config.hpp` | `RELEVO_CAMADAS` | 3 | camadas de detalhe somadas no relevo |
| `config.hpp` | `PLANICIE_Z` | +1 | nível das planícies |
| `aparencia.py` | `COR_TERRA` | (170, 85, 0) | o marrom da terra |
| `principal.py` | nível inicial da vista | +4 | a tela abre vendo do topo dos morros |
| `principal.py` | ordem no zoom afastado | … grama = neve > água > terra > pedra > ar | onde entram neve, terra e pedra |
| `mundo.cpp` | macieiras na neve | permitido | macieiras e NPCs podem nascer em cima da neve |
| `mapa3d.cpp` | vizinhos da água | 4 (N, S, L, O) | a água se espalha só em cruz, não na diagonal |
| `mapa3d.cpp` | regra de espalhar | diferença ≥ 2, 1 nível por vizinho | quando e quanto a água passa para o lado |
| `teste_definir` | — | — | função **só para testes** (cavar e despejar água à mão) |
