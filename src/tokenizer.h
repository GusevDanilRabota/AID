/**
 * @file tokenizer.h
 * @brief Токенизатор для языка C.
 * 
 * Разбивает поток символов на токены: ключевые слова, идентификаторы,
 * числа, строки, символы, операторы, пунктуаторы, препроцессорные директивы,
 * комментарии.
 */

#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Типы токенов.
 */
typedef enum {
    TOKEN_KEYWORD,      /**< Ключевое слово (int, if, return...) */
    TOKEN_IDENTIFIER,   /**< Идентификатор (имя переменной, функции) */
    TOKEN_NUMBER,       /**< Числовой литерал (целое, с плавающей точкой) */
    TOKEN_STRING,       /**< Строковый литерал */
    TOKEN_CHAR,         /**< Символьный литерал */
    TOKEN_OPERATOR,     /**< Оператор (+, -, =, ==, &&...) */
    TOKEN_PUNCTUATOR,   /**< Разделитель (;, (, ), {, }, [, ], ,, ...) */
    TOKEN_PREPROCESSOR, /**< Директива препроцессора (#include, #define...) */
    TOKEN_COMMENT,      /**< Комментарий (однострочный или многострочный) */
    TOKEN_EOF           /**< Конец файла */
} TokenType;

/**
 * @brief Структура токена.
 */
typedef struct {
    TokenType type;     /**< Тип токена */
    char *value;        /**< Строковое представление (выделяется динамически) */
} Token;

/**
 * @brief Получить следующий токен из файлового потока.
 * @param file открытый файл для чтения
 * @return указатель на новый токен (надо освободить через free_token) или NULL при ошибке
 */
Token* get_next_token(FILE *file);

/**
 * @brief Освободить память, занятую токеном.
 */
void free_token(Token *tok);

/**
 * @brief Проверить, является ли строка ключевым словом C.
 */
int is_keyword(const char *str);

/**
 * @brief Проверить, является ли строка оператором C.
 */
int is_operator(const char *str);

#ifdef __cplusplus
}
#endif

#endif