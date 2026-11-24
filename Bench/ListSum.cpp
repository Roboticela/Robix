#include <cstdio>
#include <vector>

long long sum_ints(const std::vector<int> &values);

int main() {
    std::vector<int> values;
    for (int index = 1; index <= 1000; index++) {
        values.push_back(index);
    }
    std::printf("%lld\n", sum_ints(values));
    return 0;
}
