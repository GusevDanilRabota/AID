/**
 * @file md_parser.c
 * @brief Расширенный парсер Markdown с учётом структуры блоков.
 */

#include "md_parser.h"
#include "ngram_model.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

#define MAX_WORD_LEN 256
#define SMOOTHING_ALPHA 0.01

/* Типы блоков */
typedef enum {
    BLOCK_NONE,
    BLOCK_PARAGRAPH,
    BLOCK_HEADING,
    BLOCK_LIST,
    BLOCK_QUOTE,
    BLOCK_CODE
} BlockType;

/* ------------------------------------------------------------------
 * Определение типа блока по строке
 * ------------------------------------------------------------------ */
static BlockType detect_block_type(const char *line) {
    while (isspace(*line)) line++;

    if (*line == '#') {
        int level = 0;
        while (line[level] == '#') level++;
        if (isspace(line[level])) return BLOCK_HEADING;
        return BLOCK_PARAGRAPH;
    }
    if (*line == '-' || *line == '*' || *line == '+') {
        const char *p = line + 1;
        while (isspace(*p)) p++;
        if (*p == '\0' || isspace(*p) || isalnum(*p))
            return BLOCK_LIST;
        return BLOCK_PARAGRAPH;
    }
    if (*line == '>') return BLOCK_QUOTE;
    if (strlen(line) == 0 || strspn(line, " \t") == strlen(line))
        return BLOCK_NONE;
    return BLOCK_PARAGRAPH;
}

/* ------------------------------------------------------------------
 * Вспомогательная функция добавления токена с переходами
 * ------------------------------------------------------------------ */
static void add_token_with_transitions(unigram_context *uni, bigram_context *bi, trigram_context *tri,
                                       const char *token, int *prev1, int *prev2, int *prev3) {
    int idx = add_word_unigram(uni, token);
    if (*prev1 != -1) add_transition_unigram(uni, *prev1, idx);
    if (*prev1 != -1 && *prev2 != -1) {
        char key[2 * MAX_WORD_LEN + 2];
        sprintf(key, "%s|%s", uni->words[*prev2].word, uni->words[*prev1].word);
        int state = add_state_bigram(bi, key);
        add_transition_bigram(bi, state, idx);
    }
    if (*prev1 != -1 && *prev2 != -1 && *prev3 != -1) {
        char key[3 * MAX_WORD_LEN + 3];
        sprintf(key, "%s|%s|%s", uni->words[*prev3].word, uni->words[*prev2].word, uni->words[*prev1].word);
        int state = add_state_trigram(tri, key);
        add_transition_trigram(tri, state, idx);
    }
    *prev3 = *prev2;
    *prev2 = *prev1;
    *prev1 = idx;
}

/* ------------------------------------------------------------------
 * Основная функция обработки строки с учётом блоков
 * ------------------------------------------------------------------ */
static void process_line(const char *line, unigram_context *uni, bigram_context *bi, trigram_context *tri,
                         BlockType *current_block, int *prev1, int *prev2, int *prev3,
                         int *sentence_start) {
    BlockType new_block = detect_block_type(line);

    // Смена блока
    if (new_block != *current_block) {
        // Завершаем старый блок
        if (*current_block != BLOCK_NONE) {
            add_token_with_transitions(uni, bi, tri, "<BLOCK_END>", prev1, prev2, prev3);
        }
        // Начинаем новый блок
        if (new_block != BLOCK_NONE) {
            char start_token[32];
            switch (new_block) {
                case BLOCK_HEADING: strcpy(start_token, "<BLOCK_START H>"); break;
                case BLOCK_LIST:    strcpy(start_token, "<BLOCK_START L>"); break;
                case BLOCK_QUOTE:   strcpy(start_token, "<BLOCK_START Q>"); break;
                case BLOCK_CODE:    strcpy(start_token, "<BLOCK_START C>"); break;
                default:            strcpy(start_token, "<BLOCK_START P>"); break;
            }
            add_token_with_transitions(uni, bi, tri, start_token, prev1, prev2, prev3);
        }
        *current_block = new_block;
        *sentence_start = 1;
    }

    if (new_block == BLOCK_NONE) return;

    // Токенизация содержимого строки
    const char *p = line;
    char token[MAX_WORD_LEN];
    int pos = 0, in_word = 0;

    while (*p) {
        if (isalpha(*p) || isdigit(*p)) {
            if (pos < MAX_WORD_LEN - 1) token[pos++] = *p;
            in_word = 1;
        } else {
            if (in_word) {
                token[pos] = '\0';
                if (*sentence_start) {
                    add_token_with_transitions(uni, bi, tri, "<BOS>", prev1, prev2, prev3);
                    *sentence_start = 0;
                }
                add_token_with_transitions(uni, bi, tri, token, prev1, prev2, prev3);
                pos = 0; in_word = 0;
            }

            if (strchr(".,!?;:-", *p)) {
                char punct[2] = {*p, '\0'};
                add_token_with_transitions(uni, bi, tri, punct, prev1, prev2, prev3);
                if (*p == '.' || *p == '!' || *p == '?') {
                    add_token_with_transitions(uni, bi, tri, "<EOS>", prev1, prev2, prev3);
                    *sentence_start = 1;
                }
            }
        }
        p++;
    }

    if (in_word) {
        token[pos] = '\0';
        if (*sentence_start) {
            add_token_with_transitions(uni, bi, tri, "<BOS>", prev1, prev2, prev3);
            *sentence_start = 0;
        }
        add_token_with_transitions(uni, bi, tri, token, prev1, prev2, prev3);
    }
}

/* ------------------------------------------------------------------
 * Парсинг файла
 * ------------------------------------------------------------------ */
static void parse_file(FILE *input, FILE *output, unigram_context *uni, bigram_context *bi, trigram_context *tri) {
    char line[4096];
    int prev1 = -1, prev2 = -1, prev3 = -1;
    int sentence_start = 1;
    BlockType current_block = BLOCK_NONE;

    while (fgets(line, sizeof(line), input)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        process_line(line, uni, bi, tri, &current_block, &prev1, &prev2, &prev3, &sentence_start);
    }

    // Завершаем последний блок
    if (current_block != BLOCK_NONE) {
        add_token_with_transitions(uni, bi, tri, "<BLOCK_END>", &prev1, &prev2, &prev3);
    }

    // Запись локальной таблицы
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
 * Запись глобальных таблиц вероятностей
 * ------------------------------------------------------------------ */
void write_prob_tables(const unigram_context *uni, const bigram_context *bi,
                       const trigram_context *tri, const char *dir, double alpha) {
    char path[1024];
    int total = uni->word_count;

    snprintf(path, sizeof(path), "%s/global_unigram.txt", dir);
    FILE *f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < uni->word_count; i++) {
            const word_entry *we = &uni->words[i];
            fprintf(f, "%s -> ", we->word);
            if (!we->transitions) { fprintf(f, "(no transitions)\n"); continue; }
            int sum = 0; transition *t = we->transitions;
            while (t) { sum += t->count; t = t->next; }
            t = we->transitions;
            while (t) {
                double prob = (double)(t->count + alpha) / (sum + alpha * total);
                fprintf(f, "%s(%.4f)", uni->words[t->target_index].word, prob);
                t = t->next;
                if (t) fprintf(f, ", ");
            }
            fprintf(f, "\n");
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/global_bigram.txt", dir);
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < bi->state_count; i++) {
            const bigram_state *st = &bi->states[i];
            fprintf(f, "%s -> ", st->key);
            if (!st->transitions) { fprintf(f, "(no transitions)\n"); continue; }
            int sum = 0; bigram_transition *t = st->transitions;
            while (t) { sum += t->count; t = t->next; }
            t = st->transitions;
            while (t) {
                double prob = (double)(t->count + alpha) / (sum + alpha * total);
                fprintf(f, "%s(%.4f)", uni->words[t->target_index].word, prob);
                t = t->next;
                if (t) fprintf(f, ", ");
            }
            fprintf(f, "\n");
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/global_trigram.txt", dir);
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < tri->state_count; i++) {
            const trigram_state *st = &tri->states[i];
            fprintf(f, "%s -> ", st->key);
            if (!st->transitions) { fprintf(f, "(no transitions)\n"); continue; }
            int sum = 0; trigram_transition *t = st->transitions;
            while (t) { sum += t->count; t = t->next; }
            t = st->transitions;
            while (t) {
                double prob = (double)(t->count + alpha) / (sum + alpha * total);
                fprintf(f, "%s(%.4f)", uni->words[t->target_index].word, prob);
                t = t->next;
                if (t) fprintf(f, ", ");
            }
            fprintf(f, "\n");
        }
        fclose(f);
    }
}

/* ------------------------------------------------------------------
 * Основная функция обработки всех .md файлов в директории
 * ------------------------------------------------------------------ */
void process_markdown_files(const char *input_dir, const char *output_dir) {
    ensure_dir(output_dir);
    unigram_context uni; bigram_context bi; trigram_context tri;
    init_unigram(&uni); init_bigram(&bi); init_trigram(&tri);

    DIR *dir = opendir(input_dir);
    if (!dir) { perror("opendir"); free_unigram(&uni); free_bigram(&bi); free_trigram(&tri); return; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len < 4) continue;
        if (strcasecmp(name + len - 3, ".md") != 0) continue;

        char in_path[1024];
        snprintf(in_path, sizeof(in_path), "%s/%s", input_dir, name);
        FILE *in_file = fopen(in_path, "r");
        if (!in_file) { perror("fopen input"); continue; }

        char *out_path = make_output_path(output_dir, name);
        printf("Processed: %s -> %s\n", name, out_path);
        FILE *out_file = fopen(out_path, "w");
        free(out_path);
        if (!out_file) { perror("fopen output"); fclose(in_file); continue; }

        parse_file(in_file, out_file, &uni, &bi, &tri);
        fclose(in_file); fclose(out_file);
    }
    closedir(dir);

    write_prob_tables(&uni, &bi, &tri, output_dir, SMOOTHING_ALPHA);
    printf("Global tables saved in %s/\n", output_dir);
    free_unigram(&uni); free_bigram(&bi); free_trigram(&tri);
}