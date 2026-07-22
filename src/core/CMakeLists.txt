# Явно перечисляем все исходные файлы модуля core
set(CORE_SOURCES
    ngram_model.c
    model_io.c
    token.c
    # добавьте сюда все .c файлы из этой папки
)

# Создаём статическую библиотеку core
add_library(core STATIC ${CORE_SOURCES})

# Указываем, что заголовки из этой папки должны быть видны для тех,
# кто линкует эту библиотеку (PUBLIC)
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Добавляем флаги компиляции для этого модуля
target_compile_options(core PRIVATE -Wall -Wextra)

# (Опционально) Определения для платформы, если нужны
# if(WIN32)
#     target_compile_definitions(core PRIVATE _WIN32)
# endif()