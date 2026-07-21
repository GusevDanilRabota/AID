/**
 * @file test_asm.c
 * @brief Модульные тесты для ассемблерных функций (lfsr, binary_search).
 *
 * Проверяет детерминированность LFSR и корректность бинарного поиска.
 * Возвращает 0, если все тесты пройдены, иначе 1.
 */

#include <stdio.h>
#include <stdint.h>

/* Объявления ассемблерных функций */
extern uint16_t lfsr_rand(void);
extern void lfsr_seed(uint32_t seed);
extern uint16_t binary_search_entry(const uint16_t *row, uint16_t rnd, int len);

/**
 * @brief Макрос для проверки условия в тесте.
 * @param cond Условие, которое должно быть истинным.
 * @param msg Сообщение при ошибке.
 * @return 0 при успехе, 1 при провале.
 */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s\n", msg); \
        return 1; \
    } \
} while(0)

/**
 * @brief Тест LFSR: проверяет, что после seed(12345) генерируется ожидаемая последовательность.
 * @return 0 при успехе.
 */
static int test_lfsr_sequence(void)
{
    const uint16_t expected[5] = { 41345, 65505, 51101, 35810, 39844 };
    lfsr_seed(12345U);
    for (int i = 0; i < 5; i++) {
        uint16_t val = lfsr_rand();
        printf("LFSR step %d: %u (expected %u)\n", i, (unsigned)val, (unsigned)expected[i]);
        TEST_ASSERT(val == expected[i], "LFSR value mismatch");
    }
    return 0;
}

/**
 * @brief Тест бинарного поиска: проверяет стандартные и граничные случаи.
 * @return 0 при успехе.
 */
static int test_binary_search(void)
{
    /* Массив пар {id, cum_prob} */
    const uint16_t row[] = {
        10, 10000,   /* 0..10000   → id 10 */
        20, 40000,   /* 10001..40000 → id 20 */
        30, 65535    /* 40001..65535 → id 30 */
    };

    /* 1. rnd внутри первого интервала */
    uint16_t res = binary_search_entry(row, 5000, 3);
    printf("rnd=5000 → id=%u (expected 10)\n", (unsigned)res);
    TEST_ASSERT(res == 10, "binary_search rnd=5000");

    /* 2. rnd точно на границе */
    res = binary_search_entry(row, 10000, 3);
    printf("rnd=10000 → id=%u (expected 10)\n", (unsigned)res);
    TEST_ASSERT(res == 10, "binary_search rnd=10000 boundary");

    /* 3. rnd внутри второго интервала */
    res = binary_search_entry(row, 30000, 3);
    printf("rnd=30000 → id=%u (expected 20)\n", (unsigned)res);
    TEST_ASSERT(res == 20, "binary_search rnd=30000");

    /* 4. rnd = 0 */
    res = binary_search_entry(row, 0, 3);
    printf("rnd=0 → id=%u (expected 10)\n", (unsigned)res);
    TEST_ASSERT(res == 10, "binary_search rnd=0");

    /* 5. rnd = 65535 (последний элемент) */
    res = binary_search_entry(row, 65535, 3);
    printf("rnd=65535 → id=%u (expected 30)\n", (unsigned)res);
    TEST_ASSERT(res == 30, "binary_search rnd=65535");

    /* 6. пустая строка (len=0) – ожидается 0 */
    res = binary_search_entry(row, 0, 0);
    printf("len=0 → id=%u (expected 0)\n", (unsigned)res);
    TEST_ASSERT(res == 0, "binary_search len=0");

    return 0;
}

int main(void)
{
    int failed = 0;
    printf("=== ASM Unit Tests ===\n");

    if (test_lfsr_sequence() != 0) {
        failed = 1;
    } else {
        printf("[PASS] LFSR sequence\n");
    }

    if (test_binary_search() != 0) {
        failed = 1;
    } else {
        printf("[PASS] binary_search\n");
    }

    return failed;
}