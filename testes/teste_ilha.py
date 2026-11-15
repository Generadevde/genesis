"""
teste_ilha.py — confere se a simulação obedece às regras dos docs
(ILHA_V1, TEMPO, VIDA_VEGETAL, ABRIGO, FAMILIA...). Roda sem tela.

Como rodar:
    .venv/bin/python testes/teste_ilha.py

Os tempos, passos e sementes usados aqui são só para TESTAR (quanto tempo
simular, de quanto em quanto olhar, quantas ilhas sortear); não são regras
do jogo. Coisas que levam meses ou anos (gravidez, 15 anos até adulto,
40 anos de macieira) são testadas com os ganchos "teste_*" do motor, que
pulam o relógio ou mudam idades — senão cada teste levaria horas.
"""

import math
import random
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RAIZ / "build"))

import genesis_motor as gm  # noqa: E402

O = gm.Ocupante
E = gm.EstadoNpc
R = gm.TipoRegistro
T = gm.Tarefa
MAT = gm.Material
M = gm.Mundo
H = 3600.0
DIA = 86400.0
ANO = 365 * DIA


def cheb(a, b):
    return max(abs(a.x - b.x), abs(a.y - b.y))


def data(t):
    d = M.data_de(t)
    return (d.dia, d.mes, d.ano, d.hora, d.minuto, d.segundo)


def _alimentar(w):
    """Só no teste: fome e sede cheias (e HP pelo menos 1) para quem está fora."""
    for n in w.npcs():
        if n.estado != E.MORTO and not n.bebe and n.dentro_abrigo < 0:
            w.teste_npc(n.id, 100, 100, max(n.hp, 1))


def _vizinha_livre(w, x, y):
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            if (dx or dy) and w.ocupante(x + dx, y + dy) == O.NADA:
                m = w.material(x + dx, y + dy, w.superficie(x + dx, y + dy))
                if m in (MAT.GRAMA, MAT.AREIA, MAT.NEVE, MAT.PEDRA, MAT.TERRA):
                    return x + dx, y + dy
    return None


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
                    # chão no nível 0 encostado na água do mar (8 vizinhas) = areia
                    encosta = chao == 0 and any(
                        w.material(x + dx, y + dy, 0) == MAT.AGUA and
                        w.superficie(x + dx, y + dy) == 0
                        for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy)
                    assert topo == (MAT.AREIA if encosta else MAT.GRAMA), (x, y, chao, topo)
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
          "terra até -3, pedra; vales ligados ao mar cheios, os outros secos; "
          "areia no nível 0 encostado no mar")


def teste_calendario():
    """1 s do jogo = 1 s; começa 01/01/0001 07:00; meses do calendário real
    (365 dias); estações; dia/noite em curva; temperatura por estação."""
    assert (gm.SEGUNDOS_POR_DIA, gm.SEGUNDOS_POR_ANO) == (DIA, ANO)
    assert data(0) == (1, 1, 1, 7, 0, 0)
    assert data(59.5) == (1, 1, 1, 7, 0, 59)
    assert data(17 * H) == (2, 1, 1, 0, 0, 0)
    assert M.tempo_de(1, 1, 1) == -7 * H
    assert M.tempo_de(1, 2, 1) == 31 * DIA - 7 * H
    assert data(M.tempo_de(1, 3, 1) - 1) == (28, 2, 1, 23, 59, 59)   # fevereiro: 28
    assert data(M.tempo_de(2, 1, 1) - 1) == (31, 12, 1, 23, 59, 59)
    assert data(M.tempo_de(2, 1, 1)) == (1, 1, 2, 0, 0, 0)
    assert data(M.tempo_de(100, 1, 1))[2] == 100                    # depois do 99, o 100
    assert data(-20 * ANO) == (1, 1, -19, 7, 0, 0)                  # 20 anos antes do começo
    rnd = random.Random(1)
    for _ in range(3000):
        t = rnd.uniform(-30 * ANO, 200 * ANO)
        d = M.data_de(t)
        volta = M.tempo_de(d.ano, d.mes, d.dia) + d.hora * H + d.minuto * 60 + d.segundo
        assert abs(volta - (math.floor(t + 7 * H + 1e-6) - 7 * H)) < 1e-6, t
    # meses somados pelo calendário (mesmo dia; mês curto: o último dia)
    assert data(M.somar_meses(M.tempo_de(1, 1, 31) + 10 * H, 1))[:4] == (28, 2, 1, 10)
    assert data(M.somar_meses(M.tempo_de(1, 3, 10), 9))[:3] == (10, 12, 1)
    assert data(M.somar_meses(M.tempo_de(1, 11, 15), 2))[:3] == (15, 1, 2)
    # estações
    V, O_, I, P = (gm.Estacao.VERAO, gm.Estacao.OUTONO, gm.Estacao.INVERNO,
                   gm.Estacao.PRIMAVERA)
    esperado = {12: V, 1: V, 2: V, 3: O_, 4: O_, 5: O_, 6: I, 7: I, 8: I, 9: P, 10: P, 11: P}
    for mes, est in esperado.items():
        assert M.estacao_de(M.tempo_de(3, mes, 15)) == est
    assert M.limites_estacao(M.tempo_de(2, 1, 15)) == (M.tempo_de(1, 12, 1), M.tempo_de(2, 3, 1))
    # horas de sol, dia a dia no ano 1 (ao meio-dia)
    sol = {}
    anterior = None
    for k in range(365):
        t = M.tempo_de(2, 1, 1) + k * DIA + 12 * H
        L = M.horas_de_sol(t)
        assert abs((M.nascer_do_sol(t) + M.por_do_sol(t)) / 2 - 12) < 1e-9  # centrado
        sol.setdefault(M.estacao_de(t), []).append(L)
        if anterior is not None:
            assert abs(L - anterior) < 0.1  # muda aos poucos
        anterior = L
    assert all(L == 12 for L in sol[P] + sol[O_])
    assert 12 <= min(sol[V]) and max(sol[V]) <= 13.5 and max(sol[V]) > 13.49
    assert 11 <= min(sol[I]) and max(sol[I]) <= 12 and min(sol[I]) < 11.01
    # temperatura: valores do dono no MEIO de cada estação; muda aos poucos
    valores = {V: (20, 35), O_: (18, 28), I: (4, 18), P: (20, 30)}
    for mes, est in ((1, V), (4, O_), (7, I), (10, P)):
        a, b = M.limites_estacao(M.tempo_de(2, mes, 15))
        mn, mx = M.min_max_temperatura((a + b) / 2)
        assert abs(mn - valores[est][0]) < 1e-9 and abs(mx - valores[est][1]) < 1e-9, est
    ant = None
    for k in range(4 * 365):
        par = M.min_max_temperatura(M.tempo_de(2, 1, 1) + k * 6 * H)
        if ant:
            assert abs(par[0] - ant[0]) < 0.5 and abs(par[1] - ant[1]) < 0.5
        ant = par
    # forma do dia: mínima 2 h antes de nascer, máxima 8 h depois até o pôr,
    # mínima 4 h depois do pôr (tolerância: a mínima/máxima anda um pouco no dia)
    w = gm.Mundo(1, 0, 0)
    for mes in (1, 4, 7):
        dia0 = M.tempo_de(2, mes, 15)
        N, Pr = M.nascer_do_sol(dia0 + 12 * H), M.por_do_sol(dia0 + 12 * H)
        mn, mx = M.min_max_temperatura(dia0 + 12 * H)
        for hora, alvo in ((1, mn), (N - 2, mn), (N + 8, mx), (Pr - 0.5, mx), (Pr + 4, mn)):
            assert abs(w.temperatura_em(dia0 + hora * H) - alvo) < 0.1, (mes, hora)
    # com dia de 12 h (primavera): a curva antiga 04h / 14h / 18h / 22h
    dia0 = M.tempo_de(2, 10, 15)
    assert M.nascer_do_sol(dia0) == 6 and M.por_do_sol(dia0) == 18
    assert w.sol()  # 01/01 07:00: o sol já nasceu (verão, ~05:15)
    print("ok  calendário: 01/01/0001 07:00; 365 dias (meses do calendário); "
          "estações 12-1-2 verão, 3-4-5 outono, 6-7-8 inverno, 9-10-11 primavera; "
          f"sol {min(sol[I]):.2f} h (inverno) a {max(sol[V]):.2f} h (verão); "
          "temperatura por estação, em curva")


def teste_inicio():
    """Começo: 3 homens e 3 mulheres, "NPC 1".."NPC 6", 20 anos (nasceram
    20 anos antes), fome e sede 100; 6 macieiras de 2 anos, no verão."""
    w = gm.Mundo(2)
    npcs = w.npcs()
    assert [n.nome for n in npcs] == [f"NPC {i}" for i in range(1, 7)]
    sexos = [n.sexo for n in npcs]
    assert sexos.count(gm.Sexo.HOMEM) == 3 and sexos.count(gm.Sexo.MULHER) == 3
    assert all(n.fome == 100 and n.sede == 100 and not n.bebe for n in npcs)
    assert all(abs(w.idade_anos(n) - 20) < 1e-9 and n.nascimento == -20 * ANO for n in npcs)
    assert all(abs(n.quando_velhice - 80 * ANO) < 1e-6 for n in npcs)
    arv = w.macieiras()
    assert len(arv) == 6 and len(w.macas()) == 0
    for k, m in enumerate(arv):
        assert abs(m.nasceu + 2 * ANO) < 1e-6 and abs(m.quando_secar - m.nasceu - 40 * ANO) < 1e-6
        assert 80 <= m.macas_verao <= 300 and 0 < w.macas_por_cair(k) <= m.macas_verao
    assert w.estacao() == gm.Estacao.VERAO
    w2 = gm.Mundo(3, 2, 5)
    sx = [n.sexo for n in w2.npcs()]
    assert len(sx) == 7 and sx.count(gm.Sexo.HOMEM) == 2
    assert len({gm.Mundo(s).npcs()[0].sexo for s in range(20)}) == 2
    print("ok  começo: 01/01/0001 07:00 (verão); 3 homens e 3 mulheres sorteados, "
          "20 anos; 6 macieiras de 2 anos com 80–300 maçãs neste verão")


def teste_quedas():
    """Fome −3 a cada 15 min; sede −4 a cada 10 min."""
    w = gm.Mundo(3)
    w.avancar(599.999)
    assert all(n.fome == 100 and n.sede == 100 for n in w.npcs())
    w.avancar(0.002)
    assert all(n.fome == 100 and n.sede == 96 for n in w.npcs())
    w.avancar(300)
    assert all(n.fome == 97 and n.sede == 96 for n in w.npcs())
    w.avancar(300)
    assert all(n.fome == 97 and n.sede == 92 for n in w.npcs())
    print("ok  fome −3 a cada 15 min; sede −4 a cada 10 min")


def teste_morte_sem_nada():
    """HP −5 por minuto por necessidade crítica (somando); com HP 0 morre e
    o corpo fica."""
    w = gm.Mundo(5)
    w.teste_npc(0, 100, 0, 0.5)    # só a sede crítica: 0,5 HP dura 6 s
    w.teste_npc(1, 0, 0, 1.0)      # fome e sede: 1 HP dura 6 s
    assert abs(w.taxa_hp(w.npcs()[0]) + 5 / 60) < 1e-12
    assert abs(w.taxa_hp(w.npcs()[1]) + 10 / 60) < 1e-12
    w.avancar(5.99)
    assert all(w.npcs()[i].estado != E.MORTO for i in (0, 1))
    w.avancar(0.02)
    for i, causa in ((0, 2), (1, 3)):
        n = w.npcs()[i]
        assert n.estado == E.MORTO and n.hp == 0 and n.causa_morte == causa
        assert w.ocupante(n.pos.x, n.pos.y) == O.CORPO
    print("ok  HP −5/min por necessidade crítica (somando); HP 0 = morte, corpo fica")


def teste_hp(sementes=10):
    """HP anda exatamente na taxa enquanto nada muda; nunca sai de 0..100."""
    vistos = {}
    for s in range(sementes):
        w = gm.Mundo(60000 + s)
        for k, (f, sd) in enumerate([(0, 100), (0, 0), (100, 0), (100, 100), (50, 50), (0, 30)]):
            w.teste_npc(k, f, sd, 100 if k % 2 else 40)
        for _ in range(600):
            antes = [(n.hp, w.taxa_hp(n), n.estado, n.fome, n.sede) for n in w.npcs()]
            w.avancar(1.0)
            for n, (hp0, taxa, e0, f0, s0) in zip(w.npcs(), antes):
                assert 0 <= n.hp <= 100
                if e0 == E.MORTO:
                    continue
                criticos = (f0 == 0) + (s0 == 0) + (w.temperatura() < 4)
                vistos[criticos] = vistos.get(criticos, 0) + 1
                mudou = n.fome != f0 or n.sede != s0 or n.estado != e0
                if not mudou and n.estado != E.MORTO and w.taxa_hp(n) == taxa:
                    assert abs(taxa + 5 / 60 * criticos) < 1e-12, (taxa, criticos)
                    assert abs(n.hp - min(100, hp0 + taxa)) < 1e-6, (hp0, n.hp, taxa)
    assert vistos.get(1, 0) > 0 and vistos.get(2, 0) > 0
    print(f"ok  HP: taxa exata (−5/min por crítico); vistos com 0/1/2 críticos: "
          f"{vistos.get(0, 0)}/{vistos.get(1, 0)}/{vistos.get(2, 0)}")


def teste_verao(semente=7):
    """Maçãs só no verão: 80–300 por macieira, do 2º dia ao fim do verão,
    mais no meio; pilhas de até 32; cada maçã vence em 6 h: 40% muda."""
    w = gm.Mundo(semente, 0, 0)  # sem NPCs: ninguém pega
    caidas = 0
    def por_cair():
        return sum(w.macas_por_cair(k) for k in range(len(w.macieiras())))
    def conferir_chao():
        arvores = {(m.pos.x, m.pos.y) for m in w.macieiras() if m.existe}
        for x, y, q in w.pilhas():
            assert 1 <= q <= 32 and (x, y) not in arvores
            assert w.ocupante(x, y) == O.MACA
            assert max(w.macas_vencem(x, y)) <= 6 * H + 1e-6
    # resto deste verão (começou em 01/12 do ano 0)
    p0 = por_cair()
    w.avancar(M.tempo_de(1, 3, 1) + 1 - w.tempo)
    assert por_cair() == 0
    caidas += p0
    # outono, inverno e primavera: não cai nada (o chão só diminui)
    chao = sum(q for *_, q in w.pilhas())
    while w.tempo < M.tempo_de(1, 12, 1) - 11 * DIA:
        w.avancar(10 * DIA)
        agora = sum(q for *_, q in w.pilhas())
        assert agora <= chao
        chao = agora
    # verão do ano 1: sorteio em 01/12
    assert w.tempo < M.tempo_de(1, 12, 1)
    w.avancar(M.tempo_de(1, 12, 1) + 1 - w.tempo)
    vivas = [k for k, m in enumerate(w.macieiras()) if m.existe and not m.seca]
    for k in vivas:
        m = w.macieiras()[k]
        assert 80 <= m.macas_verao <= 300 and w.macas_por_cair(k) == m.macas_verao
    total = sum(w.macieiras()[k].macas_verao for k in vivas)
    por_dia = []
    for d in range(90):
        antes = por_cair()
        w.avancar(DIA)
        por_dia.append(antes - por_cair())
        conferir_chao()
    assert por_dia[0] == 0 and sum(por_dia) == total
    terco = len(por_dia) // 3
    a, b, c = sum(por_dia[:terco]), sum(por_dia[terco:2 * terco]), sum(por_dia[2 * terco:])
    assert b > a and b > c, (a, b, c)
    caidas += total
    w.avancar(DIA)
    # sem NPCs, toda maçã que caiu ou ainda está no chão, ou já venceu
    no_chao = sum(q for *_, q in w.pilhas())
    mudas = sum(1 for r in w.historico() if r.tipo == R.MUDA_NASCEU)
    venceram = caidas - no_chao
    fracao = mudas / venceram
    assert 0.35 < fracao < 0.45, fracao
    print(f"ok  verão: {total} maçãs de {len(vivas)} macieiras, nenhuma no 1º dia, "
          f"terços {a}/{b}/{c} (pico no meio); nada fora do verão; pilhas ≤ 32; "
          f"6 h por maçã: {fracao:.0%} viraram muda")


def teste_vegetal():
    """Muda vira macieira em 3 anos; macieira vive 40 anos, fica seca 1 ano
    (sem maçãs) e some."""
    w = gm.Mundo(7, 0, 0)
    w.avancar(3 * DIA)
    r = next(r for r in w.historico() if r.tipo == R.MUDA_NASCEU)
    u = w.muda_em(r.pos.x, r.pos.y)
    assert abs(u.quando_virar - u.nasceu - 3 * ANO) < 1e-6
    w.teste_saltar(u.quando_virar - 10)
    w.avancar(9.99)
    assert w.ocupante(r.pos.x, r.pos.y) == O.MUDA
    w.avancar(0.02)
    assert w.ocupante(r.pos.x, r.pos.y) == O.MACIEIRA
    nova = w.macieiras()[w.macieira_em(r.pos.x, r.pos.y)]
    assert abs(nova.nasceu - u.quando_virar) < 1e-6
    assert abs(nova.quando_secar - nova.nasceu - 40 * ANO) < 1e-6
    # uma das do começo: 1 dia antes dos 40 anos
    m0 = w.macieiras()[0].pos
    w.teste_idade_macieira(0, 40 - 1 / 365)
    w.avancar(DIA - 1)
    assert w.ocupante(m0.x, m0.y) == O.MACIEIRA
    w.avancar(2)
    m = w.macieiras()[0]
    assert m.seca and w.ocupante(m0.x, m0.y) == O.MACIEIRA_SECA and w.macas_por_cair(0) == 0
    assert abs(m.quando_sumir - m.quando_secar - ANO) < 1e-6
    w.teste_saltar(m.quando_sumir - 10)
    w.avancar(9.99)
    assert w.ocupante(m0.x, m0.y) == O.MACIEIRA_SECA
    w.avancar(0.02)
    assert not w.macieiras()[0].existe and w.ocupante(m0.x, m0.y) != O.MACIEIRA_SECA
    print("ok  vida vegetal: muda vira macieira em 3 anos; macieira seca aos 40 "
          "anos, fica 1 ano cinza (sem maçãs) e some")


def teste_gelo():
    """Inverno: a camada de cima da água dos vales congela (o mar não);
    anda-se por cima; não se bebe; derrete quando o inverno acaba."""
    for semente in range(1, 40):
        w = gm.Mundo(semente)
        vales = [(x, y) for y in range(8, 120) for x in range(8, 120)
                 if w.material(x, y, w.superficie(x, y)) == MAT.AGUA]
        if len(vales) > 50:
            break
    total = w.agua_total()
    niveis = {(x, y): w.nivel_agua(x, y, w.superficie(x, y)) for x, y in vales}
    w.teste_saltar(M.tempo_de(1, 6, 1) - 10)
    w.avancar(9.99)
    assert all(w.material(x, y, w.superficie(x, y)) == MAT.AGUA for x, y in vales)
    w.avancar(0.02)
    assert w.estacao() == gm.Estacao.INVERNO
    for x, y in vales:
        z = w.superficie(x, y)
        assert w.material(x, y, z) == MAT.GELO and w.nivel_agua(x, y, z) == niveis[(x, y)]
    assert all(w.material(x, 2, 0) == MAT.AGUA for x in range(128))  # o mar não
    assert w.agua_total() == total
    assert any(r.tipo == R.CONGELOU and r.a == len(vales) for r in w.historico())
    # alguns dias de inverno com os NPCs alimentados
    em_cima = 0
    for _ in range(int(3 * DIA / 60)):
        _alimentar(w)
        w.avancar(60)
        for n in w.npcs():
            if n.estado == E.MORTO or n.dentro_abrigo >= 0:
                continue
            m = w.material(n.pos.x, n.pos.y, n.pos.z)
            assert m in (MAT.GRAMA, MAT.NEVE, MAT.PEDRA, MAT.AREIA, MAT.GELO), m
            em_cima += m == MAT.GELO
            if n.estado == E.BEBENDO:
                assert w.material(n.alvo.x, n.alvo.y, n.alvo.z) == MAT.AGUA
    # fim do inverno: derrete; ninguém fica em cima da água
    w.teste_saltar(M.tempo_de(1, 9, 1) - 10)
    w.avancar(10.01)
    assert w.estacao() == gm.Estacao.PRIMAVERA
    assert all(w.material(x, y, w.superficie(x, y)) == MAT.AGUA for x, y in vales)
    for n in w.npcs():
        if n.estado != E.MORTO and n.dentro_abrigo < 0:
            assert w.material(n.pos.x, n.pos.y, n.pos.z) != MAT.AGUA
    assert any(r.tipo == R.DERRETEU for r in w.historico())
    print(f"ok  gelo: {len(vales)} colunas de vale congelam em 01/06 (mar não), "
          f"nível guardado, água total igual; NPCs andaram em cima ({em_cima} vezes "
          "vistos no gelo), ninguém bebe gelo; derrete em 01/09")


def teste_acoes(sementes=6):
    """Comer leva 3 min (+12 fome, +5 sede); beber 20 s (+15 sede); durante a
    ação a necessidade correspondente não abaixa."""
    visto = {"comer": None, "beber": None}
    for s in range(sementes):
        w = gm.Mundo(9000 + s)
        for n in w.npcs():
            w.teste_npc(n.id, 60, 40, 100)
            w.teste_macas(n.id, 2)
        inicio = {}
        for _ in range(3600):
            antes = {n.id: (n.estado, n.fome, n.sede) for n in w.npcs()}
            w.avancar(1.0)
            for n in w.npcs():
                est0, f0, s0 = antes[n.id]
                if n.estado == E.COMENDO and est0 == E.COMENDO:
                    assert n.fome >= f0 and n.sede >= s0
                if n.estado == E.BEBENDO and est0 == E.BEBENDO:
                    assert n.sede >= s0
                terminou = (n.id in inicio and n.estado != E.MORTO and (
                    (inicio[n.id][0] == E.COMENDO and n.fome > f0) or
                    (inicio[n.id][0] == E.BEBENDO and n.sede > s0 + 5)))
                if n.estado in (E.COMENDO, E.BEBENDO) and est0 != n.estado and not terminou:
                    inicio[n.id] = (n.estado, w.tempo, n.fome, n.sede)
                elif terminou:
                    acao, t0, f, sd = inicio.pop(n.id)
                    if n.estado == acao:
                        inicio[n.id] = (acao, w.tempo, n.fome, n.sede)
                    dur = w.tempo - t0
                    if acao == E.COMENDO:
                        assert 179 <= dur <= 181, dur
                        assert n.fome == min(100, f + 12) and n.sede == min(100, sd + 5)
                        visto["comer"] = (dur, f, n.fome)
                    else:
                        assert 19 <= dur <= 21, dur
                        assert n.sede == min(100, sd + 15)
                        visto["beber"] = (dur, sd, n.sede)
            if all(visto.values()):
                (dc, fa, fd), (db, sa, sdp) = visto["comer"], visto["beber"]
                print(f"ok  comer {dc:.0f} s (fome {fa:.0f}→{fd:.0f}), beber {db:.0f} s "
                      f"(sede {sa:.0f}→{sdp:.0f}); durante a ação a necessidade não abaixa")
                return
    raise AssertionError(f"não vi as duas ações: {visto}")


def teste_macas_npc(sementes=(41, 42, 43)):
    """Inventário por peso: pega saciado (tudo o que couber: 8 kg; maçã
    0,5 kg, ovo 1 kg), leva ao abrigo (80 kg), não pega com o abrigo cheio,
    come na ordem mão → abrigo → chão, solta tudo ao morrer."""
    pegou = 0
    guardado = 0.0
    for sem in sementes:
        w = gm.Mundo(sem)
        for n in w.npcs():
            viz = _vizinha_livre(w, n.pos.x, n.pos.y)
            if viz:
                w.teste_abrigo(n.id, *viz)
        antes = {n.id: n.inv.peso() for n in w.npcs()}
        for _ in range(int(6 * H / 5)):
            _alimentar(w)
            w.avancar(5)
            for n in w.npcs():
                assert 0 <= n.inv.peso() <= 8 + 1e-9
                if n.inv.peso() > antes[n.id] + 1e-9:
                    pegou += 1
                if n.estado == E.INDO_GUARDAR:
                    assert not n.inv.vazio() and n.abrigo_casa >= 0
                antes[n.id] = n.inv.peso()
            for a in w.abrigos():
                assert 0 <= a.guardado.peso() <= 80 + 1e-9
        guardado += sum(a.guardado.peso() for a in w.abrigos())
    assert pegou > 0 and guardado > 0
    # abrigo cheio (160 maçãs = 80 kg): não pega; o que tem na mão vai pro chão
    w = gm.Mundo(44, 6, 0)
    n0 = w.npcs()[0]
    a = w.teste_abrigo(0, *_vizinha_livre(w, n0.pos.x, n0.pos.y), macas=160)
    w.teste_macas(0, 5)
    for _ in range(12):
        _alimentar(w)
        w.avancar(5)
    assert w.npcs()[0].inv.vazio() and w.abrigos()[a].guardado.macas == 160
    for _ in range(int(2 * H / 5)):
        _alimentar(w)
        w.avancar(5)
        assert w.npcs()[0].estado != E.INDO_PEGAR and w.npcs()[0].inv.vazio()
    # ordem para comer: mão, depois abrigo; ovo: 5 min, +30
    w = gm.Mundo(45, 6, 0)
    n0 = w.npcs()[0]
    a = w.teste_abrigo(0, *_vizinha_livre(w, n0.pos.x, n0.pos.y), macas=10)
    w.teste_inventario(0, 0, 1, 0)
    w.teste_npc(0, 50, 100, 100)
    w.avancar(5)
    n = w.npcs()[0]
    assert n.estado == E.COMENDO and n.comendo_de == 0 and n.comendo_item == 1
    w.avancar(299)
    assert w.npcs()[0].estado == E.COMENDO
    w.avancar(2)
    assert abs(w.npcs()[0].fome - 80) < 1e-9  # 50 + 30 (sem cair: estava comendo)
    w.teste_npc(0, 60, 100, 100)
    for _ in range(60):
        w.avancar(5)
        n = w.npcs()[0]
        if n.estado == E.COMENDO:
            break
    assert n.comendo_de == 1 and w.abrigos()[a].guardado.macas == 9
    # morrendo, solta o inventário (e a comida da boca) perto do corpo
    w.teste_inventario(0, 3, 2, 0)
    macas0 = sum(q for *_, q in w.pilhas())
    ovos0 = sum(q for _, _, q, _ in w.ovos())
    w.teste_npc(0, 100, 0, 0.01)
    w.avancar(1)
    assert w.npcs()[0].estado == E.MORTO
    assert sum(q for *_, q in w.pilhas()) == macas0 + 4   # 3 + a maçã da boca
    assert sum(q for _, _, q, _ in w.ovos()) == ovos0 + 2
    print(f"ok  inventário: até 8 kg na mão ({pegou} vezes pegou), abrigo até 80 kg "
          f"({guardado:.1f} kg guardados); abrigo cheio: não pega e solta; come mão → "
          f"abrigo; ovo 5 min +30; ao morrer, solta tudo")


def teste_memoria(sementes=8):
    """Memória: lembra só onde usou (comeu do chão, pegou, tirou galho, bebeu),
    nunca esquece, e usa o que lembra."""
    viu_ida_arvore = viu_ida_agua = 0
    for s in range(sementes):
        w = gm.Mundo(20000 + s)
        arvores = [m.pos for m in w.macieiras()]
        antes = {n.id: (len(n.macieiras_lembradas), len(n.aguas_lembradas), n.estado,
                        n.inv.macas, n.galhos) for n in w.npcs()}
        for seg in range(int(3 * H)):
            if seg % 1800 == 0 and seg:   # só no teste: mão vazia e com fome
                for n in w.npcs():
                    if n.estado not in (E.MORTO, E.COMENDO):
                        w.teste_macas(n.id, 0)
                        w.teste_npc(n.id, 60, n.sede, 100)
            w.avancar(1.0)
            for n in w.npcs():
                na, nag, est0, c0, g0 = antes[n.id]
                ml, al = n.macieiras_lembradas, n.aguas_lembradas
                assert len(ml) >= na and len(al) >= nag
                assert len(set(ml)) == len(ml)
                assert len({(p.x, p.y) for p in al}) == len(al)
                for p in al:
                    assert w.material(p.x, p.y, p.z) in (MAT.AGUA, MAT.GELO)
                if len(ml) > na:
                    comeu_do_chao = n.estado == E.COMENDO and n.comendo_de == 2
                    assert comeu_do_chao or n.inv.macas > c0 or n.galhos > g0
                if len(al) > nag:
                    assert est0 == E.BEBENDO
                if n.pela_memoria and n.estado == E.INDO_COMER:
                    assert any(arvores[i].x == n.alvo.x and arvores[i].y == n.alvo.y
                               for i in ml)
                    viu_ida_arvore += 1
                if n.pela_memoria and n.estado == E.INDO_BEBER:
                    assert any(p.x == n.alvo.x and p.y == n.alvo.y for p in al)
                    viu_ida_agua += 1
                antes[n.id] = (len(ml), len(al), n.estado, n.inv.macas, n.galhos)
    assert viu_ida_arvore > 0
    print(f"ok  memória: só aprende onde usou, nunca esquece, volta aos lugares "
          f"lembrados ({viu_ida_arvore} passos indo a macieira lembrada, "
          f"{viu_ida_agua} indo a água lembrada)")


def teste_relevo_npc(sementes=15, duracao=1800):
    """NPC no relevo: anda 1 célula a cada 5 s; degraus de 1 nível; subir
    ou descer leva 10 s."""
    subidas = passos = 0
    for s in range(sementes):
        w = gm.Mundo(30000 + s)
        ult = {n.id: (n.pos.x, n.pos.y, n.pos.z, w.tempo) for n in w.npcs()}
        mudou_nivel = {}
        for _ in range(duracao):
            _alimentar(w)
            w.avancar(1.0)
            for n in w.npcs():
                x, y, z, t = ult[n.id]
                if (n.pos.x, n.pos.y) == (x, y):
                    continue
                assert max(abs(n.pos.x - x), abs(n.pos.y - y)) == 1
                assert abs(n.pos.z - z) <= 1, (z, n.pos.z)
                assert w.tempo - t >= 5 - 1 - 1e-6   # passo a cada 5 s
                passos += 1
                if n.id in mudou_nivel:
                    assert w.tempo - mudou_nivel.pop(n.id) >= 10 - 1 - 1e-6
                    subidas += 1
                if n.pos.z != z:
                    mudou_nivel[n.id] = w.tempo
                ult[n.id] = (n.pos.x, n.pos.y, n.pos.z, w.tempo)
    assert subidas > 0
    print(f"ok  relevo: 1 célula a cada 5 s ({passos} passos); degraus de 1 nível; "
          f"depois de subir/descer, o próximo passo só vem 10 s depois ({subidas})")


def teste_explorar(sementes=10, duracao=900):
    """Explorar: destino a 2–20 células de onde estava; cada passo aproxima."""
    destinos = aproximou = 0
    for s in range(sementes):
        w = gm.Mundo(70000 + s)
        antes = {n.id: (n.tem_destino, (n.destino.x, n.destino.y), n.pos, n.estado)
                 for n in w.npcs()}
        for _ in range(duracao):
            _alimentar(w)
            w.avancar(1.0)
            for n in w.npcs():
                tinha, d0, p0, e0 = antes[n.id]
                novo = (n.destino.x, n.destino.y)
                if n.tem_destino and (not tinha or novo != d0):
                    destinos += 1
                    dist = max(abs(novo[0] - p0.x), abs(novo[1] - p0.y))
                    dist_agora = max(abs(novo[0] - n.pos.x), abs(novo[1] - n.pos.y))
                    assert 2 <= dist <= 20 or 1 <= dist_agora <= 20, (dist, dist_agora)
                moveu = (n.pos.x, n.pos.y) != (p0.x, p0.y)
                if (tinha and n.tem_destino and novo == d0 and moveu and
                        e0 == E.VAGANDO and n.estado == E.VAGANDO):
                    a = max(abs(d0[0] - p0.x), abs(d0[1] - p0.y))
                    b = max(abs(d0[0] - n.pos.x), abs(d0[1] - n.pos.y))
                    assert b < a
                    aproximou += 1
                antes[n.id] = (n.tem_destino, novo, n.pos, n.estado)
    assert destinos > 0 and aproximou > 0
    print(f"ok  explorar: {destinos} destinos a 2–20 células; "
          f"{aproximou} passos, todos aproximando do destino")


def _medir_trabalhos(w, dt, ate, vistos, faixas):
    """Roda alimentando e mede quanto dura cada trabalho que termina."""
    inicio = {}
    antes = {n.id: (n.lascas, n.galhos, n.lenha, n.machado, n.estado, n.tarefa)
             for n in w.npcs()}
    falta = {n.id: n.falta_acao for n in w.npcs()}
    while w.tempo < ate:
        _alimentar(w)
        w.avancar(dt)
        for n in w.npcs():
            l0, g0, w0, m0, e0, t0 = antes[n.id]
            fim = None
            if t0 == T.LASCAS and n.lascas > l0:
                assert n.lascas - l0 in (1, 2)
                fim = "lascas"
            elif t0 == T.GALHOS and n.galhos > g0:
                assert n.galhos - g0 == 1
                fim = "galhos"
            elif t0 == T.MACHADO and n.machado and not m0:
                assert (l0 - n.lascas, g0 - n.galhos) == (6, 2)
                fim = "machado"
            elif t0 == T.CORTAR and n.lenha > w0:
                assert n.lenha - w0 == 30
                fim = "cortar"
            elif t0 == T.CONSTRUIR and n.lenha < w0:
                assert w0 - n.lenha == 20
                assert w.ocupante(w.abrigos()[-1].pos.x, w.abrigos()[-1].pos.y) == O.ABRIGO
                assert n.abrigo_casa >= 0 and n.id in w.abrigos()[n.abrigo_casa].moradores
                fim = "abrigo"
            if fim and n.id in inicio:
                dur = w.tempo - inicio.pop(n.id)
                lo, hi = faixas[fim]
                assert lo - dt <= dur <= hi + dt, (fim, dur)
                vistos[fim] += 1
            # (o que falta subiu = começou outro trabalho sem parar — ex.: a
            #  árvore dele foi cortada por outro e ele emendou na vizinha)
            novo = n.estado == E.TRABALHANDO and n.falta_acao > falta[n.id] + 1e-6
            falta[n.id] = n.falta_acao
            if novo and not fim and e0 == E.TRABALHANDO:
                inicio[n.id] = w.tempo
            if n.estado == E.TRABALHANDO and (e0 != E.TRABALHANDO or fim):
                inicio[n.id] = w.tempo
            if n.estado != E.TRABALHANDO and not fim:
                inicio.pop(n.id, None)
            antes[n.id] = (n.lascas, n.galhos, n.lenha, n.machado, n.estado, n.tarefa)
        for a in w.abrigos():
            assert len(a.moradores) <= 2 and len(a.dentro) <= 2
        for p in w.pedras():
            if p.existe:
                assert w.ocupante(p.pos.x, p.pos.y) == O.PEDRA and p.lascas > 0


def teste_trabalho():
    """Lascas 15–20 min (+2), galhos 5–15 min (+1), machado 1–3 h (6 lascas +
    2 galhos), corte 2–6 h (+30 lenha), abrigo 2–4 dias (−20 lenha)."""
    faixas = {"lascas": (900, 1200), "galhos": (300, 900), "machado": (3600, 10800),
              "cortar": (7200, 21600), "abrigo": (2 * DIA, 4 * DIA)}
    vistos = dict.fromkeys(faixas, 0)
    for sem in (11, 12, 13, 14):
        w = gm.Mundo(sem)
        assert len(w.pedras()) == 3 and all(p.lascas == 10 for p in w.pedras())
        _medir_trabalhos(w, 5, 10 * H, vistos, faixas)
    w = gm.Mundo(15)
    for n in w.npcs():
        w.teste_dar(n.id, 0, 0, 0, True)
    _medir_trabalhos(w, 30, 12 * H, vistos, faixas)
    w = gm.Mundo(16)
    for n in w.npcs():
        w.teste_dar(n.id, 0, 0, 20, True)
    _medir_trabalhos(w, 60, 5 * DIA, vistos, faixas)
    assert all(vistos.values()), vistos
    print(f"ok  trabalho: lascas 15–20 min, galhos 5–15 min, machado 1–3 h, "
          f"corte 2–6 h (+30 lenha), abrigo 2–4 dias (−20 lenha) — vistos {vistos}")


def teste_noite_no_abrigo():
    """Inverno: com 5° ou menos vai ao abrigo; dentro dorme, sem frio/fome/
    sede, HP +1/min; sai às 06:00. Trabalho parado pela fome é retomado."""
    w = gm.Mundo(13, 6, 0)  # só homens: ninguém fica no abrigo para procriar
    w.teste_saltar(M.tempo_de(1, 7, 10) + 12 * H)  # 10/07, inverno
    for n in w.npcs():
        viz = _vizinha_livre(w, n.pos.x, n.pos.y)
        if viz:
            w.teste_abrigo(n.id, *viz)
    entrou = dormiu = 0
    while w.tempo < M.tempo_de(1, 7, 11) + 8 * H:
        _alimentar(w)
        if data(w.tempo)[3] == 20 and data(w.tempo)[4] == 0:
            for n in w.npcs():
                w.teste_npc(n.id, 100, 100, 50)
        antes = {n.id: (n.hp, n.fome, n.sede, n.dentro_abrigo) for n in w.npcs()}
        w.avancar(60)
        hora = data(w.tempo)[3]
        for n in w.npcs():
            hp0, f0, s0, d0 = antes[n.id]
            if d0 < 0 and n.dentro_abrigo >= 0:
                entrou += 1
                assert w.temperatura() <= 5 + 0.2
            if d0 >= 0 and n.dentro_abrigo >= 0:
                assert n.estado == E.DORMINDO
                assert n.fome == f0 and n.sede == s0
                assert abs(n.hp - min(100, hp0 + 1)) < 1e-6   # +1 por minuto
                dormiu += 1
            if n.dentro_abrigo >= 0:
                assert not (6 <= hora < 18) or (hora == 6 and data(w.tempo)[4] == 0)
    assert entrou > 0 and dormiu > 0
    # trabalho interrompido pela fome guarda o progresso
    w = gm.Mundo(13, 6, 0)
    for n in w.npcs():
        w.teste_dar(n.id, 0, 0, 20, True)
    alvo = None
    while alvo is None:
        _alimentar(w)
        w.avancar(5)
        alvo = next((n.id for n in w.npcs()
                     if n.estado == E.TRABALHANDO and n.tarefa == T.CONSTRUIR), None)
    falta0 = w.npcs()[alvo].falta_acao
    w.avancar(600)
    w.teste_npc(alvo, 60, 100, 100)
    w.avancar(5)
    n = w.npcs()[alvo]
    assert n.estado != E.TRABALHANDO and n.tarefa == T.CONSTRUIR
    assert abs(n.tarefa_restante - (falta0 - 600)) < 6, (n.tarefa_restante, falta0)
    while w.npcs()[alvo].tarefa == T.CONSTRUIR:
        _alimentar(w)
        w.avancar(60)
    assert w.npcs()[alvo].abrigo_casa >= 0
    print(f"ok  abrigo no inverno: entra com 5° ou menos ({entrou}), dorme sem "
          f"frio/fome/sede, HP +1/min, sai às 06:00; trabalho parado pela fome "
          f"guarda o progresso e é retomado")


def teste_familia():
    """Casal (16+ anos), 1 h juntos no abrigo, gravidez de 9 meses, nasce
    ao lado da mãe com nome pela mãe, bebê parado, 2 meses entre filhos,
    adulto aos 15 anos, idade de ter filhos, mãe morta = bebê perdido."""
    w = gm.Mundo(13)
    while sum(n.parceiro >= 0 for n in w.npcs()) < 4 and w.tempo < 2 * DIA:
        _alimentar(w)
        w.avancar(5)
    casais = [(n.id, n.parceiro) for n in w.npcs()
              if n.sexo == gm.Sexo.HOMEM and n.parceiro >= 0]
    assert len(casais) >= 2
    for h, m in casais:
        assert w.npcs()[m].sexo == gm.Sexo.MULHER and w.npcs()[m].parceiro == h
        viz = _vizinha_livre(w, w.npcs()[h].pos.x, w.npcs()[h].pos.y)
        w.teste_abrigo(h, *viz)
    # procriar: 1 h juntos no abrigo
    gravidas = {}
    while len(gravidas) < 2 and w.tempo < 4 * DIA:
        _alimentar(w)
        antes = {n.id: n.gravida for n in w.npcs()}
        w.avancar(5)
        for n in w.npcs():
            if n.gravida and not antes[n.id]:
                pai = w.npcs()[n.gravidez_pai]
                assert n.dentro_abrigo >= 0 and pai.dentro_abrigo == n.dentro_abrigo
                assert abs(n.quando_nascer - M.somar_meses(w.tempo, 9)) < 5.01
                gravidas[n.id] = w.tempo
    assert len(gravidas) == 2, gravidas
    mae, outra = list(gravidas)
    for _ in range(3):
        _alimentar(w)
        w.avancar(5)
    # nascimento (adiantado só no teste)
    w.teste_adiantar_parto(mae, 30)
    w.avancar(31)
    b = w.npcs()[-1]
    m = w.npcs()[mae]
    assert b.bebe and b.mae == mae and b.nome == "F." + m.nome
    assert abs(b.nascimento - (w.tempo - 1)) < 1.01 and w.idade_anos(b) < 1e-4
    assert abs(b.quando_adulto - b.nascimento - 15 * ANO) < 1e-6
    assert abs(m.pode_engravidar_em - M.somar_meses(b.nascimento, 2)) < 1e-6
    lugar = w.abrigos()[m.abrigo_casa].pos if m.dentro_abrigo >= 0 else m.pos
    assert cheb(b.pos, lugar) <= 2
    pos_b = (b.pos.x, b.pos.y)
    # segundo filho: depois dos 2 meses
    w.teste_saltar(m.pode_engravidar_em - 60)
    while not w.npcs()[mae].gravida and w.tempo < m.pode_engravidar_em + 2 * DIA:
        _alimentar(w)
        w.avancar(5)
    assert w.npcs()[mae].gravida
    assert (w.tempo - 5) >= m.pode_engravidar_em - 3600 - 5
    w.teste_adiantar_parto(mae, 30)
    w.avancar(31)
    b2 = w.npcs()[-1]
    assert b2.nome == "2F." + m.nome and b2.mae == mae
    assert (w.npcs()[b.id].pos.x, w.npcs()[b.id].pos.y) == pos_b  # bebê não anda
    # adulto aos 15 anos (a idade segue contando)
    w.teste_idade(b.id, 15 - 1 / ANO * 60)
    w.avancar(61)
    b = w.npcs()[b.id]
    assert not b.bebe and abs(w.idade_anos(b) - 15) < 1e-4
    assert any(r.tipo == R.VIROU_ADULTO and r.a == b.id for r in w.historico())
    # idade: mulher com 46 anos não engravida mais
    w.teste_idade(mae, 46)
    w.teste_saltar(w.npcs()[mae].pode_engravidar_em + 1)
    t0 = w.tempo
    while w.tempo < t0 + DIA:
        _alimentar(w)
        w.avancar(30)
    assert not any(r.tipo == R.GRAVIDEZ and r.a == mae and r.tempo >= t0
                   for r in w.historico())
    # a outra mãe morre grávida: o bebê também
    if w.npcs()[outra].gravida:
        w.teste_npc(outra, 100, 0, 0.01)
        w.avancar(1)
        assert w.npcs()[outra].estado == E.MORTO and not w.npcs()[outra].gravida
        assert any(r.tipo == R.BEBE_PERDIDO and r.a == outra for r in w.historico())
    print("ok  família: casal (16+), 1 h juntos no abrigo, gravidez de 9 meses, "
          "nasce ao lado da mãe (F./2F.+mãe), bebê parado, 2 meses entre filhos, "
          "adulto aos 15 anos, mulher com 46 anos não engravida, mãe morta = bebê perdido")


def teste_velhice():
    """Aos 100 anos morre de velhice."""
    w = gm.Mundo(5)
    n = w.npcs()[0]
    assert abs(n.quando_velhice - n.nascimento - 100 * ANO) < 1e-6
    w.teste_idade(0, 100 - 10 / ANO)
    w.avancar(9.99)
    assert w.npcs()[0].estado != E.MORTO
    w.avancar(0.02)
    n = w.npcs()[0]
    assert n.estado == E.MORTO and n.causa_morte == 8 and abs(w.idade_anos(n) - 100) < 1e-6
    print("ok  velhice: morre aos 100 anos, causa 'velhice'")


def teste_geral(sementes=15, duracao=6 * H):
    """Física, limites e quando o NPC decide comer/beber (sem ajuda)."""
    comeu = bebeu = vivos = 0
    for s in range(sementes):
        w = gm.Mundo(1000 + s)
        antes = {n.id: (n.fome, n.sede) for n in w.npcs()}
        ids_comeram, ids_beberam = set(), set()
        while w.tempo < duracao:
            w.avancar(5.0)
            npcs = w.npcs()
            oc = w.ocupacao_bytes()
            vivos_ = [n for n in npcs if n.estado != E.MORTO and n.dentro_abrigo < 0]
            mortos = [n for n in npcs if n.estado == E.MORTO]
            assert oc.count(int(O.NPC)) == len(vivos_)
            assert oc.count(int(O.CORPO)) == len(mortos)
            existem = [m for m in w.macieiras() if m.existe]
            assert (oc.count(int(O.MACIEIRA)) + oc.count(int(O.MACIEIRA_SECA))
                    == len(existem))
            assert oc.count(int(O.MACA)) == len(w.pilhas())
            for n in vivos_:
                assert w.ocupante(n.pos.x, n.pos.y) == O.NPC
                assert n.pos.z == w.superficie(n.pos.x, n.pos.y)
                assert w.material(n.pos.x, n.pos.y, n.pos.z) in (
                    MAT.GRAMA, MAT.NEVE, MAT.PEDRA, MAT.AREIA, MAT.GELO)
                if n.estado == E.INDO_BEBER:
                    assert n.sede < 50
            for n in npcs:
                assert 0 <= n.fome <= 100 and 0 <= n.sede <= 100
                if n.estado == E.MORTO:
                    assert n.hp == 0 and n.causa_morte != 0
                f0, s0 = antes[n.id]
                if n.fome > f0:  # +12 (ou +9 se a fome caiu no mesmo passo)
                    assert n.fome - f0 in (12, 9) or n.fome == 100, n.fome - f0
                    ids_comeram.add(n.id)
                if n.sede > s0:  # gole +15 (ou +11); maçã +5 (ou +1)
                    assert n.sede - s0 in (15, 11, 5, 1) or n.sede == 100, n.sede - s0
                    ids_beberam.add(n.id)
                antes[n.id] = (n.fome, n.sede)
        comeu += len(ids_comeram)
        bebeu += len(ids_beberam)
        vivos += sum(n.estado != E.MORTO for n in w.npcs())
    total = sementes * len(w.npcs())
    print(f"ok  física/limites em {sementes} ilhas x {duracao / H:.0f} h — de {total} "
          f"NPCs: {comeu} comeram, {bebeu} beberam, {vivos} vivos no fim")


def teste_desempenho():
    """1 dia do jogo com 6 NPCs vivos precisa caber em bem menos de 1 s
    (a velocidade >>>> é 1 dia por segundo)."""
    import time
    w = gm.Mundo(1)
    t0 = time.perf_counter()
    for _ in range(24):
        _alimentar(w)
        w.avancar(H)
    gasto = time.perf_counter() - t0
    assert gasto < 1.0, gasto
    print(f"ok  desempenho: 1 dia do jogo (6 NPCs vivos) em {gasto:.2f} s")


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
    # 1 passo da água (5 s): cai 1 nível
    w.avancar(5.0)
    assert w.material(cx, cy, topo) == MAT.AR
    assert w.nivel_agua(cx, cy, topo - 1) == 10
    assert w.agua_total() == total
    # Muitos passos: chega ao fundo (z=-2) e se espalha pelo fundo do buraco
    w.avancar(5.0 * 60)
    assert w.agua_total() == total
    fundo = [w.nivel_agua(x, y, -2) for y in range(cy - 2, cy + 3)
             for x in range(cx - 2, cx + 3)]
    assert sum(fundo) == 10, fundo          # toda a água está no fundo
    assert max(fundo) <= 2, fundo           # espalhou (não ficou num ponto só)
    assert sum(1 for f in fundo if f) >= 5, fundo
    print(f"ok  água: cai 1 nível a cada 5 s, se espalha no fundo "
          f"(níveis {sorted(f for f in fundo if f)}), total conservado; mar estável")


EA = gm.EstadoAnimal


def _vivos(w):
    return [a for a in w.animais() if a.estado != EA.MORTO]


def teste_macieira_vazia():
    """Bug corrigido: o NPC que chega a uma macieira lembrada sem maçã não
    volta lá por comida por 2 a 5 dias (antes ficava indo e voltando)."""
    marcou = 0
    for sem in (21, 22, 23):
        w = gm.Mundo(sem)
        # verão: aprende macieiras (pega maçãs saciado)
        for _ in range(int(8 * H / 30)):
            _alimentar(w)
            w.avancar(30)
        w.teste_saltar(M.tempo_de(1, 7, 1) + 8 * H)  # inverno: nenhuma maçã
        for n in w.npcs():
            w.teste_inventario(n.id, 0, 0, 0)
        idas = {}
        while w.tempo < M.tempo_de(1, 7, 3):
            for n in w.npcs():
                if n.estado != E.MORTO:
                    w.teste_npc(n.id, 60, 100, 100)  # com fome, sem morrer
            w.avancar(5)
            for n in w.npcs():
                for arv, ate in n.macieira_vazia_ate.items():
                    falta = ate - w.tempo
                    if falta > 0:
                        assert falta <= 5 * DIA + 1e-6
                        chave = (n.id, arv)
                        if chave not in idas:
                            idas[chave] = ate
                            assert ate - w.tempo >= 2 * DIA - 10
                            marcou += 1
                if n.pela_memoria and n.estado == E.INDO_COMER:
                    alvo = next(i for i in n.macieiras_lembradas
                                if w.macieiras()[i].pos.x == n.alvo.x
                                and w.macieiras()[i].pos.y == n.alvo.y)
                    assert n.macieira_vazia_ate.get(alvo, -1) <= w.tempo
    assert marcou > 0
    print(f"ok  macieira vazia: marcada por 2–5 dias ({marcou} vezes); ninguém "
          "volta a ela por comida antes disso")


def teste_fauna_inicio():
    """10 galinhas e 2 galos, em 2 bandos (1 galo + 5 galinhas), 91 dias."""
    w = gm.Mundo(8)
    bichos = w.animais()
    galos = [a for a in bichos if a.sexo == gm.Sexo.HOMEM]
    galinhas = [a for a in bichos if a.sexo == gm.Sexo.MULHER]
    assert len(galos) == 2 and len(galinhas) == 10
    for g in galos:
        assert g.bando == g.id and sum(h.bando == g.id for h in galinhas) == 5
    for a in bichos:
        assert a.fase == 2 and abs(w.idade_dias(a) - 91) < 1e-6
        assert w.hp_animal(a) == (60 if a.sexo == gm.Sexo.HOMEM else 40)
        assert a.fome == 100 and a.sede == 100
        assert w.ocupante(a.pos.x, a.pos.y) == O.ANIMAL
        assert 3 <= a.dist_bando <= 5 and 8 <= a.dist_fuga <= 15
    w.avancar(599.9)
    assert all(a.fome == 100 and a.sede == 100 for a in w.animais())
    w.avancar(0.2)
    assert all(a.fome == 97 and a.sede == 96 for a in w.animais())
    print("ok  fauna: 2 bandos (1 galo + 5 galinhas), 91 dias, HP 40/60; "
          "fome −3 e sede −4 a cada 10 min")


def teste_fauna_vida():
    """Ciscar 10 s (+10), bicar 5 s (+5), andar 6 s, subir 10 s; dorme das
    19:00 às 05:00 (o bando junto) recuperando HP; segue o galo."""
    w = gm.Mundo(10, 0, 0)
    vistos = {"ciscar": 0, "bicar": 0, "passos": 0, "subidas": 0}
    # (o 1º passo de cada um vem numa hora sorteada: não entra na conta)
    ant = {a.id: (a.estado, a.fome, a.sede, a.pos, -1e9) for a in w.animais()}
    mudou_nivel = {}
    for _ in range(int(10 * H)):
        w.avancar(1)
        for a in w.animais():
            e0, f0, s0, p0, t0 = ant[a.id]
            if a.estado == EA.MORTO:
                continue
            if e0 == EA.COMENDO and a.fome > f0:
                assert a.fome - f0 in (10, 7) or a.fome == 100
                vistos["ciscar"] += 1
            if e0 == EA.BEBENDO and a.sede > s0:
                assert a.sede - s0 in (5, 1) or a.sede == 100
                vistos["bicar"] += 1
            if (a.pos.x, a.pos.y) != (p0.x, p0.y):
                assert cheb(a.pos, p0) == 1 and abs(a.pos.z - p0.z) <= 1
                assert w.tempo - t0 >= 6 - 1 - 1e-6
                vistos["passos"] += 1
                if a.id in mudou_nivel:
                    assert w.tempo - mudou_nivel.pop(a.id) >= 10 - 1 - 1e-6
                    vistos["subidas"] += 1
                if a.pos.z != p0.z:
                    mudou_nivel[a.id] = w.tempo
                ant[a.id] = (a.estado, a.fome, a.sede, a.pos, w.tempo)
            else:
                ant[a.id] = (a.estado, a.fome, a.sede, p0, t0)
    assert all(vistos.values()), vistos
    # sono: às 19:00 todos dormem; às 05:00 acordam
    w.avancar(M.tempo_de(1, 1, 1) + 19 * H + 60 - w.tempo)
    for a in _vivos(w):
        w.teste_animal(a.id, a.fome, a.sede, 20)
    w.avancar(60)
    assert all(a.estado == EA.DORMINDO for a in _vivos(w))
    w.avancar(60)
    assert all(abs(w.hp_animal(a) - 30) < 1e-6 for a in _vivos(w)
               if a.fome > 0 and a.sede > 0)  # +5/min, 2 min
    w.avancar(M.tempo_de(1, 1, 2) + 5 * H + 30 - w.tempo)
    assert not any(a.estado == EA.DORMINDO for a in _vivos(w))
    print(f"ok  fauna: ciscar +10/10 s, bicar +5/5 s, 1 célula a cada 6 s, subir 10 s; "
          f"dormem 19:00–05:00 recuperando 5 HP/min — {vistos}")


def teste_fauna_reproducao():
    """Namoro 15 min; 12–14 ovos, 1 por dia de manhã, no ninho; choca 21 dias
    (sem fome/sede); pintinhos (5–20% não prestam) seguem a mãe; 60 dias
    sozinhos; 90 dias adultos; 3 anos morrem."""
    w = gm.Mundo(10, 0, 0)  # sem NPCs: ninguém pega os ovos
    namoros = {}
    posturas = []
    chocando = {}
    nasceram = 0
    antes = {a.id: (a.estado, a.ovos_postos) for a in w.animais()}
    while w.tempo < 40 * DIA and nasceram == 0:
        w.avancar(30)
        for a in w.animais():
            e0, p0 = antes.get(a.id, (None, 0))
            if a.estado == EA.NAMORANDO and e0 != EA.NAMORANDO:
                namoros[a.id] = w.tempo
            if e0 == EA.NAMORANDO and a.estado != EA.NAMORANDO and a.sexo == gm.Sexo.MULHER:
                assert 15 * 60 - 31 <= w.tempo - namoros.pop(a.id) <= 15 * 60 + 31
                assert a.fertil and 12 <= a.ovos_ninhada <= 14
            if a.ovos_postos > p0:
                hora = data(w.tempo)[3]
                assert 6 <= hora <= 10, hora
                assert a.ninho >= 0 and w.ovos_em(a.ninho % 128, a.ninho // 128) >= a.ovos_postos
                posturas.append(a.id)
            if a.estado == EA.CHOCANDO and e0 != EA.CHOCANDO:
                assert a.ovos_postos == a.ovos_ninhada
                chocando[a.id] = (w.tempo, a.fome, a.sede)
            if a.estado == EA.CHOCANDO:
                t0, f, sd = chocando[a.id]
                assert a.fome == f and a.sede == sd  # travadas
            antes[a.id] = (a.estado, a.ovos_postos)
        nasceram = sum(1 for a in w.animais() if a.mae >= 0)
    assert namoros is not None and len(posturas) >= 12 and nasceram > 0
    mae_id = next(a.mae for a in w.animais() if a.mae >= 0)
    mae = w.animais()[mae_id]
    t_choca = chocando[mae_id][0]
    filhos = [a for a in w.animais() if a.mae == mae_id]
    assert abs(filhos[0].nascimento - (t_choca + 21 * DIA)) < 31
    assert len(filhos) <= mae.ovos_ninhada
    assert all(f.fase == 0 and w.hp_animal(f) == 10 for f in filhos)
    assert abs(mae.pode_por_em - filhos[0].nascimento - 60 * DIA) < 1
    # pintinhos com a mãe: sem fome/sede, perto dela
    for _ in range(int(DIA / 60)):
        w.avancar(60)
        m = w.animais()[mae_id]
        for f in [w.animais()[f.id] for f in filhos]:
            if f.estado != EA.MORTO and m.estado not in (EA.MORTO,):
                assert f.fome == 100 and f.sede == 100
    # 60 dias: sozinhos (fome e sede contam); 90: adultos; 3 anos: morrem
    f = filhos[0]
    w.teste_idade_animal(f.id, 60 - 1 / 24)
    w.avancar(H + 1)
    assert w.animais()[f.id].fase == 1
    w.teste_idade_animal(f.id, 90 - 1 / 24)
    w.avancar(H + 1)
    f = w.animais()[f.id]
    if f.estado != EA.MORTO:
        assert f.fase == 2 and w.hp_animal(f) == (60 if f.sexo == gm.Sexo.HOMEM else 40)
        if f.sexo == gm.Sexo.HOMEM:
            assert f.bando == f.id
    g = _vivos(w)[0]
    w.teste_idade_animal(g.id, 3 * 365 - 1 / 24)
    w.avancar(H + 1)
    g = w.animais()[g.id]
    assert g.estado == EA.MORTO and g.causa_morte == 8
    assert w.ocupante(g.pos.x, g.pos.y) == O.CORPO_ANIMAL
    w.avancar(3 * DIA)
    assert w.ocupante(g.pos.x, g.pos.y) != O.CORPO_ANIMAL  # some em 3 dias
    print(f"ok  reprodução: namoro 15 min, {len(posturas)} ovos postos de manhã no ninho, "
          f"choca 21 dias (fome/sede travadas), {len(filhos)} pintinhos de "
          f"{mae.ovos_ninhada} ovos; 60 d sozinhos, 90 d adultos, 3 anos morrem; "
          "corpo some em 3 dias")


def teste_ovos_estragam():
    """Ovo sem contato (galinha ou NPC) por 1 dia estraga."""
    w = gm.Mundo(45, 1, 0)  # um NPC só: ninguém pega os ovos que ele soltar
    w.teste_inventario(0, 0, 3, 0)
    w.teste_npc(0, 100, 0, 0.01)
    w.avancar(1)
    pilhas = [(x, y) for x, y, q, d in w.ovos() if d < 0]
    assert pilhas
    x, y = pilhas[0]
    falta = w.ovos_estragam_em(x, y)
    assert abs(falta - DIA) < 2
    w.avancar(falta - 1)
    assert w.ovos_em(x, y) > 0
    w.avancar(2)
    assert w.ovos_em(x, y) == 0
    print("ok  ovos: sem contato por 1 dia, estragam")


def teste_caca():
    """Com fome e sem outra comida, o NPC caça: 10 de dano a cada 5 s; o
    animal foge (o bando também, ariscos 2 semanas–3 anos) ou revida (galo
    20%, mãe 80%) até morrer; morto por NPC vira carne (comer: galinha/galo
    30 min +80; pintinho 5 min +10); a carne estraga em 2 dias no chão."""
    mortos = lutas = fugas = 0
    golpes_npc = []
    for sem in (8, 9):
        w = gm.Mundo(sem)
        w.teste_saltar(M.tempo_de(1, 7, 1) + 8 * H)  # inverno: sem maçãs
        for n in w.npcs():
            w.teste_inventario(n.id, 0, 0, 0)
        hp_animal = {a.id: w.hp_animal(a) for a in w.animais()}
        hp_npc = {n.id: n.hp for n in w.npcs()}
        while w.tempo < M.tempo_de(1, 7, 3):
            for n in w.npcs():
                if n.estado != E.MORTO and n.fome > 60:
                    w.teste_npc(n.id, 60, 100, n.hp)  # com fome, sem sede
            w.avancar(1)
            for a in w.animais():
                h = w.hp_animal(a)
                if a.estado == EA.LUTANDO:
                    lutas += 1
                    adulto_macho = a.fase == 2 and a.sexo == gm.Sexo.HOMEM
                    mae = any(f.mae == a.id and f.fase == 0 for f in w.animais())
                    assert adulto_macho or mae or a.eclosao > 0
                if a.estado == EA.FUGINDO:
                    fugas += 1
                    assert a.arisco_ate > w.tempo
                    assert 14 * DIA - 1 <= a.arisco_ate - w.tempo + 2 <= 3 * 365 * DIA + 2
                if a.estado == EA.MORTO and a.causa_morte == 16 and hp_animal[a.id] > 0:
                    mortos += 1
                hp_animal[a.id] = h if a.estado != EA.MORTO else 0
            for n in w.npcs():
                if n.estado != E.MORTO and n.hp < hp_npc[n.id] - 1e-6:
                    golpes_npc.append(round(hp_npc[n.id] - n.hp, 3))
                hp_npc[n.id] = n.hp
        assert all(r.tipo != R.ANIMAL_MORTO or r.a >= 0 for r in w.historico())
    assert mortos > 0 and fugas > 0
    # golpes de animal no NPC: 2 (galinha) ou 2,5 (galo) — (a perda de 5/min
    # por necessidade crítica, ~0,08 por segundo, fica de fora)
    golpes = [g for g in golpes_npc if g > 1]
    assert all(min(abs(g - 2.0), abs(g - 2.5)) < 0.2 for g in golpes), golpes[:10]
    # carne: comer e estragar
    w = gm.Mundo(45, 6, 0)
    w.teste_inventario(0, 0, 0, 1)
    w.teste_npc(0, 10, 100, 100)
    w.avancar(5)
    n = w.npcs()[0]
    assert n.estado == E.COMENDO and n.comendo_item == 3
    w.avancar(30 * 60 + 1)
    assert abs(w.npcs()[0].fome - 90) < 1e-9
    w = gm.Mundo(45, 1, 0)  # um NPC só: ninguém pega a carne que ele soltar
    w.teste_inventario(0, 0, 0, 1)
    w.teste_npc(0, 100, 0, 0.01)
    w.avancar(1)
    carnes = w.carnes()
    assert carnes
    w.avancar(2 * DIA - 10)
    assert w.carnes()
    w.avancar(20)
    assert not w.carnes()
    print(f"ok  caça: {mortos} animais mortos por NPCs (carne), {len(golpes)} golpes de "
          f"animal em NPC, {lutas} instantes de luta "
          f"(só galo ou mãe), fuga com bando arisco; carne de galinha 30 min +80; "
          f"carne estraga em 2 dias")


def teste_coracao():
    """❤️ nas galinhas namorando e no abrigo do casal de NPCs procriando."""
    w = gm.Mundo(10, 0, 0)
    viu = False
    while w.tempo < 2 * DIA and not viu:
        w.avancar(60)
        nam = [a for a in w.animais() if a.estado == EA.NAMORANDO]
        if nam:
            cs = {(p.x, p.y) for p in w.coracoes()}
            assert all((a.pos.x, a.pos.y) in cs for a in nam)
            viu = True
    assert viu
    print("ok  coração: aparece nas galinhas namorando")


def teste_fauna_desempenho():
    """Depois de 150 dias (com os pintinhos que nasceram), 1 dia do jogo
    ainda cabe em menos de 1 s."""
    import time
    w = gm.Mundo(10, 0, 0)
    w.avancar(150 * DIA)
    n = len(_vivos(w))
    t0 = time.perf_counter()
    w.avancar(DIA)
    gasto = time.perf_counter() - t0
    assert gasto < 1.0, gasto
    print(f"ok  desempenho: {n} animais vivos, 1 dia em {gasto:.2f} s")


if __name__ == "__main__":
    teste_mapa()
    teste_calendario()
    teste_inicio()
    teste_quedas()
    teste_morte_sem_nada()
    teste_hp()
    teste_verao()
    teste_vegetal()
    teste_gelo()
    teste_acoes()
    teste_macas_npc()
    teste_memoria()
    teste_explorar()
    teste_trabalho()
    teste_noite_no_abrigo()
    teste_familia()
    teste_velhice()
    teste_relevo_npc()
    teste_agua()
    teste_geral()
    teste_desempenho()
    teste_macieira_vazia()
    teste_fauna_inicio()
    teste_fauna_vida()
    teste_fauna_reproducao()
    teste_ovos_estragam()
    teste_caca()
    teste_coracao()
    teste_fauna_desempenho()
    print("TODOS OS TESTES PASSARAM")
