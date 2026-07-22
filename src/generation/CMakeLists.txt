# Явно перечисляем все исходные файлы модуля generation
set(GENERATION_SOURCES
    markov.c
    stopwords.c
    # добавьте сюда все .c файлы из этой папки
)

# Создаём статическую библиотеку generation
add_library(generation STATIC ${GENERATION_SOURCES})

# Заголовки видны зависимым
target_include_directories(generation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Флаги компиляции
target_compile_options(generation PRIVATE -Wall -Wextra)

# Без линковки с другими модулями