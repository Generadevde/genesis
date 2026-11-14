# Estrutura do projeto e como rodar

## Pastas e arquivos

```
genesis/
├── genesis              ← o executável: abre direto na simulação, sem menus
├── instalar.sh          ← prepara tudo (ambiente Python + compila o C++)
├── CMakeLists.txt       ← receita de compilação do C++ (CMake)
├── motor/
│   ├── src/             ← o MOTOR de simulação, em C++20. Não sabe nada de tela.
│   │   ├── config.hpp   ← TODOS os números da simulação, num lugar só
│   │   ├── mundo.hpp    ← o mundo: ilha, macieiras, maçãs, NPCs (declaração)
│   │   ├── mundo.cpp    ← o mundo: as regras (implementação)
│   │   ├── motor.hpp
│   │   └── motor.cpp    ← identificação/versão do motor
│   └── ponte/
│       └── ponte.cpp    ← a PONTE pybind11: única peça que liga C++ e Python
├── interface/
│   └── principal.py     ← a INTERFACE, em Python + rich. Só mostra.
├── testes/
│   └── teste_ilha.py    ← confere as regras da ilha sem tela
└── docs/                ← documentação de tudo
```

Gerados no seu PC e fora do repositório: `.venv/` (ambiente Python do projeto)
e `build/` (o motor compilado, `genesis_motor*.so`).

## Como a cadeia funciona

1. O `instalar.sh` compila `motor/src/*.cpp` numa biblioteca, e a `ponte.cpp`
   transforma essa biblioteca no módulo Python `genesis_motor`.
2. O `genesis` roda `interface/principal.py` com o Python do `.venv/`.
3. O `principal.py` cria o `Mundo`, chama `avancar(dt)` com o tempo real que
   passou, lê o mapa pela ponte e o rich desenha no terminal.

## Como rodar no Linux Mint

Primeira vez:

```bash
sudo apt install build-essential cmake python3-dev python3-venv git
git clone https://github.com/Generadevde/genesis.git
cd genesis
./instalar.sh
./genesis
```

Para ver a ilha inteira: maximize o terminal e diminua bastante a fonte
(Ctrl + − no terminal do Mint). Sair: **Ctrl+C**.

Para atualizar depois:

```bash
git pull
./instalar.sh     # recompila o C++
./genesis
```

Rodar os testes:

```bash
.venv/bin/python testes/teste_ilha.py
```

## Estado atual (14/11/2026)

Motor 0.1: a ilha — ver [ILHA_V1.md](ILHA_V1.md).
