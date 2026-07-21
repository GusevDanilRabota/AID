/**
 * @file interactive.c
 * @brief Реализация интерактивного диалога с долговременной памятью.
 */

#include "interactive.h"
#include "markov.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#else
#include <sys/stat.h>
#endif

/* Проверка существования файла */
static int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

#define MAX_INPUT_LEN  4096
#define MAX_OUTPUT_LEN 8192
#define MAX_MEMORY_LEN 8192

/** Глобальный файл памяти (загружается при старте) */
static char global_memory[MAX_MEMORY_LEN] = {0};

/* ------------------------------------------------------------------
 * Вспомогательные функции для работы с папками
 * ------------------------------------------------------------------ */
static void ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
#ifdef _WIN32
        if (_mkdir(path) == -1) {
#else
        if (mkdir(path, 0755) == -1) {
#endif
            perror("mkdir");
            exit(1);
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s exists but is not a directory\n", path);
        exit(1);
    }
}

/* ------------------------------------------------------------------
 * Управление памятью
 * ------------------------------------------------------------------ */

/** Загрузить память из файла memory/memory.md */
static void load_memory(void) {
    FILE *f = fopen("memory/memory.md", "r");
    if (f) {
        char buffer[MAX_MEMORY_LEN];
        size_t n = fread(buffer, 1, sizeof(buffer)-1, f);
        buffer[n] = '\0';
        strncpy(global_memory, buffer, MAX_MEMORY_LEN-1);
        global_memory[MAX_MEMORY_LEN-1] = '\0';
        fclose(f);
    } else {
        // Если файла нет, создаём с приветствием
        global_memory[0] = '\0';
    }
}

/** Сохранить память в файл memory/memory.md */
static void save_memory(void) {
    ensure_dir("memory");
    FILE *f = fopen("memory/memory.md", "w");
    if (f) {
        fprintf(f, "# Долговременная память AI-агента\n\n");
        fprintf(f, "%s", global_memory);
        fclose(f);
    } else {
        perror("fopen memory");
    }
}

/** Добавить запись в память */
static void add_to_memory(const char *text) {
    if (strlen(global_memory) + strlen(text) + 10 >= MAX_MEMORY_LEN) {
        printf("Память переполнена, удалите старые записи.\n");
        return;
    }
    char timestamp[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    char entry[512];
    snprintf(entry, sizeof(entry), "- **%s** – %s\n", timestamp, text);
    strcat(global_memory, entry);
    save_memory();
    printf("Запомнено: %s\n", text);
}

/** Показать содержимое памяти */
static void show_memory(void) {
    printf("\n=== Память агента ===\n");
    if (strlen(global_memory) == 0) {
        printf("(пусто)\n");
    } else {
        printf("%s", global_memory);
    }
    printf("=====================\n\n");
}

/* ------------------------------------------------------------------
 * Сохранение диалога в архив
 * ------------------------------------------------------------------ */
static void archive_dialog(const char *log_filename) {
    ensure_dir("dialogs");
    // Если файл существует, копируем его в dialogs/ с временной меткой
    if (log_filename && file_exists(log_filename)) {
        char dest[1024];
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char stamp[64];
        strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", tm_info);
        snprintf(dest, sizeof(dest), "dialogs/dialog_%s.md", stamp);
        FILE *src = fopen(log_filename, "r");
        FILE *dst = fopen(dest, "w");
        if (src && dst) {
            char buffer[1024];
            while (fgets(buffer, sizeof(buffer), src)) {
                fputs(buffer, dst);
            }
            fclose(src);
            fclose(dst);
            printf("Диалог заархивирован в %s\n", dest);
        } else {
            perror("archive dialog");
        }
    }
}

/* ------------------------------------------------------------------
 * Основная функция диалога
 * ------------------------------------------------------------------ */
void interactive_start(const char *log_filename, int order, double temperature,
                       int max_tokens, int use_stopwords) {
    // Создаём необходимые папки
    ensure_dir("dialogs");
    ensure_dir("memory");

    // Загружаем память
    load_memory();

    // Генерируем имя файла для текущего диалога
    char filename[256];
    if (log_filename) {
        strncpy(filename, log_filename, sizeof(filename)-1);
        filename[sizeof(filename)-1] = '\0';
    } else {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char stamp[64];
        strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", tm_info);
        snprintf(filename, sizeof(filename), "dialog_%s.md", stamp);
    }

    FILE *log = fopen(filename, "w");
    if (!log) {
        perror("fopen log");
        return;
    }

    char ts[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(log, "# Диалог с AI-агентом\n\n");
    fprintf(log, "**Начало:** %s\n\n", ts);
    fprintf(log, "**Параметры:** порядок=%d, T=%.2f, макс. токенов=%d, стоп-слова=%s\n\n",
            order, temperature, max_tokens, use_stopwords ? "Да" : "Нет");

    // Выводим приветствие с памятью
    printf("\n=== Интерактивный режим с долговременной памятью ===\n");
    printf("Команды: /remember <текст>, /memory, /help, exit/quit\n");
    printf("Текущая память:\n");
    show_memory();

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

        if (strlen(input) == 0) continue;

        // Обработка команд
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            fprintf(log, "**User:** Выход из диалога.\n\n");
            fflush(log);
            break;
        }
        if (strcmp(input, "/help") == 0) {
            printf("Доступные команды:\n");
            printf("  /remember <текст> – сохранить важную информацию в память\n");
            printf("  /memory           – показать текущую память\n");
            printf("  /help             – эта справка\n");
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
            } else {
                printf("Укажите текст для запоминания.\n");
                continue;
            }
        }
        if (strcmp(input, "/memory") == 0) {
            show_memory();
            continue;
        }

        // Обычный пользовательский ввод – записываем в лог
        fprintf(log, "**User:** %s\n\n", input);
        fflush(log);

        // Формируем начальный токен из первого слова ввода
        char *start = strdup(input);
        char *space = strchr(start, ' ');
        if (space) *space = '\0';
        const char *start_tokens[2] = { start, NULL };
        opts.start_tokens = start_tokens;

        // Генерация ответа (с учётом памяти? Можно добавить память в контекст,
        // но у нас цепи Маркова – просто генерируем продолжение)
        int tokens = markov_generate_ex(output, sizeof(output), &opts);
        if (tokens < 0) {
            const char *err_msg = "Ошибка генерации (нет данных или таблицы не загружены).";
            printf("Bot: %s\n", err_msg);
            fprintf(log, "**Bot:** %s\n\n", err_msg);
            fflush(log);
        } else {
            printf("Bot: %s\n", output);
            fprintf(log, "**Bot:** %s\n\n", output);
            fflush(log);
        }
        free(start);
    }

    // Закрываем лог и архивируем диалог
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(log, "\n**Окончание:** %s\n", ts);
    fclose(log);

    // Архивируем в dialogs/
    archive_dialog(filename);

    printf("\nТекущий диалог сохранён в: %s\n", filename);
    printf("Память сохранена в memory/memory.md\n");
}