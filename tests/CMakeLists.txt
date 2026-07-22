# Если тесты используют какие-то модули, мы можем проверить, включены ли они,
# и линковать только доступные.

# Собираем список доступных библиотек для тестов (на основе глобальных опций)
set(TEST_LINK_LIBRARIES "")
if(ENABLE_CORE)
    list(APPEND TEST_LINK_LIBRARIES core)
endif()
if(ENABLE_UTILS)
    list(APPEND TEST_LINK_LIBRARIES utils)
endif()
if(ENABLE_PARSERS)
    list(APPEND TEST_LINK_LIBRARIES parsers)
endif()
if(ENABLE_GENERATION)
    list(APPEND TEST_LINK_LIBRARIES generation)
endif()
if(ENABLE_INTERACTIVE)
    list(APPEND TEST_LINK_LIBRARIES interactive)
endif()

# Если нет ни одной библиотеки, тесты могут не собраться – можно выдать предупреждение
if(NOT TEST_LINK_LIBRARIES)
    message(WARNING "No modules enabled, tests may fail to link.")
endif()

set(TEST_SOURCES
    test_ngram.c
    test_tokenizer.c
    test_c_parser.c
    test_md_parser.c
    test_markov.c
    test_interactive.c
)

foreach(TEST_FILE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_FILE})

    if(TEST_LINK_LIBRARIES)
        target_link_libraries(${TEST_NAME} PRIVATE ${TEST_LINK_LIBRARIES})
    endif()

    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/core
        ${PROJECT_SOURCE_DIR}/src/utils
        ${PROJECT_SOURCE_DIR}/src/parsers
        ${PROJECT_SOURCE_DIR}/src/generation
        ${PROJECT_SOURCE_DIR}/src/interactive
    )
    target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()