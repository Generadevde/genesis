// ---------------------------------------------------------------------------
// gerador.cpp — implementação do gerador de mundos (ver gerador.hpp).
// ---------------------------------------------------------------------------
#include "gerador.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "config.hpp"

namespace genesis {

namespace cfg = config;

namespace {

// Uma camada de ruído suave: valores aleatórios (0 a 1) nos cantos de uma
// grade com espaçamento `passo`; entre eles, mistura suave.
class RuidoSuave {
public:
    RuidoSuave(std::mt19937_64& rng, int largura, int altura, double passo)
        : passo_(passo) {
        nx_ = static_cast<int>(std::ceil(largura / passo)) + 2;
        ny_ = static_cast<int>(std::ceil(altura / passo)) + 2;
        std::uniform_real_distribution<double> d(0.0, 1.0);
        valores_.resize(static_cast<size_t>(nx_) * ny_);
        for (double& v : valores_) v = d(rng);
    }

    double em(double x, double y) const {
        const double gx = x / passo_, gy = y / passo_;
        const int x0 = static_cast<int>(gx), y0 = static_cast<int>(gy);
        const double tx = suave(gx - x0), ty = suave(gy - y0);
        const double a = v(x0, y0), b = v(x0 + 1, y0);
        const double c = v(x0, y0 + 1), e = v(x0 + 1, y0 + 1);
        const double cima = a + (b - a) * tx, baixo = c + (e - c) * tx;
        return cima + (baixo - cima) * ty;
    }

private:
    // Curva em "S": a mistura começa e termina devagar (sem quinas).
    static double suave(double t) { return t * t * (3.0 - 2.0 * t); }
    double v(int i, int j) const { return valores_[j * nx_ + i]; }

    double passo_;
    int nx_ = 0, ny_ = 0;
    std::vector<double> valores_;
};

}  // namespace

void gerar_mundo(Mapa3D& mapa, std::uint64_t semente) {
    std::mt19937_64 rng(semente);
    const int L = mapa.largura(), H = mapa.altura();
    const int borda = cfg::BORDA_AGUA;

    // --- 1) Mapa de alturas (só na terra) -----------------------------------
    // Soma das camadas: cada uma com metade do tamanho e metade da força.
    std::vector<RuidoSuave> camadas;
    double tamanho = cfg::RELEVO_TAMANHO_MORRO;
    for (int c = 0; c < cfg::RELEVO_CAMADAS; ++c) {
        camadas.emplace_back(rng, L, H, tamanho);
        tamanho /= 2.0;
    }
    std::vector<double> bruto(static_cast<size_t>(L) * H, 0.0);
    double menor = 1e9, maior = -1e9;
    for (int y = borda; y < H - borda; ++y)
        for (int x = borda; x < L - borda; ++x) {
            double soma = 0.0, forca = 1.0;
            for (const RuidoSuave& r : camadas) {
                soma += forca * r.em(x, y);
                forca /= 2.0;
            }
            bruto[y * L + x] = soma;
            menor = std::min(menor, soma);
            maior = std::max(maior, soma);
        }

    // Estica para caber exatamente entre SUPERFICIE_MIN e SUPERFICIE_MAX.
    const int faixa = cfg::SUPERFICIE_MAX - cfg::SUPERFICIE_MIN + 1;
    auto altura_em = [&](int x, int y) {
        const double t = (maior > menor) ? (bruto[y * L + x] - menor) / (maior - menor)
                                         : 0.0;
        const int h = cfg::SUPERFICIE_MIN + static_cast<int>(t * faixa);
        return std::min(h, cfg::SUPERFICIE_MAX);
    };

    // --- 2) Monta cada coluna -----------------------------------------------
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < L; ++x) {
            const bool mar = x < borda || y < borda || x >= L - borda ||
                             y >= H - borda;
            for (int z = mapa.z_min(); z <= mapa.z_max(); ++z) {
                Material m = Material::Ar;
                int nivel = 0;
                if (mar) {
                    if (z <= cfg::NIVEL_DO_MAR && z >= cfg::MAR_FUNDO_Z) {
                        m = Material::Agua;
                        nivel = cfg::AGUA_NIVEL_MAX;
                    } else if (z < cfg::MAR_FUNDO_Z) {
                        m = Material::Pedra;
                    }
                } else {
                    const int topo = altura_em(x, y);
                    if (z == topo) m = Material::Grama;
                    else if (z < topo && z >= cfg::TERRA_ATE_Z) m = Material::Terra;
                    else if (z < cfg::TERRA_ATE_Z) m = Material::Pedra;
                }
                mapa.definir(x, y, z, m, nivel);
            }
        }
    mapa.recalcular_superficies();
}

}  // namespace genesis
