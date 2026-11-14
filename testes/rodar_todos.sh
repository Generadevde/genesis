#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# rodar_todos.sh — roda todos os testes e PARA no primeiro erro.
# Use antes de enviar qualquer mudança:  ./testes/rodar_todos.sh
# ---------------------------------------------------------------------------
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --build build -j >/dev/null           # garante o motor atualizado
.venv/bin/python testes/teste_ilha.py
.venv/bin/python testes/teste_interface.py 2>&1 | grep -v -i "alsa\|hello from\|^pygame "
echo "== TUDO CERTO =="
