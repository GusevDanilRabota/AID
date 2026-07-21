#ifndef MARKOV_H
#define MARKOV_H

#include "ngram_model.h"
#include <stddef.h>

typedef struct {
    int order;
    double temperature;
    int max_tokens;
    const char **start_tokens;
    int use_stopwords;
} MarkovGenOptions;

int markov_load_tables(const char *unigram_file, const char *bigram_file, const char *trigram_file);
int markov_load_stopwords(const char *filename);
int markov_generate_ex(char *buffer, size_t buf_size, const MarkovGenOptions *options);
int markov_generate(char *buffer, size_t buf_size, int max_tokens,
                    double temperature, int use_bigram, const char *start_token);
void markov_free(void);

#endif