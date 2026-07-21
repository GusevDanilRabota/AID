/**
 * @file interactive.h
 * @brief Интерактивный диалог с долговременной памятью.
 */

#ifndef INTERACTIVE_H
#define INTERACTIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Запускает интерактивный режим с поддержкой памяти.
 *
 * @param log_filename  имя файла для сохранения диалога (если NULL – генерируется)
 * @param order         порядок n-граммы (0, 1, 2)
 * @param temperature   температура генерации
 * @param max_tokens    максимальное число токенов в ответе
 * @param use_stopwords 1 – фильтровать стоп-слова
 */
void interactive_start(const char *log_filename, int order, double temperature,
                       int max_tokens, int use_stopwords);

#ifdef __cplusplus
}
#endif

#endif