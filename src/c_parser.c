/**
 * @file c_parser.c
 * @brief Реализация парсера C-кода.
 */

#include "c_parser.h"
#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

/**
 * @brief Проверяет, имеет ли файл расширение .c или .h.
 */
static int is_c_file(const char *name) {
    size_t len = strlen(name);
    if (len < 3) return 0;
    const char *ext = name + len - 2;
    return (strcasecmp(ext, ".c") == 0 || strcasecmp(ext, ".h") == 0);
}

/**
 * @brief Обрабатывает один C-файл, извлекая токены и добавляя переходы в контексты.
 */
static void parse_c_file(FILE *f, unigram_context *uni, bigram_context *bi, trigram_context *tri) {
    Token *tok;
    int prev1 = -1, prev2 = -1, prev3 = -1;

    while ((tok = get_next_token(f)) != NULL) {
        if (tok->type == TOKEN_EOF) {
            free_token(tok);
            break;
        }
        // Пропускаем комментарии (они не нужны для генерации кода)
        if (tok->type == TOKEN_COMMENT) {
            free_token(tok);
            continue;
        }

        // Добавляем токен в униграмму
        int cur = add_word_unigram(uni, tok->value);

        // Добавляем переходы в униграмму
        if (prev1 != -1) {
            add_transition_unigram(uni, prev1, cur);

            // Биграмма
            if (prev2 != -1) {
                char key[2 * MAX_TOKEN_LEN + 2];
                sprintf(key, "%s|%s", uni->words[prev2].word, uni->words[prev1].word);
                int state = add_state_bigram(bi, key);
                add_transition_bigram(bi, state, cur);

                // Триграмма
                if (prev3 != -1) {
                    char key3[3 * MAX_TOKEN_LEN + 3];
                    sprintf(key3, "%s|%s|%s", uni->words[prev3].word, uni->words[prev2].word, uni->words[prev1].word);
                    int state3 = add_state_trigram(tri, key3);
                    add_transition_trigram(tri, state3, cur);
                }
            }
        }

        // Сдвиг окна
        prev3 = prev2;
        prev2 = prev1;
        prev1 = cur;

        free_token(tok);
    }
}

void process_c_files(const char *dir, unigram_context *uni, bigram_context *bi, trigram_context *tri) {
    DIR *d = opendir(dir);
    if (!d) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        const char *name = entry->d_name;
        if (!is_c_file(name)) continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, name);
        FILE *f = fopen(path, "r");
        if (!f) {
            perror("fopen");
            continue;
        }
        printf("Processing C file: %s\n", name);
        parse_c_file(f, uni, bi, tri);
        fclose(f);
    }
    closedir(d);
}