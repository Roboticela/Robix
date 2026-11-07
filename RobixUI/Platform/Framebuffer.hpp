#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Robix::UI::Platform {

// One software framebuffer. Pixels are 0x00RRGGBB. Windows is the only presenter.

struct TextSize {
    int w = 0;
    int h = 0;
};

int ScaleFor(int fontSize);
TextSize MeasureText(const std::string& text, int fontSize, const std::string& font = "Inter-Regular");

struct Clip {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool on = false;
};

Clip CurrentClip();
void SetClip(Clip clip);

void Fill(std::vector<std::uint32_t>& pixels, int frameW, int frameH, int x, int y, int w, int h, std::uint32_t color);
void Blend(std::vector<std::uint32_t>& pixels, int frameW, int frameH, int x, int y, std::uint32_t color, std::uint8_t coverage);
void DrawText(std::vector<std::uint32_t>& pixels, int frameW, int frameH, int x, int y, const std::string& text, int fontSize, std::uint32_t color, const std::string& font = "Inter-Regular");

bool WriteBmp(const std::string& path, int width, int height, const std::vector<std::uint32_t>& pixels);

// Same channel order as WriteBmp: low byte blue, then green, then red.
void CopyToDisplay(const std::vector<std::uint32_t>& pixels, std::vector<std::uint32_t>& display);

}
