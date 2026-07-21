/**
 * @file main.c
 * @brief Парсинг .md файлов, генерация текста и сохранение в .md файлы.
 */

#include "md_parser.h"
#include "markov.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 1. Парсинг Markdown-файлов
    printf("=== Parsing Markdown files ===\n");
    process_markdown_files("input", "output");

    // 2. Загрузка таблиц
    printf("\n=== Loading tables ===\n");
    if (markov_load_unigram("output/global_unigram.txt") != 0) {
        fprintf(stderr, "Error loading unigram table\n");
        return 1;
    }
    if (markov_load_bigram("output/global_bigram.txt") != 0) {
        fprintf(stderr, "Error loading bigram table\n");
        markov_free();
        return 1;
    }

    // 3. Генерация и сохранение в .md файлы
    printf("\n=== Generating Markdown files ===\n");

    // 3.1. Генерация документа с несколькими блоками
    MarkovGenOptions options[] = {
        {0, 0.8, 30, "<BOS>"},   // униграммы, T=0.8
        {1, 0.9, 30, "The"},     // биграммы, T=0.9
        {0, 0.01, 20, "hello"},  // детерминированный
        {1, 0.7, 25, "Once"}     // биграммы, T=0.7
    };
    int num_blocks = sizeof(options) / sizeof(options[0]);

    if (markov_generate_md_file("output/generated_text.md", "Сгенерированный текст", num_blocks, options) != 0) {
        fprintf(stderr, "Error generating Markdown file\n");
    } else {
        printf("Generated text saved to output/generated_text.md\n");
    }

    // 3.2. Дополнительно можно сгенерировать отдельный файл с одним большим текстом
    char buffer[8192];
    int tokens = markov_generate(buffer, sizeof(buffer), 100, 0.85, 1, "<BOS>");
    if (tokens > 0) {
        FILE *f = fopen("output/long_generated.md", "w");
        if (f) {
            fprintf(f, "# Длинный сгенерированный текст\n\n");
            fprintf(f, "%s\n", buffer);
            fclose(f);
            printf("Long generated text saved to output/long_generated.md\n");
        } else {
            perror("fopen long_generated.md");
        }
    }

    markov_free();
    return 0;
}