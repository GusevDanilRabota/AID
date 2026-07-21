#include "md_parser.h"
#include "markov.h"
#include "interactive.h"
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
    markov_load_stopwords("stopwords.txt");

    // 3. Демонстрационные генерации
    printf("\n=== Demo generations ===\n");
    // ... (можно оставить старый код или убрать)

    // 4. Интерактивный режим
    printf("\n=== Starting interactive mode ===\n");
    // Параметры: порядок=2 (биграммы), температура=0.8, макс. токенов=30, фильтрация стоп-слов включена
    interactive_start("dialog.md", 2, 0.8, 30, 1);

    markov_free();
    return 0;
}