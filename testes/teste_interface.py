"""
teste_interface.py — abre a interface sem tela de verdade (driver "dummy"
do SDL) e desenha vários quadros, passando por todas as abas, níveis z,
zooms e o menu.

Existe por causa de um erro real (14/11/2026): o objeto da janela usado
para maximizar era descartado pelo Python, o pygame destruía a janela junto
e o programa caía com "falha de segmentação". Aqui forçamos a limpeza de
memória do Python (gc.collect) para pegar esse tipo de erro.

Como rodar:
    .venv/bin/python testes/teste_interface.py
"""

import faulthandler
import gc
import os
import sys
from pathlib import Path

faulthandler.enable()
os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ["SDL_AUDIODRIVER"] = "dummy"

RAIZ = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RAIZ / "interface"))

import pygame  # noqa: E402

import principal as P  # noqa: E402


def main() -> None:
    g = P.Genesis()
    quadros = 0
    for passo in range(240):
        gc.collect()  # força o descarte de objetos soltos
        for ev in pygame.event.get():
            g.tratar_evento(ev)
        g.mundo.avancar(1 / 60)
        # passa por abas, níveis, zoom e menu
        g.abas.ativa = (passo // 40) % 2
        g.z_vista = [4, 2, 0, -5, -11, 4][passo // 40]
        g.nivel_zoom = passo % len(P.ESCALAS)
        g.menu.aberto = 0 if passo % 30 == 0 else None
        g.desenhar()
        quadros += 1
    # --- Mouse: arrastar move a câmera; clicar sem arrastar seleciona;
    #     rodinha muda o zoom.
    g.abas.ativa = 0
    g.z_vista = 4
    g.nivel_zoom = P.ESCALAS.index(1)
    g.cam_x = g.cam_y = 20
    g.desenhar()
    area, cel = g._mapa_area, g._mapa_cel
    meio = area.center

    def evento(tipo, **dados):
        g.tratar_evento(pygame.event.Event(tipo, **dados))

    evento(pygame.MOUSEBUTTONDOWN, button=1, pos=meio)
    evento(pygame.MOUSEMOTION, pos=(meio[0] - 3 * cel, meio[1] - 2 * cel),
           rel=(0, 0), buttons=(1, 0, 0))
    evento(pygame.MOUSEBUTTONUP, button=1, pos=(meio[0] - 3 * cel, meio[1] - 2 * cel))
    assert (g.cam_x, g.cam_y) == (23, 22), (g.cam_x, g.cam_y)
    assert g.selecao is None  # arrastar não seleciona

    evento(pygame.MOUSEBUTTONDOWN, button=1, pos=meio)
    evento(pygame.MOUSEBUTTONUP, button=1, pos=meio)
    assert g.selecao is not None  # clique sem arrastar seleciona

    evento(pygame.MOUSEWHEEL, x=0, y=1)
    assert P.ESCALAS[g.nivel_zoom] == 0.5  # rodinha para cima: aproxima
    evento(pygame.MOUSEWHEEL, x=0, y=-1)
    evento(pygame.MOUSEWHEEL, x=0, y=-1)
    assert P.ESCALAS[g.nivel_zoom] == 2    # para baixo: afasta
    g.desenhar()
    print("ok  mouse: arrastar move a câmera (sem selecionar), clique "
          "seleciona, rodinha aproxima/afasta")

    g.nova_simulacao()
    g.desenhar()
    print(f"ok  interface: {quadros} quadros desenhados sem cair "
          "(abas, níveis z, zooms, menu, nova simulação)")


if __name__ == "__main__":
    main()
