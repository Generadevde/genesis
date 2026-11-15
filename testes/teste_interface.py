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
        g.abas.ativa = (passo // 40) % 5  # Mapa, NPCs, Fauna, Histórico, Info
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

    # --- Botões de velocidade: || pausa, > normal, >> 1 min/s, >>> 1 h/s,
    #     >>>> 1 dia/s; o clicado fica ligado; Espaço pausa e continua.
    assert g.velocidade == 1
    esperado = [0, 1, 60, 3600, 86400]
    assert [m for _, m in g._botoes_velocidade] == esperado
    for (botao, mult) in list(g._botoes_velocidade):
        evento(pygame.MOUSEBUTTONDOWN, button=1, pos=botao.center)
        assert g.velocidade == mult, (g.velocidade, mult)
        g.desenhar()
    # 1 dia por segundo: 1/60 s real anda 1440 s do jogo (se o motor der conta)
    t0 = g.mundo.tempo
    g.andar_tempo(1 / 60)
    andou = g.mundo.tempo - t0
    assert 0 < andou <= 86400 / 60 + 1e-6, andou
    evento(pygame.KEYDOWN, key=pygame.K_SPACE, unicode=" ")
    assert g.velocidade == 0
    t0 = g.mundo.tempo
    g.andar_tempo(0.5)
    assert g.mundo.tempo == t0  # pausado: não anda
    evento(pygame.KEYDOWN, key=pygame.K_SPACE, unicode=" ")
    assert g.velocidade == 86400
    g.velocidade = 1
    t0 = g.mundo.tempo
    g.andar_tempo(0.5)
    assert abs(g.mundo.tempo - t0 - 0.5) < 1e-9  # normal: 1 s do jogo por s
    print(f"ok  velocidade: || > >> >>> >>>> = {esperado} s/s; Espaço pausa "
          f"(1 dia/s andou {andou:.0f} s num quadro)")

    # --- Barra de baixo mostra data e estação; pilha de maçãs "xN"
    texto = g.texto_relogio()
    assert texto.startswith("01/01/0001") and "Verão" in texto, texto
    g.mundo.avancar(2 * 86400)  # o verão já está derrubando maçãs
    g.abas.ativa = 0
    g.desenhar()
    print(f"ok  barra de baixo: '{texto}'; {len(g.mundo.pilhas())} pilha(s) desenhadas")

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
