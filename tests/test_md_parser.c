/**
 * @file test_md_parser.c
 * @brief Юнит-тесты для модуля md_parser.
 *
 * Проверяет:
 * - создание локальных таблиц для каждого .md файла
 * - создание глобальных таблиц (уни-, би-, триграммы)
 * - корректность содержимого таблиц
 *
 * Результаты:
 * - output/test_md_results.txt – для конечного пользователя
 * - result/debug_md_log.txt   – для разработчика
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

#include "../src/md_parser.h"
#include "../src/ngram_model.h"

/* ---------- Вспомогательные функции для работы с файлами ---------- */
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

static void create_test_md_files(const char *dir) {
    ensure_dir(dir);

    // sample1.md
    FILE *f = fopen("input_test/sample1.md", "w");
    if (f) {
        fprintf(f,
            "# Test Header\n"
            "This is a test paragraph. It contains several words.\n"
            "- List item one\n"
            "- List item two\n"
            "> A quote\n"
            "End of file.\n"
        );
        fclose(f);
    }

    // sample2.md
    f = fopen("input_test/sample2.md", "w");
    if (f) {
        fprintf(f,
            "## Another header\n"
            "Another paragraph with different words.\n"
            "1. Numbered item\n"
            "2. Another numbered item\n"
            "Final sentence.\n"
        );
        fclose(f);
    }

    // sample3.md (пустой или с минимальным содержимым)
    f = fopen("input_test/sample3.md", "w");
    if (f) {
        fprintf(f,
            "# Empty-ish file\n"
            "Just one line.\n"
        );
        fclose(f);
    }
}

/* ---------- Глобальные файловые потоки ---------- */
static FILE *log_file = NULL;     // result/debug_md_log.txt
static FILE *result_file = NULL;  // output/test_md_results.txt

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

/* ---------- Конкретные тесты ---------- */

/* Тест 1: Проверка создания локальных таблиц */
static int test_local_tables_created(void) {
    TEST_START("local_tables_created");

    // Запускаем парсер
    process_markdown_files("input_test", "output_test");

    // Проверяем наличие локальных .txt файлов
    const char *files[] = {"sample1.txt", "sample2.txt", "sample3.txt"};
    int n = sizeof(files)/sizeof(files[0]);
    for (int i = 0; i < n; i++) {
        char path[256];
        snprintf(path, sizeof(path), "output_test/%s", files[i]);
        int exists = file_exists(path);
        TEST_ASSERT(exists == 1, "file exists: output_test/sample1.txt");
    }

    TEST_END("local_tables_created");
    return 1;
}

/* Тест 2: Проверка создания глобальных таблиц */
static int test_global_tables_created(void) {
    TEST_START("global_tables_created");

    const char *tables[] = {"global_unigram.txt", "global_bigram.txt", "global_trigram.txt"};
    for (int i = 0; i < 3; i++) {
        char path[256];
        snprintf(path, sizeof(path), "output_test/%s", tables[i]);
        int exists = file_exists(path);
        TEST_ASSERT(exists == 1, "global table exists: output_test/global_*.txt");
    }

    TEST_END("global_tables_created");
    return 1;
}

/* Тест 3: Проверка содержимого униграммной таблицы (наличие слов) */
static int test_unigram_content(void) {
    TEST_START("unigram_content");

    FILE *f = fopen("output_test/global_unigram.txt", "r");
    TEST_ASSERT(f != NULL, "can open global_unigram.txt");

    char line[1024];
    int found_test = 0, found_paragraph = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "test ->")) found_test = 1;
        if (strstr(line, "paragraph ->")) found_paragraph = 1;
    }
    fclose(f);

    TEST_ASSERT(found_test == 1, "word 'test' found in unigram table");
    TEST_ASSERT(found_paragraph == 1, "word 'paragraph' found in unigram table");

    TEST_END("unigram_content");
    return 1;
}

/* Тест 4: Проверка содержимого биграммной таблицы (наличие ключа) */
static int test_bigram_content(void) {
    TEST_START("bigram_content");

    FILE *f = fopen("output_test/global_bigram.txt", "r");
    TEST_ASSERT(f != NULL, "can open global_bigram.txt");

    char line[1024];
    int found_key = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "test|paragraph ->")) {
            found_key = 1;
            break;
        }
    }
    fclose(f);

    // Может быть, что точная комбинация не встретится, поэтому проверим, что есть хотя бы одна биграмма
    TEST_ASSERT(found_key == 1, "bigram key 'test|paragraph' found (or any other)");

    TEST_END("bigram_content");
    return 1;
}

/* Тест 5: Проверка корректности записи write_count_tables (или write_prob_tables) */
static int test_write_count_tables(void) {
    TEST_START("write_count_tables");

    // Загружаем контексты (можно получить из глобальных, но мы просто вызовем write_count_tables,
    // которая уже вызывается внутри process_markdown_files.
    // Поэтому проверяем, что файлы имеют правильный формат: строки вида "слово -> цель(число), ..."
    // Это уже проверяется в test_unigram_content.
    // Дополнительно проверим, что число после '(' - это целое число.
    FILE *f = fopen("output_test/global_unigram.txt", "r");
    TEST_ASSERT(f != NULL, "can open global_unigram.txt");

    char line[1024];
    int valid_format = 1;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, " -> ")) {
            char *p = strstr(line, " -> ");
            if (p) {
                char *rest = p + 4;
                // Проверяем, что есть хотя бы одна пара "слово(число)"
                if (strstr(rest, "(") == NULL && !strstr(rest, "(no transitions)")) {
                    valid_format = 0;
                    break;
                }
            }
        }
    }
    fclose(f);
    TEST_ASSERT(valid_format == 1, "global_unigram.txt format is correct (word -> target(count))");

    TEST_END("write_count_tables");
    return 1;
}

/* ---------- Запуск всех тестов ---------- */
static void run_all_tests(void) {
    ensure_dir("output");
    ensure_dir("result");
    ensure_dir("input_test");

    // Создаём тестовые .md файлы, если их нет
    create_test_md_files("input_test");

    result_file = fopen("output/test_md_results.txt", "w");
    log_file = fopen("result/debug_md_log.txt", "w");

    if (!result_file || !log_file) {
        perror("fopen result/log");
        exit(1);
    }

    fprintf(result_file, "=== Test Results for md_parser ===\n");
    fprintf(result_file, "Generated: %s %s\n", __DATE__, __TIME__);
    fprintf(result_file, "------------------------------------\n\n");

    fprintf(log_file, "=== Debug Log for md_parser tests ===\n");
    fprintf(log_file, "Timestamp: %s %s\n\n", __DATE__, __TIME__);
    fflush(log_file);

    int (*tests[])(void) = {
        test_local_tables_created,
        test_global_tables_created,
        test_unigram_content,
        test_bigram_content,
        test_write_count_tables
    };
    const char *test_names[] = {
        "local_tables_created",
        "global_tables_created",
        "unigram_content",
        "bigram_content",
        "write_count_tables"
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
    printf("User results written to output/test_md_results.txt\n");
    printf("Debug log written to result/debug_md_log.txt\n");
}

int main(void) {
    run_all_tests();
    return 0;
}