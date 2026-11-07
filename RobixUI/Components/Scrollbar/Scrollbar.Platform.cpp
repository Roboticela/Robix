#include "Scrollbar.Platform.hpp"

#include "../../Foundation/FoundationPlatform.hpp"

namespace Robix::UI {
namespace {

Robo::Color PackedColor(const std::string& name, bool light, const char* fallback) {
    const std::string& token = name.empty() ? std::string(fallback) : name;
    if (!token.empty() && token[0] == '#') {
        return Robo::Color::FromHex(token);
    }
    const std::uint32_t packed = RoboFoundationPlatformColorForTheme(token.c_str(), light ? ROBO_TRUE : ROBO_FALSE);
    Robo::Color color;
    color.alpha = static_cast<std::uint8_t>((packed >> 24) & 0xFFu);
    color.red = static_cast<std::uint8_t>((packed >> 16) & 0xFFu);
    color.green = static_cast<std::uint8_t>((packed >> 8) & 0xFFu);
    color.blue = static_cast<std::uint8_t>(packed & 0xFFu);
    if (color.alpha == 0) {
        color.alpha = 255;
    }
    return color;
}

}  // namespace

void RoboScrollbarPlatformPaint(const ScrollbarPaint& bar, Robo::FrameBuffer& buffer, bool light) {
    if (bar.width <= 0 || bar.height <= 0) {
        return;
    }
    const int thickness = std::max(2, bar.thickness);
    const Robo::Color track = PackedColor(bar.trackColor, light, "border-default");
    const Robo::Color thumb = PackedColor(bar.thumbColor, light, "text-tertiary");
    if (bar.vertical) {
        const int trackX = bar.x + std::max(0, bar.width - thickness);
        const int trackH = std::max(thickness, bar.height - 18);
        const int trackY = bar.y + 8;
        buffer.FillRoundedRect(static_cast<float>(trackX), static_cast<float>(trackY), static_cast<float>(thickness),
                               static_cast<float>(trackH), static_cast<float>(thickness) * 0.5f, track, 1.0f);
        const int thumbH = std::max(thickness * 4, trackH / 3);
        buffer.FillRoundedRect(static_cast<float>(trackX), static_cast<float>(trackY), static_cast<float>(thickness),
                               static_cast<float>(thumbH), static_cast<float>(thickness) * 0.5f, thumb, 1.0f);
        return;
    }
    const int trackY = bar.y + std::max(0, bar.height - thickness);
    const int trackW = std::max(thickness, bar.width - 18);
    const int trackX = bar.x + 8;
    buffer.FillRoundedRect(static_cast<float>(trackX), static_cast<float>(trackY), static_cast<float>(trackW),
                           static_cast<float>(thickness), static_cast<float>(thickness) * 0.5f, track, 1.0f);
    const int thumbW = std::max(thickness * 4, trackW / 3);
    buffer.FillRoundedRect(static_cast<float>(trackX), static_cast<float>(trackY), static_cast<float>(thumbW),
                           static_cast<float>(thickness), static_cast<float>(thickness) * 0.5f, thumb, 1.0f);
}

}  // namespace Robix::UI
