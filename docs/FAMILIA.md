# Família: sexos, casais, filhos, idade e histórico (motor 0.11)

> **Mudou em 15/11/2026 (motor 0.12):** idades pelo calendário (ano de 365 dias): velhice aos **100 anos**; ter filhos dos **16 aos 45 (mulher) / 60 (homem)**; gravidez **9 meses**; **2 meses** entre filhos; bebê vira adulto aos **15 anos** (sem pular para 20). O painel mostra a **data de nascimento**. Os tempos antigos abaixo (3 dias, 7 dias, 10 dias, 60 anos, 1 ano = 14 dias) **não valem mais** — ver [TEMPO.md](TEMPO.md).

Pedido pelo dono do projeto em 14/11/2026. Todas as perguntas foram
respondidas por ele antes do código.

## Regras definidas pelo dono do projeto

### Começo e janela "Nova simulação"
- **Arquivo → Nova simulação** abre uma janela com **Homens** e **Mulheres**
  (só números, **sem máximo**). Ao abrir o programa: **3 homens e 3 mulheres**.
- Nomes **NPC 1 … NPC N**; o **sexo de cada um é sorteado** (respeitando as
  quantidades pedidas).
- Símbolos: homem **🧍**, mulher **🧍🏼‍♀️**, bebê **👶** (metade do tamanho).

### Idade
- Todos começam com **20 anos**. **1 ano = 14 dias do jogo.**
- Aos **60 anos** morre de velhice, na hora certa. O corpo ✝ fica; a causa
  aparece como **"velhice"**.

### Casal
- Um **homem adulto** e uma **mulher adulta**, os dois **sem par**, que se
  **veem** (mesma área 32 × 32) **formam um casal**.
- Se um morre, o outro **pode formar outro casal**.
- O casal **divide o abrigo e dorme junto** — pode ser o dele ou o dela.
  Se nenhum tem abrigo, **tentam construir**. Se o abrigo está **cheio,
  esperam** (cada um no seu até vagar).

### Procriar
- **Primárias antes** (fome, sede, abrigo); procriar é **secundário**: só
  quando **não precisam comer nem beber**.
- O casal **vai para o abrigo a qualquer hora**; à noite, dormindo juntos,
  também pode acontecer. Cerca de **1 hora juntos** no abrigo e **sempre dá
  certo**.

### Gravidez e nascimento
- Dura **3 dias**. Se a **mãe morre, o bebê morre junto**.
- Nasce **ao lado da mãe**, **um por vez**, **50/50** menino ou menina. Sem
  lugar ao lado, vai para o **próximo anel**.
- Depois de um parto, **7 dias** até poder engravidar de novo.
- **Sem limite** de população.

### Bebê
- **Sem necessidades** (por enquanto), **fica parado**, **ocupa a célula**.
- Com **10 dias** vira adulto: **pula para 20 anos** e troca para o símbolo
  certo (🧍 ou 🧍🏼‍♀️).
- Nome pela **mãe**: 1º filho **F.** + nome da mãe (ex.: `F.NPC 2`);
  do 2º em diante **2F.**, **3F.**, … (ex.: `2F.NPC 2`).

### Tela
- Painel do NPC: **sexo, idade, mãe, pai** (e par, filhos, gravidez).
- Tabela NPCs: colunas **Sexo**, **Idade** e **Par**.
- Aba nova **Histórico**: só os **acontecimentos importantes** (nascimento,
  morte, casal, gravidez, virou adulto, abrigo, machado, árvore cortada,
  plantas, pedra esgotada). **Comer e beber ficam de fora.** Mais novo em
  cima, com dia e hora; **rodinha rola**; **filtros por tipo**.

## Como o código implementa

| Parte | Onde | Como |
|---|---|---|
| Números | `config.hpp` | `IDADE_INICIAL_ANOS`, `IDADE_MAXIMA_ANOS`, `DIAS_POR_ANO`, `PROCRIAR_H`, `GRAVIDEZ_DIAS`, `INTERVALO_FILHOS_DIAS`, `BEBE_VIRA_ADULTO_DIAS`, `IDADE_AO_VIRAR_ADULTO`, `HOMENS_PADRAO`, `MULHERES_PADRAO`. |
| Começo | `Mundo(semente, homens, mulheres)` | Embaralha a lista de sexos e dá nomes NPC 1..N. Só coloca quem couber no mapa. |
| Idade | `Npc::idade_ref_anos`, `idade_ref_t`, `Mundo::idade_anos` | Idade = idade de referência + tempo passado ÷ `SEGUNDOS_POR_ANO`. A morte por velhice é um **evento na fila** (`VELHICE`), marcado para o instante exato dos 60 anos. |
| Casal | `Mundo::formar_casal` | No passo do NPC: adulto sem par vê alguém do outro sexo, adulto e sem par → casal (registrado no Histórico). |
| Abrigo do casal | `Mundo::unir_abrigo_casal` | Se só um tem abrigo, o outro muda para ele. Se os dois têm, fica o **mais perto dos dois** e o outro muda. Cheio → não muda (esperam); é tentado de novo a cada passo. |
| Querer procriar | `Mundo::quer_procriar`, `tentar_procriar` | Casal, mulher não grávida e já liberada (7 dias), os dois sem fome e sem sede → vão para o abrigo (estado `INDO_PROCRIAR`). |
| Juntos no abrigo | `Mundo::ver_se_comeca_procriar`, evento `CONCEPCAO` | Quando os dois estão dentro, marca o começo; depois de 1 h juntos, ela engravida (`gravida`, `quando_nascer`). Se um sai antes, cancela. |
| Nascimento | evento `NASCIMENTO`, `lugar_livre_perto` | Sorteia o sexo, procura célula livre em anéis em volta da mãe (ou do abrigo, se ela está dentro), cria o bebê com o nome `F.`/`2F.`/… e marca a mãe para só poder de novo em 7 dias. |
| Bebê | `Npc::bebe`, evento `VIRA_ADULTO` | Bebê não tem passo nem fome/sede/HP caindo. Aos 10 dias vira adulto com 20 anos. |
| Morte | `Mundo::morrer` | Guarda a causa (bits: 1 fome, 2 sede, 4 frio, 8 velhice); se estava grávida, o bebê é perdido; desfaz o casal; sai do abrigo; registra no Histórico. |
| Histórico | `Mundo::registrar`, `historico(desde)` | Lista de `Registro{tempo, tipo, a, b, c, pos}`. A tela só busca o que é novo. |
| Janela | `interface/principal.py`: `abrir_dialogo_nova`, `evento_dialogo`, `confirmar_dialogo` | Clique no campo ou Tab troca o campo; só aceita dígitos; Enter = OK, Esc = Cancelar. |
| Aba Histórico | `atualizar_historico`, `desenhar_historico`, `texto_registro` | Categorias **Pessoas**, **Trabalho**, **Plantas** (botões de filtro); tabela com rolagem pela rodinha. |

## Decisões minhas (PROVISÓRIAS, aguardando o dono)
- Quando os dois do casal têm abrigo, fica o **mais perto dos dois**.
- Se **não há nenhuma célula livre** em volta, o bebê **não nasce**
  (registrado como bebê perdido).
- A janela Nova simulação coloca **só quantos NPCs couberem** no mapa.
- Filtros do Histórico: **todos ligados** ao abrir.
- A rodinha rola **3 linhas** (`LINHAS_POR_RODINHA`); largura do campo
  numérico 120 px.
- A simulação **continua rodando** com a janela Nova simulação aberta.
- Esperar o par dentro do abrigo **não tem limite de tempo**.
- Casal com abrigos diferentes e **os dois cheios** fica separado até vagar —
  e, enquanto isso, **não procria** (precisa estar no mesmo abrigo).

## Testes
`testes/teste_ilha.py`:
- `teste_inicio` — 3 homens e 3 mulheres, NPC 1..6, 20 anos; outras quantidades.
- `teste_familia` — em 3 ilhas, 32 dias: casal sempre homem+mulher e no mesmo
  abrigo (ou os dois cheios); só engravida com os dois juntos no abrigo; nasce
  3 dias depois ao lado da mãe; nome F./2F. + mãe; bebê não anda; adulto aos
  10 dias com 20 anos; 7 dias entre filhos.
- `teste_velhice` — morre exatamente aos 60 anos, causa "velhice".

`testes/teste_interface.py` — digita 4 homens e 2 mulheres na janela, Esc
cancela, aba Histórico filtra e rola.
