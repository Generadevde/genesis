#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# instalar.sh — prepara o Genesis para rodar. Rode uma vez (e de novo
# sempre que o código C++ mudar).
#
# O que ele faz, em ordem:
#   1. Cria um ambiente Python só do projeto em .venv/ (não mexe no Python
#      do sistema; o Mint não deixa instalar pacotes com pip fora de um venv).
#   2. Instala nesse ambiente o pybind11 e o rich.
#   3. Compila o motor C++ e a ponte com o CMake, gerando build/genesis_motor*.so
#
# Antes da primeira vez, instale as ferramentas do sistema (uma vez só):
#   sudo apt install build-essential cmake python3-dev python3-venv
# ---------------------------------------------------------------------------
set -euo pipefail            # para no primeiro erro, em vez de seguir quebrado

cd "$(dirname "$0")"          # sempre trabalha a partir da raiz do projeto

if [ ! -d .venv ]; then
    python3 -m venv .venv
fi
.venv/bin/pip install --quiet --upgrade pybind11 rich

# Diz ao CMake onde está o pybind11 e qual Python usar.
PYBIND11_DIR="$(.venv/bin/python -m pybind11 --cmakedir)"
cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -Dpybind11_DIR="$PYBIND11_DIR" \
      -DPython_EXECUTABLE="$(pwd)/.venv/bin/python"
cmake --build build -j

echo "Pronto. Para abrir: ./genesis"
