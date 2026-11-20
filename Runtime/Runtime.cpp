#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cerrno>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <malloc.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

class Ownership;
class Text;
class List;
class Map;
class Object;
class Files;
class Process;
class Math;
class Float64;
class Float32;
class Float16;
class Unsigned64;
class Any;

class Ownership {
public:
    static constexpr int KIND_TEXT = 1;
    static constexpr int KIND_LIST = 2;
    static constexpr int KIND_OBJECT = 3;
    static constexpr int KIND_MAP = 4;
    static constexpr int EMPTY = 0;
    static constexpr int LIVE = 1;
    static constexpr int TOMB = 2;
    struct Entry {
        std::int64_t pointer;
        std::int32_t count;
        std::int32_t kind;
        std::int32_t aux;
        std::int32_t state;
    };
    static Entry* entries;
    static int entryCapacity;
    static int liveCount;
    static std::int64_t* temporaries;
    static int temporaryCount;
    static int temporaryCapacity;
    static int* marks;
    static int markCount;
    static int markCapacity;

    static std::uint64_t Hash(std::int64_t pointer);
    static void Grow();
    static Entry* Find(std::int64_t pointer, int create);
    static void Destroy(std::int64_t pointer, int kind, int aux);
    static void Track(std::int64_t pointer, int kind, std::int64_t aux);
    static void PushTemporary(std::int64_t pointer);
    static std::int64_t Export(std::int64_t pointer, int kind, std::int64_t aux);
    static void Release(std::int64_t pointer) asm("Ownership_Release");
    static void Retain(std::int64_t pointer) asm("Ownership_Retain");
    static void PushScope() asm("Ownership_PushScope");
    static void PopScope() asm("Ownership_PopScope");
    static void Give(std::int64_t pointer) asm("Ownership_Give");
    static std::int64_t MarkDepth() asm("Ownership_MarkDepth");
    static void Unwind(std::int64_t depth) asm("Ownership_Unwind");
    static std::int64_t Keep(std::int64_t pointer) asm("Ownership_Keep");
    static std::int64_t Allocate(std::int64_t bytes) asm("Ownership_Allocate");
    static std::int64_t AllocateAligned(std::int64_t bytes, std::int64_t alignment) asm("Ownership_AllocateAligned");
    static void Copy(std::int64_t destination, std::int64_t source, std::int64_t count) asm("Ownership_Copy");
    static void Free(std::int64_t pointer) asm("Ownership_Free");
};

class List {
public:
    std::int64_t* items;
    std::int64_t count;
    std::int64_t capacity;
    static std::int64_t Create() asm("List_Create");
    static std::int64_t Count(std::int64_t list) asm("List_Count");
    static std::int64_t Get(std::int64_t list, std::int64_t index) asm("List_Get");
    static void Add(std::int64_t list, std::int64_t item) asm("List_Add");
    static void Set(std::int64_t list, std::int64_t index, std::int64_t item) asm("List_Set");
    static std::int64_t TakeFront(std::int64_t list) asm("List_TakeFront");
    static std::int64_t TakeBack(std::int64_t list) asm("List_TakeBack");
    static std::int64_t Range(std::int64_t start, std::int64_t end, std::int64_t inclusive) asm("List_Range");
    static void Sort(std::int64_t list) asm("List_Sort");
    static void Reverse(std::int64_t list) asm("List_Reverse");
    static std::int64_t TextOf(std::int64_t list, std::int64_t tag) asm("List_TextOf");
    static std::int64_t Settle(std::int64_t list) asm("List_Settle");
};

class Map {
public:
    std::int64_t* keys;
    std::int64_t* values;
    std::int64_t count;
    std::int64_t capacity;
    std::int64_t kind;
    static int KeysEqual(std::int64_t kind, std::int64_t left, std::int64_t right);
    static std::int64_t Create(std::int64_t kind) asm("Map_Create");
    static void Put(std::int64_t map, std::int64_t key, std::int64_t item) asm("Map_Put");
    static std::int64_t Get(std::int64_t map, std::int64_t key) asm("Map_Get");
    static void AddToSet(std::int64_t list, std::int64_t item, std::int64_t kind) asm("Map_AddToSet");
    static std::int64_t SetHas(std::int64_t list, std::int64_t item, std::int64_t kind) asm("Map_SetHas");
};

class Object {
public:
    static std::int64_t Create(std::int64_t fields) asm("Object_Create");
    static std::int64_t Get(std::int64_t object, std::int64_t index) asm("Object_Get");
    static void Set(std::int64_t object, std::int64_t index, std::int64_t value) asm("Object_Set");
};

class Text {
public:
    static char* Characters(std::int64_t text);
    static std::int64_t Box(char* text);
    static char* CopyBytes(const char* text, std::int64_t length);
    static std::int64_t ClampIndex(std::int64_t length, std::int64_t index);
    static std::int64_t MapCase(std::int64_t text, int upper);
    static std::int64_t Literal(const char* text);
    static std::int64_t Join(std::int64_t text, std::int64_t extra);
    static std::int64_t Length(std::int64_t text) asm("Text_Length");
    static std::int64_t Equals(std::int64_t left, std::int64_t right) asm("Text_Equals");
    static std::int64_t Concat(std::int64_t left, std::int64_t right) asm("Text_Concat");
    static std::int64_t Substring(std::int64_t text, std::int64_t start, std::int64_t length) asm("Text_Substring");
    static std::int64_t Character(std::int64_t text, std::int64_t index) asm("Text_Character");
    static std::int64_t Lower(std::int64_t text) asm("Text_Lower");
    static std::int64_t Upper(std::int64_t text) asm("Text_Upper");
    static std::int64_t StartsWith(std::int64_t text, std::int64_t prefix) asm("Text_StartsWith");
    static std::int64_t EndsWith(std::int64_t text, std::int64_t suffix) asm("Text_EndsWith");
    static std::int64_t Contains(std::int64_t text, std::int64_t needle) asm("Text_Contains");
    static std::int64_t IndexOf(std::int64_t text, std::int64_t needle, std::int64_t start) asm("Text_IndexOf");
    static std::int64_t LastIndexOf(std::int64_t text, std::int64_t needle, std::int64_t start) asm("Text_LastIndexOf");
    static std::int64_t Replace(std::int64_t text, std::int64_t oldText, std::int64_t newText) asm("Text_Replace");
    static std::int64_t ToInt(std::int64_t text) asm("Text_ToInt");
    static double ToDouble(std::int64_t text);
    static std::int64_t FromInt(std::int64_t value) asm("Text_FromInt");
    static std::int64_t FromBool(std::int64_t value) asm("Text_FromBool");
    static std::int64_t FromChar(std::int64_t value) asm("Text_FromChar");
    static std::int64_t FromChars(std::int64_t list) asm("Text_FromChars");
    static std::int64_t IsInt(std::int64_t text) asm("Text_IsInt");
    static std::int64_t Trim(std::int64_t text) asm("Text_Trim");
    static std::int64_t Split(std::int64_t text, std::int64_t separator) asm("Text_Split");
};

class Files {
public:
    static void MakeParents(const char* path);
    static void RemoveTree(const char* path);
    static void Walk(std::int64_t list, const char* root, const char* relative);
    static std::int64_t Read(std::int64_t path) asm("Files_Read");
    static void WriteText(std::int64_t path, std::int64_t text) asm("Files_WriteText");
    static void WriteBytes(std::int64_t path, std::int64_t bytes) asm("Files_WriteBytes");
    static std::int64_t Exists(std::int64_t path) asm("Files_Exists");
    static void Delete(std::int64_t path) asm("Files_Delete");
    static void CreateDirectory(std::int64_t path) asm("Files_CreateDirectory");
    static void DeleteDirectory(std::int64_t path) asm("Files_DeleteDirectory");
    static std::int64_t DirectoryExists(std::int64_t path) asm("Files_DirectoryExists");
    static std::int64_t List(std::int64_t path) asm("Files_List");
    static std::int64_t Name(std::int64_t path) asm("Files_Name");
    static std::int64_t Join(std::int64_t directory, std::int64_t name) asm("Files_Join");
};

class Process {
public:
    static void Print(std::int64_t value, std::int64_t kind) asm("Process_Print");
    static std::int64_t Environment(std::int64_t name) asm("Process_Environment");
    static std::int64_t Run(std::int64_t command) asm("Process_Run");
    static std::int64_t Arguments(std::int32_t count, char** values) asm("Process_Arguments");
    static void Finish(std::int64_t result) asm("Process_Finish");
    static std::int64_t Input() asm("Process_Input");
    static std::int64_t ReadLine() asm("Process_ReadLine");
};

class Math {
public:
    static std::int64_t BitAnd(std::int64_t left, std::int64_t right);
    static std::int64_t BitOr(std::int64_t left, std::int64_t right);
    static std::int64_t BitXor(std::int64_t left, std::int64_t right);
    static std::int64_t ShiftLeft(std::int64_t left, std::int64_t right);
    static std::int64_t ShiftRight(std::int64_t left, std::int64_t right);
    static std::int64_t GreatestCommonDivisor(std::int64_t left, std::int64_t right) asm("Math_GreatestCommonDivisor");
    static std::int64_t IntegerSquareRoot(std::int64_t value) asm("Math_IntegerSquareRoot");
};

class Float64 {
public:
    static std::int64_t Box(double value);
    static double Unbox(std::int64_t bits);
    static void Write(double value, char* buffer, std::size_t size);
    static std::int64_t Parse(std::int64_t text) asm("Float64_Parse");
    static std::int64_t Text(std::int64_t bits) asm("Float64_Text");
    static std::int64_t FromInt(std::int64_t value) asm("Float64_FromInt");
    static std::int64_t FormatFixed(std::int64_t text, std::int64_t places) asm("Float64_FormatFixed");
    static std::int64_t Add(std::int64_t left, std::int64_t right);
    static std::int64_t Subtract(std::int64_t left, std::int64_t right);
    static std::int64_t Multiply(std::int64_t left, std::int64_t right);
    static std::int64_t Divide(std::int64_t left, std::int64_t right);
    static std::int64_t Negate(std::int64_t bits);
    static std::int64_t ToInt(std::int64_t bits) asm("Float64_ToInt");
    static std::int64_t ToIntMode(std::int64_t bits, std::int64_t mode) asm("Float64_ToIntMode");
    static std::int64_t ToIntChecked(std::int64_t bits) asm("Float64_ToIntChecked");
    static std::int64_t Format(std::int64_t bits, std::int64_t format) asm("Float64_Format");
    static std::int64_t Compare(std::int64_t left, std::int64_t right);
    static std::int64_t ParseAlias(std::int64_t text);
    static std::int64_t TextAlias(std::int64_t bits);
    static std::int64_t FromIntAlias(std::int64_t value);
    static std::int64_t ToIntAlias(std::int64_t bits);
};

class Float32 {
public:
    static std::uint16_t ToFloat16(float value);
    static std::int64_t Parse(std::int64_t text) asm("Float32_Parse");
};

class Float16 {
public:
    static float ToFloat32(std::uint16_t value);
    static std::int64_t Parse(std::int64_t text) asm("Float16_Parse");
};

class Unsigned64 {
public:
    static std::int64_t Parse(std::int64_t text) asm("Unsigned64_Parse");
};

class Any {
public:
    static std::int64_t Field(std::int64_t box, std::int64_t index);
    static std::int64_t TaggedText(std::int64_t tag, std::int64_t payload);
    static std::int64_t ListText(std::int64_t list) asm("Any_ListText");
    static std::int64_t MapText(std::int64_t map) asm("Any_MapText");
    static std::int64_t Text(std::int64_t box) asm("Any_Text");
    static std::int64_t ToInt(std::int64_t box) asm("Any_ToInt");
    static std::int64_t ToFloat(std::int64_t box) asm("Any_ToFloat");
    static std::int64_t Length(std::int64_t box) asm("Any_Length");
    static std::int64_t Upper(std::int64_t box) asm("Any_Upper");
    static std::int64_t Lower(std::int64_t box) asm("Any_Lower");
};




char *Text::Characters(int64_t text) {
    if (text == 0) {
        return const_cast<char*>("");
    }
    return (char *)(uintptr_t)text;
}


int64_t Text::Box(char *text) {
    if (text == NULL) {
        text = static_cast<char*>(std::calloc(1, 1));
    }
    return Ownership::Export((int64_t)(uintptr_t)text, 1, 0);
}

char *Text::CopyBytes(const char *text, int64_t length) {
    char *copy = static_cast<char*>(std::malloc((size_t)length + 1));
    if (copy == NULL) {
        return static_cast<char*>(std::calloc(1, 1));
    }
    if (length > 0 && text != NULL) {
        memcpy(copy, text, (size_t)length);
    }
    copy[length] = 0;
    return copy;
}

int64_t Text::ClampIndex(int64_t length, int64_t index) {
    if (index < 0) {
        return 0;
    }
    if (index > length) {
        return length;
    }
    return index;
}


/* Deterministic ownership. Not a garbage collector: every heap string, list,
   map, and object has a refcount, and the compiler releases it at scope exit
   or when a temporary dies. Raw Memory.Allocate pointers are not tracked. */




uint64_t Ownership::Hash(int64_t pointer) {
    return (uint64_t)pointer * 11400714819323198485ull;
}

void Ownership::Grow(void) {
    int next = Ownership::entryCapacity == 0 ? 1024 : Ownership::entryCapacity * 2;
    Ownership::Entry *grown = static_cast<Ownership::Entry *>(std::calloc((size_t)next, sizeof(Ownership::Entry)));
    int index = 0;
    if (grown == NULL) {
        return;
    }
    while (index < Ownership::entryCapacity) {
        if (Ownership::entries[index].state == Ownership::LIVE) {
            uint64_t slot = Ownership::Hash(Ownership::entries[index].pointer) % (uint64_t)next;
            while (grown[slot].state == Ownership::LIVE) {
                slot = (slot + 1) % (uint64_t)next;
            }
            grown[slot] = Ownership::entries[index];
        }
        index = index + 1;
    }
    free(Ownership::entries);
    Ownership::entries = grown;
    Ownership::entryCapacity = next;
}

Ownership::Entry *Ownership::Find(int64_t pointer, int create) {
    uint64_t slot = 0;
    uint64_t firstTomb = (uint64_t)-1;
    int seen = 0;
    if (pointer == 0) {
        return NULL;
    }
    if (Ownership::entryCapacity == 0) {
        if (!create) {
            return NULL;
        }
        Ownership::Grow();
    }
    slot = Ownership::Hash(pointer) % (uint64_t)Ownership::entryCapacity;
    while (seen < Ownership::entryCapacity) {
        Ownership::Entry *entry = &Ownership::entries[slot];
        if (entry->state == Ownership::EMPTY) {
            if (!create) {
                return NULL;
            }
            if (firstTomb != (uint64_t)-1) {
                return &Ownership::entries[firstTomb];
            }
            return entry;
        }
        if (entry->state == Ownership::TOMB && firstTomb == (uint64_t)-1) {
            firstTomb = slot;
        }
        if (entry->state == Ownership::LIVE && entry->pointer == pointer) {
            return entry;
        }
        slot = (slot + 1) % (uint64_t)Ownership::entryCapacity;
        seen = seen + 1;
    }
    if (create && firstTomb != (uint64_t)-1) {
        return &Ownership::entries[firstTomb];
    }
    return NULL;
}


void Ownership::Destroy(int64_t pointer, int kind, int aux) {
    if (kind == Ownership::KIND_LIST) {
        List *list = (List *)(uintptr_t)pointer;
        int64_t count = list == NULL ? 0 : list->count;
        int64_t *items = list == NULL ? NULL : list->items;
        int64_t index = 0;
        if (list != NULL) {
            list->items = NULL;
            list->count = 0;
        }
        while (index < count) {
            Ownership::Release(items[index]);
            index = index + 1;
        }
        free(items);
        free(list);
        return;
    }
    if (kind == Ownership::KIND_MAP) {
        Map *map = (Map *)(uintptr_t)pointer;
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
            Ownership::Release(keys[index]);
            Ownership::Release(values[index]);
            index = index + 1;
        }
        free(keys);
        free(values);
        free(map);
        return;
    }
    if (kind == Ownership::KIND_OBJECT) {
        int64_t *fields = (int64_t *)(uintptr_t)pointer;
        int64_t index = 0;
        while (index < aux) {
            int64_t child = fields[index];
            fields[index] = 0;
            Ownership::Release(child);
            index = index + 1;
        }
        free(fields);
        return;
    }
    free((void *)(uintptr_t)pointer);
}

void Ownership::Track(int64_t pointer, int kind, int64_t aux) {
    Ownership::Entry *entry = NULL;
    if (pointer == 0) {
        return;
    }
    if (Ownership::entryCapacity == 0 || Ownership::liveCount * 10 > Ownership::entryCapacity * 7) {
        Ownership::Grow();
    }
    entry = Ownership::Find(pointer, 1);
    if (entry == NULL) {
        return;
    }
    if (entry->state == Ownership::LIVE && entry->pointer == pointer) {
        entry->count = entry->count + 1;
        return;
    }
    entry->pointer = pointer;
    entry->count = 1;
    entry->kind = kind;
    entry->aux = (int32_t)aux;
    entry->state = Ownership::LIVE;
    Ownership::liveCount = Ownership::liveCount + 1;
}

void Ownership::PushTemporary(int64_t pointer) {
    if (pointer == 0) {
        return;
    }
    if (Ownership::temporaryCount == Ownership::temporaryCapacity) {
        int next = Ownership::temporaryCapacity == 0 ? 256 : Ownership::temporaryCapacity * 2;
        int64_t *grown = static_cast<int64_t *>(std::realloc(Ownership::temporaries, (size_t)next * sizeof(int64_t)));
        if (grown == NULL) {
            return;
        }
        Ownership::temporaries = grown;
        Ownership::temporaryCapacity = next;
    }
    Ownership::temporaries[Ownership::temporaryCount] = pointer;
    Ownership::temporaryCount = Ownership::temporaryCount + 1;
}

int64_t Ownership::Export(int64_t pointer, int kind, int64_t aux) {
    if (pointer == 0) {
        return 0;
    }
    Ownership::Track(pointer, kind, aux);
    Ownership::PushTemporary(pointer);
    return pointer;
}

void Ownership::Release(int64_t pointer) {
    Ownership::Entry *entry = Ownership::Find(pointer, 0);
    int kind = 0;
    int aux = 0;
    if (entry == NULL || entry->state != Ownership::LIVE) {
        return;
    }
    entry->count = entry->count - 1;
    if (entry->count > 0) {
        return;
    }
    kind = entry->kind;
    aux = entry->aux;
    entry->state = Ownership::TOMB;
    entry->pointer = 0;
    entry->count = 0;
    Ownership::liveCount = Ownership::liveCount - 1;
    Ownership::Destroy(pointer, kind, aux);
}

void Ownership::Retain(int64_t pointer) {
    Ownership::Entry *entry = Ownership::Find(pointer, 0);
    if (entry == NULL || entry->state != Ownership::LIVE) {
        return;
    }
    entry->count = entry->count + 1;
}

void Ownership::PushScope(void) {
    if (Ownership::markCount == Ownership::markCapacity) {
        int next = Ownership::markCapacity == 0 ? 64 : Ownership::markCapacity * 2;
        int *grown = static_cast<int *>(std::realloc(Ownership::marks, (size_t)next * sizeof(int)));
        if (grown == NULL) {
            return;
        }
        Ownership::marks = grown;
        Ownership::markCapacity = next;
    }
    Ownership::marks[Ownership::markCount] = Ownership::temporaryCount;
    Ownership::markCount = Ownership::markCount + 1;
}

void Ownership::PopScope(void) {
    int mark = 0;
    if (Ownership::markCount > 0) {
        Ownership::markCount = Ownership::markCount - 1;
        mark = Ownership::marks[Ownership::markCount];
    }
    while (Ownership::temporaryCount > mark) {
        Ownership::temporaryCount = Ownership::temporaryCount - 1;
        Ownership::Release(Ownership::temporaries[Ownership::temporaryCount]);
    }
}

void Ownership::Give(int64_t pointer) {
    Ownership::PushTemporary(pointer);
}

int64_t Ownership::MarkDepth(void) {
    return Ownership::markCount;
}

void Ownership::Unwind(int64_t depth) {
    while (Ownership::markCount > depth) {
        Ownership::PopScope();
    }
}

int64_t Ownership::Keep(int64_t pointer) {
    int start = 0;
    int index = 0;
    if (pointer == 0) {
        return 0;
    }
    if (Ownership::markCount > 0) {
        start = Ownership::marks[Ownership::markCount - 1];
    }
    index = Ownership::temporaryCount - 1;
    while (index >= start) {
        if (Ownership::temporaries[index] == pointer) {
            Ownership::temporaries[index] = Ownership::temporaries[Ownership::temporaryCount - 1];
            Ownership::temporaryCount = Ownership::temporaryCount - 1;
            return 1;
        }
        index = index - 1;
    }
    return 0;
}

int64_t Ownership::Allocate(int64_t bytes) {
    void *memory = NULL;
    if (bytes < 1) {
        bytes = 1;
    }
    memory = static_cast<void*>(std::calloc(1, (size_t)bytes));
    return (int64_t)(uintptr_t)memory;
}

int64_t Ownership::AllocateAligned(int64_t bytes, int64_t alignment) {
    void *memory = NULL;
    if (bytes < 1) {
        bytes = 1;
    }
    if (alignment < (std::int64_t)sizeof(void *)) {
        alignment = (int64_t)sizeof(void *);
    }
    if ((alignment & (alignment - 1)) != 0) {
        alignment = (int64_t)sizeof(void *);
    }
#if defined(_WIN32)
    if ((bytes % alignment) != 0) {
        bytes += alignment - (bytes % alignment);
    }
    memory = aligned_alloc((size_t)alignment, (size_t)bytes);
    if (memory == NULL) {
        return 0;
    }
#else
    if (posix_memalign(&memory, (size_t)alignment, (size_t)bytes) != 0) {
        return 0;
    }
#endif
    memset(memory, 0, (size_t)bytes);
    return (int64_t)(uintptr_t)memory;
}

int Map::KeysEqual(int64_t kind, int64_t left, int64_t right) {
    int64_t leftTag = 0;
    int64_t rightTag = 0;
    int64_t leftPayload = 0;
    int64_t rightPayload = 0;
    if (kind == 1) {
        return strcmp(Text::Characters(left), Text::Characters(right)) == 0;
    }
    if (kind == 2) {
        leftTag = Object::Get(left, 0);
        rightTag = Object::Get(right, 0);
        leftPayload = Object::Get(left, 1);
        rightPayload = Object::Get(right, 1);
        if (leftTag != rightTag) {
            return 0;
        }
        if (leftTag == 2) {
            return strcmp(Text::Characters(leftPayload), Text::Characters(rightPayload)) == 0;
        }
        return leftPayload == rightPayload;
    }
    return left == right;
}

int64_t Map::Create(int64_t kind) {
    Map *map = static_cast<Map *>(std::calloc(1, sizeof(Map)));
    if (map != NULL) {
        map->kind = kind;
    }
    return Ownership::Export((int64_t)(uintptr_t)map, Ownership::KIND_MAP, 0);
}

void Map::Put(int64_t map, int64_t key, int64_t item) {
    Map *value = (Map *)(uintptr_t)map;
    int64_t index = 0;
    int64_t cap = 0;
    int64_t *keys = NULL;
    int64_t *items = NULL;
    if (value == NULL) {
        return;
    }
    while (index < value->count) {
        if (Map::KeysEqual(value->kind, value->keys[index], key)) {
            Ownership::Release(value->values[index]);
            Ownership::Retain(item);
            value->values[index] = item;
            return;
        }
        index = index + 1;
    }
    if (value->count + 1 > value->capacity) {
        cap = value->capacity == 0 ? 8 : value->capacity * 2;
        keys = static_cast<int64_t*>(std::realloc(value->keys, (size_t)cap * sizeof(int64_t)));
        items = static_cast<int64_t*>(std::realloc(value->values, (size_t)cap * sizeof(int64_t)));
        if (keys == NULL || items == NULL) {
            return;
        }
        value->keys = keys;
        value->values = items;
        value->capacity = cap;
    }
    Ownership::Retain(key);
    Ownership::Retain(item);
    value->keys[value->count] = key;
    value->values[value->count] = item;
    value->count = value->count + 1;
}

int64_t Map::Get(int64_t map, int64_t key) {
    Map *value = (Map *)(uintptr_t)map;
    int64_t index = 0;
    if (value == NULL) {
        return 0;
    }
    while (index < value->count) {
        if (Map::KeysEqual(value->kind, value->keys[index], key)) {
            return value->values[index];
        }
        index = index + 1;
    }
    return 0;
}

void Map::AddToSet(int64_t list, int64_t item, int64_t kind) {
    List *value = (List *)(uintptr_t)list;
    int64_t index = 0;
    if (value == NULL) {
        return;
    }
    while (index < value->count) {
        if (Map::KeysEqual(kind, value->items[index], item)) {
            return;
        }
        index = index + 1;
    }
    List::Add(list, item);
}

int64_t Map::SetHas(int64_t list, int64_t item, int64_t kind) {
    List *value = (List *)(uintptr_t)list;
    int64_t index = 0;
    if (value == NULL) {
        return 0;
    }
    while (index < value->count) {
        if (Map::KeysEqual(kind, value->items[index], item)) {
            return 1;
        }
        index = index + 1;
    }
    return 0;
}

int64_t List::Create(void) {
    List *list = static_cast<List *>(std::calloc(1, sizeof(List)));
    return Ownership::Export((int64_t)(uintptr_t)list, Ownership::KIND_LIST, 0);
}

int64_t List::Count(int64_t list) {
    List *value = (List *)(uintptr_t)list;
    if (value == NULL) {
        return 0;
    }
    return value->count;
}

int64_t List::TakeFront(int64_t list) {
    List *value = (List *)(uintptr_t)list;
    int64_t item = 0;
    int64_t index = 0;
    if (value == NULL || value->count <= 0) {
        return 0;
    }
    item = value->items[0];
    if (Ownership::Find(item, 0) != NULL) {
        Ownership::PushTemporary(item);
    }
    while (index + 1 < value->count) {
        value->items[index] = value->items[index + 1];
        index = index + 1;
    }
    value->count = value->count - 1;
    return item;
}

int64_t List::TakeBack(int64_t list) {
    List *value = (List *)(uintptr_t)list;
    int64_t item = 0;
    if (value == NULL || value->count <= 0) {
        return 0;
    }
    value->count = value->count - 1;
    item = value->items[value->count];
    if (Ownership::Find(item, 0) != NULL) {
        Ownership::PushTemporary(item);
    }
    return item;
}

int64_t List::Range(int64_t start, int64_t end, int64_t inclusive) {
    int64_t list = List::Create();
    int64_t last = end;
    int64_t cursor = start;
    if (inclusive == 0) {
        last = end - 1;
    }
    while (cursor <= last) {
        List::Add(list, cursor);
        cursor = cursor + 1;
    }
    return list;
}

int64_t Math::BitAnd(int64_t left, int64_t right) { return left & right; }
int64_t Math::BitOr(int64_t left, int64_t right) { return left | right; }
int64_t Math::BitXor(int64_t left, int64_t right) { return left ^ right; }

int64_t Math::ShiftLeft(int64_t left, int64_t right) {
    if (right < 0 || right >= 64) {
        return 0;
    }
    return left << (unsigned)right;
}

int64_t Math::ShiftRight(int64_t left, int64_t right) {
    if (right < 0 || right >= 64) {
        return left < 0 ? -1 : 0;
    }
    return left >> (unsigned)right;
}

int64_t List::Get(int64_t list, int64_t index) {
    List *value = (List *)(uintptr_t)list;
    if (value == NULL || index < 0 || index >= value->count) {
        return 0;
    }
    return value->items[index];
}

void List::Add(int64_t list, int64_t item) {
    List *value = (List *)(uintptr_t)list;
    if (value == NULL) {
        return;
    }
    if (value->count + 1 > value->capacity) {
        int64_t cap = value->capacity == 0 ? 8 : value->capacity * 2;
        int64_t *items = static_cast<int64_t *>(std::realloc(value->items, (size_t)cap * sizeof(int64_t)));
        if (items == NULL) {
            return;
        }
        value->items = items;
        value->capacity = cap;
    }
    if (Ownership::Find(list, 0) != NULL) {
        Ownership::Retain(item);
    }
    value->items[value->count] = item;
    value->count = value->count + 1;
}

void List::Set(int64_t list, int64_t index, int64_t item) {
    List *value = (List *)(uintptr_t)list;
    if (value == NULL || index < 0) {
        return;
    }
    while (value->count <= index) {
        List::Add(list, 0);
    }
    if (Ownership::Find(list, 0) != NULL) {
        Ownership::Release(value->items[index]);
        Ownership::Retain(item);
    }
    value->items[index] = item;
}

int64_t Object::Create(int64_t fields) {
    int64_t *object = NULL;
    if (fields < 1) {
        fields = 1;
    }
    object = static_cast<int64_t*>(std::calloc((size_t)fields, sizeof(int64_t)));
    return Ownership::Export((int64_t)(uintptr_t)object, Ownership::KIND_OBJECT, fields);
}

int64_t Object::Get(int64_t object, int64_t index) {
    int64_t *fields = (int64_t *)(uintptr_t)object;
    if (fields == NULL || index < 0) {
        return 0;
    }
    return fields[index];
}

void Object::Set(int64_t object, int64_t index, int64_t value) {
    int64_t *fields = (int64_t *)(uintptr_t)object;
    Ownership::Entry *owner = Ownership::Find(object, 0);
    if (fields == NULL || index < 0) {
        return;
    }
    if (owner != NULL && owner->state == Ownership::LIVE && index < owner->aux) {
        Ownership::Retain(value);
        Ownership::Release(fields[index]);
    }
    fields[index] = value;
}

void Ownership::Copy(int64_t dest, int64_t src, int64_t count) {
    if (dest == 0 || src == 0 || count <= 0) {
        return;
    }
    memmove((void *)(uintptr_t)dest, (void *)(uintptr_t)src, (size_t)count);
}

void Ownership::Free(int64_t pointer) {
    Ownership::Entry *entry = NULL;
    if (pointer == 0) {
        return;
    }
    entry = Ownership::Find(pointer, 0);
    if (entry != NULL && entry->state == Ownership::LIVE) {
        entry->count = 1;
        Ownership::Release(pointer);
        return;
    }
    free((void *)(uintptr_t)pointer);
}

int64_t Text::Length(int64_t text) {
    return (int64_t)strlen(Text::Characters(text));
}

int64_t Text::Equals(int64_t left, int64_t right) {
    return strcmp(Text::Characters(left), Text::Characters(right)) == 0 ? 1 : 0;
}

int64_t Text::Concat(int64_t left, int64_t right) {
    const char *a = Text::Characters(left);
    const char *b = Text::Characters(right);
    int64_t an = (int64_t)strlen(a);
    int64_t bn = (int64_t)strlen(b);
    char *copy = static_cast<char*>(std::malloc((size_t)an + (size_t)bn + 1));
    if (copy == NULL) {
        return Text::Box(NULL);
    }
    memcpy(copy, a, (size_t)an);
    memcpy(copy + an, b, (size_t)bn);
    copy[an + bn] = 0;
    return Text::Box(copy);
}

int64_t Text::Substring(int64_t text, int64_t start, int64_t length) {
    const char *value = Text::Characters(text);
    int64_t total = (int64_t)strlen(value);
    start = Text::ClampIndex(total, start);
    if (length < 0) {
        length = 0;
    }
    if (start + length > total) {
        length = total - start;
    }
    return Text::Box(Text::CopyBytes(value + start, length));
}

int64_t Text::Character(int64_t text, int64_t index) {
    const char *value = Text::Characters(text);
    int64_t total = (int64_t)strlen(value);
    if (index < 0 || index >= total) {
        return 0;
    }
    return (unsigned char)value[index];
}

int64_t Text::MapCase(int64_t text, int upper) {
    const char *value = Text::Characters(text);
    int64_t total = (int64_t)strlen(value);
    char *copy = Text::CopyBytes(value, total);
    int64_t index = 0;
    while (index < total) {
        unsigned char character = (unsigned char)copy[index];
        copy[index] = (char)(upper ? toupper(character) : tolower(character));
        index = index + 1;
    }
    return (int64_t)(uintptr_t)copy;
}

int64_t Text::Lower(int64_t text) {
    return Text::MapCase(text, 0);
}

int64_t Text::Upper(int64_t text) {
    return Text::MapCase(text, 1);
}

int64_t Text::StartsWith(int64_t text, int64_t prefix) {
    const char *value = Text::Characters(text);
    const char *head = Text::Characters(prefix);
    int64_t length = (int64_t)strlen(head);
    return strncmp(value, head, (size_t)length) == 0 ? 1 : 0;
}

int64_t Text::EndsWith(int64_t text, int64_t suffix) {
    const char *value = Text::Characters(text);
    const char *tail = Text::Characters(suffix);
    int64_t total = (int64_t)strlen(value);
    int64_t length = (int64_t)strlen(tail);
    if (length > total) {
        return 0;
    }
    return memcmp(value + total - length, tail, (size_t)length) == 0 ? 1 : 0;
}

int64_t Text::Contains(int64_t text, int64_t needle) {
    return strstr(Text::Characters(text), Text::Characters(needle)) != NULL ? 1 : 0;
}

int64_t Text::IndexOf(int64_t text, int64_t needle, int64_t start) {
    const char *value = Text::Characters(text);
    const char *pattern = Text::Characters(needle);
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

int64_t Text::LastIndexOf(int64_t text, int64_t needle, int64_t start) {
    const char *value = Text::Characters(text);
    const char *pattern = Text::Characters(needle);
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

int64_t Text::Replace(int64_t text, int64_t oldText, int64_t newText) {
    const char *value = Text::Characters(text);
    const char *old = Text::Characters(oldText);
    const char *replacement = Text::Characters(newText);
    int64_t oldLen = (int64_t)strlen(old);
    int64_t newLen = (int64_t)strlen(replacement);
    if (oldLen == 0) {
        return Text::Box(Text::CopyBytes(value, (int64_t)strlen(value)));
    }
    int64_t count = 0;
    const char *scan = value;
    while ((scan = strstr(scan, old)) != NULL) {
        count = count + 1;
        scan = scan + oldLen;
    }
    int64_t total = (int64_t)strlen(value) + count * (newLen - oldLen);
    char *copy = static_cast<char*>(std::malloc((size_t)total + 1));
    if (copy == NULL) {
        return Text::Box(NULL);
    }
    char *out = copy;
    scan = value;
    const char *hit = NULL;
    while ((hit = strstr(scan, old)) != NULL) {
        memcpy(out, scan, (size_t)(hit - scan));
        out += hit - scan;
        memcpy(out, replacement, (size_t)newLen);
        out += newLen;
        scan = hit + oldLen;
    }
    strcpy(out, scan);
    return (int64_t)(uintptr_t)copy;
}

int64_t Text::ToInt(int64_t text) {
    return (int64_t)strtoll(Text::Characters(text), NULL, 10);
}

double Text::ToDouble(int64_t text) {
    return strtod(Text::Characters(text), NULL);
}

int64_t Float64::Box(double value) {
    int64_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

double Float64::Unbox(int64_t bits) {
    double value = 0;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

void Float64::Write(double value, char *buffer, size_t size) {
    snprintf(buffer, size, "%.15g", value);
}

int64_t Float64::Parse(int64_t text) {
    return Float64::Box(strtod(Text::Characters(text), NULL));
}

int64_t Float32::Parse(int64_t text) {
    return Float64::Box((double)strtof(Text::Characters(text), NULL));
}

int64_t Float64::Text(int64_t bits) {
    char buffer[64];
    Float64::Write(Float64::Unbox(bits), buffer, sizeof(buffer));
    return Text::Box(Text::CopyBytes(buffer, (int64_t)strlen(buffer)));
}

int64_t Float64::FromInt(int64_t value) {
    return Float64::Box((double)value);
}

int64_t Float64::FormatFixed(int64_t text, int64_t places) {
    const char *raw = Text::Characters(text);
    if (places < 0) {
        return Text::Box(Text::CopyBytes(raw, (int64_t)strlen(raw)));
    }
    if (places > 18) {
        places = 18;
    }
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%.*f", (int)places, strtod(raw, NULL));
    return Text::Box(Text::CopyBytes(buffer, (int64_t)strlen(buffer)));
}

int64_t Float64::Add(int64_t left, int64_t right) {
    return Float64::Box(Float64::Unbox(left) + Float64::Unbox(right));
}

int64_t Float64::Subtract(int64_t left, int64_t right) {
    return Float64::Box(Float64::Unbox(left) - Float64::Unbox(right));
}

int64_t Float64::Multiply(int64_t left, int64_t right) {
    return Float64::Box(Float64::Unbox(left) * Float64::Unbox(right));
}

int64_t Float64::Divide(int64_t left, int64_t right) {
    return Float64::Box(Float64::Unbox(left) / Float64::Unbox(right));
}

int64_t Float64::Negate(int64_t bits) {
    return Float64::Box(-Float64::Unbox(bits));
}

int64_t Float64::ToInt(int64_t bits) {
    return (int64_t)Float64::Unbox(bits);
}

int64_t Float64::ToIntMode(int64_t bits, int64_t mode) {
    double value = Float64::Unbox(bits);
    if (mode == 0) {
        if (value >= 0) {
            value = value + 0.5;
        } else {
            value = value - 0.5;
        }
    }
    return (int64_t)value;
}

int64_t Float64::ToIntChecked(int64_t bits) {
    double value = Float64::Unbox(bits);
    if (!(value == value) || value >= 9223372036854775808.0 || value < -9223372036854775808.0) {
        fputs("error RX121 Conversion is out of range.\n", stderr);
        exit(1);
    }
    return (int64_t)value;
}

int64_t Float64::Format(int64_t bits, int64_t format) {
    const char *spec = Text::Characters(format);
    double value = Float64::Unbox(bits);
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
        Float64::Write(value, buffer, sizeof(buffer));
    }
    return Text::Box(Text::CopyBytes(buffer, (int64_t)strlen(buffer)));
}

int64_t Text::IsInt(int64_t text) {
    const char *raw = Text::Characters(text);
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

int64_t Float64::Compare(int64_t left, int64_t right) {
    double first = Float64::Unbox(left);
    double second = Float64::Unbox(right);
    if (first < second) {
        return -1;
    }
    if (first > second) {
        return 1;
    }
    return 0;
}

int64_t Text::FromInt(int64_t value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
    return Text::Box(Text::CopyBytes(buffer, (int64_t)strlen(buffer)));
}

int64_t Text::FromBool(int64_t value) {
    const char *text = value ? "True" : "False";
    return Text::Box(Text::CopyBytes(text, (int64_t)strlen(text)));
}

int64_t Text::FromChar(int64_t value) {
    char *copy = static_cast<char*>(std::malloc(2));
    copy[0] = (char)value;
    copy[1] = 0;
    return (int64_t)(uintptr_t)copy;
}

int64_t Text::FromChars(int64_t list) {
    List *chars = (List *)(uintptr_t)list;
    if (chars == NULL) {
        return Text::Box(Text::CopyBytes("", 0));
    }
    char *copy = static_cast<char*>(std::malloc((size_t)chars->count * 4 + 1));
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

std::uint16_t Float32::ToFloat16(float value) {
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

float Float16::ToFloat32(uint16_t value) {
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

int64_t Unsigned64::Parse(int64_t text) {
    const char *raw = Text::Characters(text);
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

int64_t Float16::Parse(int64_t text) {
    float value = strtof(Text::Characters(text), NULL);
    return Float64::Box((double)Float16::ToFloat32(Float32::ToFloat16(value)));
}

void Process::Print(int64_t value, int64_t kind) {
    if (kind == 1) {
        fputs(Text::Characters(value), stdout);
    } else if (kind == 2) {
        fputs(value ? "True" : "False", stdout);
    } else if (kind == 3) {
        fputc((int)value, stdout);
    } else if (kind == 4) {
        char buffer[64];
        Float64::Write(Float64::Unbox(value), buffer, sizeof(buffer));
        fputs(buffer, stdout);
    } else if (kind == 5) {
        printf("%llu", (unsigned long long)value);
    } else {
        printf("%lld", (long long)value);
    }
    fputc('\n', stdout);
    fflush(stdout);
}

int64_t Files::Read(int64_t path) {
    FILE *file = fopen(Text::Characters(path), "rb");
    if (file == NULL) {
        return Text::Box(Text::CopyBytes("", 0));
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size < 0) {
        size = 0;
    }
    fseek(file, 0, SEEK_SET);
    char *copy = static_cast<char*>(std::malloc((size_t)size + 1));
    size_t got = fread(copy, 1, (size_t)size, file);
    copy[got] = 0;
    fclose(file);
    return (int64_t)(uintptr_t)copy;
}

void Files::WriteText(int64_t path, int64_t text) {
    FILE *file = fopen(Text::Characters(path), "wb");
    if (file == NULL) {
        return;
    }
    const char *value = Text::Characters(text);
    fwrite(value, 1, strlen(value), file);
    fclose(file);
}

void Files::WriteBytes(int64_t path, int64_t bytes) {
    FILE *file = fopen(Text::Characters(path), "wb");
    class List *list = (class List *)(uintptr_t)bytes;
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

int64_t Files::Exists(int64_t path) {
#if defined(_WIN32)
    DWORD attr = GetFileAttributesA(Text::Characters(path));
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return 0;
    }
    return 1;
#else
    struct stat info;
    if (stat(Text::Characters(path), &info) != 0) {
        return 0;
    }
    return S_ISREG(info.st_mode) ? 1 : 0;
#endif
}

void Files::Delete(int64_t path) {
#if defined(_WIN32)
    DeleteFileA(Text::Characters(path));
#else
    unlink(Text::Characters(path));
#endif
}

void Files::MakeParents(const char *path) {
    char *copy = Text::CopyBytes(path, (int64_t)strlen(path));
    int64_t length = (int64_t)strlen(copy);
    int64_t index = 0;
    while (index < length) {
        char mark = copy[index];
#if defined(_WIN32)
        if ((mark == '/' || mark == '\\') && index > 0) {
            copy[index] = 0;
            CreateDirectoryA(copy, NULL);
            copy[index] = mark;
        }
#else
        if (mark == '/' && index > 0) {
            copy[index] = 0;
            mkdir(copy, 0755);
            copy[index] = '/';
        }
#endif
        index = index + 1;
    }
#if defined(_WIN32)
    CreateDirectoryA(copy, NULL);
#else
    mkdir(copy, 0755);
#endif
    free(copy);
}

void Files::CreateDirectory(int64_t path) {
    Files::MakeParents(Text::Characters(path));
}

void Files::RemoveTree(const char *path) {
#if defined(_WIN32)
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return;
    }
    if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        DeleteFileA(path);
        return;
    }
    char pattern[4096];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    WIN32_FIND_DATAA found;
    HANDLE handle = FindFirstFileA(pattern, &found);
    if (handle == INVALID_HANDLE_VALUE) {
        RemoveDirectoryA(path);
        return;
    }
    do {
        if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0) {
            continue;
        }
        char child[4096];
        snprintf(child, sizeof(child), "%s\\%s", path, found.cFileName);
        Files::RemoveTree(child);
    } while (FindNextFileA(handle, &found));
    FindClose(handle);
    RemoveDirectoryA(path);
#else
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
        Files::RemoveTree(child);
    }
    closedir(dir);
    rmdir(path);
#endif
}

void Files::DeleteDirectory(int64_t path) {
    Files::RemoveTree(Text::Characters(path));
}

int64_t Files::DirectoryExists(int64_t path) {
#if defined(_WIN32)
    DWORD attr = GetFileAttributesA(Text::Characters(path));
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return 0;
    }
    return 1;
#else
    struct stat info;
    if (stat(Text::Characters(path), &info) != 0) {
        return 0;
    }
    return S_ISDIR(info.st_mode) ? 1 : 0;
#endif
}

void Files::Walk(int64_t list, const char *root, const char *relative) {
    char full[4096];
    if (relative[0] == 0) {
        snprintf(full, sizeof(full), "%s", root);
    } else {
        snprintf(full, sizeof(full), "%s/%s", root, relative);
    }
#if defined(_WIN32)
    char pattern[4096];
    snprintf(pattern, sizeof(pattern), "%s\\*", full);
    WIN32_FIND_DATAA found;
    HANDLE handle = FindFirstFileA(pattern, &found);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0) {
            continue;
        }
        char childRel[4096];
        if (relative[0] == 0) {
            snprintf(childRel, sizeof(childRel), "%s", found.cFileName);
        } else {
            snprintf(childRel, sizeof(childRel), "%s/%s", relative, found.cFileName);
        }
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            Files::Walk(list, root, childRel);
        } else {
            List::Add(list, Text::Box(Text::CopyBytes(childRel, (int64_t)strlen(childRel))));
        }
    } while (FindNextFileA(handle, &found));
    FindClose(handle);
#else
    DIR *dir = opendir(full);
    if (dir == NULL) {
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char childRel[4096];
        if (relative[0] == 0) {
            snprintf(childRel, sizeof(childRel), "%s", entry->d_name);
        } else {
            snprintf(childRel, sizeof(childRel), "%s/%s", relative, entry->d_name);
        }
        char childFull[4096];
        snprintf(childFull, sizeof(childFull), "%s/%s", root, childRel);
        struct stat info;
        if (stat(childFull, &info) != 0) {
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            Files::Walk(list, root, childRel);
        } else if (S_ISREG(info.st_mode)) {
            List::Add(list, Text::Box(Text::CopyBytes(childRel, (int64_t)strlen(childRel))));
        }
    }
    closedir(dir);
#endif
}

int64_t Files::List(int64_t path) {
    int64_t list = List::Create();
    const char *root = Text::Characters(path);
    if (root[0] == 0) {
        root = ".";
    }
    Files::Walk(list, root, "");
    return list;
}

int64_t Process::Environment(int64_t name) {
    const char *value = getenv(Text::Characters(name));
    if (value == NULL) {
        return Text::Box(Text::CopyBytes("", 0));
    }
    return Text::Box(Text::CopyBytes(value, (int64_t)strlen(value)));
}

int64_t Process::Run(int64_t command) {
    int code = system(Text::Characters(command));
    if (code == -1) {
        return 1;
    }
#if defined(_WIN32)
    return code;
#else
    if (WIFEXITED(code)) {
        return WEXITSTATUS(code);
    }
    return 1;
#endif
}

int64_t Process::Arguments(int32_t argc, char **argv) {
    int64_t list = List::Create();
    int32_t index = 1;
    while (index < argc) {
        List::Add(list, Text::Box(Text::CopyBytes(argv[index], (int64_t)strlen(argv[index]))));
        index = index + 1;
    }
    return list;
}

void Process::Finish(int64_t result) {
    (void)result;
}

int64_t Text::Literal(const char *text) {
    return Text::Box(Text::CopyBytes(text, (int64_t)strlen(text)));
}

int64_t Any::Field(int64_t box, int64_t index) {
    int64_t *fields = (int64_t *)(uintptr_t)box;
    if (fields == NULL) {
        return 0;
    }
    return fields[index];
}

int64_t Any::TaggedText(int64_t tag, int64_t payload) {
    if (tag == 2) {
        return payload;
    }
    if (tag == 3) {
        return Text::FromBool(payload);
    }
    if (tag == 4) {
        return Text::FromChar(payload);
    }
    if (tag == 5) {
        return Float64::Text(payload);
    }
    if (tag == 6) {
        return Text::Literal("Null");
    }
    return Text::FromInt(payload);
}

int64_t Text::Join(int64_t text, int64_t extra) {
    int64_t next = Text::Concat(text, extra);
    if (next != text) {
        Ownership::Release(text);
    }
    return next;
}

int64_t List::TextOf(int64_t list, int64_t tag) {
    int64_t count = List::Count(list);
    int64_t text = Text::Literal("[");
    int64_t comma = Text::Literal(", ");
    int64_t index = 0;
    while (index < count) {
        if (index > 0) {
            text = Text::Join(text, comma);
        }
        text = Text::Join(text, Any::TaggedText(tag, List::Get(list, index)));
        index = index + 1;
    }
    return Text::Join(text, Text::Literal("]"));
}

int64_t List::Settle(int64_t list) {
    int64_t count = List::Count(list);
    int64_t settled = List::Create();
    int64_t prefix = Text::Literal("Fulfilled:");
    int64_t index = 0;
    while (index < count) {
        List::Add(settled, Text::Concat(prefix, Text::FromInt(List::Get(list, index))));
        index = index + 1;
    }
    return settled;
}

int64_t Any::ListText(int64_t list) {
    int64_t count = List::Count(list);
    int64_t text = Text::Literal("[");
    int64_t comma = Text::Literal(", ");
    int64_t index = 0;
    while (index < count) {
        if (index > 0) {
            text = Text::Join(text, comma);
        }
        text = Text::Join(text, Any::Text(List::Get(list, index)));
        index = index + 1;
    }
    return Text::Join(text, Text::Literal("]"));
}

int64_t Any::MapText(int64_t map) {
    Map *value = (Map *)(uintptr_t)map;
    int64_t text = Text::Literal("{");
    int64_t comma = Text::Literal(", ");
    int64_t sep = Text::Literal(": ");
    int64_t index = 0;
    if (value == NULL) {
        return Text::Literal("{}");
    }
    while (index < value->count) {
        if (index > 0) {
            text = Text::Join(text, comma);
        }
        text = Text::Join(text, value->keys[index]);
        text = Text::Join(text, sep);
        text = Text::Join(text, Any::Text(value->values[index]));
        index = index + 1;
    }
    return Text::Join(text, Text::Literal("}"));
}

int64_t Any::Text(int64_t box) {
    if (box == 0) {
        return Text::Literal("Null");
    }
    int64_t tag = Any::Field(box, 0);
    int64_t payload = Any::Field(box, 1);
    if (tag != 7) {
        return Any::TaggedText(tag, payload);
    }
    int64_t element = Any::Field(box, 2);
    int64_t count = List::Count(payload);
    int64_t text = Text::Literal("[");
    int64_t comma = Text::Literal(", ");
    int64_t index = 0;
    while (index < count) {
        if (index > 0) {
            text = Text::Join(text, comma);
        }
        text = Text::Join(text, Any::TaggedText(element, List::Get(payload, index)));
        index = index + 1;
    }
    return Text::Join(text, Text::Literal("]"));
}

int64_t Any::ToInt(int64_t box) {
    if (box == 0) {
        return 0;
    }
    int64_t tag = Any::Field(box, 0);
    int64_t payload = Any::Field(box, 1);
    if (tag == 2) {
        return Text::ToInt(payload);
    }
    if (tag == 5) {
        return Float64::ToInt(payload);
    }
    if (tag == 1 || tag == 3 || tag == 4) {
        return payload;
    }
    return 0;
}

int64_t Any::ToFloat(int64_t box) {
    if (box == 0) {
        return Float64::FromInt(0);
    }
    int64_t tag = Any::Field(box, 0);
    int64_t payload = Any::Field(box, 1);
    if (tag == 5) {
        return payload;
    }
    if (tag == 2) {
        return Float64::Parse(payload);
    }
    if (tag == 1 || tag == 3 || tag == 4) {
        return Float64::FromInt(payload);
    }
    return Float64::FromInt(0);
}

int64_t Any::Length(int64_t box) {
    if (box == 0) {
        return 0;
    }
    int64_t tag = Any::Field(box, 0);
    int64_t payload = Any::Field(box, 1);
    if (tag == 2) {
        return Text::Length(payload);
    }
    if (tag == 7) {
        return List::Count(payload);
    }
    return 0;
}

int64_t Any::Upper(int64_t box) {
    if (box == 0 || Any::Field(box, 0) != 2) {
        return Text::Literal("");
    }
    return Text::Upper(Any::Field(box, 1));
}

int64_t Any::Lower(int64_t box) {
    if (box == 0 || Any::Field(box, 0) != 2) {
        return Text::Literal("");
    }
    return Text::Lower(Any::Field(box, 1));
}

int64_t Float64::ParseAlias(int64_t text) { return Float64::Parse(text); }
int64_t Float64::TextAlias(int64_t bits) { return Float64::Text(bits); }
int64_t Float64::FromIntAlias(int64_t value) { return Float64::FromInt(value); }
int64_t Float64::ToIntAlias(int64_t bits) { return Float64::ToInt(bits); }
int64_t Process::ReadLine(void) { return Process::Input(); }

int64_t Process::Input(void) {
    char *line = NULL;
    size_t capacity = 0;
    ssize_t count = getline(&line, &capacity, stdin);
    if (count < 0) {
        free(line);
        return Text::Literal("");
    }
    while (count > 0 && (line[count - 1] == '\n' || line[count - 1] == '\r')) {
        count = count - 1;
        line[count] = 0;
    }
    return Text::Box(line);
}

int64_t Text::Trim(int64_t text) {
    const char *raw = Text::Characters(text);
    int64_t len = (int64_t)strlen(raw);
    int64_t start = 0;
    while (start < len && (raw[start] == ' ' || raw[start] == '\t' || raw[start] == '\n' || raw[start] == '\r')) {
        start = start + 1;
    }
    int64_t end = len;
    while (end > start && (raw[end - 1] == ' ' || raw[end - 1] == '\t' || raw[end - 1] == '\n' || raw[end - 1] == '\r')) {
        end = end - 1;
    }
    return Text::Box(Text::CopyBytes(raw + start, end - start));
}

int64_t Text::Split(int64_t text, int64_t sep) {
    int64_t list = List::Create();
    const char *raw = Text::Characters(text);
    const char *delim = Text::Characters(sep);
    int64_t rawLength = (int64_t)strlen(raw);
    int64_t delimiterLength = (int64_t)strlen(delim);
    if (delimiterLength == 0) {
        List::Add(list, Text::Box(Text::CopyBytes(raw, rawLength)));
        return list;
    }
    int64_t start = 0;
    while (start <= rawLength) {
        const char *found = strstr(raw + start, delim);
        if (found == NULL) {
            List::Add(list, Text::Box(Text::CopyBytes(raw + start, rawLength - start)));
            break;
        }
        int64_t pos = (int64_t)(found - raw);
        List::Add(list, Text::Box(Text::CopyBytes(raw + start, pos - start)));
        start = pos + delimiterLength;
    }
    return list;
}

void List::Sort(int64_t list) {
    List *ptr = (List *)(uintptr_t)list;
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

void List::Reverse(int64_t list) {
    List *ptr = (List *)(uintptr_t)list;
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

int64_t Math::GreatestCommonDivisor(int64_t a, int64_t b) {
    if (a < 0) { a = -a; }
    if (b < 0) { b = -b; }
    while (b != 0) {
        int64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int64_t Math::IntegerSquareRoot(int64_t n) {
    if (n <= 0) {
        return 0;
    }
    int64_t r = 0;
    while ((r + 1) * (r + 1) <= n) {
        r = r + 1;
    }
    return r;
}

int64_t Files::Name(int64_t path) {
    const char *raw = Text::Characters(path);
    const char *slash = strrchr(raw, '/');
    if (slash == NULL) {
        return Text::Box(Text::CopyBytes(raw, (int64_t)strlen(raw)));
    }
    return Text::Box(Text::CopyBytes(slash + 1, (int64_t)strlen(slash + 1)));
}

int64_t Files::Join(int64_t dir, int64_t name) {
    const char *d = Text::Characters(dir);
    const char *n = Text::Characters(name);
    int64_t dlen = (int64_t)strlen(d);
    int64_t nlen = (int64_t)strlen(n);
    if (dlen == 0) {
        return Text::Box(Text::CopyBytes(n, nlen));
    }
    if (d[dlen - 1] == '/') {
        return Text::Concat(dir, name);
    }
    int64_t slash = Text::Box(Text::CopyBytes("/", 1));
    return Text::Concat(Text::Concat(dir, slash), name);
}

Ownership::Entry* Ownership::entries = nullptr;
int Ownership::entryCapacity = 0;
int Ownership::liveCount = 0;
std::int64_t* Ownership::temporaries = nullptr;
int Ownership::temporaryCount = 0;
int Ownership::temporaryCapacity = 0;
int* Ownership::marks = nullptr;
int Ownership::markCount = 0;
int Ownership::markCapacity = 0;
