// ---------------------------------------------------------------------------
// mapa3d.cpp — implementação do volume do mundo e da física da água.
// ---------------------------------------------------------------------------
#include "mapa3d.hpp"

#include <algorithm>
#include <array>

#include "config.hpp"

namespace genesis {

namespace cfg = config;

Mapa3D::Mapa3D() {
    const size_t total = static_cast<size_t>(largura()) * altura() *
                         (z_max() - z_min() + 1);
    mat_.assign(total, Material::Ar);
    agua_.assign(total, 0);
    sup_.assign(static_cast<size_t>(largura()) * altura(), z_min());
}

int Mapa3D::largura() const { return cfg::LARGURA; }
int Mapa3D::altura() const { return cfg::ALTURA; }
int Mapa3D::z_min() const { return cfg::Z_MIN; }
int Mapa3D::z_max() const { return cfg::Z_MAX; }

bool Mapa3D::dentro(int x, int y, int z) const {
    return x >= 0 && y >= 0 && x < largura() && y < altura() &&
           z >= z_min() && z <= z_max();
}

void Mapa3D::definir(int x, int y, int z, Material m, int nivel_agua) {
    const int i = i3(x, y, z);
    mat_[i] = m;
    agua_[i] = static_cast<std::uint8_t>(m == Material::Agua ? nivel_agua : 0);
    agua_parada_ = false;  // o mapa mudou: a água pode voltar a se mover
}

bool Mapa3D::e_terra(int x, int y) const {
    const int z = superficie(x, y);
    const Material m = material(x, y, z);
    return m == Material::Grama || m == Material::Terra || m == Material::Pedra;
}

void Mapa3D::recalcular_superficie(int x, int y) {
    int z = z_max();
    while (z > z_min() && material(x, y, z) == Material::Ar) --z;
    sup_[y * largura() + x] = z;
}

void Mapa3D::recalcular_superficies() {
    for (int y = 0; y < altura(); ++y)
        for (int x = 0; x < largura(); ++x) recalcular_superficie(x, y);
}

Mapa3D::Visto Mapa3D::ver(int x, int y, int z) const {
    int zz = z;
    while (zz > z_min() && material(x, y, zz) == Material::Ar) --zz;
    return {material(x, y, zz), z - zz, zz};
}

// ===========================================================================
// Física da água
// ===========================================================================

bool Mapa3D::aceita_agua(int x, int y, int z) const {
    if (!dentro(x, y, z)) return false;  // fora do mapa = parede
    const Material m = material(x, y, z);
    if (m == Material::Ar) return true;
    return m == Material::Agua && nivel_agua(x, y, z) < cfg::AGUA_NIVEL_MAX;
}

void Mapa3D::passo_agua(std::mt19937_64& rng) {
    if (agua_parada_) return;
    bool moveu = false;
    const int L = largura(), H = altura();
    // Colunas que mudaram (para recalcular a superfície no fim).
    std::vector<std::uint8_t> coluna_mudou(static_cast<size_t>(L) * H, 0);
    // Células que RECEBERAM água neste passo: não espalham de novo agora,
    // senão a água atravessaria várias células num passo só.
    std::vector<std::uint8_t> recebeu(mat_.size(), 0);

    auto por_nivel = [&](int x, int y, int z, int nivel) {
        const int i = i3(x, y, z);
        agua_[i] = static_cast<std::uint8_t>(nivel);
        mat_[i] = nivel > 0 ? Material::Agua : Material::Ar;
        coluna_mudou[y * L + x] = 1;
        moveu = true;
    };

    // --- 1) Cair: de baixo para cima ---------------------------------------
    // Indo de baixo para cima, a água que cai numa célula já processada
    // não cai de novo no mesmo passo (cai 1 nível por passo).
    for (int z = z_min() + 1; z <= z_max(); ++z)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < L; ++x) {
                if (material(x, y, z) != Material::Agua) continue;
                if (!aceita_agua(x, y, z - 1)) continue;
                const int tem = nivel_agua(x, y, z);
                const int cabe = cfg::AGUA_NIVEL_MAX - nivel_agua(x, y, z - 1);
                const int desce = std::min(tem, cabe);
                por_nivel(x, y, z - 1, nivel_agua(x, y, z - 1) + desce);
                por_nivel(x, y, z, tem - desce);
                recebeu[i3(x, y, z - 1)] = 1;
            }

    // --- 2) Espalhar para os lados ------------------------------------------
    // Junta as células com água que podem espalhar e embaralha a ordem.
    std::vector<std::array<int, 3>> fontes;
    for (int z = z_min(); z <= z_max(); ++z)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < L; ++x)
                if (material(x, y, z) == Material::Agua &&
                    nivel_agua(x, y, z) >= 2 && !recebeu[i3(x, y, z)])
                    fontes.push_back({x, y, z});
    std::shuffle(fontes.begin(), fontes.end(), rng);

    std::array<std::array<int, 2>, 4> lados = {{{0, -1}, {0, 1}, {1, 0}, {-1, 0}}};
    for (const auto& f : fontes) {
        const int x = f[0], y = f[1], z = f[2];
        std::shuffle(lados.begin(), lados.end(), rng);
        for (const auto& d : lados) {
            const int nx = x + d[0], ny = y + d[1];
            if (!aceita_agua(nx, ny, z)) continue;
            const int aqui = nivel_agua(x, y, z);
            const int la = nivel_agua(nx, ny, z);
            if (aqui - la < 2) continue;  // diferença pequena: fica parada
            por_nivel(x, y, z, aqui - 1);
            por_nivel(nx, ny, z, la + 1);
            recebeu[i3(nx, ny, z)] = 1;
        }
    }

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < L; ++x)
            if (coluna_mudou[y * L + x]) recalcular_superficie(x, y);
    agua_parada_ = !moveu;
}

long long Mapa3D::agua_total() const {
    long long soma = 0;
    for (std::uint8_t n : agua_) soma += n;
    return soma;
}

}  // namespace genesis
