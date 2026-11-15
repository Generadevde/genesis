# Explorar (motor 0.9)

> **Mudou em 15/11/2026:** o NPC agora anda 1 célula a cada **5 s** (subir/descer: 10 s). Ver [TEMPO.md](TEMPO.md).

Pedido pelo dono do projeto em 14/11/2026. Substitui o "andar à toa"
(e a regra antiga de "não voltar para a célula de onde saiu").

## Regra
- Quando não tem nada a resolver (fome, sede, pegar maçã...), o NPC
  **explora**: escolhe um **destino a uma distância de 2 a 20 células**
  (`EXPLORAR_DIST_MIN` / `EXPLORAR_DIST_MAX` no `config.hpp`) e anda até lá.
- Chegando, escolhe outro.
- Se nenhum passo o aproxima do destino (barranco de mais de 1 nível,
  água, alguém no caminho), larga o destino e escolhe outro no passo
  seguinte.
- Se ele sai para comer, beber ou pegar maçã, o destino fica guardado e
  ele continua depois.
- Na tela, o estado aparece como **"explorando"**.

## Código
`Mundo::explorar` e `Mundo::sortear_destino` (`mundo.cpp`): a distância é
sorteada de 2 a 20; entre as células de terra exatamente a essa distância
(a "camada" em volta dele), uma é sorteada como destino.

## Teste
`teste_explorar`: todo destino novo está a 2–20 células; todo passo dado
explorando aproxima do destino.
