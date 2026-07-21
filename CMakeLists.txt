cmake_minimum_required(VERSION 3.10)
project(MarkovClone C ASM)

set(CMAKE_C_STANDARD 11)

# Клон (C + ASM)
add_executable(clone clone.c asm_funcs.S)

# Утилита построения модели (чистый C)
add_executable(model_builder model_builder.c)