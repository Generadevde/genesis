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
    g.nova_simulacao()
    g.desenhar()
    print(f"ok  interface: {quadros} quadros desenhados sem cair "
          "(abas, níveis z, zooms, menu, nova simulação)")


if __name__ == "__main__":
    main()
