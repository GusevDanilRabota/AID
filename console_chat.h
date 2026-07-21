/**
 * @file console_chat.h
 * @brief Модуль для интерактивного диалога с генератором через консоль.
 * 
 * Позволяет пользователю вводить запросы, генерировать ответы, настраивать параметры
 * и сохранять историю диалога в Markdown-файл.
 */

#ifndef CONSOLE_CHAT_H
#define CONSOLE_CHAT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Структура для хранения состояния диалога.
 */
typedef struct {
    char *output_filename;    /**< Имя выходного .md файла */
    int order;                /**< Порядок модели (0,1,2) */
    double temperature;       /**< Температура */
    int max_tokens;           /**< Максимальное количество токенов в ответе */
    int use_stopwords;        /**< Флаг фильтрации стоп-слов */
} ChatConfig;

/**
 * @brief Запускает интерактивный диалог с пользователем.
 * 
 * Функция создаёт или перезаписывает .md файл, затем в цикле читает ввод
 * пользователя, генерирует ответ и записывает диалог в файл.
 * 
 * @param config конфигурация чата (если NULL, используются значения по умолчанию)
 * @return 0 при успехе, -1 при ошибке
 */
int start_chat(const ChatConfig *config);

/**
 * @brief Записывает сообщение в файл диалога.
 * 
 * @param filename путь к .md файлу
 * @param speaker  имя говорящего (например, "User" или "AI")
 * @param text     текст сообщения
 * @param append   если 0, файл перезаписывается (для заголовка), иначе дописывается
 * @return 0 при успехе, -1 при ошибке
 */
int log_message(const char *filename, const char *speaker, const char *text, int append);

#ifdef __cplusplus
}
#endif

#endif