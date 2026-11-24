#include <cstdio>
#include <vector>

int main() {
    int count = 0;
    for (int index = 1; index <= 1000; index++) {
        std::vector<int> values;
        values.push_back(index);
        count += static_cast<int>(values.size());
    }
    std::printf("%d\n", count);
    return 0;
}
