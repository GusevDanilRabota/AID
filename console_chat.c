/**
 * @file console_chat.c
 * @brief Реализация интерактивного диалога через консоль.
 */

#include "console_chat.h"
#include "markov.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_INPUT_LEN 2048
#define DEFAULT_OUTPUT "chat_log.md"

/**
 * @brief Записывает сообщение в файл диалога.
 */
int log_message(const char *filename, const char *speaker, const char *text, int append) {
    FILE *f = fopen(filename, append ? "a" : "w");
    if (!f) { perror("fopen"); return -1; }

    // Если файл только создаётся, добавляем заголовок и время
    if (!append) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(f, "# Диалог с генератором\n\n");
        fprintf(f, "**Дата и время:** %s\n\n", time_str);
        fprintf(f, "**Параметры:**\n");
        // параметры будут добавлены позже
    }

    // Если speaker не задан, пишем просто текст
    if (speaker && *speaker) {
        fprintf(f, "### %s\n\n", speaker);
    }
    fprintf(f, "%s\n\n", text);
    fclose(f);
    return 0;
}

/**
 * @brief Выводит подсказку и считывает строку из консоли.
 */
static int read_line(char *buffer, size_t size, const char *prompt) {
    printf("%s", prompt);
    if (fgets(buffer, size, stdin) == NULL) return -1;
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0';
    return 0;
}

/**
 * @brief Запрашивает у пользователя целое число с проверкой.
 */
static int get_int(const char *prompt, int default_val, int min, int max) {
    char input[64];
    int val = default_val;
    while (1) {
        printf("%s [%d]: ", prompt, default_val);
        if (fgets(input, sizeof(input), stdin) == NULL) return default_val;
        if (input[0] == '\n') return default_val;
        if (sscanf(input, "%d", &val) == 1 && val >= min && val <= max) return val;
        printf("Ошибка: введите число от %d до %d\n", min, max);
    }
}

/**
 * @brief Запрашивает у пользователя число с плавающей точкой.
 */
static double get_double(const char *prompt, double default_val, double min, double max) {
    char input[64];
    double val = default_val;
    while (1) {
        printf("%s [%.1f]: ", prompt, default_val);
        if (fgets(input, sizeof(input), stdin) == NULL) return default_val;
        if (input[0] == '\n') return default_val;
        if (sscanf(input, "%lf", &val) == 1 && val >= min && val <= max) return val;
        printf("Ошибка: введите число от %.1f до %.1f\n", min, max);
    }
}

/**
 * @brief Основная функция чата.
 */
int start_chat(const ChatConfig *config) {
    char output_file[256] = DEFAULT_OUTPUT;
    int order = 1;
    double temperature = 0.8;
    int max_tokens = 50;
    int use_stopwords = 0;

    // Если передан config, используем его, иначе запрашиваем у пользователя
    if (config) {
        if (config->output_filename) strcpy(output_file, config->output_filename);
        order = config->order;
        temperature = config->temperature;
        max_tokens = config->max_tokens;
        use_stopwords = config->use_stopwords;
    } else {
        // Запрашиваем параметры у пользователя
        printf("Настройка параметров диалога:\n");
        printf("Имя выходного файла (по умолчанию %s): ", DEFAULT_OUTPUT);
        char tmp[256];
        if (fgets(tmp, sizeof(tmp), stdin) && tmp[0] != '\n') {
            tmp[strcspn(tmp, "\n")] = '\0';
            strcpy(output_file, tmp);
        } else {
            strcpy(output_file, DEFAULT_OUTPUT);
        }

        order = get_int("Порядок модели (0-уни, 1-би, 2-три)", 1, 0, 2);
        temperature = get_double("Температура (0.0 - детерм, 1.0 - норма, >1 - хаос)", 0.8, 0.0, 2.0);
        max_tokens = get_int("Макс. токенов в ответе", 50, 5, 500);
        use_stopwords = get_int("Использовать стоп-слова (0-нет, 1-да)", 0, 0, 1);
    }

    // Создаём заголовок файла диалога с параметрами
    char header[512];
    snprintf(header, sizeof(header), "**Параметры:** порядок=%d, температура=%.2f, макс.токенов=%d, стоп-слова=%s",
             order, temperature, max_tokens, use_stopwords ? "вкл" : "выкл");
    if (log_message(output_file, NULL, header, 0) != 0) {
        fprintf(stderr, "Ошибка создания файла диалога\n");
        return -1;
    }

    // Приветствие
    printf("\n=== Добро пожаловать в диалог с генератором ===\n");
    printf("Параметры: порядок=%d, T=%.2f, макс.токенов=%d, стоп-слова=%s\n",
           order, temperature, max_tokens, use_stopwords ? "вкл" : "выкл");
    printf("Введите начальный токен или текст (для выхода введите 'exit' или 'quit')\n");
    printf("------------------------------------------------\n");

    char input[MAX_INPUT_LEN];
    char response[8192];
    char user_msg[MAX_INPUT_LEN + 32];

    while (1) {
        // Чтение ввода пользователя
        if (read_line(input, sizeof(input), "> ") != 0) break;
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) break;
        if (strlen(input) == 0) continue;

        // Логируем сообщение пользователя
        snprintf(user_msg, sizeof(user_msg), "**Ввод:** %s", input);
        log_message(output_file, "User", user_msg, 1);

        // Генерация ответа
        MarkovGenOptions opts = {
            .order = order,
            .temperature = temperature,
            .max_tokens = max_tokens,
            .start_token = input,   // используем ввод как начальный токен
            .use_stopwords = use_stopwords
        };
        int tokens = markov_generate_ex(response, sizeof(response), &opts);
        if (tokens < 0) {
            printf("Ошибка генерации. Возможно, токен '%s' не найден в таблицах.\n", input);
            log_message(output_file, "AI", "Ошибка генерации", 1);
            continue;
        }

        // Вывод ответа в консоль
        printf("AI: %s\n", response);
        printf("(сгенерировано токенов: %d)\n", tokens);

        // Логируем ответ AI
        char ai_msg[MAX_INPUT_LEN + 64];
        snprintf(ai_msg, sizeof(ai_msg), "**Ответ:** %s\n\n*(сгенерировано токенов: %d)*", response, tokens);
        log_message(output_file, "AI", ai_msg, 1);
    }

    printf("\nДиалог завершён. История сохранена в %s\n", output_file);
    return 0;
}