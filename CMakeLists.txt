# Минимальная версия CMake
cmake_minimum_required(VERSION 3.10)

# Название проекта и язык (только C)
project(MarkovProcessor LANGUAGES C)

# Устанавливаем стандарт C99 (обязательно для проекта)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Опции для включения/отключения модулей
option(ENABLE_CORE "Build core module" OFF)
option(ENABLE_UTILS "Build utils module" OFF)
option(ENABLE_PARSERS "Build parsers module" OFF)
option(ENABLE_GENERATION "Build generation module" OFF)
option(ENABLE_INTERACTIVE "Build interactive module" OFF)

# Опция для сборки тестов (по умолчанию включена)
option(BUILD_TESTS "Build unit tests" OFF)

# Подключаем поддиректорию с исходным кодом
add_subdirectory(src)

# Если тесты разрешены, подключаем поддиректорию с тестами
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Настройка флагов для отладочной и релизной сборки
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# (Опционально) Установка исполняемого файла в систему
install(TARGETS markov_processor DESTINATION bin)