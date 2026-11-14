"""
principal.py — interface do Genesis (Python + rich).

O que este arquivo faz:
    1. Cria o mundo no motor em C++ (módulo `genesis_motor`, via pybind11).
    2. Em tempo real, pede ao motor para andar o tempo que passou no relógio
       de verdade, e desenha a ilha inteira no terminal.

Regra da STACK.md: a interface só MOSTRA. Nenhuma regra da simulação mora
aqui (fome, maçãs, passos... tudo isso está no motor, em C++).

Sem menus (pedido do dono do projeto): abre direto na simulação.
Para sair: Ctrl+C.
"""

import os
import sys
import time
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

# Valor do byte de terreno para grama (número do enum vindo do motor).
GRAMA_BYTE = int(gm.Terreno.GRAMA)


def linha_de_terreno(terreno_linha: bytes) -> Text:
    """Desenha uma linha só de chão (sem nada em cima).

    Junta células iguais seguidas num pedaço só, para o rich ter menos
    trabalho (uma linha de grama inteira vira um único pedaço verde).
    """
    texto = Text(no_wrap=True, overflow="crop")
    inicio = 0
    for i in range(1, len(terreno_linha) + 1):
        if i == len(terreno_linha) or terreno_linha[i] != terreno_linha[inicio]:
            simbolo, cor = GRAMA if terreno_linha[inicio] == GRAMA_BYTE else AGUA
            texto.append(simbolo * (i - inicio), style=cor)
            inicio = i
    return texto


def linha_com_coisas(terreno_linha: bytes, ocupacao_linha: bytes) -> Text:
    """Desenha uma linha que tem macieira, maçã, NPC ou corpo em cima."""
    texto = Text(no_wrap=True, overflow="crop")
    for t, o in zip(terreno_linha, ocupacao_linha):
        if o:
            texto.append(SIMBOLO_OCUPANTE[o])
        else:
            simbolo, cor = GRAMA if t == GRAMA_BYTE else AGUA
            texto.append(simbolo, style=cor)
    return texto


class Tela:
    """Guarda o que não muda (o chão) e monta a imagem de cada quadro."""

    def __init__(self, mundo: "gm.Mundo") -> None:
        self.mundo = mundo
        self.largura = mundo.largura
        terreno = mundo.terreno_bytes()
        self.terreno_linhas = [
            terreno[y * self.largura:(y + 1) * self.largura]
            for y in range(mundo.altura)
        ]
        # O chão nunca muda: cada linha "vazia" é desenhada uma vez só.
        self.cache_chao = [linha_de_terreno(l) for l in self.terreno_linhas]

    def status(self) -> Text:
        """Linha de cima: tempo, maçãs no chão e situação de cada NPC."""
        segundos = int(self.mundo.tempo)
        partes = [f"Genesis  ⏱ {segundos // 60:02d}:{segundos % 60:02d}",
                  f"🍎 no chão: {len(self.mundo.macas())}"]
        for n in self.mundo.npcs():
            partes.append(f"🗿{n.id + 1} fome {n.fome:.0f} ({NOME_ESTADO[n.estado]})")
        return Text("   ".join(partes), style="bold")

    def quadro(self) -> Group:
        ocupacao = self.mundo.ocupacao_bytes()
        linhas = []
        for y, chao in enumerate(self.terreno_linhas):
            ocup = ocupacao[y * self.largura:(y + 1) * self.largura]
            # any(ocup) = existe algum byte diferente de zero (alguma coisa
            # em cima do chão). Se não, usa a linha de chão já pronta.
            linhas.append(linha_com_coisas(chao, ocup) if any(ocup)
                          else self.cache_chao[y])
        return Group(self.status(), *linhas)


def main() -> None:
    # Semente sorteada a cada execução: cada vez que abre, é uma ilha nova.
    semente = int.from_bytes(os.urandom(8), "little")
    mundo = gm.Mundo(semente)
    tela = Tela(mundo)

    # A tela atualiza no mesmo ritmo do passo do NPC (1 s), que é o que
    # mais se mexe na simulação. O número vem do motor (config.hpp).
    intervalo = gm.NPC_PASSO_S

    console = Console()
    try:
        with Live(console=console, screen=True, auto_refresh=False,
                  vertical_overflow="crop") as live:
            ultimo = time.monotonic()
            while True:
                agora = time.monotonic()
                mundo.avancar(agora - ultimo)  # tempo real
                ultimo = agora
                live.update(tela.quadro(), refresh=True)
                gasto = time.monotonic() - agora
                time.sleep(max(0.0, intervalo - gasto))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
