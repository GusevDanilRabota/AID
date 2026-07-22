# Явно перечисляем все исходные файлы модуля utils
set(UTILS_SOURCES
    file_utils.c
    string_utils.c
    time_utils.c
    # добавьте сюда все .c файлы из этой папки
)

# Создаём статическую библиотеку utils
add_library(utils STATIC ${UTILS_SOURCES})

# Заголовки видны зависимым
target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Флаги компиляции
target_compile_options(utils PRIVATE -Wall -Wextra)

# Никаких линковок с другими модулями (core и т.д.) не делаем