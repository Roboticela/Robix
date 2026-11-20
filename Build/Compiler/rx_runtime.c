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

static int64_t rx_export(int64_t pointer, int kind, int64_t aux);

static int64_t rx_box(char *text) {
    if (text == NULL) {
        text = calloc(1, 1);
    }
    return rx_export((int64_t)(uintptr_t)text, 1, 0);
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

typedef struct RxMap {
    int64_t *keys;
    int64_t *values;
    int64_t count;
    int64_t cap;
    int64_t kind;
} RxMap;

/* Deterministic ownership. Not a garbage collector: every heap string, list,
   map, and object has a refcount, and the compiler releases it at scope exit
   or when a temporary dies. Raw Memory.Allocate pointers are not tracked. */
enum { RX_KIND_STR = 1, RX_KIND_LIST = 2, RX_KIND_OBJ = 3, RX_KIND_MAP = 4 };

typedef struct RxOwn {
    int64_t pointer;
    int32_t count;
    int32_t kind;
    int32_t aux;
    int32_t state;
} RxOwn;

enum { RX_EMPTY = 0, RX_LIVE = 1, RX_TOMB = 2 };

static RxOwn *rx_owns = NULL;
static int rx_own_cap = 0;
static int rx_own_live = 0;
static int64_t *rx_temps = NULL;
static int rx_temp_count = 0;
static int rx_temp_cap = 0;
static int *rx_marks = NULL;
static int rx_mark_count = 0;
static int rx_mark_cap = 0;

static uint64_t rx_own_hash(int64_t pointer) {
    return (uint64_t)pointer * 11400714819323198485ull;
}

static void rx_own_grow(void) {
    int next = rx_own_cap == 0 ? 1024 : rx_own_cap * 2;
    RxOwn *grown = calloc((size_t)next, sizeof(RxOwn));
    int index = 0;
    if (grown == NULL) {
        return;
    }
    while (index < rx_own_cap) {
        if (rx_owns[index].state == RX_LIVE) {
            uint64_t slot = rx_own_hash(rx_owns[index].pointer) % (uint64_t)next;
            while (grown[slot].state == RX_LIVE) {
                slot = (slot + 1) % (uint64_t)next;
            }
            grown[slot] = rx_owns[index];
        }
        index = index + 1;
    }
    free(rx_owns);
    rx_owns = grown;
    rx_own_cap = next;
}

static RxOwn *rx_own_find(int64_t pointer, int create) {
    uint64_t slot = 0;
    uint64_t first_tomb = (uint64_t)-1;
    int seen = 0;
    if (pointer == 0) {
        return NULL;
    }
    if (rx_own_cap == 0) {
        if (!create) {
            return NULL;
        }
        rx_own_grow();
    }
    slot = rx_own_hash(pointer) % (uint64_t)rx_own_cap;
    while (seen < rx_own_cap) {
        RxOwn *entry = &rx_owns[slot];
        if (entry->state == RX_EMPTY) {
            if (!create) {
                return NULL;
            }
            if (first_tomb != (uint64_t)-1) {
                return &rx_owns[first_tomb];
            }
            return entry;
        }
        if (entry->state == RX_TOMB && first_tomb == (uint64_t)-1) {
            first_tomb = slot;
        }
        if (entry->state == RX_LIVE && entry->pointer == pointer) {
            return entry;
        }
        slot = (slot + 1) % (uint64_t)rx_own_cap;
        seen = seen + 1;
    }
    if (create && first_tomb != (uint64_t)-1) {
        return &rx_owns[first_tomb];
    }
    return NULL;
}

void rx_release(int64_t pointer);

static void rx_destroy_own(int64_t pointer, int kind, int aux) {
    if (kind == RX_KIND_LIST) {
        RxList *list = (RxList *)(uintptr_t)pointer;
        int64_t count = list == NULL ? 0 : list->count;
        int64_t *items = list == NULL ? NULL : list->items;
        int64_t index = 0;
        if (list != NULL) {
            list->items = NULL;
            list->count = 0;
        }
        while (index < count) {
            rx_release(items[index]);
            index = index + 1;
        }
        free(items);
        free(list);
        return;
    }
    if (kind == RX_KIND_MAP) {
        RxMap *map = (RxMap *)(uintptr_t)pointer;
        int64_t count = map == NULL ? 0 : map->count;
        int64_t *keys = map == NULL ? NULL : map->keys;
        int64_t *values = map == NULL ? NULL : map->values;
        int64_t index = 0;
        if (map != NULL) {
            map->keys = NULL;
            map->values = NULL;
            map->count = 0;
        }
        while (index < count) {
            rx_release(keys[index]);
            rx_release(values[index]);
            index = index + 1;
        }
        free(keys);
        free(values);
        free(map);
        return;
    }
    if (kind == RX_KIND_OBJ) {
        int64_t *fields = (int64_t *)(uintptr_t)pointer;
        int64_t index = 0;
        while (index < aux) {
            int64_t child = fields[index];
            fields[index] = 0;
            rx_release(child);
            index = index + 1;
        }
        free(fields);
        return;
    }
    free((void *)(uintptr_t)pointer);
}

static void rx_track(int64_t pointer, int kind, int64_t aux) {
    RxOwn *entry = NULL;
    if (pointer == 0) {
        return;
    }
    if (rx_own_cap == 0 || rx_own_live * 10 > rx_own_cap * 7) {
        rx_own_grow();
    }
    entry = rx_own_find(pointer, 1);
    if (entry == NULL) {
        return;
    }
    if (entry->state == RX_LIVE && entry->pointer == pointer) {
        entry->count = entry->count + 1;
        return;
    }
    entry->pointer = pointer;
    entry->count = 1;
    entry->kind = kind;
    entry->aux = (int32_t)aux;
    entry->state = RX_LIVE;
    rx_own_live = rx_own_live + 1;
}

static void rx_temp_push(int64_t pointer) {
    if (pointer == 0) {
        return;
    }
    if (rx_temp_count == rx_temp_cap) {
        int next = rx_temp_cap == 0 ? 256 : rx_temp_cap * 2;
        int64_t *grown = realloc(rx_temps, (size_t)next * sizeof(int64_t));
        if (grown == NULL) {
            return;
        }
        rx_temps = grown;
        rx_temp_cap = next;
    }
    rx_temps[rx_temp_count] = pointer;
    rx_temp_count = rx_temp_count + 1;
}

static int64_t rx_export(int64_t pointer, int kind, int64_t aux) {
    if (pointer == 0) {
        return 0;
    }
    rx_track(pointer, kind, aux);
    rx_temp_push(pointer);
    return pointer;
}

void rx_release(int64_t pointer) {
    RxOwn *entry = rx_own_find(pointer, 0);
    int kind = 0;
    int aux = 0;
    if (entry == NULL || entry->state != RX_LIVE) {
        return;
    }
    entry->count = entry->count - 1;
    if (entry->count > 0) {
        return;
    }
    kind = entry->kind;
    aux = entry->aux;
    entry->state = RX_TOMB;
    entry->pointer = 0;
    entry->count = 0;
    rx_own_live = rx_own_live - 1;
    rx_destroy_own(pointer, kind, aux);
}

void rx_retain(int64_t pointer) {
    RxOwn *entry = rx_own_find(pointer, 0);
    if (entry == NULL || entry->state != RX_LIVE) {
        return;
    }
    entry->count = entry->count + 1;
}

void rx_scope_push(void) {
    if (rx_mark_count == rx_mark_cap) {
        int next = rx_mark_cap == 0 ? 64 : rx_mark_cap * 2;
        int *grown = realloc(rx_marks, (size_t)next * sizeof(int));
        if (grown == NULL) {
            return;
        }
        rx_marks = grown;
        rx_mark_cap = next;
    }
    rx_marks[rx_mark_count] = rx_temp_count;
    rx_mark_count = rx_mark_count + 1;
}

void rx_scope_pop(void) {
    int mark = 0;
    if (rx_mark_count > 0) {
        rx_mark_count = rx_mark_count - 1;
        mark = rx_marks[rx_mark_count];
    }
    while (rx_temp_count > mark) {
        rx_temp_count = rx_temp_count - 1;
        rx_release(rx_temps[rx_temp_count]);
    }
}

void rx_give(int64_t pointer) {
    rx_temp_push(pointer);
}

int64_t rx_mark_depth(void) {
    return rx_mark_count;
}

void rx_scope_unwind(int64_t depth) {
    while (rx_mark_count > depth) {
        rx_scope_pop();
    }
}

int64_t rx_keep(int64_t pointer) {
    int start = 0;
    int index = 0;
    if (pointer == 0) {
        return 0;
    }
    if (rx_mark_count > 0) {
        start = rx_marks[rx_mark_count - 1];
    }
    index = rx_temp_count - 1;
    while (index >= start) {
        if (rx_temps[index] == pointer) {
            rx_temps[index] = rx_temps[rx_temp_count - 1];
            rx_temp_count = rx_temp_count - 1;
            return 1;
        }
        index = index - 1;
    }
    return 0;
}

int64_t rx_alloc(int64_t bytes) {
    void *memory = NULL;
    if (bytes < 1) {
        bytes = 1;
    }
    memory = calloc(1, (size_t)bytes);
    return (int64_t)(uintptr_t)memory;
}

int64_t rx_alloc_aligned(int64_t bytes, int64_t alignment) {
    void *memory = NULL;
    if (bytes < 1) {
        bytes = 1;
    }
    if (alignment < sizeof(void *)) {
        alignment = (int64_t)sizeof(void *);
    }
    if ((alignment & (alignment - 1)) != 0) {
        alignment = (int64_t)sizeof(void *);
    }
    if (posix_memalign(&memory, (size_t)alignment, (size_t)bytes) != 0) {
        return 0;
    }
    memset(memory, 0, (size_t)bytes);
    return (int64_t)(uintptr_t)memory;
}

static int rx_key_eq(int64_t kind, int64_t left, int64_t right) {
    int64_t left_tag = 0;
    int64_t right_tag = 0;
    int64_t left_payload = 0;
    int64_t right_payload = 0;
    if (kind == 1) {
        return strcmp(rx_cstr(left), rx_cstr(right)) == 0;
    }
    if (kind == 2) {
        left_tag = rx_obj_get(left, 0);
        right_tag = rx_obj_get(right, 0);
        left_payload = rx_obj_get(left, 1);
        right_payload = rx_obj_get(right, 1);
        if (left_tag != right_tag) {
            return 0;
        }
        if (left_tag == 2) {
            return strcmp(rx_cstr(left_payload), rx_cstr(right_payload)) == 0;
        }
        return left_payload == right_payload;
    }
    return left == right;
}

int64_t rx_map_new(int64_t kind) {
    RxMap *map = calloc(1, sizeof(RxMap));
    if (map != NULL) {
        map->kind = kind;
    }
    return rx_export((int64_t)(uintptr_t)map, RX_KIND_MAP, 0);
}

void rx_map_put(int64_t map, int64_t key, int64_t item) {
    RxMap *value = (RxMap *)(uintptr_t)map;
    int64_t index = 0;
    int64_t cap = 0;
    int64_t *keys = NULL;
    int64_t *items = NULL;
    if (value == NULL) {
        return;
    }
    while (index < value->count) {
        if (rx_key_eq(value->kind, value->keys[index], key)) {
            rx_release(value->values[index]);
            rx_retain(item);
            value->values[index] = item;
            return;
        }
        index = index + 1;
    }
    if (value->count + 1 > value->cap) {
        cap = value->cap == 0 ? 8 : value->cap * 2;
        keys = realloc(value->keys, (size_t)cap * sizeof(int64_t));
        items = realloc(value->values, (size_t)cap * sizeof(int64_t));
        if (keys == NULL || items == NULL) {
            return;
        }
        value->keys = keys;
        value->values = items;
        value->cap = cap;
    }
    rx_retain(key);
    rx_retain(item);
    value->keys[value->count] = key;
    value->values[value->count] = item;
    value->count = value->count + 1;
}

int64_t rx_map_get(int64_t map, int64_t key) {
    RxMap *value = (RxMap *)(uintptr_t)map;
    int64_t index = 0;
    if (value == NULL) {
        return 0;
    }
    while (index < value->count) {
        if (rx_key_eq(value->kind, value->keys[index], key)) {
            return value->values[index];
        }
        index = index + 1;
    }
    return 0;
}

void rx_set_add(int64_t list, int64_t item, int64_t kind) {
    RxList *value = (RxList *)(uintptr_t)list;
    int64_t index = 0;
    if (value == NULL) {
        return;
    }
    while (index < value->count) {
        if (rx_key_eq(kind, value->items[index], item)) {
            return;
        }
        index = index + 1;
    }
    rx_list_add(list, item);
}

int64_t rx_set_has(int64_t list, int64_t item, int64_t kind) {
    RxList *value = (RxList *)(uintptr_t)list;
    int64_t index = 0;
    if (value == NULL) {
        return 0;
    }
    while (index < value->count) {
        if (rx_key_eq(kind, value->items[index], item)) {
            return 1;
        }
        index = index + 1;
    }
    return 0;
}

int64_t rx_list_new(void) {
    RxList *list = calloc(1, sizeof(RxList));
    return rx_export((int64_t)(uintptr_t)list, RX_KIND_LIST, 0);
}

int64_t rx_list_count(int64_t list) {
    RxList *value = (RxList *)(uintptr_t)list;
    if (value == NULL) {
        return 0;
    }
    return value->count;
}

int64_t rx_list_take_front(int64_t list) {
    RxList *value = (RxList *)(uintptr_t)list;
    int64_t item = 0;
    int64_t index = 0;
    if (value == NULL || value->count <= 0) {
        return 0;
    }
    item = value->items[0];
    if (rx_own_find(item, 0) != NULL) {
        rx_temp_push(item);
    }
    while (index + 1 < value->count) {
        value->items[index] = value->items[index + 1];
        index = index + 1;
    }
    value->count = value->count - 1;
    return item;
}

int64_t rx_list_take_back(int64_t list) {
    RxList *value = (RxList *)(uintptr_t)list;
    int64_t item = 0;
    if (value == NULL || value->count <= 0) {
        return 0;
    }
    value->count = value->count - 1;
    item = value->items[value->count];
    if (rx_own_find(item, 0) != NULL) {
        rx_temp_push(item);
    }
    return item;
}

int64_t rx_range(int64_t start, int64_t end, int64_t inclusive) {
    int64_t list = rx_list_new();
    int64_t last = end;
    int64_t cursor = start;
    if (inclusive == 0) {
        last = end - 1;
    }
    while (cursor <= last) {
        rx_list_add(list, cursor);
        cursor = cursor + 1;
    }
    return list;
}

int64_t rx_BitAnd(int64_t left, int64_t right) { return left & right; }
int64_t rx_BitOr(int64_t left, int64_t right) { return left | right; }
int64_t rx_BitXor(int64_t left, int64_t right) { return left ^ right; }

int64_t rx_Shl(int64_t left, int64_t right) {
    if (right < 0 || right >= 64) {
        return 0;
    }
    return left << (unsigned)right;
}

int64_t rx_Shr(int64_t left, int64_t right) {
    if (right < 0 || right >= 64) {
        return left < 0 ? -1 : 0;
    }
    return left >> (unsigned)right;
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
    if (rx_own_find(list, 0) != NULL) {
        rx_retain(item);
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
    if (rx_own_find(list, 0) != NULL) {
        rx_release(value->items[index]);
        rx_retain(item);
    }
    value->items[index] = item;
}

int64_t rx_obj_new(int64_t fields) {
    int64_t *object = NULL;
    if (fields < 1) {
        fields = 1;
    }
    object = calloc((size_t)fields, sizeof(int64_t));
    return rx_export((int64_t)(uintptr_t)object, RX_KIND_OBJ, fields);
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
    RxOwn *owner = rx_own_find(object, 0);
    if (fields == NULL || index < 0) {
        return;
    }
    if (owner != NULL && owner->state == RX_LIVE && index < owner->aux) {
        rx_retain(value);
        rx_release(fields[index]);
    }
    fields[index] = value;
}

void rx_mem_copy(int64_t dest, int64_t src, int64_t count) {
    if (dest == 0 || src == 0 || count <= 0) {
        return;
    }
    memmove((void *)(uintptr_t)dest, (void *)(uintptr_t)src, (size_t)count);
}

void rx_free(int64_t pointer) {
    RxOwn *entry = NULL;
    if (pointer == 0) {
        return;
    }
    entry = rx_own_find(pointer, 0);
    if (entry != NULL && entry->state == RX_LIVE) {
        entry->count = 1;
        rx_release(pointer);
        return;
    }
    free((void *)(uintptr_t)pointer);
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
    return rx_box(copy);
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

int64_t rx_FormatFixed(int64_t text, int64_t places) {
    const char *raw = rx_cstr(text);
    if (places < 0) {
        return rx_box(rx_copy_n(raw, (int64_t)strlen(raw)));
    }
    if (places > 18) {
        places = 18;
    }
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%.*f", (int)places, strtod(raw, NULL));
    return rx_box(rx_copy_n(buffer, (int64_t)strlen(buffer)));
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

int64_t rx_F64ToInt(int64_t bits) {
    return (int64_t)rx_f64_unbox(bits);
}

int64_t rx_F64ToIntMode(int64_t bits, int64_t mode) {
    double value = rx_f64_unbox(bits);
    if (mode == 0) {
        if (value >= 0) {
            value = value + 0.5;
        } else {
            value = value - 0.5;
        }
    }
    return (int64_t)value;
}

int64_t rx_F64ToIntChecked(int64_t bits) {
    double value = rx_f64_unbox(bits);
    if (!(value == value) || value >= 9223372036854775808.0 || value < -9223372036854775808.0) {
        fputs("error RX121 Conversion is out of range.\n", stderr);
        exit(1);
    }
    return (int64_t)value;
}

int64_t rx_F64Format(int64_t bits, int64_t format) {
    const char *spec = rx_cstr(format);
    double value = rx_f64_unbox(bits);
    char buffer[128];
    int fixed = 0;
    int places = 0;
    if ((spec[0] == 'F' || spec[0] == 'f') && spec[1] >= '0' && spec[1] <= '9') {
        fixed = 1;
        const char *scan = spec + 1;
        while (*scan != 0) {
            if (*scan < '0' || *scan > '9') {
                fixed = 0;
                break;
            }
            places = places * 10 + (*scan - '0');
            scan++;
        }
    }
    if (fixed) {
        if (places > 18) {
            places = 18;
        }
        snprintf(buffer, sizeof(buffer), "%.*f", places, value);
    } else {
        rx_f64_format(value, buffer, sizeof(buffer));
    }
    return rx_box(rx_copy_n(buffer, (int64_t)strlen(buffer)));
}

int64_t rx_str_is_int(int64_t text) {
    const char *raw = rx_cstr(text);
    if (raw[0] == 0) {
        return 0;
    }
    size_t index = 0;
    if (raw[0] == '+' || raw[0] == '-') {
        if (raw[1] == 0) {
            return 0;
        }
        index = 1;
    }
    while (raw[index] != 0) {
        if (raw[index] < '0' || raw[index] > '9') {
            return 0;
        }
        index++;
    }
    return 1;
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

static uint16_t rx_f32_to_f16(float value) {
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    uint32_t sign = (bits >> 16) & 0x8000u;
    int32_t exponent = (int32_t)((bits >> 23) & 0xFFu) - 127 + 15;
    uint32_t fraction = bits & 0x7FFFFFu;
    if (((bits >> 23) & 0xFFu) == 0xFFu) {
        if (fraction == 0) {
            return (uint16_t)(sign | 0x7C00u);
        }
        return (uint16_t)(sign | 0x7E00u);
    }
    if (exponent <= 0) {
        if (exponent < -10) {
            return (uint16_t)sign;
        }
        fraction = (fraction | 0x800000u) >> (uint32_t)(1 - exponent);
        if ((fraction & 0x1000u) != 0) {
            fraction += 0x2000u;
        }
        return (uint16_t)(sign | (fraction >> 13));
    }
    if (exponent >= 31) {
        return (uint16_t)(sign | 0x7C00u);
    }
    if ((fraction & 0x1000u) != 0) {
        fraction += 0x2000u;
        if ((fraction & 0x800000u) != 0) {
            fraction = 0;
            exponent++;
            if (exponent >= 31) {
                return (uint16_t)(sign | 0x7C00u);
            }
        }
    }
    return (uint16_t)(sign | ((uint32_t)exponent << 10) | (fraction >> 13));
}

static float rx_f16_to_f32(uint16_t value) {
    uint32_t sign = ((uint32_t)value & 0x8000u) << 16;
    uint32_t exponent = (value >> 10) & 0x1Fu;
    uint32_t fraction = value & 0x3FFu;
    uint32_t bits = 0;
    if (exponent == 0) {
        if (fraction == 0) {
            bits = sign;
        } else {
            exponent = 1;
            while ((fraction & 0x400u) == 0) {
                fraction <<= 1;
                exponent--;
            }
            fraction &= 0x3FFu;
            bits = sign | ((exponent + (127u - 15u)) << 23) | (fraction << 13);
        }
    } else if (exponent == 31) {
        bits = sign | 0x7F800000u | (fraction << 13);
    } else {
        bits = sign | ((exponent + (127u - 15u)) << 23) | (fraction << 13);
    }
    float result = 0;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

int64_t rx_U64Parse(int64_t text) {
    const char *raw = rx_cstr(text);
    char buffer[96];
    int64_t count = 0;
    while (*raw != 0 && count < 95) {
        if (*raw != '_') {
            buffer[count++] = *raw;
        }
        raw++;
    }
    buffer[count] = 0;
    return (int64_t)strtoull(buffer, NULL, 10);
}

int64_t rx_F16Parse(int64_t text) {
    float value = strtof(rx_cstr(text), NULL);
    return rx_f64_box((double)rx_f16_to_f32(rx_f32_to_f16(value)));
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
    } else if (kind == 5) {
        printf("%llu", (unsigned long long)value);
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

static int64_t rx_text_literal(const char *text) {
    return rx_box(rx_copy_n(text, (int64_t)strlen(text)));
}

static int64_t rx_any_field(int64_t box, int64_t index) {
    int64_t *fields = (int64_t *)(uintptr_t)box;
    if (fields == NULL) {
        return 0;
    }
    return fields[index];
}

static int64_t rx_tagged_text(int64_t tag, int64_t payload) {
    if (tag == 2) {
        return payload;
    }
    if (tag == 3) {
        return rx_str_from_bool(payload);
    }
    if (tag == 4) {
        return rx_str_from_char(payload);
    }
    if (tag == 5) {
        return rx_F64Text(payload);
    }
    if (tag == 6) {
        return rx_text_literal("Null");
    }
    return rx_str_from_int(payload);
}

static int64_t rx_join(int64_t text, int64_t extra) {
    int64_t next = rx_str_concat(text, extra);
    if (next != text) {
        rx_release(text);
    }
    return next;
}

int64_t rx_list_text(int64_t list, int64_t tag) {
    int64_t count = rx_list_count(list);
    int64_t text = rx_text_literal("[");
    int64_t comma = rx_text_literal(", ");
    int64_t index = 0;
    while (index < count) {
        if (index > 0) {
            text = rx_join(text, comma);
        }
        text = rx_join(text, rx_tagged_text(tag, rx_list_get(list, index)));
        index = index + 1;
    }
    return rx_join(text, rx_text_literal("]"));
}

int64_t rx_settle_list(int64_t list) {
    int64_t count = rx_list_count(list);
    int64_t settled = rx_list_new();
    int64_t prefix = rx_text_literal("Fulfilled:");
    int64_t index = 0;
    while (index < count) {
        rx_list_add(settled, rx_str_concat(prefix, rx_str_from_int(rx_list_get(list, index))));
        index = index + 1;
    }
    return settled;
}

int64_t rx_any_list_text(int64_t list) {
    int64_t count = rx_list_count(list);
    int64_t text = rx_text_literal("[");
    int64_t comma = rx_text_literal(", ");
    int64_t index = 0;
    while (index < count) {
        if (index > 0) {
            text = rx_join(text, comma);
        }
        text = rx_join(text, rx_AnyText(rx_list_get(list, index)));
        index = index + 1;
    }
    return rx_join(text, rx_text_literal("]"));
}

int64_t rx_any_map_text(int64_t map) {
    RxMap *value = (RxMap *)(uintptr_t)map;
    int64_t text = rx_text_literal("{");
    int64_t comma = rx_text_literal(", ");
    int64_t sep = rx_text_literal(": ");
    int64_t index = 0;
    if (value == NULL) {
        return rx_text_literal("{}");
    }
    while (index < value->count) {
        if (index > 0) {
            text = rx_join(text, comma);
        }
        text = rx_join(text, value->keys[index]);
        text = rx_join(text, sep);
        text = rx_join(text, rx_AnyText(value->values[index]));
        index = index + 1;
    }
    return rx_join(text, rx_text_literal("}"));
}

int64_t rx_AnyText(int64_t box) {
    if (box == 0) {
        return rx_text_literal("Null");
    }
    int64_t tag = rx_any_field(box, 0);
    int64_t payload = rx_any_field(box, 1);
    if (tag != 7) {
        return rx_tagged_text(tag, payload);
    }
    int64_t element = rx_any_field(box, 2);
    int64_t count = rx_list_count(payload);
    int64_t text = rx_text_literal("[");
    int64_t comma = rx_text_literal(", ");
    int64_t index = 0;
    while (index < count) {
        if (index > 0) {
            text = rx_join(text, comma);
        }
        text = rx_join(text, rx_tagged_text(element, rx_list_get(payload, index)));
        index = index + 1;
    }
    return rx_join(text, rx_text_literal("]"));
}

int64_t rx_AnyToInt(int64_t box) {
    if (box == 0) {
        return 0;
    }
    int64_t tag = rx_any_field(box, 0);
    int64_t payload = rx_any_field(box, 1);
    if (tag == 2) {
        return rx_str_to_int(payload);
    }
    if (tag == 5) {
        return rx_F64ToInt(payload);
    }
    if (tag == 1 || tag == 3 || tag == 4) {
        return payload;
    }
    return 0;
}

int64_t rx_AnyToFloat(int64_t box) {
    if (box == 0) {
        return rx_F64FromInt(0);
    }
    int64_t tag = rx_any_field(box, 0);
    int64_t payload = rx_any_field(box, 1);
    if (tag == 5) {
        return payload;
    }
    if (tag == 2) {
        return rx_F64Parse(payload);
    }
    if (tag == 1 || tag == 3 || tag == 4) {
        return rx_F64FromInt(payload);
    }
    return rx_F64FromInt(0);
}

int64_t rx_AnyLength(int64_t box) {
    if (box == 0) {
        return 0;
    }
    int64_t tag = rx_any_field(box, 0);
    int64_t payload = rx_any_field(box, 1);
    if (tag == 2) {
        return rx_str_len(payload);
    }
    if (tag == 7) {
        return rx_list_count(payload);
    }
    return 0;
}

int64_t rx_AnyUpper(int64_t box) {
    if (box == 0 || rx_any_field(box, 0) != 2) {
        return rx_text_literal("");
    }
    return rx_str_upper(rx_any_field(box, 1));
}

int64_t rx_AnyLower(int64_t box) {
    if (box == 0 || rx_any_field(box, 0) != 2) {
        return rx_text_literal("");
    }
    return rx_str_lower(rx_any_field(box, 1));
}

int64_t rx_ParseF64(int64_t text) { return rx_F64Parse(text); }
int64_t rx_TextF64(int64_t bits) { return rx_F64Text(bits); }
int64_t rx_FloatFromInt(int64_t value) { return rx_F64FromInt(value); }
int64_t rx_FloatToInt(int64_t bits) { return rx_F64ToInt(bits); }
int64_t rx_ReadLine(void) { return rx_Input(); }

int64_t rx_Input(void) {
    char *line = NULL;
    size_t capacity = 0;
    ssize_t count = getline(&line, &capacity, stdin);
    if (count < 0) {
        free(line);
        return rx_text_literal("");
    }
    while (count > 0 && (line[count - 1] == '\n' || line[count - 1] == '\r')) {
        count = count - 1;
        line[count] = 0;
    }
    return rx_box(line);
}

int64_t rx_str_trim(int64_t text) {
    const char *raw = rx_cstr(text);
    int64_t len = (int64_t)strlen(raw);
    int64_t start = 0;
    while (start < len && (raw[start] == ' ' || raw[start] == '\t' || raw[start] == '\n' || raw[start] == '\r')) {
        start = start + 1;
    }
    int64_t end = len;
    while (end > start && (raw[end - 1] == ' ' || raw[end - 1] == '\t' || raw[end - 1] == '\n' || raw[end - 1] == '\r')) {
        end = end - 1;
    }
    return rx_box(rx_copy_n(raw + start, end - start));
}

int64_t rx_str_split(int64_t text, int64_t sep) {
    int64_t list = rx_list_new();
    const char *raw = rx_cstr(text);
    const char *delim = rx_cstr(sep);
    int64_t raw_len = (int64_t)strlen(raw);
    int64_t delim_len = (int64_t)strlen(delim);
    if (delim_len == 0) {
        rx_list_add(list, rx_box(rx_copy_n(raw, raw_len)));
        return list;
    }
    int64_t start = 0;
    while (start <= raw_len) {
        const char *found = strstr(raw + start, delim);
        if (found == NULL) {
            rx_list_add(list, rx_box(rx_copy_n(raw + start, raw_len - start)));
            break;
        }
        int64_t pos = (int64_t)(found - raw);
        rx_list_add(list, rx_box(rx_copy_n(raw + start, pos - start)));
        start = pos + delim_len;
    }
    return list;
}

void rx_list_sort(int64_t list) {
    RxList *ptr = (RxList *)(uintptr_t)list;
    if (ptr == NULL || ptr->count < 2) {
        return;
    }
    for (int64_t i = 0; i < ptr->count - 1; i++) {
        for (int64_t j = i + 1; j < ptr->count; j++) {
            if (ptr->items[j] < ptr->items[i]) {
                int64_t tmp = ptr->items[i];
                ptr->items[i] = ptr->items[j];
                ptr->items[j] = tmp;
            }
        }
    }
}

void rx_list_reverse(int64_t list) {
    RxList *ptr = (RxList *)(uintptr_t)list;
    if (ptr == NULL || ptr->count < 2) {
        return;
    }
    int64_t left = 0;
    int64_t right = ptr->count - 1;
    while (left < right) {
        int64_t tmp = ptr->items[left];
        ptr->items[left] = ptr->items[right];
        ptr->items[right] = tmp;
        left = left + 1;
        right = right - 1;
    }
}

int64_t rx_math_gcd(int64_t a, int64_t b) {
    if (a < 0) { a = -a; }
    if (b < 0) { b = -b; }
    while (b != 0) {
        int64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int64_t rx_math_isqrt(int64_t n) {
    if (n <= 0) {
        return 0;
    }
    int64_t r = 0;
    while ((r + 1) * (r + 1) <= n) {
        r = r + 1;
    }
    return r;
}

int64_t rx_file_name(int64_t path) {
    const char *raw = rx_cstr(path);
    const char *slash = strrchr(raw, '/');
    if (slash == NULL) {
        return rx_box(rx_copy_n(raw, (int64_t)strlen(raw)));
    }
    return rx_box(rx_copy_n(slash + 1, (int64_t)strlen(slash + 1)));
}

int64_t rx_file_join(int64_t dir, int64_t name) {
    const char *d = rx_cstr(dir);
    const char *n = rx_cstr(name);
    int64_t dlen = (int64_t)strlen(d);
    int64_t nlen = (int64_t)strlen(n);
    if (dlen == 0) {
        return rx_box(rx_copy_n(n, nlen));
    }
    if (d[dlen - 1] == '/') {
        return rx_str_concat(dir, name);
    }
    int64_t slash = rx_box(rx_copy_n("/", 1));
    return rx_str_concat(rx_str_concat(dir, slash), name);
}
