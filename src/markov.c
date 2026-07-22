/**
 * @file markov.c
 * @brief Реализация генератора текста на основе n-грамм.
 * 
 * @defgroup markov_globals Глобальные переменные
 * @{
 * Глобальные контексты для хранения загруженных таблиц и флаг их загрузки,
 * а также список стоп-слов.
 * @}
 * @defgroup markov_helpers Вспомогательные функции
 * @{
 * Функции загрузки таблиц из файлов и выбора следующего токена с учётом
 * температуры и фильтрации стоп-слов.
 * @}
 * @defgroup markov_api Публичные функции
 * @{
 * Реализация API, описанного в markov.h.
 * @}
 */

#include "markov.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#define MAX_ATTEMPTS 10   /**< Максимальное число попыток выбрать не-стоп-слово */

/* ============================================================
 * Группа: markov_globals
 * ============================================================ */
/** @addtogroup markov_globals
 * @{ */

/** Глобальный контекст униграмм */
static unigram_context g_uni;
/** Глобальный контекст биграмм */
static bigram_context g_bi;
/** Глобальный контекст триграмм */
static trigram_context g_tri;
/** Флаг: загружены ли таблицы */
static int tables_loaded = 0;

/** Массив стоп-слов */
static char **stopwords = NULL;
/** Количество стоп-слов */
static int stopwords_count = 0;

/** @} */ /* end of markov_globals */

/* ============================================================
 * Прототипы внутренних функций
 * ============================================================ */
static int load_unigram_table(unigram_context *uni, const char *filename);
static int load_bigram_table(bigram_context *bi, unigram_context *uni, const char *filename);
static int load_trigram_table(trigram_context *tri, unigram_context *uni, const char *filename);
static int is_stopword(const char *word);
static char* select_next(void *trans_list, int is_bigram, int is_trigram, double temperature);

/* ============================================================
 * Группа: markov_helpers (реализация)
 * ============================================================ */
/** @addtogroup markov_helpers
 * @{ */

/* ---------- Загрузка таблиц из файлов ---------- */

static int load_unigram_table(unigram_context *uni, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("load_unigram_table");
        return -1;
    }

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (strlen(line) == 0) continue;

        char *arrow = strstr(line, " -> ");
        if (!arrow) continue;
        *arrow = '\0';
        char *word = line;
        int from_idx = add_word_unigram(uni, word);

        char *rest = arrow + 4;
        char *token = strtok(rest, ",");
        while (token) {
            while (isspace(*token)) token++;
            char *open = strchr(token, '(');
            char *close = strchr(token, ')');
            if (open && close) {
                *open = '\0';
                *close = '\0';
                char *target_word = token;
                char *end = target_word + strlen(target_word) - 1;
                while (end > target_word && isspace(*end)) { *end = '\0'; end--; }
                int count = atoi(open + 1);
                int to_idx = add_word_unigram(uni, target_word);
                add_transition_unigram(uni, from_idx, to_idx);
                // Устанавливаем точное значение счётчика (из файла)
                transition *t = uni->words[from_idx].transitions;
                while (t) {
                    if (t->target_index == to_idx) {
                        t->count = count;
                        break;
                    }
                    t = t->next;
                }
            }
            token = strtok(NULL, ",");
        }
    }
    fclose(f);
    return 0;
}

static int load_bigram_table(bigram_context *bi, unigram_context *uni, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("load_bigram_table");
        return -1;
    }

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (strlen(line) == 0) continue;

        char *arrow = strstr(line, " -> ");
        if (!arrow) continue;
        *arrow = '\0';
        char *key = line;
        int state_idx = add_state_bigram(bi, key);

        char *rest = arrow + 4;
        char *token = strtok(rest, ",");
        while (token) {
            while (isspace(*token)) token++;
            char *open = strchr(token, '(');
            char *close = strchr(token, ')');
            if (open && close) {
                *open = '\0';
                *close = '\0';
                char *target_word = token;
                char *end = target_word + strlen(target_word) - 1;
                while (end > target_word && isspace(*end)) { *end = '\0'; end--; }
                int count = atoi(open + 1);
                int to_idx = find_word_unigram(uni, target_word);
                if (to_idx == -1) {
                    to_idx = add_word_unigram(uni, target_word);
                }
                add_transition_bigram(bi, state_idx, to_idx);
                bigram_transition *t = bi->states[state_idx].transitions;
                while (t) {
                    if (t->target_index == to_idx) {
                        t->count = count;
                        break;
                    }
                    t = t->next;
                }
            }
            token = strtok(NULL, ",");
        }
    }
    fclose(f);
    return 0;
}

static int load_trigram_table(trigram_context *tri, unigram_context *uni, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("load_trigram_table");
        return -1;
    }

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (strlen(line) == 0) continue;

        char *arrow = strstr(line, " -> ");
        if (!arrow) continue;
        *arrow = '\0';
        char *key = line;
        int state_idx = add_state_trigram(tri, key);

        char *rest = arrow + 4;
        char *token = strtok(rest, ",");
        while (token) {
            while (isspace(*token)) token++;
            char *open = strchr(token, '(');
            char *close = strchr(token, ')');
            if (open && close) {
                *open = '\0';
                *close = '\0';
                char *target_word = token;
                char *end = target_word + strlen(target_word) - 1;
                while (end > target_word && isspace(*end)) { *end = '\0'; end--; }
                int count = atoi(open + 1);
                int to_idx = find_word_unigram(uni, target_word);
                if (to_idx == -1) {
                    to_idx = add_word_unigram(uni, target_word);
                }
                add_transition_trigram(tri, state_idx, to_idx);
                trigram_transition *t = tri->states[state_idx].transitions;
                while (t) {
                    if (t->target_index == to_idx) {
                        t->count = count;
                        break;
                    }
                    t = t->next;
                }
            }
            token = strtok(NULL, ",");
        }
    }
    fclose(f);
    return 0;
}

/* ---------- Стоп-слова ---------- */

/**
 * @brief Проверяет, является ли слово стоп-словом.
 * @param word строка
 * @return 1 если стоп-слово, иначе 0
 */
static int is_stopword(const char *word) {
    for (int i = 0; i < stopwords_count; i++) {
        if (strcmp(word, stopwords[i]) == 0) return 1;
    }
    return 0;
}

/* ---------- Выбор следующего токена ---------- */

/**
 * @brief Выбирает следующий токен на основе распределения переходов с температурой.
 * @param trans_list  указатель на список переходов (transition*, bigram_transition* или trigram_transition*)
 * @param is_bigram   1 если список биграммный
 * @param is_trigram  1 если список триграммный
 * @param temperature параметр температуры
 * @return динамически выделенная строка с токеном (или NULL при ошибке/отсутствии переходов)
 */
static char* select_next(void *trans_list, int is_bigram, int is_trigram, double temperature) {
    int count = 0;
    if (is_trigram) {
        trigram_transition *t = (trigram_transition*)trans_list;
        while (t) { count++; t = t->next; }
    } else if (is_bigram) {
        bigram_transition *t = (bigram_transition*)trans_list;
        while (t) { count++; t = t->next; }
    } else {
        transition *t = (transition*)trans_list;
        while (t) { count++; t = t->next; }
    }
    if (count == 0) return NULL;

    double *weights = malloc(count * sizeof(double));
    int *indices = malloc(count * sizeof(int));
    if (!weights || !indices) {
        free(weights);
        free(indices);
        return NULL;
    }

    int i = 0;
    double sum = 0.0;
    if (is_trigram) {
        trigram_transition *t = (trigram_transition*)trans_list;
        while (t) {
            double w = (temperature > 0.0) ? pow((double)t->count, 1.0 / temperature) : (double)t->count;
            weights[i] = w;
            indices[i] = t->target_index;
            sum += w;
            i++;
            t = t->next;
        }
    } else if (is_bigram) {
        bigram_transition *t = (bigram_transition*)trans_list;
        while (t) {
            double w = (temperature > 0.0) ? pow((double)t->count, 1.0 / temperature) : (double)t->count;
            weights[i] = w;
            indices[i] = t->target_index;
            sum += w;
            i++;
            t = t->next;
        }
    } else {
        transition *t = (transition*)trans_list;
        while (t) {
            double w = (temperature > 0.0) ? pow((double)t->count, 1.0 / temperature) : (double)t->count;
            weights[i] = w;
            indices[i] = t->target_index;
            sum += w;
            i++;
            t = t->next;
        }
    }

    double r = (double)rand() / RAND_MAX * sum;
    double cum = 0.0;
    char *result = NULL;
    for (int j = 0; j < count; j++) {
        cum += weights[j];
        if (r <= cum) {
            result = strdup(g_uni.words[indices[j]].word);
            break;
        }
    }
    if (!result && count > 0) {
        result = strdup(g_uni.words[indices[count-1]].word);
    }
    free(weights);
    free(indices);
    return result;
}

/** @} */ /* end of markov_helpers */

/* ============================================================
 * Группа: markov_api (реализация)
 * ============================================================ */
/** @addtogroup markov_api
 * @{ */

int markov_load_tables(const char *unigram_file, const char *bigram_file, const char *trigram_file) {
    // Если таблицы уже загружены, освобождаем их (избегаем утечки)
    if (tables_loaded) {
        markov_free();
    }

    // Инициализируем глобальные контексты
    init_unigram(&g_uni);
    init_bigram(&g_bi);
    init_trigram(&g_tri);

    // Загружаем униграммы
    if (load_unigram_table(&g_uni, unigram_file) != 0) {
        free_unigram(&g_uni);
        free_bigram(&g_bi);
        free_trigram(&g_tri);
        return -1;
    }

    // Загружаем биграммы (требуется g_uni для поиска индексов слов)
    if (load_bigram_table(&g_bi, &g_uni, bigram_file) != 0) {
        free_unigram(&g_uni);
        free_bigram(&g_bi);
        free_trigram(&g_tri);
        return -1;
    }

    // Загружаем триграммы
    if (load_trigram_table(&g_tri, &g_uni, trigram_file) != 0) {
        free_unigram(&g_uni);
        free_bigram(&g_bi);
        free_trigram(&g_tri);
        return -1;
    }

    tables_loaded = 1;
    return 0;
}

int markov_load_stopwords(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (strlen(line) == 0) continue;

        char **new_stopwords = realloc(stopwords, (stopwords_count + 1) * sizeof(char *));
        if (!new_stopwords) {
            fclose(f);
            return -1;
        }
        stopwords = new_stopwords;
        stopwords[stopwords_count] = strdup(line);
        if (!stopwords[stopwords_count]) {
            fclose(f);
            return -1;
        }
        stopwords_count++;
    }
    fclose(f);
    return 0;
}

int markov_generate_ex(char *buffer, size_t buf_size, const MarkovGenOptions *options) {
    if (!tables_loaded) {
        fprintf(stderr, "Tables not loaded. Call markov_load_tables first.\n");
        return -1;
    }
    if (!buffer || buf_size == 0 || !options) return -1;

    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }

    buffer[0] = '\0';
    size_t used = 0;

    // Начальные токены
    const char **start = options->start_tokens;
    if (!start || !start[0]) {
        static const char *default_start[] = {"<BOS>", NULL};
        start = default_start;
    }

    // Состояние для контекста
    char *prev1 = NULL;
    char *prev2 = NULL;
    char *current = NULL;

    // Добавляем начальные токены в буфер
    for (int i = 0; start[i] != NULL; i++) {
        size_t tok_len = strlen(start[i]);
        if (used + tok_len + 2 > buf_size) break;
        if (i > 0) {
            buffer[used++] = ' ';
        }
        memcpy(buffer + used, start[i], tok_len);
        used += tok_len;
        // Сохраняем последние токены для контекста
        if (i >= 1) {
            if (prev1) free(prev1);
            prev1 = current;
            current = strdup(start[i]);
        } else {
            current = strdup(start[i]);
        }
        if (i >= 2) {
            if (prev2) free(prev2);
            prev2 = prev1;
            prev1 = current;
            current = strdup(start[i]);
        }
    }
    if (used > 0) buffer[used++] = ' ';
    int generated = 0;

    for (int step = 0; step < options->max_tokens; step++) {
        char *next = NULL;

        // Триграмма
        if (options->order == 2 && prev1 && prev2) {
            char key[3 * MAX_TOKEN_LEN + 3];
            snprintf(key, sizeof(key), "%s|%s|%s", prev2, prev1, current);
            int state_idx = find_state_trigram(&g_tri, key);
            if (state_idx != -1) {
                next = select_next(g_tri.states[state_idx].transitions, 0, 1, options->temperature);
                if (options->use_stopwords && next) {
                    for (int a = 0; a < MAX_ATTEMPTS && is_stopword(next); a++) {
                        free(next);
                        next = select_next(g_tri.states[state_idx].transitions, 0, 1, options->temperature);
                        if (!next) break;
                    }
                }
            }
        }

        // Биграмма
        if (!next && options->order >= 1 && prev1) {
            char key[2 * MAX_TOKEN_LEN + 2];
            snprintf(key, sizeof(key), "%s|%s", prev1, current);
            int state_idx = find_state_bigram(&g_bi, key);
            if (state_idx != -1) {
                next = select_next(g_bi.states[state_idx].transitions, 1, 0, options->temperature);
                if (options->use_stopwords && next) {
                    for (int a = 0; a < MAX_ATTEMPTS && is_stopword(next); a++) {
                        free(next);
                        next = select_next(g_bi.states[state_idx].transitions, 1, 0, options->temperature);
                        if (!next) break;
                    }
                }
            }
        }

        // Униграмма
        if (!next) {
            int idx = find_word_unigram(&g_uni, current);
            if (idx != -1) {
                next = select_next(g_uni.words[idx].transitions, 0, 0, options->temperature);
                if (options->use_stopwords && next) {
                    for (int a = 0; a < MAX_ATTEMPTS && is_stopword(next); a++) {
                        free(next);
                        next = select_next(g_uni.words[idx].transitions, 0, 0, options->temperature);
                        if (!next) break;
                    }
                }
            }
        }

        if (!next) break;

        // Добавляем в буфер
        size_t tok_len = strlen(next);
        if (used + tok_len + 2 > buf_size) {
            free(next);
            break;
        }
        if (used > 0) buffer[used++] = ' ';
        memcpy(buffer + used, next, tok_len);
        used += tok_len;

        // Обновляем состояние
        free(prev2);
        prev2 = prev1;
        prev1 = current;
        current = next;
        generated++;
    }

    if (used > 0 && buffer[used-1] == ' ') buffer[used-1] = '\0';
    else buffer[used] = '\0';

    free(prev1);
    free(prev2);
    free(current);
    return generated;
}

int markov_generate(char *buffer, size_t buf_size, int max_tokens,
                    double temperature, int use_bigram, const char *start_token) {
    const char *start_arr[2] = { start_token, NULL };
    MarkovGenOptions opts = {
        .order = use_bigram ? 1 : 0,
        .temperature = temperature,
        .max_tokens = max_tokens,
        .start_tokens = start_arr,
        .use_stopwords = 0
    };
    return markov_generate_ex(buffer, buf_size, &opts);
}

void markov_free(void) {
    if (tables_loaded) {
        free_unigram(&g_uni);
        free_bigram(&g_bi);
        free_trigram(&g_tri);
        tables_loaded = 0;
    }
    for (int i = 0; i < stopwords_count; i++) {
        free(stopwords[i]);
    }
    free(stopwords);
    stopwords = NULL;
    stopwords_count = 0;
}

/** @} */ /* end of markov_api */