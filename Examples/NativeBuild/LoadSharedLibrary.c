#include <stdio.h>
#include <windows.h>

typedef long long (*AddFunction)(long long left, long long right);

int main(void) {
    HMODULE library = LoadLibraryA("Build\\NativeBuild\\add.dll");
    AddFunction add = library ? (AddFunction)GetProcAddress(library, "Add") : NULL;
    if (!add) {
        printf("missing Add\n");
        return 1;
    }
    printf("%lld\n", add(20, 22));
    return 0;
}
