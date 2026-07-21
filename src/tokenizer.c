/**
 * @file tokenizer.c
 * @brief Реализация токенизатора для языка C.
 */

#include "tokenizer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Список ключевых слов (стандарт C99) */
static const char *keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "int", "long", "register", "return", "short", "signed", "sizeof", "static",
    "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while",
    "_Bool", "_Complex", "_Imaginary", "inline", "restrict"
};
#define NUM_KEYWORDS (sizeof(keywords)/sizeof(keywords[0]))

/* Список операторов */
static const char *operators[] = {
    "+", "-", "*", "/", "%", "=", "==", "!=", "<", ">", "<=", ">=",
    "&&", "||", "!", "&", "|", "^", "~", "<<", ">>", "+=", "-=",
    "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "->", ".",
    "++", "--"
};
#define NUM_OPERATORS (sizeof(operators)/sizeof(operators[0]))

/* ------------------------------------------------------------------
 * Вспомогательные функции
 * ------------------------------------------------------------------ */

static int is_idchar(int c) {
    return isalnum(c) || c == '_';
}

static int peek_char(FILE *f) {
    int c = fgetc(f);
    if (c != EOF) ungetc(c, f);
    return c;
}

static char* read_until(FILE *f, int delimiter, int include_delimiter) {
    char buffer[4096];
    int pos = 0;
    int c;
    while ((c = fgetc(f)) != EOF && c != delimiter) {
        if (pos < (int)(sizeof(buffer) - 1)) buffer[pos++] = c;
        if (c == '\\') {
            int esc = fgetc(f);
            if (esc != EOF && pos < (int)(sizeof(buffer) - 1)) buffer[pos++] = esc;
        }
    }
    if (include_delimiter && c == delimiter) {
        if (pos < (int)(sizeof(buffer) - 1)) buffer[pos++] = delimiter;
    }
    buffer[pos] = '\0';
    return strdup(buffer);
}

static char* read_quoted_literal(FILE *f, int delimiter) {
    char buffer[4096];
    int pos = 0;
    int c;
    buffer[pos++] = delimiter;
    while ((c = fgetc(f)) != EOF && c != delimiter) {
        if (pos < (int)(sizeof(buffer) - 1)) buffer[pos++] = c;
        if (c == '\\') {
            int esc = fgetc(f);
            if (esc != EOF && pos < (int)(sizeof(buffer) - 1)) buffer[pos++] = esc;
        }
    }
    if (c == delimiter) {
        if (pos < (int)(sizeof(buffer) - 1)) buffer[pos++] = delimiter;
    }
    buffer[pos] = '\0';
    return strdup(buffer);
}

/* ------------------------------------------------------------------
 * Основная функция получения следующего токена
 * ------------------------------------------------------------------ */

Token* get_next_token(FILE *f) {
    int c;

    while ((c = fgetc(f)) != EOF && isspace(c)) { }
    if (c == EOF) {
        Token *tok = malloc(sizeof(Token));
        tok->type = TOKEN_EOF;
        tok->value = strdup("EOF");
        return tok;
    }

    // Комментарии
    if (c == '/') {
        int next = peek_char(f);
        if (next == '/') { // однострочный
            fgetc(f);
            char *content = read_until(f, '\n', 0);
            Token *tok = malloc(sizeof(Token));
            tok->type = TOKEN_COMMENT;
            tok->value = malloc(strlen(content) + 3);
            sprintf(tok->value, "//%s", content);
            free(content);
            return tok;
        } else if (next == '*') { // многострочный
            fgetc(f);
            char buffer[4096];
            int pos = 0;
            int prev = 0;
            while ((c = fgetc(f)) != EOF) {
                if (prev == '*' && c == '/') {
                    break;
                }
                if (pos < (int)(sizeof(buffer) - 1)) buffer[pos++] = c;
                prev = c;
            }
            // Убираем последний символ, если это '*'
            if (pos > 0 && buffer[pos-1] == '*') {
                pos--;
            }
            buffer[pos] = '\0';
            Token *tok = malloc(sizeof(Token));
            tok->type = TOKEN_COMMENT;
            tok->value = malloc(strlen(buffer) + 5);
            sprintf(tok->value, "/*%s*/", buffer);
            return tok;
        }
    }

    // Препроцессор
    if (c == '#') {
        char buffer[256];
        int pos = 0;
        buffer[pos++] = c;
        while ((c = fgetc(f)) != EOF && !isspace(c) && c != '\n') {
            if (pos < (int)(sizeof(buffer) - 1)) buffer[pos++] = c;
        }
        buffer[pos] = '\0';
        Token *tok = malloc(sizeof(Token));
        tok->type = TOKEN_PREPROCESSOR;
        tok->value = strdup(buffer);
        return tok;
    }

    // Строки
    if (c == '"') {
        char *content = read_quoted_literal(f, '"');
        Token *tok = malloc(sizeof(Token));
        tok->type = TOKEN_STRING;
        tok->value = content;
        return tok;
    }

    // Символы
    if (c == '\'') {
        char *content = read_quoted_literal(f, '\'');
        Token *tok = malloc(sizeof(Token));
        tok->type = TOKEN_CHAR;
        tok->value = content;
        return tok;
    }

    // Идентификаторы и ключевые слова
    if (isalpha(c) || c == '_') {
        char buffer[256];
        int pos = 0;
        buffer[pos++] = c;
        while ((c = fgetc(f)) != EOF && is_idchar(c)) {
            if (pos < (int)(sizeof(buffer) - 1)) buffer[pos++] = c;
        }
        if (c != EOF) ungetc(c, f);
        buffer[pos] = '\0';
        Token *tok = malloc(sizeof(Token));
        tok->type = is_keyword(buffer) ? TOKEN_KEYWORD : TOKEN_IDENTIFIER;
        tok->value = strdup(buffer);
        return tok;
    }

    // Числа
    if (isdigit(c) || (c == '.' && isdigit(peek_char(f)))) {
        char buffer[256];
        int pos = 0;
        buffer[pos++] = c;
        int has_dot = (c == '.');
        while ((c = fgetc(f)) != EOF) {
            if (isdigit(c)) {
                buffer[pos++] = c;
            } else if (c == '.' && !has_dot) {
                buffer[pos++] = c;
                has_dot = 1;
            } else if (c == 'e' || c == 'E' || c == 'p' || c == 'P') {
                buffer[pos++] = c;
                int next = peek_char(f);
                if (next == '+' || next == '-') {
                    buffer[pos++] = fgetc(f);
                }
            } else if (c == 'x' || c == 'X') {
                buffer[pos++] = c;
            } else if (isalpha(c) && pos < (int)(sizeof(buffer)-1)) {
                buffer[pos++] = c;
            } else {
                break;
            }
            if (pos >= (int)(sizeof(buffer)-1)) break;
        }
        if (c != EOF) ungetc(c, f);
        buffer[pos] = '\0';
        Token *tok = malloc(sizeof(Token));
        tok->type = TOKEN_NUMBER;
        tok->value = strdup(buffer);
        return tok;
    }

    // Операторы и пунктуаторы
    char op[4] = {0};
    op[0] = c;
    int next1 = peek_char(f);
    if ((c == '=' || c == '!' || c == '<' || c == '>' || c == '&' || c == '|') && next1 == '=') {
        op[1] = fgetc(f);
    } else if (c == '+' && next1 == '+') { op[1] = fgetc(f); }
    else if (c == '-' && next1 == '-') { op[1] = fgetc(f); }
    else if (c == '-' && next1 == '>') { op[1] = fgetc(f); }
    else if (c == '&' && next1 == '&') { op[1] = fgetc(f); }
    else if (c == '|' && next1 == '|') { op[1] = fgetc(f); }
    else if (c == '<' && next1 == '<') {
        op[1] = fgetc(f);
        if (peek_char(f) == '=') op[2] = fgetc(f);
    }
    else if (c == '>' && next1 == '>') {
        op[1] = fgetc(f);
        if (peek_char(f) == '=') op[2] = fgetc(f);
    }
    else if (c == '+' && next1 == '=') { op[1] = fgetc(f); }
    else if (c == '-' && next1 == '=') { op[1] = fgetc(f); }
    else if (c == '*' && next1 == '=') { op[1] = fgetc(f); }
    else if (c == '/' && next1 == '=') { op[1] = fgetc(f); }
    else if (c == '%' && next1 == '=') { op[1] = fgetc(f); }
    else if (c == '^' && next1 == '=') { op[1] = fgetc(f); }
    else if (c == '&' && next1 == '=') { op[1] = fgetc(f); }
    else if (c == '|' && next1 == '=') { op[1] = fgetc(f); }
    else if (c == '.' && next1 == '.' && peek_char(f) == '.') {
        op[1] = fgetc(f); op[2] = fgetc(f);
    }

    Token *tok = malloc(sizeof(Token));
    tok->type = is_operator(op) ? TOKEN_OPERATOR : TOKEN_PUNCTUATOR;
    tok->value = strdup(op);
    return tok;
}

/* ------------------------------------------------------------------
 * Проверочные функции
 * ------------------------------------------------------------------ */

int is_keyword(const char *str) {
    for (int i = 0; i < (int)NUM_KEYWORDS; i++) {
        if (strcmp(str, keywords[i]) == 0) return 1;
    }
    return 0;
}

int is_operator(const char *str) {
    for (int i = 0; i < (int)NUM_OPERATORS; i++) {
        if (strcmp(str, operators[i]) == 0) return 1;
    }
    return 0;
}

void free_token(Token *tok) {
    if (tok) {
        free(tok->value);
        free(tok);
    }
}