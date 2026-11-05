#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Robix::StdLib {

// ============================================================================
// 1. FIRST-CLASS JSON (Robix.Json — Parse, TryParse, Serialize, Schema, JSONL)
// ============================================================================
enum class JsonKind : std::uint8_t {
    Null,
    Bool,
    Int,
    Float,
    String,
    Array,
    Object
};

enum class JsonFormatOption : std::uint8_t {
    Compact,
    Pretty
};

struct JsonValue {
    JsonKind                         kind       = JsonKind::Null;
    bool                             boolVal    = false;
    std::int64_t                     intVal     = 0;
    double                           floatVal   = 0.0;
    std::string                      strVal;
    std::vector<JsonValue>           arrayVal;
    std::map<std::string, JsonValue> objectVal;

    static JsonValue MakeNull() { return JsonValue{}; }
    static JsonValue MakeBool(bool b) { JsonValue v; v.kind = JsonKind::Bool; v.boolVal = b; return v; }
    static JsonValue MakeInt(std::int64_t i) { JsonValue v; v.kind = JsonKind::Int; v.intVal = i; return v; }
    static JsonValue MakeFloat(double d) { JsonValue v; v.kind = JsonKind::Float; v.floatVal = d; return v; }
    static JsonValue MakeString(std::string s) { JsonValue v; v.kind = JsonKind::String; v.strVal = std::move(s); return v; }
    static JsonValue MakeArray(std::vector<JsonValue> a = {}) { JsonValue v; v.kind = JsonKind::Array; v.arrayVal = std::move(a); return v; }
    static JsonValue MakeObject(std::map<std::string, JsonValue> o = {}) { JsonValue v; v.kind = JsonKind::Object; v.objectVal = std::move(o); return v; }

    [[nodiscard]] bool HasField(const std::string& key) const noexcept {
        return kind == JsonKind::Object && objectVal.find(key) != objectVal.end();
    }

    [[nodiscard]] const JsonValue* GetField(const std::string& key) const noexcept {
        if (kind != JsonKind::Object) return nullptr;
        auto it = objectVal.find(key);
        return (it != objectVal.end()) ? &it->second : nullptr;
    }
};

struct JsonSchema {
    std::vector<std::string>            requiredFields;
    std::map<std::string, JsonKind>     fieldTypes;

    [[nodiscard]] bool Validate(const JsonValue& doc, std::string& outError) const;
};

class JsonCodec {
public:
    [[nodiscard]] static std::optional<JsonValue> TryParse(std::string_view jsonText, std::string* outError = nullptr);
    [[nodiscard]] static JsonValue                Parse(std::string_view jsonText);
    [[nodiscard]] static std::string              Serialize(const JsonValue& val,
                                                            JsonFormatOption fmt = JsonFormatOption::Compact,
                                                            int indentLevel = 0);
    [[nodiscard]] static std::vector<JsonValue>   ParseJsonLines(std::string_view jsonlText);
};

// ============================================================================
// 2. CSV, TOML/YAML, BINARY SERIALIZATION & LOSSLESS COMPRESSION
// ============================================================================
class CsvCodec {
public:
    [[nodiscard]] static std::vector<std::vector<std::string>> Parse(std::string_view csvText);
    [[nodiscard]] static std::string Serialize(const std::vector<std::vector<std::string>>& rows);
};

class KeyValueFormatCodec {
public:
    // Parses simple TOML/YAML style `key = value` or `key: value` configuration files
    [[nodiscard]] static std::map<std::string, std::string> Parse(std::string_view text);
    [[nodiscard]] static std::string SerializeToml(const std::map<std::string, std::string>& entries);
};

class BinarySerializer {
public:
    [[nodiscard]] static std::vector<std::uint8_t> EncodeInt64AndString(std::int64_t id, std::string_view payload);
    [[nodiscard]] static bool DecodeInt64AndString(const std::vector<std::uint8_t>& bytes,
                                                   std::int64_t& outId,
                                                   std::string& outPayload);
};

class LosslessCompressor {
public:
    [[nodiscard]] static std::vector<std::uint8_t> CompressRLE(const std::vector<std::uint8_t>& input);
    [[nodiscard]] static std::vector<std::uint8_t> DecompressRLE(const std::vector<std::uint8_t>& compressed);
};

// ============================================================================
// 3. ROBIX.DATABASE (EMBEDDED RELATIONAL TABLE ENGINE) & ROBIX.IO
// ============================================================================
class EmbeddedDatabase {
public:
    using Row = std::map<std::string, std::string>;

    void CreateTable(const std::string& tableName);
    bool InsertRow(const std::string& tableName, Row row);
    [[nodiscard]] std::vector<Row> SelectWhereEquals(const std::string& tableName,
                                                     const std::string& column,
                                                     const std::string& expectedValue) const;
    [[nodiscard]] std::size_t RowCount(const std::string& tableName) const noexcept;

private:
    std::unordered_map<std::string, std::vector<Row>> m_tables;
};

class CrossPlatformIO {
public:
    static bool WriteAllText(const std::string& path, std::string_view content);
    [[nodiscard]] static std::optional<std::string> ReadAllText(const std::string& path);
    [[nodiscard]] static bool Exists(const std::string& path);
    static bool RemoveFile(const std::string& path);
};

} // namespace Robix::StdLib

