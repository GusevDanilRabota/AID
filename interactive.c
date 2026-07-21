/**
 * @file interactive.c
 * @brief Интерактивный режим с записью диалога в MD.
 */

#include "interactive.h"
#include "markov.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Максимальная длина ввода/ответа */
#define MAX_INPUT_LEN  4096
#define MAX_OUTPUT_LEN 8192

/**
 * @brief Получить текущую временную метку в формате "YYYY-MM-DD HH:MM:SS".
 */
static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/**
 * @brief Создать имя файла для лога, если не задано.
 *        Формат: dialog_YYYY-MM-DD_HH-MM-SS.md
 */
static char* generate_log_filename(void) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char stamp[64];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", tm_info);
    char *filename = malloc(strlen(stamp) + 16);
    if (!filename) return NULL;
    sprintf(filename, "dialog_%s.md", stamp);
    return filename;
}

/**
 * @brief Запись сообщения в лог-файл и вывод в консоль.
 */
static void log_message(FILE *log, const char *prefix, const char *message, int is_user) {
    if (log) {
        if (is_user) {
            fprintf(log, "**User:** %s\n\n", message);
        } else {
            fprintf(log, "**Bot:** %s\n\n", message);
        }
        fflush(log);
    }
    printf("%s: %s\n", is_user ? "You" : "Bot", message);
}

/**
 * @brief Основной цикл интерактивного диалога.
 */
void interactive_start(const char *log_filename, int order, double temperature,
                       int max_tokens, int use_stopwords) {
    // Определяем имя файла лога
    char *filename = NULL;
    if (log_filename) {
        filename = strdup(log_filename);
    } else {
        filename = generate_log_filename();
    }
    if (!filename) {
        fprintf(stderr, "Failed to create log filename\n");
        return;
    }

    // Открываем файл лога
    FILE *log = fopen(filename, "w");
    if (!log) {
        perror("fopen log");
        free(filename);
        return;
    }

    // Записываем заголовок и метаданные
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(log, "# Диалог с генератором\n\n");
    fprintf(log, "**Дата и время начала:** %s\n\n", timestamp);
    fprintf(log, "**Параметры генерации:**\n");
    fprintf(log, "- Порядок n-грамм: %d\n", order);
    fprintf(log, "- Температура: %.2f\n", temperature);
    fprintf(log, "- Максимум токенов в ответе: %d\n", max_tokens);
    fprintf(log, "- Фильтрация стоп-слов: %s\n\n", use_stopwords ? "Да" : "Нет");
    fflush(log);

    printf("=== Интерактивный режим ===\n");
    printf("Введите 'exit' или 'quit' для выхода.\n");
    printf("Параметры: порядок=%d, T=%.2f, макс. токенов=%d\n\n", order, temperature, max_tokens);

    char input[MAX_INPUT_LEN];
    char output[MAX_OUTPUT_LEN];
    MarkovGenOptions opts = {
        .order = order,
        .temperature = temperature,
        .max_tokens = max_tokens,
        .start_token = NULL,   // будет устанавливаться из ввода
        .use_stopwords = use_stopwords
    };

    while (1) {
        printf("You: ");
        if (!fgets(input, sizeof(input), stdin)) break;

        // Убираем символ новой строки
        size_t len = strlen(input);
        if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';

        // Проверка на выход
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            log_message(log, "User", "Выход из диалога.", 1);
            break;
        }

        // Если ввод пустой – пропускаем
        if (strlen(input) == 0) continue;

        // Записываем вопрос пользователя в лог и выводим
        log_message(log, "User", input, 1);

        // Генерируем ответ
        // Если ввод содержит несколько слов, используем первое как start_token? 
        // Для простоты используем весь ввод как начальный токен (но тогда он должен быть одним токеном).
        // Мы можем взять первое слово или фразу. Лучше взять последнее слово или всю фразу,
        // но генератор ожидает один токен. 
        // Для удобства будем использовать последнее слово в вводе как start_token.
        // Или можно использовать <BOS> и просто генерировать продолжение.
        // Сделаем так: если ввод содержит пробелы, возьмём первое слово как start_token.
        char *start = strdup(input);
        char *space = strchr(start, ' ');
        if (space) *space = '\0'; // берём первое слово
        opts.start_token = start;

        int tokens = markov_generate_ex(output, sizeof(output), &opts);
        if (tokens < 0) {
            log_message(log, "Bot", "Ошибка генерации.", 0);
        } else {
            // Убираем возможный начальный токен, если он совпадает с start_token
            // Можно просто вывести как есть
            log_message(log, "Bot", output, 0);
        }

        free(start);
    }

    // Записываем время окончания
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(log, "\n**Время окончания:** %s\n", timestamp);
    fclose(log);

    printf("\nДиалог сохранён в файл: %s\n", filename);
    free(filename);
}