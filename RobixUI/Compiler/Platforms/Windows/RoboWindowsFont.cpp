#include "../../../RoboGPU/RoboText.hpp"
#include "../../../RoboShared/RoboFontPack.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwrite.h>
#include <dwrite_2.h>

namespace {

IDWriteFactory2* gFactory = nullptr;
IDWriteFontFace* gFaces[5] = {};
const RoboU8* gPack = nullptr;
RoboSize gPackBytes = 0;

const wchar_t* kFacePaths[5] = {
    L"Assets\\Fonts\\Inter\\Inter-Regular.ttf", L"Assets\\Fonts\\Inter\\Inter-Medium.ttf",
    L"Assets\\Fonts\\Inter\\Inter-SemiBold.ttf", L"Assets\\Fonts\\Inter\\Inter-Bold.ttf",
    L"Assets\\Fonts\\Inter\\Inter-Italic.ttf"};

struct RoboWindowsGlyph {
  int face;
  RoboU16 size;
  RoboU32 code;
  RoboI32 advance;
  RoboI32 originX;
  RoboI32 originY;
  RoboI32 width;
  RoboI32 height;
  RoboU8* alpha;
  RoboBool ready;
};

RoboWindowsGlyph gGlyphs[384];

bool RoboWindowsNameEq(const char* name, const char* token) {
  while (*name != 0 && *name != '@' && *token != 0) {
    char a = *name;
    char b = *token;
    if (a >= 'A' && a <= 'Z') {
      a = static_cast<char>(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
      b = static_cast<char>(b - 'A' + 'a');
    }
    if (a != b) {
      return false;
    }
    ++name;
    ++token;
  }
  return (*token == 0) && (*name == 0 || *name == '@');
}

int RoboWindowsFaceIndex(const char* name) {
  if (name == nullptr) {
    return 0;
  }
  if (RoboWindowsNameEq(name, "Inter-Medium") || RoboWindowsNameEq(name, "sans-medium")) {
    return 1;
  }
  if (RoboWindowsNameEq(name, "Inter-SemiBold") || RoboWindowsNameEq(name, "sans-semibold")) {
    return 2;
  }
  if (RoboWindowsNameEq(name, "Inter-Bold") || RoboWindowsNameEq(name, "sans-bold")) {
    return 3;
  }
  if (RoboWindowsNameEq(name, "Inter-Italic") || RoboWindowsNameEq(name, "sans-italic")) {
    return 4;
  }
  return 0;
}

struct RoboWindowsFaceCache {
  const void* header;
  int face;
};

RoboWindowsFaceCache gFaceCache[16];
int gFaceCacheCount = 0;

int RoboWindowsFaceForFont(const RoboFontView* font) {
  if (font == nullptr || font->header == nullptr || gPack == nullptr || gPackBytes < sizeof(RoboFontPackHeader)) {
    if (font != nullptr && font->header != nullptr && font->header->firstCodepoint <= 4) {
      return static_cast<int>(font->header->firstCodepoint);
    }
    return 0;
  }
  for (int index = 0; index < gFaceCacheCount; ++index) {
    if (gFaceCache[index].header == font->header) {
      return gFaceCache[index].face;
    }
  }
  const auto* header = reinterpret_cast<const RoboFontPackHeader*>(gPack);
  if (header->magic != ROBO_FONT_PACK_MAGIC || header->fontCount == 0) {
    return 0;
  }
  const auto* entries = reinterpret_cast<const RoboFontPackEntry*>(gPack + header->entryOffset);
  const auto* bytes = reinterpret_cast<const RoboU8*>(font->header);
  int face = 0;
  for (RoboU32 index = 0; index < header->fontCount; ++index) {
    if (gPack + entries[index].offset == bytes) {
      face = RoboWindowsFaceIndex(entries[index].name);
      break;
    }
  }
  if (gFaceCacheCount < static_cast<int>(sizeof(gFaceCache) / sizeof(gFaceCache[0]))) {
    gFaceCache[gFaceCacheCount].header = font->header;
    gFaceCache[gFaceCacheCount].face = face;
    ++gFaceCacheCount;
  }
  return face;
}

std::wstring RoboWindowsResolveFont(const wchar_t* relative) {
  wchar_t exe[MAX_PATH]{};
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring roots[2];
  roots[0].assign(exe);
  const size_t slash = roots[0].find_last_of(L"\\/");
  if (slash != std::wstring::npos) {
    roots[0].resize(slash);
  }
  wchar_t cwd[MAX_PATH]{};
  GetCurrentDirectoryW(MAX_PATH, cwd);
  roots[1].assign(cwd);
  for (std::wstring root : roots) {
    for (int step = 0; step < 8; ++step) {
      const std::wstring candidate = root + L"\\" + relative;
      if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return candidate;
      }
      const size_t cut = root.find_last_of(L"\\/");
      if (cut == std::wstring::npos) {
        break;
      }
      root.resize(cut);
    }
  }
  return relative;
}

IDWriteFontFace* RoboWindowsOpenFace(int index) {
  if (index < 0 || index > 4 || gFactory == nullptr) {
    return nullptr;
  }
  if (gFaces[index] != nullptr) {
    return gFaces[index];
  }
  static std::wstring resolved[5];
  resolved[index] = RoboWindowsResolveFont(kFacePaths[index]);
  IDWriteFontFile* file = nullptr;
  if (FAILED(gFactory->CreateFontFileReference(resolved[index].c_str(), nullptr, &file)) || file == nullptr) {
    return nullptr;
  }
  IDWriteFontFace* face = nullptr;
  const HRESULT created =
      gFactory->CreateFontFace(DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
  file->Release();
  if (FAILED(created) || face == nullptr) {
    return nullptr;
  }
  gFaces[index] = face;
  return face;
}

int RoboWindowsGlyphIndex(int faceIndex, RoboU32 code, RoboU16 size) {
  const RoboU32 hash = (static_cast<RoboU32>(faceIndex) * 131u) ^ (static_cast<RoboU32>(size) * 17u) ^ code;
  const int count = static_cast<int>(sizeof(gGlyphs) / sizeof(gGlyphs[0]));
  int slot = static_cast<int>(hash % static_cast<RoboU32>(count));
  for (int step = 0; step < count; ++step) {
    const int index = (slot + step) % count;
    if (!gGlyphs[index].ready ||
        (gGlyphs[index].face == faceIndex && gGlyphs[index].size == size && gGlyphs[index].code == code)) {
      return index;
    }
  }
  return slot;
}

RoboWindowsGlyph* RoboWindowsRasterGlyph(int faceIndex, RoboU32 code, RoboU16 size) {
  RoboWindowsGlyph* cached = &gGlyphs[RoboWindowsGlyphIndex(faceIndex, code, size)];
  if (cached->ready && cached->face == faceIndex && cached->size == size && cached->code == code) {
    return cached;
  }
  IDWriteFontFace* face = RoboWindowsOpenFace(faceIndex);
  if (face == nullptr || size == 0 || gFactory == nullptr) {
    return nullptr;
  }
  UINT32 codepoint = code;
  UINT16 index = 0;
  face->GetGlyphIndices(&codepoint, 1, &index);
  DWRITE_GLYPH_METRICS metrics{};
  face->GetDesignGlyphMetrics(&index, 1, &metrics, FALSE);
  DWRITE_FONT_METRICS fontMetrics{};
  face->GetMetrics(&fontMetrics);
  const RoboI32 advance = fontMetrics.designUnitsPerEm == 0
                              ? 0
                              : (static_cast<RoboI32>(metrics.advanceWidth) * static_cast<RoboI32>(size) +
                                 static_cast<RoboI32>(fontMetrics.designUnitsPerEm / 2)) /
                                    static_cast<RoboI32>(fontMetrics.designUnitsPerEm);
  FLOAT runAdvance = static_cast<FLOAT>(advance);
  DWRITE_GLYPH_OFFSET offset{};
  DWRITE_GLYPH_RUN run{};
  run.fontFace = face;
  run.fontEmSize = static_cast<FLOAT>(size);
  run.glyphCount = 1;
  run.glyphIndices = &index;
  run.glyphAdvances = &runAdvance;
  run.glyphOffsets = &offset;
  IDWriteGlyphRunAnalysis* analysis = nullptr;
  if (FAILED(gFactory->CreateGlyphRunAnalysis(&run, nullptr, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
                                             DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_ENABLED,
                                             DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0.0f, 0.0f, &analysis)) ||
      analysis == nullptr) {
    return nullptr;
  }
  RECT bounds{};
  analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &bounds);
  const RoboI32 width = bounds.right - bounds.left;
  const RoboI32 height = bounds.bottom - bounds.top;
  RoboU8* alpha = nullptr;
  if (width > 0 && height > 0 && width < 512 && height < 512) {
    const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
    alpha = static_cast<RoboU8*>(std::malloc(count));
    if (alpha == nullptr ||
        FAILED(analysis->CreateAlphaTexture(DWRITE_TEXTURE_ALIASED_1x1, &bounds, alpha,
                                            static_cast<UINT32>(count)))) {
      std::free(alpha);
      alpha = nullptr;
    }
  }
  analysis->Release();

  RoboWindowsGlyph* slot = cached;
  std::free(slot->alpha);
  slot->face = faceIndex;
  slot->size = size;
  slot->code = code;
  slot->advance = advance;
  slot->originX = bounds.left;
  slot->originY = bounds.top;
  slot->width = alpha != nullptr ? width : 0;
  slot->height = alpha != nullptr ? height : 0;
  slot->alpha = alpha;
  slot->ready = ROBO_TRUE;
  return slot;
}

RoboI32 RoboWindowsMeasureGlyph(const RoboFontView* font, RoboU32 codepoint, RoboU16 sizePx) {
  if (codepoint == '\n' || codepoint == '\r') {
    return 0;
  }
  if (codepoint == '\t') {
    return RoboWindowsMeasureGlyph(font, ' ', sizePx) * 4;
  }
  const RoboU16 size = sizePx == 0 ? static_cast<RoboU16>(16) : sizePx;
  RoboWindowsGlyph* glyph = RoboWindowsRasterGlyph(RoboWindowsFaceForFont(font), codepoint, size);
  return glyph != nullptr ? glyph->advance : 0;
}

void RoboWindowsDrawGlyph(RoboGPU* gpu, const RoboFontView* font, RoboI32 x, RoboI32 baselineY, RoboU32 codepoint,
                          RoboU32 colorARGB, RoboU16 sizePx) {
  if (gpu == nullptr || codepoint < 32) {
    return;
  }
  const RoboU16 size = sizePx == 0 ? static_cast<RoboU16>(16) : sizePx;
  RoboWindowsGlyph* glyph = RoboWindowsRasterGlyph(RoboWindowsFaceForFont(font), codepoint, size);
  if (glyph == nullptr || glyph->alpha == nullptr || glyph->width <= 0 || glyph->height <= 0) {
    return;
  }
  for (RoboI32 row = 0; row < glyph->height; ++row) {
    const RoboU8* coverage = glyph->alpha + static_cast<size_t>(row) * static_cast<size_t>(glyph->width);
    for (RoboI32 col = 0; col < glyph->width; ++col) {
      if (coverage[col] == 0) {
        continue;
      }
      RoboGPUBlendPixel(gpu, x + glyph->originX + col, baselineY + glyph->originY + row, colorARGB, coverage[col]);
    }
  }
}

const RoboTextHostRaster gRaster = {RoboWindowsMeasureGlyph, RoboWindowsDrawGlyph};

void FontExtentInternal(int face, int size, int* above, int* below) {
  const int em = size > 0 ? size : 1;
  if (above != nullptr) {
    *above = em;
  }
  if (below != nullptr) {
    *below = 0;
  }
  if (size <= 0) {
    return;
  }
  IDWriteFontFace* fontFace = RoboWindowsOpenFace(face);
  if (fontFace == nullptr) {
    return;
  }
  DWRITE_FONT_METRICS metrics{};
  fontFace->GetMetrics(&metrics);
  if (metrics.designUnitsPerEm == 0) {
    return;
  }
  const auto scale = [&](int value) {
    return static_cast<int>((static_cast<long long>(value) * size + metrics.designUnitsPerEm / 2) /
                            metrics.designUnitsPerEm);
  };
  if (above != nullptr) {
    *above = std::max(1, scale(static_cast<int>(metrics.ascent)));
  }
  if (below != nullptr) {
    *below = std::max(0, scale(static_cast<int>(metrics.descent)));
  }
}

}  // namespace

void RoboWindowsFontExtent(int face, int size, int* above, int* below) {
  FontExtentInternal(face, size, above, below);
}

void RoboWindowsInstallFontRaster(const RoboU8* pack, RoboSize packBytes) {
  gPack = pack;
  gPackBytes = packBytes;
  if (gFactory == nullptr) {
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2),
                                   reinterpret_cast<IUnknown**>(&gFactory))) ||
        gFactory == nullptr) {
      return;
    }
  }
  if (RoboWindowsOpenFace(0) == nullptr) {
    return;
  }
  RoboTextSetHostRaster(&gRaster);
}
