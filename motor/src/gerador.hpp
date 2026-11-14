// ---------------------------------------------------------------------------
// gerador.hpp — o gerador de mundos: monta as camadas do Mapa3D.
//
// Regras do dono do projeto (14/11/2026), números em config.hpp:
//   - Borda de BORDA_AGUA células = mar: água do nível 0 até o -10
//     (cheia, nível 10); abaixo, pedra; acima, ar.
//   - Terra (o meio): 40% planície (+1), 30% morro (até +4), 30% vale
//     (até -4). Topo = grama (neve se está em +4; pedra exposta se está
//     abaixo do -3); abaixo do topo, terra até o -3; dali para baixo, pedra.
//   - Vales ligados ao mar enchem de água até o nível do mar (0).
//   - Chão no nível do mar (0) encostado na água do mar vira areia.
//
// O relevo vem de "ruído suave": valores sorteados numa grade espaçada e
// interpolados com suavidade entre os pontos (value noise), somando
// algumas camadas de detalhe. Depois a terra é dividida pela altura do
// ruído em vale / planície / morro nas porcentagens pedidas.
// ---------------------------------------------------------------------------
#pragma once

#include <cstdint>

#include "mapa3d.hpp"

namespace genesis {

// Preenche `mapa` com um mundo novo a partir da `semente`.
void gerar_mundo(Mapa3D& mapa, std::uint64_t semente);

}  // namespace genesis
