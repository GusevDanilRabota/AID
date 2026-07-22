/**
 * @file interactive.c
 * @brief Интерактивный режим с обучением на диалогах и контекстной генерацией.
 */

#include "interactive.h"
#include "markov.h"
#include "md_parser.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_INPUT_LEN   4096
#define MAX_OUTPUT_LEN  8192
#define MAX_MEMORY_LEN  8192
#define CONTEXT_TOKENS  30

static char global_memory[MAX_MEMORY_LEN] = {0};

static void load_memory(void) {
    FILE *f = fopen("memory/memory.md", "r");
    if (f) {
        char buffer[MAX_MEMORY_LEN];
        size_t n = fread(buffer, 1, sizeof(buffer)-1, f);
        buffer[n] = '\0';
        strncpy(global_memory, buffer, MAX_MEMORY_LEN-1);
        global_memory[MAX_MEMORY_LEN-1] = '\0';
        fclose(f);
    }
}

static void save_memory(void) {
    ensure_dir("memory");
    FILE *f = fopen("memory/memory.md", "w");
    if (f) {
        fprintf(f, "# Долговременная память AI-агента\n\n");
        fprintf(f, "%s", global_memory);
        fclose(f);
    }
}

static void add_to_memory(const char *text) {
    if (strlen(global_memory) + strlen(text) + 10 >= MAX_MEMORY_LEN) {
        printf("Память переполнена, удалите старые записи.\n");
        return;
    }
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    char entry[512];
    snprintf(entry, sizeof(entry), "- **%s** – %s\n", timestamp, text);
    strcat(global_memory, entry);
    save_memory();
    printf("Запомнено: %s\n", text);
}

static void show_memory(void) {
    printf("\n=== Память агента ===\n");
    if (strlen(global_memory) == 0) printf("(пусто)\n");
    else printf("%s", global_memory);
    printf("=====================\n\n");
}

static void train_on_dialog(const char *dialog_filename) {
    ensure_dir("input_dialogs");
    char dest[1024];
    snprintf(dest, sizeof(dest), "input_dialogs/%s", strrchr(dialog_filename, '/') ? strrchr(dialog_filename, '/') + 1 : dialog_filename);
    FILE *src = fopen(dialog_filename, "r");
    FILE *dst = fopen(dest, "w");
    if (!src || !dst) {
        perror("copy dialog");
        if (src) fclose(src);
        if (dst) fclose(dst);
        return;
    }
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), src)) fputs(buffer, dst);
    fclose(src); fclose(dst);

    char final_path[1024];
    snprintf(final_path, sizeof(final_path), "input/%s", strrchr(dialog_filename, '/') ? strrchr(dialog_filename, '/') + 1 : dialog_filename);
    if (rename(dest, final_path) != 0) {
        src = fopen(dest, "r");
        dst = fopen(final_path, "w");
        if (src && dst) {
            while (fgets(buffer, sizeof(buffer), src)) fputs(buffer, dst);
            fclose(src); fclose(dst);
        }
        remove(dest);
    }
    printf("Переобучение модели на основе нового диалога...\n");
    process_markdown_files("input", "output");
    markov_load_tables("output/global_unigram.txt",
                       "output/global_bigram.txt",
                       "output/global_trigram.txt");
    printf("Модель обновлена.\n");
}

void interactive_start(const char *log_filename, int order, double temperature,
                       int max_tokens, int use_stopwords) {
    ensure_dir("dialogs");
    ensure_dir("memory");
    load_memory();

    char filename[256];
    if (log_filename) {
        const char *base = strrchr(log_filename, '/');
        if (base) base++; else base = log_filename;
        snprintf(filename, sizeof(filename), "dialogs/%s", base);
    } else {
        char stamp[64];
        get_timestamp(stamp, sizeof(stamp));
        snprintf(filename, sizeof(filename), "dialogs/dialog_%s.md", stamp);
    }

    FILE *log = fopen(filename, "w");
    if (!log) { perror("fopen log"); return; }

    char ts[64];
    get_timestamp(ts, sizeof(ts));
    fprintf(log, "# Диалог с AI-агентом\n\n");
    fprintf(log, "**Начало:** %s\n\n", ts);
    fprintf(log, "**Параметры:** порядок=%d, T=%.2f, макс. токенов=%d, стоп-слова=%s\n\n",
            order, temperature, max_tokens, use_stopwords ? "Да" : "Нет");

    printf("\n=== Интерактивный режим с обучением ===\n");
    printf("Команды: /remember <текст>, /memory, /help, exit/quit\n");
    show_memory();

    char history[8192] = {0};
    char input[MAX_INPUT_LEN], output[MAX_OUTPUT_LEN];
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
        if (strlen(input) == 0) continue;

        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            fprintf(log, "**User:** Выход из диалога.\n\n");
            break;
        }
        if (strcmp(input, "/help") == 0) {
            printf("Доступные команды:\n");
            printf("  /remember <текст> – сохранить в память\n");
            printf("  /memory           – показать память\n");
            printf("  /help             – справка\n");
            printf("  exit, quit        – завершить диалог\n");
            continue;
        }
        if (strncmp(input, "/remember ", 10) == 0) {
            const char *text = input + 10;
            if (strlen(text) > 0) {
                add_to_memory(text);
                fprintf(log, "**User:** /remember %s\n", text);
                fprintf(log, "**System:** Запомнено.\n\n");
                fflush(log);
                continue;
            }
        }
        if (strcmp(input, "/memory") == 0) {
            show_memory();
            continue;
        }

        fprintf(log, "**User:** %s\n\n", input);
        fflush(log);

        if (strlen(history) + strlen(input) + 3 < sizeof(history)) {
            strcat(history, input);
            strcat(history, " ");
        } else {
            char temp[8192];
            size_t half = strlen(history) / 2;
            while (half > 0 && history[half] != ' ') half++;
            if (half == 0) half = strlen(history);
            snprintf(temp, sizeof(temp), "%s %s", history + half, input);
            strcpy(history, temp);
        }

        char *start_tokens[4] = {NULL, NULL, NULL, NULL};
        char history_copy[8192];
        strcpy(history_copy, history);
        char *word = strtok(history_copy, " ");
        char *last_words[3] = {NULL, NULL, NULL};
        while (word) {
            last_words[0] = last_words[1];
            last_words[1] = last_words[2];
            last_words[2] = word;
            word = strtok(NULL, " ");
        }
        int start_count = 0;
        for (int i = 0; i < 3; i++) {
            if (last_words[i]) start_tokens[start_count++] = last_words[i];
        }
        start_tokens[start_count] = NULL;
        opts.start_tokens = (const char**)start_tokens;

        int tokens = markov_generate_ex(output, sizeof(output), &opts);
        if (tokens < 0) {
            const char *err = "Ошибка генерации (нет данных или таблицы не загружены).";
            printf("Bot: %s\n", err);
            fprintf(log, "**Bot:** %s\n\n", err);
        } else {
            printf("Bot: %s\n", output);
            fprintf(log, "**Bot:** %s\n\n", output);
            if (strlen(history) + strlen(output) + 3 < sizeof(history)) {
                strcat(history, output);
                strcat(history, " ");
            }
        }
        fflush(log);
    }

    get_timestamp(ts, sizeof(ts));
    fprintf(log, "\n**Окончание:** %s\n", ts);
    fclose(log);

    printf("\nХотите обучить агента на этом диалоге? (y/n): ");
    char answer[10];
    fgets(answer, sizeof(answer), stdin);
    if (answer[0] == 'y' || answer[0] == 'Y') {
        train_on_dialog(filename);
    }

    printf("\nТекущий диалог сохранён в: %s\n", filename);
    printf("Память сохранена в memory/memory.md\n");
}