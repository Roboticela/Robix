extern void rx_print(const char* text, int length);
extern void rx_warn(const char* text, int length);
extern int rx_input(const char* prompt, int prompt_length, char* out, int cap);

void* memcpy(void* dest, const void* src, unsigned long n) {
    unsigned char* out = (unsigned char*)dest;
    const unsigned char* in = (const unsigned char*)src;
    for (unsigned long i = 0; i < n; ++i) out[i] = in[i];
    return dest;
}

void* memset(void* dest, int value, unsigned long n) {
    unsigned char* out = (unsigned char*)dest;
    for (unsigned long i = 0; i < n; ++i) out[i] = (unsigned char)value;
    return dest;
}

void* memmove(void* dest, const void* src, unsigned long n) {
    unsigned char* out = (unsigned char*)dest;
    const unsigned char* in = (const unsigned char*)src;
    if (out < in) {
        for (unsigned long i = 0; i < n; ++i) out[i] = in[i];
    } else {
        for (unsigned long i = n; i > 0; --i) out[i - 1] = in[i - 1];
    }
    return dest;
}

enum { MAX_REG = 1024, MAX_FRAME = 16, MAX_OBJ = 256, MAX_FIELD = 32, HEAP_SIZE = 65536, MAX_LOCAL = 64 };

typedef struct {
    int tag;
    long long i;
    double f;
    const char* s;
    int slen;
    int obj;
} Val;

typedef struct {
    const char* name;
    Val val;
} Field;

typedef struct {
    int used;
    int freed;
    const char* cls;
    const char* base;
    int nfields;
    Field fields[MAX_FIELD];
    unsigned char* buffer;
    int buflen;
    int buffer_freed;
    int index;
    int width;
    int signed_value;
    int cell;
} Obj;

typedef struct {
    int used;
    char name[64];
    Val val;
} Cell;

typedef struct {
    char name[64];
    Val val;
} Local;

typedef struct {
    Val regs[MAX_REG];
    Local locals[MAX_LOCAL];
    int nlocals;
} Frame;

static char heap[HEAP_SIZE];
static int heap_used;
static Obj objects[MAX_OBJ];
static int object_count;
static Cell cells[MAX_LOCAL];
static int cell_count;
static Frame frames[MAX_FRAME];
static int depth;
static int fault;
static char call_name[128];
static char input_buf[512];
static char file_path[8][96];
static char file_body[8][192];
static int file_count;
static char env_key[8][64];
static char env_val[8][96];
static int env_count;

static int find_chunk(const char* name);

static int slen(const char* text) {
    int n = 0;
    if (!text) return 0;
    while (text[n]) ++n;
    return n;
}

static int streq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *a == *b) { ++a; ++b; }
    return *a == 0 && *b == 0;
}

static int ends_method(const char* name, const char* method) {
    int nlen = slen(name);
    int mlen = slen(method);
    if (nlen <= mlen || name[nlen - mlen - 1] != '.') return 0;
    for (int i = 0; i < mlen; ++i) if (name[nlen - mlen + i] != method[i]) return 0;
    return 1;
}

static void copy_name(char* dest, const char* src) {
    int i = 0;
    if (!src) src = "";
    while (src[i] && i < 63) { dest[i] = src[i]; ++i; }
    dest[i] = 0;
}

static Val vvoid(void) { Val v; v.tag = 0; v.i = 0; v.f = 0; v.s = ""; v.slen = 0; v.obj = 0; return v; }
static Val vint(long long i) { Val v = vvoid(); v.tag = 1; v.i = i; return v; }
static Val vfloat(double f) { Val v = vvoid(); v.tag = 2; v.f = f; return v; }
static Val vbool(int b) { Val v = vvoid(); v.tag = 3; v.i = b ? 1 : 0; return v; }
static Val vstr(const char* s, int n) { Val v = vvoid(); v.tag = 4; v.s = s ? s : ""; v.slen = n; return v; }

static long long as_int(Val v) {
    if (v.tag == 1 || v.tag == 3) return v.i;
    if (v.tag == 2) return (long long)v.f;
    return 0;
}
static double as_float(Val v) {
    if (v.tag == 2) return v.f;
    if (v.tag == 1) return (double)v.i;
    return 0;
}
static int as_bool(Val v) {
    if (v.tag == 3) return v.i != 0;
    if (v.tag == 1) return v.i != 0;
    return 0;
}

static void text_of(Val v, char* out, int cap) {
    if (cap <= 0) return;
    if (v.tag == 1) {
        long long n = v.i;
        char tmp[32];
        int i = 0;
        int neg = n < 0;
        unsigned long long u = neg ? (unsigned long long)(-n) : (unsigned long long)n;
        if (u == 0) tmp[i++] = '0';
        while (u && i < 31) { tmp[i++] = (char)('0' + (u % 10)); u /= 10; }
        int p = 0;
        if (neg && p + 1 < cap) out[p++] = '-';
        while (i > 0 && p + 1 < cap) out[p++] = tmp[--i];
        out[p] = 0;
        return;
    }
    if (v.tag == 3) { const char* s = v.i ? "true" : "false"; int i = 0; while (s[i] && i + 1 < cap) { out[i] = s[i]; ++i; } out[i] = 0; return; }
    if (v.tag == 4) { int i = 0; while (i < v.slen && i + 1 < cap && v.s[i]) { out[i] = v.s[i]; ++i; } out[i] = 0; return; }
    if (v.tag == 5 && v.obj > 0 && v.obj <= object_count) {
        const char* cls = objects[v.obj - 1].cls ? objects[v.obj - 1].cls : "";
        int p = 0;
        if (p + 1 < cap) out[p++] = '<';
        for (int i = 0; cls[i] && p + 1 < cap; ++i) out[p++] = cls[i];
        if (p + 1 < cap) out[p++] = '>';
        out[p] = 0;
        return;
    }
    const char* s = v.tag == 2 ? "0" : "void";
    if (v.tag == 2) {
        long long whole = (long long)v.f;
        text_of(vint(whole), out, cap);
        return;
    }
    int i = 0;
    while (s[i] && i + 1 < cap) { out[i] = s[i]; ++i; }
    out[i] = 0;
}

static const char* intern(const char* text, int n) {
    if (heap_used + n + 1 >= HEAP_SIZE) { fault = 1; return ""; }
    char* dest = heap + heap_used;
    for (int i = 0; i < n; ++i) dest[i] = text ? text[i] : 0;
    dest[n] = 0;
    heap_used += n + 1;
    return dest;
}

static void fail(const char* message) {
    fault = 1;
    rx_print(message, slen(message));
}

static Obj* obj_at(Val v) {
    if (v.tag != 5 || v.obj <= 0 || v.obj > object_count) return 0;
    return &objects[v.obj - 1];
}

static Val* field_slot(Obj* obj, const char* name, int create) {
    for (int i = 0; i < obj->nfields; ++i) if (streq(obj->fields[i].name, name)) return &obj->fields[i].val;
    if (!create || obj->nfields >= MAX_FIELD) return 0;
    Field* field = &obj->fields[obj->nfields++];
    field->name = name;
    field->val = vvoid();
    return &field->val;
}

static long long read_pointee(Obj* obj) {
    if (obj->cell > 0 && obj->cell <= cell_count) return as_int(cells[obj->cell - 1].val);
    if (!obj->buffer || obj->buffer_freed) { fail("robix vm: use after free"); return 0; }
    int width = obj->width <= 0 ? 1 : obj->width;
    if (obj->index < 0 || obj->index + width > obj->buflen) { fail("robix vm: pointer out of bounds"); return 0; }
    unsigned long long bits = 0;
    for (int i = 0; i < width; ++i) bits |= (unsigned long long)obj->buffer[obj->index + i] << (8 * i);
    if (obj->signed_value && width < 8) {
        unsigned long long sign = 1ull << (width * 8 - 1);
        if (bits & sign) bits |= ~((1ull << (width * 8)) - 1);
    }
    return (long long)bits;
}

static void write_pointee(Obj* obj, long long value) {
    if (obj->cell > 0 && obj->cell <= cell_count) { cells[obj->cell - 1].val = vint(value); return; }
    if (!obj->buffer || obj->buffer_freed) { fail("robix vm: use after free"); return; }
    int width = obj->width <= 0 ? 1 : obj->width;
    if (obj->index < 0 || obj->index + width > obj->buflen) { fail("robix vm: pointer out of bounds"); return; }
    unsigned long long bits = (unsigned long long)value;
    for (int i = 0; i < width; ++i) obj->buffer[obj->index + i] = (unsigned char)((bits >> (8 * i)) & 0xFFu);
}

static int alloc_obj(const char* symbol, long long length) {
    if (object_count >= MAX_OBJ) { fail("robix vm: too many objects"); return 0; }
    Obj* obj = &objects[object_count++];
    obj->used = 1;
    obj->freed = 0;
    obj->nfields = 0;
    obj->buffer = 0;
    obj->buflen = 0;
    obj->buffer_freed = 0;
    obj->index = 0;
    obj->width = 8;
    obj->signed_value = 0;
    obj->cell = 0;
    const char* at = 0;
    for (const char* p = symbol; p && *p; ++p) if (*p == '@') at = p;
    if (!at) { obj->cls = symbol ? symbol : ""; obj->base = ""; }
    else {
        obj->cls = intern(symbol, (int)(at - symbol));
        obj->base = at + 1;
    }
    if (streq(obj->cls, "Array")) {
        Val* slot = field_slot(obj, "len", 1);
        if (slot) *slot = vint(length);
    }
    return object_count;
}

static Val* local_slot(Frame* frame, const char* name, int create) {
    for (int i = 0; i < frame->nlocals; ++i) if (streq(frame->locals[i].name, name)) return &frame->locals[i].val;
    if (!create || frame->nlocals >= MAX_LOCAL) return 0;
    Local* local = &frame->locals[frame->nlocals++];
    copy_name(local->name, name);
    local->val = vvoid();
    return &local->val;
}

static int reg_ok(int id) { return id >= 0 && id < MAX_REG; }

static Val run_chunk(int index, const Val* args, int nargs);

static void intrinsic(const Ins* inst, Frame* frame) {
    const char* name = STR[inst->str];
    Val out = vint(0);
    int has = 0;
    if (streq(name, "Memory.Allocate") && inst->argc > 0) {
        long long bytes = as_int(frame->regs[inst->args[0]]);
        if (bytes < 0) { fail("robix vm: negative allocation"); return; }
        int id = alloc_obj("Mem", 0);
        if (id) {
            Obj* obj = &objects[id - 1];
            obj->width = 1;
            obj->buflen = (int)bytes;
            obj->buffer = (unsigned char*)intern("", 1);
            if (heap_used + bytes < HEAP_SIZE) {
                obj->buffer = (unsigned char*)(heap + heap_used);
                for (int i = 0; i < bytes; ++i) heap[heap_used++] = 0;
                heap[heap_used++] = 0;
            }
            out = vvoid(); out.tag = 5; out.obj = id; has = 1;
        }
    } else if (streq(name, "Memory.Free") && inst->argc > 0) {
        Obj* obj = obj_at(frame->regs[inst->args[0]]);
        if (!obj) { fail("robix vm: Memory.Free requires an allocated pointer"); return; }
        if (obj->buffer) {
            if (obj->buffer_freed) { fail("robix vm: double free"); return; }
            obj->buffer_freed = 1;
            obj->buffer = 0;
        } else if (obj->freed) { fail("robix vm: double free"); return; }
        else { obj->freed = 1; obj->nfields = 0; }
    } else if (streq(name, "File.WriteText") && inst->argc >= 2) {
        char path[96]; char body[192];
        text_of(frame->regs[inst->args[0]], path, 96);
        text_of(frame->regs[inst->args[1]], body, 192);
        int slot = -1;
        for (int i = 0; i < file_count; ++i) if (streq(file_path[i], path)) slot = i;
        if (slot < 0 && file_count < 8) slot = file_count++;
        if (slot >= 0) { copy_name(file_path[slot], path); copy_name(file_body[slot], body); }
        out = vbool(slot >= 0); has = 1;
    } else if (streq(name, "File.ReadText") && inst->argc > 0) {
        char path[96];
        text_of(frame->regs[inst->args[0]], path, 96);
        const char* body = "";
        for (int i = 0; i < file_count; ++i) if (streq(file_path[i], path)) body = file_body[i];
        out = vstr(body, slen(body)); has = 1;
    } else if (streq(name, "Env.Get") && inst->argc > 0) {
        char key[64];
        text_of(frame->regs[inst->args[0]], key, 64);
        const char* body = "";
        for (int i = 0; i < env_count; ++i) if (streq(env_key[i], key)) body = env_val[i];
        out = vstr(body, slen(body)); has = 1;
    } else if (streq(name, "Env.Exists") && inst->argc > 0) {
        char key[64];
        text_of(frame->regs[inst->args[0]], key, 64);
        int found = 0;
        for (int i = 0; i < env_count; ++i) if (streq(env_key[i], key)) found = 1;
        out = vbool(found); has = 1;
    } else if ((streq(name, "Env.Set") || streq(name, "Env.Add")) && inst->argc >= 2) {
        char key[64]; char body[96];
        text_of(frame->regs[inst->args[0]], key, 64);
        text_of(frame->regs[inst->args[1]], body, 96);
        int slot = -1;
        for (int i = 0; i < env_count; ++i) if (streq(env_key[i], key)) slot = i;
        if (slot >= 0 && streq(name, "Env.Add")) { out = vint(0); has = 1; }
        else {
            if (slot < 0 && env_count < 8) slot = env_count++;
            if (slot >= 0) { copy_name(env_key[slot], key); copy_name(env_val[slot], body); }
            out = vint(slot >= 0 ? 1 : 0); has = 1;
        }
    } else if (streq(name, "String.Length") && inst->argc > 0) {
        out = vint(frame->regs[inst->args[0]].tag == 4 ? frame->regs[inst->args[0]].slen : 0); has = 1;
    } else if (streq(name, "Time.NowMicroseconds")) { out = vint(1000000); has = 1; }
    else if (streq(name, "Args.Count")) { out = vint(0); has = 1; }
    else if (streq(name, "Math.Abs") && inst->argc > 0) {
        long long v = as_int(frame->regs[inst->args[0]]);
        out = vint(v < 0 ? -v : v); has = 1;
    } else if ((streq(name, "Math.Min") || streq(name, "Math.Max")) && inst->argc >= 2) {
        long long a = as_int(frame->regs[inst->args[0]]);
        long long b = as_int(frame->regs[inst->args[1]]);
        out = vint(streq(name, "Math.Min") ? (a < b ? a : b) : (a > b ? a : b)); has = 1;
    } else if (streq(name, "Math.Clamp") && inst->argc >= 3) {
        long long v = as_int(frame->regs[inst->args[0]]);
        long long lo = as_int(frame->regs[inst->args[1]]);
        long long hi = as_int(frame->regs[inst->args[2]]);
        if (v < lo) v = lo;
        if (v > hi) v = hi;
        out = vint(v); has = 1;
    } else if (streq(name, "Input") || streq(name, "Input.Int")) {
        char prompt[128];
        prompt[0] = 0;
        if (inst->argc > 0) text_of(frame->regs[inst->args[0]], prompt, 128);
        int n = rx_input(prompt, slen(prompt), input_buf, 511);
        if (n < 0) n = 0;
        if (n > 511) n = 511;
        input_buf[n] = 0;
        if (streq(name, "Input.Int")) {
            long long value = 0; int sign = 1; int i = 0;
            if (input_buf[0] == '-') { sign = -1; i = 1; }
            for (; input_buf[i] >= '0' && input_buf[i] <= '9'; ++i) value = value * 10 + (input_buf[i] - '0');
            out = vint(sign * value);
        } else out = vstr(intern(input_buf, n), n);
        has = 1;
    } else has = inst->dest != 0;
    if (has && inst->dest != 0 && reg_ok(inst->dest)) frame->regs[inst->dest] = out;
}

static Val run_chunk(int index, const Val* args, int nargs) {
    if (fault || index < 0 || index >= CHUNK_COUNT) return vvoid();
    if (depth >= MAX_FRAME) { fail("robix vm: call depth exceeded"); return vvoid(); }
    Frame* frame = &frames[depth++];
    frame->nlocals = 0;
    const Chunk* chunk = &CHUNKS[index];
    int param = 0;
    const char* chunk_name = STR[chunk->name];
    if (nargs > 0 && args[0].tag == 5) {
        int dotted = 0;
        for (const char* p = chunk_name; *p; ++p) if (*p == '.') dotted = 1;
        if (dotted) { Val* slot = local_slot(frame, "this", 1); if (slot) *slot = args[0]; param = 1; }
    }
    for (int i = 0; i < chunk->nparam && i + param < nargs; ++i) {
        Val* slot = local_slot(frame, STR[chunk->params[i]], 1);
        if (slot) *slot = args[i + param];
    }
    int pc = 0;
    Val result = vvoid();
    while (!fault && pc < chunk->count) {
        const Ins* inst = &INS[chunk->begin + pc];
        ++pc;
        int op = inst->op;
        if (op == 0 || op == 40 || op == 41) continue;
        if (inst->dest && !reg_ok(inst->dest)) { fail("robix vm: register is outside the WebAssembly form"); break; }
        if (op == 1 && reg_ok(inst->dest)) frame->regs[inst->dest] = vint(inst->imm);
        else if (op == 2 && reg_ok(inst->dest)) frame->regs[inst->dest] = vfloat(inst->fimm);
        else if (op == 3 && reg_ok(inst->dest)) frame->regs[inst->dest] = vbool(inst->bo);
        else if (op == 4 && reg_ok(inst->dest)) frame->regs[inst->dest] = vstr(STR[inst->str], SLEN[inst->str]);
        else if (op == 5 && reg_ok(inst->dest) && reg_ok(inst->a)) {
            Val src = frame->regs[inst->a];
            if (streq(STR[inst->str], "ToFloat")) frame->regs[inst->dest] = vfloat(as_float(src));
            else if (streq(STR[inst->str], "ToInt")) frame->regs[inst->dest] = vint(as_int(src));
            else if (streq(STR[inst->str], "ToString")) {
                char buf[128]; text_of(src, buf, 128);
                frame->regs[inst->dest] = vstr(intern(buf, slen(buf)), slen(buf));
            } else frame->regs[inst->dest] = src;
        } else if (op >= 6 && op <= 23 && reg_ok(inst->dest) && reg_ok(inst->a) && reg_ok(inst->b)) {
            Val l = frame->regs[inst->a];
            Val r = frame->regs[inst->b];
            if (op == 6 && (l.tag == 4 || r.tag == 4)) {
                char buf[256];
                text_of(l, buf, 128);
                int n = slen(buf);
                char right[128];
                text_of(r, right, 128);
                int m = slen(right);
                for (int i = 0; i < m && n < 255; ++i) buf[n++] = right[i];
                buf[n] = 0;
                frame->regs[inst->dest] = vstr(intern(buf, n), n);
            } else if (l.tag == 2 || r.tag == 2) {
                double lf = as_float(l), rf = as_float(r);
                if (op == 6) frame->regs[inst->dest] = vfloat(lf + rf);
                else if (op == 7) frame->regs[inst->dest] = vfloat(lf - rf);
                else if (op == 8) frame->regs[inst->dest] = vfloat(lf * rf);
                else if (op == 9) frame->regs[inst->dest] = vfloat(rf != 0 ? lf / rf : 0);
                else if (op == 11) frame->regs[inst->dest] = vbool(lf == rf);
                else if (op == 12) frame->regs[inst->dest] = vbool(lf != rf);
                else if (op == 13) frame->regs[inst->dest] = vbool(lf < rf);
                else if (op == 14) frame->regs[inst->dest] = vbool(lf <= rf);
                else if (op == 15) frame->regs[inst->dest] = vbool(lf > rf);
                else if (op == 16) frame->regs[inst->dest] = vbool(lf >= rf);
            } else if (op == 6) frame->regs[inst->dest] = vint(as_int(l) + as_int(r));
            else if (op == 7) frame->regs[inst->dest] = vint(as_int(l) - as_int(r));
            else if (op == 8) frame->regs[inst->dest] = vint(as_int(l) * as_int(r));
            else if (op == 9) { long long d = as_int(r); frame->regs[inst->dest] = vint(d ? as_int(l) / d : 0); }
            else if (op == 10) { long long d = as_int(r); frame->regs[inst->dest] = vint(d ? as_int(l) % d : 0); }
            else if (op == 11) frame->regs[inst->dest] = vbool(l.tag == 4 && r.tag == 4 ? (l.slen == r.slen && streq(l.s, r.s)) : as_int(l) == as_int(r));
            else if (op == 12) frame->regs[inst->dest] = vbool(l.tag == 4 && r.tag == 4 ? !(l.slen == r.slen && streq(l.s, r.s)) : as_int(l) != as_int(r));
            else if (op == 13) frame->regs[inst->dest] = vbool(as_int(l) < as_int(r));
            else if (op == 14) frame->regs[inst->dest] = vbool(as_int(l) <= as_int(r));
            else if (op == 15) frame->regs[inst->dest] = vbool(as_int(l) > as_int(r));
            else if (op == 16) frame->regs[inst->dest] = vbool(as_int(l) >= as_int(r));
            else if (op == 17) frame->regs[inst->dest] = vbool(as_bool(l) && as_bool(r));
            else if (op == 18) frame->regs[inst->dest] = vbool(as_bool(l) || as_bool(r));
            else if (op == 19) frame->regs[inst->dest] = vint(as_int(l) & as_int(r));
            else if (op == 20) frame->regs[inst->dest] = vint(as_int(l) | as_int(r));
            else if (op == 21) frame->regs[inst->dest] = vint(as_int(l) ^ as_int(r));
            else if (op == 22) frame->regs[inst->dest] = vint((long long)((unsigned long long)as_int(l) << (unsigned)(as_int(r) & 63)));
            else if (op == 23) frame->regs[inst->dest] = vint(as_int(l) >> (unsigned)(as_int(r) & 63));
        } else if (op == 24 && reg_ok(inst->dest) && reg_ok(inst->a)) {
            Val v = frame->regs[inst->a];
            frame->regs[inst->dest] = v.tag == 2 ? vfloat(-as_float(v)) : vint(-as_int(v));
        } else if (op == 25 && reg_ok(inst->dest) && reg_ok(inst->a)) frame->regs[inst->dest] = vbool(!as_bool(frame->regs[inst->a]));
        else if (op == 26 && reg_ok(inst->dest) && reg_ok(inst->a)) frame->regs[inst->dest] = vint(~as_int(frame->regs[inst->a]));
        else if (op == 27 && reg_ok(inst->dest)) {
            int id = alloc_obj(STR[inst->str], inst->imm);
            Val v = vvoid(); v.tag = 5; v.obj = id;
            frame->regs[inst->dest] = v;
        } else if (op == 28 && reg_ok(inst->dest)) {
            Val* slot = local_slot(frame, STR[inst->str], 0);
            frame->regs[inst->dest] = slot ? *slot : vvoid();
        } else if (op == 29 && reg_ok(inst->a)) {
            Val* slot = local_slot(frame, STR[inst->str], 1);
            if (slot) *slot = frame->regs[inst->a];
            for (int i = 0; i < cell_count; ++i) if (streq(cells[i].name, STR[inst->str])) cells[i].val = frame->regs[inst->a];
        } else if (op == 30 && reg_ok(inst->dest) && reg_ok(inst->a)) {
            Obj* obj = obj_at(frame->regs[inst->a]);
            Val* slot = obj ? field_slot(obj, STR[inst->str], 0) : 0;
            frame->regs[inst->dest] = slot ? *slot : vvoid();
        } else if (op == 31 && reg_ok(inst->a) && reg_ok(inst->b)) {
            Obj* obj = obj_at(frame->regs[inst->a]);
            Val* slot = obj ? field_slot(obj, STR[inst->str], 1) : 0;
            if (slot) *slot = frame->regs[inst->b];
        } else if (op == 32 || op == 49) {
            Val call_args[8];
            int n = inst->argc > 8 ? 8 : inst->argc;
            for (int i = 0; i < n; ++i) call_args[i] = reg_ok(inst->args[i]) ? frame->regs[inst->args[i]] : vvoid();
            int target = -1;
            if (op == 49) {
                Obj* self = n ? obj_at(call_args[0]) : 0;
                if (!self) fail("robix vm: virtual call needs an object");
                else {
                    const char* cursor = self->cls;
                    const char* rest = self->base;
                    for (int guard = 0; guard < 16 && cursor && cursor[0]; ++guard) {
                        char joined[128];
                        int p = 0;
                        for (int i = 0; cursor[i] && p < 100; ++i) joined[p++] = cursor[i];
                        joined[p++] = '.';
                        const char* method = STR[inst->str];
                        for (int i = 0; method[i] && p < 126; ++i) joined[p++] = method[i];
                        joined[p] = 0;
                        target = find_chunk(joined);
                        if (target >= 0) break;
                        const char* sep = 0;
                        for (const char* q = rest; q && *q; ++q) if (*q == '@') sep = q;
                        if (!rest || !rest[0]) break;
                        if (!sep) { cursor = rest; rest = ""; }
                        else { cursor = intern(rest, (int)(sep - rest)); rest = sep + 1; }
                    }
                    if (target < 0) fail("robix vm: no implementation of method");
                }
            } else target = find_chunk(STR[inst->str]);
            Val ret = target >= 0 ? run_chunk(target, call_args, n) : vvoid();
            if (inst->dest && reg_ok(inst->dest)) frame->regs[inst->dest] = ret;
        } else if (op == 33) intrinsic(inst, frame);
        else if (op == 34 && reg_ok(inst->a)) {
            char buf[256];
            text_of(frame->regs[inst->a], buf, 256);
            rx_print(buf, slen(buf));
        } else if (op == 36) pc = inst->target;
        else if (op == 37 && reg_ok(inst->a)) { if (!as_bool(frame->regs[inst->a])) pc = inst->target; }
        else if (op == 38 && reg_ok(inst->a)) { result = frame->regs[inst->a]; break; }
        else if (op == 39) { result = vvoid(); break; }
        else if (op == 42 && reg_ok(inst->dest) && reg_ok(inst->a) && reg_ok(inst->b)) {
            Obj* obj = obj_at(frame->regs[inst->a]);
            if (!obj) fail("robix vm: index of a non-array");
            else {
                char key[32];
                text_of(vint(as_int(frame->regs[inst->b])), key, 32);
                Val* len = field_slot(obj, "len", 0);
                if (!len || as_int(frame->regs[inst->b]) < 0 || as_int(frame->regs[inst->b]) >= as_int(*len)) fail("robix vm: index out of bounds");
                else { Val* slot = field_slot(obj, intern(key, slen(key)), 0); frame->regs[inst->dest] = slot ? *slot : vvoid(); }
            }
        } else if (op == 43 && inst->argc >= 3) {
            Obj* obj = obj_at(frame->regs[inst->args[0]]);
            if (!obj) fail("robix vm: index of a non-array");
            else {
                char key[32];
                text_of(vint(as_int(frame->regs[inst->args[1]])), key, 32);
                Val* slot = field_slot(obj, intern(key, slen(key)), 1);
                if (slot) *slot = frame->regs[inst->args[2]];
            }
        }
    }
    --depth;
    return result;
}

static int find_chunk(const char* name) {
    for (int i = 0; i < CHUNK_COUNT; ++i) if (streq(STR[CHUNKS[i].name], name)) return i;
    return -1;
}

static int find_handler(const char* name) {
    int exact = find_chunk(name);
    if (exact >= 0) return exact;
    for (int i = 0; i < CHUNK_COUNT; ++i) if (ends_method(STR[CHUNKS[i].name], name)) return i;
    return -1;
}

__attribute__((export_name("rx_name")))
char* rx_name(void) { return call_name; }

__attribute__((export_name("rx_call")))
void rx_call(int len) {
    if (len < 0) len = 0;
    if (len > 127) len = 127;
    call_name[len] = 0;
    int index = find_handler(call_name);
    if (index < 0) return;
    Val args[1];
    int n = 0;
    const char* chunk_name = STR[CHUNKS[index].name];
    for (const char* p = chunk_name; *p; ++p) {
        if (*p == '.') {
            int id = alloc_obj(intern(chunk_name, (int)(p - chunk_name)), 0);
            args[0] = vvoid(); args[0].tag = 5; args[0].obj = id; n = 1;
            break;
        }
    }
    run_chunk(index, args, n);
}

__attribute__((export_name("start")))
void start(void) {
    for (int i = 0; i < WARN_COUNT; ++i) rx_warn(STR[WARN[i]], SLEN[WARN[i]]);
    int main_index = find_chunk("Main");
    if (main_index >= 0) run_chunk(main_index, 0, 0);
}
