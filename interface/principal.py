"""
principal.py — interface própria do Genesis (pygame, estilo Windows 98).

O que este arquivo faz:
    1. Abre uma janela maximizada no estilo Windows 98: barra de menus,
       abas (Mapa / NPCs), painel da direita (seleção) e barra de status.
    2. Cria o mundo no motor em C++ (módulo `genesis_motor`, via pybind11)
       e, em tempo real, pede ao motor para andar o tempo que passou.
    3. Mouse: clicar no mapa (ou numa linha da tabela) seleciona; o painel
       da direita mostra o símbolo grande e o que é.
       Teclado: setas movem a câmera; + e - mudam o zoom; I troca de aba.

Regra da STACK.md: a interface só MOSTRA e CONTROLA. Nenhuma regra da
simulação mora aqui; tudo isso está no motor, em C++.

Os desenhos de Windows 98 (relevo, menu, abas, tabela) estão em win98.py;
todas as cores e tamanhos estão em aparencia.py.
"""

import os
import sys
from pathlib import Path

os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")  # sem propaganda no terminal
import pygame  # noqa: E402

import aparencia as A  # noqa: E402
import win98  # noqa: E402

# O módulo compilado fica na pasta build/ na raiz do projeto.
RAIZ = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RAIZ / "build"))

import genesis_motor as gm  # noqa: E402

OC = gm.Ocupante
EST = gm.EstadoNpc

# ---------------------------------------------------------------------------
# O que cada coisa é: símbolo (escolhido pelo dono do projeto) e nome.
# ---------------------------------------------------------------------------
SIMBOLO = {
    int(OC.MACIEIRA): "🌳",
    int(OC.MACA): "🍎",
    int(OC.NPC): "🗿",
    int(OC.CORPO): "✝",
}
GRAMA_SIMB = '"'
AGUA_SIMB = "~"

NOME_ESTADO = {
    EST.VAGANDO: "vagando",
    EST.INDO_COMER: "indo comer",
    EST.COMENDO: "comendo",
    EST.MORTO: "morto",
    EST.INDO_BEBER: "indo beber",
    EST.BEBENDO: "bebendo",
}

# Zoom (escolhido pelo dono do projeto): + aproxima, - afasta.
# Quantas células do mapa cabem em 1 célula da tela, em cada eixo:
#   0.5 -> aproximado: cada célula do mapa fica do tamanho de 2 x 2
#   1   -> normal
#   2, 4, 6 -> afastado: cada célula da tela mostra um bloco 2x2, 4x4, 6x6
ESCALAS = [0.5, 1, 2, 4, 6]

# No zoom afastado aparece a coisa mais importante do bloco, nesta ordem
# (escolha do dono do projeto): NPC > corpo > maçã > macieira > grama > água.
PRIORIDADE = {int(OC.NPC): 4, int(OC.CORPO): 3, int(OC.MACA): 2,
              int(OC.MACIEIRA): 1}
GRAMA = int(gm.Terreno.GRAMA)
# Códigos de chão (acima dos números dos ocupantes, que vão de 1 a 4).
COD_GRAMA, COD_AGUA = 100, 101


class Simbolos:
    """Desenha símbolos em qualquer tamanho, guardando o que já desenhou.

    Emojis coloridos vêm da fonte Noto Color Emoji, que só existe no tamanho
    nativo; por isso são desenhados nele e depois redimensionados.
    Os outros símbolos (", ~, ✝) vêm da fonte do mapa, na cor pedida.
    """

    def __init__(self) -> None:
        self._cache = {}
        self._emoji = None
        if os.path.exists(A.FONTE_EMOJI):
            self._emoji = pygame.font.Font(A.FONTE_EMOJI,
                                           A.FONTE_EMOJI_TAMANHO_NATIVO)
        else:
            print("Aviso: fonte de emoji não encontrada. Instale com:\n"
                  "  sudo apt install fonts-noto-color-emoji")
        self._fontes_mapa = {}

    def _fonte_mapa(self, tamanho: int) -> pygame.font.Font:
        if tamanho not in self._fontes_mapa:
            self._fontes_mapa[tamanho] = pygame.font.Font(A.FONTE_MAPA, tamanho)
        return self._fontes_mapa[tamanho]

    def get(self, simbolo: str, lado: int, cor=A.PRETO) -> pygame.Surface:
        chave = (simbolo, lado, cor)
        if chave not in self._cache:
            emoji = ord(simbolo[0]) > 0x1F000
            if emoji and self._emoji:
                img = self._emoji.render(simbolo, True, A.PRETO)
            else:
                img = self._fonte_mapa(lado).render(simbolo, True, cor)
            # Encaixa no quadrado lado x lado sem deformar.
            esc = min(lado / img.get_width(), lado / img.get_height())
            tam = (max(1, round(img.get_width() * esc)),
                   max(1, round(img.get_height() * esc)))
            self._cache[chave] = pygame.transform.smoothscale(img, tam)
        return self._cache[chave]


class Genesis:
    """A janela inteira: estado da tela + desenho + mouse e teclado."""

    def __init__(self) -> None:
        pygame.display.init()
        pygame.font.init()
        info = pygame.display.Info()
        self.tela = pygame.display.set_mode((info.current_w, info.current_h),
                                            pygame.RESIZABLE)
        pygame.display.set_caption("Genesis")
        self._maximizar()
        pygame.key.set_repeat(A.REPETIR_TECLA_ESPERA_MS,
                              A.REPETIR_TECLA_INTERVALO_MS)

        self.fonte = pygame.font.Font(A.FONTE_UI, A.TAM_FONTE_UI)
        self.fonte_negrito = pygame.font.Font(A.FONTE_UI_NEGRITO, A.TAM_FONTE_UI)
        self.simbolos = Simbolos()

        # Menu "Arquivo" (pedido do dono do projeto).
        self.menu = win98.BarraMenu(self.fonte, [
            ("Arquivo", [
                ("Nova simulação", self.nova_simulacao),
                ("Exibir", self.exibir_informacoes),
                None,
                ("Sair", self.sair),
            ]),
        ])
        self.abas = win98.Abas(self.fonte, ["Mapa", "NPCs"])

        self.rodando = True
        self.nivel_zoom = ESCALAS.index(1)
        self.nova_simulacao()

    # ------------------------------------------------------------------
    # Janela
    # ------------------------------------------------------------------
    def _maximizar(self) -> None:
        """Pede ao sistema para maximizar a janela (pedido do dono)."""
        try:
            from pygame._sdl2.video import Window
            Window.from_display_module().maximize()
        except Exception:
            pass  # sem maximizar: a janela já abre do tamanho da tela

    # ------------------------------------------------------------------
    # Ações do menu
    # ------------------------------------------------------------------
    def nova_simulacao(self) -> None:
        """Começa um mundo novo, com semente sorteada."""
        semente = int.from_bytes(os.urandom(8), "little")
        self.mundo = gm.Mundo(semente)
        self.terreno = self.mundo.terreno_bytes()  # o chão não muda
        self.L, self.H = self.mundo.largura, self.mundo.altura
        self.cam_x = self.cam_y = None  # centraliza no próximo quadro
        self.selecao = None  # ("npc", id) ou ("celula", x, y)

    def exibir_informacoes(self) -> None:
        """'Exibir': mostra a tela com as informações (aba NPCs)."""
        self.abas.ativa = self.abas.nomes.index("NPCs")

    def sair(self) -> None:
        self.rodando = False

    # ------------------------------------------------------------------
    # Medidas da tela (recalculadas a cada quadro: a janela pode mudar)
    # ------------------------------------------------------------------
    def layout(self):
        L, H = self.tela.get_size()
        alt_status = self.fonte.get_height() + 2 * A.ESPACO
        topo = self.menu.altura
        abas_y = topo + A.ESPACO
        conteudo = pygame.Rect(A.ESPACO, abas_y + self.abas.altura,
                               L - A.LARGURA_PAINEL - 3 * A.ESPACO,
                               H - (abas_y + self.abas.altura) - alt_status - A.ESPACO)
        painel = pygame.Rect(conteudo.right + A.ESPACO, abas_y,
                             A.LARGURA_PAINEL, conteudo.bottom - abas_y)
        status = pygame.Rect(0, H - alt_status, L, alt_status)
        return conteudo, painel, status, abas_y

    @property
    def escala(self) -> float:
        return ESCALAS[self.nivel_zoom]

    def medidas_mapa(self, area: pygame.Rect):
        """Pixels por célula da tela, células do mapa por célula da tela,
        e quantas células do mapa cabem (largura, altura)."""
        cel = A.TAM_CELULA * (2 if self.escala < 1 else 1)
        lado = max(1, int(self.escala))
        vis_l = min((area.width - 4) // cel * lado, self.L)
        vis_a = min((area.height - 4) // cel * lado, self.H)
        return cel, lado, max(1, vis_l), max(1, vis_a)

    def limitar_camera(self, vis_l: int, vis_a: int) -> None:
        self.cam_x = max(0, min(self.cam_x, self.L - vis_l))
        self.cam_y = max(0, min(self.cam_y, self.H - vis_a))

    # ------------------------------------------------------------------
    # O que tem numa célula / num bloco
    # ------------------------------------------------------------------
    def codigo_bloco(self, oc: bytes, x0: int, y0: int, lado: int):
        """Código e posição do que aparece num bloco lado x lado."""
        melhor, pos, prio = COD_AGUA, (x0, y0), -1
        for y in range(y0, min(y0 + lado, self.H)):
            base = y * self.L
            for x in range(x0, min(x0 + lado, self.L)):
                o = oc[base + x]
                if o:
                    if PRIORIDADE[o] > prio:
                        melhor, pos, prio = o, (x, y), PRIORIDADE[o]
                elif prio < 0 and self.terreno[base + x] == GRAMA:
                    melhor, pos = COD_GRAMA, (x, y)
        return melhor, pos

    def npc_em(self, x: int, y: int):
        for n in self.mundo.npcs():
            if n.pos.x == x and n.pos.y == y:
                return n
        return None

    # ------------------------------------------------------------------
    # Desenho
    # ------------------------------------------------------------------
    def desenhar_mapa(self, area: pygame.Rect) -> None:
        win98.caixa(self.tela, area, saliente=False, cor=A.FUNDO_MAPA)
        cel, lado, vis_l, vis_a = self.medidas_mapa(area)
        if self.cam_x is None:  # primeiro quadro: centro do mapa no centro
            self.cam_x = (self.L - vis_l) // 2
            self.cam_y = (self.H - vis_a) // 2
        self.limitar_camera(vis_l, vis_a)
        self._mapa_area, self._mapa_cel, self._mapa_lado = area, cel, lado

        oc = self.mundo.ocupacao_bytes()
        grama = self.simbolos.get(GRAMA_SIMB, cel, A.COR_GRAMA)
        agua = self.simbolos.get(AGUA_SIMB, cel, A.COR_AGUA)
        ox, oy = area.left + 2, area.top + 2
        anterior = self.tela.get_clip()
        self.tela.set_clip(area.inflate(-4, -4))
        for j, my in enumerate(range(self.cam_y, self.cam_y + vis_a, lado)):
            for i, mx in enumerate(range(self.cam_x, self.cam_x + vis_l, lado)):
                if lado == 1:  # zoom normal/aproximado: célula direta
                    k = my * self.L + mx
                    cod = oc[k] or (COD_GRAMA if self.terreno[k] == GRAMA
                                    else COD_AGUA)
                else:
                    cod, _ = self.codigo_bloco(oc, mx, my, lado)
                if cod == COD_GRAMA:
                    img = grama
                elif cod == COD_AGUA:
                    img = agua
                else:
                    cor = A.COR_CRUZ if cod == int(OC.CORPO) else A.PRETO
                    img = self.simbolos.get(SIMBOLO[cod], cel, cor)
                px = ox + i * cel + (cel - img.get_width()) // 2
                py = oy + j * cel + (cel - img.get_height()) // 2
                self.tela.blit(img, (px, py))
        # Seleção: moldura branca fina em volta da célula selecionada
        # (PROVISÓRIO, aguardando o dono do projeto).
        alvo = self.posicao_selecao()
        if alvo:
            sx = (alvo[0] - self.cam_x) // lado
            sy = (alvo[1] - self.cam_y) // lado
            if 0 <= sx * lado < vis_l and 0 <= sy * lado < vis_a:
                r = pygame.Rect(ox + sx * cel, oy + sy * cel, cel, cel)
                pygame.draw.rect(self.tela, A.BRANCO, r, 1)
        self.tela.set_clip(anterior)

    def linhas_tabela(self):
        return [[f"NPC {n.id + 1}", f"{n.fome:.0f}", f"{n.sede:.0f}",
                 NOME_ESTADO[n.estado], f"({n.pos.x}, {n.pos.y})"]
                for n in self.mundo.npcs()]

    def desenhar_npcs(self, area: pygame.Rect) -> None:
        pygame.draw.rect(self.tela, A.CINZA, area)
        sel = self.selecao[1] if self.selecao and self.selecao[0] == "npc" else None
        self._linhas_rects = win98.tabela(
            self.tela, area.inflate(-2 * A.ESPACO, -2 * A.ESPACO), self.fonte,
            ["NPC", "Fome", "Sede", "Estado", "Posição"],
            self.linhas_tabela(), selecionada=sel)

    def descrever_selecao(self):
        """Símbolo, cor e linhas de texto do que está selecionado."""
        if self.selecao is None:
            return None, None, ["Clique em algo no mapa."]
        if self.selecao[0] == "npc":
            n = self.mundo.npcs()[self.selecao[1]]
            if n.estado == EST.MORTO:
                causa = "fome" if n.fome == 0 else "sede"
                return "✝", A.PRETO, [f"Corpo do NPC {n.id + 1}",
                                      f"Morreu de {causa}",
                                      f"Posição: ({n.pos.x}, {n.pos.y})"]
            return "🗿", A.PRETO, [f"NPC {n.id + 1}",
                                   f"Fome: {n.fome:.0f}",
                                   f"Sede: {n.sede:.0f}",
                                   f"Estado: {NOME_ESTADO[n.estado]}",
                                   f"Posição: ({n.pos.x}, {n.pos.y})"]
        _, x, y = self.selecao
        o = self.mundo.ocupante(x, y)
        pos = f"Posição: ({x}, {y})"
        if o == OC.MACIEIRA:
            return "🌳", A.PRETO, ["Macieira", pos]
        if o == OC.MACA:
            return "🍎", A.PRETO, ["Maçã", pos]
        if self.mundo.terreno(x, y) == gm.Terreno.GRAMA:
            return GRAMA_SIMB, A.COR_GRAMA, ["Grama", pos]
        return AGUA_SIMB, A.COR_AGUA, ["Água", pos]

    def posicao_selecao(self):
        if self.selecao is None:
            return None
        if self.selecao[0] == "npc":
            n = self.mundo.npcs()[self.selecao[1]]
            return n.pos.x, n.pos.y
        return self.selecao[1], self.selecao[2]

    def desenhar_painel(self, r: pygame.Rect) -> None:
        win98.caixa(self.tela, r)
        y = r.top + 2 * A.ESPACO
        win98.escrever(self.tela, self.fonte_negrito, "Seleção",
                       (r.left + 2 * A.ESPACO, y))
        y += self.fonte.get_height() + 2 * A.ESPACO
        simb, cor, linhas = self.descrever_selecao()
        quadro = pygame.Rect(r.left + (r.width - A.TAM_SIMBOLO_GRANDE) // 2 - A.ESPACO,
                             y, A.TAM_SIMBOLO_GRANDE + 2 * A.ESPACO,
                             A.TAM_SIMBOLO_GRANDE + 2 * A.ESPACO)
        fundo = A.FUNDO_MAPA if simb in (GRAMA_SIMB, AGUA_SIMB) else A.FUNDO_CAMPO
        win98.caixa(self.tela, quadro, saliente=False, cor=fundo)
        if simb:
            img = self.simbolos.get(simb, A.TAM_SIMBOLO_GRANDE, cor)
            self.tela.blit(img, (quadro.centerx - img.get_width() // 2,
                                 quadro.centery - img.get_height() // 2))
        y = quadro.bottom + 2 * A.ESPACO
        for i, linha in enumerate(linhas):
            f = self.fonte_negrito if i == 0 else self.fonte
            win98.escrever(self.tela, f, linha, (r.left + 2 * A.ESPACO, y))
            y += self.fonte.get_height() + A.ESPACO

    def desenhar_status(self, r: pygame.Rect) -> None:
        pygame.draw.rect(self.tela, A.CINZA, r)
        s = int(self.mundo.tempo)
        zoom = "0,5x" if self.escala == 0.5 else f"{self.escala}x"
        partes = [f"Tempo {s // 60:02d}:{s % 60:02d}",
                  f"Maçãs no chão: {len(self.mundo.macas())}",
                  f"Zoom {zoom}",
                  f"Câmera ({self.cam_x}, {self.cam_y})",
                  "Setas: mover   + / -: zoom   I: trocar aba"]
        x = r.left + 2
        for p in partes:
            w = self.fonte.size(p)[0] + 4 * A.ESPACO
            campo = pygame.Rect(x, r.top + 2, w, r.height - 4)
            win98.relevo(self.tela, campo, saliente=False)
            win98.escrever(self.tela, self.fonte, p,
                           (campo.left + 2 * A.ESPACO, campo.top + A.ESPACO - 2))
            x += w + 2

    def desenhar(self) -> None:
        self.tela.fill(A.CINZA)
        conteudo, painel, status, abas_y = self.layout()
        self.menu.desenhar(self.tela, self.tela.get_width())
        # Área de conteúdo com relevo; abas por cima, coladas nela.
        win98.caixa(self.tela, conteudo.inflate(4, 4))
        if self.abas.nomes[self.abas.ativa] == "Mapa":
            self.desenhar_mapa(conteudo)
        else:
            self.desenhar_npcs(conteudo)
        self.abas.desenhar(self.tela, conteudo.left, abas_y)
        self.desenhar_painel(painel)
        self.desenhar_status(status)
        self.menu.desenhar_aberto(self.tela)  # por último: fica por cima
        pygame.display.flip()

    # ------------------------------------------------------------------
    # Mouse e teclado
    # ------------------------------------------------------------------
    def zoom(self, direcao: int) -> None:
        """direcao -1 = aproxima (+), +1 = afasta (-). Mantém o centro."""
        novo = max(0, min(len(ESCALAS) - 1, self.nivel_zoom + direcao))
        if novo == self.nivel_zoom:
            return
        conteudo = self.layout()[0]
        _, _, vl, va = self.medidas_mapa(conteudo)
        cx, cy = self.cam_x + vl / 2, self.cam_y + va / 2
        self.nivel_zoom = novo
        _, _, vl, va = self.medidas_mapa(conteudo)
        self.cam_x, self.cam_y = int(cx - vl / 2), int(cy - va / 2)
        self.limitar_camera(vl, va)

    def clique_mapa(self, pos) -> None:
        area, cel, lado = self._mapa_area, self._mapa_cel, self._mapa_lado
        i = (pos[0] - area.left - 2) // cel
        j = (pos[1] - area.top - 2) // cel
        mx, my = self.cam_x + i * lado, self.cam_y + j * lado
        if not (0 <= mx < self.L and 0 <= my < self.H):
            return
        cod, (x, y) = self.codigo_bloco(self.mundo.ocupacao_bytes(), mx, my, lado)
        if cod in (int(OC.NPC), int(OC.CORPO)):
            n = self.npc_em(x, y)
            self.selecao = ("npc", n.id) if n else ("celula", x, y)
        else:
            self.selecao = ("celula", x, y)

    def tratar_evento(self, ev) -> None:
        if ev.type == pygame.QUIT:
            self.sair()
        elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            if self.menu.clique(ev.pos) or self.abas.clique(ev.pos):
                return
            conteudo = self.layout()[0]
            if not conteudo.collidepoint(ev.pos):
                return
            if self.abas.nomes[self.abas.ativa] == "Mapa":
                self.clique_mapa(ev.pos)
            else:
                for i, r in enumerate(getattr(self, "_linhas_rects", [])):
                    if r.collidepoint(ev.pos):
                        self.selecao = ("npc", i)
        elif ev.type == pygame.KEYDOWN:
            passo = max(1, int(self.escala))  # 1 célula da tela
            if ev.key == pygame.K_ESCAPE:
                self.menu.aberto = None
            elif ev.key == pygame.K_LEFT:
                self.cam_x -= passo
            elif ev.key == pygame.K_RIGHT:
                self.cam_x += passo
            elif ev.key == pygame.K_UP:
                self.cam_y -= passo
            elif ev.key == pygame.K_DOWN:
                self.cam_y += passo
            elif ev.unicode in ("+", "=") or ev.key == pygame.K_KP_PLUS:
                self.zoom(-1)
            elif ev.unicode == "-" or ev.key == pygame.K_KP_MINUS:
                self.zoom(+1)
            elif ev.key == pygame.K_i:
                self.abas.ativa = (self.abas.ativa + 1) % len(self.abas.nomes)

    # ------------------------------------------------------------------
    # Laço principal
    # ------------------------------------------------------------------
    def rodar(self) -> None:
        relogio = pygame.time.Clock()
        while self.rodando:
            for ev in pygame.event.get():
                self.tratar_evento(ev)
            # Tempo real: o motor anda exatamente o que passou no relógio.
            dt_ms = relogio.tick(A.QUADROS_POR_SEGUNDO)
            self.mundo.avancar(dt_ms / 1000.0)
            self.desenhar()
        pygame.quit()


if __name__ == "__main__":
    Genesis().rodar()
