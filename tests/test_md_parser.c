/**
 * @file test_md_parser.c
 * @brief Юнит-тесты для модуля md_parser с проверкой блоков.
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
#include "../src/utils.h"

static FILE *log_file = NULL;
static FILE *result_file = NULL;
static int tests_passed = 0, tests_failed = 0;

#define TEST_START(name) \
    do { fprintf(log_file, "\n=== Starting test: %s ===\n", name); fflush(log_file); } while (0)

#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { fprintf(log_file, "  [FAIL] %s\n", msg); fflush(log_file); tests_failed++; return 0; } \
         else { fprintf(log_file, "  [PASS] %s\n", msg); fflush(log_file); } } while (0)

#define TEST_END(name) \
    do { fprintf(log_file, "=== Test %s completed ===\n", name); fflush(log_file); } while (0)

static void create_test_md_files(const char *dir) {
    ensure_dir(dir);
    FILE *f = fopen("input_test/test_blocks.md", "w");
    if (f) {
        fprintf(f,
            "# Заголовок\n"
            "Обычный абзац.\n"
            "- элемент списка\n"
            "- второй элемент\n"
            "> цитата\n"
            "```\n"
            "код\n"
            "```\n"
        );
        fclose(f);
    }
}

/* --- Тесты --- */
static int test_block_tokens_present(void) {
    TEST_START("block_tokens_present");
    unigram_context uni; bigram_context bi; trigram_context tri;
    init_unigram(&uni); init_bigram(&bi); init_trigram(&tri);
    process_markdown_files("input_test", "output_test");

    int found_start = (find_word_unigram(&uni, "<BLOCK_START H>") != -1) ||
                      (find_word_unigram(&uni, "<BLOCK_START P>") != -1);
    int found_end = (find_word_unigram(&uni, "<BLOCK_END>") != -1);
    TEST_ASSERT(found_start, "block start token found");
    TEST_ASSERT(found_end, "block end token found");

    free_unigram(&uni); free_bigram(&bi); free_trigram(&tri);
    TEST_END("block_tokens_present");
    return 1;
}

static int test_local_tables_created(void) {
    TEST_START("local_tables_created");
    unigram_context uni; bigram_context bi; trigram_context tri;
    init_unigram(&uni); init_bigram(&bi); init_trigram(&tri);
    process_markdown_files("input_test", "output_test");
    TEST_ASSERT(file_exists("output_test/test_blocks.txt"), "local table created");
    free_unigram(&uni); free_bigram(&bi); free_trigram(&tri);
    TEST_END("local_tables_created");
    return 1;
}

static int test_global_tables_created(void) {
    TEST_START("global_tables_created");
    unigram_context uni; bigram_context bi; trigram_context tri;
    init_unigram(&uni); init_bigram(&bi); init_trigram(&tri);
    process_markdown_files("input_test", "output_test");
    TEST_ASSERT(file_exists("output_test/global_unigram.txt"), "global_unigram created");
    TEST_ASSERT(file_exists("output_test/global_bigram.txt"), "global_bigram created");
    TEST_ASSERT(file_exists("output_test/global_trigram.txt"), "global_trigram created");
    free_unigram(&uni); free_bigram(&bi); free_trigram(&tri);
    TEST_END("global_tables_created");
    return 1;
}

static int test_unigram_content(void) {
    TEST_START("unigram_content");
    unigram_context uni; bigram_context bi; trigram_context tri;
    init_unigram(&uni); init_bigram(&bi); init_trigram(&tri);
    process_markdown_files("input_test", "output_test");
    int found_heading = (find_word_unigram(&uni, "Заголовок") != -1);
    int found_list = (find_word_unigram(&uni, "элемент") != -1);
    TEST_ASSERT(found_heading, "word 'Заголовок' found");
    TEST_ASSERT(found_list, "word 'элемент' found");
    free_unigram(&uni); free_bigram(&bi); free_trigram(&tri);
    TEST_END("unigram_content");
    return 1;
}

/* --- Запуск --- */
static void run_all_tests(void) {
    ensure_dir("output");
    ensure_dir("result");
    ensure_dir("input_test");
    create_test_md_files("input_test");

    result_file = fopen("output/test_md_results.txt", "w");
    log_file = fopen("result/debug_md_log.txt", "w");
    if (!result_file || !log_file) { perror("fopen result/log"); exit(1); }

    fprintf(result_file, "=== Test Results for md_parser (blocks) ===\n");
    fprintf(result_file, "Generated: %s %s\n", __DATE__, __TIME__);
    fprintf(result_file, "--------------------------------------------\n\n");

    fprintf(log_file, "=== Debug Log for md_parser tests ===\n");
    fprintf(log_file, "Timestamp: %s %s\n\n", __DATE__, __TIME__);

    int (*tests[])(void) = {
        test_local_tables_created,
        test_global_tables_created,
        test_unigram_content,
        test_block_tokens_present
    };
    const char *names[] = {
        "local_tables_created",
        "global_tables_created",
        "unigram_content",
        "block_tokens_present"
    };
    int n = sizeof(tests)/sizeof(tests[0]);

    for (int i = 0; i < n; i++) {
        int ok = tests[i]();
        if (ok) { tests_passed++; fprintf(result_file, "[PASS] %s\n", names[i]); }
        else { tests_failed++; fprintf(result_file, "[FAIL] %s\n", names[i]); }
    }

    fprintf(result_file, "\n--------------------------------------------\n");
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