#pragma once

#include <cstdint>
#include <string>

namespace Robo {

struct Color {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 255;

    static Color FromHex(const std::string& hex);
    static Color FromRgba(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255);

    std::string ToCss() const;
    bool Equals(const Color& other) const;
};

}  // namespace Robo
