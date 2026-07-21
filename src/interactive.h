/**
 * @file interactive.h
 * @brief Интерактивный режим диалога с генератором.
 * 
 * Позволяет пользователю вводить текст, а генератор отвечает,
 * используя загруженные модели. Диалог сохраняется в Markdown-файл.
 */

#ifndef INTERACTIVE_H
#define INTERACTIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Запускает интерактивный режим диалога.
 * 
 * @param log_filename  имя файла для сохранения диалога (если NULL – генерируется автоматически)
 * @param order         порядок n-граммы (0 – униграммы, 1 – биграммы, 2 – триграммы)
 * @param temperature   температура генерации (0.0 – детерминированно, 1.0 – нормально)
 * @param max_tokens    максимальное число токенов в ответе
 * @param use_stopwords 1 – фильтровать стоп-слова, 0 – нет
 */
void interactive_start(const char *log_filename, int order, double temperature,
                       int max_tokens, int use_stopwords);

#ifdef __cplusplus
}
#endif

#endif