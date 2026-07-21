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

    // 1. Парсинг
    printf("=== Parsing Markdown files ===\n");
    process_markdown_files("input", "output");

    // 2. Загрузка таблиц (включая триграммы)
    printf("\n=== Loading tables ===\n");
    markov_load_unigram("output/global_unigram.txt");
    markov_load_bigram("output/global_bigram.txt");
    markov_load_trigram("output/global_trigram.txt");
    markov_load_stopwords("stopwords.txt");  // файл должен существовать

    // 3. Установка seed для воспроизводимости
    markov_set_seed(12345);

    // 4. Генерация с разными порядками
    printf("\n=== Generating text ===\n");

    // Униграммы, без стоп-слов
    char buf1[4096];
    MarkovGenOptions opts1 = {0, 0.8, 30, "<BOS>", 0};
    markov_generate_ex(buf1, sizeof(buf1), &opts1);
    printf("Unigram (T=0.8):\n%s\n\n", buf1);

    // Биграммы, с фильтрацией стоп-слов
    char buf2[4096];
    MarkovGenOptions opts2 = {1, 0.9, 30, "The", 1};
    markov_generate_ex(buf2, sizeof(buf2), &opts2);
    printf("Bigram (T=0.9, stopwords filtered):\n%s\n\n", buf2);

    // Триграммы, детерминированный режим
    char buf3[4096];
    MarkovGenOptions opts3 = {2, 0.01, 20, "Once", 0};
    markov_generate_ex(buf3, sizeof(buf3), &opts3);
    printf("Trigram (deterministic):\n%s\n\n", buf3);

    // 5. Экспорт графа в DOT (униграммы и биграммы)
    markov_export_dot("output/unigram_graph.dot", 0, 0.05);
    markov_export_dot("output/bigram_graph.dot", 1, 0.05);
    printf("Graphs exported to output/*.dot\n");

    // 6. Генерация .md файла с использованием триграмм и разных параметров
    MarkovGenOptions md_opts[] = {
        {0, 0.8, 30, "<BOS>", 0},
        {1, 0.9, 30, "The", 1},
        {2, 0.7, 25, "Once", 0}
    };
    markov_generate_md_file("output/generated_markdown.md", "Сгенерированный текст с триграммами", 3, md_opts);

    markov_free();
    return 0;
}