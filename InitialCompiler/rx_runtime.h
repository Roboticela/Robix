/* Runtime shared by the C backup compiler and the self-hosted LLVM compiler.
   Integers, bools, chars, and object fields are raw 64-bit values.
   Strings are null-terminated bytes. Lists and objects are plain structs,
   so a field load is the same machine code C would emit. */
#ifndef RX_RUNTIME_H
#define RX_RUNTIME_H

#include <stdint.h>

typedef struct RxList {
    int64_t *items;
    int64_t count;
    int64_t cap;
} RxList;

int64_t rx_map_new(int64_t kind);
void rx_map_put(int64_t map, int64_t key, int64_t value);
int64_t rx_map_get(int64_t map, int64_t key);
void rx_set_add(int64_t list, int64_t item, int64_t kind);
int64_t rx_set_has(int64_t list, int64_t item, int64_t kind);
int64_t rx_list_new(void);
int64_t rx_list_count(int64_t list);
int64_t rx_list_get(int64_t list, int64_t index);
void rx_list_add(int64_t list, int64_t value);
void rx_list_set(int64_t list, int64_t index, int64_t value);

int64_t rx_obj_new(int64_t fields);
int64_t rx_obj_get(int64_t object, int64_t index);
void rx_obj_set(int64_t object, int64_t index, int64_t value);

int64_t rx_str_len(int64_t text);
int64_t rx_str_eq(int64_t left, int64_t right);
int64_t rx_str_concat(int64_t left, int64_t right);
int64_t rx_str_substr(int64_t text, int64_t start, int64_t length);
int64_t rx_str_get(int64_t text, int64_t index);
int64_t rx_str_lower(int64_t text);
int64_t rx_str_upper(int64_t text);
int64_t rx_str_starts(int64_t text, int64_t prefix);
int64_t rx_str_ends(int64_t text, int64_t suffix);
int64_t rx_str_contains(int64_t text, int64_t needle);
int64_t rx_str_index(int64_t text, int64_t needle, int64_t start);
int64_t rx_str_rindex(int64_t text, int64_t needle, int64_t start);
int64_t rx_str_replace(int64_t text, int64_t old_text, int64_t new_text);
int64_t rx_str_to_int(int64_t text);
double rx_str_to_double(int64_t text);
int64_t rx_F64Parse(int64_t text);
int64_t rx_F32Parse(int64_t text);
int64_t rx_F64Text(int64_t bits);
int64_t rx_F64FromInt(int64_t value);
int64_t rx_F64Add(int64_t left, int64_t right);
int64_t rx_F64Sub(int64_t left, int64_t right);
int64_t rx_F64Mul(int64_t left, int64_t right);
int64_t rx_F64Div(int64_t left, int64_t right);
int64_t rx_F64Neg(int64_t bits);
int64_t rx_F64Cmp(int64_t left, int64_t right);
int64_t rx_F64ToInt(int64_t bits);
int64_t rx_AnyText(int64_t box);
int64_t rx_AnyToInt(int64_t box);
int64_t rx_AnyToFloat(int64_t box);
int64_t rx_AnyLength(int64_t box);
int64_t rx_AnyUpper(int64_t box);
int64_t rx_AnyLower(int64_t box);
int64_t rx_Input(void);
int64_t rx_str_from_int(int64_t value);
int64_t rx_str_from_bool(int64_t value);
int64_t rx_str_from_char(int64_t value);
int64_t rx_str_from_chars(int64_t list);

void rx_print(int64_t value, int64_t kind);
int64_t rx_file_read(int64_t path);
void rx_file_write_text(int64_t path, int64_t text);
void rx_file_write_bytes(int64_t path, int64_t bytes);
int64_t rx_file_exists(int64_t path);
void rx_file_delete(int64_t path);
void rx_dir_create(int64_t path);
void rx_dir_delete(int64_t path);
int64_t rx_dir_exists(int64_t path);
int64_t rx_dir_files(int64_t path);
int64_t rx_env_get(int64_t name);
int64_t rx_process(int64_t command);
int64_t rx_args(int32_t argc, char **argv);
void rx_finish_result(int64_t result);

#endif
