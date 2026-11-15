// ---------------------------------------------------------------------------
// mapa3d.hpp — o volume do mundo: cada célula (x, y, z) é feita de um material.
//
// O mundo é uma pilha de camadas, como no Dwarf Fortress:
//   x = coluna (oeste -> leste), y = linha (norte -> sul), z = altura.
//   z vai de Z_MIN (-64) a Z_MAX (+64); z = 0 é o nível do mar.
//
// Cada célula guarda:
//   - material: ar, grama, neve, areia, terra, pedra ou água;
//   - nível de água (0 a 10). Só faz sentido quando o material é água:
//     10 = cheia (100%), 1 = 10%. Nível 0 = sem água (vira ar).
//
// "Superfície" de uma coluna (x, y) = o z da célula mais alta que não é ar.
// Em terra é a grama do topo; no mar é a água de cima. Fica guardada para
// não precisar procurar toda hora (e é recalculada quando a coluna muda).
//
// A física da água também mora aqui (passo_agua), porque ela só mexe em
// materiais. Ver docs/MUNDO_3D.md.
// ---------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace genesis {

enum class Material : std::uint8_t {
    Ar = 0,
    Grama = 1,
    Terra = 2,
    Pedra = 3,
    Agua = 4,
    Neve = 5,   // topo dos morros mais altos (+4)
    Areia = 6,  // chão do nível 0 encostado no mar
    Gelo = 7,   // água de vale congelada (inverno): anda por cima, não bebe
};

class Mapa3D {
public:
    Mapa3D();

    // --- Tamanho ---------------------------------------------------------
    int largura() const;
    int altura() const;
    int z_min() const;
    int z_max() const;
    bool dentro(int x, int y, int z) const;

    // --- Leitura e escrita de células --------------------------------------
    Material material(int x, int y, int z) const { return mat_[i3(x, y, z)]; }
    int nivel_agua(int x, int y, int z) const { return agua_[i3(x, y, z)]; }
    void definir(int x, int y, int z, Material m, int nivel_agua = 0);

    // Superfície da coluna (ver explicação no topo).
    int superficie(int x, int y) const { return sup_[y * largura() + x]; }
    // A coluna é terra firme (o topo é sólido, não água nem gelo)?
    bool e_terra(int x, int y) const;
    // O topo da coluna é gelo?
    bool e_gelo(int x, int y) const;
    // Gelo (pedido em 15/11/2026): a camada de cima da água das colunas
    // dentro do retângulo [x0, x1) x [y0, y1) congela. O nível da água fica
    // guardado no gelo; ao derreter, volta a ser a mesma água. Devolve as
    // colunas que congelaram / derreteram.
    std::vector<int> congelar(int x0, int y0, int x1, int y1);
    std::vector<int> degelar();
    void recalcular_superficie(int x, int y);
    void recalcular_superficies();

    // --- Água ---------------------------------------------------------------
    // Um "passo" da física da água (chamado a cada AGUA_INTERVALO_S):
    //   1) cair: de baixo para cima, cada célula de água passa para a de baixo
    //      tudo o que couber nela (se a de baixo é ar ou água não cheia);
    //   2) espalhar: o que sobrou se espalha para os 4 vizinhos do mesmo nível
    //      (norte, sul, leste, oeste) que tenham pelo menos 2 níveis a menos,
    //      1 nível por vizinho. A ordem é sorteada para não puxar a água
    //      sempre para o mesmo lado.
    // A água nunca é criada nem destruída: só muda de lugar.
    void passo_agua(std::mt19937_64& rng);
    long long agua_total() const;  // soma de todos os níveis (para testes)
    // A água está parada (o próximo passo não moveria nada)?
    bool agua_parada() const { return agua_parada_; }

    // --- Visão de uma camada (para a interface) ----------------------------
    // Para cada coluna, olhando de cima a partir do nível z para baixo:
    // o material da primeira célula que não é ar, e quantos níveis abaixo
    // de z ela está (0 = está no próprio nível z).
    struct Visto { Material material; int profundidade; int z; };
    Visto ver(int x, int y, int z) const;

private:
    int i3(int x, int y, int z) const {
        return ((z - z_min()) * altura() + y) * largura() + x;
    }
    bool aceita_agua(int x, int y, int z) const;  // ar ou água não cheia

    std::vector<Material> mat_;
    std::vector<std::uint8_t> agua_;
    std::vector<int> sup_;
    // A água está parada? Se no último passo nada se moveu e ninguém mexeu
    // no mapa depois, o próximo passo também não moveria nada: pula.
    // Só economiza tempo; não muda o resultado.
    bool agua_parada_ = false;
};

}  // namespace genesis
