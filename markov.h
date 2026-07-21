/**
 * @file markov.h
 * @brief Генератор текста на основе цепей Маркова (униграммы и биграммы).
 *
 * Загружает таблицы вероятностей из файлов, созданных парсером,
 * и генерирует случайный текст, начиная с заданного токена.
 * Поддерживает температурный параметр для контроля случайности.
 */

#ifndef MARKOV_H
#define MARKOV_H

#include <stddef.h>   // для size_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Загружает униграммную таблицу из файла.
 * @param filename путь к файлу (например, "output/global_unigram.txt")
 * @return 0 при успехе, -1 при ошибке
 */
int markov_load_unigram(const char *filename);

/**
 * @brief Загружает биграммную таблицу из файла.
 * @param filename путь к файлу (например, "output/global_bigram.txt")
 * @return 0 при успехе, -1 при ошибке
 */
int markov_load_bigram(const char *filename);

/**
 * @brief Генерирует текст на основе загруженных таблиц.
 *
 * @param buffer       выходной буфер (должен быть достаточно большим)
 * @param buf_size     размер буфера
 * @param max_tokens   максимальное количество токенов для генерации
 * @param temperature  параметр случайности (0.0 – детерминированно, 1.0 – нормально, >1 – более хаотично)
 * @param use_bigram   1 – использовать биграммы (второй порядок), 0 – униграммы
 * @param start_token  начальный токен (например, "<BOS>" или конкретное слово)
 * @return количество сгенерированных токенов (не включая начальный), или -1 при ошибке
 */
int markov_generate(char *buffer, size_t buf_size, int max_tokens,
                    double temperature, int use_bigram, const char *start_token);

/**
 * @brief Освобождает память, занятую загруженными таблицами.
 */
void markov_free(void);

#ifdef __cplusplus
}
#endif

#endif