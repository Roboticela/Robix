#include "rxu_raster.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    char *text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fclose(file);
        return NULL;
    }
    size_t got = fread(text, 1, (size_t)length, file);
    fclose(file);
    text[got] = 0;
    return text;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: rxu-raster display-list [--damage x y w h] [--hit x y]\n");
        return 2;
    }
    char *text = read_file(argv[1]);
    if (text == NULL) {
        fprintf(stderr, "present failed\n");
        return 1;
    }
    int damage_x = 0;
    int damage_y = 0;
    int damage_w = 0;
    int damage_h = 0;
    int hit_x = -1;
    int hit_y = -1;
    int index = 2;
    while (index < argc) {
        if (strcmp(argv[index], "--damage") == 0 && index + 4 < argc) {
            damage_x = atoi(argv[index + 1]);
            damage_y = atoi(argv[index + 2]);
            damage_w = atoi(argv[index + 3]);
            damage_h = atoi(argv[index + 4]);
            index = index + 5;
        } else if (strcmp(argv[index], "--hit") == 0 && index + 2 < argc) {
            hit_x = atoi(argv[index + 1]);
            hit_y = atoi(argv[index + 2]);
            index = index + 3;
        } else {
            index = index + 1;
        }
    }
    RxuRaster raster;
    if (!rxu_raster_text(text, damage_x, damage_y, damage_w, damage_h, &raster)) {
        fprintf(stderr, "present failed\n");
        free(text);
        return 1;
    }
    printf("HASH %016llx\n", (unsigned long long)raster.hash);
    printf("PIXELS %d\n", raster.width * raster.height);
    printf("DIRTY %d\n", raster.dirty);
    printf("SIZE %d %d\n", raster.width, raster.height);
    if (hit_x >= 0) {
        char id[64];
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
        if (rxu_raster_hit(text, hit_x, hit_y, id, 64, &x, &y, &w, &h)) {
            printf("HIT %s %d %d %d %d\n", id, x, y, w, h);
        } else {
            printf("HIT\n");
        }
    }
    rxu_raster_free(&raster);
    free(text);
    return 0;
}
