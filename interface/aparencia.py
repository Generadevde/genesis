"""
aparencia.py — TODOS os valores visuais da interface, num lugar só.

Duas origens:
  * CORES DO WINDOWS 98: são a paleta padrão do Windows 98 (a referência
    que o dono do projeto mandou). Não são inventadas.
  * PROVISÓRIOS: tamanhos que precisavam de algum valor para a janela
    existir (fonte, célula, painel...). Estão marcados como PROVISÓRIO e
    aguardam a aprovação ou troca pelo dono do projeto.

Para mudar o visual, muda-se aqui; o resto do código só lê estes nomes.
"""

# ---------------------------------------------------------------------------
# Cores do Windows 98 (paleta padrão do sistema)
# ---------------------------------------------------------------------------
CINZA = (192, 192, 192)        # cor das janelas, botões e barras (#C0C0C0)
BRANCO = (255, 255, 255)       # luz do relevo (borda de cima/esquerda)
CINZA_CLARO = (223, 223, 223)  # luz interna do relevo
CINZA_ESCURO = (128, 128, 128) # sombra do relevo (borda de baixo/direita)
PRETO = (0, 0, 0)              # sombra externa do relevo e texto
AZUL_MARINHO = (0, 0, 128)     # item selecionado / destaque (#000080)
TEXTO = PRETO
TEXTO_SELECIONADO = BRANCO
FUNDO_CAMPO = BRANCO           # fundo de tabelas e campos

# Cores do mapa (as do terminal, na paleta de 16 cores do Windows 98)
FUNDO_MAPA = PRETO
COR_GRAMA = (0, 128, 0)        # verde
COR_AGUA = (0, 0, 255)         # azul
COR_CRUZ = BRANCO              # corpo do NPC morto (✝)
# Terra "d" marrom e pedra "p" cinza (pedido do dono do projeto).
COR_TERRA = (170, 85, 0)       # PROVISÓRIO: o marrom clássico das telas de 16 cores
COR_PEDRA = CINZA              # cinza do Windows 98 (#C0C0C0)
COR_NEVE = BRANCO              # neve "n" branca (pedido do dono do projeto)
COR_AREIA = (255, 255, 0)      # areia "a" amarelo claro (amarelo claro da paleta de 16 cores)
COR_MUDA = (0, 255, 0)         # muda "♣" verde-claro (verde claro da paleta de 16 cores)

# ---------------------------------------------------------------------------
# Fontes do sistema (vêm com o Linux Mint)
# ---------------------------------------------------------------------------
FONTE_UI = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONTE_UI_NEGRITO = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONTE_MAPA = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
FONTE_EMOJI = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf"
# A fonte de emoji colorido só existe neste tamanho; depois é redimensionada.
FONTE_EMOJI_TAMANHO_NATIVO = 109

# ---------------------------------------------------------------------------
# Quadros por segundo: 60, a taxa do monitor do dono do projeto (xrandr).
# ---------------------------------------------------------------------------
QUADROS_POR_SEGUNDO = 60

# ---------------------------------------------------------------------------
# PROVISÓRIOS — aguardando o dono do projeto
# ---------------------------------------------------------------------------
TAM_FONTE_UI = 14          # PROVISÓRIO: texto de menus, abas, tabela, status
TAM_CELULA = 24            # PROVISÓRIO: lado de 1 célula do mapa, em pixels (zoom 1x)
LARGURA_PAINEL = 280       # PROVISÓRIO: painel da direita (seleção)
TAM_SIMBOLO_GRANDE = 128   # PROVISÓRIO: símbolo grande no painel da direita
ESPACO = 6                 # PROVISÓRIO: folga interna de barras, abas e células da tabela
# Segurar uma seta: espera antes de repetir e intervalo entre repetições (ms).
REPETIR_TECLA_ESPERA_MS = 300     # PROVISÓRIO
REPETIR_TECLA_INTERVALO_MS = 50   # PROVISÓRIO
