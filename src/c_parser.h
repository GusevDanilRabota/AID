#ifndef C_PARSER_H
#define C_PARSER_H

#include "ngram_model.h"

void process_c_files(const char *dir, unigram_context *uni, bigram_context *bi, trigram_context *tri);

#endif