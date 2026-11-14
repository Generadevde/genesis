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
├── interface/           ← a INTERFACE própria (pygame, estilo Windows 98)
│   ├── principal.py     ← a janela: monta a tela, mouse, teclado, motor
│   ├── win98.py         ← peças de desenho: relevo, menu, abas, tabela
│   └── aparencia.py     ← todas as cores e tamanhos
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
3. O `principal.py` abre a janela (pygame), cria o `Mundo`, chama
   `avancar(dt)` com o tempo real que passou, lê o mapa pela ponte e desenha.

## Como rodar no Linux Mint

Primeira vez:

```bash
sudo apt install build-essential cmake python3-dev python3-venv git fonts-noto-color-emoji
git clone https://github.com/Generadevde/genesis.git
cd genesis
./instalar.sh
./genesis
```

Controles: ver [INTERFACE.md](INTERFACE.md). Sair: menu **Arquivo → Sair** (ou fechar a janela).

Para atualizar depois:

```bash
git pull
./genesis
```

O `./genesis` confere sozinho se o código do motor (C++) mudou desde a
última compilação e, se mudou, recompila antes de abrir. O `./instalar.sh`
precisa ser rodado de novo quando mudar uma biblioteca do Python (como na
troca do rich pelo pygame) ou se o ambiente Python for apagado.

Rodar os testes:

```bash
.venv/bin/python testes/teste_ilha.py
```

## Estado atual (14/11/2026)

Motor 0.3: a ilha com fome, sede e memória — ver [ILHA_V1.md](ILHA_V1.md).
