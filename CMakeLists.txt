cmake_minimum_required(VERSION 3.10)
project(MarkovClone C ASM)

set(CMAKE_C_STANDARD 11)

# Тесты для ассемблерных функций (единственная цель)
add_executable(test_asm tests/test_asm.c asm_funcs.S)