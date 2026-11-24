#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    size_t capacity = 8;
    size_t length = 0;
    char *text = malloc(capacity);
    for (int index = 0; index < 1000; index++) {
        if (length + 2 > capacity) {
            capacity *= 2;
            text = realloc(text, capacity);
        }
        text[length] = 'a';
        length++;
        text[length] = 0;
    }
    printf("%zu\n", length);
    free(text);
    return 0;
}
