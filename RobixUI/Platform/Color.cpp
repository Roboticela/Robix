#include "Color.hpp"

#include <cstdio>

namespace Robo {

namespace {

int ParseHexByte(char high, char low) {
    auto Nibble = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return 10 + (ch - 'a');
        }
        if (ch >= 'A' && ch <= 'F') {
            return 10 + (ch - 'A');
        }
        return 0;
    };
    return (Nibble(high) << 4) | Nibble(low);
}

}  // namespace

Color Color::FromHex(const std::string& hex) {
    Color color;
    std::string value = hex;
    if (!value.empty() && value[0] == '#') {
        value = value.substr(1);
    }

    if (value.size() == 3) {
        color.red = static_cast<std::uint8_t>(ParseHexByte(value[0], value[0]));
        color.green = static_cast<std::uint8_t>(ParseHexByte(value[1], value[1]));
        color.blue = static_cast<std::uint8_t>(ParseHexByte(value[2], value[2]));
        color.alpha = 255;
        return color;
    }

    if (value.size() == 6 || value.size() == 8) {
        color.red = static_cast<std::uint8_t>(ParseHexByte(value[0], value[1]));
        color.green = static_cast<std::uint8_t>(ParseHexByte(value[2], value[3]));
        color.blue = static_cast<std::uint8_t>(ParseHexByte(value[4], value[5]));
        if (value.size() == 8) {
            color.alpha = static_cast<std::uint8_t>(ParseHexByte(value[6], value[7]));
        } else {
            color.alpha = 255;
        }
    }

    return color;
}

Color Color::FromRgba(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
    Color color;
    color.red = red;
    color.green = green;
    color.blue = blue;
    color.alpha = alpha;
    return color;
}

std::string Color::ToCss() const {
    if (alpha == 255) {
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", red, green, blue);
        return buffer;
    }

    char buffer[64];
    const double alphaValue = static_cast<double>(alpha) / 255.0;
    std::snprintf(buffer, sizeof(buffer), "rgba(%u,%u,%u,%.3f)", red, green, blue, alphaValue);
    return buffer;
}

bool Color::Equals(const Color& other) const {
    return red == other.red && green == other.green && blue == other.blue && alpha == other.alpha;
}

}  // namespace Robo
