"""
principal.py — interface do Genesis (Python + rich).

O que este arquivo faz:
    Importa o motor em C++ (módulo `genesis_motor`, gerado pela ponte
    pybind11) e mostra no terminal o que o motor devolve.

Regra da STACK.md: a interface só MOSTRA e CONTROLA. Nenhuma regra da
simulação mora aqui; tudo isso fica no motor em C++.

Por enquanto não há menus (pedido do dono do projeto): o programa abre
direto na simulação. Como a simulação ainda não existe, ele só confirma
que o motor carregou.
"""

import sys
from pathlib import Path

from rich.console import Console
from rich.panel import Panel

# O módulo compilado fica na pasta build/ na raiz do projeto.
# Colocamos essa pasta no caminho de busca do Python para o import funcionar.
RAIZ = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RAIZ / "build"))

import genesis_motor  # noqa: E402  (precisa vir depois do ajuste do caminho)


def main() -> None:
    console = Console()
    console.print(Panel(genesis_motor.identificacao(), title="Genesis"))


if __name__ == "__main__":
    main()
