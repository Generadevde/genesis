# ---------------------------------------------------------------------------
# CMakeLists.txt — receita de compilação do motor do Genesis.
#
# O que este arquivo faz:
#   1. Compila o motor de simulação (C++20) como uma biblioteca estática.
#   2. Compila a "ponte" pybind11, que transforma o motor num módulo Python
#      chamado `genesis_motor` (um arquivo .so que o Python consegue importar).
#
# Por que separar motor e ponte:
#   O motor não pode saber nada de Python nem de tela (regra da STACK.md).
#   Assim, a ponte é a única parte que conhece os dois lados.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)
project(genesis LANGUAGES CXX)

# Padrão da linguagem decidido pelo dono do projeto: C++20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# O pybind11 é encontrado pelo caminho que o script instalar.sh passa
# (vem do pacote pybind11 instalado no ambiente Python do projeto).
# PYBIND11_FINDPYTHON usa o jeito moderno do CMake de achar o Python.
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)

# --- Motor: só simulação, sem nada de Python -------------------------------
file(GLOB MOTOR_FONTES CONFIGURE_DEPENDS motor/src/*.cpp)
add_library(motor STATIC ${MOTOR_FONTES})
target_include_directories(motor PUBLIC motor/src)
# Necessário porque a biblioteca estática vai dentro de um módulo .so.
set_target_properties(motor PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- Ponte: módulo Python `genesis_motor` ----------------------------------
pybind11_add_module(genesis_motor motor/ponte/ponte.cpp)
target_link_libraries(genesis_motor PRIVATE motor)
