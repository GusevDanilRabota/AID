/**
 * @file markov.c
 * @brief Генератор текста с поддержкой униграмм, биграмм, триграмм, стоп-слов и экспорта DOT.
 */

#include "markov.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

/* ------------------------------------------------------------------
 * Конфигурация
 * ------------------------------------------------------------------ */
#define MAX_TOKEN_LEN 256
#define MAX_LINE_LEN  4096
#define HASH_SIZE     1024
#define MAX_ATTEMPTS  10

/* ------------------------------------------------------------------
 * Определения структур (ДО объявления глобальных переменных)
 * ------------------------------------------------------------------ */

/* Униграмма */
typedef struct unigram_transition {
    char *target;
    double prob;
    struct unigram_transition *next;
} unigram_transition;

typedef struct unigram_entry {
    char *word;
    unigram_transition *transitions;
    double cumulative;          /* не используется, но оставлено для совместимости */
    struct unigram_entry *next;
} unigram_entry;

/* Биграмма */
typedef struct bigram_transition {
    char *target;
    double prob;
    struct bigram_transition *next;
} bigram_transition;

typedef struct bigram_entry {
    char *key;
    bigram_transition *transitions;
    double cumulative;
    struct bigram_entry *next;
} bigram_entry;

/* Триграмма */
typedef struct trigram_transition {
    char *target;
    double prob;
    struct trigram_transition *next;
} trigram_transition;

typedef struct trigram_entry {
    char *key;
    trigram_transition *transitions;
    double cumulative;
    struct trigram_entry *next;
} trigram_entry;

/* ------------------------------------------------------------------
 * Глобальные хеш-таблицы и счётчики
 * ------------------------------------------------------------------ */
static unigram_entry **unigram_hash = NULL;
static bigram_entry  **bigram_hash  = NULL;
static trigram_entry **trigram_hash = NULL;

static int unigram_count = 0;
static int bigram_count  = 0;
static int trigram_count = 0;

static char **stopwords = NULL;
static int stopwords_count = 0;

/* ------------------------------------------------------------------
 * Хеш-функция
 * ------------------------------------------------------------------ */
static unsigned int hash_str(const char *s) {
    unsigned int h = 0;
    while (*s) h = h * 31 + *s++;
    return h % HASH_SIZE;
}

/* ------------------------------------------------------------------
 * Загрузка униграмм
 * ------------------------------------------------------------------ */
int markov_load_unigram(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("markov_load_unigram"); return -1; }
    unigram_hash = calloc(HASH_SIZE, sizeof(unigram_entry *));
    if (!unigram_hash) { perror("calloc"); fclose(f); return -1; }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (strlen(line) == 0) continue;

        char *arrow = strstr(line, " -> ");
        if (!arrow) continue;
        *arrow = '\0';
        char *word = strdup(line);
        if (!word) { fclose(f); return -1; }

        unigram_entry *entry = malloc(sizeof(unigram_entry));
        if (!entry) { free(word); fclose(f); return -1; }
        entry->word = word;
        entry->transitions = NULL;
        entry->cumulative = 0.0;

        char *rest = arrow + 4;
        char *token = strtok(rest, ",");
        while (token) {
            while (isspace(*token)) token++;
            char *open = strchr(token, '(');
            char *close = strchr(token, ')');
            if (open && close) {
                *open = '\0';
                *close = '\0';
                char *target = strdup(token);
                if (target) {
                    char *end = target + strlen(target) - 1;
                    while (end > target && isspace(*end)) { *end = '\0'; end--; }
                    double prob = atof(open + 1);
                    unigram_transition *t = malloc(sizeof(unigram_transition));
                    if (t) {
                        t->target = target;
                        t->prob = prob;
                        t->next = entry->transitions;
                        entry->transitions = t;
                    } else {
                        free(target);
                    }
                }
            }
            token = strtok(NULL, ",");
        }

        unsigned int h = hash_str(word);
        entry->next = unigram_hash[h];
        unigram_hash[h] = entry;
        unigram_count++;
    }
    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------
 * Загрузка биграмм
 * ------------------------------------------------------------------ */
int markov_load_bigram(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("markov_load_bigram"); return -1; }
    bigram_hash = calloc(HASH_SIZE, sizeof(bigram_entry *));
    if (!bigram_hash) { perror("calloc"); fclose(f); return -1; }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (strlen(line) == 0) continue;

        char *arrow = strstr(line, " -> ");
        if (!arrow) continue;
        *arrow = '\0';
        char *key = strdup(line);
        if (!key) { fclose(f); return -1; }

        bigram_entry *entry = malloc(sizeof(bigram_entry));
        if (!entry) { free(key); fclose(f); return -1; }
        entry->key = key;
        entry->transitions = NULL;
        entry->cumulative = 0.0;

        char *rest = arrow + 4;
        char *token = strtok(rest, ",");
        while (token) {
            while (isspace(*token)) token++;
            char *open = strchr(token, '(');
            char *close = strchr(token, ')');
            if (open && close) {
                *open = '\0';
                *close = '\0';
                char *target = strdup(token);
                if (target) {
                    char *end = target + strlen(target) - 1;
                    while (end > target && isspace(*end)) { *end = '\0'; end--; }
                    double prob = atof(open + 1);
                    bigram_transition *t = malloc(sizeof(bigram_transition));
                    if (t) {
                        t->target = target;
                        t->prob = prob;
                        t->next = entry->transitions;
                        entry->transitions = t;
                    } else {
                        free(target);
                    }
                }
            }
            token = strtok(NULL, ",");
        }

        unsigned int h = hash_str(key);
        entry->next = bigram_hash[h];
        bigram_hash[h] = entry;
        bigram_count++;
    }
    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------
 * Загрузка триграмм
 * ------------------------------------------------------------------ */
int markov_load_trigram(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("markov_load_trigram"); return -1; }
    trigram_hash = calloc(HASH_SIZE, sizeof(trigram_entry *));
    if (!trigram_hash) { perror("calloc"); fclose(f); return -1; }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (strlen(line) == 0) continue;

        char *arrow = strstr(line, " -> ");
        if (!arrow) continue;
        *arrow = '\0';
        char *key = strdup(line);
        if (!key) { fclose(f); return -1; }

        trigram_entry *entry = malloc(sizeof(trigram_entry));
        if (!entry) { free(key); fclose(f); return -1; }
        entry->key = key;
        entry->transitions = NULL;
        entry->cumulative = 0.0;

        char *rest = arrow + 4;
        char *token = strtok(rest, ",");
        while (token) {
            while (isspace(*token)) token++;
            char *open = strchr(token, '(');
            char *close = strchr(token, ')');
            if (open && close) {
                *open = '\0';
                *close = '\0';
                char *target = strdup(token);
                if (target) {
                    char *end = target + strlen(target) - 1;
                    while (end > target && isspace(*end)) { *end = '\0'; end--; }
                    double prob = atof(open + 1);
                    trigram_transition *t = malloc(sizeof(trigram_transition));
                    if (t) {
                        t->target = target;
                        t->prob = prob;
                        t->next = entry->transitions;
                        entry->transitions = t;
                    } else {
                        free(target);
                    }
                }
            }
            token = strtok(NULL, ",");
        }

        unsigned int h = hash_str(key);
        entry->next = trigram_hash[h];
        trigram_hash[h] = entry;
        trigram_count++;
    }
    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------
 * Загрузка стоп-слов
 * ------------------------------------------------------------------ */
int markov_load_stopwords(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("markov_load_stopwords"); return -1; }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (strlen(line) == 0) continue;
        char **new = realloc(stopwords, (stopwords_count + 1) * sizeof(char *));
        if (!new) { perror("realloc"); fclose(f); return -1; }
        stopwords = new;
        stopwords[stopwords_count] = strdup(line);
        if (!stopwords[stopwords_count]) { perror("strdup"); fclose(f); return -1; }
        stopwords_count++;
    }
    fclose(f);
    return 0;
}

static int is_stopword(const char *word) {
    for (int i = 0; i < stopwords_count; i++) {
        if (strcmp(word, stopwords[i]) == 0) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------
 * Универсальная функция выбора следующего токена
 * ------------------------------------------------------------------ */
static char* choose_next_generic(void *transitions, double temperature, int is_bigram, int is_trigram) {
    int count = 0;
    if (is_trigram) {
        trigram_transition *t = (trigram_transition*)transitions;
        while (t) { count++; t = t->next; }
    } else if (is_bigram) {
        bigram_transition *t = (bigram_transition*)transitions;
        while (t) { count++; t = t->next; }
    } else {
        unigram_transition *t = (unigram_transition*)transitions;
        while (t) { count++; t = t->next; }
    }
    if (count == 0) return NULL;

    double *probs = malloc(count * sizeof(double));
    char **targets = malloc(count * sizeof(char *));
    if (!probs || !targets) { free(probs); free(targets); return NULL; }

    int i = 0;
    double sum = 0.0;
    if (is_trigram) {
        trigram_transition *t = (trigram_transition*)transitions;
        while (t) {
            double p = (temperature > 0.0) ? pow(t->prob, 1.0 / temperature) : t->prob;
            probs[i] = p;
            targets[i] = t->target;
            sum += p;
            i++;
            t = t->next;
        }
    } else if (is_bigram) {
        bigram_transition *t = (bigram_transition*)transitions;
        while (t) {
            double p = (temperature > 0.0) ? pow(t->prob, 1.0 / temperature) : t->prob;
            probs[i] = p;
            targets[i] = t->target;
            sum += p;
            i++;
            t = t->next;
        }
    } else {
        unigram_transition *t = (unigram_transition*)transitions;
        while (t) {
            double p = (temperature > 0.0) ? pow(t->prob, 1.0 / temperature) : t->prob;
            probs[i] = p;
            targets[i] = t->target;
            sum += p;
            i++;
            t = t->next;
        }
    }

    double r = (double)rand() / RAND_MAX * sum;
    double cum = 0.0;
    char *result = NULL;
    for (int j = 0; j < count; j++) {
        cum += probs[j];
        if (r <= cum) {
            result = strdup(targets[j]);
            break;
        }
    }
    if (!result && count > 0) result = strdup(targets[count-1]);
    free(probs);
    free(targets);
    return result;
}

/* ------------------------------------------------------------------
 * Основная функция генерации (расширенная)
 * ------------------------------------------------------------------ */
int markov_generate_ex(char *buffer, size_t buf_size, const MarkovGenOptions *options) {
    if (!buffer || buf_size == 0 || !options) return -1;
    if (!unigram_hash && !bigram_hash && !trigram_hash) {
        fprintf(stderr, "Error: no tables loaded\n");
        return -1;
    }

    static int seeded = 0;
    if (!seeded) { srand((unsigned int)time(NULL)); seeded = 1; }

    buffer[0] = '\0';
    size_t used = 0;

    const char *start = options->start_token ? options->start_token : "<BOS>";
    char *current = strdup(start);
    if (!current) return -1;

    char *prev1 = NULL;   // для биграмм и триграмм
    char *prev2 = NULL;   // для триграмм

    size_t len = strlen(current);
    if (used + len + 2 < buf_size) {
        strcpy(buffer + used, current);
        used += len;
        buffer[used++] = ' ';
    } else {
        free(current);
        return -1;
    }

    int generated = 0;
    for (int step = 0; step < options->max_tokens; step++) {
        char *next = NULL;

        // Триграмма
        if (options->order == 2 && prev1 && prev2) {
            char key[3 * MAX_TOKEN_LEN + 3];
            snprintf(key, sizeof(key), "%s|%s|%s", prev2, prev1, current);
            trigram_entry *entry = NULL;
            if (trigram_hash) {
                unsigned int h = hash_str(key);
                entry = trigram_hash[h];
                while (entry) {
                    if (strcmp(entry->key, key) == 0) break;
                    entry = entry->next;
                }
            }
            if (entry) {
                next = choose_next_generic(entry->transitions, options->temperature, 0, 1);
                if (options->use_stopwords && next) {
                    for (int attempt = 0; attempt < MAX_ATTEMPTS && is_stopword(next); attempt++) {
                        free(next);
                        next = choose_next_generic(entry->transitions, options->temperature, 0, 1);
                        if (!next) break;
                    }
                }
            }
        }

        // Биграмма (если триграмма не дала результат или не используется)
        if (!next && options->order >= 1 && prev1) {
            char key[2 * MAX_TOKEN_LEN + 2];
            snprintf(key, sizeof(key), "%s|%s", prev1, current);
            bigram_entry *entry = NULL;
            if (bigram_hash) {
                unsigned int h = hash_str(key);
                entry = bigram_hash[h];
                while (entry) {
                    if (strcmp(entry->key, key) == 0) break;
                    entry = entry->next;
                }
            }
            if (entry) {
                next = choose_next_generic(entry->transitions, options->temperature, 1, 0);
                if (options->use_stopwords && next) {
                    for (int attempt = 0; attempt < MAX_ATTEMPTS && is_stopword(next); attempt++) {
                        free(next);
                        next = choose_next_generic(entry->transitions, options->temperature, 1, 0);
                        if (!next) break;
                    }
                }
            }
        }

        // Униграмма (если ничего не сработало)
        if (!next) {
            unigram_entry *entry = NULL;
            if (unigram_hash) {
                unsigned int h = hash_str(current);
                entry = unigram_hash[h];
                while (entry) {
                    if (strcmp(entry->word, current) == 0) break;
                    entry = entry->next;
                }
            }
            if (entry) {
                next = choose_next_generic(entry->transitions, options->temperature, 0, 0);
                if (options->use_stopwords && next) {
                    for (int attempt = 0; attempt < MAX_ATTEMPTS && is_stopword(next); attempt++) {
                        free(next);
                        next = choose_next_generic(entry->transitions, options->temperature, 0, 0);
                        if (!next) break;
                    }
                }
            }
        }

        if (!next) break;

        len = strlen(next);
        if (used + len + 2 > buf_size) {
            free(next);
            break;
        }
        strcpy(buffer + used, next);
        used += len;
        buffer[used++] = ' ';

        free(prev2);
        prev2 = prev1;
        prev1 = current;
        current = next;
        generated++;
    }

    if (used > 0 && buffer[used-1] == ' ') buffer[used-1] = '\0';
    else buffer[used] = '\0';

    free(current);
    if (prev1) free(prev1);
    if (prev2) free(prev2);
    return generated;
}

/* ------------------------------------------------------------------
 * Совместимая обёртка для старого API
 * ------------------------------------------------------------------ */
int markov_generate(char *buffer, size_t buf_size, int max_tokens,
                    double temperature, int use_bigram, const char *start_token) {
    MarkovGenOptions opts = { use_bigram ? 1 : 0, temperature, max_tokens, start_token, 0 };
    return markov_generate_ex(buffer, buf_size, &opts);
}

/* ------------------------------------------------------------------
 * Форматирование строки для Markdown (замена спецтокенов)
 * ------------------------------------------------------------------ */
static void format_md_string(char *str) {
    char *patterns[][2] = {
        {"<H>", "## "},
        {"<LI>", "- "},
        {"<BLOCKQUOTE>", "> "},
        {"<EOS>", ".\n"},
        {"<BOS>", ""}
    };
    int n = sizeof(patterns) / sizeof(patterns[0]);
    for (int i = 0; i < n; i++) {
        char *pos;
        while ((pos = strstr(str, patterns[i][0])) != NULL) {
            size_t len = strlen(patterns[i][0]);
            size_t repl_len = strlen(patterns[i][1]);
            if (repl_len > len) {
                memmove(pos + repl_len, pos + len, strlen(pos + len) + 1);
                memcpy(pos, patterns[i][1], repl_len);
            } else {
                memcpy(pos, patterns[i][1], repl_len);
                memmove(pos + repl_len, pos + len, strlen(pos + len) + 1);
            }
        }
    }
}

/* ------------------------------------------------------------------
 * Генерация MD-файла
 * ------------------------------------------------------------------ */
int markov_generate_md_file(const char *filename, const char *title,
                            int num_blocks, const MarkovGenOptions *options) {
    if (!filename || num_blocks <= 0 || !options) return -1;
    FILE *f = fopen(filename, "w");
    if (!f) { perror("fopen md file"); return -1; }

    if (title) fprintf(f, "# %s\n\n", title);

    char buffer[8192];
    for (int i = 0; i < num_blocks; i++) {
        const MarkovGenOptions *opt = &options[i];
        fprintf(f, "## Block %d (order=%d, T=%.2f, max_tokens=%d)\n\n",
                i+1, opt->order, opt->temperature, opt->max_tokens);

        int tokens = markov_generate_ex(buffer, sizeof(buffer), opt);
        if (tokens < 0) { fclose(f); return -1; }
        format_md_string(buffer);
        fprintf(f, "%s\n\n", buffer);
    }
    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------
 * Экспорт графа в DOT
 * ------------------------------------------------------------------ */
int markov_export_dot(const char *filename, int order, double min_prob) {
    FILE *f = fopen(filename, "w");
    if (!f) { perror("fopen dot"); return -1; }

    fprintf(f, "digraph Markov {\n");
    fprintf(f, "  rankdir=LR;\n");
    fprintf(f, "  node [shape=box];\n");

    if (order == 0 && unigram_hash) {
        for (int i = 0; i < HASH_SIZE; i++) {
            unigram_entry *e = unigram_hash[i];
            while (e) {
                unigram_transition *t = e->transitions;
                while (t) {
                    if (t->prob >= min_prob) {
                        fprintf(f, "  \"%s\" -> \"%s\" [label=\"%.2f\"];\n",
                                e->word, t->target, t->prob);
                    }
                    t = t->next;
                }
                e = e->next;
            }
        }
    } else if (order == 1 && bigram_hash) {
        for (int i = 0; i < HASH_SIZE; i++) {
            bigram_entry *e = bigram_hash[i];
            while (e) {
                bigram_transition *t = e->transitions;
                while (t) {
                    if (t->prob >= min_prob) {
                        fprintf(f, "  \"%s\" -> \"%s\" [label=\"%.2f\"];\n",
                                e->key, t->target, t->prob);
                    }
                    t = t->next;
                }
                e = e->next;
            }
        }
    } else {
        fprintf(stderr, "Unsupported order for DOT export\n");
        fclose(f);
        return -1;
    }

    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------
 * Установка seed
 * ------------------------------------------------------------------ */
void markov_set_seed(unsigned int seed) {
    srand(seed);
}

/* ------------------------------------------------------------------
 * Освобождение памяти
 * ------------------------------------------------------------------ */
void markov_free(void) {
    if (unigram_hash) {
        for (int i = 0; i < HASH_SIZE; i++) {
            unigram_entry *e = unigram_hash[i];
            while (e) {
                unigram_entry *next = e->next;
                free(e->word);
                unigram_transition *t = e->transitions;
                while (t) {
                    unigram_transition *next_t = t->next;
                    free(t->target);
                    free(t);
                    t = next_t;
                }
                free(e);
                e = next;
            }
        }
        free(unigram_hash);
        unigram_hash = NULL;
    }
    if (bigram_hash) {
        for (int i = 0; i < HASH_SIZE; i++) {
            bigram_entry *e = bigram_hash[i];
            while (e) {
                bigram_entry *next = e->next;
                free(e->key);
                bigram_transition *t = e->transitions;
                while (t) {
                    bigram_transition *next_t = t->next;
                    free(t->target);
                    free(t);
                    t = next_t;
                }
                free(e);
                e = next;
            }
        }
        free(bigram_hash);
        bigram_hash = NULL;
    }
    if (trigram_hash) {
        for (int i = 0; i < HASH_SIZE; i++) {
            trigram_entry *e = trigram_hash[i];
            while (e) {
                trigram_entry *next = e->next;
                free(e->key);
                trigram_transition *t = e->transitions;
                while (t) {
                    trigram_transition *next_t = t->next;
                    free(t->target);
                    free(t);
                    t = next_t;
                }
                free(e);
                e = next;
            }
        }
        free(trigram_hash);
        trigram_hash = NULL;
    }
    for (int i = 0; i < stopwords_count; i++) free(stopwords[i]);
    free(stopwords);
    stopwords = NULL;
    stopwords_count = 0;
    unigram_count = bigram_count = trigram_count = 0;
}