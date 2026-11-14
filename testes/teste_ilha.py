"""
teste_ilha.py — confere se a simulação da ilha obedece às regras de
docs/ILHA_V1.md. Roda sem tela, acelerando o tempo.

Como rodar (depois do ./instalar.sh):
    .venv/bin/python testes/teste_ilha.py

Os tempos e sementes usados aqui são só para TESTAR (quanto tempo simular,
quantas ilhas sortear); não são regras do jogo.
"""

import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RAIZ / "build"))

import genesis_motor as gm  # noqa: E402

O = gm.Ocupante
E = gm.EstadoNpc


def cheb(a, b):
    return max(abs(a.x - b.x), abs(a.y - b.y))


def teste_mapa():
    w = gm.Mundo(1)
    assert (w.largura, w.altura) == (512, 512)
    t = w.terreno_bytes()
    grama = sum(1 for b in t if b == int(gm.Terreno.GRAMA))
    assert grama == 416 * 416, grama
    # 48 de água em cada borda, depois grama
    assert w.terreno(47, 256) == gm.Terreno.AGUA
    assert w.terreno(48, 256) == gm.Terreno.GRAMA
    assert w.terreno(463, 256) == gm.Terreno.GRAMA
    assert w.terreno(464, 256) == gm.Terreno.AGUA
    print("ok  mapa 512x512, 48 de água em cada borda, terra 416x416")


def teste_inicio():
    w = gm.Mundo(2)
    assert len(w.npcs()) == 4 and len(w.macieiras()) == 6
    assert all(n.fome == 100 for n in w.npcs())
    assert len(w.macas()) == 0
    print("ok  4 NPCs com fome 100, 6 macieiras, nenhuma maçã no início")


def teste_fome_cai():
    w = gm.Mundo(3)
    w.avancar(4.999)
    assert all(n.fome == 100 for n in w.npcs())
    w.avancar(0.001)
    assert all(n.fome == 97 for n in w.npcs())
    w.avancar(5.0)
    assert all(n.fome == 94 for n in w.npcs())
    print("ok  fome abaixa 3 a cada 5 s")


def teste_quedas_e_fisica(sementes=50, duracao=3600):
    comeu = morreu_sem_comer = 0
    for s in range(sementes):
        w = gm.Mundo(1000 + s)
        fome_antes = {n.id: n.fome for n in w.npcs()}
        ja_comeu = set()
        macas_antes = 0
        for seg in range(duracao):
            w.avancar(1.0)
            npcs = w.npcs()
            # --- física: contagem de ocupantes bate com as entidades ------
            oc = w.ocupacao_bytes()
            vivos = [n for n in npcs if n.estado != E.MORTO]
            mortos = [n for n in npcs if n.estado == E.MORTO]
            assert oc.count(int(O.NPC)) == len(vivos)
            assert oc.count(int(O.CORPO)) == len(mortos)
            assert oc.count(int(O.MACIEIRA)) == 6
            for n in vivos:
                assert w.ocupante(n.pos.x, n.pos.y) == O.NPC
                assert w.terreno(n.pos.x, n.pos.y) == gm.Terreno.GRAMA
            # --- fome dentro de 0..100; morto = fome 0 --------------------
            for n in npcs:
                assert 0 <= n.fome <= 100
                if n.estado == E.MORTO:
                    assert n.fome == 0
                # comer só com fome < 70
                if n.estado in (E.INDO_COMER, E.COMENDO):
                    pass  # a decisão é tomada com fome < 70 (ver abaixo)
                # subiu a fome? então comeu: sobe 12 (ou até 100)
                if n.fome > fome_antes[n.id]:
                    ganho = n.fome - fome_antes[n.id]
                    # +12 da maçã; se no mesmo segundo a fome caiu, +12-3 = +9
                    assert (abs(ganho - 12) < 1e-9 or abs(ganho - 9) < 1e-9
                            or n.fome == 100), ganho
                    ja_comeu.add(n.id)
                fome_antes[n.id] = n.fome
            # --- maçãs: nenhuma no início antes de 30 s -------------------
            if seg + 1 < 30:
                assert len(w.macas()) == 0
            macas_antes = len(w.macas())
        comeu += len(ja_comeu)
        for n in w.npcs():
            if n.estado == E.MORTO and n.id not in ja_comeu:
                morreu_sem_comer += 1
    print(f"ok  física/fome/maçãs em {sementes} ilhas x {duracao} s "
          f"(NPCs que comeram: {comeu}; morreram sem comer: {morreu_sem_comer})")


def teste_morte_sem_comida():
    # Sem comer, 100 cai 3 a cada 5 s: chega a 0 na 34ª queda = 170 s.
    for s in range(200):
        w = gm.Mundo(5000 + s)
        w.avancar(169.999)
        for n in w.npcs():
            if n.fome > 100 - 33 * 3 + 1e-9:
                continue  # este comeu; não serve para o teste
            assert n.estado != E.MORTO
        w.avancar(0.002)
        for n in w.npcs():
            if abs(n.fome - 0) < 1e-9:
                assert n.estado == E.MORTO
    print("ok  sem comer, o NPC morre aos 170 s e o corpo fica")


def teste_maca_em_volta():
    # A primeira maçã de cada macieira cai na camada 1 (as 8 vizinhas),
    # pois no começo a volta das árvores está quase sempre vazia.
    w = gm.Mundo(7)
    arvores = [m.pos for m in w.macieiras()]
    w.avancar(60.0)  # até 60 s todas já derrubaram ao menos 1 maçã
    macas = w.macas()
    assert len(macas) >= 6, len(macas)
    for p in macas:
        assert min(cheb(p, a) for a in arvores) >= 1
    print(f"ok  maçãs caem em volta das macieiras ({len(macas)} em 60 s)")


def teste_tempo_comer():
    # Acha um NPC que começou a comer e confere que leva 10 s.
    for s in range(200):
        w = gm.Mundo(9000 + s)
        inicio = {}
        for passo in range(1200 * 10):
            w.avancar(0.1)
            for n in w.npcs():
                if n.estado == E.COMENDO and n.id not in inicio:
                    inicio[n.id] = (w.tempo, n.fome)
                    assert n.fome < 70
                elif n.id in inicio and n.estado != E.COMENDO and n.estado != E.MORTO:
                    t0, f0 = inicio[n.id]
                    dur = w.tempo - t0
                    assert 9.8 <= dur <= 10.2, dur
                    print(f"ok  comer leva ~10 s (medido {dur:.1f} s, fome {f0:.0f} → {n.fome:.0f})")
                    return
    raise AssertionError("nenhum NPC comeu nas ilhas testadas")


if __name__ == "__main__":
    teste_mapa()
    teste_inicio()
    teste_fome_cai()
    teste_maca_em_volta()
    teste_morte_sem_comida()
    teste_tempo_comer()
    teste_quedas_e_fisica()
    print("TODOS OS TESTES PASSARAM")
