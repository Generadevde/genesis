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

import faulthandler
import os
import sys
from pathlib import Path

# Se o programa cair com "falha de segmentação", o Python mostra no terminal
# em que linha estava. Ajuda a achar o problema.
faulthandler.enable()

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
    int(OC.MUDA): "♣",
    int(OC.MACIEIRA_SECA): "🌳",  # desenhada em cinza (ver Simbolos.get)
}
MAT = gm.Material
# Materiais do chão: símbolo, cor e nome (escolhidos pelo dono do projeto).
MATERIAL = {
    int(MAT.AR): (" ", A.FUNDO_MAPA, "Ar"),
    int(MAT.GRAMA): ('"', A.COR_GRAMA, "Grama"),
    int(MAT.NEVE): ("n", A.COR_NEVE, "Neve"),
    int(MAT.TERRA): ("d", A.COR_TERRA, "Terra"),
    int(MAT.PEDRA): ("p", A.COR_PEDRA, "Pedra"),
    int(MAT.AGUA): ("~", A.COR_AGUA, "Água"),
}

NOME_ESTADO = {
    EST.VAGANDO: "vagando",
    EST.INDO_COMER: "indo comer",
    EST.COMENDO: "comendo",
    EST.MORTO: "morto",
    EST.INDO_BEBER: "indo beber",
    EST.BEBENDO: "bebendo",
    EST.INDO_PEGAR: "indo pegar maçã",
}

# Zoom (escolhido pelo dono do projeto): + aproxima, - afasta.
# Quantas células do mapa cabem em 1 célula da tela, em cada eixo:
#   0.5 -> aproximado: cada célula do mapa fica do tamanho de 2 x 2
#   1   -> normal
#   2, 4, 6 -> afastado: cada célula da tela mostra um bloco 2x2, 4x4, 6x6
ESCALAS = [0.5, 1, 2, 4, 6]

# Botões de velocidade ao lado do tempo (pedido do dono do projeto):
# ">" = 2x, ">>" = 4x, ">>>>" = 8x. Clicar no botão já ligado volta a 1x.
VELOCIDADES = [(">", 2), (">>", 4), (">>>>", 8)]

# Cada célula da tela vira um "código" antes de virar desenho:
#   1..4       = ocupante (NPC, corpo, maçã, macieira — números do motor)
#   100 + m    = material m do chão (ar, grama, terra, pedra, água)
COD_MATERIAL = 100

# No zoom afastado aparece a coisa mais importante do bloco, nesta ordem:
# NPC > corpo > maçã > macieira > grama > água (escolha do dono do projeto)
# > terra > pedra > ar (PROVISÓRIO: terra e pedra vieram depois);
# a neve fica junto da grama (PROVISÓRIO).
PRIORIDADE = {
    int(OC.NPC): 9, int(OC.CORPO): 8, int(OC.MACA): 7, int(OC.MACIEIRA): 6,
    int(OC.MACIEIRA_SECA): 6, int(OC.MUDA): 5.5,  # PROVISÓRIO
    COD_MATERIAL + int(MAT.GRAMA): 5, COD_MATERIAL + int(MAT.NEVE): 5,
    COD_MATERIAL + int(MAT.AGUA): 4,
    COD_MATERIAL + int(MAT.TERRA): 3, COD_MATERIAL + int(MAT.PEDRA): 2,
    COD_MATERIAL + int(MAT.AR): 1,
}


def decodificar(byte_visao: int, ocupante: int) -> int:
    """Byte da visão do motor -> código do que aparece. Ver Mundo::visao.

    O que está abaixo do nível visto aparece NORMAL, sem escurecer
    (pedido do dono do projeto em 14/11/2026).
    """
    material = byte_visao & 0x0F
    if byte_visao & 0x20 and ocupante:
        return ocupante
    return COD_MATERIAL + material


def duracao(segundos: float) -> str:
    """Tempo do jogo por extenso: "2 dias 5 h", "3 h 20 min", "45 min".
    (1 hora do jogo = SEGUNDOS_POR_HORA segundos da simulação.)"""
    minutos = max(0, int(segundos / (gm.SEGUNDOS_POR_HORA / 60)))
    dias, resto = divmod(minutos, 24 * 60)
    horas, mins = divmod(resto, 60)
    if dias:
        return f"{dias} dia{'s' if dias > 1 else ''} {horas} h"
    if horas:
        return f"{horas} h {mins} min"
    return f"{mins} min"


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

    def get(self, simbolo: str, lado: int, cor=A.PRETO,
            cinza: bool = False) -> pygame.Surface:
        """Símbolo no tamanho `lado`, na cor `cor` (emojis têm cor própria).
        `cinza` = o mesmo desenho em tons de cinza (macieira seca)."""
        if cinza:
            chave = (simbolo, lado, cor, "cinza")
            if chave not in self._cache:
                self._cache[chave] = pygame.transform.grayscale(
                    self.get(simbolo, lado, cor))
            return self._cache[chave]
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
        self.abas = win98.Abas(self.fonte, ["Mapa", "NPCs", "Info"])

        self.rodando = True
        self._arraste = None  # clique-e-arraste do mapa em andamento
        self.velocidade = 1   # 1x = tempo real; 2x, 4x, 8x pelos botões
        self._botoes_velocidade = []  # (retângulo, multiplicador)
        self.nivel_zoom = ESCALAS.index(1)
        self.nova_simulacao()

    # ------------------------------------------------------------------
    # Janela
    # ------------------------------------------------------------------
    def _maximizar(self) -> None:
        """Pede ao sistema para maximizar a janela (pedido do dono)."""
        # IMPORTANTE: o objeto Window precisa ficar guardado (self._janela).
        # Se ele for descartado, o pygame destrói a janela junto, e o próximo
        # desenho derruba o programa (falha de segmentação).
        try:
            from pygame._sdl2.video import Window
            self._janela = Window.from_display_module()
            self._janela.maximize()
        except Exception:
            self._janela = None  # sem maximizar: a janela já abre do tamanho da tela

    # ------------------------------------------------------------------
    # Ações do menu
    # ------------------------------------------------------------------
    def nova_simulacao(self) -> None:
        """Começa um mundo novo, com semente sorteada."""
        semente = int.from_bytes(os.urandom(8), "little")
        self.mundo = gm.Mundo(semente)
        self.L, self.H = self.mundo.largura, self.mundo.altura
        # Nível z que se está vendo. Começa no topo dos morros (+4), assim
        # a ilha inteira aparece vista de cima — PROVISÓRIO.
        self.z_vista = gm.SUPERFICIE_MAX
        self.cam_x = self.cam_y = None  # centraliza no próximo quadro
        self.selecao = None  # ("npc", id) ou ("celula", x, y, z)

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
    def codigo_bloco(self, vis: bytes, oc: bytes, x0: int, y0: int, lado: int):
        """O que aparece num bloco lado x lado: (código, (x, y))."""
        melhor = None
        for y in range(y0, min(y0 + lado, self.H)):
            base = y * self.L
            for x in range(x0, min(x0 + lado, self.L)):
                cod = decodificar(vis[base + x], oc[base + x])
                if melhor is None or PRIORIDADE[cod] > PRIORIDADE[melhor[0]]:
                    melhor = (cod, (x, y))
        return melhor

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
        vis = self.mundo.visao_bytes(self.z_vista)
        ox, oy = area.left + 2, area.top + 2
        anterior = self.tela.get_clip()
        self.tela.set_clip(area.inflate(-4, -4))
        for j, my in enumerate(range(self.cam_y, self.cam_y + vis_a, lado)):
            for i, mx in enumerate(range(self.cam_x, self.cam_x + vis_l, lado)):
                if lado == 1:  # zoom normal/aproximado: célula direta
                    k = my * self.L + mx
                    cod = decodificar(vis[k], oc[k])
                else:
                    cod, _ = self.codigo_bloco(vis, oc, mx, my, lado)
                img = self.imagem(cod, cel)
                if img is None:
                    continue
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

    @staticmethod
    def texto_carregando(n) -> str:
        """O que o NPC leva na mão (hoje só maçã, no máximo 1)."""
        if n.macas_carregadas == 0:
            return "nada"
        return "1 maçã" if n.macas_carregadas == 1 else f"{n.macas_carregadas} maçãs"

    @staticmethod
    def texto_estado(n) -> str:
        """Estado do NPC; indo a um lugar lembrado ganha "(memória)"."""
        texto = NOME_ESTADO[n.estado]
        if n.estado == EST.COMENDO and n.comendo_da_mao:
            return "comendo a maçã que carregava"
        if n.pela_memoria and n.estado in (EST.INDO_COMER, EST.INDO_BEBER):
            texto += " (memória)"
        return texto

    def imagem(self, cod: int, lado: int):
        """Desenho de um código (ocupante ou material) no tamanho `lado`."""
        if cod >= COD_MATERIAL:
            simb, cor, _ = MATERIAL[cod - COD_MATERIAL]
            if simb == " ":
                return None  # ar sem nada embaixo: fica preto
            return self.simbolos.get(simb, lado, cor)
        cor = {int(OC.CORPO): A.COR_CRUZ, int(OC.MUDA): A.COR_MUDA}.get(cod, A.PRETO)
        return self.simbolos.get(SIMBOLO[cod], lado, cor,
                                 cinza=(cod == int(OC.MACIEIRA_SECA)))

    def linhas_tabela(self):
        return [[n.nome, f"{n.fome:.0f}", f"{n.sede:.0f}",
                 self.texto_estado(n), f"({n.pos.x}, {n.pos.y}, {n.pos.z:+d})",
                 self.texto_carregando(n),
                 str(len(n.macieiras_lembradas)), str(len(n.aguas_lembradas))]
                for n in self.mundo.npcs()]

    def desenhar_npcs(self, area: pygame.Rect) -> None:
        pygame.draw.rect(self.tela, A.CINZA, area)
        sel = self.selecao[1] if self.selecao and self.selecao[0] == "npc" else None
        self._linhas_rects = win98.tabela(
            self.tela, area.inflate(-2 * A.ESPACO, -2 * A.ESPACO), self.fonte,
            ["NPC", "Fome", "Sede", "Estado", "Posição", "Carregando",
             "Macieiras lembradas", "Águas lembradas"],
            self.linhas_tabela(), selecionada=sel)

    def descrever_selecao(self):
        """Símbolo, cor e linhas de texto do que está selecionado."""
        if self.selecao is None:
            return None, None, ["Clique em algo no mapa."]
        if self.selecao[0] == "npc":
            n = self.mundo.npcs()[self.selecao[1]]
            if n.estado == EST.MORTO:
                causa = "fome" if n.fome == 0 else "sede"
                return "✝", A.PRETO, [f"Corpo de {n.nome}",
                                      f"Morreu de {causa}",
                                      f"Posição: ({n.pos.x}, {n.pos.y}, {n.pos.z:+d})"]
            linhas = [n.nome,
                      f"Fome: {n.fome:.0f}",
                      f"Sede: {n.sede:.0f}",
                      f"Estado: {self.texto_estado(n)}",
                      f"Carregando: {self.texto_carregando(n)}",
                      f"Posição: ({n.pos.x}, {n.pos.y}, {n.pos.z:+d})",
                      "",
                      "Memória:"]
            arvores = self.mundo.macieiras()
            for i in n.macieiras_lembradas:
                p = arvores[i].pos
                linhas.append(f"  macieira em ({p.x}, {p.y}, {p.z:+d})")
            if n.aguas_lembradas:
                linhas.append(f"  água: {len(n.aguas_lembradas)} ponto(s)")
            if not n.macieiras_lembradas and not n.aguas_lembradas:
                linhas.append("  (nenhum lugar ainda)")
            return "🗿", A.PRETO, linhas
        _, x, y, z = self.selecao
        pos = f"Posição: ({x}, {y}, {z:+d})"
        # Árvore e maçã ficam na superfície.
        if z == self.mundo.superficie(x, y):
            o = self.mundo.ocupante(x, y)
            agora = self.mundo.tempo
            if o in (OC.MACIEIRA, OC.MACIEIRA_SECA):
                m = self.mundo.macieiras()[self.mundo.macieira_em(x, y)]
                linhas = ["Macieira seca" if m.seca else "Macieira", pos,
                          f"Idade: {duracao(agora - m.nasceu)}"]
                if m.seca:
                    linhas.append(f"Some em: {duracao(m.quando_sumir - agora)}")
                else:
                    linhas.append(f"Seca em: {duracao(m.quando_secar - agora)}")
                # cor None = desenhar em cinza (macieira seca)
                return "🌳", (None if m.seca else A.PRETO), linhas
            if o == OC.MACA:
                falta = self.mundo.falta_virar_muda(x, y)
                return "🍎", A.PRETO, ["Maçã", pos,
                                       f"Vira muda em: {duracao(falta)}"]
            if o == OC.MUDA:
                u = self.mundo.muda_em(x, y)
                return "♣", A.COR_MUDA, ["Muda", pos,
                                         f"Idade: {duracao(agora - u.nasceu)}",
                                         f"Vira macieira em: {duracao(u.quando_virar - agora)}"]
        m = int(self.mundo.material(x, y, z))
        simb, cor, nome = MATERIAL[m]
        linhas = [nome, pos]
        if m == int(MAT.AGUA):
            nivel = self.mundo.nivel_agua(x, y, z)
            linhas.append(f"Nível da água: {nivel}/{gm.AGUA_NIVEL_MAX}"
                          f" ({nivel * 100 // gm.AGUA_NIVEL_MAX}%)")
        return simb, cor, linhas

    def posicao_selecao(self):
        if self.selecao is None:
            return None
        if self.selecao[0] == "npc":
            n = self.mundo.npcs()[self.selecao[1]]
            return n.pos.x, n.pos.y
        return self.selecao[1], self.selecao[2]  # (x, y)

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
        # Materiais (letras coloridas) aparecem no fundo preto, como no mapa.
        fundo = A.FUNDO_MAPA if simb and simb in "\"ndp~♣ " else A.FUNDO_CAMPO
        win98.caixa(self.tela, quadro, saliente=False, cor=fundo)
        if simb:
            # cor None = o símbolo em cinza (macieira seca)
            img = self.simbolos.get(simb, A.TAM_SIMBOLO_GRANDE,
                                    cor if cor is not None else A.PRETO,
                                    cinza=cor is None)
            self.tela.blit(img, (quadro.centerx - img.get_width() // 2,
                                 quadro.centery - img.get_height() // 2))
        y = quadro.bottom + 2 * A.ESPACO
        for i, linha in enumerate(linhas):
            f = self.fonte_negrito if i == 0 else self.fonte
            win98.escrever(self.tela, f, linha, (r.left + 2 * A.ESPACO, y))
            y += self.fonte.get_height() + A.ESPACO

    def texto_relogio(self) -> str:
        """Dia e hora do jogo (1 h = 60 s; ver config.hpp)."""
        r = self.mundo.relogio()
        return f"Dia {r.dia}, {r.hora:02d}:{r.minuto:02d}"

    def desenhar_status(self, r: pygame.Rect) -> None:
        """Barra de baixo: só o tempo e o nome do que está sob o mouse."""
        pygame.draw.rect(self.tela, A.CINZA, r)
        x = r.left + 2
        x = self._campo_status(r, x, self.texto_relogio())
        # Botões de velocidade: o ligado fica afundado.
        self._botoes_velocidade = []
        for texto, mult in VELOCIDADES:
            w = self.fonte.size(texto)[0] + 4 * A.ESPACO
            botao = pygame.Rect(x, r.top + 2, w, r.height - 4)
            win98.caixa(self.tela, botao, saliente=(self.velocidade != mult))
            desloca = 0 if self.velocidade != mult else 1  # "afunda" o texto
            win98.escrever(self.tela, self.fonte_negrito, texto,
                           (botao.left + 2 * A.ESPACO + desloca,
                            botao.top + A.ESPACO - 2 + desloca))
            self._botoes_velocidade.append((botao, mult))
            x += w + 2
        sob_mouse = self.nome_sob_mouse()
        if sob_mouse:
            self._campo_status(r, x, sob_mouse)

    def _campo_status(self, r: pygame.Rect, x: int, p: str) -> int:
        """Um campo afundado da barra de baixo; devolve onde o próximo começa."""
        w = self.fonte.size(p)[0] + 4 * A.ESPACO
        campo = pygame.Rect(x, r.top + 2, w, r.height - 4)
        win98.relevo(self.tela, campo, saliente=False)
        win98.escrever(self.tela, self.fonte, p,
                       (campo.left + 2 * A.ESPACO, campo.top + A.ESPACO - 2))
        return x + w + 2

    def clique_velocidade(self, pos) -> bool:
        """Clique num botão de velocidade. Devolve True se foi num deles."""
        for botao, mult in self._botoes_velocidade:
            if botao.collidepoint(pos):
                # Clicar no que já está ligado volta ao tempo normal (1x).
                self.velocidade = 1 if self.velocidade == mult else mult
                return True
        return False

    def nome_sob_mouse(self):
        """Nome da entidade ou do material sob o mouse (só na aba Mapa)."""
        if self.abas.nomes[self.abas.ativa] != "Mapa" or self.menu.aberto is not None:
            return None
        achado = self.o_que_tem_em(pygame.mouse.get_pos())
        if achado is None:
            return None
        cod, x, y, z = achado
        if cod in (int(OC.NPC), int(OC.CORPO)):
            n = self.npc_em(x, y)
            if n:
                return n.nome if cod == int(OC.NPC) else f"Corpo de {n.nome}"
        if cod == int(OC.MACIEIRA):
            return "Macieira"
        if cod == int(OC.MACIEIRA_SECA):
            return "Macieira seca"
        if cod == int(OC.MUDA):
            return "Muda"
        if cod == int(OC.MACA):
            return "Maçã"
        return MATERIAL[cod - COD_MATERIAL][2]

    def desenhar_info(self, area: pygame.Rect) -> None:
        """Aba Info: informações da simulação e atalhos de teclado."""
        pygame.draw.rect(self.tela, A.CINZA, area)
        campo = area.inflate(-2 * A.ESPACO, -2 * A.ESPACO)
        win98.caixa(self.tela, campo, saliente=False, cor=A.FUNDO_CAMPO)
        zoom = "0,5x" if self.escala == 0.5 else f"{self.escala}x"
        secoes = [
            ("Simulação", [
                f"Tempo: {self.texto_relogio()}  (1 hora do jogo = "
                f"{gm.SEGUNDOS_POR_HORA:.0f} s)",
                f"Velocidade: {self.velocidade}x",
                f"Maçãs no chão: {len(self.mundo.macas())}",
            ]),
            ("Vista", [
                f"Nível z: {self.z_vista:+d}",
                f"Zoom: {zoom}",
                f"Câmera: ({self.cam_x}, {self.cam_y})",
            ]),
            ("Atalhos", [
                "Setas  ou  clicar e arrastar — mover o mapa",
                "+ / -  ou  rodinha do mouse — zoom (aproximar / afastar)",
                "Z / X — descer / subir um nível",
                "Clique (sem arrastar) — selecionar",
                "I — trocar de aba (Mapa, NPCs, Info)",
                "Esc — fechar o menu",
                ">  >>  >>>>  (barra de baixo) — tempo 2x, 4x, 8x; clicar de novo volta a 1x",
            ]),
        ]
        x = campo.left + 3 * A.ESPACO
        y = campo.top + 2 * A.ESPACO
        alt = self.fonte.get_height() + A.ESPACO
        for titulo, linhas in secoes:
            win98.escrever(self.tela, self.fonte_negrito, titulo, (x, y))
            y += alt
            for linha in linhas:
                win98.escrever(self.tela, self.fonte, linha, (x + 2 * A.ESPACO, y))
                y += alt
            y += alt // 2

    def desenhar(self) -> None:
        # Pega a superfície ATUAL da janela. Quando a janela é maximizada ou
        # muda de tamanho, o pygame troca a superfície por uma nova; desenhar
        # na antiga (já descartada) derruba o programa (falha de segmentação).
        self.tela = pygame.display.get_surface()
        self.tela.fill(A.CINZA)
        conteudo, painel, status, abas_y = self.layout()
        self.menu.desenhar(self.tela, self.tela.get_width())
        # Área de conteúdo com relevo; abas por cima, coladas nela.
        win98.caixa(self.tela, conteudo.inflate(4, 4))
        aba = self.abas.nomes[self.abas.ativa]
        if aba == "Mapa":
            self.desenhar_mapa(conteudo)
        elif aba == "NPCs":
            self.desenhar_npcs(conteudo)
        else:
            self.desenhar_info(conteudo)
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

    def arrastar(self, pos) -> None:
        """Clicar e arrastar move a câmera, como as setas.

        O mapa acompanha o mouse ("pega e puxa"): arrastar para a esquerda
        mostra o que está à direita. Só conta como arraste depois de o
        mouse andar pelo menos 1 célula da tela; antes disso, é um clique.
        """
        a = self._arraste
        cel, lado = self._mapa_cel, self._mapa_lado
        dx = int((pos[0] - a["inicio"][0]) / cel)
        dy = int((pos[1] - a["inicio"][1]) / cel)
        if dx == 0 and dy == 0 and not a["arrastou"]:
            return
        a["arrastou"] = True
        self.cam_x = a["cam"][0] - dx * lado
        self.cam_y = a["cam"][1] - dy * lado

    def o_que_tem_em(self, pos):
        """O que aparece no ponto `pos` da tela, no mapa.

        Devolve (código, x, y, z) — z é o nível da célula vista (olhando do
        nível atual para baixo) — ou None se o ponto está fora do mapa.
        """
        if not hasattr(self, "_mapa_area") or not self._mapa_area.collidepoint(pos):
            return None
        area, cel, lado = self._mapa_area, self._mapa_cel, self._mapa_lado
        i = (pos[0] - area.left - 2) // cel
        j = (pos[1] - area.top - 2) // cel
        mx, my = self.cam_x + i * lado, self.cam_y + j * lado
        if not (0 <= mx < self.L and 0 <= my < self.H):
            return None
        vis = self.mundo.visao_bytes(self.z_vista)
        oc = self.mundo.ocupacao_bytes()
        cod, (x, y) = self.codigo_bloco(vis, oc, mx, my, lado)
        z = self.z_vista
        while z > gm.Z_MIN and self.mundo.material(x, y, z) == MAT.AR:
            z -= 1
        return cod, x, y, z

    def clique_mapa(self, pos) -> None:
        achado = self.o_que_tem_em(pos)
        if achado is None:
            return
        cod, x, y, z = achado
        if cod in (int(OC.NPC), int(OC.CORPO)):
            n = self.npc_em(x, y)
            if n:
                self.selecao = ("npc", n.id)
                return
        self.selecao = ("celula", x, y, z)

    def tratar_evento(self, ev) -> None:
        if ev.type == pygame.QUIT:
            self.sair()
        elif ev.type == pygame.MOUSEWHEEL:
            # Rodinha: para cima aproxima, para baixo afasta (como + e -).
            if self.abas.nomes[self.abas.ativa] == "Mapa":
                self.zoom(-1 if ev.y > 0 else +1)
        elif ev.type == pygame.MOUSEMOTION and self._arraste is not None:
            self.arrastar(ev.pos)
        elif ev.type == pygame.MOUSEBUTTONUP and ev.button == 1:
            # Soltou o botão no mapa: se não arrastou, foi um clique (seleciona).
            if self._arraste is not None:
                arrastou = self._arraste["arrastou"]
                self._arraste = None
                if not arrastou:
                    self.clique_mapa(ev.pos)
        elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            if (self.menu.clique(ev.pos) or self.abas.clique(ev.pos)
                    or self.clique_velocidade(ev.pos)):
                return
            conteudo = self.layout()[0]
            if not conteudo.collidepoint(ev.pos):
                return
            if self.abas.nomes[self.abas.ativa] == "Mapa":
                # Começa um possível arraste; a seleção só acontece ao soltar.
                self._arraste = {"inicio": ev.pos, "cam": (self.cam_x, self.cam_y),
                                 "arrastou": False}
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
            elif ev.key == pygame.K_z:  # desce um nível
                self.z_vista = max(gm.Z_MIN, self.z_vista - 1)
            elif ev.key == pygame.K_x:  # sobe um nível
                self.z_vista = min(gm.Z_MAX, self.z_vista + 1)
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
            # Tempo real: o motor anda exatamente o que passou no relógio,
            # vezes a velocidade escolhida (1x, 2x, 4x ou 8x).
            dt_ms = relogio.tick(A.QUADROS_POR_SEGUNDO)
            self.mundo.avancar(dt_ms / 1000.0 * self.velocidade)
            self.desenhar()
        pygame.quit()


if __name__ == "__main__":
    Genesis().rodar()
