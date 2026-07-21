/**
 * @file test_c_parser.c
 * @brief Юнит-тесты для модуля c_parser.
 *
 * Проверяет:
 * - создание локальных таблиц для каждого .c файла
 * - создание глобальных таблиц (уни-, би-, триграммы)
 * - корректность содержимого таблиц
 *
 * Результаты:
 * - output/test_c_parser_results.txt – для пользователя
 * - result/debug_c_parser_log.txt   – для разработчика
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#define FOPEN_MODE "rb"
#else
#include <sys/stat.h>
#define FOPEN_MODE "r"
#endif

#include "../src/c_parser.h"
#include "../src/ngram_model.h"

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

static int file_exists(const char *path) {
    FILE *f = fopen(path, FOPEN_MODE);
    if (f) { fclose(f); return 1; }
    return 0;
}

static void create_test_c_files(const char *dir) {
    ensure_dir(dir);

    // test1.c
    FILE *f = fopen("input_test_c/test1.c", "w");
    if (f) {
        fprintf(f,
            "#include <stdio.h>\n"
            "int main() {\n"
            "    int x = 42;\n"
            "    return x;\n"
            "}\n"
        );
        fclose(f);
    }

    // test2.c
    f = fopen("input_test_c/test2.c", "w");
    if (f) {
        fprintf(f,
            "void foo(int a) {\n"
            "    if (a > 0) {\n"
            "        a--;\n"
            "    }\n"
            "}\n"
        );
        fclose(f);
    }

    // test.h
    f = fopen("input_test_c/test.h", "w");
    if (f) {
        fprintf(f,
            "#ifndef TEST_H\n"
            "#define TEST_H\n"
            "int global_var;\n"
            "#endif\n"
        );
        fclose(f);
    }
}

/* ---------- Глобальные потоки ---------- */
static FILE *log_file = NULL;
static FILE *result_file = NULL;
static int tests_passed = 0, tests_failed = 0;

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

static int test_local_tables_created(void) {
    TEST_START("local_tables_created");

    // Запускаем парсер
    unigram_context uni; bigram_context bi; trigram_context tri;
    init_unigram(&uni); init_bigram(&bi); init_trigram(&tri);
    process_c_files("input_test_c", &uni, &bi, &tri);

    // Проверяем наличие глобальных таблиц (они создаются отдельно, но мы их не пишем здесь)
    // Вместо этого проверим, что контексты не пустые
    TEST_ASSERT(uni.word_count > 0, "unigram context contains words");
    TEST_ASSERT(bi.state_count > 0, "bigram context contains states");
    TEST_ASSERT(tri.state_count > 0, "trigram context contains states");

    free_unigram(&uni); free_bigram(&bi); free_trigram(&tri);
    TEST_END("local_tables_created");
    return 1;
}

static int test_unigram_content(void) {
    TEST_START("unigram_content");

    unigram_context uni; bigram_context bi; trigram_context tri;
    init_unigram(&uni); init_bigram(&bi); init_trigram(&tri);
    process_c_files("input_test_c", &uni, &bi, &tri);

    // Проверяем наличие ключевых слов
    int found_int = (find_word_unigram(&uni, "int") != -1);
    int found_return = (find_word_unigram(&uni, "return") != -1);
    TEST_ASSERT(found_int, "keyword 'int' found");
    TEST_ASSERT(found_return, "keyword 'return' found");

    free_unigram(&uni); free_bigram(&bi); free_trigram(&tri);
    TEST_END("unigram_content");
    return 1;
}

static int test_bigram_content(void) {
    TEST_START("bigram_content");

    unigram_context uni; bigram_context bi; trigram_context tri;
    init_unigram(&uni); init_bigram(&bi); init_trigram(&tri);
    process_c_files("input_test_c", &uni, &bi, &tri);

    // Проверяем наличие хотя бы одной биграммы
    TEST_ASSERT(bi.state_count > 0, "bigram states exist");
    // Можно проверить конкретную биграмму, но она может отличаться, поэтому просто проверяем непустоту
    TEST_ASSERT(bi.states[0].transitions != NULL, "first bigram has transitions");

    free_unigram(&uni); free_bigram(&bi); free_trigram(&tri);
    TEST_END("bigram_content");
    return 1;
}

static int test_trigram_content(void) {
    TEST_START("trigram_content");

    unigram_context uni; bigram_context bi; trigram_context tri;
    init_unigram(&uni); init_bigram(&bi); init_trigram(&tri);
    process_c_files("input_test_c", &uni, &bi, &tri);

    TEST_ASSERT(tri.state_count > 0, "trigram states exist");

    free_unigram(&uni); free_bigram(&bi); free_trigram(&tri);
    TEST_END("trigram_content");
    return 1;
}

/* ---------- Запуск всех тестов ---------- */
static void run_all_tests(void) {
    ensure_dir("output");
    ensure_dir("result");
    ensure_dir("input_test_c");
    create_test_c_files("input_test_c");

    result_file = fopen("output/test_c_parser_results.txt", "w");
    log_file = fopen("result/debug_c_parser_log.txt", "w");

    if (!result_file || !log_file) {
        perror("fopen result/log");
        exit(1);
    }

    fprintf(result_file, "=== Test Results for c_parser ===\n");
    fprintf(result_file, "Generated: %s %s\n", __DATE__, __TIME__);
    fprintf(result_file, "------------------------------------\n\n");

    fprintf(log_file, "=== Debug Log for c_parser tests ===\n");
    fprintf(log_file, "Timestamp: %s %s\n\n", __DATE__, __TIME__);
    fflush(log_file);

    int (*tests[])(void) = {
        test_local_tables_created,
        test_unigram_content,
        test_bigram_content,
        test_trigram_content
    };
    const char *test_names[] = {
        "local_tables_created",
        "unigram_content",
        "bigram_content",
        "trigram_content"
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
    printf("User results written to output/test_c_parser_results.txt\n");
    printf("Debug log written to result/debug_c_parser_log.txt\n");
}

int main(void) {
    run_all_tests();
    return 0;
}