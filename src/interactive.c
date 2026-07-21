/**
 * @file interactive.c
 * @brief Реализация интерактивного диалога.
 */

#include "interactive.h"
#include "markov.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_INPUT_LEN  4096
#define MAX_OUTPUT_LEN 8192

/** Получить текущую временную метку в формате "YYYY-MM-DD HH:MM:SS" */
static void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/** Сгенерировать имя файла для лога, если не задано */
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

/** Записать сообщение в лог-файл и вывести в консоль */
static void log_message(FILE *log, const char *message, int is_user) {
    if (log) {
        if (is_user)
            fprintf(log, "**User:** %s\n\n", message);
        else
            fprintf(log, "**Bot:** %s\n\n", message);
        fflush(log);
    }
    printf("%s: %s\n", is_user ? "You" : "Bot", message);
}

void interactive_start(const char *log_filename, int order, double temperature,
                       int max_tokens, int use_stopwords) {
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

    FILE *log = fopen(filename, "w");
    if (!log) {
        perror("fopen log");
        free(filename);
        return;
    }

    char ts[64];
    get_timestamp(ts, sizeof(ts));
    fprintf(log, "# Диалог с генератором\n\n");
    fprintf(log, "**Начало:** %s\n\n", ts);
    fprintf(log, "**Параметры:**\n");
    fprintf(log, "- Порядок n-грамм: %d\n", order);
    fprintf(log, "- Температура: %.2f\n", temperature);
    fprintf(log, "- Максимум токенов в ответе: %d\n", max_tokens);
    fprintf(log, "- Фильтрация стоп-слов: %s\n\n", use_stopwords ? "Да" : "Нет");
    fflush(log);

    printf("\n=== Интерактивный режим ===\n");
    printf("Введите текст для генерации продолжения.\n");
    printf("Команды: 'exit' или 'quit' – выход, '/help' – справка.\n");
    printf("Параметры: порядок=%d, T=%.2f\n\n", order, temperature);

    char input[MAX_INPUT_LEN];
    char output[MAX_OUTPUT_LEN];
    MarkovGenOptions opts = {
        .order = order,
        .temperature = temperature,
        .max_tokens = max_tokens,
        .start_tokens = NULL,
        .use_stopwords = use_stopwords
    };

    while (1) {
        printf("You: ");
        if (!fgets(input, sizeof(input), stdin)) break;
        size_t len = strlen(input);
        if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';

        // Пропускаем пустые строки
        if (strlen(input) == 0) continue;

        // Обработка команд
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            log_message(log, "Выход из диалога.", 1);
            break;
        }
        if (strcmp(input, "/help") == 0) {
            printf("Доступные команды: exit, quit, /help\n");
            printf("Введите любой текст, и генератор попытается его продолжить.\n");
            continue;
        }

        // Запись вопроса пользователя
        log_message(log, input, 1);

        // Извлекаем первое слово как start_token (можно улучшить)
        char *start = strdup(input);
        char *space = strchr(start, ' ');
        if (space) *space = '\0';
        const char *start_tokens[2] = { start, NULL };
        opts.start_tokens = start_tokens;

        int tokens = markov_generate_ex(output, sizeof(output), &opts);
        if (tokens < 0) {
            log_message(log, "Ошибка генерации (возможно, нет данных).", 0);
        } else {
            log_message(log, output, 0);
        }
        free(start);
    }

    get_timestamp(ts, sizeof(ts));
    fprintf(log, "\n**Окончание:** %s\n", ts);
    fclose(log);
    printf("\nДиалог сохранён в файл: %s\n", filename);
    free(filename);
}