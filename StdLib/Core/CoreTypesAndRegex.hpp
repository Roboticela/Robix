#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Robix::StdLib {

// ============================================================================
// 1. BUILT-IN CONSOLE I/O (Print, Input<T>, TryInput<T>)
// ============================================================================
struct InputError {
    std::string expectedType;
    std::string rawInput;
    std::string message;
};

template <typename T>
struct InputResult {
    std::optional<T>          value;
    std::optional<InputError> error;

    [[nodiscard]] bool IsSuccess() const noexcept { return value.has_value() && !error.has_value(); }
    [[nodiscard]] bool IsError() const noexcept { return error.has_value(); }
};

class ConsoleIO {
public:
    static void ClearBuffers() noexcept;
    static void QueueSimulatedInputLine(std::string line);
    static const std::vector<std::string>& OutputLines() noexcept;

    static void Print(std::string_view text);
    static void Print(std::int64_t val);
    static void Print(double val);
    static void Print(bool val);

    // Input() -> String
    static std::string Input(std::string_view prompt = "");

    // TryInput<T>(prompt) -> Result<T, InputError>
    static InputResult<std::int64_t> TryInputInt(std::string_view prompt = "");
    static InputResult<double>       TryInputFloat(std::string_view prompt = "");
    static InputResult<bool>         TryInputBool(std::string_view prompt = "");
    static InputResult<std::string>  TryInputString(std::string_view prompt = "");
};

// ============================================================================
// 2. NUMERIC, STRING & BOOLEAN BUILT-IN METHODS
// ============================================================================
class NumericOps {
public:
    template <typename T>
    static constexpr T            Abs(T v) noexcept { return v < T{ 0 } ? -v : v; }
    static constexpr double       ToFloat(std::int64_t v) noexcept { return static_cast<double>(v); }
    static constexpr std::int64_t ToInt(double v) noexcept { return static_cast<std::int64_t>(v); }
    static std::string            ToString(std::int64_t v) { return std::to_string(v); }
    static std::string            ToString(double v);
    static double                 Round(double v) noexcept { return std::round(v); }
    static double                 Floor(double v) noexcept { return std::floor(v); }
    static double                 Ceiling(double v) noexcept { return std::ceil(v); }
    static double                 Sqrt(double v) noexcept { return std::sqrt(v); }
    static double                 Pow(double base, double exp) noexcept { return std::pow(base, exp); }
    static bool                   IsNaN(double v) noexcept { return std::isnan(v); }
    static bool                   IsInfinite(double v) noexcept { return std::isinf(v); }
};

class StringOps {
public:
    static std::size_t              Length(std::string_view s) noexcept { return s.size(); }
    static bool                     IsEmpty(std::string_view s) noexcept { return s.empty(); }
    static std::string              ToUpper(std::string_view s);
    static std::string              ToLower(std::string_view s);
    static std::string              Trim(std::string_view s);
    static bool                     Contains(std::string_view s, std::string_view sub) noexcept;
    static bool                     StartsWith(std::string_view s, std::string_view prefix) noexcept;
    static bool                     EndsWith(std::string_view s, std::string_view suffix) noexcept;
    static std::string              Replace(std::string_view s, std::string_view from, std::string_view to);
    static std::vector<std::string> Split(std::string_view s, std::string_view delimiter);
    static std::string              Substring(std::string_view s, std::size_t start, std::size_t length);
    static std::int64_t             IndexOf(std::string_view s, std::string_view sub) noexcept;
    static std::optional<std::int64_t> ToInt(std::string_view s) noexcept;
    static std::optional<double>       ToFloat(std::string_view s) noexcept;
};

class BooleanOps {
public:
    static constexpr bool Not(bool b) noexcept { return !b; }
    static constexpr bool And(bool a, bool b) noexcept { return a && b; }
    static constexpr bool Or(bool a, bool b) noexcept { return a || b; }
};

// ============================================================================
// 3. ROBIX.MATH (SCALARS, VECTORS & QUATERNIONS)
// ============================================================================
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vec3 operator+(const Vec3& rhs) const noexcept { return { x + rhs.x, y + rhs.y, z + rhs.z }; }
    constexpr Vec3 operator-(const Vec3& rhs) const noexcept { return { x - rhs.x, y - rhs.y, z - rhs.z }; }
    constexpr Vec3 operator*(double s) const noexcept { return { x * s, y * s, z * s }; }
    constexpr double Dot(const Vec3& rhs) const noexcept { return x * rhs.x + y * rhs.y + z * rhs.z; }
    constexpr Vec3 Cross(const Vec3& rhs) const noexcept {
        return {
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x
        };
    }
    double Magnitude() const noexcept { return std::sqrt(Dot(*this)); }
    Vec3 Normalized() const noexcept {
        const double m = Magnitude();
        return (m > 1e-12) ? (*this * (1.0 / m)) : Vec3{};
    }
};

struct Quaternion {
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    static Quaternion FromAxisAngle(const Vec3& axis, double radians) noexcept;
    Quaternion operator*(const Quaternion& q) const noexcept;
    Quaternion Normalized() const noexcept;
    Vec3 Rotate(const Vec3& v) const noexcept;
};

class MathModule {
public:
    static constexpr double PI = 3.14159265358979323846;

    template <typename T>
    static constexpr T Min(T a, T b) noexcept { return (a < b) ? a : b; }

    template <typename T>
    static constexpr T Max(T a, T b) noexcept { return (a > b) ? a : b; }

    template <typename T>
    static constexpr T Clamp(T val, T lo, T hi) noexcept {
        return (val < lo) ? lo : ((val > hi) ? hi : val);
    }

    static constexpr double ReLU(double x) noexcept { return x > 0.0 ? x : 0.0; }
    static double Sin(double rad) noexcept { return std::sin(rad); }
    static double Cos(double rad) noexcept { return std::cos(rad); }
    static double Tan(double rad) noexcept { return std::tan(rad); }
};

// ============================================================================
// 4. FIRST-CLASS LINEAR-TIME REGEX (Robix.Regex — Thompson NFA, ReDoS-Immune)
// ============================================================================
// Guarantees O(N * M) worst-case execution time with zero backtracking.
class LinearRegex {
public:
    explicit LinearRegex(std::string_view pattern);

    [[nodiscard]] bool IsValid() const noexcept { return m_valid; }
    [[nodiscard]] std::string_view Pattern() const noexcept { return m_pattern; }

    // Full or partial match check in O(N * M) time
    [[nodiscard]] bool Match(std::string_view text) const;
    [[nodiscard]] bool IsFullMatch(std::string_view text) const;
    [[nodiscard]] std::vector<std::string> FindAll(std::string_view text) const;
    [[nodiscard]] std::string Replace(std::string_view text, std::string_view replacement) const;

private:
    enum class AtomKind : std::uint8_t {
        LiteralChar,
        AnyChar,     // .
        Digit,       // \d
        WordChar,    // \w
        Whitespace,  // \s
        CharClass    // [a-z0-9...]
    };

    enum class Quantifier : std::uint8_t {
        ExactlyOne,
        ZeroOrOne,   // ?
        ZeroOrMore,  // *
        OneOrMore    // +
    };

    struct RegexToken {
        AtomKind    kind       = AtomKind::LiteralChar;
        Quantifier  quantifier = Quantifier::ExactlyOne;
        char        literal    = '\0';
        bool        negated    = false;
        std::string charSet;
    };

    std::string m_pattern;
    bool        m_valid       = true;
    bool        m_anchorStart = false;
    bool        m_anchorEnd   = false;
    std::vector<RegexToken> m_tokens;

    static bool MatchesAtom(const RegexToken& tok, char c) noexcept;
    bool MatchAtPosition(std::string_view text, std::size_t startPos, std::size_t& outMatchEnd) const;
};

// ============================================================================
// 5. ROBIX.TIME (DATE, TIME, DATETIME & HIGH-RESOLUTION MONOTONIC CLOCK)
// ============================================================================
struct RobixDateTime {
    int           year       = 2026;
    int           month      = 1;
    int           day        = 1;
    int           hour       = 0;
    int           minute     = 0;
    int           second     = 0;
    std::int64_t  unixTimeSec = 0;

    static RobixDateTime Now() noexcept;
    [[nodiscard]] RobixDateTime AddDays(int days) const noexcept;
    [[nodiscard]] RobixDateTime AddHours(int hours) const noexcept;
    [[nodiscard]] std::int64_t  ToUnixTime() const noexcept { return unixTimeSec; }
    [[nodiscard]] std::string   ToISO8601() const;
};

class HighResClock {
public:
    static std::int64_t NowNanoseconds() noexcept;
    static std::int64_t NowMicroseconds() noexcept;
    static void         SleepMilliseconds(std::uint64_t ms);
};

} // namespace Robix::StdLib

