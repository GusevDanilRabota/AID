# Список библиотек, которые будут линковаться к исполняемому файлу
set(LINK_LIBRARIES "")

# Условно подключаем поддиректории и добавляем их библиотеки в список
if(ENABLE_CORE)
    add_subdirectory(core)
    list(APPEND LINK_LIBRARIES core)
endif()

if(ENABLE_UTILS)
    add_subdirectory(utils)
    list(APPEND LINK_LIBRARIES utils)
endif()

if(ENABLE_PARSERS)
    add_subdirectory(parsers)
    list(APPEND LINK_LIBRARIES parsers)
endif()

if(ENABLE_GENERATION)
    add_subdirectory(generation)
    list(APPEND LINK_LIBRARIES generation)
endif()

if(ENABLE_INTERACTIVE)
    add_subdirectory(interactive)
    list(APPEND LINK_LIBRARIES interactive)
endif()

# Создаём исполняемый файл (он будет всегда, даже если нет библиотек)
add_executable(markov_processor main.c)

# Линкуем только те библиотеки, которые были добавлены
if(LINK_LIBRARIES)
    target_link_libraries(markov_processor PRIVATE ${LINK_LIBRARIES})
endif()

# Общие флаги компиляции
target_compile_options(markov_processor PRIVATE -Wall -Wextra)

if(WIN32)
    target_compile_definitions(markov_processor PRIVATE _WIN32)
endif()