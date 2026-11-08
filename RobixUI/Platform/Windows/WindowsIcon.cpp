#include "WindowsIcon.hpp"

#include "../../RoboGPU/RoboGPU.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#define NANOSVG_IMPLEMENTATION
#include "../../ThirdParty/nanosvg/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "../../ThirdParty/nanosvg/nanosvgrast.h"

namespace Robix::UI {
namespace {

std::string HexByte(unsigned value) {
    const char* digits = "0123456789ABCDEF";
    std::string out;
    out.push_back(digits[(value >> 4) & 0xF]);
    out.push_back(digits[value & 0xF]);
    return out;
}

void ReplaceAll(std::string& text, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }
    for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
        text.replace(at, from.size(), to);
    }
}

std::string PaintTokens(std::string svg, RoboU32 color) {
    const std::string hex = "#" + HexByte((color >> 16) & 0xFF) + HexByte((color >> 8) & 0xFF) + HexByte(color & 0xFF);
    ReplaceAll(svg, "#ROBO_FILL", hex);
    ReplaceAll(svg, "#ROBO_STROKE", hex);
    ReplaceAll(svg, "#robo_fill", hex);
    ReplaceAll(svg, "#robo_stroke", hex);
    ReplaceAll(svg, "currentColor", hex);
    ReplaceAll(svg, "#ROBO_ACCENT", "#3B9EFF");
    ReplaceAll(svg, "#robo_accent", "#3B9EFF");
    return svg;
}

}  // namespace

void PaintWindowsSvg(RoboGPU* gpu, int x, int y, int size, const std::string& svg, RoboU32 color) {
    if (gpu == nullptr || svg.empty() || size <= 0) {
        return;
    }
    const std::string painted = PaintTokens(svg, color);
    static std::unordered_map<std::string, std::vector<uint8_t>> cache;
    const std::string key = std::to_string(size) + "\n" + painted;
    std::vector<uint8_t>* pixels = nullptr;
    const auto found = cache.find(key);
    if (found != cache.end()) {
        pixels = &found->second;
    } else {
        std::string mutableSvg = painted;
        NSVGimage* image = nsvgParse(mutableSvg.data(), "px", 96.0f);
        if (image == nullptr) {
            return;
        }
        const float srcW = image->width > 1.0f ? image->width : 24.0f;
        const float srcH = image->height > 1.0f ? image->height : 24.0f;
        const float scale = static_cast<float>(size) / (srcW > srcH ? srcW : srcH);
        NSVGrasterizer* raster = nsvgCreateRasterizer();
        if (raster == nullptr) {
            nsvgDelete(image);
            return;
        }
        std::vector<uint8_t> rgba(static_cast<size_t>(size * size * 4), 0);
        const float ox = (static_cast<float>(size) - srcW * scale) * 0.5f;
        const float oy = (static_cast<float>(size) - srcH * scale) * 0.5f;
        nsvgRasterize(raster, image, ox, oy, scale, rgba.data(), size, size, size * 4);
        nsvgDeleteRasterizer(raster);
        nsvgDelete(image);
        pixels = &cache.emplace(key, std::move(rgba)).first->second;
    }
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            const uint8_t* px = pixels->data() + static_cast<size_t>((row * size + col) * 4);
            if (px[3] == 0) {
                continue;
            }
            const RoboU32 argb = (static_cast<RoboU32>(px[0]) << 16) | (static_cast<RoboU32>(px[1]) << 8) |
                                 static_cast<RoboU32>(px[2]);
            RoboGPUBlendPixel(gpu, x + col, y + row, argb, px[3]);
        }
    }
}

}  // namespace Robix::UI
