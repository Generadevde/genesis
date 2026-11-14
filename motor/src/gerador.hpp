// ---------------------------------------------------------------------------
// gerador.hpp — o gerador de mundos: monta as camadas do Mapa3D.
//
// Regras do dono do projeto (14/11/2026), números em config.hpp:
//   - Borda de BORDA_AGUA células = mar: água do nível 0 até o -10
//     (cheia, nível 10); abaixo, pedra; acima, ar.
//   - Terra (o meio): a superfície fica entre +1 e +4 (relevo "médio").
//     Topo = grama; abaixo dela, terra até o -3; dali para baixo, pedra.
//
// O relevo vem de "ruído suave": valores sorteados numa grade espaçada e
// interpolados com suavidade entre os pontos (value noise), somando
// algumas camadas de detalhe. Depois é esticado para caber em +1..+4.
// ---------------------------------------------------------------------------
#pragma once

#include <cstdint>

#include "mapa3d.hpp"

namespace genesis {

// Preenche `mapa` com um mundo novo a partir da `semente`.
void gerar_mundo(Mapa3D& mapa, std::uint64_t semente);

}  // namespace genesis
