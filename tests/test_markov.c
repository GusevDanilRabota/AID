/**
 * @file test_markov.c
 * @brief Тесты для модуля markov.
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

#include "../src/markov.h"
#include "../src/ngram_model.h"

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

static void create_test_tables(const char *dir) {
    ensure_dir(dir);
    FILE *f = fopen("input_test_tables/global_unigram.txt", "w");
    if (f) {
        fprintf(f, "hello -> world(2), universe(1)\n");
        fprintf(f, "world -> hello(1), universe(1)\n");
        fprintf(f, "universe -> hello(1), world(1)\n");
        fprintf(f, "test -> only(1)\n");
        fclose(f);
    }
    f = fopen("input_test_tables/global_bigram.txt", "w");
    if (f) {
        fprintf(f, "hello|world -> universe(1), hello(1)\n");
        fprintf(f, "world|universe -> hello(2)\n");
        fprintf(f, "universe|hello -> world(1)\n");
        fclose(f);
    }
    f = fopen("input_test_tables/global_trigram.txt", "w");
    if (f) {
        fprintf(f, "hello|world|universe -> hello(1), world(1)\n");
        fprintf(f, "world|universe|hello -> world(1)\n");
        fclose(f);
    }
}

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

/* Тесты */
static int test_load_tables(void) {
    TEST_START("load_tables");
    int res = markov_load_tables("input_test_tables/global_unigram.txt",
                                 "input_test_tables/global_bigram.txt",
                                 "input_test_tables/global_trigram.txt");
    TEST_ASSERT(res == 0, "markov_load_tables returns 0");
    TEST_END("load_tables");
    return 1;
}

static int test_generate_unigram(void) {
    TEST_START("generate_unigram");
    char buffer[1024];
    const char *start_arr[2] = {"hello", NULL};
    MarkovGenOptions opts = {0, 0.8, 10, start_arr, 0};
    int tokens = markov_generate_ex(buffer, sizeof(buffer), &opts);
    TEST_ASSERT(tokens > 0, "generation returns positive");
    TEST_ASSERT(strlen(buffer) > 0, "buffer not empty");
    fprintf(log_file, "  Generated: %s\n", buffer);
    TEST_END("generate_unigram");
    return 1;
}

static int test_generate_bigram(void) {
    TEST_START("generate_bigram");
    char buffer[1024];
    const char *start_arr[3] = {"hello", "world", NULL};
    MarkovGenOptions opts = {1, 0.9, 15, start_arr, 0};
    int tokens = markov_generate_ex(buffer, sizeof(buffer), &opts);
    TEST_ASSERT(tokens > 0, "generation returns positive");
    TEST_ASSERT(strlen(buffer) > 0, "buffer not empty");
    fprintf(log_file, "  Generated: %s\n", buffer);
    TEST_END("generate_bigram");
    return 1;
}

static int test_generate_trigram(void) {
    TEST_START("generate_trigram");
    char buffer[1024];
    const char *start_arr[4] = {"hello", "world", "universe", NULL};
    MarkovGenOptions opts = {2, 0.7, 12, start_arr, 0};
    int tokens = markov_generate_ex(buffer, sizeof(buffer), &opts);
    TEST_ASSERT(tokens > 0, "generation returns positive");
    TEST_ASSERT(strlen(buffer) > 0, "buffer not empty");
    fprintf(log_file, "  Generated: %s\n", buffer);
    TEST_END("generate_trigram");
    return 1;
}

static int test_stopwords_filter(void) {
    TEST_START("stopwords_filter");
    FILE *f = fopen("temp_stopwords.txt", "w");
    if (f) { fprintf(f, "hello\nworld\n"); fclose(f); }
    markov_load_stopwords("temp_stopwords.txt");
    char buffer[1024];
    const char *start_arr[2] = {"universe", NULL};
    MarkovGenOptions opts = {0, 0.8, 10, start_arr, 1};
    int tokens = markov_generate_ex(buffer, sizeof(buffer), &opts);
    TEST_ASSERT(tokens > 0, "generation with stopwords filter works");
    fprintf(log_file, "  Generated with stopwords: %s\n", buffer);
    remove("temp_stopwords.txt");
    TEST_END("stopwords_filter");
    return 1;
}

static int test_old_api(void) {
    TEST_START("old_api");
    char buffer[1024];
    int tokens = markov_generate(buffer, sizeof(buffer), 8, 0.8, 1, "hello");
    TEST_ASSERT(tokens > 0, "old API works");
    fprintf(log_file, "  Old API generated: %s\n", buffer);
    TEST_END("old_api");
    return 1;
}

static int test_free_memory(void) {
    TEST_START("free_memory");
    markov_free();
    TEST_END("free_memory");
    return 1;
}

static void run_all_tests(void) {
    ensure_dir("output");
    ensure_dir("result");
    ensure_dir("input_test_tables");
    create_test_tables("input_test_tables");

    result_file = fopen("output/test_markov_results.txt", "w");
    log_file = fopen("result/debug_markov_log.txt", "w");
    if (!result_file || !log_file) { perror("fopen"); exit(1); }

    fprintf(result_file, "=== Test Results for markov ===\n");
    fprintf(result_file, "Generated: %s %s\n", __DATE__, __TIME__);
    fprintf(result_file, "----------------------------------\n\n");

    fprintf(log_file, "=== Debug Log for markov tests ===\n");
    fprintf(log_file, "Timestamp: %s %s\n\n", __DATE__, __TIME__);

    int (*tests[])(void) = {
        test_load_tables,
        test_generate_unigram,
        test_generate_bigram,
        test_generate_trigram,
        test_stopwords_filter,
        test_old_api,
        test_free_memory
    };
    const char *names[] = {
        "load_tables", "generate_unigram", "generate_bigram",
        "generate_trigram", "stopwords_filter", "old_api", "free_memory"
    };
    int num = sizeof(tests)/sizeof(tests[0]);

    for (int i = 0; i < num; i++) {
        int ok = tests[i]();
        if (ok) { tests_passed++; fprintf(result_file, "[PASS] %s\n", names[i]); }
        else { tests_failed++; fprintf(result_file, "[FAIL] %s\n", names[i]); }
    }

    fprintf(result_file, "\n----------------------------------\n");
    fprintf(result_file, "TOTAL: %d passed, %d failed\n", tests_passed, tests_failed);
    fclose(result_file);
    fprintf(log_file, "\n=== All tests completed ===\n");
    fprintf(log_file, "Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    fclose(log_file);

    printf("\nTests completed.\n");
    printf("User results written to output/test_markov_results.txt\n");
    printf("Debug log written to result/debug_markov_log.txt\n");
}

int main(void) {
    run_all_tests();
    return 0;
}