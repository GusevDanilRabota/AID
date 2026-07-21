/**
 * @file md_parser.h
 * @brief Парсер Markdown с поддержкой униграмм, биграмм и триграмм.
 */

#ifndef MD_PARSER_H
#define MD_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Обрабатывает все .md файлы из input_dir, сохраняет локальные и глобальные таблицы.
 *
 * Выходные файлы:
 * - для каждого .md: <имя>.txt (униграммы частот)
 * - global_unigram.txt (вероятности)
 * - global_bigram.txt  (вероятности)
 * - global_trigram.txt (вероятности)   <-- НОВОЕ
 */
void process_markdown_files(const char *input_dir, const char *output_dir);

#ifdef __cplusplus
}
#endif

#endif