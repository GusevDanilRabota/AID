/**
 * @file c_parser.h
 * @brief Парсер C-кода для сбора n-грамм.
 */

#ifndef C_PARSER_H
#define C_PARSER_H

#include "ngram_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Обрабатывает все .c и .h файлы из директории dir,
 *        заполняя контексты униграмм, биграмм и триграмм.
 * @param dir  входная директория
 * @param uni  контекст униграмм
 * @param bi   контекст биграмм
 * @param tri  контекст триграмм
 */
void process_c_files(const char *dir, unigram_context *uni, bigram_context *bi, trigram_context *tri);

#ifdef __cplusplus
}
#endif

#endif