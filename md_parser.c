/**
 * @file md_parser.c
 * @brief Расширенный парсер Markdown с поддержкой униграмм, биграмм и триграмм.
 */

#include "md_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <direct.h>
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

/* ------------------------------------------------------------------
 * Конфигурация
 * ------------------------------------------------------------------ */
#define HASH_SIZE       1024
#define MAX_WORD_LEN    256
#define SMOOTHING_ALPHA 0.01f

#define UNIGRAM_TABLE "global_unigram.txt"
#define BIGRAM_TABLE  "global_bigram.txt"
#define TRIGRAM_TABLE "global_trigram.txt"

/* ------------------------------------------------------------------
 * Структуры для униграмм
 * ------------------------------------------------------------------ */
typedef struct transition {
    int target_index;
    int count;
    struct transition *next;
} transition;

typedef struct word_entry {
    char *word;
    transition *transitions;
} word_entry;

typedef struct word_node {
    char *word;
    int index;
    struct word_node *next;
} word_node;

typedef struct {
    word_entry *words;
    int word_count;
    int word_capacity;
    word_node **hash_table;
} unigram_context;

/* ------------------------------------------------------------------
 * Структуры для биграмм
 * ------------------------------------------------------------------ */
typedef struct bigram_transition {
    int target_index;
    int count;
    struct bigram_transition *next;
} bigram_transition;

typedef struct bigram_state {
    char *key;
    bigram_transition *transitions;
} bigram_state;

typedef struct bigram_node {
    char *key;
    int index;
    struct bigram_node *next;
} bigram_node;

typedef struct {
    bigram_state *states;
    int state_count;
    int state_capacity;
    bigram_node **hash_table;
} bigram_context;

/* ------------------------------------------------------------------
 * Структуры для триграмм
 * ------------------------------------------------------------------ */
typedef struct trigram_transition {
    int target_index;
    int count;
    struct trigram_transition *next;
} trigram_transition;

typedef struct trigram_state {
    char *key;                 /* "word1|word2|word3" */
    trigram_transition *transitions;
} trigram_state;

typedef struct trigram_node {
    char *key;
    int index;
    struct trigram_node *next;
} trigram_node;

typedef struct {
    trigram_state *states;
    int state_count;
    int state_capacity;
    trigram_node **hash_table;
} trigram_context;

/* ------------------------------------------------------------------
 * Функции для униграмм (аналогичны предыдущей версии)
 * ------------------------------------------------------------------ */
static void init_unigram(unigram_context *ctx) {
    ctx->words = NULL;
    ctx->word_count = 0;
    ctx->word_capacity = 0;
    ctx->hash_table = calloc(HASH_SIZE, sizeof(word_node *));
    if (!ctx->hash_table) { perror("calloc"); exit(1); }
}

static void free_unigram(unigram_context *ctx) {
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
        word_node *n = ctx->hash_table[i];
        while (n) {
            word_node *next = n->next;
            free(n);
            n = next;
        }
    }
    free(ctx->hash_table);
}

static unsigned int hash_str(const char *s) {
    unsigned int h = 0;
    while (*s) h = h * 31 + *s++;
    return h % HASH_SIZE;
}

static int find_unigram(unigram_context *ctx, const char *s) {
    unsigned int h = hash_str(s);
    word_node *n = ctx->hash_table[h];
    while (n) {
        if (strcmp(n->word, s) == 0) return n->index;
        n = n->next;
    }
    return -1;
}

static int add_unigram(unigram_context *ctx, const char *s) {
    int idx = find_unigram(ctx, s);
    if (idx != -1) return idx;

    if (ctx->word_count >= ctx->word_capacity) {
        ctx->word_capacity = ctx->word_capacity ? ctx->word_capacity * 2 : 16;
        ctx->words = realloc(ctx->words, ctx->word_capacity * sizeof(word_entry));
        if (!ctx->words) { perror("realloc"); exit(1); }
    }

    char *copy = malloc(strlen(s) + 1);
    if (!copy) { perror("malloc"); exit(1); }
    strcpy(copy, s);

    ctx->words[ctx->word_count].word = copy;
    ctx->words[ctx->word_count].transitions = NULL;

    unsigned int h = hash_str(s);
    word_node *node = malloc(sizeof(word_node));
    if (!node) { perror("malloc"); exit(1); }
    node->word = copy;
    node->index = ctx->word_count;
    node->next = ctx->hash_table[h];
    ctx->hash_table[h] = node;

    return ctx->word_count++;
}

static void add_unigram_transition(unigram_context *ctx, int from, int to) {
    transition *t = ctx->words[from].transitions;
    while (t) {
        if (t->target_index == to) { t->count++; return; }
        t = t->next;
    }
    transition *new_t = malloc(sizeof(transition));
    if (!new_t) { perror("malloc"); exit(1); }
    new_t->target_index = to;
    new_t->count = 1;
    new_t->next = ctx->words[from].transitions;
    ctx->words[from].transitions = new_t;
}

/* ------------------------------------------------------------------
 * Функции для биграмм
 * ------------------------------------------------------------------ */
static void init_bigram(bigram_context *ctx) {
    ctx->states = NULL;
    ctx->state_count = 0;
    ctx->state_capacity = 0;
    ctx->hash_table = calloc(HASH_SIZE, sizeof(bigram_node *));
    if (!ctx->hash_table) { perror("calloc"); exit(1); }
}

static void free_bigram(bigram_context *ctx) {
    for (int i = 0; i < ctx->state_count; i++) {
        free(ctx->states[i].key);
        bigram_transition *t = ctx->states[i].transitions;
        while (t) {
            bigram_transition *next = t->next;
            free(t);
            t = next;
        }
    }
    free(ctx->states);
    for (int i = 0; i < HASH_SIZE; i++) {
        bigram_node *n = ctx->hash_table[i];
        while (n) {
            bigram_node *next = n->next;
            free(n);
            n = next;
        }
    }
    free(ctx->hash_table);
}

static unsigned int hash_key(const char *key) {
    unsigned int h = 0;
    while (*key) h = h * 31 + *key++;
    return h % HASH_SIZE;
}

static int find_bigram_state(bigram_context *ctx, const char *key) {
    unsigned int h = hash_key(key);
    bigram_node *n = ctx->hash_table[h];
    while (n) {
        if (strcmp(n->key, key) == 0) return n->index;
        n = n->next;
    }
    return -1;
}

static int add_bigram_state(bigram_context *ctx, const char *key) {
    int idx = find_bigram_state(ctx, key);
    if (idx != -1) return idx;

    if (ctx->state_count >= ctx->state_capacity) {
        ctx->state_capacity = ctx->state_capacity ? ctx->state_capacity * 2 : 16;
        ctx->states = realloc(ctx->states, ctx->state_capacity * sizeof(bigram_state));
        if (!ctx->states) { perror("realloc"); exit(1); }
    }

    char *copy = malloc(strlen(key) + 1);
    if (!copy) { perror("malloc"); exit(1); }
    strcpy(copy, key);

    ctx->states[ctx->state_count].key = copy;
    ctx->states[ctx->state_count].transitions = NULL;

    unsigned int h = hash_key(key);
    bigram_node *node = malloc(sizeof(bigram_node));
    if (!node) { perror("malloc"); exit(1); }
    node->key = copy;
    node->index = ctx->state_count;
    node->next = ctx->hash_table[h];
    ctx->hash_table[h] = node;

    return ctx->state_count++;
}

static void add_bigram_transition(bigram_context *ctx, int state_idx, int target_word_idx) {
    bigram_transition *t = ctx->states[state_idx].transitions;
    while (t) {
        if (t->target_index == target_word_idx) { t->count++; return; }
        t = t->next;
    }
    bigram_transition *new_t = malloc(sizeof(bigram_transition));
    if (!new_t) { perror("malloc"); exit(1); }
    new_t->target_index = target_word_idx;
    new_t->count = 1;
    new_t->next = ctx->states[state_idx].transitions;
    ctx->states[state_idx].transitions = new_t;
}

/* ------------------------------------------------------------------
 * Функции для триграмм
 * ------------------------------------------------------------------ */
static void init_trigram(trigram_context *ctx) {
    ctx->states = NULL;
    ctx->state_count = 0;
    ctx->state_capacity = 0;
    ctx->hash_table = calloc(HASH_SIZE, sizeof(trigram_node *));
    if (!ctx->hash_table) { perror("calloc"); exit(1); }
}

static void free_trigram(trigram_context *ctx) {
    for (int i = 0; i < ctx->state_count; i++) {
        free(ctx->states[i].key);
        trigram_transition *t = ctx->states[i].transitions;
        while (t) {
            trigram_transition *next = t->next;
            free(t);
            t = next;
        }
    }
    free(ctx->states);
    for (int i = 0; i < HASH_SIZE; i++) {
        trigram_node *n = ctx->hash_table[i];
        while (n) {
            trigram_node *next = n->next;
            free(n);
            n = next;
        }
    }
    free(ctx->hash_table);
}

static int find_trigram_state(trigram_context *ctx, const char *key) {
    unsigned int h = hash_key(key);
    trigram_node *n = ctx->hash_table[h];
    while (n) {
        if (strcmp(n->key, key) == 0) return n->index;
        n = n->next;
    }
    return -1;
}

static int add_trigram_state(trigram_context *ctx, const char *key) {
    int idx = find_trigram_state(ctx, key);
    if (idx != -1) return idx;

    if (ctx->state_count >= ctx->state_capacity) {
        ctx->state_capacity = ctx->state_capacity ? ctx->state_capacity * 2 : 16;
        ctx->states = realloc(ctx->states, ctx->state_capacity * sizeof(trigram_state));
        if (!ctx->states) { perror("realloc"); exit(1); }
    }

    char *copy = malloc(strlen(key) + 1);
    if (!copy) { perror("malloc"); exit(1); }
    strcpy(copy, key);

    ctx->states[ctx->state_count].key = copy;
    ctx->states[ctx->state_count].transitions = NULL;

    unsigned int h = hash_key(key);
    trigram_node *node = malloc(sizeof(trigram_node));
    if (!node) { perror("malloc"); exit(1); }
    node->key = copy;
    node->index = ctx->state_count;
    node->next = ctx->hash_table[h];
    ctx->hash_table[h] = node;

    return ctx->state_count++;
}

static void add_trigram_transition(trigram_context *ctx, int state_idx, int target_word_idx) {
    trigram_transition *t = ctx->states[state_idx].transitions;
    while (t) {
        if (t->target_index == target_word_idx) { t->count++; return; }
        t = t->next;
    }
    trigram_transition *new_t = malloc(sizeof(trigram_transition));
    if (!new_t) { perror("malloc"); exit(1); }
    new_t->target_index = target_word_idx;
    new_t->count = 1;
    new_t->next = ctx->states[state_idx].transitions;
    ctx->states[state_idx].transitions = new_t;
}

/* ------------------------------------------------------------------
 * Вспомогательная: формирование ключей
 * ------------------------------------------------------------------ */
static char* make_bigram_key(const unigram_context *uni, int idx1, int idx2) {
    char *key = malloc(2 * MAX_WORD_LEN + 2);
    if (!key) { perror("malloc"); exit(1); }
    sprintf(key, "%s|%s", uni->words[idx1].word, uni->words[idx2].word);
    return key;
}

static char* make_trigram_key(const unigram_context *uni, int idx1, int idx2, int idx3) {
    char *key = malloc(3 * MAX_WORD_LEN + 3);
    if (!key) { perror("malloc"); exit(1); }
    sprintf(key, "%s|%s|%s", uni->words[idx1].word, uni->words[idx2].word, uni->words[idx3].word);
    return key;
}

/* ------------------------------------------------------------------
 * Распознавание структуры Markdown
 * ------------------------------------------------------------------ */
static const char* detect_markdown_token(const char *line) {
    while (isspace(*line)) line++;
    if (*line == '#') return "<H>";
    if (*line == '-' || *line == '*' || *line == '+') return "<LI>";
    if (*line == '>') return "<BLOCKQUOTE>";
    return NULL;
}

/* ------------------------------------------------------------------
 * Обработка одной строки с обновлением уни-, би- и триграмм
 * ------------------------------------------------------------------ */
static void process_line(const char *line, unigram_context *uni, bigram_context *bi, trigram_context *tri,
                         int *prev1, int *prev2, int *prev3, int *sentence_start) {
    const char *md_token = detect_markdown_token(line);
    if (md_token) {
        int token_idx = add_unigram(uni, md_token);
        // Добавляем переходы для биграмм и триграмм (если есть контекст)
        if (*prev1 != -1 && *prev2 != -1) {
            char *key = make_bigram_key(uni, *prev2, *prev1);
            int state_idx = add_bigram_state(bi, key);
            add_bigram_transition(bi, state_idx, token_idx);
            free(key);
        }
        if (*prev1 != -1 && *prev2 != -1 && *prev3 != -1) {
            char *key = make_trigram_key(uni, *prev3, *prev2, *prev1);
            int state_idx = add_trigram_state(tri, key);
            add_trigram_transition(tri, state_idx, token_idx);
            free(key);
        }
        // Сдвиг окна
        *prev3 = *prev2;
        *prev2 = *prev1;
        *prev1 = token_idx;
        *sentence_start = 0;
    }

    const char *p = line;
    char token[MAX_WORD_LEN];
    int pos = 0, in_word = 0;

    while (*p) {
        if (isalpha(*p) || isdigit(*p)) {
            if (pos < MAX_WORD_LEN - 1) {
                token[pos++] = *p;
                in_word = 1;
            }
        } else {
            if (in_word) {
                token[pos] = '\0';
                if (*sentence_start) {
                    int bos_idx = add_unigram(uni, "<BOS>");
                    // Добавляем переходы с BOS
                    if (*prev1 != -1 && *prev2 != -1) {
                        char *key = make_bigram_key(uni, *prev2, *prev1);
                        int state_idx = add_bigram_state(bi, key);
                        add_bigram_transition(bi, state_idx, bos_idx);
                        free(key);
                    }
                    if (*prev1 != -1 && *prev2 != -1 && *prev3 != -1) {
                        char *key = make_trigram_key(uni, *prev3, *prev2, *prev1);
                        int state_idx = add_trigram_state(tri, key);
                        add_trigram_transition(tri, state_idx, bos_idx);
                        free(key);
                    }
                    *prev3 = *prev2;
                    *prev2 = *prev1;
                    *prev1 = bos_idx;
                    *sentence_start = 0;
                }
                int word_idx = add_unigram(uni, token);
                // Униграмма
                if (*prev1 != -1) add_unigram_transition(uni, *prev1, word_idx);
                // Биграмма
                if (*prev1 != -1 && *prev2 != -1) {
                    char *key = make_bigram_key(uni, *prev2, *prev1);
                    int state_idx = add_bigram_state(bi, key);
                    add_bigram_transition(bi, state_idx, word_idx);
                    free(key);
                }
                // Триграмма
                if (*prev1 != -1 && *prev2 != -1 && *prev3 != -1) {
                    char *key = make_trigram_key(uni, *prev3, *prev2, *prev1);
                    int state_idx = add_trigram_state(tri, key);
                    add_trigram_transition(tri, state_idx, word_idx);
                    free(key);
                }
                *prev3 = *prev2;
                *prev2 = *prev1;
                *prev1 = word_idx;
                pos = 0;
                in_word = 0;
            }

            if (strchr(".,!?;:-", *p)) {
                char punct[2] = {*p, '\0'};
                int punct_idx = add_unigram(uni, punct);
                // Добавляем переходы с пунктуацией
                if (*prev1 != -1) add_unigram_transition(uni, *prev1, punct_idx);
                if (*prev1 != -1 && *prev2 != -1) {
                    char *key = make_bigram_key(uni, *prev2, *prev1);
                    int state_idx = add_bigram_state(bi, key);
                    add_bigram_transition(bi, state_idx, punct_idx);
                    free(key);
                }
                if (*prev1 != -1 && *prev2 != -1 && *prev3 != -1) {
                    char *key = make_trigram_key(uni, *prev3, *prev2, *prev1);
                    int state_idx = add_trigram_state(tri, key);
                    add_trigram_transition(tri, state_idx, punct_idx);
                    free(key);
                }
                *prev3 = *prev2;
                *prev2 = *prev1;
                *prev1 = punct_idx;

                if (*p == '.' || *p == '!' || *p == '?') {
                    int eos_idx = add_unigram(uni, "<EOS>");
                    if (*prev1 != -1) add_unigram_transition(uni, *prev1, eos_idx);
                    if (*prev1 != -1 && *prev2 != -1) {
                        char *key = make_bigram_key(uni, *prev2, *prev1);
                        int state_idx = add_bigram_state(bi, key);
                        add_bigram_transition(bi, state_idx, eos_idx);
                        free(key);
                    }
                    if (*prev1 != -1 && *prev2 != -1 && *prev3 != -1) {
                        char *key = make_trigram_key(uni, *prev3, *prev2, *prev1);
                        int state_idx = add_trigram_state(tri, key);
                        add_trigram_transition(tri, state_idx, eos_idx);
                        free(key);
                    }
                    *prev3 = *prev2;
                    *prev2 = *prev1;
                    *prev1 = eos_idx;
                    *sentence_start = 1;
                }
            }
        }
        p++;
    }

    if (in_word) {
        token[pos] = '\0';
        if (*sentence_start) {
            int bos_idx = add_unigram(uni, "<BOS>");
            if (*prev1 != -1 && *prev2 != -1) {
                char *key = make_bigram_key(uni, *prev2, *prev1);
                int state_idx = add_bigram_state(bi, key);
                add_bigram_transition(bi, state_idx, bos_idx);
                free(key);
            }
            if (*prev1 != -1 && *prev2 != -1 && *prev3 != -1) {
                char *key = make_trigram_key(uni, *prev3, *prev2, *prev1);
                int state_idx = add_trigram_state(tri, key);
                add_trigram_transition(tri, state_idx, bos_idx);
                free(key);
            }
            *prev3 = *prev2;
            *prev2 = *prev1;
            *prev1 = bos_idx;
            *sentence_start = 0;
        }
        int word_idx = add_unigram(uni, token);
        if (*prev1 != -1) add_unigram_transition(uni, *prev1, word_idx);
        if (*prev1 != -1 && *prev2 != -1) {
            char *key = make_bigram_key(uni, *prev2, *prev1);
            int state_idx = add_bigram_state(bi, key);
            add_bigram_transition(bi, state_idx, word_idx);
            free(key);
        }
        if (*prev1 != -1 && *prev2 != -1 && *prev3 != -1) {
            char *key = make_trigram_key(uni, *prev3, *prev2, *prev1);
            int state_idx = add_trigram_state(tri, key);
            add_trigram_transition(tri, state_idx, word_idx);
            free(key);
        }
        *prev3 = *prev2;
        *prev2 = *prev1;
        *prev1 = word_idx;
    }
}

/* ------------------------------------------------------------------
 * Парсинг одного файла и запись локальной таблицы
 * ------------------------------------------------------------------ */
static void parse_file(FILE *input, FILE *output, unigram_context *uni, bigram_context *bi, trigram_context *tri) {
    char line[4096];
    int prev1 = -1, prev2 = -1, prev3 = -1;
    int sentence_start = 1;

    while (fgets(line, sizeof(line), input)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        process_line(line, uni, bi, tri, &prev1, &prev2, &prev3, &sentence_start);
    }

    if (output) {
        for (int i = 0; i < uni->word_count; i++) {
            fprintf(output, "%s -> ", uni->words[i].word);
            transition *t = uni->words[i].transitions;
            if (!t) fprintf(output, "(no transitions)\n");
            else {
                while (t) {
                    fprintf(output, "%s(%d)", uni->words[t->target_index].word, t->count);
                    t = t->next;
                    if (t) fprintf(output, ", ");
                }
                fprintf(output, "\n");
            }
        }
    }
}

/* ------------------------------------------------------------------
 * Запись глобальных таблиц с вероятностями (сглаженными)
 * ------------------------------------------------------------------ */
static void write_prob_table_unigram(const unigram_context *uni, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen unigram"); return; }
    int total = uni->word_count;
    for (int i = 0; i < uni->word_count; i++) {
        const word_entry *we = &uni->words[i];
        fprintf(f, "%s -> ", we->word);
        if (!we->transitions) { fprintf(f, "(no transitions)\n"); continue; }
        int sum = 0; transition *t = we->transitions;
        while (t) { sum += t->count; t = t->next; }
        t = we->transitions;
        while (t) {
            double prob = (double)(t->count + SMOOTHING_ALPHA) / (sum + SMOOTHING_ALPHA * total);
            fprintf(f, "%s(%.4f)", uni->words[t->target_index].word, prob);
            t = t->next;
            if (t) fprintf(f, ", ");
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

static void write_prob_table_bigram(const bigram_context *bi, const unigram_context *uni, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen bigram"); return; }
    int total = uni->word_count;
    for (int i = 0; i < bi->state_count; i++) {
        const bigram_state *st = &bi->states[i];
        fprintf(f, "%s -> ", st->key);
        if (!st->transitions) { fprintf(f, "(no transitions)\n"); continue; }
        int sum = 0; bigram_transition *t = st->transitions;
        while (t) { sum += t->count; t = t->next; }
        t = st->transitions;
        while (t) {
            double prob = (double)(t->count + SMOOTHING_ALPHA) / (sum + SMOOTHING_ALPHA * total);
            fprintf(f, "%s(%.4f)", uni->words[t->target_index].word, prob);
            t = t->next;
            if (t) fprintf(f, ", ");
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

static void write_prob_table_trigram(const trigram_context *tri, const unigram_context *uni, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen trigram"); return; }
    int total = uni->word_count;
    for (int i = 0; i < tri->state_count; i++) {
        const trigram_state *st = &tri->states[i];
        fprintf(f, "%s -> ", st->key);
        if (!st->transitions) { fprintf(f, "(no transitions)\n"); continue; }
        int sum = 0; trigram_transition *t = st->transitions;
        while (t) { sum += t->count; t = t->next; }
        t = st->transitions;
        while (t) {
            double prob = (double)(t->count + SMOOTHING_ALPHA) / (sum + SMOOTHING_ALPHA * total);
            fprintf(f, "%s(%.4f)", uni->words[t->target_index].word, prob);
            t = t->next;
            if (t) fprintf(f, ", ");
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

/* ------------------------------------------------------------------
 * Вспомогательные файловые функции
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

static char* make_output_path(const char *out_dir, const char *in_filename) {
    const char *base = strrchr(in_filename, '/');
    if (!base) base = in_filename;
    else base++;
    char *name_copy = strdup(base);
    if (!name_copy) { perror("strdup"); exit(1); }
    char *dot = strrchr(name_copy, '.');
    if (dot) *dot = '\0';
    size_t out_len = strlen(out_dir) + strlen(name_copy) + 6;
    char *out_path = malloc(out_len);
    if (!out_path) { perror("malloc"); exit(1); }
    snprintf(out_path, out_len, "%s/%s.txt", out_dir, name_copy);
    free(name_copy);
    return out_path;
}

/* ------------------------------------------------------------------
 * Основная экспортируемая функция
 * ------------------------------------------------------------------ */
void process_markdown_files(const char *input_dir, const char *output_dir) {
    ensure_dir(output_dir);

    unigram_context uni;
    bigram_context bi;
    trigram_context tri;
    init_unigram(&uni);
    init_bigram(&bi);
    init_trigram(&tri);

    DIR *dir = opendir(input_dir);
    if (!dir) {
        perror("opendir");
        free_unigram(&uni);
        free_bigram(&bi);
        free_trigram(&tri);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len < 4) continue;
        if (strcasecmp(name + len - 3, ".md") != 0) continue;

        size_t in_len = strlen(input_dir) + len + 2;
        char *in_path = malloc(in_len);
        if (!in_path) { perror("malloc"); continue; }
        snprintf(in_path, in_len, "%s/%s", input_dir, name);

        FILE *in_file = fopen(in_path, "r");
        free(in_path);
        if (!in_file) { perror("fopen input"); continue; }

        char *out_path = make_output_path(output_dir, name);
        printf("Processed: %s -> %s\n", name, out_path);

        FILE *out_file = fopen(out_path, "w");
        free(out_path);
        if (!out_file) {
            perror("fopen output");
            fclose(in_file);
            continue;
        }

        parse_file(in_file, out_file, &uni, &bi, &tri);
        fclose(in_file);
        fclose(out_file);
    }

    closedir(dir);

    // Сохраняем глобальные таблицы
    write_prob_table_unigram(&uni, "output/global_unigram.txt");
    write_prob_table_bigram(&bi, &uni, "output/global_bigram.txt");
    write_prob_table_trigram(&tri, &uni, "output/global_trigram.txt");
    printf("Global tables saved in %s/\n", output_dir);

    free_unigram(&uni);
    free_bigram(&bi);
    free_trigram(&tri);
}