/**
 * @file md_parser.h
 * @brief Парсер Markdown с учётом структуры блоков.
 */

#ifndef MD_PARSER_H
#define MD_PARSER_H

#include "ngram_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Обрабатывает все .md файлы из input_dir, заполняет контексты
 *        и сохраняет локальные и глобальные таблицы.
 * @param input_dir  путь к папке с .md файлами
 * @param output_dir путь для сохранения результатов
 */
void process_markdown_files(const char *input_dir, const char *output_dir);

/**
 * @brief Записывает глобальные таблицы вероятностей в файлы.
 * @param uni   контекст униграмм
 * @param bi    контекст биграмм
 * @param tri   контекст триграмм
 * @param dir   выходная директория
 * @param alpha коэффициент сглаживания
 */
void write_prob_tables(const unigram_context *uni, const bigram_context *bi,
                       const trigram_context *tri, const char *dir, double alpha);

#ifdef __cplusplus
}
#endif

#endif