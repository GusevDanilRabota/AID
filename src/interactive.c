#include "interactive.h"
#include "markov.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_INPUT_LEN 4096
#define MAX_OUTPUT_LEN 8192

static void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

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

static void log_message(FILE *log, const char *prefix, const char *message, int is_user) {
    (void)prefix; // подавление предупреждения
    if (log) {
        if (is_user) fprintf(log, "**User:** %s\n\n", message);
        else fprintf(log, "**Bot:** %s\n\n", message);
        fflush(log);
    }
    printf("%s: %s\n", is_user ? "You" : "Bot", message);
}

void interactive_start(const char *log_filename, int order, double temperature,
                       int max_tokens, int use_stopwords) {
    char *filename = NULL;
    if (log_filename) filename = strdup(log_filename);
    else filename = generate_log_filename();
    if (!filename) { fprintf(stderr, "Failed to create log filename\n"); return; }

    FILE *log = fopen(filename, "w");
    if (!log) { perror("fopen log"); free(filename); return; }

    char ts[64];
    get_timestamp(ts, sizeof(ts));
    fprintf(log, "# Диалог с генератором\n\n");
    fprintf(log, "**Начало:** %s\n\n", ts);
    fprintf(log, "**Параметры:** порядок=%d, T=%.2f, макс. токенов=%d, стоп-слова=%s\n\n",
            order, temperature, max_tokens, use_stopwords ? "Да" : "Нет");
    fflush(log);

    printf("=== Интерактивный режим ===\n");
    printf("Введите 'exit' или 'quit' для выхода.\n");
    printf("Порядок=%d, T=%.2f\n\n", order, temperature);

    char input[MAX_INPUT_LEN], output[MAX_OUTPUT_LEN];
    MarkovGenOptions opts = { order, temperature, max_tokens, NULL, use_stopwords };

    while (1) {
        printf("You: ");
        if (!fgets(input, sizeof(input), stdin)) break;
        size_t len = strlen(input);
        if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            log_message(log, NULL, "Выход из диалога.", 1);
            break;
        }
        if (strlen(input) == 0) continue;

        log_message(log, NULL, input, 1);

        char *start = strdup(input);
        char *space = strchr(start, ' ');
        if (space) *space = '\0';
        const char *start_tokens[2] = { start, NULL };
        opts.start_tokens = start_tokens;

        int tokens = markov_generate_ex(output, sizeof(output), &opts);
        if (tokens < 0) log_message(log, NULL, "Ошибка генерации.", 0);
        else log_message(log, NULL, output, 0);
        free(start);
    }

    get_timestamp(ts, sizeof(ts));
    fprintf(log, "\n**Окончание:** %s\n", ts);
    fclose(log);
    printf("\nДиалог сохранён в: %s\n", filename);
    free(filename);
}