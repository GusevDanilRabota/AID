/**
 * @file test_tokenizer.c
 * @brief Юнит-тесты для модуля tokenizer.
 *
 * Проверяет распознавание всех типов токенов.
 * Результаты записываются в output/test_tokenizer_results.txt
 * и result/debug_tokenizer_log.txt.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#else
#include <sys/stat.h>
#endif

#include "../src/tokenizer.h"

/* ---------- Вспомогательные функции ---------- */
static void ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
#ifdef _WIN32
        if (_mkdir(path) == -1) {
#else
        if (mkdir(path, 0755) == -1) {
#endif
            perror("mkdir");
            exit(1);
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s exists but is not a directory\n", path);
        exit(1);
    }
}

/* ---------- Глобальные файловые потоки ---------- */
static FILE *log_file = NULL;
static FILE *result_file = NULL;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_START(name) \
    do { \
        fprintf(log_file, "\n=== Starting test: %s ===\n", name); \
        fflush(log_file); \
    } while (0)

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(log_file, "  [FAIL] %s\n", msg); \
            fflush(log_file); \
            tests_failed++; \
            return 0; \
        } else { \
            fprintf(log_file, "  [PASS] %s\n", msg); \
            fflush(log_file); \
        } \
    } while (0)

#define TEST_END(name) \
    do { \
        fprintf(log_file, "=== Test %s completed ===\n", name); \
        fflush(log_file); \
    } while (0)

/* ---------- Тесты ---------- */

/** Вспомогательная: получить токены из строки и проверить их */
static int test_token_sequence(const char *code, TokenType expected_types[], const char *expected_values[], int count) {
    // Создаём временный файл с кодом
    FILE *f = tmpfile();
    if (!f) {
        perror("tmpfile");
        return 0;
    }
    fprintf(f, "%s", code);
    rewind(f);

    int ok = 1;
    for (int i = 0; i < count; i++) {
        Token *tok = get_next_token(f);
        if (!tok) {
            fprintf(log_file, "  [FAIL] get_next_token returned NULL at token %d\n", i);
            ok = 0;
            break;
        }
        if (tok->type != expected_types[i]) {
            fprintf(log_file, "  [FAIL] token %d: expected type %d, got %d (value: %s)\n",
                    i, expected_types[i], tok->type, tok->value);
            free_token(tok);
            ok = 0;
            break;
        }
        if (strcmp(tok->value, expected_values[i]) != 0) {
            fprintf(log_file, "  [FAIL] token %d: expected value '%s', got '%s'\n",
                    i, expected_values[i], tok->value);
            free_token(tok);
            ok = 0;
            break;
        }
        free_token(tok);
    }
    if (ok) {
        // Проверяем, что после всех токенов идёт EOF
        Token *tok = get_next_token(f);
        if (!tok || tok->type != TOKEN_EOF) {
            fprintf(log_file, "  [FAIL] expected EOF after all tokens\n");
            ok = 0;
        }
        if (tok) free_token(tok);
    }
    fclose(f);
    return ok;
}

/* Тест 1: Ключевые слова */
static int test_keywords(void) {
    TEST_START("keywords");
    TokenType types[] = {TOKEN_KEYWORD, TOKEN_KEYWORD, TOKEN_KEYWORD};
    const char *values[] = {"int", "return", "if"};
    int ok = test_token_sequence("int return if", types, values, 3);
    TEST_ASSERT(ok, "keywords recognized");
    TEST_END("keywords");
    return 1;
}

/* Тест 2: Идентификаторы */
static int test_identifiers(void) {
    TEST_START("identifiers");
    TokenType types[] = {TOKEN_IDENTIFIER, TOKEN_IDENTIFIER, TOKEN_IDENTIFIER};
    const char *values[] = {"myVar", "_hidden", "var123"};
    int ok = test_token_sequence("myVar _hidden var123", types, values, 3);
    TEST_ASSERT(ok, "identifiers recognized");
    TEST_END("identifiers");
    return 1;
}

/* Тест 3: Числа */
static int test_numbers(void) {
    TEST_START("numbers");
    TokenType types[] = {TOKEN_NUMBER, TOKEN_NUMBER, TOKEN_NUMBER, TOKEN_NUMBER};
    const char *values[] = {"123", "3.14", "0x1F", "1.2e-3"};
    int ok = test_token_sequence("123 3.14 0x1F 1.2e-3", types, values, 4);
    TEST_ASSERT(ok, "numbers recognized");
    TEST_END("numbers");
    return 1;
}

/* Тест 4: Строки */
static int test_strings(void) {
    TEST_START("strings");
    TokenType types[] = {TOKEN_STRING, TOKEN_STRING};
    const char *values[] = {"\"hello\"", "\"world\\n\""};
    int ok = test_token_sequence("\"hello\" \"world\\n\"", types, values, 2);
    TEST_ASSERT(ok, "strings recognized");
    TEST_END("strings");
    return 1;
}

/* Тест 5: Символы */
static int test_chars(void) {
    TEST_START("chars");
    TokenType types[] = {TOKEN_CHAR, TOKEN_CHAR};
    const char *values[] = {"'a'", "'\\n'"};
    int ok = test_token_sequence("'a' '\\n'", types, values, 2);
    TEST_ASSERT(ok, "characters recognized");
    TEST_END("chars");
    return 1;
}

/* Тест 6: Операторы */
static int test_operators(void) {
    TEST_START("operators");
    TokenType types[] = {TOKEN_OPERATOR, TOKEN_OPERATOR, TOKEN_OPERATOR, TOKEN_OPERATOR, TOKEN_OPERATOR};
    const char *values[] = {"+=", "->", "==", "&&", "<<"};
    int ok = test_token_sequence("+= -> == && <<", types, values, 5);
    TEST_ASSERT(ok, "operators recognized");
    TEST_END("operators");
    return 1;
}

/* Тест 7: Пунктуаторы */
static int test_punctuators(void) {
    TEST_START("punctuators");
    TokenType types[] = {TOKEN_PUNCTUATOR, TOKEN_PUNCTUATOR, TOKEN_PUNCTUATOR, TOKEN_PUNCTUATOR};
    const char *values[] = {";", "(", ")", "{"};
    int ok = test_token_sequence("; ( ) {", types, values, 4);
    TEST_ASSERT(ok, "punctuators recognized");
    TEST_END("punctuators");
    return 1;
}

/* Тест 8: Препроцессор */
static int test_preprocessor(void) {
    TEST_START("preprocessor");
    TokenType types[] = {TOKEN_PREPROCESSOR, TOKEN_PREPROCESSOR};
    const char *values[] = {"#include", "#define"};
    int ok = test_token_sequence("#include #define", types, values, 2);
    TEST_ASSERT(ok, "preprocessor directives recognized");
    TEST_END("preprocessor");
    return 1;
}

/* Тест 9: Комментарии */
static int test_comments(void) {
    TEST_START("comments");
    TokenType types[] = {TOKEN_COMMENT, TOKEN_COMMENT};
    const char *values[] = {"// single line", "/* multi line */"};
    // Для теста придётся использовать отдельные строки, т.к. пробелы в комментариях
    int ok = test_token_sequence("// single line", &types[0], &values[0], 1);
    if (!ok) return 0;
    ok = test_token_sequence("/* multi line */", &types[1], &values[1], 1);
    TEST_ASSERT(ok, "comments recognized");
    TEST_END("comments");
    return 1;
}

/* Тест 10: Смесь токенов */
static int test_mixed(void) {
    TEST_START("mixed");
    TokenType types[] = {
        TOKEN_KEYWORD, TOKEN_IDENTIFIER, TOKEN_OPERATOR, TOKEN_NUMBER,
        TOKEN_PUNCTUATOR, TOKEN_KEYWORD, TOKEN_IDENTIFIER, TOKEN_PUNCTUATOR
    };
    const char *values[] = {"int", "x", "=", "42", ";", "return", "x", ";"};
    int ok = test_token_sequence("int x = 42; return x;", types, values, 8);
    TEST_ASSERT(ok, "mixed tokens recognized");
    TEST_END("mixed");
    return 1;
}

/* ---------- Запуск всех тестов ---------- */
static void run_all_tests(void) {
    ensure_dir("output");
    ensure_dir("result");

    result_file = fopen("output/test_tokenizer_results.txt", "w");
    log_file = fopen("result/debug_tokenizer_log.txt", "w");

    if (!result_file || !log_file) {
        perror("fopen result/log");
        exit(1);
    }

    fprintf(result_file, "=== Test Results for tokenizer ===\n");
    fprintf(result_file, "Generated: %s %s\n", __DATE__, __TIME__);
    fprintf(result_file, "------------------------------------\n\n");

    fprintf(log_file, "=== Debug Log for tokenizer tests ===\n");
    fprintf(log_file, "Timestamp: %s %s\n\n", __DATE__, __TIME__);
    fflush(log_file);

    int (*tests[])(void) = {
        test_keywords,
        test_identifiers,
        test_numbers,
        test_strings,
        test_chars,
        test_operators,
        test_punctuators,
        test_preprocessor,
        test_comments,
        test_mixed
    };
    const char *test_names[] = {
        "keywords",
        "identifiers",
        "numbers",
        "strings",
        "chars",
        "operators",
        "punctuators",
        "preprocessor",
        "comments",
        "mixed"
    };
    int num_tests = sizeof(tests)/sizeof(tests[0]);

    for (int i = 0; i < num_tests; i++) {
        int ok = tests[i]();
        if (ok) {
            tests_passed++;
            fprintf(result_file, "[PASS] %s\n", test_names[i]);
        } else {
            tests_failed++;
            fprintf(result_file, "[FAIL] %s\n", test_names[i]);
        }
    }

    fprintf(result_file, "\n------------------------------------\n");
    fprintf(result_file, "TOTAL: %d passed, %d failed\n", tests_passed, tests_failed);
    fclose(result_file);

    fprintf(log_file, "\n=== All tests completed ===\n");
    fprintf(log_file, "Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    fclose(log_file);

    printf("\nTests completed.\n");
    printf("User results written to output/test_tokenizer_results.txt\n");
    printf("Debug log written to result/debug_tokenizer_log.txt\n");
}

int main(void) {
    run_all_tests();
    return 0;
}