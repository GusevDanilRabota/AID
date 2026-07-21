#include "tokenizer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "int", "long", "register", "return", "short", "signed", "sizeof", "static",
    "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while",
    "_Bool", "_Complex", "_Imaginary", "inline", "restrict"
};
#define NUM_KEYWORDS (sizeof(keywords)/sizeof(keywords[0]))

int is_keyword(const char *str) {
    for (int i = 0; i < (int)NUM_KEYWORDS; i++) {
        if (strcmp(str, keywords[i]) == 0) return 1;
    }
    return 0;
}

int is_operator(const char *str) {
    static const char *operators[] = {
        "+", "-", "*", "/", "%", "=", "==", "!=", "<", ">", "<=", ">=",
        "&&", "||", "!", "&", "|", "^", "~", "<<", ">>", "+=", "-=",
        "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "->", ".",
        "++", "--"
    };
    int n = sizeof(operators)/sizeof(operators[0]);
    for (int i = 0; i < n; i++) {
        if (strcmp(str, operators[i]) == 0) return 1;
    }
    return 0;
}

static int peek_char(FILE *f) {
    int c = fgetc(f);
    if (c != EOF) ungetc(c, f);
    return c;
}

Token* get_next_token(FILE *f) {
    int c;
    while ((c = fgetc(f)) != EOF && isspace(c)) { }
    if (c == EOF) {
        Token *tok = malloc(sizeof(Token));
        tok->type = TOKEN_EOF;
        tok->value = strdup("EOF");
        return tok;
    }

    if (c == '/') {
        int next = peek_char(f);
        if (next == '/') {
            fgetc(f);
            char buffer[1024];
            int pos = 0;
            while ((c = fgetc(f)) != EOF && c != '\n') {
                if (pos < 1023) buffer[pos++] = c;
            }
            buffer[pos] = '\0';
            Token *tok = malloc(sizeof(Token));
            tok->type = TOKEN_COMMENT;
            tok->value = malloc(pos + 3);
            sprintf(tok->value, "//%s", buffer);
            return tok;
        } else if (next == '*') {
            fgetc(f);
            char buffer[4096];
            int pos = 0, prev = 0;
            while ((c = fgetc(f)) != EOF) {
                if (prev == '*' && c == '/') break;
                if (pos < 4095) buffer[pos++] = c;
                prev = c;
            }
            buffer[pos] = '\0';
            Token *tok = malloc(sizeof(Token));
            tok->type = TOKEN_COMMENT;
            tok->value = malloc(pos + 5);
            sprintf(tok->value, "/*%s*/", buffer);
            return tok;
        }
    }

    if (c == '#') {
        char buffer[256];
        int pos = 0;
        buffer[pos++] = c;
        while ((c = fgetc(f)) != EOF && !isspace(c) && c != '\n') {
            if (pos < 255) buffer[pos++] = c;
        }
        buffer[pos] = '\0';
        Token *tok = malloc(sizeof(Token));
        tok->type = TOKEN_PREPROCESSOR;
        tok->value = strdup(buffer);
        return tok;
    }

    if (c == '"') {
        char buffer[1024];
        int pos = 0;
        buffer[pos++] = c;
        while ((c = fgetc(f)) != EOF && c != '"') {
            if (pos < 1023) buffer[pos++] = c;
            if (c == '\\') buffer[pos++] = fgetc(f);
        }
        if (c == '"') buffer[pos++] = '"';
        buffer[pos] = '\0';
        Token *tok = malloc(sizeof(Token));
        tok->type = TOKEN_STRING;
        tok->value = strdup(buffer);
        return tok;
    }

    if (c == '\'') {
        char buffer[1024];
        int pos = 0;
        buffer[pos++] = c;
        while ((c = fgetc(f)) != EOF && c != '\'') {
            if (pos < 1023) buffer[pos++] = c;
            if (c == '\\') buffer[pos++] = fgetc(f);
        }
        if (c == '\'') buffer[pos++] = '\'';
        buffer[pos] = '\0';
        Token *tok = malloc(sizeof(Token));
        tok->type = TOKEN_CHAR;
        tok->value = strdup(buffer);
        return tok;
    }

    if (isalpha(c) || c == '_') {
        char buffer[256];
        int pos = 0;
        buffer[pos++] = c;
        while ((c = fgetc(f)) != EOF && (isalnum(c) || c == '_')) {
            if (pos < 255) buffer[pos++] = c;
        }
        if (c != EOF) ungetc(c, f);
        buffer[pos] = '\0';
        Token *tok = malloc(sizeof(Token));
        if (is_keyword(buffer)) tok->type = TOKEN_KEYWORD;
        else tok->type = TOKEN_IDENTIFIER;
        tok->value = strdup(buffer);
        return tok;
    }

    if (isdigit(c)) {
        char buffer[256];
        int pos = 0;
        buffer[pos++] = c;
        while ((c = fgetc(f)) != EOF && (isdigit(c) || c == '.' || c == 'x' || c == 'X' ||
               (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
            if (pos < 255) buffer[pos++] = c;
        }
        if (c != EOF) ungetc(c, f);
        buffer[pos] = '\0';
        Token *tok = malloc(sizeof(Token));
        tok->type = TOKEN_NUMBER;
        tok->value = strdup(buffer);
        return tok;
    }

    char op[3] = {c, '\0'};
    int next = peek_char(f);
    if ((c == '=' || c == '!' || c == '<' || c == '>' || c == '&' || c == '|') && next == '=') {
        op[1] = fgetc(f); op[2] = '\0';
    } else if (c == '+' && next == '+') { op[1] = fgetc(f); op[2] = '\0'; }
    else if (c == '-' && next == '-') { op[1] = fgetc(f); op[2] = '\0'; }
    else if (c == '-' && next == '>') { op[1] = fgetc(f); op[2] = '\0'; }
    else if (c == '&' && next == '&') { op[1] = fgetc(f); op[2] = '\0'; }
    else if (c == '|' && next == '|') { op[1] = fgetc(f); op[2] = '\0'; }
    else if (c == '<' && next == '<') { op[1] = fgetc(f); op[2] = '\0'; }
    else if (c == '>' && next == '>') { op[1] = fgetc(f); op[2] = '\0'; }
    Token *tok = malloc(sizeof(Token));
    tok->type = is_operator(op) ? TOKEN_OPERATOR : TOKEN_PUNCTUATOR;
    tok->value = strdup(op);
    return tok;
}

void free_token(Token *tok) {
    if (tok) { free(tok->value); free(tok); }
}