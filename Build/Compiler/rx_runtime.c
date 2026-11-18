#define _GNU_SOURCE
#include "rx_runtime.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static char *rx_cstr(int64_t text) {
    if (text == 0) {
        return "";
    }
    return (char *)(uintptr_t)text;
}

static int64_t rx_box(char *text) {
    if (text == NULL) {
        text = calloc(1, 1);
    }
    return (int64_t)(uintptr_t)text;
}

static char *rx_copy_n(const char *text, int64_t length) {
    char *copy = malloc((size_t)length + 1);
    if (copy == NULL) {
        return calloc(1, 1);
    }
    if (length > 0 && text != NULL) {
        memcpy(copy, text, (size_t)length);
    }
    copy[length] = 0;
    return copy;
}

static int64_t rx_clamp_index(int64_t length, int64_t index) {
    if (index < 0) {
        return 0;
    }
    if (index > length) {
        return length;
    }
    return index;
}

int64_t rx_list_new(void) {
    RxList *list = calloc(1, sizeof(RxList));
    return (int64_t)(uintptr_t)list;
}

int64_t rx_list_count(int64_t list) {
    RxList *value = (RxList *)(uintptr_t)list;
    if (value == NULL) {
        return 0;
    }
    return value->count;
}

int64_t rx_list_get(int64_t list, int64_t index) {
    RxList *value = (RxList *)(uintptr_t)list;
    if (value == NULL || index < 0 || index >= value->count) {
        return 0;
    }
    return value->items[index];
}

void rx_list_add(int64_t list, int64_t item) {
    RxList *value = (RxList *)(uintptr_t)list;
    if (value == NULL) {
        return;
    }
    if (value->count + 1 > value->cap) {
        int64_t cap = value->cap == 0 ? 8 : value->cap * 2;
        int64_t *items = realloc(value->items, (size_t)cap * sizeof(int64_t));
        if (items == NULL) {
            return;
        }
        value->items = items;
        value->cap = cap;
    }
    value->items[value->count] = item;
    value->count = value->count + 1;
}

void rx_list_set(int64_t list, int64_t index, int64_t item) {
    RxList *value = (RxList *)(uintptr_t)list;
    if (value == NULL || index < 0) {
        return;
    }
    while (value->count <= index) {
        rx_list_add(list, 0);
    }
    value->items[index] = item;
}

int64_t rx_obj_new(int64_t fields) {
    if (fields < 1) {
        fields = 1;
    }
    int64_t *object = calloc((size_t)fields, sizeof(int64_t));
    return (int64_t)(uintptr_t)object;
}

int64_t rx_obj_get(int64_t object, int64_t index) {
    int64_t *fields = (int64_t *)(uintptr_t)object;
    if (fields == NULL || index < 0) {
        return 0;
    }
    return fields[index];
}

void rx_obj_set(int64_t object, int64_t index, int64_t value) {
    int64_t *fields = (int64_t *)(uintptr_t)object;
    if (fields == NULL || index < 0) {
        return;
    }
    fields[index] = value;
}

int64_t rx_str_len(int64_t text) {
    return (int64_t)strlen(rx_cstr(text));
}

int64_t rx_str_eq(int64_t left, int64_t right) {
    return strcmp(rx_cstr(left), rx_cstr(right)) == 0 ? 1 : 0;
}

int64_t rx_str_concat(int64_t left, int64_t right) {
    const char *a = rx_cstr(left);
    const char *b = rx_cstr(right);
    int64_t an = (int64_t)strlen(a);
    int64_t bn = (int64_t)strlen(b);
    char *copy = malloc((size_t)an + (size_t)bn + 1);
    if (copy == NULL) {
        return rx_box(NULL);
    }
    memcpy(copy, a, (size_t)an);
    memcpy(copy + an, b, (size_t)bn);
    copy[an + bn] = 0;
    return (int64_t)(uintptr_t)copy;
}

int64_t rx_str_substr(int64_t text, int64_t start, int64_t length) {
    const char *value = rx_cstr(text);
    int64_t total = (int64_t)strlen(value);
    start = rx_clamp_index(total, start);
    if (length < 0) {
        length = 0;
    }
    if (start + length > total) {
        length = total - start;
    }
    return rx_box(rx_copy_n(value + start, length));
}

int64_t rx_str_get(int64_t text, int64_t index) {
    const char *value = rx_cstr(text);
    int64_t total = (int64_t)strlen(value);
    if (index < 0 || index >= total) {
        return 0;
    }
    return (unsigned char)value[index];
}

static int64_t rx_str_map(int64_t text, int upper) {
    const char *value = rx_cstr(text);
    int64_t total = (int64_t)strlen(value);
    char *copy = rx_copy_n(value, total);
    int64_t index = 0;
    while (index < total) {
        unsigned char character = (unsigned char)copy[index];
        copy[index] = (char)(upper ? toupper(character) : tolower(character));
        index = index + 1;
    }
    return (int64_t)(uintptr_t)copy;
}

int64_t rx_str_lower(int64_t text) {
    return rx_str_map(text, 0);
}

int64_t rx_str_upper(int64_t text) {
    return rx_str_map(text, 1);
}

int64_t rx_str_starts(int64_t text, int64_t prefix) {
    const char *value = rx_cstr(text);
    const char *head = rx_cstr(prefix);
    int64_t length = (int64_t)strlen(head);
    return strncmp(value, head, (size_t)length) == 0 ? 1 : 0;
}

int64_t rx_str_ends(int64_t text, int64_t suffix) {
    const char *value = rx_cstr(text);
    const char *tail = rx_cstr(suffix);
    int64_t total = (int64_t)strlen(value);
    int64_t length = (int64_t)strlen(tail);
    if (length > total) {
        return 0;
    }
    return memcmp(value + total - length, tail, (size_t)length) == 0 ? 1 : 0;
}

int64_t rx_str_contains(int64_t text, int64_t needle) {
    return strstr(rx_cstr(text), rx_cstr(needle)) != NULL ? 1 : 0;
}

int64_t rx_str_index(int64_t text, int64_t needle, int64_t start) {
    const char *value = rx_cstr(text);
    const char *pattern = rx_cstr(needle);
    int64_t total = (int64_t)strlen(value);
    if (start < 0) {
        start = 0;
    }
    if (start > total) {
        return -1;
    }
    const char *found = strstr(value + start, pattern);
    if (found == NULL) {
        return -1;
    }
    return (int64_t)(found - value);
}

int64_t rx_str_rindex(int64_t text, int64_t needle, int64_t start) {
    const char *value = rx_cstr(text);
    const char *pattern = rx_cstr(needle);
    int64_t total = (int64_t)strlen(value);
    int64_t length = (int64_t)strlen(pattern);
    if (start < 0 || start >= total) {
        start = total - 1;
    }
    if (length == 0) {
        return start < 0 ? 0 : start;
    }
    int64_t index = start;
    while (index >= 0) {
        if (index + length <= total && memcmp(value + index, pattern, (size_t)length) == 0) {
            return index;
        }
        if (index == 0) {
            break;
        }
        index = index - 1;
    }
    return -1;
}

int64_t rx_str_replace(int64_t text, int64_t old_text, int64_t new_text) {
    const char *value = rx_cstr(text);
    const char *old = rx_cstr(old_text);
    const char *replacement = rx_cstr(new_text);
    int64_t old_len = (int64_t)strlen(old);
    int64_t new_len = (int64_t)strlen(replacement);
    if (old_len == 0) {
        return rx_box(rx_copy_n(value, (int64_t)strlen(value)));
    }
    int64_t count = 0;
    const char *scan = value;
    while ((scan = strstr(scan, old)) != NULL) {
        count = count + 1;
        scan = scan + old_len;
    }
    int64_t total = (int64_t)strlen(value) + count * (new_len - old_len);
    char *copy = malloc((size_t)total + 1);
    if (copy == NULL) {
        return rx_box(NULL);
    }
    char *out = copy;
    scan = value;
    const char *hit = NULL;
    while ((hit = strstr(scan, old)) != NULL) {
        memcpy(out, scan, (size_t)(hit - scan));
        out += hit - scan;
        memcpy(out, replacement, (size_t)new_len);
        out += new_len;
        scan = hit + old_len;
    }
    strcpy(out, scan);
    return (int64_t)(uintptr_t)copy;
}

int64_t rx_str_to_int(int64_t text) {
    return (int64_t)strtoll(rx_cstr(text), NULL, 10);
}

double rx_str_to_double(int64_t text) {
    return strtod(rx_cstr(text), NULL);
}

static int64_t rx_f64_box(double value) {
    int64_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double rx_f64_unbox(int64_t bits) {
    double value = 0;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void rx_f64_format(double value, char *buffer, size_t size) {
    snprintf(buffer, size, "%.15g", value);
}

int64_t rx_F64Parse(int64_t text) {
    return rx_f64_box(strtod(rx_cstr(text), NULL));
}

int64_t rx_F32Parse(int64_t text) {
    return rx_f64_box((double)strtof(rx_cstr(text), NULL));
}

int64_t rx_F64Text(int64_t bits) {
    char buffer[64];
    rx_f64_format(rx_f64_unbox(bits), buffer, sizeof(buffer));
    return rx_box(rx_copy_n(buffer, (int64_t)strlen(buffer)));
}

int64_t rx_F64FromInt(int64_t value) {
    return rx_f64_box((double)value);
}

int64_t rx_F64Add(int64_t left, int64_t right) {
    return rx_f64_box(rx_f64_unbox(left) + rx_f64_unbox(right));
}

int64_t rx_F64Sub(int64_t left, int64_t right) {
    return rx_f64_box(rx_f64_unbox(left) - rx_f64_unbox(right));
}

int64_t rx_F64Mul(int64_t left, int64_t right) {
    return rx_f64_box(rx_f64_unbox(left) * rx_f64_unbox(right));
}

int64_t rx_F64Div(int64_t left, int64_t right) {
    return rx_f64_box(rx_f64_unbox(left) / rx_f64_unbox(right));
}

int64_t rx_F64Neg(int64_t bits) {
    return rx_f64_box(-rx_f64_unbox(bits));
}

int64_t rx_F64Cmp(int64_t left, int64_t right) {
    double first = rx_f64_unbox(left);
    double second = rx_f64_unbox(right);
    if (first < second) {
        return -1;
    }
    if (first > second) {
        return 1;
    }
    return 0;
}

int64_t rx_str_from_int(int64_t value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
    return rx_box(rx_copy_n(buffer, (int64_t)strlen(buffer)));
}

int64_t rx_str_from_bool(int64_t value) {
    const char *text = value ? "True" : "False";
    return rx_box(rx_copy_n(text, (int64_t)strlen(text)));
}

int64_t rx_str_from_char(int64_t value) {
    char *copy = malloc(2);
    copy[0] = (char)value;
    copy[1] = 0;
    return (int64_t)(uintptr_t)copy;
}

int64_t rx_str_from_chars(int64_t list) {
    RxList *chars = (RxList *)(uintptr_t)list;
    if (chars == NULL) {
        return rx_box(rx_copy_n("", 0));
    }
    char *copy = malloc((size_t)chars->count * 4 + 1);
    int64_t out = 0;
    int64_t index = 0;
    while (index < chars->count) {
        uint32_t code = (uint32_t)chars->items[index];
        if (code < 0x80) {
            copy[out++] = (char)code;
        } else if (code < 0x800) {
            copy[out++] = (char)(0xC0 | (code >> 6));
            copy[out++] = (char)(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            copy[out++] = (char)(0xE0 | (code >> 12));
            copy[out++] = (char)(0x80 | ((code >> 6) & 0x3F));
            copy[out++] = (char)(0x80 | (code & 0x3F));
        } else {
            copy[out++] = (char)(0xF0 | (code >> 18));
            copy[out++] = (char)(0x80 | ((code >> 12) & 0x3F));
            copy[out++] = (char)(0x80 | ((code >> 6) & 0x3F));
            copy[out++] = (char)(0x80 | (code & 0x3F));
        }
        index = index + 1;
    }
    copy[out] = 0;
    return (int64_t)(uintptr_t)copy;
}

void rx_print(int64_t value, int64_t kind) {
    if (kind == 1) {
        fputs(rx_cstr(value), stdout);
    } else if (kind == 2) {
        fputs(value ? "True" : "False", stdout);
    } else if (kind == 3) {
        fputc((int)value, stdout);
    } else if (kind == 4) {
        char buffer[64];
        rx_f64_format(rx_f64_unbox(value), buffer, sizeof(buffer));
        fputs(buffer, stdout);
    } else {
        printf("%lld", (long long)value);
    }
    fputc('\n', stdout);
    fflush(stdout);
}

int64_t rx_file_read(int64_t path) {
    FILE *file = fopen(rx_cstr(path), "rb");
    if (file == NULL) {
        return rx_box(rx_copy_n("", 0));
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size < 0) {
        size = 0;
    }
    fseek(file, 0, SEEK_SET);
    char *copy = malloc((size_t)size + 1);
    size_t got = fread(copy, 1, (size_t)size, file);
    copy[got] = 0;
    fclose(file);
    return (int64_t)(uintptr_t)copy;
}

void rx_file_write_text(int64_t path, int64_t text) {
    FILE *file = fopen(rx_cstr(path), "wb");
    if (file == NULL) {
        return;
    }
    const char *value = rx_cstr(text);
    fwrite(value, 1, strlen(value), file);
    fclose(file);
}

void rx_file_write_bytes(int64_t path, int64_t bytes) {
    FILE *file = fopen(rx_cstr(path), "wb");
    RxList *list = (RxList *)(uintptr_t)bytes;
    if (file == NULL || list == NULL) {
        if (file != NULL) {
            fclose(file);
        }
        return;
    }
    int64_t index = 0;
    while (index < list->count) {
        unsigned char byte = (unsigned char)list->items[index];
        fwrite(&byte, 1, 1, file);
        index = index + 1;
    }
    fclose(file);
}

int64_t rx_file_exists(int64_t path) {
    struct stat info;
    if (stat(rx_cstr(path), &info) != 0) {
        return 0;
    }
    return S_ISREG(info.st_mode) ? 1 : 0;
}

void rx_file_delete(int64_t path) {
    unlink(rx_cstr(path));
}

static void rx_mkdir_parents(const char *path) {
    char *copy = rx_copy_n(path, (int64_t)strlen(path));
    int64_t length = (int64_t)strlen(copy);
    int64_t index = 0;
    while (index < length) {
        if (copy[index] == '/' && index > 0) {
            copy[index] = 0;
            mkdir(copy, 0755);
            copy[index] = '/';
        }
        index = index + 1;
    }
    mkdir(copy, 0755);
    free(copy);
}

void rx_dir_create(int64_t path) {
    rx_mkdir_parents(rx_cstr(path));
}

static void rx_remove_tree(const char *path) {
    struct stat info;
    if (lstat(path, &info) != 0) {
        return;
    }
    if (!S_ISDIR(info.st_mode)) {
        unlink(path);
        return;
    }
    DIR *dir = opendir(path);
    if (dir == NULL) {
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child[4096];
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        rx_remove_tree(child);
    }
    closedir(dir);
    rmdir(path);
}

void rx_dir_delete(int64_t path) {
    rx_remove_tree(rx_cstr(path));
}

int64_t rx_dir_exists(int64_t path) {
    struct stat info;
    if (stat(rx_cstr(path), &info) != 0) {
        return 0;
    }
    return S_ISDIR(info.st_mode) ? 1 : 0;
}

static void rx_walk(int64_t list, const char *root, const char *relative) {
    char full[4096];
    if (relative[0] == 0) {
        snprintf(full, sizeof(full), "%s", root);
    } else {
        snprintf(full, sizeof(full), "%s/%s", root, relative);
    }
    DIR *dir = opendir(full);
    if (dir == NULL) {
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child_rel[4096];
        if (relative[0] == 0) {
            snprintf(child_rel, sizeof(child_rel), "%s", entry->d_name);
        } else {
            snprintf(child_rel, sizeof(child_rel), "%s/%s", relative, entry->d_name);
        }
        char child_full[4096];
        snprintf(child_full, sizeof(child_full), "%s/%s", root, child_rel);
        struct stat info;
        if (stat(child_full, &info) != 0) {
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            rx_walk(list, root, child_rel);
        } else if (S_ISREG(info.st_mode)) {
            rx_list_add(list, rx_box(rx_copy_n(child_rel, (int64_t)strlen(child_rel))));
        }
    }
    closedir(dir);
}

int64_t rx_dir_files(int64_t path) {
    int64_t list = rx_list_new();
    const char *root = rx_cstr(path);
    if (root[0] == 0) {
        root = ".";
    }
    rx_walk(list, root, "");
    return list;
}

int64_t rx_env_get(int64_t name) {
    const char *value = getenv(rx_cstr(name));
    if (value == NULL) {
        return rx_box(rx_copy_n("", 0));
    }
    return rx_box(rx_copy_n(value, (int64_t)strlen(value)));
}

int64_t rx_process(int64_t command) {
    int code = system(rx_cstr(command));
    if (code == -1) {
        return 1;
    }
    if (WIFEXITED(code)) {
        return WEXITSTATUS(code);
    }
    return 1;
}

int64_t rx_args(int32_t argc, char **argv) {
    int64_t list = rx_list_new();
    int32_t index = 1;
    while (index < argc) {
        rx_list_add(list, rx_box(rx_copy_n(argv[index], (int64_t)strlen(argv[index]))));
        index = index + 1;
    }
    return list;
}

void rx_finish_result(int64_t result) {
    (void)result;
}
