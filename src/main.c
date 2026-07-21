/**
 * @file main.c
 * @brief Главный модуль программы – точка входа.
 *
 * Обрабатывает аргументы командной строки, запускает парсинг
 * Markdown или C-кода, загружает таблицы и выполняет генерацию
 * или интерактивный режим.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "md_parser.h"
#include "c_parser.h"
#include "markov.h"
#include "interactive.h"
#include "ngram_model.h"

/* ------------------------------------------------------------------
 * Вспомогательные функции
 * ------------------------------------------------------------------ */

/** Вывод справки */
static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "Options:\n"
        "  --mode markdown|code   - parsing mode (default: markdown)\n"
        "  --generate             - generate text/code after parsing\n"
        "  --interactive          - start interactive dialog\n"
        "  --order N              - n-gram order (0=unigram, 1=bigram, 2=trigram) (default: 1)\n"
        "  --temp T               - temperature (0.0-2.0) (default: 0.8)\n"
        "  --tokens N             - max tokens per generation (default: 30)\n"
        "  --start \"token\"        - start token for generation (default: <BOS>)\n"
        "  --stopwords file       - path to stopwords file (optional)\n"
        "  --help                 - show this help\n",
        prog);
}

/** Проверка существования файла */
static int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* ------------------------------------------------------------------
 * Основная функция
 * ------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
#ifdef _WIN32
    // Устанавливаем UTF-8 для консоли Windows
    SetConsoleOutputCP(CP_UTF8);
#endif

    /* --- Параметры по умолчанию --- */
    const char *mode = "markdown";
    int generate = 0;
    int interactive = 0;
    int order = 1;
    double temperature = 0.8;
    int max_tokens = 30;
    const char *start_token = "<BOS>";
    const char *stopwords_file = NULL;

    /* --- Разбор аргументов командной строки --- */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = argv[++i];
        } else if (strcmp(argv[i], "--generate") == 0) {
            generate = 1;
        } else if (strcmp(argv[i], "--interactive") == 0) {
            interactive = 1;
        } else if (strcmp(argv[i], "--order") == 0 && i + 1 < argc) {
            order = atoi(argv[++i]);
            if (order < 0 || order > 2) {
                fprintf(stderr, "Order must be 0, 1, or 2\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--temp") == 0 && i + 1 < argc) {
            temperature = atof(argv[++i]);
            if (temperature < 0.0) temperature = 0.0;
        } else if (strcmp(argv[i], "--tokens") == 0 && i + 1 < argc) {
            max_tokens = atoi(argv[++i]);
            if (max_tokens < 1) max_tokens = 1;
        } else if (strcmp(argv[i], "--start") == 0 && i + 1 < argc) {
            start_token = argv[++i];
        } else if (strcmp(argv[i], "--stopwords") == 0 && i + 1 < argc) {
            stopwords_file = argv[++i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* --- Режим Markdown --- */
    if (strcmp(mode, "markdown") == 0) {
        printf("=== Parsing Markdown files from 'input/' ===\n");
        process_markdown_files("input", "output");
    }
    /* --- Режим C-кода --- */
    else if (strcmp(mode, "code") == 0) {
        printf("=== Parsing C files from 'input/' ===\n");
        unigram_context uni;
        bigram_context bi;
        trigram_context tri;
        init_unigram(&uni);
        init_bigram(&bi);
        init_trigram(&tri);

        process_c_files("input", &uni, &bi, &tri);

        // Сохраняем глобальные таблицы (используем функцию из md_parser)
        // Передаём alpha = 0.01 (стандартное сглаживание)
        write_prob_tables(&uni, &bi, &tri, "output", 0.01);

        free_unigram(&uni);
        free_bigram(&bi);
        free_trigram(&tri);
        printf("Global tables for C-code saved in 'output/'\n");
    } else {
        fprintf(stderr, "Unknown mode: %s\n", mode);
        return 1;
    }

    /* --- Если требуется генерация или интерактив, загружаем таблицы --- */
    if (generate || interactive) {
        printf("\n=== Loading tables ===\n");
        if (markov_load_tables("output/global_unigram.txt",
                               "output/global_bigram.txt",
                               "output/global_trigram.txt") != 0) {
            fprintf(stderr, "Failed to load tables. Run parsing first.\n");
            return 1;
        }

        // Загружаем стоп-слова, если указаны
        if (stopwords_file && file_exists(stopwords_file)) {
            if (markov_load_stopwords(stopwords_file) != 0) {
                fprintf(stderr, "Warning: could not load stopwords from %s\n", stopwords_file);
            }
        } else if (stopwords_file) {
            fprintf(stderr, "Warning: stopwords file '%s' not found\n", stopwords_file);
        }
    }

    /* --- Генерация --- */
    if (generate) {
        printf("\n=== Generating text ===\n");
        char buffer[8192];
        const char *start_arr[2] = { start_token, NULL };
        MarkovGenOptions opts = {
            .order = order,
            .temperature = temperature,
            .max_tokens = max_tokens,
            .start_tokens = start_arr,
            .use_stopwords = (stopwords_file != NULL) // используем стоп-слова, если они загружены
        };

        int tokens = markov_generate_ex(buffer, sizeof(buffer), &opts);
        if (tokens > 0) {
            printf("\nGenerated text:\n%s\n", buffer);
            // Сохраняем в файл
            FILE *f = fopen("output/generated_output.txt", "w");
            if (f) {
                fprintf(f, "%s\n", buffer);
                fclose(f);
                printf("Generated text saved to output/generated_output.txt\n");
            } else {
                perror("fopen output/generated_output.txt");
            }
        } else {
            fprintf(stderr, "Generation failed or returned no tokens.\n");
        }
    }

    /* --- Интерактивный режим --- */
    if (interactive) {
        printf("\n=== Starting interactive mode ===\n");
        interactive_start("dialog.md", order, temperature, max_tokens,
                          (stopwords_file != NULL));
    }

    /* --- Освобождение ресурсов --- */
    markov_free();

    return 0;
}