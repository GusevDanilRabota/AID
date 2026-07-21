/**
 * @file test_ngram.c
 * @brief Юнит-тесты для модуля ngram_model.
 * 
 * Проверяет:
 * - инициализацию контекстов
 * - добавление слов/состояний
 * - добавление переходов
 * - поиск слов/состояний
 * - освобождение памяти
 * 
 * Результаты тестов записываются:
 * - output/test_results.txt  – для конечного пользователя (краткий отчёт)
 * - result/debug_log.txt     – для разработчика (подробный лог)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#else
#include <sys/stat.h>
#endif

#include "../src/ngram_model.h"

/* ---------- Вспомогательные функции для создания папок ---------- */
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
static FILE *log_file = NULL;   // подробный лог (result/debug_log.txt)
static FILE *result_file = NULL; // краткий отчёт (output/test_results.txt)

/* ---------- Утилиты для тестирования ---------- */
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

/* Тест 1: Инициализация униграмм */
static int test_unigram_init(void) {
    TEST_START("unigram_init");
    unigram_context ctx;
    init_unigram(&ctx);
    TEST_ASSERT(ctx.words == NULL, "words pointer is NULL");
    TEST_ASSERT(ctx.word_count == 0, "word_count is 0");
    TEST_ASSERT(ctx.word_capacity == 0, "word_capacity is 0");
    TEST_ASSERT(ctx.hash_table != NULL, "hash_table allocated");
    free_unigram(&ctx);
    TEST_END("unigram_init");
    return 1;
}

/* Тест 2: Добавление слов и переходов в униграммы */
static int test_unigram_add(void) {
    TEST_START("unigram_add");
    unigram_context ctx;
    init_unigram(&ctx);

    int idx1 = add_word_unigram(&ctx, "hello");
    TEST_ASSERT(idx1 == 0, "first word index = 0");
    TEST_ASSERT(ctx.word_count == 1, "word_count = 1");

    int idx2 = add_word_unigram(&ctx, "world");
    TEST_ASSERT(idx2 == 1, "second word index = 1");
    TEST_ASSERT(ctx.word_count == 2, "word_count = 2");

    int idx3 = add_word_unigram(&ctx, "hello"); // повтор
    TEST_ASSERT(idx3 == 0, "duplicate returns same index");
    TEST_ASSERT(ctx.word_count == 2, "word_count unchanged");

    // Добавляем переход от hello к world
    add_transition_unigram(&ctx, idx1, idx2);
    transition *t = ctx.words[idx1].transitions;
    TEST_ASSERT(t != NULL, "transition added");
    TEST_ASSERT(t->target_index == idx2, "target index correct");
    TEST_ASSERT(t->count == 1, "count = 1");

    add_transition_unigram(&ctx, idx1, idx2);
    TEST_ASSERT(t->count == 2, "count incremented to 2");

    free_unigram(&ctx);
    TEST_END("unigram_add");
    return 1;
}

/* Тест 3: Поиск слов в униграммах */
static int test_unigram_find(void) {
    TEST_START("unigram_find");
    unigram_context ctx;
    init_unigram(&ctx);
    add_word_unigram(&ctx, "apple");
    add_word_unigram(&ctx, "banana");
    add_word_unigram(&ctx, "cherry");

    int idx = find_word_unigram(&ctx, "banana");
    TEST_ASSERT(idx == 1, "found banana at index 1");

    idx = find_word_unigram(&ctx, "grape");
    TEST_ASSERT(idx == -1, "grape not found");

    free_unigram(&ctx);
    TEST_END("unigram_find");
    return 1;
}

/* Тест 4: Инициализация биграмм и добавление состояний */
static int test_bigram_init_add(void) {
    TEST_START("bigram_init_add");
    bigram_context ctx;
    init_bigram(&ctx);

    int idx1 = add_state_bigram(&ctx, "hello|world");
    TEST_ASSERT(idx1 == 0, "first state index = 0");
    TEST_ASSERT(ctx.state_count == 1, "state_count = 1");

    int idx2 = add_state_bigram(&ctx, "world|hello");
    TEST_ASSERT(idx2 == 1, "second state index = 1");
    TEST_ASSERT(ctx.state_count == 2, "state_count = 2");

    int idx3 = add_state_bigram(&ctx, "hello|world"); // duplicate
    TEST_ASSERT(idx3 == 0, "duplicate returns same index");
    TEST_ASSERT(ctx.state_count == 2, "state_count unchanged");

    free_bigram(&ctx);
    TEST_END("bigram_init_add");
    return 1;
}

/* Тест 5: Добавление переходов в биграммы */
static int test_bigram_transition(void) {
    TEST_START("bigram_transition");
    // Для биграмм нам нужен униграмный контекст, чтобы получать индексы слов
    unigram_context uni;
    init_unigram(&uni);
    int w1 = add_word_unigram(&uni, "hello");
    int w2 = add_word_unigram(&uni, "world");
    int w3 = add_word_unigram(&uni, "!");

    bigram_context ctx;
    init_bigram(&ctx);
    int state = add_state_bigram(&ctx, "hello|world");
    add_transition_bigram(&ctx, state, w3);
    // Найти переход и проверить
    bigram_transition *t = ctx.states[state].transitions;
    TEST_ASSERT(t != NULL, "transition added");
    TEST_ASSERT(t->target_index == w3, "target index correct");
    TEST_ASSERT(t->count == 1, "count = 1");

    add_transition_bigram(&ctx, state, w3);
    TEST_ASSERT(t->count == 2, "count incremented");

    free_bigram(&ctx);
    free_unigram(&uni);
    TEST_END("bigram_transition");
    return 1;
}

/* Тест 6: Поиск состояний биграмм */
static int test_bigram_find(void) {
    TEST_START("bigram_find");
    bigram_context ctx;
    init_bigram(&ctx);
    add_state_bigram(&ctx, "a|b");
    add_state_bigram(&ctx, "b|c");
    add_state_bigram(&ctx, "c|d");

    int idx = find_state_bigram(&ctx, "b|c");
    TEST_ASSERT(idx == 1, "found state at index 1");

    idx = find_state_bigram(&ctx, "x|y");
    TEST_ASSERT(idx == -1, "not found");

    free_bigram(&ctx);
    TEST_END("bigram_find");
    return 1;
}

/* Тест 7: Триграммы (аналогично биграммам) – кратко */
static int test_trigram_basic(void) {
    TEST_START("trigram_basic");
    trigram_context ctx;
    init_trigram(&ctx);

    int idx1 = add_state_trigram(&ctx, "a|b|c");
    TEST_ASSERT(idx1 == 0, "first state index = 0");
    TEST_ASSERT(ctx.state_count == 1, "state_count = 1");

    int idx2 = add_state_trigram(&ctx, "b|c|d");
    TEST_ASSERT(idx2 == 1, "second state index = 1");
    TEST_ASSERT(ctx.state_count == 2, "state_count = 2");

    int idx3 = add_state_trigram(&ctx, "a|b|c"); // duplicate
    TEST_ASSERT(idx3 == 0, "duplicate returns same index");

    // Добавим переход
    unigram_context uni;
    init_unigram(&uni);
    int w = add_word_unigram(&uni, "target");
    add_transition_trigram(&ctx, idx1, w);
    trigram_transition *t = ctx.states[idx1].transitions;
    TEST_ASSERT(t != NULL, "transition added");
    TEST_ASSERT(t->target_index == w, "target index correct");

    free_trigram(&ctx);
    free_unigram(&uni);
    TEST_END("trigram_basic");
    return 1;
}

/* Тест 8: Освобождение памяти (не приводит к утечкам) */
static int test_free_memory(void) {
    TEST_START("free_memory");
    // Просто вызываем free для всех типов контекстов
    unigram_context u; init_unigram(&u);
    add_word_unigram(&u, "test");
    add_transition_unigram(&u, 0, 0);
    free_unigram(&u);

    bigram_context b; init_bigram(&b);
    add_state_bigram(&b, "a|b");
    add_transition_bigram(&b, 0, 0);
    free_bigram(&b);

    trigram_context t; init_trigram(&t);
    add_state_trigram(&t, "a|b|c");
    add_transition_trigram(&t, 0, 0);
    free_trigram(&t);

    TEST_END("free_memory");
    return 1;
}

/* ---------- Запуск всех тестов ---------- */
static void run_all_tests(void) {
    // Создаём папки, если их нет
    ensure_dir("output");
    ensure_dir("result");

    // Открываем файлы для записи
    result_file = fopen("output/test_results.txt", "w");
    log_file = fopen("result/debug_log.txt", "w");

    if (!result_file || !log_file) {
        perror("fopen result/log");
        exit(1);
    }

    // Заголовки
    fprintf(result_file, "=== Test Results for ngram_model ===\n");
    fprintf(result_file, "Generated: %s\n", __DATE__ " " __TIME__);
    fprintf(result_file, "--------------------------------------\n\n");

    fprintf(log_file, "=== Debug Log for ngram_model tests ===\n");
    fprintf(log_file, "Timestamp: %s %s\n\n", __DATE__, __TIME__);
    fflush(log_file);

    // Массив указателей на тесты
    int (*tests[])(void) = {
        test_unigram_init,
        test_unigram_add,
        test_unigram_find,
        test_bigram_init_add,
        test_bigram_transition,
        test_bigram_find,
        test_trigram_basic,
        test_free_memory
    };
    const char *test_names[] = {
        "unigram_init",
        "unigram_add",
        "unigram_find",
        "bigram_init_add",
        "bigram_transition",
        "bigram_find",
        "trigram_basic",
        "free_memory"
    };
    int num_tests = sizeof(tests)/sizeof(tests[0]);

    // Запуск
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

    // Итоговый отчёт
    fprintf(result_file, "\n--------------------------------------\n");
    fprintf(result_file, "TOTAL: %d passed, %d failed\n", tests_passed, tests_failed);
    fclose(result_file);

    fprintf(log_file, "\n=== All tests completed ===\n");
    fprintf(log_file, "Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    fclose(log_file);

    // Вывод в консоль
    printf("\nTests completed.\n");
    printf("User results written to output/test_results.txt\n");
    printf("Debug log written to result/debug_log.txt\n");
}

/* ---------- Точка входа ---------- */
int main(void) {
    run_all_tests();
    return 0;
}