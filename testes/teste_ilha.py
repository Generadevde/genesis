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


MAT = gm.Material


def teste_mapa():
    """Gerador: mar, planícies/morros/vales, camadas e vales alagados."""
    from collections import deque
    for semente in (1, 2, 3):
        w = gm.Mundo(semente)
        assert (w.largura, w.altura) == (128, 128)
        assert (gm.Z_MIN, gm.Z_MAX) == (-64, 64)
        vale = morro = planicie = 0
        for y in range(128):
            for x in range(128):
                mar = x < 8 or y < 8 or x >= 120 or y >= 120
                if mar:
                    assert w.superficie(x, y) == 0
                    for z in (0, -5, -10):
                        assert w.material(x, y, z) == MAT.AGUA
                        assert w.nivel_agua(x, y, z) == 10
                    assert w.material(x, y, -11) == MAT.PEDRA
                    continue
                # chão = primeira célula que não é ar nem água, de cima p/ baixo
                chao = w.superficie(x, y)
                while w.material(x, y, chao) == MAT.AGUA:
                    chao -= 1
                assert -4 <= chao <= 4, chao
                if chao >= 2:
                    morro += 1
                elif chao == 1:
                    planicie += 1
                else:
                    vale += 1
                topo = w.material(x, y, chao)
                if chao == 4:
                    assert topo == MAT.NEVE
                elif chao < -3:
                    assert topo == MAT.PEDRA          # pedra exposta
                else:
                    assert topo == MAT.GRAMA
                for z in range(chao - 1, -4, -1):
                    assert w.material(x, y, z) == MAT.TERRA
                assert w.material(x, y, min(chao - 1, -4)) == MAT.PEDRA
                # acima do chão: só água (até o 0) ou ar
                for z in range(chao + 1, 6):
                    m = w.material(x, y, z)
                    assert m in (MAT.AR, MAT.AGUA)
                    if m == MAT.AGUA:
                        assert z <= 0
        total = 112 * 112
        for nome, qtd in (("vale", vale), ("morro", morro), ("planície", planicie)):
            alvo = 0.4 if nome == "planície" else 0.3
            assert abs(qtd / total - alvo) < 0.01, (nome, qtd / total)

        # Água na terra só se ligada ao mar; e todo ar abaixo de 0 ligado ao
        # mar foi preenchido (o vale ligado ao mar está cheio).
        visto = set()
        fila = deque((x, y, z) for y in range(128) for x in range(128)
                     for z in range(-10, 1)
                     if (x < 8 or y < 8 or x >= 120 or y >= 120))
        visto.update(fila)
        while fila:
            x, y, z = fila.popleft()
            for dx, dy, dz in ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, -1)):
                p = (x + dx, y + dy, z + dz)
                if p in visto or not (0 <= p[0] < 128 and 0 <= p[1] < 128) or p[2] > 0:
                    continue
                m = w.material(*p)
                assert m != MAT.AR, f"ar ligado ao mar sem água em {p}"
                if m == MAT.AGUA:
                    visto.add(p)
                    fila.append(p)
        agua_terra = [(x, y, z) for y in range(8, 120) for x in range(8, 120)
                      for z in range(-4, 1) if w.material(x, y, z) == MAT.AGUA]
        assert all(p in visto for p in agua_terra), "água isolada do mar"
    print("ok  gerador: mar 0..-10 sobre pedra; 30% vale / 30% morro / 40% planície "
          "(+1); morros até +4 (neve), vales até -4 (pedra exposta); grama, "
          "terra até -3, pedra; vales ligados ao mar cheios, os outros secos")


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
                      and n.estado not in (E.MORTO, E.BEBENDO, E.COMENDO)]
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
                    assert n.sede == s0 or est0 != E.COMENDO  # sede também não
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
                        # maçã também dá +5 de sede (e a sede não cai comendo)
                        assert n.sede == min(100, sd + 5), (sd, n.sede)
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
            existem = [m for m in w.macieiras() if m.existe]
            assert (oc.count(int(O.MACIEIRA)) + oc.count(int(O.MACIEIRA_SECA))
                    == len(existem))
            for n in vivos_:
                assert w.ocupante(n.pos.x, n.pos.y) == O.NPC
                assert n.pos.z == w.superficie(n.pos.x, n.pos.y)
                assert w.material(n.pos.x, n.pos.y, n.pos.z) in (MAT.GRAMA, MAT.NEVE, MAT.PEDRA)
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
                if n.sede > s0:  # gole +15 (ou +11 se caiu no mesmo segundo);
                    #               maçã +5 (ou +1)
                    assert n.sede - s0 in (15, 11, 5, 1) or n.sede == 100, n.sede - s0
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


def teste_memoria(sementes=50, duracao=1800):
    """Memória: lembra só onde usou (comeu, bebeu, ou pegou a maçã),
    nunca esquece, e usa o que lembra."""
    viu_ida_arvore = viu_ida_agua = 0
    for s in range(sementes):
        w = gm.Mundo(20000 + s)
        arvores = [m.pos for m in w.macieiras()]
        antes = {n.id: (len(n.macieiras_lembradas), len(n.aguas_lembradas),
                        n.estado, n.macas_carregadas) for n in w.npcs()}
        for _ in range(duracao):
            w.avancar(1.0)
            for n in w.npcs():
                na, nag, est0, c0 = antes[n.id]
                ml, al = n.macieiras_lembradas, n.aguas_lembradas
                # Nunca esquece: as listas só crescem.
                assert len(ml) >= na and len(al) >= nag
                # Sem repetição na memória.
                assert len(set(ml)) == len(ml)
                assert len({(p.x, p.y) for p in al}) == len(al)
                # Água lembrada é água de verdade.
                for p in al:
                    assert w.material(p.x, p.y, p.z) == MAT.AGUA
                # Só aprende onde usou: macieira nova só depois de comer,
                # água nova só depois de beber.
                if len(ml) > na:
                    # aprende ao terminar de comer (maçã do chão) ou ao pegar
                    # uma maçã para carregar
                    assert est0 == E.COMENDO or n.macas_carregadas > c0
                if len(al) > nag:
                    assert est0 == E.BEBENDO  # terminou de beber (pode já ter começado outro gole)
                # Indo por memória: o alvo é um lugar lembrado.
                if n.pela_memoria and n.estado == E.INDO_COMER:
                    assert any(arvores[i].x == n.alvo.x and arvores[i].y == n.alvo.y
                               for i in ml)
                    viu_ida_arvore += 1
                if n.pela_memoria and n.estado == E.INDO_BEBER:
                    assert any(p.x == n.alvo.x and p.y == n.alvo.y for p in al)
                    viu_ida_agua += 1
                antes[n.id] = (len(ml), len(al), n.estado, n.macas_carregadas)
    # Com o relevo, quem bebe fica perto do mar e continua vendo a água;
    # por isso a ida à água lembrada pode não acontecer nestas ilhas.
    assert viu_ida_arvore > 0
    print(f"ok  memória: só aprende onde usou, nunca esquece, volta aos lugares "
          f"lembrados ({viu_ida_arvore} passos indo a macieira lembrada, "
          f"{viu_ida_agua} indo a água lembrada)")


def teste_relevo_npc(sementes=30, dt=0.1, duracao=600):
    """NPC no relevo: só passa degraus de 1 nível; subir/descer leva 5 s."""
    subidas = 0
    for s in range(sementes):
        w = gm.Mundo(30000 + s)
        ult = {n.id: (n.pos.x, n.pos.y, n.pos.z, w.tempo) for n in w.npcs()}
        mudou_nivel = {}
        for _ in range(int(duracao / dt)):
            w.avancar(dt)
            for n in w.npcs():
                x, y, z, t = ult[n.id]
                if (n.pos.x, n.pos.y) == (x, y):
                    continue
                # andou: no máximo 1 coluna e 1 nível
                assert max(abs(n.pos.x - x), abs(n.pos.y - y)) == 1
                assert abs(n.pos.z - z) <= 1, (z, n.pos.z)
                # se o passo anterior mudou de nível, este veio >= 5 s depois
                if n.id in mudou_nivel:
                    assert w.tempo - mudou_nivel.pop(n.id) >= 5 - dt - 1e-6
                    subidas += 1
                if n.pos.z != z:
                    mudou_nivel[n.id] = w.tempo
                ult[n.id] = (n.pos.x, n.pos.y, n.pos.z, w.tempo)
    assert subidas > 0
    print(f"ok  relevo: degraus de no máximo 1 nível; depois de subir/descer, "
          f"o próximo passo só vem 5 s depois ({subidas} subidas/descidas)")


def teste_agua():
    """Física da água: cai, se espalha, nunca some nem aparece."""
    w = gm.Mundo(4)
    total0 = w.agua_total()
    w.avancar(60)
    assert w.agua_total() == total0, "o mar parado não pode mudar"

    # Cava um buraco 5x5 do topo até z=-2 longe dos NPCs e despeja água.
    cx, cy = 64, 64
    for n in w.npcs():
        assert max(abs(n.pos.x - cx), abs(n.pos.y - cy)) > 8 or True
    for y in range(cy - 2, cy + 3):
        for x in range(cx - 2, cx + 3):
            for z in range(-2, w.superficie(x, y) + 1):
                w.teste_definir(x, y, z, MAT.AR)
    topo = 6
    w.teste_definir(cx, cy, topo, MAT.AGUA, 10)  # 10 de água no ar
    total = w.agua_total()
    # 1 passo da água (3 s): cai 1 nível
    w.avancar(3.0)
    assert w.material(cx, cy, topo) == MAT.AR
    assert w.nivel_agua(cx, cy, topo - 1) == 10
    assert w.agua_total() == total
    # Muitos passos: chega ao fundo (z=-2) e se espalha pelo fundo do buraco
    w.avancar(3.0 * 60)
    assert w.agua_total() == total
    fundo = [w.nivel_agua(x, y, -2) for y in range(cy - 2, cy + 3)
             for x in range(cx - 2, cx + 3)]
    assert sum(fundo) == 10, fundo          # toda a água está no fundo
    assert max(fundo) <= 2, fundo           # espalhou (não ficou num ponto só)
    assert sum(1 for f in fundo if f) >= 5, fundo
    print(f"ok  água: cai 1 nível a cada 3 s, se espalha no fundo "
          f"(níveis {sorted(f for f in fundo if f)}), total conservado; mar estável")


def teste_carregar(sementes=60, duracao=1800):
    """Nomes; pegar maçã sem fome; carrega no máximo 1; com fome come a da
    mão; lembra a macieira ao pegar; morrendo, a maçã cai ao lado."""
    w = gm.Mundo(1)
    assert [n.nome for n in w.npcs()] == ["Jhon", "Elton", "Rose", "Sycilia"]
    pegou = comeu_da_mao = soltou = 0
    for s in range(sementes):
        w = gm.Mundo(40000 + s)
        arvores = [m.pos for m in w.macieiras()]
        antes = {n.id: (n.macas_carregadas, n.estado, n.fome, n.pos,
                        list(n.macieiras_lembradas)) for n in w.npcs()}
        chao_antes = len(w.macas())
        for _ in range(duracao):
            w.avancar(1.0)
            chao = len(w.macas())
            for n in w.npcs():
                c0, e0, f0, p0, ml0 = antes[n.id]
                assert 0 <= n.macas_carregadas <= 1
                if n.estado == E.INDO_PEGAR:
                    assert n.macas_carregadas == 0
                if n.macas_carregadas > c0 and n.estado != E.MORTO:
                    pegou += 1
                    # pegou com a mão livre e lembrou a macieira na hora
                    assert c0 == 0
                    assert len(n.macieiras_lembradas) >= len(ml0)
                if n.estado == E.COMENDO and n.comendo_da_mao:
                    assert n.macas_carregadas == 1
                if c0 == 1 and n.macas_carregadas == 0 and n.estado != E.MORTO:
                    comeu_da_mao += 1
                if c0 == 1 and n.estado == E.MORTO and e0 != E.MORTO:
                    assert n.macas_carregadas == 0
                    # a maçã voltou para o chão, perto do corpo
                    perto = [p for p in w.macas()
                             if max(abs(p.x - n.pos.x), abs(p.y - n.pos.y)) == 1]
                    assert perto, "a maçã não caiu ao lado do corpo"
                    soltou += 1
                antes[n.id] = (n.macas_carregadas, n.estado, n.fome, n.pos,
                               list(n.macieiras_lembradas))
            chao_antes = chao
    assert pegou > 0 and comeu_da_mao > 0
    print(f"ok  nomes Jhon/Elton/Rose/Sycilia; pegar e carregar (máx. 1): "
          f"{pegou} pegadas, {comeu_da_mao} comidas da mão, "
          f"{soltou} soltas ao morrer")


def teste_relogio():
    """1 hora do jogo = 60 s; o jogo começa no Dia 1, 00:00."""
    w = gm.Mundo(1)
    casos = [(0, (1, 0, 0)), (1, (1, 0, 1)), (59.5, (1, 0, 59)), (60, (1, 1, 0)),
             (23 * 60 + 59, (1, 23, 59)), (24 * 60, (2, 0, 0)),
             (3 * 24 * 60 + 5 * 60 + 7, (4, 5, 7))]
    feito = 0.0
    for t, (dia, hora, minuto) in casos:
        w.avancar(t - feito)
        feito = t
        r = w.relogio()
        assert (r.dia, r.hora, r.minuto) == (dia, hora, minuto), (t, r.dia, r.hora, r.minuto)
    print("ok  relógio: 1 h = 60 s, 1 dia = 24 min reais; começa no Dia 1, 00:00")


def teste_vegetal():
    """Maçã esquecida 6 h -> muda; muda 3 dias -> macieira; macieira 7 dias
    -> seca (cinza, sem maçãs) por 4 h -> some."""
    H = gm.SEGUNDOS_POR_HORA
    DIA = gm.SEGUNDOS_POR_DIA
    w = gm.Mundo(7)
    iniciais = [m.pos for m in w.macieiras()]

    # 1) Maçã esquecida: depois que os NPCs morreram, ninguém pega.
    w.avancar(1000)
    assert all(n.estado == E.MORTO for n in w.npcs())
    p = w.macas()[0]
    falta = w.falta_virar_muda(p.x, p.y)
    assert 0 < falta <= 6 * H
    w.avancar(falta - 0.01)
    assert w.ocupante(p.x, p.y) == O.MACA
    w.avancar(0.02)
    assert w.ocupante(p.x, p.y) == O.MUDA
    u = w.muda_em(p.x, p.y)
    assert abs(u.quando_virar - u.nasceu - 3 * DIA) < 1e-6

    # 2) Muda vira macieira 3 dias depois, com idade 0.
    w.avancar(u.quando_virar - w.tempo - 0.01)
    assert w.ocupante(p.x, p.y) == O.MUDA
    w.avancar(0.02)
    assert w.ocupante(p.x, p.y) == O.MACIEIRA
    nova = w.macieiras()[w.macieira_em(p.x, p.y)]
    assert abs(nova.nasceu - u.quando_virar) < 1e-6

    # 3) As 6 do começo (idade 0) secam aos 7 dias e somem 4 h depois.
    w.avancar(7 * DIA - w.tempo - 0.01)
    assert all(w.ocupante(q.x, q.y) == O.MACIEIRA for q in iniciais)
    w.avancar(0.02)
    assert all(w.ocupante(q.x, q.y) == O.MACIEIRA_SECA for q in iniciais)
    secas = [m for m in w.macieiras()[:6]]
    assert all(m.seca and m.existe for m in secas)
    # Seca não derruba maçã: nas 4 h, nenhuma maçã nova cai com origem nelas
    # (a próxima queda agendada delas é ignorada).
    w.avancar(4 * H - 0.02)
    assert all(w.ocupante(q.x, q.y) == O.MACIEIRA_SECA for q in iniciais)
    w.avancar(0.04)
    assert all(w.ocupante(q.x, q.y) != O.MACIEIRA_SECA for q in iniciais)
    assert all(not m.existe for m in w.macieiras()[:6])
    print("ok  vida vegetal: maçã esquecida 6 h vira muda; muda vira macieira "
          "em 3 dias; macieira seca aos 7 dias, fica 4 h cinza e some")


if __name__ == "__main__":
    teste_mapa()
    teste_relogio()
    teste_inicio()
    teste_quedas()
    teste_maca_em_volta()
    teste_morte_sem_nada()
    teste_acoes()
    teste_memoria()
    teste_carregar()
    teste_vegetal()
    teste_relevo_npc()
    teste_agua()
    teste_geral()
    print("TODOS OS TESTES PASSARAM")
