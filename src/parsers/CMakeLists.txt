# Явно перечисляем все исходные файлы модуля parsers
set(PARSERS_SOURCES
    md_parser.c
    c_parser.c
    tokenizer.c
    # добавьте сюда все .c файлы из этой папки
)

# Создаём статическую библиотеку parsers
add_library(parsers STATIC ${PARSERS_SOURCES})

# Заголовки видны зависимым
target_include_directories(parsers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Флаги компиляции
target_compile_options(parsers PRIVATE -Wall -Wextra)

# Межмодульных зависимостей нет – если парсеры используют функции из core,
# то это будет разрешено на этапе линковки исполняемого файла,
# но здесь мы не указываем target_link_libraries.