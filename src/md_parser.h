#ifndef MD_PARSER_H
#define MD_PARSER_H

#include "ngram_model.h"

void process_markdown_files(const char *input_dir, const char *output_dir);

// Добавлен параметр alpha
void write_prob_tables(const unigram_context *uni, const bigram_context *bi,
                       const trigram_context *tri, const char *dir, double alpha);

#endif