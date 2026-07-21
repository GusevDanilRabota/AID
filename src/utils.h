/**
 * @file utils.h
 * @brief Вспомогательные функции для работы с файлами, строками и временем.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stddef.h>

/**
 * @brief Создаёт директорию, если она не существует.
 * @param path путь к директории
 */
void ensure_dir(const char *path);

/**
 * @brief Проверяет существование файла.
 * @param path путь к файлу
 * @return 1 если существует, иначе 0
 */
int file_exists(const char *path);

/**
 * @brief Формирует путь к выходному файлу, заменяя расширение на .txt.
 * @param out_dir      выходная директория
 * @param in_filename  полное имя входного файла
 * @return динамически выделенная строка (надо освободить free())
 */
char* make_output_path(const char *out_dir, const char *in_filename);

/**
 * @brief Получить текущую временную метку в формате "YYYY-MM-DD HH:MM:SS".
 * @param buffer буфер для строки
 * @param size размер буфера
 */
void get_timestamp(char *buffer, size_t size);

#endif