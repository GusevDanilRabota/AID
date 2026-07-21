/**
 * @file test_interactive.c
 * @brief Тесты для интерактивного модуля.
 *
 * Проверяет, что interactive_start создаёт файл диалога
 * и корректно обрабатывает команды.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#endif

#include "../src/interactive.h"

static void ensure_dir(const char *path) {
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
    }
}

int main(void) {
    ensure_dir("output");
    ensure_dir("result");

    // 1. Создаём временный файл с командами
    FILE *cmd = fopen("temp_commands.txt", "w");
    if (!cmd) { perror("fopen temp_commands"); return 1; }
    fprintf(cmd, "Hello world\n");
    fprintf(cmd, "How are you?\n");
    fprintf(cmd, "exit\n");
    fclose(cmd);

    // 2. Перенаправляем stdin через freopen
    if (freopen("temp_commands.txt", "r", stdin) == NULL) {
        perror("freopen stdin");
        return 1;
    }

    // 3. Запускаем interactive с фиксированным именем лога
    interactive_start("test_dialog.md", 1, 0.8, 10, 0);

    // 4. Возвращаем stdin в исходное состояние (перенаправляем на консоль)
    // В Windows это может быть CON:, в Unix /dev/tty
#ifdef _WIN32
    freopen("CON:", "r", stdin);
#else
    freopen("/dev/tty", "r", stdin);
#endif

    // Удаляем временный файл с командами
    remove("temp_commands.txt");

    // 5. Проверяем, что файл диалога создан
    FILE *log = fopen("test_dialog.md", "r");
    if (!log) {
        fprintf(stderr, "Dialog file not created\n");
        return 1;
    }

    // 6. Проверяем содержимое (хотя бы наличие заголовка и сообщений)
    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), log)) {
        if (strstr(line, "**User:** Hello world")) found++;
        if (strstr(line, "**User:** How are you?")) found++;
        if (strstr(line, "**User:** exit") || strstr(line, "Выход из диалога")) found++;
    }
    fclose(log);

    if (found >= 3) {
        printf("Interactive test passed.\n");
        FILE *res = fopen("output/test_interactive_results.txt", "w");
        if (res) {
            fprintf(res, "PASS: interactive test\n");
            fclose(res);
        }
        return 0;
    } else {
        fprintf(stderr, "Interactive test failed: not all messages found.\n");
        FILE *res = fopen("output/test_interactive_results.txt", "w");
        if (res) {
            fprintf(res, "FAIL: interactive test\n");
            fclose(res);
        }
        return 1;
    }
}