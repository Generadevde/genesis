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
import time
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
    int(OC.PEDRA): "⬟",
    int(OC.ABRIGO): "🏠",
}
# Pessoas (pedido do dono do projeto): homem 🧍, mulher 🧍🏼‍♀️, bebê 👶
# desenhado com metade do tamanho da célula.
COD_HOMEM, COD_MULHER, COD_BEBE = 60, 61, 62
SIMBOLO_PESSOA = {COD_HOMEM: "🧍", COD_MULHER: "🧍🏼‍♀️", COD_BEBE: "👶"}
FRACAO_BEBE = 0.5  # o bebê ocupa metade da célula (pedido: "pode ser meio")

# Símbolos que não existem na fonte do mapa: desenhados com a fonte da
# interface (DejaVu Sans), que tem o ⬟.
SIMBOLOS_FONTE_UI = {"⬟"}
MAT = gm.Material
# Materiais do chão: símbolo, cor e nome (escolhidos pelo dono do projeto).
MATERIAL = {
    int(MAT.AR): (" ", A.FUNDO_MAPA, "Ar"),
    int(MAT.GRAMA): ('"', A.COR_GRAMA, "Grama"),
    int(MAT.NEVE): ("n", A.COR_NEVE, "Neve"),
    int(MAT.AREIA): ("a", A.COR_AREIA, "Areia"),
    int(MAT.TERRA): ("d", A.COR_TERRA, "Terra"),
    int(MAT.PEDRA): ("p", A.COR_PEDRA, "Pedra"),
    int(MAT.AGUA): ("~", A.COR_AGUA, "Água"),
    int(MAT.GELO): ("g", A.COR_GELO, "Gelo"),
}

NOME_ESTADO = {
    EST.VAGANDO: "explorando",
    EST.INDO_COMER: "indo comer",
    EST.COMENDO: "comendo",
    EST.MORTO: "morto",
    EST.INDO_BEBER: "indo beber",
    EST.BEBENDO: "bebendo",
    EST.INDO_PEGAR: "indo pegar maçã",
    EST.INDO_TRABALHAR: "indo trabalhar",
    EST.TRABALHANDO: "trabalhando",
    EST.INDO_ABRIGO: "indo para o abrigo (frio)",
    EST.DORMINDO: "dormindo no abrigo",
    EST.INDO_PROCRIAR: "indo ao abrigo com o par (procriar)",
    EST.PROCRIANDO: "no abrigo com o par (procriar)",
    EST.BEBE: "bebê",
    EST.INDO_GUARDAR: "levando maçãs para o abrigo",
}

SEXO = gm.Sexo
REG = gm.TipoRegistro
# Categorias do histórico (filtros da aba Histórico).
CATEGORIA = {
    REG.INICIO: "Pessoas", REG.NASCIMENTO: "Pessoas", REG.MORTE: "Pessoas",
    REG.CASAL: "Pessoas", REG.GRAVIDEZ: "Pessoas", REG.VIROU_ADULTO: "Pessoas",
    REG.BEBE_PERDIDO: "Pessoas",
    REG.ABRIGO_FEITO: "Trabalho", REG.MACHADO_FEITO: "Trabalho",
    REG.ARVORE_CORTADA: "Trabalho", REG.PEDRA_ESGOTOU: "Trabalho",
    REG.MUDA_NASCEU: "Plantas", REG.ARVORE_NASCEU: "Plantas",
    REG.ARVORE_SECOU: "Plantas", REG.ARVORE_SUMIU: "Plantas",
    REG.ESTACAO: "Estações", REG.CONGELOU: "Estações", REG.DERRETEU: "Estações",
}
CATEGORIAS = ["Pessoas", "Trabalho", "Plantas", "Estações"]

ESTACAO = gm.Estacao
NOME_ESTACAO = {ESTACAO.VERAO: "Verão", ESTACAO.OUTONO: "Outono",
                ESTACAO.INVERNO: "Inverno", ESTACAO.PRIMAVERA: "Primavera"}
# O que a macieira está fazendo em cada estação (pedido do dono do projeto).
FASE_MACIEIRA = {ESTACAO.PRIMAVERA: "florescendo", ESTACAO.VERAO: "dando frutos",
                 ESTACAO.OUTONO: "folhas secas", ESTACAO.INVERNO: "repouso"}


def causas_morte(bits: int) -> str:
    nomes = [c for b, c in ((1, "fome"), (2, "sede"), (4, "frio"), (8, "velhice")) if bits & b]
    return " e ".join(nomes) if nomes else "?"


def data_texto(d) -> str:
    """'01/01/0001' (dia/mês/ano) de uma Data do motor."""
    return f"{d.dia:02d}/{d.mes:02d}/{d.ano:04d}"


def relogio_de(tempo: float) -> str:
    """'01/01/0001 07:00' para um tempo da simulação (segundos)."""
    d = gm.Mundo.data_de(tempo)
    return f"{data_texto(d)} {d.hora:02d}:{d.minuto:02d}"


def hora_texto(horas: float) -> str:
    """6.25 -> '06:15'."""
    m = int(round(horas * 60))
    return f"{m // 60:02d}:{m % 60:02d}"


TAREFA = gm.Tarefa
NOME_TAREFA = {
    TAREFA.NENHUMA: "nenhum",
    TAREFA.LASCAS: "tirar lascas da pedra",
    TAREFA.GALHOS: "tirar galhos da árvore",
    TAREFA.CORTAR: "cortar árvore (lenha)",
    TAREFA.CONSTRUIR: "construir abrigo",
    TAREFA.MACHADO: "fazer machado",
}

# Zoom (escolhido pelo dono do projeto): + aproxima, - afasta.
# Quantas células do mapa cabem em 1 célula da tela, em cada eixo:
#   0.5 -> aproximado: cada célula do mapa fica do tamanho de 2 x 2
#   1   -> normal
#   2, 4, 6 -> afastado: cada célula da tela mostra um bloco 2x2, 4x4, 6x6
ESCALAS = [0.5, 1, 2, 4, 6]

# Botões de velocidade ao lado do tempo (pedido do dono do projeto em
# 15/11/2026): quantos segundos do jogo andam por segundo real.
#   ⏸ = pausa;  > = normal (1 s por s);  >> = 1 minuto por s;
#   >>> = 1 hora por s;  >>>> = 1 dia por s.
VELOCIDADES = [("||", 0), (">", 1), (">>", 60), (">>>", 3600), (">>>>", 86400)]
NOME_VELOCIDADE = {0: "pausado", 1: "normal (1 s/s)", 60: "1 min/s",
                   3600: "1 h/s", 86400: "1 dia/s"}

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
    int(OC.ABRIGO): 8.5, int(OC.PEDRA): 5.6,      # PROVISÓRIO
    COD_HOMEM: 9, COD_MULHER: 9, COD_BEBE: 9,
    COD_MATERIAL + int(MAT.GRAMA): 5, COD_MATERIAL + int(MAT.NEVE): 5,
    COD_MATERIAL + int(MAT.AREIA): 5,
    COD_MATERIAL + int(MAT.AGUA): 4, COD_MATERIAL + int(MAT.GELO): 4,
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
    """Tempo do jogo por extenso: "3 anos 20 dias", "2 dias 5 h",
    "3 h 20 min", "4 min 10 s", "45 s"."""
    s = max(0, int(segundos))
    anos, s = divmod(s, int(gm.SEGUNDOS_POR_ANO))
    dias, s = divmod(s, int(gm.SEGUNDOS_POR_DIA))
    horas, s = divmod(s, 3600)
    mins, s = divmod(s, 60)
    if anos:
        return f"{anos} ano{'s' if anos > 1 else ''} {dias} dia{'s' if dias != 1 else ''}"
    if dias:
        return f"{dias} dia{'s' if dias > 1 else ''} {horas} h"
    if horas:
        return f"{horas} h {mins} min"
    if mins:
        return f"{mins} min {s} s"
    return f"{s} s"


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

    def _fonte_ui(self, tamanho: int) -> pygame.font.Font:
        chave = ("ui", tamanho)
        if chave not in self._fontes_mapa:
            self._fontes_mapa[chave] = pygame.font.Font(A.FONTE_UI, tamanho)
        return self._fontes_mapa[chave]

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
            elif simbolo in SIMBOLOS_FONTE_UI:
                img = self._fonte_ui(lado).render(simbolo, True, cor)
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
                ("Nova simulação", self.abrir_dialogo_nova),
                ("Exibir", self.exibir_informacoes),
                None,
                ("Sair", self.sair),
            ]),
        ])
        self.abas = win98.Abas(self.fonte, ["Mapa", "NPCs", "Histórico", "Info"])

        self.rodando = True
        self.dialogo = None          # janela "Nova simulação" aberta
        self.rolagem_npcs = 0        # primeira linha mostrada na tabela de NPCs
        self.rolagem_hist = 0        # primeira linha mostrada no histórico
        self.filtros = {c: True for c in CATEGORIAS}  # PROVISÓRIO: tudo ligado
        self._arraste = None  # clique-e-arraste do mapa em andamento
        self.velocidade = 1   # segundos do jogo por segundo real (0 = pausa)
        self.velocidade_real = 1.0    # o que o motor conseguiu de verdade
        self._botoes_velocidade = []  # (retângulo, velocidade)
        self._fontes_pilha = {}       # fonte do "xN" por tamanho
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
    def nova_simulacao(self, homens: int = gm.HOMENS_PADRAO,
                       mulheres: int = gm.MULHERES_PADRAO) -> None:
        """Começa um mundo novo, com semente sorteada e esses NPCs."""
        semente = int.from_bytes(os.urandom(8), "little")
        self.mundo = gm.Mundo(semente, homens, mulheres)
        self.historico = []   # acontecimentos já formatados (aba Histórico)
        self.rolagem_npcs = self.rolagem_hist = 0
        self.L, self.H = self.mundo.largura, self.mundo.altura
        # Nível z que se está vendo. Começa no topo dos morros (+4), assim
        # a ilha inteira aparece vista de cima — PROVISÓRIO.
        self.z_vista = gm.SUPERFICIE_MAX
        self.cam_x = self.cam_y = None  # centraliza no próximo quadro
        self.selecao = None  # ("npc", id) ou ("celula", x, y, z)

    # --- Janela "Nova simulação" -------------------------------------------
    def abrir_dialogo_nova(self) -> None:
        """Abre a janela com os campos de homens e mulheres."""
        self.dialogo = {"homens": str(gm.HOMENS_PADRAO),
                        "mulheres": str(gm.MULHERES_PADRAO), "foco": "homens"}

    def desenhar_dialogo(self) -> None:
        d = self.dialogo
        alt = self.fonte.get_height() + 2 * A.ESPACO
        largura = (self.fonte.size("Mulheres:")[0] + A.LARGURA_CAMPO_NUMERO
                   + 8 * A.ESPACO)
        altura = 5 * alt + 6 * A.ESPACO
        tela_l, tela_a = self.tela.get_size()
        r = pygame.Rect((tela_l - largura) // 2, (tela_a - altura) // 2, largura, altura)
        self._dlg_fechar = win98.janela(self.tela, r, self.fonte_negrito, "Nova simulação")
        x_rot = r.left + 2 * A.ESPACO
        x_campo = x_rot + self.fonte.size("Mulheres:")[0] + 2 * A.ESPACO
        y = r.top + alt + 2 * A.ESPACO
        self._dlg_campos = {}
        for chave, rotulo in (("homens", "Homens:"), ("mulheres", "Mulheres:")):
            win98.escrever(self.tela, self.fonte, rotulo, (x_rot, y + A.ESPACO))
            campo = pygame.Rect(x_campo, y, A.LARGURA_CAMPO_NUMERO, alt)
            win98.caixa(self.tela, campo, saliente=False, cor=A.FUNDO_CAMPO)
            texto = d[chave] + ("|" if d["foco"] == chave else "")  # cursor
            win98.escrever(self.tela, self.fonte, texto, (campo.left + A.ESPACO, y + A.ESPACO))
            self._dlg_campos[chave] = campo
            y += alt + A.ESPACO
        y += A.ESPACO
        bw = self.fonte.size("Cancelar")[0] + 4 * A.ESPACO
        self._dlg_ok = pygame.Rect(r.right - 2 * bw - 3 * A.ESPACO, y, bw, alt)
        self._dlg_cancelar = pygame.Rect(r.right - bw - 2 * A.ESPACO, y, bw, alt)
        win98.botao(self.tela, self._dlg_ok, self.fonte, "OK")
        win98.botao(self.tela, self._dlg_cancelar, self.fonte, "Cancelar")

    def confirmar_dialogo(self) -> None:
        d = self.dialogo
        self.dialogo = None
        self.nova_simulacao(int(d["homens"] or 0), int(d["mulheres"] or 0))

    def evento_dialogo(self, ev) -> None:
        """Enquanto a janela está aberta, só ela recebe mouse e teclado."""
        d = self.dialogo
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            for chave, campo in self._dlg_campos.items():
                if campo.collidepoint(ev.pos):
                    d["foco"] = chave
            if self._dlg_ok.collidepoint(ev.pos):
                self.confirmar_dialogo()
            elif self._dlg_cancelar.collidepoint(ev.pos) or self._dlg_fechar.collidepoint(ev.pos):
                self.dialogo = None
        elif ev.type == pygame.KEYDOWN:
            if ev.key in (pygame.K_RETURN, pygame.K_KP_ENTER):
                self.confirmar_dialogo()
            elif ev.key == pygame.K_ESCAPE:
                self.dialogo = None
            elif ev.key == pygame.K_TAB:
                d["foco"] = "mulheres" if d["foco"] == "homens" else "homens"
            elif ev.key == pygame.K_BACKSPACE:
                d[d["foco"]] = d[d["foco"]][:-1]
            elif ev.unicode.isdigit():  # só números (sem máximo)
                d[d["foco"]] = (d[d["foco"]] + ev.unicode).lstrip("0") or "0"

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

    @staticmethod
    def codigo_pessoa(n) -> int:
        if n.bebe:
            return COD_BEBE
        return COD_MULHER if n.sexo == SEXO.MULHER else COD_HOMEM

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
        # Quem está em cada célula (para desenhar homem / mulher / bebê).
        pessoas = {(n.pos.x, n.pos.y): self.codigo_pessoa(n) for n in self.mundo.npcs()
                   if n.estado != EST.MORTO and n.dentro_abrigo < 0}
        # Pilhas de maçãs: quantas em cada célula (para o "xN").
        pilhas = {(x, y): q for x, y, q in self.mundo.pilhas()}
        ox, oy = area.left + 2, area.top + 2
        anterior = self.tela.get_clip()
        self.tela.set_clip(area.inflate(-4, -4))
        for j, my in enumerate(range(self.cam_y, self.cam_y + vis_a, lado)):
            for i, mx in enumerate(range(self.cam_x, self.cam_x + vis_l, lado)):
                if lado == 1:  # zoom normal/aproximado: célula direta
                    k = my * self.L + mx
                    cod = decodificar(vis[k], oc[k])
                    onde = (mx, my)
                else:
                    cod, onde = self.codigo_bloco(vis, oc, mx, my, lado)
                if cod == int(OC.NPC):
                    cod = pessoas.get(onde, COD_HOMEM)
                img = self.imagem(cod, cel)
                if img is None:
                    continue
                px = ox + i * cel + (cel - img.get_width()) // 2
                py = oy + j * cel + (cel - img.get_height()) // 2
                self.tela.blit(img, (px, py))
                if cod == int(OC.MACA) and pilhas.get(onde, 0) > 1:
                    self.desenhar_pilha(pilhas[onde], ox + i * cel, oy + j * cel, cel)
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

    def desenhar_pilha(self, quantas: int, x: int, y: int, cel: int) -> None:
        """O pequeno "xN" no canto de baixo da célula (pedido do dono)."""
        tam = max(8, int(cel * A.FRACAO_FONTE_PILHA))
        fonte = self._fontes_pilha.get(tam)
        if fonte is None:
            fonte = self._fontes_pilha[tam] = pygame.font.Font(A.FONTE_UI_NEGRITO, tam)
        img = fonte.render(f"x{quantas}", True, A.COR_PILHA)
        fundo = pygame.Surface((img.get_width() + 2, img.get_height()), pygame.SRCALPHA)
        fundo.fill((0, 0, 0, 160))
        bx, by = x + cel - fundo.get_width(), y + cel - fundo.get_height()
        self.tela.blit(fundo, (bx, by))
        self.tela.blit(img, (bx + 1, by))

    @staticmethod
    def texto_carregando(n) -> str:
        """O que o NPC leva: maçã, lascas, galhos, lenha, machado."""
        partes = []
        if n.macas_carregadas:
            partes.append("1 maçã" if n.macas_carregadas == 1
                          else f"{n.macas_carregadas} maçãs")
        for qtd, um, varios in ((n.lascas, "lasca", "lascas"),
                                (n.galhos, "galho", "galhos"),
                                (n.lenha, "lenha", "lenhas")):
            if qtd:
                partes.append(f"{qtd} {um if qtd == 1 else varios}")
        if n.machado:
            partes.append("machado")
        return ", ".join(partes) if partes else "nada"

    @staticmethod
    def texto_tarefa(n) -> str:
        texto = NOME_TAREFA[n.tarefa]
        if n.tarefa != TAREFA.NENHUMA and n.tarefa_restante > 0:
            texto += f" (parado, falta {duracao(n.tarefa_restante)})"
        return texto

    def texto_abrigo(self, n) -> str:
        if n.abrigo_casa < 0:
            return "não tem"
        a = self.mundo.abrigos()[n.abrigo_casa]
        onde = "dentro" if n.dentro_abrigo >= 0 else "fora"
        return f"({a.pos.x}, {a.pos.y}, {a.pos.z:+d}) — {onde}"

    @staticmethod
    def texto_estado(n) -> str:
        """Estado do NPC; indo a um lugar lembrado ganha "(memória)"."""
        texto = NOME_ESTADO[n.estado]
        if n.estado == EST.COMENDO:
            return ("comendo uma maçã que carregava", "comendo maçã do abrigo",
                    "comendo maçã do chão")[n.comendo_de]
        if n.pela_memoria and n.estado in (EST.INDO_COMER, EST.INDO_BEBER):
            texto += " (memória)"
        return texto

    def imagem(self, cod: int, lado: int):
        """Desenho de um código (ocupante ou material) no tamanho `lado`."""
        if cod in SIMBOLO_PESSOA:
            tam = max(1, int(lado * FRACAO_BEBE)) if cod == COD_BEBE else lado
            return self.simbolos.get(SIMBOLO_PESSOA[cod], tam)
        if cod >= COD_MATERIAL:
            simb, cor, _ = MATERIAL[cod - COD_MATERIAL]
            if simb == " ":
                return None  # ar sem nada embaixo: fica preto
            return self.simbolos.get(simb, lado, cor)
        cor = {int(OC.CORPO): A.COR_CRUZ, int(OC.MUDA): A.COR_MUDA,
               int(OC.PEDRA): A.COR_PEDRA_ENTIDADE}.get(cod, A.PRETO)
        return self.simbolos.get(SIMBOLO[cod], lado, cor,
                                 cinza=(cod == int(OC.MACIEIRA_SECA)))

    def texto_idade(self, n) -> str:
        anos = int(self.mundo.idade_anos(n) + 1e-9)
        if n.bebe and anos < 1:
            fim = n.morreu_em if n.morreu_em >= 0 else self.mundo.tempo
            return "bebê, " + duracao(fim - n.nascimento)
        texto = f"{anos} ano{'s' if anos != 1 else ''}"
        return ("bebê, " + texto) if n.bebe else texto

    @staticmethod
    def texto_nascimento(n) -> str:
        d = gm.Mundo.data_de(n.nascimento)
        if d.ano < 1:  # os do começo nasceram antes do ano 1
            return "antes do ano 1"
        return f"{data_texto(d)} {d.hora:02d}:{d.minuto:02d}"

    def nome_de(self, i: int) -> str:
        return self.mundo.npcs()[i].nome if i >= 0 else "—"

    def linhas_tabela(self):
        return [[n.nome, "homem" if n.sexo == SEXO.HOMEM else "mulher",
                 self.texto_idade(n), f"{n.hp:.0f}", f"{n.fome:.0f}", f"{n.sede:.0f}",
                 self.texto_estado(n), f"({n.pos.x}, {n.pos.y}, {n.pos.z:+d})",
                 self.texto_carregando(n), self.nome_de(n.parceiro)]
                for n in self.mundo.npcs()]

    def desenhar_npcs(self, area: pygame.Rect) -> None:
        pygame.draw.rect(self.tela, A.CINZA, area)
        sel = self.selecao[1] if self.selecao and self.selecao[0] == "npc" else None
        campo = area.inflate(-2 * A.ESPACO, -2 * A.ESPACO)
        linhas = self.linhas_tabela()
        cabem = win98.linhas_visiveis(campo, self.fonte)
        self.rolagem_npcs = max(0, min(self.rolagem_npcs, len(linhas) - cabem))
        self._linhas_rects = win98.tabela(
            self.tela, campo, self.fonte,
            ["NPC", "Sexo", "Idade", "HP", "Fome", "Sede", "Estado", "Posição",
             "Carregando", "Par"],
            linhas, selecionada=sel, inicio=self.rolagem_npcs)

    # --- Histórico --------------------------------------------------------
    def texto_registro(self, r) -> str:
        nome = self.nome_de
        p = f"({r.pos.x}, {r.pos.y})"
        t = r.tipo
        if t == REG.INICIO:
            return f"A simulação começou: {r.a} homens e {r.b} mulheres"
        if t == REG.NASCIMENTO:
            f = self.mundo.npcs()[r.a]
            sexo = "menino" if f.sexo == SEXO.HOMEM else "menina"
            return f"Nasceu {f.nome} ({sexo}) — mãe {nome(r.b)}, pai {nome(r.c)}"
        if t == REG.MORTE:
            return f"{nome(r.a)} morreu ({causas_morte(r.b)})"
        if t == REG.CASAL:
            return f"{nome(r.a)} e {nome(r.b)} formaram um casal"
        if t == REG.GRAVIDEZ:
            return f"{nome(r.a)} engravidou (pai {nome(r.b)})"
        if t == REG.VIROU_ADULTO:
            return f"{nome(r.a)} virou adulto"
        if t == REG.BEBE_PERDIDO:
            return f"{nome(r.a)} morreu grávida; o bebê também"
        if t == REG.ABRIGO_FEITO:
            return f"{nome(r.a)} construiu um abrigo em {p}"
        if t == REG.MACHADO_FEITO:
            return f"{nome(r.a)} fez um machado primitivo"
        if t == REG.ARVORE_CORTADA:
            return f"{nome(r.a)} cortou uma árvore em {p}"
        if t == REG.PEDRA_ESGOTOU:
            return f"A pedra em {p} esgotou e sumiu"
        if t == REG.MUDA_NASCEU:
            return f"Uma maçã esquecida virou muda em {p}"
        if t == REG.ARVORE_NASCEU:
            return f"Uma muda virou macieira em {p}"
        if t == REG.ARVORE_SECOU:
            return f"A macieira em {p} secou"
        if t == REG.ARVORE_SUMIU:
            return f"A macieira seca em {p} sumiu"
        return str(t)

    def atualizar_historico(self) -> None:
        """Busca no motor só os acontecimentos novos e guarda já formatados."""
        if self.mundo.total_historico() == len(self.historico):
            return
        for r in self.mundo.historico(len(self.historico)):
            self.historico.append((CATEGORIA.get(r.tipo, "Pessoas"),
                                   relogio_de(r.tempo), self.texto_registro(r)))

    def desenhar_historico(self, area: pygame.Rect) -> None:
        pygame.draw.rect(self.tela, A.CINZA, area)
        # Filtros: um botão por categoria (afundado = mostrando).
        x, y = area.left + A.ESPACO, area.top + A.ESPACO
        alt = self.fonte.get_height() + 2 * A.ESPACO
        win98.escrever(self.tela, self.fonte, "Mostrar:", (x, y + A.ESPACO))
        x += self.fonte.size("Mostrar:")[0] + 2 * A.ESPACO
        self._botoes_filtro = []
        for cat in CATEGORIAS:
            w = self.fonte.size(cat)[0] + 4 * A.ESPACO
            b = pygame.Rect(x, y, w, alt)
            win98.botao(self.tela, b, self.fonte, cat, apertado=self.filtros[cat])
            self._botoes_filtro.append((b, cat))
            x += w + A.ESPACO
        campo = pygame.Rect(area.left + A.ESPACO, y + alt + A.ESPACO,
                            area.width - 2 * A.ESPACO,
                            area.bottom - (y + alt + 2 * A.ESPACO))
        # Mais novo primeiro.
        itens = [h for h in reversed(self.historico) if self.filtros[h[0]]]
        linhas = [[quando, cat, texto] for cat, quando, texto in itens]
        cabem = win98.linhas_visiveis(campo, self.fonte)
        self.rolagem_hist = max(0, min(self.rolagem_hist, len(linhas) - cabem))
        win98.tabela(self.tela, campo, self.fonte, ["Quando", "Tipo", "O que aconteceu"],
                     linhas or [["", "", "(nada ainda)"]], inicio=self.rolagem_hist)

    def descrever_selecao(self):
        """Símbolo, cor e linhas de texto do que está selecionado."""
        if self.selecao is None:
            return None, None, ["Clique em algo no mapa."]
        if self.selecao[0] == "npc":
            n = self.mundo.npcs()[self.selecao[1]]
            if n.estado == EST.MORTO:
                causa = causas_morte(n.causa_morte)
                return "✝", A.PRETO, [f"Corpo de {n.nome}",
                                      f"Morreu (HP 0) de: {causa}",
                                      f"Posição: ({n.pos.x}, {n.pos.y}, {n.pos.z:+d})"]
            linhas = [n.nome,
                      f"Sexo: {'homem' if n.sexo == SEXO.HOMEM else 'mulher'}",
                      f"Idade: {self.texto_idade(n)}",
                      f"Nascimento: {self.texto_nascimento(n)}",
                      *([f"Vira adulto em: {relogio_de(n.quando_adulto)}"] if n.bebe else []),
                      f"Mãe: {self.nome_de(n.mae)}   Pai: {self.nome_de(n.pai)}",
                      f"Par: {self.nome_de(n.parceiro)}"
                      + (f"   Filhos: {n.filhos}" if n.filhos else ""),
                      f"HP: {n.hp:.0f}",
                      f"Fome: {n.fome:.0f}",
                      f"Sede: {n.sede:.0f}",
                      f"Estado: {self.texto_estado(n)}",
                      f"Carregando: {self.texto_carregando(n)}",
                      *([f"Grávida: nasce em {relogio_de(n.quando_nascer)}"]
                        if n.gravida else []),
                      f"Trabalho: {self.texto_tarefa(n)}",
                      f"Abrigo: {self.texto_abrigo(n)}",
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
            return SIMBOLO_PESSOA[self.codigo_pessoa(n)], A.PRETO, linhas
        _, x, y, z = self.selecao
        pos = f"Posição: ({x}, {y}, {z:+d})"
        # Árvore e maçã ficam na superfície.
        if z == self.mundo.superficie(x, y):
            o = self.mundo.ocupante(x, y)
            agora = self.mundo.tempo
            if o in (OC.MACIEIRA, OC.MACIEIRA_SECA):
                k = self.mundo.macieira_em(x, y)
                m = self.mundo.macieiras()[k]
                linhas = ["Macieira seca" if m.seca else "Macieira", pos,
                          f"Idade: {duracao(agora - m.nasceu)}"]
                if m.seca:
                    linhas.append("Fase: seca (sem maçãs)")
                    linhas.append(f"Some em: {relogio_de(m.quando_sumir)}")
                else:
                    linhas.append(f"Fase: {FASE_MACIEIRA[self.mundo.estacao()]}")
                    if m.macas_verao and self.mundo.estacao() == ESTACAO.VERAO:
                        linhas.append(f"Maçãs neste verão: {m.macas_verao}")
                        linhas.append(f"Ainda vão cair: {self.mundo.macas_por_cair(k)}")
                    linhas.append(f"Seca em: {relogio_de(m.quando_secar)}")
                # cor None = desenhar em cinza (macieira seca)
                return "🌳", (None if m.seca else A.PRETO), linhas
            if o == OC.MACA:
                vencem = self.mundo.macas_vencem(x, y)
                return "🍎", A.PRETO, [
                    f"Maçãs: {len(vencem)} (cabem {gm.MACAS_POR_PILHA})", pos,
                    f"A próxima vence em: {duracao(vencem[0])}",
                    "Ao vencer: 40% vira muda,", "60% apodrece"]
            if o == OC.PEDRA:
                p = self.mundo.pedras()[self.mundo.pedra_em(x, y)]
                return "⬟", A.COR_PEDRA_ENTIDADE, ["Pedra", pos,
                                                   f"Lascas: {p.lascas}"]
            if o == OC.ABRIGO:
                a = self.mundo.abrigos()[self.mundo.abrigo_em(x, y)]
                nomes = {n.id: n.nome for n in self.mundo.npcs()}
                mor = ", ".join(nomes[i] for i in a.moradores) or "ninguém"
                den = ", ".join(nomes[i] for i in a.dentro) or "ninguém"
                return "🏠", A.PRETO, ["Abrigo", pos, f"Moradores: {mor}",
                                       f"Dentro agora: {den}",
                                       f"Maçãs guardadas: {a.macas}/{gm.ABRIGO_MACAS_MAX}"]
            if o == OC.MUDA:
                u = self.mundo.muda_em(x, y)
                return "♣", A.COR_MUDA, ["Muda", pos,
                                         f"Idade: {duracao(agora - u.nasceu)}",
                                         f"Vira macieira em: {relogio_de(u.quando_virar)}"]
        m = int(self.mundo.material(x, y, z))
        simb, cor, nome = MATERIAL[m]
        linhas = [nome, pos]
        if m in (int(MAT.AGUA), int(MAT.GELO)):
            if m == int(MAT.GELO):
                linhas.append("Água congelada: dá para andar, não para beber")
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
        fundo = A.FUNDO_MAPA if simb and simb in "\"ndpa~g♣⬟ " else A.FUNDO_CAMPO
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

    def texto_temperatura(self) -> str:
        periodo = "dia" if self.mundo.sol() else "noite"
        return f"{self.mundo.temperatura():.1f} °C ({periodo})"

    def texto_relogio(self) -> str:
        """Data, hora e estação (pedido do dono: a estação aparece embaixo)."""
        d = self.mundo.data()
        return (f"{data_texto(d)} {d.hora:02d}:{d.minuto:02d}:{d.segundo:02d}"
                f" — {NOME_ESTACAO[self.mundo.estacao()]}")

    def desenhar_status(self, r: pygame.Rect) -> None:
        """Barra de baixo: só o tempo e o nome do que está sob o mouse."""
        pygame.draw.rect(self.tela, A.CINZA, r)
        x = r.left + 2
        x = self._campo_status(r, x, self.texto_relogio())
        # Temperatura (uma só para o mundo todo, por enquanto) — é a do
        # lugar onde o mouse está, já que hoje é igual em todo lugar.
        x = self._campo_status(r, x, self.texto_temperatura())
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
        # Se o computador não dá conta da velocidade pedida, mostra a real.
        if self.velocidade > 1 and self.velocidade_real < 0.9 * self.velocidade:
            x = self._campo_status(r, x, f"real: {self.texto_real()}")
        sob_mouse = self.nome_sob_mouse()
        if sob_mouse:
            self._campo_status(r, x, sob_mouse)

    def texto_real(self) -> str:
        """Velocidade de verdade, na unidade que fizer mais sentido."""
        v = self.velocidade_real
        if v >= gm.SEGUNDOS_POR_DIA:
            return f"{v / gm.SEGUNDOS_POR_DIA:.1f} dia/s"
        if v >= 3600:
            return f"{v / 3600:.1f} h/s"
        if v >= 60:
            return f"{v / 60:.1f} min/s"
        return f"{v:.1f} s/s"

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
                self.velocidade = mult  # um botão sempre ligado (o escolhido)
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
        if cod == int(OC.PEDRA):
            return "Pedra"
        if cod == int(OC.ABRIGO):
            return "Abrigo"
        if cod == int(OC.MACA):
            q = self.mundo.macas_em(x, y)
            return "Maçã" if q == 1 else f"Maçãs (x{q})"
        return MATERIAL[cod - COD_MATERIAL][2]

    def desenhar_info(self, area: pygame.Rect) -> None:
        """Aba Info: informações da simulação e atalhos de teclado."""
        pygame.draw.rect(self.tela, A.CINZA, area)
        campo = area.inflate(-2 * A.ESPACO, -2 * A.ESPACO)
        win98.caixa(self.tela, campo, saliente=False, cor=A.FUNDO_CAMPO)
        zoom = "0,5x" if self.escala == 0.5 else f"{self.escala}x"
        secoes = [
            ("Simulação", [
                f"Data: {self.texto_relogio()}",
                f"Sol: nasce {hora_texto(gm.Mundo.nascer_do_sol(self.mundo.tempo))}, "
                f"se põe {hora_texto(gm.Mundo.por_do_sol(self.mundo.tempo))} "
                f"({hora_texto(gm.Mundo.horas_de_sol(self.mundo.tempo))} de sol)",
                f"Velocidade: {NOME_VELOCIDADE[self.velocidade]}"
                + (f" — de verdade: {self.texto_real()}" if self.velocidade > 1 else ""),
                f"Temperatura: {self.texto_temperatura()}  (hoje: mín. "
                f"{gm.Mundo.min_max_temperatura(self.mundo.tempo)[0]:.1f} °C, máx. "
                f"{gm.Mundo.min_max_temperatura(self.mundo.tempo)[1]:.1f} °C)",
                f"Maçãs no chão: {sum(q for _, _, q in self.mundo.pilhas())}",
                f"Maçãs nos abrigos: {sum(a.macas for a in self.mundo.abrigos())}",
                "Calendário: 1 s do jogo = 1 s real (na velocidade normal); "
                "ano de 365 dias, 12 meses",
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
                "I — trocar de aba (Mapa, NPCs, Histórico, Info)",
                "Rodinha nas abas NPCs e Histórico — rolar a lista",
                "Arquivo → Nova simulação — escolher quantos homens e mulheres",
                "Esc — fechar o menu",
                "||  >  >>  >>>  >>>>  (barra de baixo) — pausa, normal, "
                "1 minuto/s, 1 hora/s, 1 dia/s",
                "Espaço — pausa / continua",
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
        self.atualizar_historico()
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
        elif aba == "Histórico":
            self.desenhar_historico(conteudo)
        else:
            self.desenhar_info(conteudo)
        self.abas.desenhar(self.tela, conteudo.left, abas_y)
        self.desenhar_painel(painel)
        self.desenhar_status(status)
        self.menu.desenhar_aberto(self.tela)  # por último: fica por cima
        if self.dialogo is not None:
            self.desenhar_dialogo()
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
            return
        if self.dialogo is not None:
            self.evento_dialogo(ev)
            return
        aba = self.abas.nomes[self.abas.ativa]
        if ev.type == pygame.MOUSEWHEEL:
            # Rodinha: no mapa, zoom (para cima aproxima); nas listas, rola.
            if aba == "Mapa":
                self.zoom(-1 if ev.y > 0 else +1)
            elif aba == "NPCs":
                self.rolagem_npcs = max(0, self.rolagem_npcs - ev.y * A.LINHAS_POR_RODINHA)
            elif aba == "Histórico":
                self.rolagem_hist = max(0, self.rolagem_hist - ev.y * A.LINHAS_POR_RODINHA)
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
                if aba == "NPCs":
                    for i, r in getattr(self, "_linhas_rects", []):
                        if r.collidepoint(ev.pos):
                            self.selecao = ("npc", i)
                elif aba == "Histórico":
                    for b, cat in getattr(self, "_botoes_filtro", []):
                        if b.collidepoint(ev.pos):
                            self.filtros[cat] = not self.filtros[cat]
                            self.rolagem_hist = 0
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
            elif ev.key == pygame.K_SPACE:  # pausa / continua (PROVISÓRIO)
                if self.velocidade:
                    self._antes_da_pausa, self.velocidade = self.velocidade, 0
                else:
                    self.velocidade = getattr(self, "_antes_da_pausa", 1)

    # ------------------------------------------------------------------
    # Laço principal
    # ------------------------------------------------------------------
    def rodar(self) -> None:
        relogio = pygame.time.Clock()
        while self.rodando:
            for ev in pygame.event.get():
                self.tratar_evento(ev)
            dt_ms = relogio.tick(A.QUADROS_POR_SEGUNDO)
            self.andar_tempo(dt_ms / 1000.0)
            self.desenhar()
        pygame.quit()

    def andar_tempo(self, dt_real: float) -> None:
        """Anda o motor dt_real segundos reais × a velocidade escolhida.

        Nas velocidades altas o motor pode não dar conta de tudo dentro do
        quadro: ele anda em pedaços até gastar ORCAMENTO_MOTOR_MS e o resto é
        deixado de lado (o tempo do jogo anda menos). A velocidade de
        verdade fica em self.velocidade_real (média suave).
        """
        pedido = dt_real * self.velocidade
        if pedido <= 0:
            self.velocidade_real = 0.0
            return
        inicio = time.perf_counter()
        limite = inicio + A.ORCAMENTO_MOTOR_MS / 1000.0
        pedaco = max(pedido / 8, 1.0)
        andou = 0.0
        while andou < pedido:
            passo = min(pedaco, pedido - andou)
            self.mundo.avancar(passo)
            andou += passo
            if time.perf_counter() > limite:
                break
        if dt_real > 0:
            agora = andou / dt_real
            self.velocidade_real = 0.8 * self.velocidade_real + 0.2 * agora


if __name__ == "__main__":
    Genesis().rodar()
