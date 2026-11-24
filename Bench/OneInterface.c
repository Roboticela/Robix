#include <stdio.h>

struct Work {
    int (*step)(struct Work *self, int value);
};

static int only_step(struct Work *self, int value) {
    (void)self;
    return value + 1;
}

int main(void) {
    struct Work worker;
    worker.step = only_step;
    int total = 0;
    for (int index = 0; index < 1000; index++) {
        total = worker.step(&worker, total);
    }
    printf("%d\n", total);
    return 0;
}
