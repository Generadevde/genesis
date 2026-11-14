# Stack técnica

Definida pelo dono do projeto em 14/11/2026.

| Parte | Tecnologia | Papel |
|---|---|---|
| Interface | Python + rich | Mostrar o mundo e receber comandos do jogador, no terminal |
| Motor de simulação | C++ | Toda a simulação (mundo, entidades, regras) |
| Ponte | pybind11 | Liga o Python ao motor em C++ |
| Compilação do C++ | CMake | Compila o motor e o módulo pybind11 |
| Persistência | SQLite | Dados persistentes e históricos do mundo |
| Representação | Unicode | Mapa e entidades desenhados com caracteres Unicode |

## Versões (decididas em 14/11/2026)

- **Python 3.12** (o que já vem no Mint; antes cogitado o 3.13)
- **C++20**

## Plataforma

Só Linux. A máquina do dono do projeto usa **Linux Mint Cinnamon**.

## Interface

Por enquanto **rich** (texto no terminal). No futuro o projeto terá uma
interface própria, que substituirá o rich. Por isso a interface fica
separada da simulação: trocar a tela não pode exigir mexer no motor.

## Divisão de responsabilidades (aprovada em 14/11/2026)

- O **C++** simula. Não sabe nada de tela.
- O **Python** mostra e controla. Não contém regras da simulação.
- O **pybind11** é a única porta entre os dois.
- O **SQLite** guarda o estado e a história do mundo.

## Em aberto

- Versão exata do Mint.
