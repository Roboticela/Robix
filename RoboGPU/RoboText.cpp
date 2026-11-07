#include "RoboText.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwrite.h>
#include <dwrite_2.h>

namespace {

IDWriteFactory2* gFactory = nullptr;
IDWriteFontFace* gFaces[5] = {};

const wchar_t* kFaceFiles[5] = {L"Inter-Regular.ttf", L"Inter-Medium.ttf", L"Inter-SemiBold.ttf", L"Inter-Bold.ttf",
                                L"Inter-Italic.ttf"};

RoboFontHeader gHeaders[5] = {
    {780, 220, 0, 1000, 0}, {780, 220, 0, 1000, 1}, {780, 220, 0, 1000, 2},
    {780, 220, 0, 1000, 3}, {780, 220, 0, 1000, 4},
};

struct Glyph {
  int face = 0;
  RoboU16 size = 0;
  RoboU32 code = 0;
  RoboI32 advance = 0;
  RoboI32 originX = 0;
  RoboI32 originY = 0;
  RoboI32 width = 0;
  RoboI32 height = 0;
  RoboU8* alpha = nullptr;
  RoboBool ready = ROBO_FALSE;
};

Glyph gGlyphs[384];

int FaceIndex(const char* name) {
  if (name == ROBO_NULL) {
    return 0;
  }
  std::string key = name;
  for (char& ch : key) {
    if (ch >= 'A' && ch <= 'Z') {
      ch = static_cast<char>(ch - 'A' + 'a');
    }
  }
  if (key.find("medium") != std::string::npos) {
    return 1;
  }
  if (key.find("semibold") != std::string::npos || key.find("semi-bold") != std::string::npos) {
    return 2;
  }
  if (key.find("bold") != std::string::npos) {
    return 3;
  }
  if (key.find("italic") != std::string::npos) {
    return 4;
  }
  return 0;
}

std::wstring FontPath(int index) {
  namespace fs = std::filesystem;
  fs::path cursor = fs::current_path();
  for (int depth = 0; depth < 6; ++depth) {
    const fs::path file = cursor / "Assets" / "Fonts" / "Inter" / kFaceFiles[index];
    if (fs::is_regular_file(file)) {
      return file.wstring();
    }
    if (!cursor.has_parent_path() || cursor == cursor.parent_path()) {
      break;
    }
    cursor = cursor.parent_path();
  }
  return std::wstring(L"Assets\\Fonts\\Inter\\") + kFaceFiles[index];
}

bool EnsureFactory() {
  if (gFactory != nullptr) {
    return true;
  }
  return SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2),
                                       reinterpret_cast<IUnknown**>(&gFactory))) &&
         gFactory != nullptr;
}

IDWriteFontFace* OpenFace(int index) {
  if (index < 0 || index > 4 || !EnsureFactory()) {
    return nullptr;
  }
  if (gFaces[index] != nullptr) {
    return gFaces[index];
  }
  const std::wstring path = FontPath(index);
  IDWriteFontFile* file = nullptr;
  if (FAILED(gFactory->CreateFontFileReference(path.c_str(), nullptr, &file)) || file == nullptr) {
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

int GlyphSlot(int face, RoboU32 code, RoboU16 size) {
  const RoboU32 hash = (static_cast<RoboU32>(face) * 131u) ^ (static_cast<RoboU32>(size) * 17u) ^ code;
  const int count = static_cast<int>(sizeof(gGlyphs) / sizeof(gGlyphs[0]));
  const int slot = static_cast<int>(hash % static_cast<RoboU32>(count));
  for (int step = 0; step < count; ++step) {
    const int index = (slot + step) % count;
    if (!gGlyphs[index].ready || (gGlyphs[index].face == face && gGlyphs[index].size == size && gGlyphs[index].code == code)) {
      return index;
    }
  }
  return slot;
}

Glyph* Raster(int faceIndex, RoboU32 code, RoboU16 size) {
  Glyph* cached = &gGlyphs[GlyphSlot(faceIndex, code, size)];
  if (cached->ready && cached->face == faceIndex && cached->size == size && cached->code == code) {
    return cached;
  }
  IDWriteFontFace* face = OpenFace(faceIndex);
  if (face == nullptr || size == 0) {
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
                              ? static_cast<RoboI32>(size / 2)
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
    const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u + 64u;
    alpha = static_cast<RoboU8*>(std::malloc(count));
    if (alpha == nullptr || FAILED(analysis->CreateAlphaTexture(DWRITE_TEXTURE_ALIASED_1x1, &bounds, alpha,
                                                                static_cast<UINT32>(count)))) {
      std::free(alpha);
      alpha = nullptr;
    }
  }
  analysis->Release();
  std::free(cached->alpha);
  cached->face = faceIndex;
  cached->size = size;
  cached->code = code;
  cached->advance = advance;
  cached->originX = bounds.left;
  cached->originY = bounds.top;
  cached->width = alpha != nullptr ? width : 0;
  cached->height = alpha != nullptr ? height : 0;
  cached->alpha = alpha;
  cached->ready = ROBO_TRUE;
  return cached;
}

int FaceOf(const RoboFontView& face) {
  return face.header == nullptr ? 0 : static_cast<int>(face.header->face);
}

void Blend(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboU32 colorARGB, RoboU8 coverage) {
  if (gpu == nullptr || gpu->pixels == nullptr || x < 0 || y < 0 || x >= gpu->width || y >= gpu->height || coverage == 0) {
    return;
  }
  const RoboU32 sourceAlpha = ((colorARGB >> 24) & 0xFFu) * coverage / 255u;
  if (sourceAlpha == 0) {
    return;
  }
  std::uint8_t* pixel = gpu->pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(gpu->stride) +
                        static_cast<std::size_t>(x) * 4u;
  const std::uint8_t red = static_cast<std::uint8_t>((colorARGB >> 16) & 0xFFu);
  const std::uint8_t green = static_cast<std::uint8_t>((colorARGB >> 8) & 0xFFu);
  const std::uint8_t blue = static_cast<std::uint8_t>(colorARGB & 0xFFu);
  if (sourceAlpha >= 250u) {
    pixel[0] = red;
    pixel[1] = green;
    pixel[2] = blue;
    pixel[3] = 255;
    return;
  }
  const RoboU32 inverse = 255u - sourceAlpha;
  pixel[0] = static_cast<std::uint8_t>((red * sourceAlpha + pixel[0] * inverse) / 255u);
  pixel[1] = static_cast<std::uint8_t>((green * sourceAlpha + pixel[1] * inverse) / 255u);
  pixel[2] = static_cast<std::uint8_t>((blue * sourceAlpha + pixel[2] * inverse) / 255u);
  pixel[3] = static_cast<std::uint8_t>(sourceAlpha + pixel[3] * inverse / 255u);
}

RoboU32 FirstCode(const char* utf8) {
  if (utf8 == ROBO_NULL || utf8[0] == 0) {
    return 0;
  }
  const auto* bytes = reinterpret_cast<const unsigned char*>(utf8);
  if (bytes[0] < 0x80u) {
    return bytes[0];
  }
  if ((bytes[0] & 0xE0u) == 0xC0u) {
    return (static_cast<RoboU32>(bytes[0] & 0x1Fu) << 6) | (bytes[1] & 0x3Fu);
  }
  if ((bytes[0] & 0xF0u) == 0xE0u) {
    return (static_cast<RoboU32>(bytes[0] & 0x0Fu) << 12) | (static_cast<RoboU32>(bytes[1] & 0x3Fu) << 6) |
           (bytes[2] & 0x3Fu);
  }
  return bytes[0];
}

}  // namespace

RoboFontView RoboFontForName(const char* name) {
  RoboFontView view;
  view.header = &gHeaders[FaceIndex(name)];
  return view;
}

RoboFontView RoboFontPackFindSize(const RoboFontPackView&, const char*, RoboU16) {
  return {};
}

RoboU16 RoboFontBaseSize(const RoboFontView& face) {
  return face.header == nullptr ? 0 : face.header->units;
}

RoboI32 RoboTextMeasureCode(const RoboFontView& face, RoboU32 code, RoboU16 sizePx) {
  if (code == '\n' || code == '\r') {
    return 0;
  }
  const RoboU16 size = sizePx == 0 ? ROBO_FONT_MASTER_PX : sizePx;
  if (code == '\t') {
    return RoboTextMeasureCode(face, ' ', size) * 4;
  }
  Glyph* glyph = Raster(FaceOf(face), code, size);
  if (glyph != nullptr) {
    return glyph->advance;
  }
  return code == ' ' ? static_cast<RoboI32>(size / 3) : static_cast<RoboI32>(size / 2);
}

void RoboTextDrawUTF8Baseline(RoboGPU* gpu, const RoboFontView& face, RoboI32 x, RoboI32 baselineY, const char* utf8,
                              RoboU32 colorARGB, RoboU16 sizePx) {
  const RoboU32 code = FirstCode(utf8);
  if (gpu == nullptr || code < 32) {
    return;
  }
  const RoboU16 size = sizePx == 0 ? ROBO_FONT_MASTER_PX : sizePx;
  Glyph* glyph = Raster(FaceOf(face), code, size);
  if (glyph == nullptr || glyph->alpha == nullptr) {
    return;
  }
  for (RoboI32 row = 0; row < glyph->height; ++row) {
    const RoboU8* coverage = glyph->alpha + static_cast<std::size_t>(row) * static_cast<std::size_t>(glyph->width);
    for (RoboI32 col = 0; col < glyph->width; ++col) {
      Blend(gpu, x + glyph->originX + col, baselineY + glyph->originY + row, colorARGB, coverage[col]);
    }
  }
}
