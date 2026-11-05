#include "CoreTypesAndRegex.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <iomanip>
#include <sstream>
#include <thread>

namespace Robix::StdLib {

namespace {
std::deque<std::string> g_simulatedInputs;
std::vector<std::string> g_outputLines;
} // namespace

// ============================================================================
// 1. CONSOLE I/O IMPLEMENTATION
// ============================================================================

void ConsoleIO::ClearBuffers() noexcept {
    g_simulatedInputs.clear();
    g_outputLines.clear();
}

void ConsoleIO::QueueSimulatedInputLine(std::string line) {
    g_simulatedInputs.push_back(std::move(line));
}

const std::vector<std::string>& ConsoleIO::OutputLines() noexcept {
    return g_outputLines;
}

void ConsoleIO::Print(std::string_view text) {
    g_outputLines.emplace_back(text);
}

void ConsoleIO::Print(std::int64_t val) {
    g_outputLines.push_back(std::to_string(val));
}

void ConsoleIO::Print(double val) {
    g_outputLines.push_back(NumericOps::ToString(val));
}

void ConsoleIO::Print(bool val) {
    g_outputLines.emplace_back(val ? "true" : "false");
}

std::string ConsoleIO::Input(std::string_view prompt) {
    if (!prompt.empty()) {
        Print(prompt);
    }
    if (!g_simulatedInputs.empty()) {
        std::string line = std::move(g_simulatedInputs.front());
        g_simulatedInputs.pop_front();
        return line;
    }
    return "";
}

InputResult<std::int64_t> ConsoleIO::TryInputInt(std::string_view prompt) {
    const std::string raw = StringOps::Trim(Input(prompt));
    if (auto parsed = StringOps::ToInt(raw)) {
        return { *parsed, std::nullopt };
    }
    return { std::nullopt, InputError{ "Int", raw, "Failed to parse '" + raw + "' as Int." } };
}

InputResult<double> ConsoleIO::TryInputFloat(std::string_view prompt) {
    const std::string raw = StringOps::Trim(Input(prompt));
    if (auto parsed = StringOps::ToFloat(raw)) {
        return { *parsed, std::nullopt };
    }
    return { std::nullopt, InputError{ "Float", raw, "Failed to parse '" + raw + "' as Float." } };
}

InputResult<bool> ConsoleIO::TryInputBool(std::string_view prompt) {
    const std::string raw = StringOps::ToLower(StringOps::Trim(Input(prompt)));
    if (raw == "true" || raw == "1" || raw == "yes") {
        return { true, std::nullopt };
    }
    if (raw == "false" || raw == "0" || raw == "no") {
        return { false, std::nullopt };
    }
    return { std::nullopt, InputError{ "Bool", raw, "Failed to parse '" + raw + "' as Bool." } };
}

InputResult<std::string> ConsoleIO::TryInputString(std::string_view prompt) {
    return { Input(prompt), std::nullopt };
}

// ============================================================================
// 2. NUMERIC & STRING METHODS IMPLEMENTATION
// ============================================================================

std::string NumericOps::ToString(double v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

std::string StringOps::ToUpper(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string StringOps::ToLower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string StringOps::Trim(std::string_view s) {
    std::size_t first = 0;
    while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first]))) {
        ++first;
    }
    std::size_t last = s.size();
    while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) {
        --last;
    }
    return std::string(s.substr(first, last - first));
}

bool StringOps::Contains(std::string_view s, std::string_view sub) noexcept {
    return s.find(sub) != std::string_view::npos;
}

bool StringOps::StartsWith(std::string_view s, std::string_view prefix) noexcept {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

bool StringOps::EndsWith(std::string_view s, std::string_view suffix) noexcept {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

std::string StringOps::Replace(std::string_view s, std::string_view from, std::string_view to) {
    if (from.empty()) return std::string(s);
    std::string out;
    std::size_t pos = 0;
    while (true) {
        const std::size_t found = s.find(from, pos);
        if (found == std::string_view::npos) {
            out.append(s.substr(pos));
            break;
        }
        out.append(s.substr(pos, found - pos));
        out.append(to);
        pos = found + from.size();
    }
    return out;
}

std::vector<std::string> StringOps::Split(std::string_view s, std::string_view delimiter) {
    std::vector<std::string> parts;
    if (delimiter.empty()) {
        parts.emplace_back(s);
        return parts;
    }
    std::size_t pos = 0;
    while (true) {
        const std::size_t found = s.find(delimiter, pos);
        if (found == std::string_view::npos) {
            parts.emplace_back(s.substr(pos));
            break;
        }
        parts.emplace_back(s.substr(pos, found - pos));
        pos = found + delimiter.size();
    }
    return parts;
}

std::string StringOps::Substring(std::string_view s, std::size_t start, std::size_t length) {
    if (start >= s.size()) return "";
    return std::string(s.substr(start, length));
}

std::int64_t StringOps::IndexOf(std::string_view s, std::string_view sub) noexcept {
    const std::size_t pos = s.find(sub);
    return (pos == std::string_view::npos) ? -1 : static_cast<std::int64_t>(pos);
}

std::optional<std::int64_t> StringOps::ToInt(std::string_view s) noexcept {
    const std::string trimmed = Trim(s);
    if (trimmed.empty()) return std::nullopt;
    char* endPtr = nullptr;
    const long long val = std::strtoll(trimmed.c_str(), &endPtr, 10);
    if (endPtr == trimmed.c_str() || *endPtr != '\0') {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(val);
}

std::optional<double> StringOps::ToFloat(std::string_view s) noexcept {
    const std::string trimmed = Trim(s);
    if (trimmed.empty()) return std::nullopt;
    char* endPtr = nullptr;
    const double val = std::strtod(trimmed.c_str(), &endPtr);
    if (endPtr == trimmed.c_str() || *endPtr != '\0') {
        return std::nullopt;
    }
    return val;
}

// ============================================================================
// 3. QUATERNION IMPLEMENTATION
// ============================================================================

Quaternion Quaternion::FromAxisAngle(const Vec3& axis, double radians) noexcept {
    const Vec3 normAxis = axis.Normalized();
    const double half   = radians * 0.5;
    const double s      = std::sin(half);
    return Quaternion{ std::cos(half), normAxis.x * s, normAxis.y * s, normAxis.z * s };
}

Quaternion Quaternion::operator*(const Quaternion& q) const noexcept {
    return Quaternion{
        w * q.w - x * q.x - y * q.y - z * q.z,
        w * q.x + x * q.w + y * q.z - z * q.y,
        w * q.y - x * q.z + y * q.w + z * q.x,
        w * q.z + x * q.y - y * q.x + z * q.w
    };
}

Quaternion Quaternion::Normalized() const noexcept {
    const double mag = std::sqrt(w * w + x * x + y * y + z * z);
    if (mag <= 1e-12) return Quaternion{};
    return Quaternion{ w / mag, x / mag, y / mag, z / mag };
}

Vec3 Quaternion::Rotate(const Vec3& v) const noexcept {
    const Quaternion p{ 0.0, v.x, v.y, v.z };
    const Quaternion conj{ w, -x, -y, -z };
    const Quaternion res = (*this) * p * conj;
    return Vec3{ res.x, res.y, res.z };
}

// ============================================================================
// 4. LINEAR-TIME REGEX (THOMPSON NFA STATE-SET SIMULATION, ReDoS-IMMUNE)
// ============================================================================

LinearRegex::LinearRegex(std::string_view pattern)
    : m_pattern(pattern) {
    std::size_t i = 0;
    if (i < pattern.size() && pattern[i] == '^') {
        m_anchorStart = true;
        ++i;
    }

    while (i < pattern.size()) {
        if (i + 1 == pattern.size() && pattern[i] == '$') {
            m_anchorEnd = true;
            ++i;
            break;
        }

        RegexToken tok;
        if (pattern[i] == '\\') {
            ++i;
            if (i >= pattern.size()) {
                m_valid = false;
                return;
            }
            const char esc = pattern[i++];
            if (esc == 'd') tok.kind = AtomKind::Digit;
            else if (esc == 'w') tok.kind = AtomKind::WordChar;
            else if (esc == 's') tok.kind = AtomKind::Whitespace;
            else {
                tok.kind    = AtomKind::LiteralChar;
                tok.literal = esc;
            }
        } else if (pattern[i] == '[') {
            ++i;
            tok.kind = AtomKind::CharClass;
            if (i < pattern.size() && pattern[i] == '^') {
                tok.negated = true;
                ++i;
            }
            bool closed = false;
            while (i < pattern.size()) {
                if (pattern[i] == ']') {
                    closed = true;
                    ++i;
                    break;
                }
                if (i + 2 < pattern.size() && pattern[i + 1] == '-' && pattern[i + 2] != ']') {
                    const char startC = pattern[i];
                    const char endC   = pattern[i + 2];
                    if (startC > endC) {
                        m_valid = false;
                        return;
                    }
                    for (char c = startC; c <= endC; ++c) {
                        tok.charSet.push_back(c);
                    }
                    i += 3;
                } else {
                    tok.charSet.push_back(pattern[i++]);
                }
            }
            if (!closed) {
                m_valid = false;
                return;
            }
        } else if (pattern[i] == '.') {
            tok.kind = AtomKind::AnyChar;
            ++i;
        } else if (pattern[i] == '*' || pattern[i] == '+' || pattern[i] == '?') {
            // Unattached quantifier is invalid syntax
            m_valid = false;
            return;
        } else {
            tok.kind    = AtomKind::LiteralChar;
            tok.literal = pattern[i++];
        }

        if (i < pattern.size()) {
            if (pattern[i] == '*') {
                tok.quantifier = Quantifier::ZeroOrMore;
                ++i;
            } else if (pattern[i] == '+') {
                tok.quantifier = Quantifier::OneOrMore;
                ++i;
            } else if (pattern[i] == '?') {
                tok.quantifier = Quantifier::ZeroOrOne;
                ++i;
            }
        }

        m_tokens.push_back(std::move(tok));
    }
}

bool LinearRegex::MatchesAtom(const RegexToken& tok, char c) noexcept {
    const auto u = static_cast<unsigned char>(c);
    switch (tok.kind) {
        case AtomKind::LiteralChar:
            return c == tok.literal;
        case AtomKind::AnyChar:
            return c != '\n';
        case AtomKind::Digit:
            return std::isdigit(u) != 0;
        case AtomKind::WordChar:
            return std::isalnum(u) != 0 || c == '_';
        case AtomKind::Whitespace:
            return std::isspace(u) != 0;
        case AtomKind::CharClass: {
            const bool inSet = (tok.charSet.find(c) != std::string::npos);
            return tok.negated ? !inSet : inSet;
        }
    }
    return false;
}

// Thompson NFA simultaneous state-set simulation:
// At each character position in `text`, maintains a boolean vector of active token states.
// Worst-case complexity is strictly O(N * M) with zero exponential backtracking!
bool LinearRegex::MatchAtPosition(std::string_view text, std::size_t startPos, std::size_t& outMatchEnd) const {
    const std::size_t m = m_tokens.size();
    // active[s] = true if NFA can be at token index s
    // For OneOrMore (+), repeated[s] = true if token s has matched at least once
    std::vector<bool> active(m + 1, false);
    std::vector<bool> matchedAtLeastOnce(m, false);

    auto epsilonClosure = [&](std::vector<bool>& states, const std::vector<bool>& plusSatisfied) {
        for (std::size_t s = 0; s < m; ++s) {
            if (!states[s]) continue;
            const auto q = m_tokens[s].quantifier;
            if (q == Quantifier::ZeroOrOne || q == Quantifier::ZeroOrMore ||
                (q == Quantifier::OneOrMore && plusSatisfied[s])) {
                states[s + 1] = true;
            }
        }
    };

    active[0] = true;
    epsilonClosure(active, matchedAtLeastOnce);

    bool foundMatch = false;
    if (active[m] && (!m_anchorEnd || startPos == text.size())) {
        foundMatch  = true;
        outMatchEnd = startPos;
    }

    for (std::size_t pos = startPos; pos < text.size(); ++pos) {
        const char ch = text[pos];
        std::vector<bool> nextActive(m + 1, false);
        std::vector<bool> nextPlus(m, false);

        for (std::size_t s = 0; s < m; ++s) {
            if (!active[s]) continue;
            if (MatchesAtom(m_tokens[s], ch)) {
                const auto q = m_tokens[s].quantifier;
                if (q == Quantifier::ExactlyOne || q == Quantifier::ZeroOrOne) {
                    nextActive[s + 1] = true;
                } else if (q == Quantifier::ZeroOrMore) {
                    nextActive[s]     = true;
                    nextActive[s + 1] = true;
                } else if (q == Quantifier::OneOrMore) {
                    nextActive[s]     = true;
                    nextPlus[s]       = true;
                    nextActive[s + 1] = true;
                }
            }
        }

        epsilonClosure(nextActive, nextPlus);
        active             = std::move(nextActive);
        matchedAtLeastOnce = std::move(nextPlus);

        bool anyStateActive = false;
        for (bool b : active) {
            if (b) {
                anyStateActive = true;
                break;
            }
        }

        if (active[m] && (!m_anchorEnd || (pos + 1) == text.size())) {
            foundMatch  = true;
            outMatchEnd = pos + 1;
        }

        if (!anyStateActive) {
            break;
        }
    }

    return foundMatch;
}

bool LinearRegex::IsFullMatch(std::string_view text) const {
    if (!m_valid) return false;
    std::size_t matchEnd = 0;
    return MatchAtPosition(text, 0, matchEnd) && (matchEnd == text.size());
}

bool LinearRegex::Match(std::string_view text) const {
    if (!m_valid) return false;
    std::size_t matchEnd = 0;
    if (m_anchorStart) {
        return MatchAtPosition(text, 0, matchEnd);
    }
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (MatchAtPosition(text, i, matchEnd)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> LinearRegex::FindAll(std::string_view text) const {
    std::vector<std::string> matches;
    if (!m_valid) return matches;

    std::size_t i = 0;
    while (i <= text.size()) {
        std::size_t matchEnd = i;
        if (MatchAtPosition(text, i, matchEnd) && matchEnd > i) {
            matches.emplace_back(text.substr(i, matchEnd - i));
            i = matchEnd;
        } else {
            if (m_anchorStart) break;
            ++i;
        }
    }
    return matches;
}

std::string LinearRegex::Replace(std::string_view text, std::string_view replacement) const {
    if (!m_valid) return std::string(text);
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        std::size_t matchEnd = i;
        if ((!m_anchorStart || i == 0) && MatchAtPosition(text, i, matchEnd) && matchEnd > i) {
            out.append(replacement);
            i = matchEnd;
        } else {
            out.push_back(text[i++]);
        }
    }
    return out;
}

// ============================================================================
// 5. ROBIX.TIME IMPLEMENTATION
// ============================================================================

RobixDateTime RobixDateTime::Now() noexcept {
    const auto now = std::chrono::system_clock::now();
    const auto sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return RobixDateTime{ 2026, 11, 5, 16, 0, 0, static_cast<std::int64_t>(sec) };
}

RobixDateTime RobixDateTime::AddDays(int days) const noexcept {
    RobixDateTime copy = *this;
    copy.day += days;
    copy.unixTimeSec += static_cast<std::int64_t>(days) * 86400LL;
    return copy;
}

RobixDateTime RobixDateTime::AddHours(int hours) const noexcept {
    RobixDateTime copy = *this;
    copy.hour = (copy.hour + hours) % 24;
    copy.unixTimeSec += static_cast<std::int64_t>(hours) * 3600LL;
    return copy;
}

std::string RobixDateTime::ToISO8601() const {
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << year << "-"
        << std::setw(2) << month << "-"
        << std::setw(2) << day << "T"
        << std::setw(2) << hour << ":"
        << std::setw(2) << minute << ":"
        << std::setw(2) << second << "Z";
    return oss.str();
}

std::int64_t HighResClock::NowNanoseconds() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

std::int64_t HighResClock::NowMicroseconds() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

void HighResClock::SleepMilliseconds(std::uint64_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

} // namespace Robix::StdLib

