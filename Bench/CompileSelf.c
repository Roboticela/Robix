#include <stdio.h>
#include <string.h>

int main(void) {
    const char *text = "int main(void) { return 0; }";
    int count = 0;
    for (size_t index = 0; index < strlen(text); index++) {
        if (text[index] == '{' || text[index] == '}') {
            count++;
        }
    }
    printf("%d\n", count);
    return 0;
}
