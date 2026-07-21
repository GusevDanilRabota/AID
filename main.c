// gcc -std=c99 -Wall -o markov_processor main.c md_parser.c 
#include "md_parser.h"

int main(void) {
    process_markdown_files("input", "output");
    return 0;
}