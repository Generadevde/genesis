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
        g.abas.ativa = (passo // 40) % 4  # Mapa, NPCs, Histórico, Info
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

    # --- Barra de baixo: nome do que está sob o mouse
    from unittest import mock
    g.nivel_zoom = P.ESCALAS.index(1)
    n = g.mundo.npcs()[0]
    g.cam_x, g.cam_y = max(0, n.pos.x - 5), max(0, n.pos.y - 5)
    g.desenhar()
    area, cel = g._mapa_area, g._mapa_cel
    ponto = (area.left + 2 + (n.pos.x - g.cam_x) * cel + cel // 2,
             area.top + 2 + (n.pos.y - g.cam_y) * cel + cel // 2)
    with mock.patch("pygame.mouse.get_pos", return_value=ponto):
        nome = g.nome_sob_mouse()
        g.desenhar()
    assert nome in (n.nome, f"Corpo de {n.nome}"), nome
    print(f"ok  barra de baixo: tempo + nome sob o mouse ('{nome}')")

    # --- Botões de velocidade: > 2x, >> 4x, >>>> 8x; clicar de novo volta a 1x
    assert g.velocidade == 1
    for (botao, mult) in list(g._botoes_velocidade):
        evento(pygame.MOUSEBUTTONDOWN, button=1, pos=botao.center)
        assert g.velocidade == mult, (g.velocidade, mult)
        g.desenhar()
    botao, mult = g._botoes_velocidade[-1]
    evento(pygame.MOUSEBUTTONDOWN, button=1, pos=botao.center)
    assert g.velocidade == 1
    # o tempo do motor anda vezes a velocidade
    g.velocidade = 8
    t0 = g.mundo.tempo
    g.mundo.avancar(0.5 * g.velocidade)
    assert abs(g.mundo.tempo - t0 - 4.0) < 1e-9
    g.velocidade = 1
    print("ok  velocidade: > 2x, >> 4x, >>>> 8x; clicar de novo volta a 1x")

    # --- Janela "Nova simulação": digita 4 homens e 2 mulheres, OK.
    g.abrir_dialogo_nova()
    g.desenhar()
    campo_h, campo_m = g._dlg_campos["homens"], g._dlg_campos["mulheres"]
    evento(pygame.MOUSEBUTTONDOWN, button=1, pos=campo_h.center)
    evento(pygame.KEYDOWN, key=pygame.K_BACKSPACE, unicode="")
    evento(pygame.KEYDOWN, key=pygame.K_4, unicode="4")
    evento(pygame.KEYDOWN, key=pygame.K_TAB, unicode="\t")
    evento(pygame.KEYDOWN, key=pygame.K_BACKSPACE, unicode="")
    evento(pygame.KEYDOWN, key=pygame.K_2, unicode="2")
    evento(pygame.KEYDOWN, key=pygame.K_a, unicode="a")   # letra: ignorada
    g.desenhar()
    evento(pygame.MOUSEBUTTONDOWN, button=1, pos=g._dlg_ok.center)
    assert g.dialogo is None
    sexos = [n.sexo for n in g.mundo.npcs()]
    assert sexos.count(P.gm.Sexo.HOMEM) == 4 and sexos.count(P.gm.Sexo.MULHER) == 2
    # Cancelar não muda nada
    antes = g.mundo
    g.abrir_dialogo_nova()
    g.desenhar()
    evento(pygame.KEYDOWN, key=pygame.K_ESCAPE, unicode="")
    assert g.dialogo is None and g.mundo is antes
    # Aba Histórico: desenha, filtra e rola
    g.abas.ativa = g.abas.nomes.index("Histórico")
    g.desenhar()
    assert g.historico and g.historico[0][2].startswith("A simulação começou")
    b, cat = g._botoes_filtro[0]
    evento(pygame.MOUSEBUTTONDOWN, button=1, pos=b.center)
    assert g.filtros[cat] is False
    evento(pygame.MOUSEWHEEL, x=0, y=-1)
    g.desenhar()
    print("ok  janela Nova simulação (4 homens, 2 mulheres; Esc cancela); "
          "aba Histórico (filtro e rolagem)")

    g.nova_simulacao()
    g.desenhar()
    print(f"ok  interface: {quadros} quadros desenhados sem cair "
          "(abas, níveis z, zooms, menu, nova simulação)")


if __name__ == "__main__":
    main()
