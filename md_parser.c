/**
 * @file md_parser.c
 * @brief Реализация парсера Markdown-файлов для цепей Маркова.
 * 
 * Содержит все структуры и функции для извлечения слов, подсчёта переходов,
 * работы с файловой системой и вывода результатов.
 */

#include "md_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <direct.h>      // для _mkdir
    #define strcasecmp _stricmp
#else
    #include <strings.h>     // для strcasecmp
#endif

/* Константы настройки парсера */
#define HASH_SIZE   1024     /**< Размер хеш-таблицы для слов */
#define MAX_WORD_LEN 256     /**< Максимальная длина слова */

/**
 * @struct transition
 * @brief Элемент списка переходов из одного слова в другое.
 */
typedef struct transition {
    int target_index;        /**< Индекс слова-приёмника в глобальном массиве */
    int count;               /**< Количество таких переходов */
    struct transition *next; /**< Следующий переход в списке */
} transition;

/**
 * @struct word_entry
 * @brief Запись о слове и все переходы из него.
 */
typedef struct word_entry {
    char *word;              /**< Само слово (строка) */
    transition *transitions; /**< Список переходов в другие слова */
} word_entry;

/**
 * @struct word_node
 * @brief Элемент цепочки в хеш-таблице для быстрого поиска индекса слова.
 */
typedef struct word_node {
    char *word;              /**< Слово (указатель на ту же строку, что и в word_entry) */
    int index;               /**< Индекс слова в массиве word_entry */
    struct word_node *next;  /**< Следующий элемент в цепочке коллизий */
} word_node;

/**
 * @struct parse_context
 * @brief Контекст парсинга одного файла (содержит все динамические структуры).
 */
typedef struct {
    word_entry *words;       /**< Массив всех уникальных слов */
    int word_count;          /**< Текущее количество слов */
    int word_capacity;       /**< Выделенная ёмкость массива words */
    word_node **hash_table;  /**< Хеш-таблица для поиска индекса по строке */
} parse_context;

/* ------------------------------------------------------------
 * Внутренние функции для работы с контекстом и словами
 * ------------------------------------------------------------ */

/**
 * @brief Инициализирует контекст парсинга (выделяет память под хеш-таблицу).
 * @param ctx указатель на контекст
 */
static void init_context(parse_context *ctx) {
    ctx->words = NULL;
    ctx->word_count = 0;
    ctx->word_capacity = 0;
    ctx->hash_table = calloc(HASH_SIZE, sizeof(word_node *));
    if (!ctx->hash_table) {
        perror("calloc");
        exit(1);
    }
}

/**
 * @brief Освобождает всю память, занятую контекстом.
 * @param ctx указатель на контекст
 */
static void free_context(parse_context *ctx) {
    for (int i = 0; i < ctx->word_count; i++) {
        free(ctx->words[i].word);
        transition *t = ctx->words[i].transitions;
        while (t) {
            transition *next = t->next;
            free(t);
            t = next;
        }
    }
    free(ctx->words);

    for (int i = 0; i < HASH_SIZE; i++) {
        word_node *node = ctx->hash_table[i];
        while (node) {
            word_node *next = node->next;
            free(node);
            node = next;
        }
    }
    free(ctx->hash_table);
}

/**
 * @brief Хеш-функция для строк (djb2-подобная).
 * @param s строка
 * @return хеш-значение в диапазоне [0, HASH_SIZE-1]
 */
static unsigned int hash_str(const char *s) {
    unsigned int h = 0;
    while (*s)
        h = h * 31 + *s++;
    return h % HASH_SIZE;
}

/**
 * @brief Поиск слова в хеш-таблице.
 * @param ctx контекст
 * @param s строка для поиска
 * @return индекс слова, если найдено, иначе -1
 */
static int find_word(parse_context *ctx, const char *s) {
    unsigned int h = hash_str(s);
    word_node *node = ctx->hash_table[h];
    while (node) {
        if (strcmp(node->word, s) == 0)
            return node->index;
        node = node->next;
    }
    return -1;
}

/**
 * @brief Добавляет новое слово в контекст (если его ещё нет).
 * @param ctx контекст
 * @param s строка (копируется)
 * @return индекс добавленного или уже существующего слова
 */
static int add_word(parse_context *ctx, const char *s) {
    int idx = find_word(ctx, s);
    if (idx != -1)
        return idx;

    /* Расширение массива слов при необходимости */
    if (ctx->word_count >= ctx->word_capacity) {
        ctx->word_capacity = ctx->word_capacity ? ctx->word_capacity * 2 : 16;
        ctx->words = realloc(ctx->words, ctx->word_capacity * sizeof(word_entry));
        if (!ctx->words) {
            perror("realloc");
            exit(1);
        }
    }

    char *copy = malloc(strlen(s) + 1);
    if (!copy) {
        perror("malloc");
        exit(1);
    }
    strcpy(copy, s);   // безопасно, т.к. длина контролируется вызывающей стороной

    ctx->words[ctx->word_count].word = copy;
    ctx->words[ctx->word_count].transitions = NULL;

    unsigned int h = hash_str(s);
    word_node *node = malloc(sizeof(word_node));
    if (!node) {
        perror("malloc");
        exit(1);
    }
    node->word = copy;
    node->index = ctx->word_count;
    node->next = ctx->hash_table[h];
    ctx->hash_table[h] = node;

    return ctx->word_count++;
}

/**
 * @brief Добавляет переход от слова from к слову to (увеличивает счётчик).
 * @param ctx контекст
 * @param from индекс слова-источника
 * @param to   индекс слова-приёмника
 */
static void add_transition(parse_context *ctx, int from, int to) {
    transition *t = ctx->words[from].transitions;
    while (t) {
        if (t->target_index == to) {
            t->count++;
            return;
        }
        t = t->next;
    }
    transition *new_t = malloc(sizeof(transition));
    if (!new_t) {
        perror("malloc");
        exit(1);
    }
    new_t->target_index = to;
    new_t->count = 1;
    new_t->next = ctx->words[from].transitions;
    ctx->words[from].transitions = new_t;
}

/* ------------------------------------------------------------
 * Парсинг одного файла и запись таблицы
 * ------------------------------------------------------------ */

/**
 * @brief Парсит входной поток и записывает таблицу частот в выходной поток.
 * @param input  открытый FILE* для чтения (MD-файл)
 * @param output открытый FILE* для записи (текстовый файл)
 */
static void parse_file(FILE *input, FILE *output) {
    parse_context ctx;
    init_context(&ctx);

    char word_buf[MAX_WORD_LEN];
    int pos = 0;
    int c;
    int prev_index = -1;   // индекс предыдущего слова, -1 означает "нет"

    while ((c = fgetc(input)) != EOF) {
        if (isalpha(c)) {
            if (pos < MAX_WORD_LEN - 1) {
                word_buf[pos++] = tolower(c);
            }
        } else {
            if (pos > 0) {
                word_buf[pos] = '\0';
                int cur_index = add_word(&ctx, word_buf);
                if (prev_index != -1) {
                    add_transition(&ctx, prev_index, cur_index);
                }
                prev_index = cur_index;
                pos = 0;
            }
        }
    }
    if (pos > 0) {
        word_buf[pos] = '\0';
        int cur_index = add_word(&ctx, word_buf);
        if (prev_index != -1) {
            add_transition(&ctx, prev_index, cur_index);
        }
    }

    /* Вывод таблицы в выходной поток */
    for (int i = 0; i < ctx.word_count; i++) {
        fprintf(output, "%s -> ", ctx.words[i].word);
        transition *t = ctx.words[i].transitions;
        if (!t) {
            fprintf(output, "(no transitions)\n");
        } else {
            while (t) {
                fprintf(output, "%s(%d)", ctx.words[t->target_index].word, t->count);
                t = t->next;
                if (t) fprintf(output, ", ");
            }
            fprintf(output, "\n");
        }
    }

    free_context(&ctx);
}

/* ------------------------------------------------------------
 * Вспомогательные функции для работы с файловой системой
 * ------------------------------------------------------------ */

/**
 * @brief Создаёт директорию, если она не существует.
 * @param path путь к директории
 * @note В случае ошибки программа завершается через exit().
 */
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
        fprintf(stderr, "Ошибка: %s существует, но не является директорией\n", path);
        exit(1);
    }
}

/**
 * @brief Формирует путь к выходному файлу, заменяя расширение на .txt.
 * @param out_dir      выходная директория
 * @param in_filename  полное имя входного файла (может содержать путь)
 * @return указатель на динамически выделенную строку с путём (надо освободить free())
 */
static char *make_output_path(const char *out_dir, const char *in_filename) {
    /* Отделяем базовое имя файла (без пути) */
    const char *base = strrchr(in_filename, '/');
    if (!base) base = in_filename;
    else base++;   // пропускаем '/'

    /* Копируем имя и отрезаем расширение */
    char *name_copy = strdup(base);
    if (!name_copy) {
        perror("strdup");
        exit(1);
    }
    char *dot = strrchr(name_copy, '.');
    if (dot) *dot = '\0';   // обрезаем расширение

    /* Собираем выходной путь: output_dir/имя.txt */
    size_t out_len = strlen(out_dir) + strlen(name_copy) + 5;  // +1 для '/', +4 для ".txt"
    char *out_path = malloc(out_len);
    if (!out_path) {
        perror("malloc");
        exit(1);
    }
    snprintf(out_path, out_len, "%s/%s.txt", out_dir, name_copy);
    free(name_copy);
    return out_path;
}

/* ------------------------------------------------------------
 * Экспортируемая функция
 * ------------------------------------------------------------ */

/**
 * @brief Основная функция: обрабатывает все .md файлы из input_dir и пишет в output_dir.
 * @param input_dir  путь к папке с исходными .md файлами
 * @param output_dir путь к папке для результатов
 */
void process_markdown_files(const char *input_dir, const char *output_dir) {
    ensure_dir(output_dir);

    DIR *dir = opendir(input_dir);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len < 4) continue;
        if (strcasecmp(name + len - 3, ".md") != 0) continue;

        /* Полный путь к входному файлу */
        size_t in_len = strlen(input_dir) + len + 2;
        char *in_path = malloc(in_len);
        if (!in_path) {
            perror("malloc");
            continue;
        }
        snprintf(in_path, in_len, "%s/%s", input_dir, name);

        FILE *in_file = fopen(in_path, "r");
        free(in_path);
        if (!in_file) {
            perror("fopen input");
            continue;
        }

        /* Путь к выходному файлу */
        char *out_path = make_output_path(output_dir, name);
        /* Выводим сообщение до открытия/записи, чтобы использовать out_path до free */
        printf("Обработан: %s -> %s\n", name, out_path);

        FILE *out_file = fopen(out_path, "w");
        free(out_path);   // освобождаем после использования
        if (!out_file) {
            perror("fopen output");
            fclose(in_file);
            continue;
        }

        parse_file(in_file, out_file);

        fclose(in_file);
        fclose(out_file);
    }

    closedir(dir);
}