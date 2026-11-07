#include "Framebuffer.hpp"

#include <algorithm>
#include <fstream>
#include <vector>

namespace Robix::UI::Platform {
namespace {

std::uint8_t GlyphRow(char ch, int row) {
    const char upper = (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - 32) : ch;
    static const std::uint8_t kDigits[10][5] = {
        {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
        {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}
    };
    static const std::uint8_t kLetters[26][5] = {
        {7, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {7, 4, 4, 4, 7}, {6, 5, 5, 5, 6}, {7, 4, 7, 4, 7},
        {7, 4, 7, 4, 4}, {7, 4, 5, 5, 7}, {5, 5, 7, 5, 5}, {7, 2, 2, 2, 7}, {1, 1, 1, 5, 7},
        {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7}, {7, 7, 5, 5, 5}, {7, 5, 5, 5, 5}, {7, 5, 5, 5, 7},
        {7, 5, 7, 4, 4}, {7, 5, 5, 7, 1}, {7, 5, 6, 5, 5}, {7, 4, 7, 1, 7}, {7, 2, 2, 2, 2},
        {5, 5, 5, 5, 7}, {5, 5, 5, 5, 2}, {5, 5, 5, 7, 7}, {5, 5, 2, 5, 5}, {5, 5, 7, 2, 2},
        {7, 1, 2, 4, 7}
    };
    if (row < 0 || row > 4) return 0;
    if (upper >= 'A' && upper <= 'Z') return kLetters[upper - 'A'][row];
    if (upper >= '0' && upper <= '9') return kDigits[upper - '0'][row];
    if (upper == ' ') return 0;
    if (upper == '-') return row == 2 ? 7 : 0;
    return 7;
}

} // namespace

Clip g_clip;

Clip CurrentClip() { return g_clip; }

void SetClip(Clip clip) { g_clip = clip; }

int ScaleFor(int fontSize) {
    return std::max(1, fontSize / 8);
}

bool MeasureFont(const std::string& text, int fontSize, const std::string& font, int& width, int& height);
bool DrawFont(std::vector<std::uint32_t>& pixels, int frameW, int frameH, int x, int y, const std::string& text, int fontSize, std::uint32_t color, const std::string& font);

TextSize MeasureText(const std::string& text, int fontSize, const std::string& font) {
    int width = 0;
    int height = 0;
    if (MeasureFont(text, fontSize, font, width, height)) return {std::max(1, width), std::max(1, height)};
    const int scale = ScaleFor(fontSize);
    const int count = std::max<int>(1, static_cast<int>(text.size()));
    return {count * (3 * scale + scale), 5 * scale};
}

void Fill(std::vector<std::uint32_t>& pixels, int frameW, int frameH, int x, int y, int w, int h, std::uint32_t color) {
    for (int row = y; row < y + h; ++row) {
        if (row < 0 || row >= frameH) continue;
        if (g_clip.on && (row < g_clip.y || row >= g_clip.y + g_clip.h)) continue;
        for (int col = x; col < x + w; ++col) {
            if (col < 0 || col >= frameW) continue;
            if (g_clip.on && (col < g_clip.x || col >= g_clip.x + g_clip.w)) continue;
            pixels[static_cast<std::size_t>(row * frameW + col)] = color;
        }
    }
}

void Blend(std::vector<std::uint32_t>& pixels, int frameW, int frameH, int x, int y, std::uint32_t color, std::uint8_t coverage) {
    if (coverage == 0 || x < 0 || y < 0 || x >= frameW || y >= frameH) return;
    if (g_clip.on && (x < g_clip.x || y < g_clip.y || x >= g_clip.x + g_clip.w || y >= g_clip.y + g_clip.h)) return;
    const std::size_t index = static_cast<std::size_t>(y * frameW + x);
    const std::uint32_t dst = pixels[index];
    const int amount = coverage;
    const int red = ((static_cast<int>(color >> 16) & 255) * amount + (static_cast<int>(dst >> 16) & 255) * (255 - amount)) / 255;
    const int green = ((static_cast<int>(color >> 8) & 255) * amount + (static_cast<int>(dst >> 8) & 255) * (255 - amount)) / 255;
    const int blue = ((static_cast<int>(color) & 255) * amount + (static_cast<int>(dst) & 255) * (255 - amount)) / 255;
    pixels[index] = (static_cast<std::uint32_t>(red) << 16) | (static_cast<std::uint32_t>(green) << 8) | static_cast<std::uint32_t>(blue);
}

void DrawText(std::vector<std::uint32_t>& pixels, int frameW, int frameH, int x, int y, const std::string& text, int fontSize, std::uint32_t color, const std::string& font) {
    if (DrawFont(pixels, frameW, frameH, x, y, text, fontSize, color, font)) return;
    const int scale = ScaleFor(fontSize);
    int cursor = x;
    for (unsigned char ch : text) {
        for (int row = 0; row < 5; ++row) {
            const std::uint8_t bits = GlyphRow(static_cast<char>(ch), row);
            for (int col = 0; col < 3; ++col) {
                if ((bits & (4 >> col)) == 0) continue;
                Fill(pixels, frameW, frameH, cursor + col * scale, y + row * scale, scale, scale, color);
            }
        }
        cursor += 3 * scale + scale;
    }
}

bool WriteBmp(const std::string& path, int width, int height, const std::vector<std::uint32_t>& pixels) {
    const int stride = (width * 3 + 3) & ~3;
    const std::uint32_t dataSize = static_cast<std::uint32_t>(stride * height);
    const std::uint32_t fileSize = 54 + dataSize;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    unsigned char header[54] = {};
    header[0] = 'B';
    header[1] = 'M';
    header[2] = static_cast<unsigned char>(fileSize);
    header[3] = static_cast<unsigned char>(fileSize >> 8);
    header[4] = static_cast<unsigned char>(fileSize >> 16);
    header[5] = static_cast<unsigned char>(fileSize >> 24);
    header[10] = 54;
    header[14] = 40;
    header[18] = static_cast<unsigned char>(width);
    header[19] = static_cast<unsigned char>(width >> 8);
    header[20] = static_cast<unsigned char>(width >> 16);
    header[21] = static_cast<unsigned char>(width >> 24);
    header[22] = static_cast<unsigned char>(height);
    header[23] = static_cast<unsigned char>(height >> 8);
    header[24] = static_cast<unsigned char>(height >> 16);
    header[25] = static_cast<unsigned char>(height >> 24);
    header[26] = 1;
    header[28] = 24;
    out.write(reinterpret_cast<const char*>(header), 54);
    std::vector<unsigned char> row(static_cast<std::size_t>(stride));
    for (int y = height - 1; y >= 0; --y) {
        std::fill(row.begin(), row.end(), 0);
        for (int x = 0; x < width; ++x) {
            const std::uint32_t color = pixels[static_cast<std::size_t>(y * width + x)];
            row[static_cast<std::size_t>(x * 3)] = static_cast<unsigned char>(color);
            row[static_cast<std::size_t>(x * 3 + 1)] = static_cast<unsigned char>(color >> 8);
            row[static_cast<std::size_t>(x * 3 + 2)] = static_cast<unsigned char>(color >> 16);
        }
        out.write(reinterpret_cast<const char*>(row.data()), stride);
    }
    return static_cast<bool>(out);
}

void CopyToDisplay(const std::vector<std::uint32_t>& pixels, std::vector<std::uint32_t>& display) {
    display.resize(pixels.size());
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        const std::uint32_t color = pixels[i];
        const std::uint32_t blue = color & 0xFFu;
        const std::uint32_t green = (color >> 8) & 0xFFu;
        const std::uint32_t red = (color >> 16) & 0xFFu;
        display[i] = blue | (green << 8) | (red << 16);
    }
}

}
