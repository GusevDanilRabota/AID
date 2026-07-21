/**
 * @file main.c
 * @brief Главный файл: парсинг, генерация и диалог.
 * gcc -std=c99 -Wall -o markov_processor main.c md_parser.c markov.c console_chat.c -lm
 */

#include "md_parser.h"
#include "markov.h"
#include "console_chat.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 1. Парсинг Markdown-файлов (если папка input существует)
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
    if (markov_load_trigram("output/global_trigram.txt") != 0) {
        fprintf(stderr, "Error loading trigram table\n");
        markov_free();
        return 1;
    }

    // Загружаем стоп-слова (если файл существует)
    markov_load_stopwords("stopwords.txt");

    // 3. Выбор режима
    printf("\n=== Choose mode ===\n");
    printf("1 - Generate Markdown files (demo)\n");
    printf("2 - Interactive chat with Markov generator\n");
    printf("Enter your choice (1 or 2): ");
    char choice[8];
    if (fgets(choice, sizeof(choice), stdin) == NULL) choice[0] = '1';
    int mode = atoi(choice);
    if (mode != 2) mode = 1;

    if (mode == 1) {
        // Демонстрационный режим: генерация .md файлов
        printf("\n=== Generating Markdown files ===\n");
        MarkovGenOptions opts[] = {
            {0, 0.8, 30, "<BOS>", 0},
            {1, 0.9, 30, "The", 1},
            {2, 0.7, 25, "Once", 0}
        };
        markov_generate_md_file("output/generated_markdown.md", "Generated Text with Trigram", 3, opts);
        markov_export_dot("output/unigram_graph.dot", 0, 0.05);
        markov_export_dot("output/bigram_graph.dot", 1, 0.05);
        printf("Generated: output/generated_markdown.md, output/*.dot\n");
    } else {
        // Интерактивный чат
        printf("\n=== Starting interactive chat ===\n");
        ChatConfig config = {
            .output_filename = "chat_log.md",
            .order = 1,
            .temperature = 0.8,
            .max_tokens = 50,
            .use_stopwords = 0
        };
        start_chat(&config);
    }

    markov_free();
    return 0;
}