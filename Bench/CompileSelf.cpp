#include <cstdio>
#include <string>

int main() {
    std::string text = "int main() { return 0; }";
    int count = 0;
    for (char character : text) {
        if (character == '{' || character == '}') {
            count++;
        }
    }
    std::printf("%d\n", count);
    return 0;
}
