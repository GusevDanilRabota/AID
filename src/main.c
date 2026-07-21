#include "md_parser.h"
#include "c_parser.h"
#include "markov.h"
#include "interactive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "Options:\n"
        "  --mode markdown|code   (parse mode)\n"
        "  --generate             generate text/code after parsing\n"
        "  --interactive          start interactive dialog\n"
        "  --order N              (0,1,2)\n"
        "  --temp T               (0.0-2.0)\n"
        "  --tokens N             max tokens\n"
        "  --start \"token\"        start token\n"
        "  --stopwords file       stopwords file\n"
        "  --help                 this help\n",
        prog);
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    const char *mode = "markdown";
    int generate = 0, interactive = 0;
    int order = 1;
    double temperature = 0.8;
    int max_tokens = 30;
    const char *start_token = "<BOS>";
    const char *stopwords_file = "stopwords.txt";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) { print_usage(argv[0]); return 0; }
        else if (strcmp(argv[i], "--mode") == 0 && i+1 < argc) mode = argv[++i];
        else if (strcmp(argv[i], "--generate") == 0) generate = 1;
        else if (strcmp(argv[i], "--interactive") == 0) interactive = 1;
        else if (strcmp(argv[i], "--order") == 0 && i+1 < argc) order = atoi(argv[++i]);
        else if (strcmp(argv[i], "--temp") == 0 && i+1 < argc) temperature = atof(argv[++i]);
        else if (strcmp(argv[i], "--tokens") == 0 && i+1 < argc) max_tokens = atoi(argv[++i]);
        else if (strcmp(argv[i], "--start") == 0 && i+1 < argc) start_token = argv[++i];
        else if (strcmp(argv[i], "--stopwords") == 0 && i+1 < argc) stopwords_file = argv[++i];
    }

    if (strcmp(mode, "markdown") == 0) {
        printf("=== Parsing Markdown files ===\n");
        process_markdown_files("input", "output");
    } else if (strcmp(mode, "code") == 0) {
        printf("=== Parsing C files ===\n");
        unigram_context uni; bigram_context bi; trigram_context tri;
        init_unigram(&uni); init_bigram(&bi); init_trigram(&tri);
        process_c_files("input", &uni, &bi, &tri);
        write_prob_tables(&uni, &bi, &tri, "output", 0.01);
        free_unigram(&uni); free_bigram(&bi); free_trigram(&tri);
    }

    if (generate || interactive) {
        markov_load_tables("output/global_unigram.txt",
                           "output/global_bigram.txt",
                           "output/global_trigram.txt");
        if (stopwords_file) markov_load_stopwords(stopwords_file);
    }

    if (generate) {
        char buffer[8192];
        const char *start_arr[2] = { start_token, NULL };
        MarkovGenOptions opts = { order, temperature, max_tokens, start_arr, 1 };
        int n = markov_generate_ex(buffer, sizeof(buffer), &opts);
        if (n > 0) {
            printf("\n=== Generated Text ===\n%s\n", buffer);
            FILE *f = fopen("output/generated_output.txt", "w");
            if (f) { fprintf(f, "%s\n", buffer); fclose(f); }
        }
    }

    if (interactive) {
        interactive_start("dialog.md", order, temperature, max_tokens, 1);
    }

    markov_free();
    return 0;
}