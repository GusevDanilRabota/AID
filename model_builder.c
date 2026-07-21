/**
 * @file model_builder.c
 * @brief Реализация построения биграммной модели Маркова.
 *
 * ## Общая логика
 *
 * 1. **Первый проход** – сбор всех уникальных слов (токенов) из корпуса.
 *    Каждому слову присваивается числовой идентификатор (id). В словарь
 *    сразу добавляются служебные токены:
 *      - `<S>`  – начало предложения,
 *      - `</S>` – конец предложения,
 *      - `<UNK>` – маркер неизвестного слова.
 *
 * 2. После первого прохода выделяется память под плоский массив счётчиков
 *    биграмм размером V×V (V – размер словаря). Все элементы инициализируются нулями.
 *
 * 3. **Второй проход** – повторное чтение тех же файлов и подсчёт переходов:
 *    для каждой последовательности слов `word_a → word_b` увеличивается
 *    элемент массива `counts[id_a * V + id_b]`. Перед началом обработки
 *    файла текущим состоянием считается `<S>`, а в конце добавляется переход
 *    к `</S>`.
 *
 * 4. Полученная матрица частот преобразуется в матрицу накопленных
 *    вероятностей (cumulative probabilities). Каждая строка сжимается –
 *    сохраняются только ненулевые переходы с их накопленной вероятностью,
 *    масштабированной в диапазон 0…65535 (uint16_t). Для пустых строк
 *    записывается единственный переход в `<UNK>` с вероятностью 1.0.
 *
 * 5. Сжатые данные и словарь записываются в файлы:
 *    - `model.bin`  – бинарный файл, содержащий размер словаря, количество
 *      ненулевых элементов, смещения строк (CSR‑подобный формат) и сами записи
 *      (пары `{next_id, cum_prob}`).
 *    - `vocab.txt`  – текстовый файл в кодировке UTF‑8 с BOM, по одному слову
 *      на строку.
 *
 * Все функции возвращают коды ошибок из enum build_status_e.
 * Код 0 означает успех, любое значение от 1 до 9 – ошибку.
 */

#include "model_builder.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

/** Максимально допустимый размер словаря (ограничен 16‑битными id) */
#define MAX_VOCAB 65535U

/**
 * @brief Элемент словаря.
 */
typedef struct {
    char *word;      /**< Токен в UTF‑8 */
    uint32_t id;     /**< Его уникальный идентификатор (0…V-1) */
} vocab_entry_s;

/** Статический массив всех слов (индекс = id) */
static vocab_entry_s vocab[MAX_VOCAB];
/** Текущий размер словаря */
static uint32_t vocab_size = 0U;

/* -----------------------------------------------------------------------
 * Вспомогательные функции
 * ----------------------------------------------------------------------- */

/**
 * @brief Получить идентификатор слова; при необходимости добавить его в словарь.
 *
 * Если слово уже существует, возвращается его старый id.
 * Если слово новое и в словаре есть место, оно добавляется.
 * При переполнении возвращается UINT32_MAX (не является ошибкой всей программы).
 *
 * @param word  Строка токена (в UTF‑8).
 * @return ID слова, или UINT32_MAX при переполнении словаря или ошибке strdup.
 */
static uint32_t get_word_id(const char *word)
{
    uint32_t i;
    for (i = 0U; i < vocab_size; i++) {
        if (strcmp(vocab[i].word, word) == 0) {
            return vocab[i].id;
        }
    }

    if (vocab_size >= MAX_VOCAB) {
        return UINT32_MAX;   /* словарь полностью заполнен */
    }

    vocab[vocab_size].word = strdup(word);
    if (vocab[vocab_size].word == NULL) {
        return UINT32_MAX;   /* ошибка выделения памяти (не критично, слово будет пропущено) */
    }
    vocab[vocab_size].id = vocab_size;
    vocab_size++;
    return vocab[vocab_size - 1U].id;
}

/**
 * @brief Прочитать файл целиком в динамический буфер (бинарный режим).
 *
 * @param filename  Имя файла.
 * @return Указатель на буфер (освобождается free()), или NULL при ошибке.
 */
static char *read_file_utf8(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        return NULL;
    }

    if (fseek(f, 0L, SEEK_END) != 0) {
        (void)fclose(f);
        return NULL;
    }

    long sz = ftell(f);
    if (sz < 0L) {
        (void)fclose(f);
        return NULL;
    }
    rewind(f);

    char *text = malloc((size_t)sz + 1U);
    if (text == NULL) {
        (void)fclose(f);
        return NULL;
    }

    size_t rd = fread(text, 1U, (size_t)sz, f);
    if (rd != (size_t)sz) {
        free(text);
        (void)fclose(f);
        return NULL;
    }
    text[sz] = '\0';
    (void)fclose(f);
    return text;
}

/**
 * @brief Первый проход по файлу – сбор уникальных токенов.
 *
 * Добавляет все слова из файла в словарь (через get_word_id).
 * Служебные токены `<S>`, `</S>`, `<UNK>` также гарантированно попадают
 * в словарь.
 *
 * @param filename  Путь к .md файлу.
 */
static void collect_tokens_from_file(const char *filename)
{
    char *text = read_file_utf8(filename);
    if (text == NULL) {
        return;   /* ошибка чтения файла – пропускаем его */
    }

    /* Гарантируем наличие служебных токенов (добавляются один раз) */
    (void)get_word_id("<S>");
    (void)get_word_id("</S>");
    (void)get_word_id("<UNK>");

    /* Разбиваем текст на токены по стандартным разделителям */
    const char delim[] = " \t\n\r,.;:!?\"'(){}[]";
    char *token = strtok(text, delim);
    while (token != NULL) {
        (void)get_word_id(token);
        token = strtok(NULL, delim);
    }
    free(text);
}

/**
 * @brief Второй проход по файлу – подсчёт биграмм.
 *
 * Для каждой пары последовательных токенов увеличивает счётчик
 * в плоском массиве `counts` размера V×V.
 * Обработка начинается с токена `<S>`, завершается переходом в `</S>`.
 *
 * @param filename  Путь к .md файлу.
 * @param counts    Указатель на массив счётчиков [V * V], заполненный нулями.
 */
static void count_bigrams_file(const char *filename, uint32_t *counts)
{
    char *text = read_file_utf8(filename);
    if (text == NULL) {
        return;
    }

    const uint32_t start_id = get_word_id("<S>");
    const uint32_t end_id   = get_word_id("</S>");
    uint32_t cur_id = start_id;   /* начинаем с маркера начала */

    const char delim[] = " \t\n\r,.;:!?\"'(){}[]";
    char *token = strtok(text, delim);
    while (token != NULL) {
        uint32_t nxt_id = get_word_id(token);
        if (nxt_id != UINT32_MAX) {
            /* увеличиваем переход cur_id -> nxt_id */
            counts[cur_id * vocab_size + nxt_id]++;
            cur_id = nxt_id;
        }
        token = strtok(NULL, delim);
    }
    /* переход из последнего слова в конец предложения */
    counts[cur_id * vocab_size + end_id]++;
    free(text);
}

/* -----------------------------------------------------------------------
 * Основная функция – построение модели
 * ----------------------------------------------------------------------- */

int build_model(const char *input_dir, const char *output_dir)
{
    /* ---------- Первый проход: сбор словаря ---------- */
    DIR *d = opendir(input_dir);
    if (d == NULL) {
        perror("opendir input_dir");
        return BUILD_ERR_OPEN_INPUT_DIR;
    }

    struct dirent *entry;
    char path[1024];
    while ((entry = readdir(d)) != NULL) {
        /* обрабатываем только файлы с расширением .md */
        if (strstr(entry->d_name, ".md") == NULL) {
            continue;
        }
        (void)snprintf(path, sizeof(path), "%s/%s", input_dir, entry->d_name);
        struct stat st;
        if ((stat(path, &st) == 0) && S_ISREG(st.st_mode)) {
            collect_tokens_from_file(path);
        }
    }
    closedir(d);

    if (vocab_size == 0U) {
        (void)fprintf(stderr, "Error: no words found in corpus.\n");
        return BUILD_ERR_EMPTY_CORPUS;
    }

    /* ---------- Выделение памяти под счётчики биграмм ---------- */
    const uint32_t total_cells = vocab_size * vocab_size;
    uint32_t *bigram_counts = calloc(total_cells, sizeof(uint32_t));
    if (bigram_counts == NULL) {
        perror("calloc bigram_counts");
        return BUILD_ERR_ALLOC;
    }

    /* ---------- Второй проход: подсчёт биграмм ---------- */
    d = opendir(input_dir);
    if (d == NULL) {
        perror("opendir input_dir (2nd pass)");
        free(bigram_counts);
        return BUILD_ERR_OPEN_INPUT_DIR;
    }

    while ((entry = readdir(d)) != NULL) {
        if (strstr(entry->d_name, ".md") == NULL) {
            continue;
        }
        (void)snprintf(path, sizeof(path), "%s/%s", input_dir, entry->d_name);
        struct stat st;
        if ((stat(path, &st) == 0) && S_ISREG(st.st_mode)) {
            count_bigrams_file(path, bigram_counts);
        }
    }
    closedir(d);

    /* ---------- Запись model.bin ---------- */
    char model_path[1024];
    (void)snprintf(model_path, sizeof(model_path), "%s/model.bin", output_dir);
    FILE *out = fopen(model_path, "wb");
    if (out == NULL) {
        perror("fopen model.bin");
        free(bigram_counts);
        return BUILD_ERR_OUTPUT_FILE;
    }

    /* 1. Размер словаря */
    if (fwrite(&vocab_size, sizeof(uint32_t), 1U, out) != 1U) {
        (void)fclose(out);
        free(bigram_counts);
        return BUILD_ERR_OUTPUT_FILE;
    }

    /* 2. Количество ненулевых элементов матрицы */
    uint32_t total_entries = 0U;
    for (uint32_t i = 0U; i < vocab_size; i++) {
        for (uint32_t j = 0U; j < vocab_size; j++) {
            if (bigram_counts[i * vocab_size + j] > 0U) {
                total_entries++;
            }
        }
    }
    if (fwrite(&total_entries, sizeof(uint32_t), 1U, out) != 1U) {
        (void)fclose(out);
        free(bigram_counts);
        return BUILD_ERR_OUTPUT_FILE;
    }

    /* 3. Смещения строк (CSR offsets) */
    uint32_t *offsets = malloc(vocab_size * sizeof(uint32_t));
    if (offsets == NULL) {
        (void)fclose(out);
        free(bigram_counts);
        return BUILD_ERR_ALLOC;
    }
    uint32_t running_offset = 0U;
    for (uint32_t i = 0U; i < vocab_size; i++) {
        offsets[i] = running_offset;
        for (uint32_t j = 0U; j < vocab_size; j++) {
            if (bigram_counts[i * vocab_size + j] > 0U) {
                running_offset++;
            }
        }
    }
    if (fwrite(offsets, sizeof(uint32_t), vocab_size, out) != vocab_size) {
        (void)fclose(out);
        free(offsets);
        free(bigram_counts);
        return BUILD_ERR_OUTPUT_FILE;
    }

    /* 4. Данные строк: пары (next_id, cum_prob) */
    for (uint32_t i = 0U; i < vocab_size; i++) {
        uint32_t cum = 0U;
        uint32_t row_sum = 0U;

        /* суммируем все счётчики в строке */
        for (uint32_t j = 0U; j < vocab_size; j++) {
            row_sum += bigram_counts[i * vocab_size + j];
        }

        if (row_sum == 0U) {
            /* Пустая строка – только переход в <UNK> с вероятностью 1.0 */
            uint16_t unk_id = (uint16_t)get_word_id("<UNK>");
            uint16_t prob   = 65535U;
            if ((fwrite(&unk_id, sizeof(uint16_t), 1U, out) != 1U) ||
                (fwrite(&prob,   sizeof(uint16_t), 1U, out) != 1U)) {
                (void)fclose(out);
                free(offsets);
                free(bigram_counts);
                return BUILD_ERR_OUTPUT_FILE;
            }
            continue;
        }

        for (uint32_t j = 0U; j < vocab_size; j++) {
            uint32_t cnt = bigram_counts[i * vocab_size + j];
            if (cnt == 0U) {
                continue;
            }
            /* Масштабируем частоту в диапазон 0..65535 */
            cum += (uint32_t)((double)cnt / (double)row_sum * 65535.0 + 0.5);
            if (cum > 65535U) {
                cum = 65535U;
            }
            uint16_t nxt = (uint16_t)j;
            uint16_t cp  = (uint16_t)cum;
            if ((fwrite(&nxt, sizeof(uint16_t), 1U, out) != 1U) ||
                (fwrite(&cp,  sizeof(uint16_t), 1U, out) != 1U)) {
                (void)fclose(out);
                free(offsets);
                free(bigram_counts);
                return BUILD_ERR_OUTPUT_FILE;
            }
        }
    }
    (void)fclose(out);

    /* ---------- Запись vocab.txt (UTF-8 с BOM) ---------- */
    char vocab_path[1024];
    (void)snprintf(vocab_path, sizeof(vocab_path), "%s/vocab.txt", output_dir);
    FILE *vout = fopen(vocab_path, "wb");
    if (vout == NULL) {
        perror("fopen vocab.txt");
        free(offsets);
        free(bigram_counts);
        return BUILD_ERR_OUTPUT_FILE;
    }

    /* Маркер BOM для UTF-8 */
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    if (fwrite(bom, 1U, 3U, vout) != 3U) {
        (void)fclose(vout);
        free(offsets);
        free(bigram_counts);
        return BUILD_ERR_OUTPUT_FILE;
    }

    for (uint32_t i = 0U; i < vocab_size; i++) {
        size_t len = strlen(vocab[i].word);
        if ((fwrite(vocab[i].word, 1U, len, vout) != len) ||
            (fwrite("\n", 1U, 1U, vout) != 1U)) {
            (void)fclose(vout);
            free(offsets);
            free(bigram_counts);
            return BUILD_ERR_OUTPUT_FILE;
        }
    }
    (void)fclose(vout);

    /* Освобождение памяти */
    free(offsets);
    free(bigram_counts);
    return BUILD_OK;
}

/* -----------------------------------------------------------------------
 * Точка входа утилиты model_builder
 * ----------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    if ((argc < 2) || (argc > 3)) {
        (void)fprintf(stderr, "Usage: %s <input_dir> [output_dir]\n",
                      (argv != NULL) ? argv[0] : "model_builder");
        return BUILD_ERR_USAGE;
    }

    const char *out_dir = (argc == 3) ? argv[2] : "output";

    /* Создаём выходную директорию, если она ещё не существует */
#ifdef _WIN32
    if (mkdir(out_dir) != 0 && errno != EEXIST) {
        perror("mkdir output_dir");
        return BUILD_ERR_MKDIR;
    }
#else
    if (mkdir(out_dir, 0755) != 0 && errno != EEXIST) {
        perror("mkdir output_dir");
        return BUILD_ERR_MKDIR;
    }
#endif

    int status = build_model(argv[1], out_dir);
    if (status != BUILD_OK) {
        (void)fprintf(stderr, "Model building failed with error code %d.\n", status);
    }
    return status;
}