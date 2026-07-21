/**
 * @file md_parser.c
 * @brief Реализация парсера Markdown с общими n-граммами.
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

/* ------------------------------------------------------------
 * Вспомогательные функции (ensure_dir, make_output_path удалены)
 * ------------------------------------------------------------ */

static const char* detect_markdown_token(const char *line) {
    while (isspace(*line)) line++;
    if (*line == '#') return "<H>";
    if (*line == '-' || *line == '*' || *line == '+') return "<LI>";
    if (*line == '>') return "<BLOCKQUOTE>";
    return NULL;
}

static void process_line(const char *line, unigram_context *uni, bigram_context *bi, trigram_context *tri,
                         int *prev1, int *prev2, int *prev3, int *sentence_start) {
    const char *md_token = detect_markdown_token(line);
    if (md_token) {
        int idx = add_word_unigram(uni, md_token);
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
        *prev3 = *prev2; *prev2 = *prev1; *prev1 = idx;
        *sentence_start = 0;
    }

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
                    int bos = add_word_unigram(uni, "<BOS>");
                    if (*prev1 != -1 && *prev2 != -1) {
                        char key[2 * MAX_WORD_LEN + 2];
                        sprintf(key, "%s|%s", uni->words[*prev2].word, uni->words[*prev1].word);
                        int state = add_state_bigram(bi, key);
                        add_transition_bigram(bi, state, bos);
                    }
                    if (*prev1 != -1 && *prev2 != -1 && *prev3 != -1) {
                        char key[3 * MAX_WORD_LEN + 3];
                        sprintf(key, "%s|%s|%s", uni->words[*prev3].word, uni->words[*prev2].word, uni->words[*prev1].word);
                        int state = add_state_trigram(tri, key);
                        add_transition_trigram(tri, state, bos);
                    }
                    *prev3 = *prev2; *prev2 = *prev1; *prev1 = bos;
                    *sentence_start = 0;
                }
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
                *prev3 = *prev2; *prev2 = *prev1; *prev1 = idx;
                pos = 0; in_word = 0;
            }

            if (strchr(".,!?;:-", *p)) {
                char punct[2] = {*p, '\0'};
                int idx = add_word_unigram(uni, punct);
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
                *prev3 = *prev2; *prev2 = *prev1; *prev1 = idx;
                if (*p == '.' || *p == '!' || *p == '?') {
                    int eos = add_word_unigram(uni, "<EOS>");
                    if (*prev1 != -1) add_transition_unigram(uni, *prev1, eos);
                    if (*prev1 != -1 && *prev2 != -1) {
                        char key[2 * MAX_WORD_LEN + 2];
                        sprintf(key, "%s|%s", uni->words[*prev2].word, uni->words[*prev1].word);
                        int state = add_state_bigram(bi, key);
                        add_transition_bigram(bi, state, eos);
                    }
                    if (*prev1 != -1 && *prev2 != -1 && *prev3 != -1) {
                        char key[3 * MAX_WORD_LEN + 3];
                        sprintf(key, "%s|%s|%s", uni->words[*prev3].word, uni->words[*prev2].word, uni->words[*prev1].word);
                        int state = add_state_trigram(tri, key);
                        add_transition_trigram(tri, state, eos);
                    }
                    *prev3 = *prev2; *prev2 = *prev1; *prev1 = eos;
                    *sentence_start = 1;
                }
            }
        }
        p++;
    }
    if (in_word) {
        token[pos] = '\0';
        if (*sentence_start) {
            int bos = add_word_unigram(uni, "<BOS>");
            if (*prev1 != -1 && *prev2 != -1) {
                char key[2 * MAX_WORD_LEN + 2];
                sprintf(key, "%s|%s", uni->words[*prev2].word, uni->words[*prev1].word);
                int state = add_state_bigram(bi, key);
                add_transition_bigram(bi, state, bos);
            }
            if (*prev1 != -1 && *prev2 != -1 && *prev3 != -1) {
                char key[3 * MAX_WORD_LEN + 3];
                sprintf(key, "%s|%s|%s", uni->words[*prev3].word, uni->words[*prev2].word, uni->words[*prev1].word);
                int state = add_state_trigram(tri, key);
                add_transition_trigram(tri, state, bos);
            }
            *prev3 = *prev2; *prev2 = *prev1; *prev1 = bos;
            *sentence_start = 0;
        }
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
        *prev3 = *prev2; *prev2 = *prev1; *prev1 = idx;
    }
}

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