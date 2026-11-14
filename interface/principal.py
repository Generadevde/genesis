"""
principal.py — interface do Genesis (Python + rich).

O que este arquivo faz:
    1. Cria o mundo no motor em C++ (módulo `genesis_motor`, via pybind11).
    2. Em tempo real, pede ao motor para andar o tempo que passou no relógio
       de verdade, e desenha no terminal o pedaço da ilha que cabe na tela.
    3. As SETAS do teclado movem a câmera (a vista) pelo mapa;
       + e - mudam o zoom (0,5x, 1x, 2x, 4x, 6x).
    4. A tecla I abre/fecha a tela da tabela dos NPCs.

Regra da STACK.md: a interface só MOSTRA e CONTROLA. Nenhuma regra da
simulação mora aqui (fome, maçãs, passos... tudo isso está no motor, em C++).
A câmera é só da tela: mexer nela não muda nada no mundo.

Sem menus (pedido do dono do projeto): abre direto na simulação.
Sair: Ctrl+C ou a tecla q.
"""

import os
import select
import sys
import termios
import time
import tty
from pathlib import Path

from rich.console import Console, Group
from rich.live import Live
from rich.table import Table
from rich.text import Text

# O módulo compilado fica na pasta build/ na raiz do projeto.
RAIZ = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RAIZ / "build"))

import genesis_motor as gm  # noqa: E402  (precisa vir depois do ajuste do caminho)

# ---------------------------------------------------------------------------
# Símbolos (escolhidos pelo dono do projeto, docs/ILHA_V1.md).
# Os emojis ocupam 2 colunas no terminal; por isso TODA célula tem 2 colunas,
# e a grama e a água aparecem dobradas ("" e ~~) para o mapa não entortar.
# ---------------------------------------------------------------------------
COLUNAS_POR_CELULA = 2
GRAMA = ('""', "green")
AGUA = ("~~", "blue")
SIMBOLO_OCUPANTE = {
    int(gm.Ocupante.MACIEIRA): "🌳",
    int(gm.Ocupante.MACA): "🍎",
    int(gm.Ocupante.NPC): "🗿",
    # Corpo do NPC morto: cruz latina (U+271D), escolha do dono do projeto.
    # Ela tem 1 coluna só; o espaço completa as 2 colunas da célula.
    int(gm.Ocupante.CORPO): "✝ ",
}

NOME_ESTADO = {
    gm.EstadoNpc.VAGANDO: "vagando",
    gm.EstadoNpc.INDO_COMER: "indo comer",
    gm.EstadoNpc.COMENDO: "comendo",
    gm.EstadoNpc.MORTO: "morto",
    gm.EstadoNpc.INDO_BEBER: "indo beber",
    gm.EstadoNpc.BEBENDO: "bebendo",
}

GRAMA_BYTE = int(gm.Terreno.GRAMA)

# Teclas: o terminal manda as setas como sequências de 3 caracteres.
# Existem dois formatos, conforme o modo do terminal: ESC [ A ou ESC O A.
# Aceitamos os dois. Cada seta move a câmera 1 célula DA TELA (no zoom
# normal, isso é 1 célula do mapa; afastado em x4, são 4 células do mapa).
SETAS = {}
for _prefixo in (b"\x1b[", b"\x1bO"):
    SETAS[_prefixo + b"A"] = (0, -1)  # cima
    SETAS[_prefixo + b"B"] = (0, 1)   # baixo
    SETAS[_prefixo + b"C"] = (1, 0)   # direita
    SETAS[_prefixo + b"D"] = (-1, 0)  # esquerda

# ---------------------------------------------------------------------------
# Zoom (escolhido pelo dono do projeto): + aproxima, - afasta.
# O número é quantas células do mapa cabem em 1 célula da tela, em cada eixo:
#   0.5 -> aproximado: cada célula do mapa ocupa 2 x 2 células da tela
#   1   -> normal
#   2, 4, 6 -> afastado: cada célula da tela mostra um bloco 2x2, 4x4, 6x6
# ---------------------------------------------------------------------------
ESCALAS = [0.5, 1, 2, 4, 6]
ESCALA_INICIAL = ESCALAS.index(1)

# No zoom afastado, um bloco pode ter várias coisas. Aparece a mais
# importante, nesta ordem (escolha do dono do projeto):
# NPC > corpo > maçã > macieira > grama > água.
PRIORIDADE = {
    int(gm.Ocupante.NPC): 4,
    int(gm.Ocupante.CORPO): 3,
    int(gm.Ocupante.MACA): 2,
    int(gm.Ocupante.MACIEIRA): 1,
}

# Cada célula da tela vira um "código" antes de virar texto:
#   1..4 = o ocupante (mesmos números do motor); GRAMA_COD / AGUA_COD = chão.
GRAMA_COD = 100
AGUA_COD = 101


def texto_da_linha(codigos: list[int]) -> Text:
    """Transforma uma linha de códigos em texto colorido.

    Chão igual e seguido vira um pedaço só (menos trabalho para o rich).
    """
    texto = Text(no_wrap=True, overflow="crop")
    i, n = 0, len(codigos)
    while i < n:
        c = codigos[i]
        if c < GRAMA_COD:
            texto.append(SIMBOLO_OCUPANTE[c])
            i += 1
            continue
        j = i
        while j < n and codigos[j] == c:
            j += 1
        simbolo, cor = GRAMA if c == GRAMA_COD else AGUA
        texto.append(simbolo * (j - i), style=cor)
        i = j
    return texto


class Tela:
    """Câmera, zoom e montagem de cada quadro.

    A câmera guarda a célula DO MAPA que fica no canto de cima à esquerda
    da tela. O zoom só muda quantas células do mapa cabem na tela.
    """

    def __init__(self, mundo: "gm.Mundo") -> None:
        self.mundo = mundo
        self.largura = mundo.largura
        self.altura = mundo.altura
        self.terreno = mundo.terreno_bytes()  # o chão não muda
        self.nivel = ESCALA_INICIAL
        # Começa com o centro do mapa no centro da tela (ajustado no 1º quadro).
        self.cam_x = None
        self.cam_y = None
        self._ultima_vista = (1, 1)

    @property
    def escala(self) -> float:
        return ESCALAS[self.nivel]

    def vista(self, console: Console) -> tuple[int, int]:
        """Quantas células DO MAPA cabem na tela (1 linha fica para o status)."""
        colunas_tela = max(1, console.width // COLUNAS_POR_CELULA)
        linhas_tela = max(1, console.height - 1)
        mapa_l = max(1, min(int(colunas_tela * self.escala), self.largura))
        mapa_a = max(1, min(int(linhas_tela * self.escala), self.altura))
        return mapa_l, mapa_a

    def limitar(self, vis_l: int, vis_a: int) -> None:
        """Não deixa a câmera sair do mapa."""
        self.cam_x = max(0, min(self.cam_x, self.largura - vis_l))
        self.cam_y = max(0, min(self.cam_y, self.altura - vis_a))

    def mover(self, dx: int, dy: int) -> None:
        passo = max(1, int(self.escala))  # 1 célula da tela
        self.cam_x += dx * passo
        self.cam_y += dy * passo

    def zoom(self, direcao: int, console: Console) -> None:
        """direcao -1 = aproxima (+), +1 = afasta (-). Mantém o centro."""
        novo = max(0, min(len(ESCALAS) - 1, self.nivel + direcao))
        if novo == self.nivel:
            return
        vl, va = self._ultima_vista
        centro_x = self.cam_x + vl / 2
        centro_y = self.cam_y + va / 2
        self.nivel = novo
        vl, va = self.vista(console)
        self.cam_x = int(centro_x - vl / 2)
        self.cam_y = int(centro_y - va / 2)
        self.limitar(vl, va)

    def status(self) -> Text:
        """Linha de cima: tempo, maçãs no chão, NPCs, câmera e zoom."""
        segundos = int(self.mundo.tempo)
        partes = [f"⏱ {segundos // 60:02d}:{segundos % 60:02d}",
                  f"🍎 {len(self.mundo.macas())}"]
        for n in self.mundo.npcs():
            partes.append(f"🗿{n.id + 1} fome {n.fome:.0f} sede {n.sede:.0f}")
        zoom = "0,5x" if self.escala == 0.5 else f"{self.escala}x"
        partes.append(f"câmera ({self.cam_x},{self.cam_y}) zoom {zoom}"
                      "  setas: mover  +/-: zoom  I: tabela  q: sair")
        return Text("  ".join(partes), style="bold", no_wrap=True,
                    overflow="crop")

    def tabela(self) -> Group:
        """Tela separada (tecla I): uma linha por NPC."""
        t = Table(title="NPCs  (I: voltar ao mapa)", header_style="bold")
        t.add_column("NPC")
        t.add_column("Fome", justify="right")
        t.add_column("Sede", justify="right")
        t.add_column("Estado")
        t.add_column("Posição", justify="right")
        for n in self.mundo.npcs():
            t.add_row(f"🗿{n.id + 1}", f"{n.fome:.0f}", f"{n.sede:.0f}",
                      NOME_ESTADO[n.estado], f"({n.pos.x}, {n.pos.y})")
        return Group(self.status(), t)

    def _codigo_celula(self, ocupacao: bytes, x: int, y: int) -> int:
        i = y * self.largura + x
        if ocupacao[i]:
            return ocupacao[i]
        return GRAMA_COD if self.terreno[i] == GRAMA_BYTE else AGUA_COD

    def _codigo_bloco(self, ocupacao: bytes, x0: int, y0: int, lado: int) -> int:
        """Código do que aparece num bloco lado x lado (zoom afastado)."""
        melhor, melhor_p = AGUA_COD, -1
        for y in range(y0, min(y0 + lado, self.altura)):
            linha = y * self.largura
            for x in range(x0, min(x0 + lado, self.largura)):
                i = linha + x
                o = ocupacao[i]
                if o:
                    p = PRIORIDADE[o]
                    if p > melhor_p:
                        melhor, melhor_p = o, p
                elif melhor_p < 0 and self.terreno[i] == GRAMA_BYTE:
                    melhor = GRAMA_COD  # grama ganha da água
        return melhor

    def quadro(self, console: Console) -> Group:
        vis_l, vis_a = self.vista(console)
        if self.cam_x is None:  # primeiro quadro: centraliza
            self.cam_x = (self.largura - vis_l) // 2
            self.cam_y = (self.altura - vis_a) // 2
        self.limitar(vis_l, vis_a)
        self._ultima_vista = (vis_l, vis_a)

        ocupacao = self.mundo.ocupacao_bytes()
        cx, cy = self.cam_x, self.cam_y
        linhas = []
        if self.escala == 0.5:
            # Aproximado: cada célula do mapa vira 2 x 2 células da tela.
            # O emoji aparece no canto de cima à esquerda; o resto é o chão.
            for y in range(cy, cy + vis_a):
                cima, baixo = [], []
                for x in range(cx, cx + vis_l):
                    c = self._codigo_celula(ocupacao, x, y)
                    chao = (GRAMA_COD if self.terreno[y * self.largura + x]
                            == GRAMA_BYTE else AGUA_COD)
                    cima += [c, chao]
                    baixo += [chao, chao]
                linhas += [texto_da_linha(cima), texto_da_linha(baixo)]
        else:
            lado = int(self.escala)
            for y in range(cy, cy + vis_a, lado):
                codigos = [self._codigo_bloco(ocupacao, x, y, lado)
                           for x in range(cx, cx + vis_l, lado)]
                linhas.append(texto_da_linha(codigos))
        return Group(self.status(), *linhas)


def ler_teclas(fd: int, espera: float) -> bytes:
    """Espera até `espera` segundos por teclas; devolve o que chegou."""
    pronto, _, _ = select.select([fd], [], [], max(0.0, espera))
    return os.read(fd, 64) if pronto else b""


def main() -> None:
    # Semente sorteada a cada execução: cada vez que abre, é uma ilha nova.
    semente = int.from_bytes(os.urandom(8), "little")
    mundo = gm.Mundo(semente)
    tela = Tela(mundo)
    console = Console()

    # A simulação é redesenhada no ritmo do passo do NPC (1 s), que é o que
    # mais se mexe. O número vem do motor (config.hpp). Apertar uma seta
    # redesenha na hora, sem esperar.
    intervalo = gm.NPC_PASSO_S

    fd = sys.stdin.fileno()
    config_terminal = termios.tcgetattr(fd)
    try:
        # cbreak: as teclas chegam na hora, sem precisar de Enter.
        tty.setcbreak(fd)
        with Live(console=console, screen=True, auto_refresh=False,
                  vertical_overflow="crop") as live:
            ultimo = time.monotonic()
            proximo_quadro = ultimo
            na_tabela = False  # tecla I alterna entre mapa e tabela
            while True:
                agora = time.monotonic()
                mundo.avancar(agora - ultimo)  # tempo real
                ultimo = agora
                imagem = tela.tabela() if na_tabela else tela.quadro(console)
                live.update(imagem, refresh=True)
                if agora >= proximo_quadro:
                    proximo_quadro = agora + intervalo

                teclas = ler_teclas(fd, proximo_quadro - time.monotonic())
                if b"q" in teclas:
                    break
                for _ in range(teclas.count(b"i") + teclas.count(b"I")):
                    na_tabela = not na_tabela
                for seq, (dx, dy) in SETAS.items():
                    for _ in range(teclas.count(seq)):
                        tela.mover(dx, dy)
                # Zoom: "+" aproxima, "-" afasta. Aceita também "=" (mesma
                # tecla do "+" sem Shift) e o "+"/"-" do teclado numérico.
                for _ in range(teclas.count(b"+") + teclas.count(b"=")):
                    tela.zoom(-1, console)
                for _ in range(teclas.count(b"-")):
                    tela.zoom(+1, console)
    except KeyboardInterrupt:
        pass
    finally:
        # Devolve o terminal ao normal, aconteça o que acontecer.
        termios.tcsetattr(fd, termios.TCSADRAIN, config_terminal)


if __name__ == "__main__":
    main()
