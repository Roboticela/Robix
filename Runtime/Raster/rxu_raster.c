#include "rxu_raster.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char rxu_font[128][7];

typedef struct {
    int x;
    int y;
    int w;
    int h;
} Rect;

typedef struct {
    unsigned code;
    unsigned char rows[7];
    int used;
} Glyph;

static Glyph cache[128];

static int clamp_channel(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}

static int hex_nibble(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

static void parse_color(const char *text, int *red, int *green, int *blue) {
    *red = 51;
    *green = 102;
    *blue = 153;
    if (text == NULL || text[0] != '#') {
        return;
    }
    int digits[6];
    int index = 0;
    while (index < 6) {
        int nibble = hex_nibble(text[index + 1]);
        if (nibble < 0) {
            return;
        }
        digits[index] = nibble;
        index = index + 1;
    }
    *red = digits[0] * 16 + digits[1];
    *green = digits[2] * 16 + digits[3];
    *blue = digits[4] * 16 + digits[5];
}

static const unsigned char *glyph_rows(unsigned code) {
    if (code > 127) {
        code = '?';
    }
    if (cache[code].used) {
        return cache[code].rows;
    }
    int row = 0;
    while (row < 7) {
        cache[code].rows[row] = rxu_font[code][row];
        row = row + 1;
    }
    cache[code].code = code;
    cache[code].used = 1;
    return cache[code].rows;
}

static int inside(Rect rect, int x, int y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.w && y < rect.y + rect.h;
}

static Rect clip_of(Rect a, Rect b) {
    int x = a.x > b.x ? a.x : b.x;
    int y = a.y > b.y ? a.y : b.y;
    int right = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    int bottom = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    Rect out;
    out.x = x;
    out.y = y;
    out.w = right - x;
    out.h = bottom - y;
    if (out.w < 0) {
        out.w = 0;
    }
    if (out.h < 0) {
        out.h = 0;
    }
    return out;
}

static void write_pixel(RxuRaster *out, Rect damage, int x, int y, int red, int green, int blue, int alpha) {
    if (out == NULL || out->pixels == NULL) {
        return;
    }
    if (!inside(damage, x, y)) {
        return;
    }
    if (x < 0 || y < 0 || x >= out->width || y >= out->height) {
        return;
    }
    int offset = (y * out->width + x) * 4;
    int coverage = clamp_channel(alpha);
    out->pixels[offset] = (uint8_t)((red * coverage) / 255);
    out->pixels[offset + 1] = (uint8_t)((green * coverage) / 255);
    out->pixels[offset + 2] = (uint8_t)((blue * coverage) / 255);
    out->pixels[offset + 3] = (uint8_t)coverage;
    out->dirty = out->dirty + 1;
}

static void fill_rect(RxuRaster *out, Rect damage, Rect area, Rect clip, int red, int green, int blue, int alpha) {
    Rect box = clip_of(area, clip);
    box = clip_of(box, damage);
    int y = box.y;
    while (y < box.y + box.h) {
        int x = box.x;
        while (x < box.x + box.w) {
            write_pixel(out, damage, x, y, red, green, blue, alpha);
            x = x + 1;
        }
        y = y + 1;
    }
}

static void blit_ppm(RxuRaster *out, Rect damage, Rect area, Rect clip, const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fill_rect(out, damage, area, clip, 51, 102, 153, 255);
        return;
    }
    char magic[3];
    magic[0] = 0;
    magic[1] = 0;
    magic[2] = 0;
    int width = 0;
    int height = 0;
    int max_value = 0;
    if (fread(magic, 1, 2, file) != 2 || magic[0] != 'P' || magic[1] != '6' || fscanf(file, " %d %d %d", &width, &height, &max_value) != 3 || width < 1 || height < 1 || max_value != 255) {
        fclose(file);
        fill_rect(out, damage, area, clip, 51, 102, 153, 255);
        return;
    }
    fgetc(file);
    unsigned char *source = (unsigned char *)malloc((size_t)width * (size_t)height * 3u);
    if (source == NULL || fread(source, 1, (size_t)width * (size_t)height * 3u, file) != (size_t)width * (size_t)height * 3u) {
        free(source);
        fclose(file);
        fill_rect(out, damage, area, clip, 51, 102, 153, 255);
        return;
    }
    fclose(file);
    Rect box = clip_of(area, clip);
    box = clip_of(box, damage);
    int y = box.y;
    while (y < box.y + box.h) {
        int x = box.x;
        while (x < box.x + box.w) {
            int sample_x = (x - area.x) * width / area.w;
            int sample_y = (y - area.y) * height / area.h;
            if (sample_x < 0) {
                sample_x = 0;
            }
            if (sample_y < 0) {
                sample_y = 0;
            }
            if (sample_x >= width) {
                sample_x = width - 1;
            }
            if (sample_y >= height) {
                sample_y = height - 1;
            }
            unsigned char *pixel = source + ((size_t)sample_y * (size_t)width + (size_t)sample_x) * 3u;
            write_pixel(out, damage, x, y, pixel[0], pixel[1], pixel[2], 255);
            x = x + 1;
        }
        y = y + 1;
    }
    free(source);
}

static void stroke_rect(RxuRaster *out, Rect damage, Rect area, Rect clip, int red, int green, int blue) {
    Rect top;
    top.x = area.x;
    top.y = area.y;
    top.w = area.w;
    top.h = 1;
    Rect bottom;
    bottom.x = area.x;
    bottom.y = area.y + area.h - 1;
    bottom.w = area.w;
    bottom.h = 1;
    Rect left;
    left.x = area.x;
    left.y = area.y;
    left.w = 1;
    left.h = area.h;
    Rect right;
    right.x = area.x + area.w - 1;
    right.y = area.y;
    right.w = 1;
    right.h = area.h;
    fill_rect(out, damage, top, clip, red, green, blue, 255);
    fill_rect(out, damage, bottom, clip, red, green, blue, 255);
    fill_rect(out, damage, left, clip, red, green, blue, 255);
    fill_rect(out, damage, right, clip, red, green, blue, 255);
}

static void blit_text_clipped(RxuRaster *out, Rect damage, Rect area, Rect clip, const char *text, int red, int green, int blue) {
    Rect box = clip_of(area, clip);
    if (box.w <= 0 || box.h <= 0) {
        return;
    }
    int cursor = area.x + 1;
    int index = 0;
    if (text == NULL) {
        return;
    }
    while (text[index] != 0 && text[index] != '\n' && cursor < area.x + area.w) {
        unsigned code = (unsigned char)text[index];
        const unsigned char *rows = glyph_rows(code);
        int row = 0;
        while (row < 7) {
            int col = 0;
            while (col < 5) {
                int x = cursor + col;
                int y = area.y + 2 + row;
                if ((rows[row] & (1 << (4 - col))) != 0 && inside(box, x, y)) {
                    write_pixel(out, damage, x, y, red, green, blue, 255);
                }
                col = col + 1;
            }
            row = row + 1;
        }
        cursor = cursor + 6;
        index = index + 1;
    }
}

static uint64_t hash_bytes(const uint8_t *pixels, int count) {
    uint64_t hash = 1469598103934665603ull;
    int index = 0;
    while (index < count) {
        hash = hash ^ pixels[index];
        hash = hash * 1099511628211ull;
        index = index + 1;
    }
    return hash;
}

static int read_int(const char **cursor) {
    const char *text = *cursor;
    while (*text == ' ') {
        text = text + 1;
    }
    int sign = 1;
    if (*text == '-') {
        sign = -1;
        text = text + 1;
    }
    int value = 0;
    int saw = 0;
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (*text - '0');
        text = text + 1;
        saw = 1;
    }
    *cursor = text;
    if (!saw) {
        return 0;
    }
    return sign * value;
}

static void read_token(const char **cursor, char *token, int limit) {
    const char *text = *cursor;
    while (*text == ' ') {
        text = text + 1;
    }
    int index = 0;
    while (*text != 0 && *text != ' ' && index + 1 < limit) {
        token[index] = *text;
        index = index + 1;
        text = text + 1;
    }
    token[index] = 0;
    *cursor = text;
}

int rxu_raster_text(const char *text, int damage_x, int damage_y, int damage_w, int damage_h, RxuRaster *out) {
    if (out == NULL) {
        return 0;
    }
    out->pixels = NULL;
    out->width = 640;
    out->height = 480;
    out->dirty = 0;
    out->hash = 0;
    if (text == NULL) {
        return 0;
    }
    const char *scan = text;
    while (*scan != 0) {
        if (strncmp(scan, "size ", 5) == 0) {
            const char *cursor = scan + 5;
            int width = read_int(&cursor);
            int height = read_int(&cursor);
            if (width > 0 && height > 0 && width <= 4096 && height <= 4096) {
                out->width = width;
                out->height = height;
            }
        }
        const char *next = strchr(scan, '\n');
        if (next == NULL) {
            break;
        }
        scan = next + 1;
    }
    size_t bytes = (size_t)out->width * (size_t)out->height * 4u;
    out->pixels = (uint8_t *)calloc(1, bytes);
    if (out->pixels == NULL) {
        return 0;
    }
    Rect full;
    full.x = 0;
    full.y = 0;
    full.w = out->width;
    full.h = out->height;
    Rect damage = full;
    if (damage_w > 0 && damage_h > 0) {
        damage.x = damage_x;
        damage.y = damage_y;
        damage.w = damage_w;
        damage.h = damage_h;
        damage = clip_of(damage, full);
    }
    Rect stack[32];
    int depth = 0;
    stack[0] = full;
    const char *line = text;
    while (*line != 0) {
        char kind[32];
        const char *cursor = line;
        read_token(&cursor, kind, 32);
        Rect clip = stack[depth];
        if (strcmp(kind, "Clip") == 0) {
            Rect next;
            next.x = read_int(&cursor);
            next.y = read_int(&cursor);
            next.w = read_int(&cursor);
            next.h = read_int(&cursor);
            if (depth + 1 < 32) {
                depth = depth + 1;
                stack[depth] = clip_of(clip, next);
            }
        } else if (strcmp(kind, "Pop") == 0) {
            if (depth > 0) {
                depth = depth - 1;
            }
        } else if (strcmp(kind, "Text") == 0 || strcmp(kind, "Image") == 0 || strcmp(kind, "Fill") == 0 || strcmp(kind, "Border") == 0 || strcmp(kind, "Path") == 0 || strcmp(kind, "Shadow") == 0) {
            char name[64];
            read_token(&cursor, name, 64);
            Rect area;
            area.x = read_int(&cursor);
            area.y = read_int(&cursor);
            area.w = read_int(&cursor);
            area.h = read_int(&cursor);
            if (strcmp(kind, "Text") == 0) {
                while (*cursor == ' ') {
                    cursor = cursor + 1;
                }
                blit_text_clipped(out, damage, area, clip, cursor, 236, 240, 244);
            } else if (strcmp(kind, "Image") == 0) {
                char path[256];
                read_token(&cursor, path, 256);
                if (path[0] == 0) {
                    fill_rect(out, damage, area, clip, 51, 102, 153, 255);
                } else {
                    blit_ppm(out, damage, area, clip, path);
                }
            } else if (strcmp(kind, "Fill") == 0) {
                while (*cursor == ' ') {
                    cursor = cursor + 1;
                }
                int red;
                int green;
                int blue;
                parse_color(cursor, &red, &green, &blue);
                fill_rect(out, damage, area, clip, red, green, blue, 255);
            } else if (strcmp(kind, "Border") == 0) {
                stroke_rect(out, damage, area, clip, 51, 102, 153);
            } else if (strcmp(kind, "Path") == 0) {
                Rect line_box;
                line_box.x = area.x;
                line_box.y = area.y + area.h / 2;
                line_box.w = area.w;
                line_box.h = 2;
                fill_rect(out, damage, line_box, clip, 51, 102, 153, 255);
            } else if (strcmp(kind, "Shadow") == 0) {
                Rect shadow = area;
                shadow.x = shadow.x + 4;
                shadow.y = shadow.y + 4;
                fill_rect(out, damage, shadow, clip, 0, 0, 0, 96);
            }
        }
        const char *next = strchr(line, '\n');
        if (next == NULL) {
            break;
        }
        line = next + 1;
    }
    out->hash = hash_bytes(out->pixels, (int)bytes);
    return 1;
}

void rxu_raster_free(RxuRaster *out) {
    if (out == NULL) {
        return;
    }
    free(out->pixels);
    out->pixels = NULL;
}

int rxu_raster_hit(const char *text, int x, int y, char *id, int id_cap, int *hit_x, int *hit_y, int *hit_w, int *hit_h) {
    if (text == NULL || id == NULL || id_cap < 2) {
        return 0;
    }
    id[0] = 0;
    Rect clips[32];
    int depth = 0;
    clips[0].x = 0;
    clips[0].y = 0;
    clips[0].w = 4096;
    clips[0].h = 4096;
    int found = 0;
    const char *line = text;
    while (*line != 0) {
        char kind[32];
        const char *cursor = line;
        read_token(&cursor, kind, 32);
        if (strcmp(kind, "size") == 0) {
            clips[0].w = read_int(&cursor);
            clips[0].h = read_int(&cursor);
        } else if (strcmp(kind, "Clip") == 0) {
            Rect next;
            next.x = read_int(&cursor);
            next.y = read_int(&cursor);
            next.w = read_int(&cursor);
            next.h = read_int(&cursor);
            if (depth + 1 < 32) {
                depth = depth + 1;
                clips[depth] = clip_of(clips[depth - 1], next);
            }
        } else if (strcmp(kind, "Pop") == 0) {
            if (depth > 0) {
                depth = depth - 1;
            }
        } else if (strcmp(kind, "Text") == 0 || strcmp(kind, "Image") == 0 || strcmp(kind, "Fill") == 0 || strcmp(kind, "Border") == 0 || strcmp(kind, "Path") == 0 || strcmp(kind, "Shadow") == 0) {
            char name[64];
            read_token(&cursor, name, 64);
            Rect area;
            area.x = read_int(&cursor);
            area.y = read_int(&cursor);
            area.w = read_int(&cursor);
            area.h = read_int(&cursor);
            Rect visible = clip_of(area, clips[depth]);
            if (inside(visible, x, y)) {
                int index = 0;
                while (name[index] != 0 && index + 1 < id_cap) {
                    id[index] = name[index];
                    index = index + 1;
                }
                id[index] = 0;
                if (hit_x != NULL) {
                    *hit_x = area.x;
                }
                if (hit_y != NULL) {
                    *hit_y = area.y;
                }
                if (hit_w != NULL) {
                    *hit_w = area.w;
                }
                if (hit_h != NULL) {
                    *hit_h = area.h;
                }
                found = 1;
            }
        }
        const char *next = strchr(line, '\n');
        if (next == NULL) {
            break;
        }
        line = next + 1;
    }
    return found;
}

char *rxu_raster_set_text(const char *text, const char *id, const char *content) {
    if (text == NULL || id == NULL || content == NULL) {
        return NULL;
    }
    size_t cap = strlen(text) + strlen(content) + 64u;
    char *out = (char *)malloc(cap + 1u);
    if (out == NULL) {
        return NULL;
    }
    size_t used = 0;
    int changed = 0;
    const char *line = text;
    while (*line != 0) {
        const char *next = strchr(line, '\n');
        int length = next == NULL ? (int)strlen(line) : (int)(next - line);
        char kind[32];
        char name[64];
        const char *cursor = line;
        read_token(&cursor, kind, 32);
        int replaced = 0;
        if (strcmp(kind, "Text") == 0) {
            read_token(&cursor, name, 64);
            if (strcmp(name, id) == 0) {
                int x = read_int(&cursor);
                int y = read_int(&cursor);
                int w = read_int(&cursor);
                int h = read_int(&cursor);
                char rebuilt[512];
                int wrote = snprintf(rebuilt, sizeof(rebuilt), "Text %s %d %d %d %d %s\n", name, x, y, w, h, content);
                if (wrote > 0 && (size_t)wrote < sizeof(rebuilt) && used + (size_t)wrote < cap) {
                    memcpy(out + used, rebuilt, (size_t)wrote);
                    used = used + (size_t)wrote;
                    replaced = 1;
                    changed = 1;
                }
            }
        }
        if (!replaced) {
            if (used + (size_t)length + 1u >= cap) {
                free(out);
                return NULL;
            }
            memcpy(out + used, line, (size_t)length);
            used = used + (size_t)length;
            out[used] = '\n';
            used = used + 1;
        }
        if (next == NULL) {
            break;
        }
        line = next + 1;
    }
    if (!changed) {
        free(out);
        return NULL;
    }
    out[used] = 0;
    return out;
}
