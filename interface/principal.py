"""
principal.py — interface do Genesis (Python + rich).

O que este arquivo faz:
    1. Cria o mundo no motor em C++ (módulo `genesis_motor`, via pybind11).
    2. Em tempo real, pede ao motor para andar o tempo que passou no relógio
       de verdade, e desenha no terminal o pedaço da ilha que cabe na tela.
    3. As SETAS do teclado movem a câmera (a vista) pelo mapa.

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
    int(gm.Ocupante.CORPO): "💀",  # PROVISÓRIO: aguardando o dono escolher
}

NOME_ESTADO = {
    gm.EstadoNpc.VAGANDO: "vagando",
    gm.EstadoNpc.INDO_COMER: "indo comer",
    gm.EstadoNpc.COMENDO: "comendo",
    gm.EstadoNpc.MORTO: "morto",
}

GRAMA_BYTE = int(gm.Terreno.GRAMA)

# Teclas: o terminal manda as setas como sequências de 3 caracteres.
# Cada seta move a câmera 1 célula (a menor unidade do mapa).
SETAS = {
    b"\x1b[A": (0, -1),  # cima
    b"\x1b[B": (0, 1),   # baixo
    b"\x1b[C": (1, 0),   # direita
    b"\x1b[D": (-1, 0),  # esquerda
}


def desenhar_linha(terreno: bytes, ocupacao: bytes) -> Text:
    """Desenha um pedaço de linha do mapa.

    Células de chão iguais e seguidas viram um pedaço só (menos trabalho
    para o rich). Onde há algo em cima do chão, entra o emoji.
    """
    texto = Text(no_wrap=True, overflow="crop")
    i, n = 0, len(terreno)
    while i < n:
        if ocupacao[i]:
            texto.append(SIMBOLO_OCUPANTE[ocupacao[i]])
            i += 1
            continue
        j = i
        while j < n and not ocupacao[j] and terreno[j] == terreno[i]:
            j += 1
        simbolo, cor = GRAMA if terreno[i] == GRAMA_BYTE else AGUA
        texto.append(simbolo * (j - i), style=cor)
        i = j
    return texto


class Tela:
    """Câmera + montagem de cada quadro."""

    def __init__(self, mundo: "gm.Mundo") -> None:
        self.mundo = mundo
        self.largura = mundo.largura
        self.altura = mundo.altura
        self.terreno = mundo.terreno_bytes()  # o chão não muda
        # Câmera = célula do mapa no canto de cima à esquerda da tela.
        # Começa com o centro do mapa no centro da tela (ajustado no 1º quadro).
        self.cam_x = None
        self.cam_y = None

    def tamanho_vista(self, console: Console) -> tuple[int, int]:
        """Quantas células cabem na tela (1 linha fica para o status)."""
        colunas = max(1, console.width // COLUNAS_POR_CELULA)
        linhas = max(1, console.height - 1)
        return min(colunas, self.largura), min(linhas, self.altura)

    def limitar(self, vis_l: int, vis_a: int) -> None:
        """Não deixa a câmera sair do mapa."""
        self.cam_x = max(0, min(self.cam_x, self.largura - vis_l))
        self.cam_y = max(0, min(self.cam_y, self.altura - vis_a))

    def mover(self, dx: int, dy: int) -> None:
        self.cam_x += dx
        self.cam_y += dy

    def status(self) -> Text:
        """Linha de cima: tempo, maçãs no chão e situação de cada NPC."""
        segundos = int(self.mundo.tempo)
        partes = [f"⏱ {segundos // 60:02d}:{segundos % 60:02d}",
                  f"🍎 {len(self.mundo.macas())}"]
        for n in self.mundo.npcs():
            partes.append(f"🗿{n.id + 1} {n.fome:.0f} {NOME_ESTADO[n.estado]}")
        partes.append(f"câmera ({self.cam_x},{self.cam_y}) setas: mover  q: sair")
        return Text("  ".join(partes), style="bold", no_wrap=True,
                    overflow="crop")

    def quadro(self, console: Console) -> Group:
        vis_l, vis_a = self.tamanho_vista(console)
        if self.cam_x is None:  # primeiro quadro: centraliza
            self.cam_x = (self.largura - vis_l) // 2
            self.cam_y = (self.altura - vis_a) // 2
        self.limitar(vis_l, vis_a)

        ocupacao = self.mundo.ocupacao_bytes()
        linhas = []
        for y in range(self.cam_y, self.cam_y + vis_a):
            ini = y * self.largura + self.cam_x
            fim = ini + vis_l
            linhas.append(desenhar_linha(self.terreno[ini:fim], ocupacao[ini:fim]))
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
            while True:
                agora = time.monotonic()
                mundo.avancar(agora - ultimo)  # tempo real
                ultimo = agora
                live.update(tela.quadro(console), refresh=True)
                if agora >= proximo_quadro:
                    proximo_quadro = agora + intervalo

                teclas = ler_teclas(fd, proximo_quadro - time.monotonic())
                if b"q" in teclas:
                    break
                for seq, (dx, dy) in SETAS.items():
                    for _ in range(teclas.count(seq)):
                        tela.mover(dx, dy)
    except KeyboardInterrupt:
        pass
    finally:
        # Devolve o terminal ao normal, aconteça o que acontecer.
        termios.tcsetattr(fd, termios.TCSADRAIN, config_terminal)


if __name__ == "__main__":
    main()
