# Estrutura do projeto e como rodar

## Pastas e arquivos

```
genesis/
├── genesis              ← o executável: abre direto na simulação, sem menus
├── instalar.sh          ← prepara tudo (ambiente Python + compila o C++)
├── CMakeLists.txt       ← receita de compilação do C++ (CMake)
├── motor/
│   ├── src/             ← o MOTOR de simulação, em C++20. Não sabe nada de tela.
│   │   ├── motor.hpp
│   │   └── motor.cpp
│   └── ponte/
│       └── ponte.cpp    ← a PONTE pybind11: única peça que liga C++ e Python
├── interface/
│   └── principal.py     ← a INTERFACE, em Python + rich. Só mostra e controla.
└── docs/                ← documentação de tudo
```

Gerados no seu PC e fora do repositório: `.venv/` (ambiente Python do projeto)
e `build/` (o motor compilado, `genesis_motor*.so`).

## Como a cadeia funciona

1. O `instalar.sh` compila `motor/src/*.cpp` numa biblioteca, e a `ponte.cpp`
   transforma essa biblioteca no módulo Python `genesis_motor`.
2. O `genesis` roda `interface/principal.py` com o Python do `.venv/`.
3. O `principal.py` faz `import genesis_motor` e chama o motor pela ponte.
4. O rich desenha no terminal.

## Como rodar no Linux Mint (primeira vez)

```bash
sudo apt install build-essential cmake python3-dev python3-venv git
git clone https://github.com/Generadevde/genesis.git
cd genesis
./instalar.sh
./genesis
```

Depois disso, basta `./genesis`. Sempre que o código C++ mudar
(depois de um `git pull`), rode `./instalar.sh` de novo antes.

## Estado atual (14/11/2026)

Esqueleto 0.0: o motor só se identifica. Serve para provar que a cadeia
C++ → pybind11 → Python → rich funciona. A primeira simulação está
descrita em [ILHA_V1.md](ILHA_V1.md).
