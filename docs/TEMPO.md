# Sistema de tempo (motor 0.6)

Pedido pelo dono do projeto em 14/11/2026, antes da vida vegetal.

## Regras

- **1 hora do jogo = 60 s** da simulação (valor "por enquanto").
  Logo, **1 minuto do jogo = 1 s** e **1 dia = 24 horas = 24 minutos reais**
  (em velocidade 1x).
- A **barra de baixo** mostra **dia e hora**: `Dia 1, 00:00`.
- O jogo começa no **Dia 1, às 00:00** (`HORA_INICIAL`, PROVISÓRIO).
- Os botões de velocidade (`>` 2x, `>>` 4x, `>>>>` 8x) aceleram o relógio
  do jogo junto com todo o resto.

## Como o código implementa

| Parte | Onde | Como |
|---|---|---|
| Números | `config.hpp` | `SEGUNDOS_POR_HORA = 60`, `HORAS_POR_DIA = 24`, `SEGUNDOS_POR_DIA`, `HORA_INICIAL`. |
| Relógio | `Mundo::relogio()` | Converte os segundos da simulação (`tempo`) em minutos do jogo e daí em dia (a partir de 1), hora e minuto. |
| Tela | `principal.py` (`texto_relogio`) | Barra de baixo e aba Info. |

Durações do jogo em horas e dias (como as da vida vegetal) são guardadas
em horas/dias no `config.hpp` e convertidas para segundos com essas
constantes — assim, mudar a escala do tempo muda tudo junto.

## Teste

`teste_relogio` (em `testes/teste_ilha.py`): 0 s → Dia 1 00:00; 60 s →
01:00; 24 min → Dia 2 00:00; e outros casos.
