#include <stdio.h>

/* Функция нахождения максимума в массиве */
int max_array(int arr[], int size) {
    int max = arr[0];
    for (int i = 1; i < size; i++) {
        if (arr[i] > max) {
            max = arr[i];
        }
    }
    return max;
}

int main() {
    int numbers[] = {3, 7, 2, 9, 5};
    int size = sizeof(numbers) / sizeof(numbers[0]);
    int max = max_array(numbers, size);
    printf("Max: %d\n", max);
    return 0;
}