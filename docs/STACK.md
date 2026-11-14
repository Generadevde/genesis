# Stack técnica

Definida pelo dono do projeto em 14/11/2026.

| Parte | Tecnologia | Papel |
|---|---|---|
| Interface | Python + pygame (interface própria, estilo Windows 98) | Mostrar o mundo e receber comandos (mouse e teclado) |
| Motor de simulação | C++ | Toda a simulação (mundo, entidades, regras) |
| Ponte | pybind11 | Liga o Python ao motor em C++ |
| Compilação do C++ | CMake | Compila o motor e o módulo pybind11 |
| Persistência | SQLite | Dados persistentes e históricos do mundo |
| Representação | Unicode | Mapa e entidades desenhados com caracteres Unicode |

## Versões (decididas em 14/11/2026)

- **Python 3.12** (já instalado no Mint do dono do projeto, confirmado; antes cogitado o 3.13)
- **C++20**

## Plataforma

Só Linux. A máquina do dono do projeto usa **Linux Mint Cinnamon**.

## Interface

Começou com **rich** (texto no terminal). Em 14/11/2026 o dono do projeto
decidiu fazer a **interface própria**, em **pygame**, no estilo retrô do
Windows 98; a versão rich foi apagada. Detalhes em [INTERFACE.md](INTERFACE.md).
A interface continua separada da simulação: trocar a tela não mexe no motor.

## Divisão de responsabilidades (aprovada em 14/11/2026)

- O **C++** simula. Não sabe nada de tela.
- O **Python** mostra e controla. Não contém regras da simulação.
- O **pybind11** é a única porta entre os dois.
- O **SQLite** guarda o estado e a história do mundo.

