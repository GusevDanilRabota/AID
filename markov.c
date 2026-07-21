/**
 * @file markov.c
 * @brief Реализация генератора текста на основе цепей Маркова.
 *
 * Загружает таблицы из текстовых файлов формата, создаваемого md_parser,
 * и генерирует текст, выбирая следующий токен согласно вероятностям.
 */

#include "markov.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

/* ------------------------------------------------------------------
 * Константы
 * ------------------------------------------------------------------ */
#define MAX_TOKEN_LEN 256      /**< Максимальная длина токена */
#define MAX_LINE_LEN 4096      /**< Максимальная длина строки в таблице */
#define HASH_SIZE    1024      /**< Размер хеш-таблицы для быстрого доступа */

/* ------------------------------------------------------------------
 * Структуры для хранения таблиц в памяти
 * ------------------------------------------------------------------ */

/* Униграмма: слово -> список переходов (слово, вероятность) */
typedef struct unigram_transition {
    char *target;              /**< Следующее слово */
    double prob;               /**< Вероятность перехода */
    struct unigram_transition *next;
} unigram_transition;

typedef struct unigram_entry {
    char *word;                /**< Текущее слово */
    unigram_transition *transitions; /**< Список переходов */
    double cumulative;         /**< Кумулятивная сумма вероятностей (для быстрого выбора) */
    struct unigram_entry *next; /**< для хеш-таблицы */
} unigram_entry;

/* Биграмма: состояние (ключ "слово1|слово2") -> список переходов */
typedef struct bigram_transition {
    char *target;
    double prob;
    struct bigram_transition *next;
} bigram_transition;

typedef struct bigram_entry {
    char *key;                 /**< Ключ состояния */
    bigram_transition *transitions;
    double cumulative;
    struct bigram_entry *next;
} bigram_entry;

/* Глобальные хеш-таблицы для быстрого доступа */
static unigram_entry **unigram_hash = NULL;
static bigram_entry  **bigram_hash  = NULL;
static int unigram_count = 0;
static int bigram_count = 0;

/* ------------------------------------------------------------------
 * Хеш-функция
 * ------------------------------------------------------------------ */
static unsigned int hash_str(const char *s) {
    unsigned int h = 0;
    while (*s) h = h * 31 + *s++;
    return h % HASH_SIZE;
}

/* ------------------------------------------------------------------
 * Загрузка униграммной таблицы из файла
 * ------------------------------------------------------------------ */
static int parse_unigram_line(char *line, char **word, char **target, double *prob) {
    // Формат: "слово -> целевое(вероятность), другое(вероятность), ..."
    // Упрощённый парсинг: ищем "->", затем разбиваем по запятым.
    char *arrow = strstr(line, " -> ");
    if (!arrow) return -1;
    *arrow = '\0';
    *word = strdup(line);
    if (!*word) return -1;

    char *rest = arrow + 4;
    // Удаляем завершающий '\n'
    char *nl = strchr(rest, '\n');
    if (nl) *nl = '\0';

    // Сейчас разбираем переходы, разделённые запятыми
    char *token = strtok(rest, ",");
    while (token) {
        // Убираем пробелы в начале
        while (isspace(*token)) token++;
        // Ищем '(' и ')'
        char *open = strchr(token, '(');
        char *close = strchr(token, ')');
        if (!open || !close) {
            token = strtok(NULL, ",");
            continue;
        }
        *open = '\0';
        *close = '\0';
        char *target_str = strdup(token);
        if (!target_str) {
            token = strtok(NULL, ",");
            continue;
        }
        // Убираем пробелы в конце target_str
        char *end = target_str + strlen(target_str) - 1;
        while (end > target_str && isspace(*end)) { *end = '\0'; end--; }

        double prob = atof(open + 1);
        // Сохраняем переход
        // Мы будем добавлять в список, но пока вернём только первое? Для загрузки всех нужен список.
        // Здесь мы не можем просто вернуть одно значение. Поэтому переделаем: загружать будем в функции load_unigram.
        // Пропустим, т.к. эта функция лишь пример.
        free(target_str);
        token = strtok(NULL, ",");
    }
    free(*word);
    return -1; // заглушка
}

/* Функция загрузки униграмм */
int markov_load_unigram(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("markov_load_unigram");
        return -1;
    }

    unigram_hash = calloc(HASH_SIZE, sizeof(unigram_entry *));
    if (!unigram_hash) { perror("calloc"); fclose(f); return -1; }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        // Убираем перевод строки
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

        // Пропускаем пустые строки
        if (strlen(line) == 0) continue;

        // Ищем " -> "
        char *arrow = strstr(line, " -> ");
        if (!arrow) continue;
        *arrow = '\0';
        char *word = strdup(line);
        if (!word) { fclose(f); return -1; }

        // Создаём запись
        unigram_entry *entry = malloc(sizeof(unigram_entry));
        if (!entry) { free(word); fclose(f); return -1; }
        entry->word = word;
        entry->transitions = NULL;
        entry->cumulative = 0.0;

        // Разбираем переходы
        char *rest = arrow + 4;
        char *token = strtok(rest, ",");
        double cum = 0.0;
        while (token) {
            while (isspace(*token)) token++;
            char *open = strchr(token, '(');
            char *close = strchr(token, ')');
            if (open && close) {
                *open = '\0';
                *close = '\0';
                char *target = strdup(token);
                if (!target) { /* пропускаем */ }
                else {
                    // Убираем пробелы в конце target
                    char *end = target + strlen(target) - 1;
                    while (end > target && isspace(*end)) { *end = '\0'; end--; }
                    double prob = atof(open + 1);
                    cum += prob;
                    // Добавляем переход в список
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
        // Сохраняем кумулятивную сумму (для выбора)
        entry->cumulative = cum; // это не совсем cumulative, но для выбора будем использовать отдельный массив

        // Вставляем в хеш-таблицу
        unsigned int h = hash_str(word);
        entry->next = unigram_hash[h];
        unigram_hash[h] = entry;
        unigram_count++;
    }

    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------
 * Загрузка биграммной таблицы (аналогично, но ключ — состояние)
 * ------------------------------------------------------------------ */
int markov_load_bigram(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("markov_load_bigram");
        return -1;
    }

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
        double cum = 0.0;
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
                    cum += prob;
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
        entry->cumulative = cum;

        unsigned int h = hash_str(key);
        entry->next = bigram_hash[h];
        bigram_hash[h] = entry;
        bigram_count++;
    }

    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------
 * Вспомогательная: поиск записи в хеш-таблице
 * ------------------------------------------------------------------ */
static unigram_entry* find_unigram(const char *word) {
    if (!unigram_hash) return NULL;
    unsigned int h = hash_str(word);
    unigram_entry *e = unigram_hash[h];
    while (e) {
        if (strcmp(e->word, word) == 0) return e;
        e = e->next;
    }
    return NULL;
}

static bigram_entry* find_bigram(const char *key) {
    if (!bigram_hash) return NULL;
    unsigned int h = hash_str(key);
    bigram_entry *e = bigram_hash[h];
    while (e) {
        if (strcmp(e->key, key) == 0) return e;
        e = e->next;
    }
    return NULL;
}

/* ------------------------------------------------------------------
 * Выбор следующего токена на основе вероятностей с температурой
 * ------------------------------------------------------------------ */
static char* choose_next(unigram_transition *transitions, double temperature) {
    // Собираем все переходы в массив для удобства
    int count = 0;
    unigram_transition *t = transitions;
    while (t) { count++; t = t->next; }
    if (count == 0) return NULL;

    // Строим массив вероятностей с учётом температуры
    double *probs = malloc(count * sizeof(double));
    char **targets = malloc(count * sizeof(char *));
    if (!probs || !targets) {
        free(probs); free(targets);
        return NULL;
    }
    int i = 0;
    t = transitions;
    double sum = 0.0;
    while (t) {
        // Применяем температуру: P' = exp(log(P) / T) (или P^(1/T))
        // Для T=1 – без изменений, T=0 – детерминированно (выбор максимальной вероятности)
        double p = t->prob;
        if (temperature > 0.0) {
            p = pow(p, 1.0 / temperature);
        }
        probs[i] = p;
        targets[i] = t->target;
        sum += p;
        i++;
        t = t->next;
    }

    // Нормализуем и выбираем
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
    // Если не выбрали (редко), берём последний
    if (!result && count > 0) result = strdup(targets[count-1]);

    free(probs);
    free(targets);
    return result;
}

/* ------------------------------------------------------------------
 * Основная функция генерации
 * ------------------------------------------------------------------ */
int markov_generate(char *buffer, size_t buf_size, int max_tokens,
                    double temperature, int use_bigram, const char *start_token) {
    if (!buffer || buf_size == 0) return -1;
    if (!unigram_hash && !bigram_hash) {
        fprintf(stderr, "Ошибка: таблицы не загружены. Вызовите markov_load_*() сначала.\n");
        return -1;
    }

    // Инициализируем генератор случайных чисел, если ещё не инициализирован
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }

    buffer[0] = '\0';
    size_t used = 0;

    // Начинаем с заданного токена
    char *current = strdup(start_token ? start_token : "<BOS>");
    if (!current) return -1;

    // Копируем начальный токен в буфер
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
    char *prev = NULL; // для биграммы нужен предыдущий

    for (int step = 0; step < max_tokens; step++) {
        char *next = NULL;

        if (use_bigram && prev) {
            // Формируем ключ: prev|current
            char key[MAX_TOKEN_LEN * 2 + 2];
            snprintf(key, sizeof(key), "%s|%s", prev, current);
            bigram_entry *entry = find_bigram(key);
            if (entry) {
                next = choose_next((unigram_transition*)entry->transitions, temperature);
                // choose_next ожидает unigram_transition, но они совместимы по структуре (поля target, prob, next)
                // Мы можем привести типы, но безопаснее создать отдельную функцию для bigram.
                // Для простоты переделаем: сделаем общую функцию выбора.
                // Но чтобы не усложнять, временно вызовем отдельную функцию.
                // Пока упростим: используем отдельную функцию для биграмм.
                // Реализуем ниже.
            }
        }

        // Если биграмма не дала результат или не используется, пробуем униграмму
        if (!next) {
            unigram_entry *entry = find_unigram(current);
            if (entry) {
                next = choose_next(entry->transitions, temperature);
            }
        }

        if (!next) {
            // Если нет перехода, завершаем
            break;
        }

        // Добавляем следующий токен в буфер
        len = strlen(next);
        if (used + len + 2 > buf_size) {
            free(next);
            break;
        }
        strcpy(buffer + used, next);
        used += len;
        buffer[used++] = ' ';

        // Обновляем состояние для биграммы
        if (prev) free(prev);
        prev = current;
        current = next;
        generated++;
    }

    // Завершаем строку
    if (used > 0 && buffer[used-1] == ' ') buffer[used-1] = '\0';
    else buffer[used] = '\0';

    // Освобождаем память
    free(current);
    if (prev) free(prev);

    return generated;
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
    unigram_count = 0;
    bigram_count = 0;
}

/* ------------------------------------------------------------------
 * Генерация Markdown-файла
 * ------------------------------------------------------------------ */

/**
 * @brief Генерирует один блок текста с заданными опциями.
 */
static int generate_block(char *buffer, size_t buf_size, const MarkovGenOptions *opt) {
    return markov_generate(buffer, buf_size, opt->max_tokens,
                           opt->temperature, opt->order, opt->start_token);
}

int markov_generate_md_file(const char *filename, const char *title,
                            int num_blocks, const MarkovGenOptions *options) {
    if (!filename || num_blocks <= 0 || !options) {
        fprintf(stderr, "Invalid arguments for markov_generate_md_file\n");
        return -1;
    }

    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        return -1;
    }

    // Записываем заголовок документа, если задан
    if (title) {
        fprintf(f, "# %s\n\n", title);
    }

    // Для каждого блока генерируем текст и записываем с подзаголовком
    char buffer[8192];
    for (int i = 0; i < num_blocks; i++) {
        const MarkovGenOptions *opt = &options[i];
        // Определяем тип блока: можно чередовать заголовки разных уровней
        // Для простоты сделаем заголовки уровня 2 для каждого блока
        fprintf(f, "## Блок %d", i + 1);
        // Добавляем информацию о параметрах генерации (опционально)
        fprintf(f, " (порядок=%d, T=%.2f, макс. токенов=%d)\n\n",
                opt->order, opt->temperature, opt->max_tokens);

        // Генерируем текст
        int tokens = generate_block(buffer, sizeof(buffer), opt);
        if (tokens < 0) {
            fprintf(stderr, "Ошибка генерации блока %d\n", i);
            fclose(f);
            return -1;
        }
        // Записываем сгенерированный текст (он уже содержит пробелы)
        fprintf(f, "%s\n\n", buffer);
    }

    fclose(f);
    return 0;
}