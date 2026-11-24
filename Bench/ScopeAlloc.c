#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int count = 0;
    for (int index = 1; index <= 1000; index++) {
        int *values = malloc(sizeof(int));
        values[0] = index;
        count += 1;
        free(values);
    }
    printf("%d\n", count);
    return 0;
}
