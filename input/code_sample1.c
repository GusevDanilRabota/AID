#include <stdio.h>

/* Простая программа, которая считает сумму чисел от 1 до N */
int main() {
    int n = 10;
    int sum = 0;
    for (int i = 1; i <= n; i++) {
        sum += i;
    }
    printf("Sum of 1 to %d is %d\n", n, sum);
    return 0;
}