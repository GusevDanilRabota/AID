/**
 * @file ngram_model.c
 * @brief Реализация функций для работы с n-граммами.
 */

#include "ngram_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Хеш-функция
 * ============================================================ */
unsigned int hash_string(const char *s) {
    unsigned int h = 0;
    while (*s) {
        h = h * 31 + (unsigned char)(*s);
        s++;
    }
    return h % NGRAM_HASH_SIZE;
}

/* ============================================================
 * Униграммы
 * ============================================================ */

void init_unigram(unigram_context *ctx) {
    ctx->words = NULL;
    ctx->word_count = 0;
    ctx->word_capacity = 0;
    ctx->hash_table = calloc(NGRAM_HASH_SIZE, sizeof(word_node *));
    if (!ctx->hash_table) {
        perror("calloc");
        exit(1);
    }
}

void free_unigram(unigram_context *ctx) {
    // Освобождаем каждое слово и его переходы
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

    // Освобождаем хеш-таблицу (только сами узлы, строки уже освобождены)
    for (int i = 0; i < NGRAM_HASH_SIZE; i++) {
        word_node *n = ctx->hash_table[i];
        while (n) {
            word_node *next = n->next;
            free(n);
            n = next;
        }
    }
    free(ctx->hash_table);

    // Обнуляем структуру (необязательно, но для безопасности)
    ctx->words = NULL;
    ctx->word_count = 0;
    ctx->word_capacity = 0;
    ctx->hash_table = NULL;
}

int find_word_unigram(const unigram_context *ctx, const char *s) {
    unsigned int h = hash_string(s);
    word_node *n = ctx->hash_table[h];
    while (n) {
        if (strcmp(n->word, s) == 0)
            return n->index;
        n = n->next;
    }
    return -1;
}

int add_word_unigram(unigram_context *ctx, const char *s) {
    int idx = find_word_unigram(ctx, s);
    if (idx != -1)
        return idx;

    // Проверяем, нужно ли расширять массив
    if (ctx->word_count >= ctx->word_capacity) {
        ctx->word_capacity = (ctx->word_capacity == 0) ? 16 : ctx->word_capacity * 2;
        ctx->words = realloc(ctx->words, ctx->word_capacity * sizeof(word_entry));
        if (!ctx->words) {
            perror("realloc");
            exit(1);
        }
    }

    // Копируем строку
    char *copy = malloc(strlen(s) + 1);
    if (!copy) {
        perror("malloc");
        exit(1);
    }
    strcpy(copy, s);

    // Заполняем запись
    ctx->words[ctx->word_count].word = copy;
    ctx->words[ctx->word_count].transitions = NULL;

    // Вставляем в хеш-таблицу
    unsigned int h = hash_string(s);
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

void add_transition_unigram(unigram_context *ctx, int from, int to) {
    // Ищем, есть ли уже такой переход
    transition *t = ctx->words[from].transitions;
    while (t) {
        if (t->target_index == to) {
            t->count++;
            return;
        }
        t = t->next;
    }

    // Если нет, создаём новый
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

/* ============================================================
 * Биграммы
 * ============================================================ */

void init_bigram(bigram_context *ctx) {
    ctx->states = NULL;
    ctx->state_count = 0;
    ctx->state_capacity = 0;
    ctx->hash_table = calloc(NGRAM_HASH_SIZE, sizeof(bigram_node *));
    if (!ctx->hash_table) {
        perror("calloc");
        exit(1);
    }
}

void free_bigram(bigram_context *ctx) {
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

    for (int i = 0; i < NGRAM_HASH_SIZE; i++) {
        bigram_node *n = ctx->hash_table[i];
        while (n) {
            bigram_node *next = n->next;
            free(n);
            n = next;
        }
    }
    free(ctx->hash_table);

    ctx->states = NULL;
    ctx->state_count = 0;
    ctx->state_capacity = 0;
    ctx->hash_table = NULL;
}

int find_state_bigram(const bigram_context *ctx, const char *key) {
    unsigned int h = hash_string(key);
    bigram_node *n = ctx->hash_table[h];
    while (n) {
        if (strcmp(n->key, key) == 0)
            return n->index;
        n = n->next;
    }
    return -1;
}

int add_state_bigram(bigram_context *ctx, const char *key) {
    int idx = find_state_bigram(ctx, key);
    if (idx != -1)
        return idx;

    if (ctx->state_count >= ctx->state_capacity) {
        ctx->state_capacity = (ctx->state_capacity == 0) ? 16 : ctx->state_capacity * 2;
        ctx->states = realloc(ctx->states, ctx->state_capacity * sizeof(bigram_state));
        if (!ctx->states) {
            perror("realloc");
            exit(1);
        }
    }

    char *copy = malloc(strlen(key) + 1);
    if (!copy) {
        perror("malloc");
        exit(1);
    }
    strcpy(copy, key);

    ctx->states[ctx->state_count].key = copy;
    ctx->states[ctx->state_count].transitions = NULL;

    unsigned int h = hash_string(key);
    bigram_node *node = malloc(sizeof(bigram_node));
    if (!node) {
        perror("malloc");
        exit(1);
    }
    node->key = copy;
    node->index = ctx->state_count;
    node->next = ctx->hash_table[h];
    ctx->hash_table[h] = node;

    return ctx->state_count++;
}

void add_transition_bigram(bigram_context *ctx, int state_idx, int target_word_idx) {
    bigram_transition *t = ctx->states[state_idx].transitions;
    while (t) {
        if (t->target_index == target_word_idx) {
            t->count++;
            return;
        }
        t = t->next;
    }

    bigram_transition *new_t = malloc(sizeof(bigram_transition));
    if (!new_t) {
        perror("malloc");
        exit(1);
    }
    new_t->target_index = target_word_idx;
    new_t->count = 1;
    new_t->next = ctx->states[state_idx].transitions;
    ctx->states[state_idx].transitions = new_t;
}

/* ============================================================
 * Триграммы (аналогично биграммам)
 * ============================================================ */

void init_trigram(trigram_context *ctx) {
    ctx->states = NULL;
    ctx->state_count = 0;
    ctx->state_capacity = 0;
    ctx->hash_table = calloc(NGRAM_HASH_SIZE, sizeof(trigram_node *));
    if (!ctx->hash_table) {
        perror("calloc");
        exit(1);
    }
}

void free_trigram(trigram_context *ctx) {
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

    for (int i = 0; i < NGRAM_HASH_SIZE; i++) {
        trigram_node *n = ctx->hash_table[i];
        while (n) {
            trigram_node *next = n->next;
            free(n);
            n = next;
        }
    }
    free(ctx->hash_table);

    ctx->states = NULL;
    ctx->state_count = 0;
    ctx->state_capacity = 0;
    ctx->hash_table = NULL;
}

int find_state_trigram(const trigram_context *ctx, const char *key) {
    unsigned int h = hash_string(key);
    trigram_node *n = ctx->hash_table[h];
    while (n) {
        if (strcmp(n->key, key) == 0)
            return n->index;
        n = n->next;
    }
    return -1;
}

int add_state_trigram(trigram_context *ctx, const char *key) {
    int idx = find_state_trigram(ctx, key);
    if (idx != -1)
        return idx;

    if (ctx->state_count >= ctx->state_capacity) {
        ctx->state_capacity = (ctx->state_capacity == 0) ? 16 : ctx->state_capacity * 2;
        ctx->states = realloc(ctx->states, ctx->state_capacity * sizeof(trigram_state));
        if (!ctx->states) {
            perror("realloc");
            exit(1);
        }
    }

    char *copy = malloc(strlen(key) + 1);
    if (!copy) {
        perror("malloc");
        exit(1);
    }
    strcpy(copy, key);

    ctx->states[ctx->state_count].key = copy;
    ctx->states[ctx->state_count].transitions = NULL;

    unsigned int h = hash_string(key);
    trigram_node *node = malloc(sizeof(trigram_node));
    if (!node) {
        perror("malloc");
        exit(1);
    }
    node->key = copy;
    node->index = ctx->state_count;
    node->next = ctx->hash_table[h];
    ctx->hash_table[h] = node;

    return ctx->state_count++;
}

void add_transition_trigram(trigram_context *ctx, int state_idx, int target_word_idx) {
    trigram_transition *t = ctx->states[state_idx].transitions;
    while (t) {
        if (t->target_index == target_word_idx) {
            t->count++;
            return;
        }
        t = t->next;
    }

    trigram_transition *new_t = malloc(sizeof(trigram_transition));
    if (!new_t) {
        perror("malloc");
        exit(1);
    }
    new_t->target_index = target_word_idx;
    new_t->count = 1;
    new_t->next = ctx->states[state_idx].transitions;
    ctx->states[state_idx].transitions = new_t;
}