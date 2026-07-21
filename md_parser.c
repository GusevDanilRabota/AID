/**
 * @file md_parser.c
 * @brief Реализация парсера Markdown-файлов для цепей Маркова с поддержкой глобальной таблицы вероятностей.
 * 
 * Содержит все структуры и функции для извлечения слов, подсчёта переходов,
 * работы с файловой системой и вывода результатов как по отдельным файлам, так и в обобщённом виде.
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
#define GLOBAL_TABLE_NAME "global_table.txt" /**< Имя файла для глобальной таблицы вероятностей */

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
 * @brief Контекст парсинга (содержит все динамические структуры для одного или глобального набора данных).
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
    strcpy(copy, s);

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
 * Парсинг одного файла с возможностью одновременного заполнения глобального контекста
 * ------------------------------------------------------------ */

/**
 * @brief Парсит входной поток и записывает таблицу частот в выходной поток.
 * Одновременно, если передан глобальный контекст, накапливает переходы в нём.
 * 
 * @param input     открытый FILE* для чтения (MD-файл)
 * @param output    открытый FILE* для записи локальной таблицы частот (может быть NULL, тогда только глобальное накопление)
 * @param global_ctx указатель на глобальный контекст (может быть NULL, тогда только локальная обработка)
 */
static void parse_file(FILE *input, FILE *output, parse_context *global_ctx) {
    parse_context local_ctx;
    init_context(&local_ctx);

    char word_buf[MAX_WORD_LEN];
    int pos = 0;
    int c;
    int prev_local = -1;
    int prev_global = -1;

    while ((c = fgetc(input)) != EOF) {
        if (isalpha(c)) {
            if (pos < MAX_WORD_LEN - 1) {
                word_buf[pos++] = tolower(c);
            }
        } else {
            if (pos > 0) {
                word_buf[pos] = '\0';
                int cur_local = add_word(&local_ctx, word_buf);
                int cur_global = -1;
                if (global_ctx) {
                    cur_global = add_word(global_ctx, word_buf);
                }

                if (prev_local != -1) {
                    add_transition(&local_ctx, prev_local, cur_local);
                    if (global_ctx && prev_global != -1) {
                        add_transition(global_ctx, prev_global, cur_global);
                    }
                }

                prev_local = cur_local;
                prev_global = cur_global;
                pos = 0;
            }
        }
    }
    if (pos > 0) {
        word_buf[pos] = '\0';
        int cur_local = add_word(&local_ctx, word_buf);
        int cur_global = -1;
        if (global_ctx) {
            cur_global = add_word(global_ctx, word_buf);
        }
        if (prev_local != -1) {
            add_transition(&local_ctx, prev_local, cur_local);
            if (global_ctx && prev_global != -1) {
                add_transition(global_ctx, prev_global, cur_global);
            }
        }
        /* последнее слово не обновляем, т.к. после него нет перехода */
    }

    /* Если задан выходной поток, записываем локальную таблицу частот */
    if (output) {
        for (int i = 0; i < local_ctx.word_count; i++) {
            fprintf(output, "%s -> ", local_ctx.words[i].word);
            transition *t = local_ctx.words[i].transitions;
            if (!t) {
                fprintf(output, "(no transitions)\n");
            } else {
                while (t) {
                    fprintf(output, "%s(%d)", local_ctx.words[t->target_index].word, t->count);
                    t = t->next;
                    if (t) fprintf(output, ", ");
                }
                fprintf(output, "\n");
            }
        }
    }

    free_context(&local_ctx);
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
    const char *base = strrchr(in_filename, '/');
    if (!base) base = in_filename;
    else base++;

    char *name_copy = strdup(base);
    if (!name_copy) {
        perror("strdup");
        exit(1);
    }
    char *dot = strrchr(name_copy, '.');
    if (dot) *dot = '\0';

    size_t out_len = strlen(out_dir) + strlen(name_copy) + 5;
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
 * Запись глобальной таблицы с вероятностями
 * ------------------------------------------------------------ */

/**
 * @brief Записывает глобальную таблицу вероятностей переходов в файл.
 * @param global_ctx глобальный контекст с накопленными переходами
 * @param output_dir выходная директория
 * @param filename   имя файла для записи (обычно GLOBAL_TABLE_NAME)
 */
static void write_global_table(const parse_context *global_ctx, const char *output_dir, const char *filename) {
    char *path = malloc(strlen(output_dir) + strlen(filename) + 2);
    if (!path) {
        perror("malloc");
        exit(1);
    }
    sprintf(path, "%s/%s", output_dir, filename);

    FILE *out = fopen(path, "w");
    if (!out) {
        perror("fopen global output");
        free(path);
        return;
    }

    for (int i = 0; i < global_ctx->word_count; i++) {
        const word_entry *we = &global_ctx->words[i];
        fprintf(out, "%s -> ", we->word);

        if (!we->transitions) {
            fprintf(out, "(no transitions)\n");
            continue;
        }

        /* Подсчёт общей суммы переходов из данного слова */
        int total = 0;
        transition *t = we->transitions;
        while (t) {
            total += t->count;
            t = t->next;
        }

        /* Вывод каждого перехода с вероятностью */
        t = we->transitions;
        while (t) {
            double prob = (double)t->count / total;
            fprintf(out, "%s(%.4f)", global_ctx->words[t->target_index].word, prob);
            t = t->next;
            if (t) fprintf(out, ", ");
        }
        fprintf(out, "\n");
    }

    fclose(out);
    free(path);
}

/* ------------------------------------------------------------
 * Экспортируемая функция
 * ------------------------------------------------------------ */

/**
 * @brief Основная функция: обрабатывает все .md файлы из input_dir и пишет в output_dir.
 * Дополнительно создаёт глобальную таблицу вероятностей.
 */
void process_markdown_files(const char *input_dir, const char *output_dir) {
    ensure_dir(output_dir);

    /* Инициализация глобального контекста */
    parse_context global_ctx;
    init_context(&global_ctx);

    DIR *dir = opendir(input_dir);
    if (!dir) {
        perror("opendir");
        free_context(&global_ctx);
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

        /* Путь к выходному файлу для локальной таблицы */
        char *out_path = make_output_path(output_dir, name);
        printf("Обработан: %s -> %s\n", name, out_path);

        FILE *out_file = fopen(out_path, "w");
        free(out_path);
        if (!out_file) {
            perror("fopen output");
            fclose(in_file);
            continue;
        }

        /* Парсим файл, записываем локальную таблицу и накапливаем глобальную */
        parse_file(in_file, out_file, &global_ctx);

        fclose(in_file);
        fclose(out_file);
    }

    closedir(dir);

    /* После обработки всех файлов записываем глобальную таблицу вероятностей */
    write_global_table(&global_ctx, output_dir, GLOBAL_TABLE_NAME);
    printf("Глобальная таблица сохранена в %s/%s\n", output_dir, GLOBAL_TABLE_NAME);

    free_context(&global_ctx);
}