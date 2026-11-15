# Nomes e primeiro comportamento: carregar maçã (motor 0.5)

> **Mudou em 15/11/2026 (motor 0.12):** o NPC carrega **até 16 maçãs**; saciado, pega todas as da pilha que couberem e **leva para o abrigo** dele (até 300 lá; no abrigo e na mão não estragam); abrigo cheio: deixa as da mão no chão e não pega mais. Com fome come **da mão → de um abrigo → do chão**. Ver [TEMPO.md](TEMPO.md).

Pedido pelo dono do projeto em 14/11/2026.

## Regras definidas pelo dono do projeto

- **Nomes** (por enquanto): **Jhon, Elton, Rose, Sycilia** — na ordem dos
  NPCs 1 a 4. Ficam em `NOMES_NPC` no `config.hpp`.
- **Carregar maçã**: quando o NPC **vê uma maçã**, mesmo **com a fome
  saciada**, ele **pega e leva com ele**.
- Carrega **no máximo 1 maçã**.
- **Pegar é na hora**: ao chegar ao lado da maçã, ela sai do chão e vai
  para a mão.
- **Com fome** (abaixo de 70), ele **come a maçã que está carregando**
  (10 s, +12 de fome), sem precisar procurar.
- **Memória**: lembra a macieira **ao pegar** a maçã (além de ao comer uma
  maçã do chão, como antes).
- **Morreu carregando**: a maçã **cai numa célula livre ao lado** do corpo.
- O painel da direita (e a tabela) mostram **o que está carregando**.

## Como o código implementa

| Regra | Onde | Como |
|---|---|---|
| Nomes | `config.hpp` (`NOMES_NPC`), `Mundo::Mundo` | Cada NPC recebe o nome da lista na ordem em que é criado. |
| Ordem de decisão | `Mundo::evento_passo` | 1) Fome/sede baixas (como antes). 2) Se não há nada a resolver: **tentar pegar** uma maçã. 3) Senão, anda à toa. |
| Pegar | `Mundo::tentar_pegar` | Só com a mão livre (`macas_carregadas < 1`). Vê a maçã mais próxima (área 32 × 32, sem contar as que outro está comendo). Ao lado dela: tira do chão, põe na mão, lembra a macieira de origem. Longe: passo em direção (estado "indo pegar maçã"). |
| Comer a da mão | `Mundo::tentar_comer` | Se está carregando, começa a comer a da mão na hora (`comendo_da_mao`). No fim (`evento_fim_comer`): a mão esvazia, fome +12. |
| Soltar ao morrer | `Mundo::morrer`, `Mundo::soltar_maca_perto` | A maçã volta ao chão na célula livre mais perto do corpo (as 8 vizinhas; se lotadas, a próxima camada), guardando de qual macieira ela veio. |

## Na tela

- **Painel da direita** do NPC: nome, fome, sede, estado,
  **"Carregando: 1 maçã"** ou **"Carregando: nada"**, posição e memória.
- **Aba NPCs**: coluna **Nome** e coluna **Carregando**.
- **Barra de baixo**: o nome do NPC sob o mouse (ou "Corpo de …").
- Estados novos: **"indo pegar maçã"** e **"comendo a maçã que carregava"**.

## Testes (`testes/teste_ilha.py`, `teste_carregar`)

Em 60 ilhas × 30 minutos: nomes certos; nunca carrega mais de 1; só pega
com a mão livre e já lembra a macieira; comendo da mão só se carrega uma;
morrendo carregando, a maçã aparece no chão ao lado do corpo.
