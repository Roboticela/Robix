#include "../Framebuffer.hpp"
#include "../../Foundation.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <dwrite.h>
#include <dwrite_2.h>

namespace Robix::UI::Platform {
namespace {

IDWriteFactory2* g_factory = nullptr;
std::map<std::string, IDWriteFontFace*> g_faces;

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

std::filesystem::path FontRoot() {
    namespace fs = std::filesystem;
    fs::path cursor = fs::current_path();
    for (int depth = 0; depth < 8 && !cursor.empty(); ++depth) {
        if (fs::is_regular_file(cursor / "Assets" / "Fonts" / "Inter" / "Inter-Regular.ttf")) return cursor / "Assets" / "Fonts";
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor) break;
        cursor = cursor.parent_path();
    }
    return {};
}

std::string FaceFile(const std::string& font) {
    std::string src;
    std::string token;
    if (FoundationFont(font, src, token)) return src;
    const std::string key = Lower(font);
    if (key.empty() || key == "sans" || key == "sans-serif" || key == "geometric" || key == "humanist" || key == "rounded" || key == "display" || key == "expanded") {
        if (FoundationFont("sans", src, token)) return src;
    }
    if (key.size() > 4 && key.rfind(".ttf") == key.size() - 4) return font;
    return {};
}

std::string SystemFamily(const std::string& font) {
    const std::string key = Lower(font);
    if (key == "serif") return "Georgia";
    if (key == "mono" || key == "monospace") return "Consolas";
    if (key == "system" || key == "system-default") return "Segoe UI";
    if (key == "condensed") return "Arial Narrow";
    if (key == "handwriting" || key == "cursive" || key == "decorative") return "Segoe Script";
    return font;
}

IDWriteFontFace* OpenSystemFace(const std::string& family) {
    IDWriteFontCollection* collection = nullptr;
    if (FAILED(g_factory->GetSystemFontCollection(&collection)) || collection == nullptr) return nullptr;
    std::wstring wide(family.begin(), family.end());
    UINT32 index = 0;
    BOOL exists = FALSE;
    collection->FindFamilyName(wide.c_str(), &index, &exists);
    if (!exists) {
        collection->Release();
        return nullptr;
    }
    IDWriteFontFamily* found = nullptr;
    collection->GetFontFamily(index, &found);
    collection->Release();
    if (found == nullptr) return nullptr;
    IDWriteFont* font = nullptr;
    found->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
    found->Release();
    if (font == nullptr) return nullptr;
    IDWriteFontFace* face = nullptr;
    font->CreateFontFace(&face);
    font->Release();
    return face;
}

IDWriteFontFace* OpenFace(const std::string& font) {
    if (g_factory == nullptr) {
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2), reinterpret_cast<IUnknown**>(&g_factory))) || g_factory == nullptr) {
            return nullptr;
        }
    }
    const std::string relative = FaceFile(font);
    if (relative.empty()) {
        const std::string family = SystemFamily(font);
        const auto found = g_faces.find(family);
        if (found != g_faces.end()) return found->second;
        IDWriteFontFace* face = OpenSystemFace(family);
        if (face == nullptr) {
            std::cerr << "font " << font << " was not found\n";
            return OpenFace("Inter-Regular");
        }
        g_faces[family] = face;
        std::cout << "font " << family << "\n";
        return face;
    }
    const auto found = g_faces.find(relative);
    if (found != g_faces.end()) return found->second;
    const std::filesystem::path root = FontRoot();
    if (root.empty()) return nullptr;
    const std::filesystem::path file = root / relative;
    if (!std::filesystem::is_regular_file(file)) {
        std::cerr << "font " << font << " was not found at " << file.string() << "\n";
        return nullptr;
    }
    IDWriteFontFile* fontFile = nullptr;
    if (FAILED(g_factory->CreateFontFileReference(file.wstring().c_str(), nullptr, &fontFile)) || fontFile == nullptr) return nullptr;
    IDWriteFontFace* face = nullptr;
    const HRESULT created = g_factory->CreateFontFace(DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &fontFile, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    fontFile->Release();
    if (FAILED(created) || face == nullptr) return nullptr;
    g_faces[relative] = face;
    std::cout << "font " << relative << "\n";
    return face;
}

struct Glyph {
    int advance = 0;
    int originX = 0;
    int originY = 0;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> alpha;
};

bool Raster(IDWriteFontFace* face, int size, char32_t code, Glyph& glyph) {
    UINT32 codepoint = static_cast<UINT32>(code);
    UINT16 index = 0;
    face->GetGlyphIndices(&codepoint, 1, &index);
    DWRITE_GLYPH_METRICS metrics{};
    face->GetDesignGlyphMetrics(&index, 1, &metrics, FALSE);
    DWRITE_FONT_METRICS fontMetrics{};
    face->GetMetrics(&fontMetrics);
    if (fontMetrics.designUnitsPerEm == 0) return false;
    glyph.advance = (static_cast<int>(metrics.advanceWidth) * size + static_cast<int>(fontMetrics.designUnitsPerEm / 2)) / static_cast<int>(fontMetrics.designUnitsPerEm);
    FLOAT runAdvance = static_cast<FLOAT>(glyph.advance);
    DWRITE_GLYPH_OFFSET offset{};
    DWRITE_GLYPH_RUN run{};
    run.fontFace = face;
    run.fontEmSize = static_cast<FLOAT>(size);
    run.glyphCount = 1;
    run.glyphIndices = &index;
    run.glyphAdvances = &runAdvance;
    run.glyphOffsets = &offset;
    IDWriteGlyphRunAnalysis* analysis = nullptr;
    if (FAILED(g_factory->CreateGlyphRunAnalysis(&run, nullptr, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_ENABLED, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0.0f, 0.0f, &analysis)) || analysis == nullptr) {
        return false;
    }
    RECT bounds{};
    analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &bounds);
    glyph.originX = bounds.left;
    glyph.originY = bounds.top;
    glyph.width = bounds.right - bounds.left;
    glyph.height = bounds.bottom - bounds.top;
    if (glyph.width > 0 && glyph.height > 0 && glyph.width < 512 && glyph.height < 512) {
        glyph.alpha.resize(static_cast<std::size_t>(glyph.width * glyph.height));
        if (FAILED(analysis->CreateAlphaTexture(DWRITE_TEXTURE_ALIASED_1x1, &bounds, glyph.alpha.data(), static_cast<UINT32>(glyph.alpha.size())))) {
            glyph.alpha.clear();
        }
    }
    analysis->Release();
    return true;
}

int Ascent(IDWriteFontFace* face, int size) {
    DWRITE_FONT_METRICS metrics{};
    face->GetMetrics(&metrics);
    if (metrics.designUnitsPerEm == 0) return size;
    return metrics.ascent * size / static_cast<int>(metrics.designUnitsPerEm);
}

} // namespace

bool MeasureFont(const std::string& text, int fontSize, const std::string& font, int& width, int& height) {
    IDWriteFontFace* face = OpenFace(font);
    if (face == nullptr || fontSize <= 0) return false;
    width = 0;
    for (unsigned char ch : text) {
        Glyph glyph;
        if (!Raster(face, fontSize, ch, glyph)) return false;
        width += glyph.advance;
    }
    height = fontSize;
    if (width <= 0) width = 1;
    return true;
}

bool DrawFont(std::vector<std::uint32_t>& pixels, int frameW, int frameH, int x, int y, const std::string& text, int fontSize, std::uint32_t color, const std::string& font) {
    IDWriteFontFace* face = OpenFace(font);
    if (face == nullptr || fontSize <= 0) return false;
    const int baseline = y + Ascent(face, fontSize);
    int cursor = x;
    for (unsigned char ch : text) {
        Glyph glyph;
        if (!Raster(face, fontSize, ch, glyph)) return false;
        for (int row = 0; row < glyph.height; ++row) {
            for (int col = 0; col < glyph.width; ++col) {
                const std::uint8_t coverage = glyph.alpha.empty() ? 0 : glyph.alpha[static_cast<std::size_t>(row * glyph.width + col)];
                Blend(pixels, frameW, frameH, cursor + glyph.originX + col, baseline + glyph.originY + row, color, coverage);
            }
        }
        cursor += glyph.advance;
    }
    return true;
}

} // namespace Robix::UI::Platform
