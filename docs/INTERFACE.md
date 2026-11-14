# Interface própria (pygame, estilo Windows 98)

Pedida pelo dono do projeto em 14/11/2026, substituindo a versão no
terminal (rich), que foi apagada.

## Decisões do dono do projeto

- Interface **própria**, feita em **pygame**.
- Estilo **retrô do Windows 98**: cinza, com menus e abas (referência: a
  tela de temas do Windows 98 que ele mandou).
- Janela **maximizada** (tela dele: 1920 × 1200, 60 Hz).
- Menu **Arquivo** com:
  - **Nova simulação** — começa o mundo de novo;
  - **Exibir** — mostra a tela com as informações (aba NPCs);
  - **Sair**.
- **Barra de status embaixo**, como estava no desenho.
- **Clique num caractere**: aparece **à direita** um painel com o **símbolo
  grande** e, embaixo, **o que ele é**.
- Símbolos continuam os mesmos: 🗿 NPC, 🌳 macieira, 🍎 maçã, ✝ corpo,
  `"` grama (verde), `~` água (azul); desde o mundo 3D, também `d` terra
  (marrom), `p` pedra (cinza) e `n` neve (branco).
- **Mundo 3D**: a tela mostra **um nível z por vez**; onde o nível é ar,
  aparece o que está abaixo, **com a cor normal**. A barra de status mostra o nível. Clicar numa célula
  mostra o material, a posição (x, y, z) e, na água, o nível (ex.: 10/10).
  Ver [MUNDO_3D.md](MUNDO_3D.md).

## Como a tela é montada

```
┌ Arquivo ───────────────────────────────────────────────┐ ← barra de menus
│ [Mapa][NPCs]                              ┌ Seleção ──┐ │ ← abas
│ ┌───────────────────────────────────────┐ │  ┌─────┐  │ │
│ │                                       │ │  │ 🗿  │  │ │ ← símbolo grande
│ │   aba Mapa: o mapa                    │ │  └─────┘  │ │
│ │   aba NPCs: a tabela                  │ │ NPC 1     │ │ ← o que é
│ │                                       │ │ Fome: 55  │ │
│ └───────────────────────────────────────┘ └───────────┘ │
│ [Tempo 01:15][Maçãs no chão: 6][Zoom 1x][Câmera (0,10)] │ ← barra de status
└─────────────────────────────────────────────────────────┘
```

## Controles

| Ação | Como |
|---|---|
| Selecionar algo | Clique (sem arrastar) no mapa, ou numa linha da tabela NPCs |
| Mover a câmera | Setas (segurar repete), ou **clicar e arrastar** o mapa |
| Zoom | `+` aproxima, `-` afasta (0,5x, 1x, 2x, 4x, 6x), ou a **rodinha** do mouse (para cima aproxima). `=` e o `+`/`-` do teclado numérico também servem |
| Descer / subir de nível z | `Z` / `X` |
| Trocar de aba | Clique na aba, ou tecla `I` |
| Menu | Clique em Arquivo; `Esc` fecha |

O painel de um NPC mostra também a **memória** dele (macieiras lembradas e
quantos pontos de água), e a aba NPCs tem as colunas "Macieiras lembradas"
e "Águas lembradas" (ver [MEMORIA.md](MEMORIA.md)).

A seleção de um NPC acompanha o NPC enquanto ele anda (e vira o corpo ✝ se
ele morrer: o painel mostra "Morreu de fome" ou "Morreu de sede").

### Arrastar × clicar
No mapa, a seleção acontece ao **soltar** o botão. Se, com o botão
apertado, o mouse andou pelo menos 1 célula da tela, foi um **arraste**: a
câmera se move junto ("pega e puxa": arrastar para a esquerda mostra o
que está à direita) e nada é selecionado. Senão, foi um **clique**.

## Arquivos

| Arquivo | O que tem |
|---|---|
| `interface/aparencia.py` | **Todas** as cores e tamanhos. Cores = paleta padrão do Windows 98. Tamanhos marcados **PROVISÓRIO**. |
| `interface/win98.py` | Peças de desenho estilo Windows 98, sem saber nada da simulação: relevo 3D, barra de menus, abas, tabela. |
| `interface/principal.py` | A janela do Genesis: monta a tela, lê mouse e teclado, conversa com o motor. |

### O relevo do Windows 98
Cada caixa tem 2 pixels de borda: clara em cima/à esquerda e escura
embaixo/à direita (saliente, como um botão), ou o contrário (afundada, como
um campo). É isso que dá o visual "3D" cinza. Está em `win98.relevo`.

### Emojis
A fonte Noto Color Emoji (vem com o Mint) só existe num tamanho; a interface
desenha o emoji nele e redimensiona para o tamanho da célula, guardando o
resultado para não refazer a cada quadro (`Simbolos` em `principal.py`).

### Janela maximizada (cuidado!)
Para maximizar, a interface usa o objeto `Window` do pygame. Esse objeto
**precisa ficar guardado** (`self._janela`): se o Python o descartar, o
pygame destrói a janela junto e o programa cai com "falha de segmentação"
(aconteceu em 14/11/2026). A cada quadro a interface também pega a
superfície atual da janela (`pygame.display.get_surface()`), porque ela
muda quando a janela muda de tamanho. O `testes/teste_interface.py`
confere isso.

### Tempo real
A cada quadro, o pygame mede quanto tempo passou e o motor anda exatamente
isso (`mundo.avancar(dt)`). 60 quadros por segundo = taxa do monitor do dono.

## Valores PROVISÓRIOS (aguardando o dono do projeto)

Estão em `interface/aparencia.py`:

| Nome | Valor | O que é |
|---|---|---|
| `TAM_FONTE_UI` | 14 | tamanho do texto de menus, abas, tabela e status |
| `TAM_CELULA` | 24 | lado de 1 célula do mapa em pixels (zoom 1x) |
| `LARGURA_PAINEL` | 280 | largura do painel da direita |
| `TAM_SIMBOLO_GRANDE` | 128 | tamanho do símbolo grande no painel |
| `ESPACO` | 6 | folga interna de barras, abas e tabela |
| `REPETIR_TECLA_ESPERA_MS` | 300 | segurando a seta: espera antes de repetir |
| `REPETIR_TECLA_INTERVALO_MS` | 50 | segurando a seta: intervalo entre repetições |

Também provisórios: a moldura **branca fina** em volta da célula
selecionada, e o tom das cores do mapa (verde `#008000` e azul `#0000FF`,
da paleta de 16 cores do Windows 98).
