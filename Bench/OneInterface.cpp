#include <cstdio>

struct Work {
    virtual int step(int value) = 0;
    virtual ~Work() {}
};

struct OnlyWork : Work {
    int step(int value) override { return value + 1; }
};

int main() {
    OnlyWork worker;
    Work *face = &worker;
    int total = 0;
    for (int index = 0; index < 1000; index++) {
        total = face->step(total);
    }
    std::printf("%d\n", total);
    return 0;
}
