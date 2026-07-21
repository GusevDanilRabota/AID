/**
 * @file markov.h
 * @brief Генератор текста на основе цепей Маркова с экспортом в Markdown.
 */

#ifndef MARKOV_H
#define MARKOV_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Опции для генерации одного блока текста.
 */
typedef struct {
    int order;                /**< 0 – униграммы, 1 – биграммы */
    double temperature;       /**< Температура (0.0 – детерминированно) */
    int max_tokens;           /**< Максимальное количество токенов в блоке */
    const char *start_token;  /**< Начальный токен (если NULL, используется <BOS>) */
} MarkovGenOptions;

/**
 * @brief Загружает униграммную таблицу.
 */
int markov_load_unigram(const char *filename);

/**
 * @brief Загружает биграммную таблицу.
 */
int markov_load_bigram(const char *filename);

/**
 * @brief Генерирует текст в буфер.
 */
int markov_generate(char *buffer, size_t buf_size, int max_tokens,
                    double temperature, int use_bigram, const char *start_token);

/**
 * @brief Генерирует Markdown-файл с несколькими блоками текста.
 *
 * @param filename    путь к выходному .md файлу
 * @param title       заголовок документа (если NULL, заголовок не создаётся)
 * @param num_blocks  количество генерируемых блоков (абзацев/разделов)
 * @param options     массив опций для каждого блока (должен содержать num_blocks элементов)
 * @return 0 при успехе, -1 при ошибке
 */
int markov_generate_md_file(const char *filename, const char *title,
                            int num_blocks, const MarkovGenOptions *options);

/**
 * @brief Освобождает память, занятую таблицами.
 */
void markov_free(void);

#ifdef __cplusplus
}
#endif

#endif