// ---------------------------------------------------------------------------
// gerador.cpp — implementação do gerador de mundos (ver gerador.hpp).
// ---------------------------------------------------------------------------
#include "gerador.hpp"

#include <algorithm>
#include <array>
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

    // Divide a terra pela altura do ruído: os 30% mais baixos viram vale,
    // os 30% mais altos viram morro, o meio vira planície. Usar a ordem dos
    // valores (e não o valor em si) garante as porcentagens exatas.
    std::vector<double> valores;
    for (int y = borda; y < H - borda; ++y)
        for (int x = borda; x < L - borda; ++x) valores.push_back(bruto[y * L + x]);
    std::sort(valores.begin(), valores.end());
    const size_t n = valores.size();
    const double limite_vale = valores[static_cast<size_t>(n * cfg::FRACAO_VALE)];
    const double limite_morro =
        valores[std::min(n - 1, static_cast<size_t>(n * (1.0 - cfg::FRACAO_MORRO)))];

    // Dentro do vale, quanto mais baixo o ruído, mais fundo (até o -4).
    // Dentro do morro, quanto mais alto o ruído, mais alto (até o +4).
    auto altura_em = [&](int x, int y) {
        const double v = bruto[y * L + x];
        if (v < limite_vale) {
            const double t = (limite_vale - v) / (limite_vale - menor);  // 0..1
            const int degraus = cfg::PLANICIE_Z - cfg::VALE_MIN_Z;  // 1 até 5
            const int fundo = cfg::PLANICIE_Z - 1 -
                              static_cast<int>(t * (degraus - 1) + 0.5);
            return std::max(fundo, cfg::VALE_MIN_Z);
        }
        if (v >= limite_morro) {
            const double t = (maior > limite_morro)
                                 ? (v - limite_morro) / (maior - limite_morro) : 0.0;
            const int degraus = cfg::SUPERFICIE_MAX - cfg::PLANICIE_Z;  // 1 até 3
            const int topo = cfg::PLANICIE_Z + 1 +
                             static_cast<int>(t * (degraus - 1) + 0.5);
            return std::min(topo, cfg::SUPERFICIE_MAX);
        }
        return cfg::PLANICIE_Z;
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
                    if (z == topo) {
                        // topo: neve a partir de +4; pedra exposta abaixo do
                        // -3 (fundo dos vales mais fundos); senão, grama.
                        if (topo >= cfg::NEVE_A_PARTIR_Z) m = Material::Neve;
                        else if (topo < cfg::TERRA_ATE_Z) m = Material::Pedra;
                        else m = Material::Grama;
                    } else if (z < topo && z >= cfg::TERRA_ATE_Z) {
                        m = Material::Terra;
                    } else if (z < topo) {
                        m = Material::Pedra;
                    }
                }
                mapa.definir(x, y, z, m, nivel);
            }
        }
    mapa.recalcular_superficies();

    // --- 3) Vales ligados ao mar enchem de água -----------------------------
    // Pedido do dono: o vale enche de água só se estiver ligado ao mar.
    // A água sai do mar e ocupa todo ar que ela alcança indo para os lados
    // (norte, sul, leste, oeste) ou para baixo, do nível do mar para baixo.
    // Feito já na criação, o mundo nasce em equilíbrio (a física não precisa
    // "escoar" o mar para dentro dos vales).
    std::vector<std::array<int, 3>> fila;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < L; ++x)
            for (int z = cfg::MAR_FUNDO_Z; z <= cfg::NIVEL_DO_MAR; ++z)
                if (mapa.material(x, y, z) == Material::Agua) fila.push_back({x, y, z});
    const int passos[5][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, -1}};
    while (!fila.empty()) {
        const auto [x, y, z] = fila.back();
        fila.pop_back();
        for (const auto& d : passos) {
            const int nx = x + d[0], ny = y + d[1], nz = z + d[2];
            if (!mapa.dentro(nx, ny, nz) || nz > cfg::NIVEL_DO_MAR) continue;
            if (mapa.material(nx, ny, nz) != Material::Ar) continue;
            mapa.definir(nx, ny, nz, Material::Agua, cfg::AGUA_NIVEL_MAX);
            fila.push_back({nx, ny, nz});
        }
    }
    mapa.recalcular_superficies();

    // --- 4) Areia ------------------------------------------------------------
    // Pedido do dono: o chão cujo topo está no nível do mar (0) e encosta no
    // mar vira areia. "Encostar" = alguma das 8 colunas vizinhas tem água no
    // topo, no nível 0 (o mar, ou um vale ligado a ele) — PROVISÓRIO: 8
    // vizinhas (as diagonais contam).
    std::vector<std::array<int, 2>> areia;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < L; ++x) {
            if (mapa.superficie(x, y) != cfg::NIVEL_DO_MAR) continue;
            if (mapa.material(x, y, cfg::NIVEL_DO_MAR) == Material::Agua) continue;
            bool encosta = false;
            for (int dy = -1; dy <= 1 && !encosta; ++dy)
                for (int dx = -1; dx <= 1 && !encosta; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if ((dx || dy) && nx >= 0 && ny >= 0 && nx < L && ny < H &&
                        mapa.material(nx, ny, cfg::NIVEL_DO_MAR) == Material::Agua &&
                        mapa.superficie(nx, ny) == cfg::NIVEL_DO_MAR)
                        encosta = true;
                }
            if (encosta) areia.push_back({x, y});
        }
    for (const auto& [x, y] : areia)
        mapa.definir(x, y, cfg::NIVEL_DO_MAR, Material::Areia);
}

}  // namespace genesis
