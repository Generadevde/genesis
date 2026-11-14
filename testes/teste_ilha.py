"""
teste_ilha.py — confere se a simulação da ilha obedece às regras de
docs/ILHA_V1.md. Roda sem tela, acelerando o tempo.

Como rodar:
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
    assert (w.largura, w.altura) == (128, 128)
    t = w.terreno_bytes()
    grama = sum(1 for b in t if b == int(gm.Terreno.GRAMA))
    assert grama == 112 * 112, grama
    assert w.terreno(7, 64) == gm.Terreno.AGUA
    assert w.terreno(8, 64) == gm.Terreno.GRAMA
    assert w.terreno(119, 64) == gm.Terreno.GRAMA
    assert w.terreno(120, 64) == gm.Terreno.AGUA
    print("ok  mapa 128x128, 8 de água em cada borda, terra 112x112")


def teste_inicio():
    w = gm.Mundo(2)
    assert len(w.npcs()) == 4 and len(w.macieiras()) == 6
    assert all(n.fome == 100 and n.sede == 100 for n in w.npcs())
    assert len(w.macas()) == 0
    print("ok  4 NPCs com fome 100 e sede 100, 6 macieiras, nenhuma maçã")


def teste_quedas():
    w = gm.Mundo(3)
    w.avancar(4.999)
    assert all(n.fome == 100 and n.sede == 100 for n in w.npcs())
    w.avancar(0.001)
    assert all(n.fome == 97 and n.sede == 96 for n in w.npcs())
    w.avancar(5.0)
    assert all(n.fome == 94 and n.sede == 92 for n in w.npcs())
    print("ok  a cada 5 s: fome -3, sede -4")


def teste_morte_sem_nada():
    # Sem beber, 100 cai 4 a cada 5 s: chega a 0 na 25ª queda = 125 s
    # (antes da fome, que levaria 170 s).
    testados = 0
    for s in range(200):
        w = gm.Mundo(5000 + s)
        w.avancar(124.999)
        candidatos = [n.id for n in w.npcs()
                      if abs(n.sede - 4) < 1e-9
                      and n.estado not in (E.MORTO, E.BEBENDO)]
        w.avancar(0.002)
        for n in w.npcs():
            if n.id in candidatos:
                assert n.estado == E.MORTO and n.sede == 0
                assert w.ocupante(n.pos.x, n.pos.y) == O.CORPO
                testados += 1
    assert testados > 0
    print(f"ok  sem beber, morre aos 125 s e o corpo fica ({testados} NPCs)")


def teste_maca_em_volta():
    w = gm.Mundo(7)
    arvores = [m.pos for m in w.macieiras()]
    w.avancar(60.0)  # até 60 s todas já derrubaram ao menos 1 maçã
    macas = w.macas()
    assert len(macas) >= 6, len(macas)
    for p in macas:
        assert min(cheb(p, a) for a in arvores) >= 1
    print(f"ok  maçãs caem em volta das macieiras ({len(macas)} em 60 s)")


def teste_acoes(sementes=300, dt=0.1, duracao=1200):
    """Comer e beber: duração de 10 s, ganho certo, e durante a ação a
    necessidade correspondente não abaixa."""
    visto = {"comer": None, "beber": None}
    for s in range(sementes):
        w = gm.Mundo(9000 + s)
        inicio = {}
        for _ in range(int(duracao / dt)):
            antes = {n.id: (n.estado, n.fome, n.sede) for n in w.npcs()}
            w.avancar(dt)
            for n in w.npcs():
                est0, f0, s0 = antes[n.id]
                if n.estado == E.COMENDO:
                    assert n.fome == f0 or est0 != E.COMENDO  # não abaixa
                if n.estado == E.BEBENDO:
                    assert n.sede == s0 or est0 != E.BEBENDO  # não abaixa
                if n.estado in (E.COMENDO, E.BEBENDO) and est0 != n.estado:
                    inicio[n.id] = (n.estado, w.tempo, n.fome, n.sede)
                elif n.id in inicio and n.estado not in (E.COMENDO, E.BEBENDO,
                                                         E.MORTO):
                    acao, t0, f, sd = inicio.pop(n.id)
                    dur = w.tempo - t0
                    assert 9.8 <= dur <= 10.2, dur
                    if acao == E.COMENDO:
                        assert n.fome == min(100, f + 12), (f, n.fome)
                        visto["comer"] = (dur, f, n.fome)
                    else:
                        assert n.sede == min(100, sd + 15), (sd, n.sede)
                        visto["beber"] = (dur, sd, n.sede)
            if all(visto.values()):
                (dc, fa, fd), (db, sa, sdp) = visto["comer"], visto["beber"]
                print(f"ok  comer {dc:.1f} s (fome {fa:.0f}→{fd:.0f}), "
                      f"beber {db:.1f} s (sede {sa:.0f}→{sdp:.0f}); "
                      "durante a ação a necessidade não abaixa")
                return
    raise AssertionError(f"não vi as duas ações: {visto}")


def teste_geral(sementes=50, duracao=3600):
    """Física, limites, e quando o NPC decide comer/beber."""
    comeu = bebeu = vivos = 0
    for s in range(sementes):
        w = gm.Mundo(1000 + s)
        antes = {n.id: (n.fome, n.sede) for n in w.npcs()}
        ids_comeram, ids_beberam = set(), set()
        for seg in range(duracao):
            w.avancar(1.0)
            npcs = w.npcs()
            oc = w.ocupacao_bytes()
            vivos_ = [n for n in npcs if n.estado != E.MORTO]
            mortos = [n for n in npcs if n.estado == E.MORTO]
            assert oc.count(int(O.NPC)) == len(vivos_)
            assert oc.count(int(O.CORPO)) == len(mortos)
            assert oc.count(int(O.MACIEIRA)) == 6
            for n in vivos_:
                assert w.ocupante(n.pos.x, n.pos.y) == O.NPC
                assert w.terreno(n.pos.x, n.pos.y) == gm.Terreno.GRAMA
                # Só vai comer com fome < 70, e só vai beber com sede < 50.
                if n.estado == E.INDO_COMER:
                    assert n.fome < 70
                if n.estado == E.INDO_BEBER:
                    assert n.sede < 50
            for n in npcs:
                assert 0 <= n.fome <= 100 and 0 <= n.sede <= 100
                if n.estado == E.MORTO:
                    assert n.fome == 0 or n.sede == 0
                f0, s0 = antes[n.id]
                if n.fome > f0:  # +12 (ou +9 se a fome caiu no mesmo segundo)
                    assert n.fome - f0 in (12, 9) or n.fome == 100, n.fome - f0
                    ids_comeram.add(n.id)
                if n.sede > s0:  # +15 (ou +11 se a sede caiu no mesmo segundo)
                    assert n.sede - s0 in (15, 11) or n.sede == 100, n.sede - s0
                    ids_beberam.add(n.id)
                antes[n.id] = (n.fome, n.sede)
            if seg + 1 < 30:
                assert len(w.macas()) == 0
        comeu += len(ids_comeram)
        bebeu += len(ids_beberam)
        vivos += sum(n.estado != E.MORTO for n in w.npcs())
    total = sementes * 4
    print(f"ok  física/limites em {sementes} ilhas x {duracao} s — de {total} "
          f"NPCs: {comeu} comeram, {bebeu} beberam, {vivos} vivos no fim")


if __name__ == "__main__":
    teste_mapa()
    teste_inicio()
    teste_quedas()
    teste_maca_em_volta()
    teste_morte_sem_nada()
    teste_acoes()
    teste_geral()
    print("TODOS OS TESTES PASSARAM")
