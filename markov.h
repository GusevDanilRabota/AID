#ifndef MARKOV_H
#define MARKOV_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int order;
    double temperature;
    int max_tokens;
    const char *start_token;
    int use_stopwords;
} MarkovGenOptions;

int markov_load_unigram(const char *filename);
int markov_load_bigram(const char *filename);
int markov_load_trigram(const char *filename);
int markov_load_stopwords(const char *filename);
void markov_set_seed(unsigned int seed);
int markov_generate(char *buffer, size_t buf_size, int max_tokens,
                    double temperature, int use_bigram, const char *start_token);
int markov_generate_ex(char *buffer, size_t buf_size, const MarkovGenOptions *options);
int markov_generate_md_file(const char *filename, const char *title,
                            int num_blocks, const MarkovGenOptions *options);
int markov_export_dot(const char *filename, int order, double min_prob);
void markov_free(void);

#ifdef __cplusplus
}
#endif

#endif