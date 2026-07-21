/**
 * @file utils.c
 * @brief Реализация вспомогательных функций.
 */

#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#endif

void ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
#ifdef _WIN32
        if (_mkdir(path) == -1) {
#else
        if (mkdir(path, 0755) == -1) {
#endif
            perror("mkdir");
            exit(1);
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s exists but is not a directory\n", path);
        exit(1);
    }
}

int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

char* make_output_path(const char *out_dir, const char *in_filename) {
    const char *base = strrchr(in_filename, '/');
    if (!base) base = in_filename;
    else base++;
    char *name_copy = strdup(base);
    if (!name_copy) { perror("strdup"); exit(1); }
    char *dot = strrchr(name_copy, '.');
    if (dot) *dot = '\0';
    size_t out_len = strlen(out_dir) + strlen(name_copy) + 6;
    char *out_path = malloc(out_len);
    if (!out_path) { perror("malloc"); exit(1); }
    snprintf(out_path, out_len, "%s/%s.txt", out_dir, name_copy);
    free(name_copy);
    return out_path;
}

void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}