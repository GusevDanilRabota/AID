/**
 * @file ngram_model.h
 * @brief Модуль для работы с n-граммами (уни-, би-, триграммы).
 *
 * Предоставляет структуры данных и функции для хранения статистики переходов
 * между словами/токенами. Используется парсерами (md_parser, c_parser)
 * и генератором (markov).
 */

#ifndef NGRAM_MODEL_H
#define NGRAM_MODEL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Константы
 * ============================================================ */
#define NGRAM_HASH_SIZE 1024   /**< Размер хеш-таблицы для ускорения поиска */
#define MAX_TOKEN_LEN   256    /**< Максимальная длина токена (слова, пунктуации, спецсимвола) */

/* ============================================================
 * Униграммы (первый порядок)
 * ============================================================ */

/**
 * @brief Элемент списка переходов для униграммы.
 */
typedef struct transition {
    int target_index;          /**< Индекс целевого слова в массиве words */
    int count;                 /**< Количество переходов к этому целевому слову */
    struct transition *next;   /**< Указатель на следующий переход в списке */
} transition;

/**
 * @brief Запись о слове (униграмма).
 */
typedef struct word_entry {
    char *word;                /**< Строка слова */
    transition *transitions;   /**< Список переходов из этого слова (связанный список) */
} word_entry;

/**
 * @brief Элемент хеш-таблицы для быстрого поиска индекса слова по строке.
 */
typedef struct word_node {
    char *word;                /**< Слово (указатель на ту же строку, что и в word_entry) */
    int index;                 /**< Индекс в массиве words */
    struct word_node *next;    /**< Следующий элемент в цепочке коллизий */
} word_node;

/**
 * @brief Контекст униграмм (содержит все данные).
 */
typedef struct {
    word_entry *words;         /**< Массив всех уникальных слов */
    int word_count;            /**< Текущее количество уникальных слов */
    int word_capacity;         /**< Выделенная ёмкость массива words */
    word_node **hash_table;    /**< Хеш-таблица для поиска индекса по строке (размер NGRAM_HASH_SIZE) */
} unigram_context;

/**
 * @brief Инициализация контекста униграмм.
 * @param ctx Указатель на контекст
 */
void init_unigram(unigram_context *ctx);

/**
 * @brief Освобождение памяти, занятой контекстом униграмм.
 * @param ctx Указатель на контекст
 */
void free_unigram(unigram_context *ctx);

/**
 * @brief Добавление нового слова (если его нет) и возврат его индекса.
 * @param ctx Контекст
 * @param s   Строка слова (копируется)
 * @return Индекс добавленного или уже существующего слова
 */
int add_word_unigram(unigram_context *ctx, const char *s);

/**
 * @brief Добавление перехода от слова from к слову to (увеличивает счётчик).
 * @param ctx  Контекст
 * @param from Индекс слова-источника
 * @param to   Индекс слова-приёмника
 */
void add_transition_unigram(unigram_context *ctx, int from, int to);

/**
 * @brief Поиск слова в хеш-таблице.
 * @param ctx Контекст
 * @param s   Строка для поиска
 * @return Индекс слова, если найдено, иначе -1
 */
int find_word_unigram(const unigram_context *ctx, const char *s);

/* ============================================================
 * Биграммы (второй порядок)
 * ============================================================ */

/**
 * @brief Элемент списка переходов для биграммы.
 */
typedef struct bigram_transition {
    int target_index;          /**< Индекс целевого слова (из униграмного контекста) */
    int count;
    struct bigram_transition *next;
} bigram_transition;

/**
 * @brief Состояние биграммы (ключ вида "слово1|слово2").
 */
typedef struct bigram_state {
    char *key;                 /**< Строковый ключ состояния */
    bigram_transition *transitions;
} bigram_state;

/**
 * @brief Элемент хеш-таблицы для биграмм.
 */
typedef struct bigram_node {
    char *key;
    int index;
    struct bigram_node *next;
} bigram_node;

/**
 * @brief Контекст биграмм.
 */
typedef struct {
    bigram_state *states;      /**< Массив состояний */
    int state_count;           /**< Текущее количество состояний */
    int state_capacity;        /**< Выделенная ёмкость массива states */
    bigram_node **hash_table;  /**< Хеш-таблица для поиска индекса состояния по ключу */
} bigram_context;

void init_bigram(bigram_context *ctx);
void free_bigram(bigram_context *ctx);
int add_state_bigram(bigram_context *ctx, const char *key);
void add_transition_bigram(bigram_context *ctx, int state_idx, int target_word_idx);
int find_state_bigram(const bigram_context *ctx, const char *key);

/* ============================================================
 * Триграммы (третий порядок)
 * ============================================================ */

typedef struct trigram_transition {
    int target_index;
    int count;
    struct trigram_transition *next;
} trigram_transition;

typedef struct trigram_state {
    char *key;                 /* "слово1|слово2|слово3" */
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

void init_trigram(trigram_context *ctx);
void free_trigram(trigram_context *ctx);
int add_state_trigram(trigram_context *ctx, const char *key);
void add_transition_trigram(trigram_context *ctx, int state_idx, int target_word_idx);
int find_state_trigram(const trigram_context *ctx, const char *key);

/* ============================================================
 * Общая хеш-функция (для всех типов ключей)
 * ============================================================ */

/**
 * @brief Хеш-функция для строк (djb2).
 * @param s Строка
 * @return Хеш-значение в диапазоне [0, NGRAM_HASH_SIZE-1]
 */
unsigned int hash_string(const char *s);

#ifdef __cplusplus
}
#endif

#endif