#include <vector>

template <typename T>
static long long sum_items(const std::vector<T> &values) {
    long long total = 0;
    for (T item : values) {
        total += item;
    }
    return total;
}

long long sum_ints(const std::vector<int> &values) {
    return sum_items(values);
}
