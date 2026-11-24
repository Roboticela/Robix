#include <iostream>
#include <string>

int main() {
    std::string text;
    for (int index = 0; index < 1000; index++) {
        text += "a";
    }
    std::cout << text.size() << "\n";
    return 0;
}
