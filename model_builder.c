#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_VOCAB 65535

typedef struct {
    char *word;
    uint32_t id;
} VocabEntry;

VocabEntry vocab[MAX_VOCAB];
uint32_t vocab_size = 0;

uint32_t get_word_id(const char *word) {
    for (uint32_t i = 0; i < vocab_size; i++) {
        if (strcmp(vocab[i].word, word) == 0)
            return i;
    }
    if (vocab_size >= MAX_VOCAB) return UINT32_MAX;
    vocab[vocab_size].word = strdup(word);
    vocab[vocab_size].id = vocab_size;
    return vocab_size++;
}

// Первый проход: только сбор уникальных слов
void collect_tokens(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = malloc(sz + 1);
    fread(text, 1, sz, f);
    text[sz] = '\0';
    fclose(f);

    get_word_id("<S>");
    get_word_id("</S>");
    get_word_id("<UNK>");

    char *token = strtok(text, " \t\n\r,.;:!?\"'(){}[]");
    while (token) {
        get_word_id(token);
        token = strtok(NULL, " \t\n\r,.;:!?\"'(){}[]");
    }
    free(text);
}

// Второй проход: подсчёт биграмм
void count_bigrams_file(const char *filename, uint32_t *bigram_counts) {
    FILE *f = fopen(filename, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = malloc(sz + 1);
    fread(text, 1, sz, f);
    text[sz] = '\0';
    fclose(f);

    uint32_t cur_id = get_word_id("<S>");
    char *token = strtok(text, " \t\n\r,.;:!?\"'(){}[]");
    while (token) {
        uint32_t nxt_id = get_word_id(token);
        if (nxt_id != UINT32_MAX) {
            bigram_counts[cur_id * vocab_size + nxt_id]++;
            cur_id = nxt_id;
        }
        token = strtok(NULL, " \t\n\r,.;:!?\"'(){}[]");
    }
    bigram_counts[cur_id * get_word_id("</S>")]++;
    free(text);
}

void build_model(const char *dirpath) {
    // Первый проход: собираем все токены
    DIR *d = opendir(dirpath);
    if (!d) { perror("opendir"); exit(1); }
    struct dirent *entry;
    char path[1024];
    while ((entry = readdir(d))) {
        if (strstr(entry->d_name, ".md") == NULL) continue;
        snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            collect_tokens(path);
    }
    closedir(d);

    // Выделяем память под биграммы
    uint32_t total_cells = vocab_size * vocab_size;
    uint32_t *bigram_counts = calloc(total_cells, sizeof(uint32_t));
    if (!bigram_counts) { perror("calloc"); exit(1); }

    // Второй проход: считаем биграммы
    d = opendir(dirpath);
    if (!d) { perror("opendir"); exit(1); }
    while ((entry = readdir(d))) {
        if (strstr(entry->d_name, ".md") == NULL) continue;
        snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            count_bigrams_file(path, bigram_counts);
    }
    closedir(d);

    // Сериализация модели
    FILE *out = fopen("model.bin", "wb");
    if (!out) { perror("fopen model.bin"); exit(1); }
    fwrite(&vocab_size, sizeof(uint32_t), 1, out);

    uint32_t total_entries = 0;
    for (uint32_t i = 0; i < vocab_size; i++) {
        for (uint32_t j = 0; j < vocab_size; j++)
            if (bigram_counts[i * vocab_size + j] > 0)
                total_entries++;
    }
    fwrite(&total_entries, sizeof(uint32_t), 1, out);

    uint32_t *offsets = malloc(vocab_size * sizeof(uint32_t));
    uint32_t running_offset = 0;
    for (uint32_t i = 0; i < vocab_size; i++) {
        offsets[i] = running_offset;
        for (uint32_t j = 0; j < vocab_size; j++)
            if (bigram_counts[i * vocab_size + j] > 0)
                running_offset++;
    }
    fwrite(offsets, sizeof(uint32_t), vocab_size, out);

    for (uint32_t i = 0; i < vocab_size; i++) {
        uint32_t cum = 0;
        uint32_t row_sum = 0;
        for (uint32_t j = 0; j < vocab_size; j++)
            row_sum += bigram_counts[i * vocab_size + j];
        if (row_sum == 0) {
            uint16_t id = (uint16_t)get_word_id("<UNK>");
            uint16_t prob = 65535;
            fwrite(&id, sizeof(uint16_t), 1, out);
            fwrite(&prob, sizeof(uint16_t), 1, out);
            continue;
        }
        for (uint32_t j = 0; j < vocab_size; j++) {
            uint32_t cnt = bigram_counts[i * vocab_size + j];
            if (cnt == 0) continue;
            cum += (uint32_t)((double)cnt / row_sum * 65535.0 + 0.5);
            if (cum > 65535) cum = 65535;
            uint16_t nxt = (uint16_t)j;
            uint16_t cum_prob = (uint16_t)cum;
            fwrite(&nxt, sizeof(uint16_t), 1, out);
            fwrite(&cum_prob, sizeof(uint16_t), 1, out);
        }
    }
    fclose(out);

    // Словарь
    FILE *v = fopen("vocab.txt", "w");
    for (uint32_t i = 0; i < vocab_size; i++)
        fprintf(v, "%s\n", vocab[i].word);
    fclose(v);

    free(offsets);
    free(bigram_counts);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory_with_md_files>\n", argv[0]);
        return 1;
    }
    build_model(argv[1]);
    return 0;
}