# Явно перечисляем все исходные файлы модуля interactive
set(INTERACTIVE_SOURCES
    interactive.c
    memory.c
    # добавьте сюда все .c файлы из этой папки
)

# Создаём статическую библиотеку interactive
add_library(interactive STATIC ${INTERACTIVE_SOURCES})

# Заголовки видны зависимым
target_include_directories(interactive PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Флаги компиляции
target_compile_options(interactive PRIVATE -Wall -Wextra)

# Без линковки с другими модулями