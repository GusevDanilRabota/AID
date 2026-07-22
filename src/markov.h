/**
 * @file markov.h
 * @brief Генератор текста на основе n-грамм.
 * 
 * Использует глобальные контексты из ngram_model.h.
 * @defgroup markov_api Markov API
 * @{
 */

#ifndef MARKOV_H
#define MARKOV_H

#include "ngram_model.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Параметры генерации текста.
 */
typedef struct {
    int order;              /**< Порядок n-граммы: 0=униграмма, 1=биграмма, 2=триграмма */
    double temperature;     /**< Температура для выборки (0.0 – детерминированно) */
    int max_tokens;         /**< Максимальное число токенов в генерируемом тексте */
    const char **start_tokens; /**< Массив начальных токенов (заканчивается NULL) */
    int use_stopwords;      /**< 1 – фильтровать стоп-слова, 0 – нет */
} MarkovGenOptions;

/**
 * @brief Загружает таблицы n-грамм из файлов (уни-, би-, триграммы).
 * @param unigram_file  путь к файлу униграмм
 * @param bigram_file   путь к файлу биграмм
 * @param trigram_file  путь к файлу триграмм
 * @return 0 при успехе, -1 при ошибке (все таблицы освобождаются)
 */
int markov_load_tables(const char *unigram_file, const char *bigram_file, const char *trigram_file);

/**
 * @brief Загружает список стоп-слов из файла (по одному на строку).
 * @param filename путь к файлу
 * @return 0 при успехе, -1 при ошибке
 */
int markov_load_stopwords(const char *filename);

/**
 * @brief Генерирует текст с расширенными опциями.
 * @param buffer   выходной буфер
 * @param buf_size размер буфера
 * @param options  указатель на структуру параметров
 * @return количество сгенерированных токенов, -1 при ошибке
 */
int markov_generate_ex(char *buffer, size_t buf_size, const MarkovGenOptions *options);

/**
 * @brief Упрощённый API генерации (совместимость).
 * @param buffer       выходной буфер
 * @param buf_size     размер буфера
 * @param max_tokens   максимальное число токенов
 * @param temperature  температура
 * @param use_bigram   1 – использовать биграммы, 0 – униграммы (второй порядок)
 * @param start_token  начальный токен (или NULL для использования "<BOS>")
 * @return количество сгенерированных токенов, -1 при ошибке
 */
int markov_generate(char *buffer, size_t buf_size, int max_tokens,
                    double temperature, int use_bigram, const char *start_token);

/**
 * @brief Освобождает всю память, занятую загруженными таблицами и стоп-словами.
 */
void markov_free(void);

#ifdef __cplusplus
}
#endif

#endif /* MARKOV_H */
/** @} */