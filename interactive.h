/**
 * @file interactive.h
 * @brief Интерактивный режим общения с генератором через консоль.
 *
 * Позволяет пользователю вводить вопросы или начальные фразы,
 * генератор отвечает с использованием загруженных таблиц.
 * Диалог сохраняется в Markdown-файл с временной меткой.
 */

#ifndef INTERACTIVE_H
#define INTERACTIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Запускает интерактивный режим диалога.
 *
 * @param log_filename  имя файла для сохранения диалога (если NULL, создаётся автоматически)
 * @param order         порядок n-граммы (0, 1, 2)
 * @param temperature   температура генерации
 * @param max_tokens    максимальное число токенов в ответе
 * @param use_stopwords 1 – фильтровать стоп-слова, 0 – нет
 *
 * @note Для выхода из режима введите "exit" или "quit".
 */
void interactive_start(const char *log_filename, int order, double temperature,
                       int max_tokens, int use_stopwords);

#ifdef __cplusplus
}
#endif

#endif