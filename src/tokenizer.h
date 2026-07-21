#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdio.h>

typedef enum {
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_CHAR,
    TOKEN_OPERATOR,
    TOKEN_PUNCTUATOR,
    TOKEN_PREPROCESSOR,
    TOKEN_COMMENT,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char *value;
} Token;

Token* get_next_token(FILE *file);
void free_token(Token *tok);
int is_keyword(const char *str);
int is_operator(const char *str);

#endif