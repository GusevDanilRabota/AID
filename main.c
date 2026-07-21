/**
 * @file main.c
 * @brief Главный файл: парсинг .md файлов и генерация текста с записью в файл.
 */

#include "md_parser.h"
#include "markov.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>   // для SetConsoleOutputCP
#endif

int main(void) {
#ifdef _WIN32
    // Устанавливаем кодировку UTF-8 для консоли Windows (чтобы корректно отображать русские буквы, если они всё же появятся)
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 1. Parsing Markdown files
    printf("=== Parsing Markdown files ===\n");
    process_markdown_files("input", "output");

    // 2. Load global tables
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

    // 3. Generate text
    printf("\n=== Generating text ===\n");

    char buffer[4096];
    int tokens;
    FILE *outfile = fopen("output/dialog.txt", "w");
    if (!outfile) {
        perror("fopen output/dialog.txt");
        outfile = stdout; // fallback to console
    }

    // Generate with unigrams, temperature 0.8, start <BOS>
    tokens = markov_generate(buffer, sizeof(buffer), 30, 0.8, 0, "<BOS>");
    printf("\nUnigram (T=0.8):\n%s\n", buffer);
    printf("Generated tokens: %d\n", tokens);
    if (outfile != stdout) {
        fprintf(outfile, "=== Unigram (T=0.8) ===\n%s\n\n", buffer);
    }

    // Generate with bigrams, temperature 0.9, start "The"
    tokens = markov_generate(buffer, sizeof(buffer), 30, 0.9, 1, "The");
    printf("\nBigram (T=0.9, start='The'):\n%s\n", buffer);
    printf("Generated tokens: %d\n", tokens);
    if (outfile != stdout) {
        fprintf(outfile, "=== Bigram (T=0.9, start='The') ===\n%s\n\n", buffer);
    }

    // Deterministic mode (temperature 0.01)
    tokens = markov_generate(buffer, sizeof(buffer), 20, 0.01, 0, "hello");
    printf("\nDeterministic (T=0.01):\n%s\n", buffer);
    printf("Generated tokens: %d\n", tokens);
    if (outfile != stdout) {
        fprintf(outfile, "=== Deterministic (T=0.01) ===\n%s\n\n", buffer);
    }

    // Additional: generate with a different start token
    tokens = markov_generate(buffer, sizeof(buffer), 25, 0.7, 1, "Once");
    printf("\nBigram (T=0.7, start='Once'):\n%s\n", buffer);
    printf("Generated tokens: %d\n", tokens);
    if (outfile != stdout) {
        fprintf(outfile, "=== Bigram (T=0.7, start='Once') ===\n%s\n\n", buffer);
    }

    if (outfile != stdout) {
        fclose(outfile);
        printf("\nGenerated text saved to output/dialog.txt\n");
    }

    markov_free();
    return 0;
}