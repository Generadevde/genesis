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
│   │   ├── mapa3d.hpp/.cpp  ← o volume (x, y, z): materiais + física da água
│   │   ├── gerador.hpp/.cpp ← o gerador de mundos (relevo e camadas)
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
│   ├── teste_ilha.py    ← confere as regras da simulação sem tela
│   └── teste_interface.py ← abre a interface sem tela e desenha vários quadros
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

O `./genesis` confere sozinho, antes de abrir: se falta alguma biblioteca
do Python (instala) e se o código do motor (C++) mudou desde a última
compilação (recompila). O `./instalar.sh` só precisa ser rodado de novo se
o ambiente Python (`.venv/`) for apagado.

Rodar os testes:

```bash
.venv/bin/python testes/teste_ilha.py
.venv/bin/python testes/teste_interface.py
```

## Estado atual (14/11/2026)

Motor 0.5: nomes e carregar maçã — ver [CARREGAR.md](CARREGAR.md); mundo em 3D — ver [MUNDO_3D.md](MUNDO_3D.md); a ilha com fome, sede e memória — ver [ILHA_V1.md](ILHA_V1.md).
