#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Прототипы ASM функций
extern uint16_t lfsr_rand(void);
extern uint16_t binary_search_entry(const uint16_t *row, uint16_t rnd, int len);

// Структуры данных модели
static uint32_t V;                 // размер словаря
static uint32_t total_entries;     // общее число записей в матрице
static uint32_t *offsets = NULL;   // смещения строк в массиве entries (V элементов)
static uint16_t *entries = NULL;   // все записи подряд (total_entries * 2 элементов, 4 байта на запись)
static char **vocab = NULL;        // словарь, индекс = id

void load_model(const char *model_path, const char *vocab_path) {
    FILE *f = fopen(model_path, "rb");
    if (!f) { perror("fopen model"); exit(1); }
    fread(&V, sizeof(uint32_t), 1, f);
    fread(&total_entries, sizeof(uint32_t), 1, f);
    offsets = malloc(V * sizeof(uint32_t));
    fread(offsets, sizeof(uint32_t), V, f);
    entries = malloc(total_entries * 2 * sizeof(uint16_t));
    fread(entries, sizeof(uint16_t), total_entries * 2, f);
    fclose(f);

    // Загружаем словарь
    f = fopen(vocab_path, "r");
    if (!f) { perror("fopen vocab"); exit(1); }
    vocab = malloc(V * sizeof(char*));
    char line[256];
    for (uint32_t i = 0; i < V; i++) {
        if (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            vocab[i] = strdup(line);
        } else {
            vocab[i] = strdup("<UNK>");
        }
    }
    fclose(f);
}

// Генерация следующего токена по текущему id
uint16_t sample_next(uint16_t current_id) {
    uint32_t start = offsets[current_id];
    uint32_t end = (current_id + 1 < V) ? offsets[current_id + 1] : total_entries;
    int row_len = end - start;
    if (row_len == 0) return 0; // fallback <UNK> или <S>
    uint16_t rnd = lfsr_rand();
    const uint16_t *row = entries + start * 2; // указывает на первый id в строке
    return binary_search_entry(row, rnd, row_len);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <model.bin> <vocab.txt>\n", argv[0]);
        return 1;
    }
    load_model(argv[1], argv[2]);

    // Начальный токен – <S> (всегда id 0, если builder сохранил его первым)
    uint16_t current = 0;
    printf("%s", vocab[current]); // печатать <S> обычно не нужно, для отладки можно
    // Генерируем 200 токенов
    for (int i = 0; i < 200; i++) {
        uint16_t next = sample_next(current);
        if (next == 0) break; // </S> или ошибка, остановим
        printf(" %s", vocab[next]);
        current = next;
    }
    printf("\n");
    return 0;
}