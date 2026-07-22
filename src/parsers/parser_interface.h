/**
 * @file parser_interface.h
 * @brief Абстрактный интерфейс для парсеров различных форматов.
 *
 * Каждый парсер должен реализовать функцию process_file(), которая
 * читает файл и заполняет переданные контексты n-грамм.
 */

#ifndef PARSER_INTERFACE_H
#define PARSER_INTERFACE_H

#include "ngram_model.h"  // из core

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Обрабатывает один файл, извлекая токены и обновляя модели.
 *
 * @param filename  путь к файлу
 * @param uni       контекст униграмм
 * @param bi        контекст биграмм
 * @param tri       контекст триграмм
 * @return 0 при успехе, -1 при ошибке
 */
int process_file(const char *filename,
                 unigram_context *uni,
                 bigram_context *bi,
                 trigram_context *tri);

#ifdef __cplusplus
}
#endif

#endif // PARSER_INTERFACE_H