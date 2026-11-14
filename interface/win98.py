"""
win98.py — peças de desenho no estilo Windows 98, feitas à mão em pygame.

Nada aqui sabe da simulação: são só "tijolos" visuais (relevo, botão, menu,
abas, tabela) que a tela principal usa. Assim, o visual pode ser trocado
sem mexer no resto.

O "relevo" é o segredo do visual do Windows 98: cada caixa tem bordas
claras de um lado e escuras do outro, imitando luz vindo de cima à esquerda.
  - Saliente (botão solto, janela):  claro em cima/esquerda, escuro embaixo/direita.
  - Afundado (campo, área de conteúdo): o contrário.
"""

import pygame

import aparencia as A


# ---------------------------------------------------------------------------
# Relevo
# ---------------------------------------------------------------------------
def relevo(tela: pygame.Surface, r: pygame.Rect, saliente: bool = True) -> None:
    """Desenha a borda 3D de 2 pixels em volta do retângulo `r`."""
    if saliente:
        ext_luz, int_luz, int_sombra, ext_sombra = (
            A.BRANCO, A.CINZA_CLARO, A.CINZA_ESCURO, A.PRETO)
    else:
        ext_luz, int_luz, int_sombra, ext_sombra = (
            A.CINZA_ESCURO, A.PRETO, A.CINZA_CLARO, A.BRANCO)
    x0, y0, x1, y1 = r.left, r.top, r.right - 1, r.bottom - 1
    # Borda de fora
    pygame.draw.line(tela, ext_luz, (x0, y0), (x1 - 1, y0))
    pygame.draw.line(tela, ext_luz, (x0, y0), (x0, y1 - 1))
    pygame.draw.line(tela, ext_sombra, (x0, y1), (x1, y1))
    pygame.draw.line(tela, ext_sombra, (x1, y0), (x1, y1))
    # Borda de dentro
    pygame.draw.line(tela, int_luz, (x0 + 1, y0 + 1), (x1 - 2, y0 + 1))
    pygame.draw.line(tela, int_luz, (x0 + 1, y0 + 1), (x0 + 1, y1 - 2))
    pygame.draw.line(tela, int_sombra, (x0 + 1, y1 - 1), (x1 - 1, y1 - 1))
    pygame.draw.line(tela, int_sombra, (x1 - 1, y0 + 1), (x1 - 1, y1 - 1))


def caixa(tela: pygame.Surface, r: pygame.Rect, saliente: bool = True,
          cor=A.CINZA) -> None:
    """Retângulo preenchido com relevo."""
    pygame.draw.rect(tela, cor, r)
    relevo(tela, r, saliente)


def escrever(tela: pygame.Surface, fonte: pygame.font.Font, texto: str,
             pos, cor=A.TEXTO) -> pygame.Rect:
    """Escreve `texto` com o canto de cima à esquerda em `pos`."""
    img = fonte.render(texto, True, cor)
    return tela.blit(img, pos)


# ---------------------------------------------------------------------------
# Barra de menus (com menus que abrem para baixo)
# ---------------------------------------------------------------------------
class BarraMenu:
    """Barra de menus no topo.

    `menus` é uma lista de (nome, itens); cada item é (texto, função) ou
    None para uma linha separadora. Clicar num nome abre o menu; clicar num
    item chama a função e fecha; clicar fora fecha.
    """

    def __init__(self, fonte: pygame.font.Font, menus) -> None:
        self.fonte = fonte
        self.menus = menus
        self.aberto = None          # índice do menu aberto, ou None
        self._rects_nomes = []      # onde está cada nome na barra
        self._rects_itens = []      # onde está cada item do menu aberto
        self.altura = fonte.get_height() + 2 * A.ESPACO

    def desenhar(self, tela: pygame.Surface, largura: int) -> None:
        barra = pygame.Rect(0, 0, largura, self.altura)
        pygame.draw.rect(tela, A.CINZA, barra)
        self._rects_nomes = []
        x = A.ESPACO
        for i, (nome, _) in enumerate(self.menus):
            w = self.fonte.size(nome)[0] + 2 * A.ESPACO
            r = pygame.Rect(x, 0, w, self.altura)
            if i == self.aberto:  # nome do menu aberto fica "afundado"
                relevo(tela, r, saliente=False)
            escrever(tela, self.fonte, nome, (x + A.ESPACO, A.ESPACO))
            self._rects_nomes.append(r)
            x += w

    def desenhar_aberto(self, tela: pygame.Surface) -> None:
        """Desenha o menu aberto (por último, para ficar por cima de tudo)."""
        self._rects_itens = []
        if self.aberto is None:
            return
        itens = self.menus[self.aberto][1]
        alt_item = self.fonte.get_height() + A.ESPACO
        largura = max(self.fonte.size(t)[0] for t in
                      (i[0] for i in itens if i)) + 6 * A.ESPACO
        altura = sum(alt_item if i else A.ESPACO for i in itens) + 2 * A.ESPACO
        base = self._rects_nomes[self.aberto]
        caixa_menu = pygame.Rect(base.left, base.bottom, largura, altura)
        caixa(tela, caixa_menu)
        y = caixa_menu.top + A.ESPACO
        mouse = pygame.mouse.get_pos()
        for item in itens:
            if item is None:  # separador: linha afundada
                meio = y + A.ESPACO // 2
                pygame.draw.line(tela, A.CINZA_ESCURO,
                                 (caixa_menu.left + 3, meio), (caixa_menu.right - 4, meio))
                pygame.draw.line(tela, A.BRANCO,
                                 (caixa_menu.left + 3, meio + 1), (caixa_menu.right - 4, meio + 1))
                self._rects_itens.append(None)
                y += A.ESPACO
                continue
            r = pygame.Rect(caixa_menu.left + 3, y, largura - 6, alt_item)
            cor = A.TEXTO
            if r.collidepoint(mouse):  # item sob o mouse: azul-marinho
                pygame.draw.rect(tela, A.AZUL_MARINHO, r)
                cor = A.TEXTO_SELECIONADO
            escrever(tela, self.fonte, item[0],
                     (r.left + 2 * A.ESPACO, r.top + A.ESPACO // 2), cor)
            self._rects_itens.append(r)
            y += alt_item

    def clique(self, pos) -> bool:
        """Trata um clique. Devolve True se o clique foi do menu."""
        if self.aberto is not None:
            itens = self.menus[self.aberto][1]
            for item, r in zip(itens, self._rects_itens):
                if item and r and r.collidepoint(pos):
                    self.aberto = None
                    item[1]()
                    return True
        for i, r in enumerate(self._rects_nomes):
            if r.collidepoint(pos):
                self.aberto = None if self.aberto == i else i
                return True
        if self.aberto is not None:  # clicou fora: só fecha
            self.aberto = None
            return True
        return False


# ---------------------------------------------------------------------------
# Abas
# ---------------------------------------------------------------------------
class Abas:
    """Fileira de abas. A aba ativa fica "colada" na área de conteúdo."""

    def __init__(self, fonte: pygame.font.Font, nomes) -> None:
        self.fonte = fonte
        self.nomes = list(nomes)
        self.ativa = 0
        self._rects = []
        self.altura = fonte.get_height() + 2 * A.ESPACO

    def desenhar(self, tela: pygame.Surface, x: int, y: int) -> None:
        self._rects = []
        for i, nome in enumerate(self.nomes):
            w = self.fonte.size(nome)[0] + 4 * A.ESPACO
            ativa = i == self.ativa
            # A aba ativa é um pouco mais alta e não tem borda embaixo.
            sobe = 2 if ativa else 0
            r = pygame.Rect(x, y - sobe, w, self.altura + sobe)
            pygame.draw.rect(tela, A.CINZA, r)
            pygame.draw.line(tela, A.BRANCO, (r.left, r.bottom - 1), (r.left, r.top + 2))
            pygame.draw.line(tela, A.BRANCO, (r.left + 2, r.top), (r.right - 3, r.top))
            pygame.draw.line(tela, A.PRETO, (r.right - 1, r.top + 2), (r.right - 1, r.bottom - 1))
            pygame.draw.line(tela, A.CINZA_ESCURO, (r.right - 2, r.top + 1), (r.right - 2, r.bottom - 1))
            escrever(tela, self.fonte, nome, (r.left + 2 * A.ESPACO, r.top + A.ESPACO))
            self._rects.append(r)
            x += w

    def clique(self, pos) -> bool:
        for i, r in enumerate(self._rects):
            if r.collidepoint(pos):
                self.ativa = i
                return True
        return False


# ---------------------------------------------------------------------------
# Tabela
# ---------------------------------------------------------------------------
def tabela(tela: pygame.Surface, r: pygame.Rect, fonte: pygame.font.Font,
           colunas, linhas, selecionada=None, inicio: int = 0):
    """Desenha uma tabela num campo branco afundado.

    `colunas`: nomes; `linhas`: lista de listas de texto. `inicio`: primeira
    linha mostrada (para rolar). Devolve [(índice da linha, retângulo)] das
    linhas que aparecem (para saber em qual clicaram).
    """
    caixa(tela, r, saliente=False, cor=A.FUNDO_CAMPO)
    alt = fonte.get_height() + A.ESPACO
    larguras = []
    for c, nome in enumerate(colunas):
        maior = max([fonte.size(nome)[0]] + [fonte.size(l[c])[0] for l in linhas])
        larguras.append(maior + 4 * A.ESPACO)
    anterior = tela.get_clip()
    tela.set_clip(r.inflate(-4, -4))
    x = r.left + 2
    y = r.top + 2
    for nome, w in zip(colunas, larguras):
        cab = pygame.Rect(x, y, w, alt)
        caixa(tela, cab)
        escrever(tela, fonte, nome, (cab.left + 2 * A.ESPACO, cab.top + A.ESPACO // 2))
        x += w
    rects = []
    y += alt
    for i in range(max(0, inicio), len(linhas)):
        if y + alt > r.bottom:
            break
        linha = linhas[i]
        lr = pygame.Rect(r.left + 2, y, sum(larguras), alt)
        cor = A.TEXTO
        if i == selecionada:
            pygame.draw.rect(tela, A.AZUL_MARINHO, lr)
            cor = A.TEXTO_SELECIONADO
        x = lr.left
        for texto, w in zip(linha, larguras):
            escrever(tela, fonte, texto, (x + 2 * A.ESPACO, y + A.ESPACO // 2), cor)
            x += w
        rects.append((i, lr))
        y += alt
    tela.set_clip(anterior)
    return rects


def linhas_visiveis(r: pygame.Rect, fonte: pygame.font.Font, cabecalho: bool = True) -> int:
    """Quantas linhas de texto cabem no campo `r` (descontando o cabeçalho)."""
    alt = fonte.get_height() + A.ESPACO
    return max(1, (r.height - 4) // alt - (1 if cabecalho else 0))


def botao(tela: pygame.Surface, r: pygame.Rect, fonte: pygame.font.Font, texto: str,
          apertado: bool = False) -> None:
    """Botão do Windows 98: saliente; apertado = afundado."""
    caixa(tela, r, saliente=not apertado)
    w, h = fonte.size(texto)
    d = 1 if apertado else 0
    escrever(tela, fonte, texto, (r.centerx - w // 2 + d, r.centery - h // 2 + d))


def janela(tela: pygame.Surface, r: pygame.Rect, fonte_titulo: pygame.font.Font,
           titulo: str) -> pygame.Rect:
    """Janela do Windows 98 (caixa cinza com barra de título azul-marinho).
    Devolve o retângulo do botão [X] da barra de título."""
    caixa(tela, r)
    alt = fonte_titulo.get_height() + A.ESPACO
    barra = pygame.Rect(r.left + 3, r.top + 3, r.width - 6, alt)
    pygame.draw.rect(tela, A.AZUL_MARINHO, barra)
    escrever(tela, fonte_titulo, titulo, (barra.left + A.ESPACO, barra.top + A.ESPACO // 2),
             A.TEXTO_SELECIONADO)
    x = pygame.Rect(barra.right - alt + 2, barra.top + 2, alt - 4, alt - 4)
    botao(tela, x, fonte_titulo, "x")
    return x
