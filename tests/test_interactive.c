/**
 * @file test_interactive.c
 * @brief Интеграционный тест для интерактивного режима.
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
#include "../src/utils.h"

int main(void) {
    ensure_dir("output");
    ensure_dir("result");
    ensure_dir("dialogs"); // создаём явно для надёжности

    // 1. Создаём временный файл с командами для диалога
    FILE *cmd = fopen("temp_commands.txt", "w");
    if (!cmd) { perror("fopen temp_commands"); return 1; }
    fprintf(cmd, "Hello world\n");
    fprintf(cmd, "How are you?\n");
    fprintf(cmd, "exit\n");
    fclose(cmd);

    // 2. Перенаправляем stdin
    FILE *stdin_backup = stdin;
    stdin = fopen("temp_commands.txt", "r");
    if (!stdin) { perror("fopen stdin"); return 1; }

    // Запускаем интерактивный режим с именем "test_dialog.md"
    interactive_start("test_dialog.md", 1, 0.8, 10, 0);

    fclose(stdin);
    stdin = stdin_backup;
    remove("temp_commands.txt");

    // 3. Проверяем, что файл диалога создан в папке dialogs/
    char dialog_path[256];
    snprintf(dialog_path, sizeof(dialog_path), "dialogs/test_dialog.md");
    FILE *log = fopen(dialog_path, "r");
    if (!log) {
        fprintf(stderr, "Dialog file not created at %s\n", dialog_path);
        return 1;
    }
    fclose(log);

    // 4. Проверяем содержимое (наличие хотя бы одного сообщения)
    log = fopen(dialog_path, "r");
    if (!log) return 1;
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