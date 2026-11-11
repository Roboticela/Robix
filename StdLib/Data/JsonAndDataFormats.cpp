#include "JSONAndDataFormats.hpp"
#include "../Core/CoreTypesAndRegex.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace Robix::StdLib {

namespace {

class JSONReader {
public:
    explicit JSONReader(std::string_view src) noexcept : m_src(src) {}

    bool ParseRoot(JSONValue& out, std::string& err) {
        SkipWS();
        if (!ParseElement(out, err)) return false;
        SkipWS();
        if (m_pos != m_src.size()) {
            err = "Unexpected trailing characters at end of JSON input.";
            return false;
        }
        return true;
    }

private:
    std::string_view m_src;
    std::size_t      m_pos = 0;

    void SkipWS() noexcept {
        while (m_pos < m_src.size() && std::isspace(static_cast<unsigned char>(m_src[m_pos]))) {
            ++m_pos;
        }
    }

    bool ParseElement(JSONValue& out, std::string& err) {
        SkipWS();
        if (m_pos >= m_src.size()) {
            err = "Unexpected end of JSON input.";
            return false;
        }
        const char c = m_src[m_pos];
        if (c == '{') return ParseObject(out, err);
        if (c == '[') return ParseArray(out, err);
        if (c == '"') {
            std::string s;
            if (!ParseString(s, err)) return false;
            out = JSONValue::MakeString(std::move(s));
            return true;
        }
        if (c == 't' && m_src.substr(m_pos, 4) == "true") {
            m_pos += 4;
            out = JSONValue::MakeBool(true);
            return true;
        }
        if (c == 'f' && m_src.substr(m_pos, 5) == "false") {
            m_pos += 5;
            out = JSONValue::MakeBool(false);
            return true;
        }
        if (c == 'n' && m_src.substr(m_pos, 4) == "null") {
            m_pos += 4;
            out = JSONValue::MakeNull();
            return true;
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            return ParseNumber(out, err);
        }
        err = std::string("Invalid JSON token starting with '") + c + "'.";
        return false;
    }

    bool ParseString(std::string& out, std::string& err) {
        if (m_pos >= m_src.size() || m_src[m_pos] != '"') {
            err = "Expected '\"' to start JSON string.";
            return false;
        }
        ++m_pos;
        out.clear();
        while (m_pos < m_src.size()) {
            const char c = m_src[m_pos++];
            if (c == '"') return true;
            if (c == '\\') {
                if (m_pos >= m_src.size()) {
                    err = "Unterminated escape sequence in JSON string.";
                    return false;
                }
                const char esc = m_src[m_pos++];
                switch (esc) {
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    default:   out.push_back(esc); break;
                }
            } else {
                out.push_back(c);
            }
        }
        err = "Unterminated JSON string literal.";
        return false;
    }

    bool ParseNumber(JSONValue& out, std::string& err) {
        const std::size_t start = m_pos;
        bool isFloat = false;
        if (m_src[m_pos] == '-') ++m_pos;
        while (m_pos < m_src.size() && std::isdigit(static_cast<unsigned char>(m_src[m_pos]))) {
            ++m_pos;
        }
        if (m_pos < m_src.size() && m_src[m_pos] == '.') {
            isFloat = true;
            ++m_pos;
            while (m_pos < m_src.size() && std::isdigit(static_cast<unsigned char>(m_src[m_pos]))) {
                ++m_pos;
            }
        }
        const std::string numStr(m_src.substr(start, m_pos - start));
        if (isFloat) {
            if (auto f = StringOps::ToFloat(numStr)) {
                out = JSONValue::MakeFloat(*f);
                return true;
            }
        } else {
            if (auto i = StringOps::ToInt(numStr)) {
                out = JSONValue::MakeInt(*i);
                return true;
            }
        }
        err = "Invalid numeric literal '" + numStr + "' in JSON.";
        return false;
    }

    bool ParseArray(JSONValue& out, std::string& err) {
        ++m_pos; // '['
        out = JSONValue::MakeArray();
        SkipWS();
        if (m_pos < m_src.size() && m_src[m_pos] == ']') {
            ++m_pos;
            return true;
        }
        while (true) {
            JSONValue elem;
            if (!ParseElement(elem, err)) return false;
            out.arrayVal.push_back(std::move(elem));
            SkipWS();
            if (m_pos < m_src.size() && m_src[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_pos < m_src.size() && m_src[m_pos] == ']') {
                ++m_pos;
                return true;
            }
            err = "Expected ',' or ']' in JSON array.";
            return false;
        }
    }

    bool ParseObject(JSONValue& out, std::string& err) {
        ++m_pos; // '{'
        out = JSONValue::MakeObject();
        SkipWS();
        if (m_pos < m_src.size() && m_src[m_pos] == '}') {
            ++m_pos;
            return true;
        }
        while (true) {
            SkipWS();
            std::string key;
            if (!ParseString(key, err)) return false;
            SkipWS();
            if (m_pos >= m_src.size() || m_src[m_pos] != ':') {
                err = "Expected ':' after JSON object key '" + key + "'.";
                return false;
            }
            ++m_pos;
            JSONValue val;
            if (!ParseElement(val, err)) return false;
            out.objectVal[std::move(key)] = std::move(val);
            SkipWS();
            if (m_pos < m_src.size() && m_src[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_pos < m_src.size() && m_src[m_pos] == '}') {
                ++m_pos;
                return true;
            }
            err = "Expected ',' or '}' in JSON object.";
            return false;
        }
    }
};

} // namespace

// ============================================================================
// 1. JSON CODEC & SCHEMA IMPLEMENTATION
// ============================================================================

std::optional<JSONValue> JSONCodec::TryParse(std::string_view jsonText, std::string* outError) {
    JSONReader reader(jsonText);
    JSONValue val;
    std::string err;
    if (!reader.ParseRoot(val, err)) {
        if (outError) *outError = err;
        return std::nullopt;
    }
    return val;
}

JSONValue JSONCodec::Parse(std::string_view jsonText) {
    std::string err;
    if (auto res = TryParse(jsonText, &err)) {
        return *res;
    }
    throw std::runtime_error("Robix.JSON ParseError: " + err);
}

std::string JSONCodec::Serialize(const JSONValue& val, JSONFormatOption fmt, int indentLevel) {
    const std::string indent(fmt == JSONFormatOption::Pretty ? static_cast<std::size_t>(indentLevel * 2) : 0, ' ');
    const std::string childIndent(fmt == JSONFormatOption::Pretty ? static_cast<std::size_t>((indentLevel + 1) * 2) : 0, ' ');
    const std::string nl = (fmt == JSONFormatOption::Pretty) ? "\n" : "";
    const std::string sp = (fmt == JSONFormatOption::Pretty) ? " " : "";

    switch (val.kind) {
        case JSONKind::Null:
            return "null";
        case JSONKind::Bool:
            return val.boolVal ? "true" : "false";
        case JSONKind::Int:
            return std::to_string(val.intVal);
        case JSONKind::Float:
            return NumericOps::ToString(val.floatVal);
        case JSONKind::String: {
            std::string escaped = "\"";
            escaped.reserve(val.strVal.size() + 2);
            for (char ch : val.strVal) {
                switch (ch) {
                    case '"':  escaped += "\\\""; break;
                    case '\\': escaped += "\\\\"; break;
                    case '\n': escaped += "\\n"; break;
                    case '\r': escaped += "\\r"; break;
                    case '\t': escaped += "\\t"; break;
                    default:   escaped += ch; break;
                }
            }
            escaped += "\"";
            return escaped;
        }
        case JSONKind::Array: {
            if (val.arrayVal.empty()) return "[]";
            std::string out = "[" + nl;
            for (std::size_t i = 0; i < val.arrayVal.size(); ++i) {
                if (fmt == JSONFormatOption::Pretty) out += childIndent;
                out += Serialize(val.arrayVal[i], fmt, indentLevel + 1);
                if (i + 1 < val.arrayVal.size()) out += ",";
                out += nl;
            }
            if (fmt == JSONFormatOption::Pretty) out += indent;
            out += "]";
            return out;
        }
        case JSONKind::Object: {
            if (val.objectVal.empty()) return "{}";
            std::string out = "{" + nl;
            std::size_t idx = 0;
            for (const auto& [k, v] : val.objectVal) {
                if (fmt == JSONFormatOption::Pretty) out += childIndent;
                out += "\"" + k + "\":" + sp + Serialize(v, fmt, indentLevel + 1);
                if (++idx < val.objectVal.size()) out += ",";
                out += nl;
            }
            if (fmt == JSONFormatOption::Pretty) out += indent;
            out += "}";
            return out;
        }
    }
    return "null";
}

std::vector<JSONValue> JSONCodec::ParseJSONLines(std::string_view jsonlText) {
    std::vector<JSONValue> docs;
    for (const auto& line : StringOps::Split(jsonlText, "\n")) {
        const std::string trimmed = StringOps::Trim(line);
        if (!trimmed.empty()) {
            docs.push_back(Parse(trimmed));
        }
    }
    return docs;
}

bool JSONSchema::Validate(const JSONValue& doc, std::string& outError) const {
    if (doc.kind != JSONKind::Object) {
        outError = "Expected root JSON value to be an Object.";
        return false;
    }
    for (const auto& req : requiredFields) {
        if (!doc.HasField(req)) {
            outError = "Missing required JSON field '" + req + "'.";
            return false;
        }
    }
    for (const auto& [field, expectedKind] : fieldTypes) {
        if (const JSONValue* actual = doc.GetField(field)) {
            if (actual->kind != expectedKind) {
                outError = "JSON field '" + field + "' has mismatched type.";
                return false;
            }
        }
    }
    return true;
}

// ============================================================================
// 2. CSV, TOML/YAML, BINARY & COMPRESSION IMPLEMENTATION
// ============================================================================

std::vector<std::vector<std::string>> CsvCodec::Parse(std::string_view csvText) {
    std::vector<std::vector<std::string>> rows;
    for (const auto& line : StringOps::Split(csvText, "\n")) {
        const std::string trimmed = StringOps::Trim(line);
        if (trimmed.empty()) continue;
        std::vector<std::string> cols;
        for (const auto& cell : StringOps::Split(trimmed, ",")) {
            cols.push_back(StringOps::Trim(cell));
        }
        rows.push_back(std::move(cols));
    }
    return rows;
}

std::string CsvCodec::Serialize(const std::vector<std::vector<std::string>>& rows) {
    std::ostringstream oss;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        for (std::size_t c = 0; c < rows[r].size(); ++c) {
            if (c > 0) oss << ",";
            oss << rows[r][c];
        }
        if (r + 1 < rows.size()) oss << "\n";
    }
    return oss.str();
}

std::map<std::string, std::string> KeyValueFormatCodec::Parse(std::string_view text) {
    std::map<std::string, std::string> result;
    for (const auto& line : StringOps::Split(text, "\n")) {
        const std::string trimmed = StringOps::Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        std::size_t sep = trimmed.find('=');
        if (sep == std::string::npos) {
            sep = trimmed.find(':');
        }
        if (sep != std::string::npos) {
            std::string k = StringOps::Trim(trimmed.substr(0, sep));
            std::string v = StringOps::Trim(trimmed.substr(sep + 1));
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
                v = v.substr(1, v.size() - 2);
            }
            result[std::move(k)] = std::move(v);
        }
    }
    return result;
}

std::string KeyValueFormatCodec::SerializeToml(const std::map<std::string, std::string>& entries) {
    std::ostringstream oss;
    for (const auto& [k, v] : entries) {
        oss << k << " = \"" << v << "\"\n";
    }
    return oss.str();
}

std::vector<std::uint8_t> BinarySerializer::EncodeInt64AndString(std::int64_t id, std::string_view payload) {
    std::vector<std::uint8_t> buf;
    const auto uId = static_cast<std::uint64_t>(id);
    const auto len = static_cast<std::uint32_t>(payload.size());
    buf.reserve(8 + 4 + payload.size());

    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<std::uint8_t>((uId >> (i * 8)) & 0xFFU));
    }
    for (int i = 0; i < 4; ++i) {
        buf.push_back(static_cast<std::uint8_t>((len >> (i * 8)) & 0xFFU));
    }
    for (char c : payload) {
        buf.push_back(static_cast<std::uint8_t>(c));
    }
    return buf;
}

bool BinarySerializer::DecodeInt64AndString(const std::vector<std::uint8_t>& bytes,
                                            std::int64_t& outId,
                                            std::string& outPayload) {
    if (bytes.size() < 12) return false;
    std::uint64_t uId = 0;
    for (int i = 0; i < 8; ++i) {
        uId |= (static_cast<std::uint64_t>(bytes[static_cast<std::size_t>(i)]) << (i * 8));
    }
    std::uint32_t len = 0;
    for (int i = 0; i < 4; ++i) {
        len |= (static_cast<std::uint32_t>(bytes[static_cast<std::size_t>(8 + i)]) << (i * 8));
    }
    if (bytes.size() != 12 + static_cast<std::size_t>(len)) return false;

    outId = static_cast<std::int64_t>(uId);
    outPayload.assign(reinterpret_cast<const char*>(bytes.data() + 12), len);
    return true;
}

std::vector<std::uint8_t> LosslessCompressor::CompressRLE(const std::vector<std::uint8_t>& input) {
    std::vector<std::uint8_t> out;
    std::size_t i = 0;
    while (i < input.size()) {
        const std::uint8_t byteVal = input[i];
        std::uint8_t runLen        = 1;
        while ((i + runLen) < input.size() && input[i + runLen] == byteVal && runLen < 255) {
            ++runLen;
        }
        out.push_back(runLen);
        out.push_back(byteVal);
        i += runLen;
    }
    return out;
}

std::vector<std::uint8_t> LosslessCompressor::DecompressRLE(const std::vector<std::uint8_t>& compressed) {
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i + 1 < compressed.size(); i += 2) {
        const std::uint8_t count   = compressed[i];
        const std::uint8_t byteVal = compressed[i + 1];
        out.insert(out.end(), count, byteVal);
    }
    return out;
}

// ============================================================================
// 3. ROBIX.DATABASE & ROBIX.IO IMPLEMENTATION
// ============================================================================

void EmbeddedDatabase::CreateTable(const std::string& tableName) {
    m_tables.try_emplace(tableName);
}

bool EmbeddedDatabase::InsertRow(const std::string& tableName, Row row) {
    auto it = m_tables.find(tableName);
    if (it == m_tables.end()) return false;
    it->second.push_back(std::move(row));
    return true;
}

std::vector<EmbeddedDatabase::Row> EmbeddedDatabase::SelectWhereEquals(
    const std::string& tableName,
    const std::string& column,
    const std::string& expectedValue) const {

    std::vector<Row> matches;
    auto it = m_tables.find(tableName);
    if (it == m_tables.end()) return matches;
    for (const auto& r : it->second) {
        auto cIt = r.find(column);
        if (cIt != r.end() && cIt->second == expectedValue) {
            matches.push_back(r);
        }
    }
    return matches;
}

std::size_t EmbeddedDatabase::RowCount(const std::string& tableName) const noexcept {
    auto it = m_tables.find(tableName);
    return (it != m_tables.end()) ? it->second.size() : 0;
}

bool CrossPlatformIO::WriteAllText(const std::string& path, std::string_view content) {
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    return ofs.good();
}

std::optional<std::string> CrossPlatformIO::ReadAllText(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return std::nullopt;
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

bool CrossPlatformIO::Exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool CrossPlatformIO::RemoveFile(const std::string& path) {
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

} // namespace Robix::StdLib

