#pragma once

#include <cstdint>
#include <string>

namespace Robix::UI {

struct FoundationPaint {
    std::uint32_t primary = 0xFFFFFF;
    std::uint32_t window = 0xF5F5F7;
    std::uint32_t text = 0x1D1D1F;
    std::uint32_t button = 0x0E7AFF;
    std::uint32_t buttonText = 0xFFFFFF;
    int space = 16;
    int titleSize = 16;
    int bodySize = 17;
    int radius = 8;
    int radiusLg = 12;
};

// Reads RobixUI/Components/Foundation.rbxu. Web and Windows use this same file.
bool LoadFoundation(FoundationPaint& paint, bool dark, std::string& error);
// A Font entry from Foundation.rbxu. Name is matched without case. Src is the path under Assets/Fonts.
bool FoundationFont(const std::string& name, std::string& src, std::string& token);
std::string FoundationCss(const std::string& fontPrefix = "Assets/Fonts");
std::string FoundationLine();
bool SameFoundation(std::string& error);

}
