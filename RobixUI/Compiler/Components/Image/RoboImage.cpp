#include "RoboImage.hpp"

#include "../../../RoboShared/RoboImage.hpp"

#include "../../../ThirdParty/nanosvg/nanosvg.h"
#include "../../../ThirdParty/nanosvg/nanosvgrast.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#endif

namespace {

const char* const ROBO_IMAGE_ATTRIBUTES[] = {
    "id", "source", "src", "fit", "objectfit", "object-fit", "alt", "description", "width", "height", "onload", "onerror",
};

std::string RoboImageLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string RoboImageFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

bool RoboImageKnown(const std::string& key) {
    const std::string folded = RoboImageLower(key);
    const int count = static_cast<int>(sizeof(ROBO_IMAGE_ATTRIBUTES) / sizeof(ROBO_IMAGE_ATTRIBUTES[0]));
    for (int index = 0; index < count; ++index) {
        if (folded == ROBO_IMAGE_ATTRIBUTES[index]) {
            return true;
        }
    }
    return false;
}

std::string RoboImageAttr(const RoboXMLNode& node, const char* key, const char* alias = nullptr) {
    const std::string value = RoboXMLAttr(node, key);
    if (!value.empty() || alias == nullptr) {
        return value;
    }
    return RoboXMLAttr(node, alias);
}

bool RoboImageInteger(const std::string& text, int& number) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || end == nullptr || *end != '\0') {
        return false;
    }
    number = static_cast<int>(parsed);
    return true;
}

void RoboImageFire(RoboImageRecord& image, const char* name) {
    image.fired.push_back(name);
}

bool RoboImageFit(std::string& fit) {
    fit = RoboImageLower(fit);
    if (fit.empty()) {
        fit = "contain";
    }
    return fit == "contain" || fit == "cover" || fit == "fill" || fit == "none" || fit == "scale-down";
}

constexpr int ROBO_IMAGE_MAX_EDGE = 8192;

bool RoboImageCommit(RoboImageRecord& image, int width, int height, const RoboU8* rgba, int stride, const char* format) {
    if (rgba == nullptr || width < 1 || height < 1 || width > ROBO_IMAGE_MAX_EDGE || height > ROBO_IMAGE_MAX_EDGE || stride < width * 4) {
        return false;
    }
    image.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int row = 0; row < height; ++row) {
        const RoboU8* line = rgba + static_cast<std::size_t>(row) * static_cast<std::size_t>(stride);
        for (int column = 0; column < width; ++column) {
            const RoboU8* pixel = line + static_cast<std::size_t>(column) * 4u;
            image.pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(width) + static_cast<std::size_t>(column)] =
                (static_cast<RoboU32>(pixel[3]) << 24) | (static_cast<RoboU32>(pixel[0]) << 16) | (static_cast<RoboU32>(pixel[1]) << 8) |
                static_cast<RoboU32>(pixel[2]);
        }
    }
    image.sourceWidth = width;
    image.sourceHeight = height;
    image.format = format;
    image.frame = 0;
    image.frameCount = 1;
    image.delays = {0};
    image.frames = image.pixels;
    if (!image.hasWidth) {
        image.width = width;
    }
    if (!image.hasHeight) {
        image.height = height;
    }
    RoboImageFire(image, "OnLoad");
    return true;
}

bool RoboImageStartsSvg(const RoboU8* data, std::size_t size) {
    std::size_t index = 0;
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        index = 3;
    }
    while (index < size && std::isspace(data[index])) {
        ++index;
    }
    if (index >= size || data[index] != '<') {
        return false;
    }
    const std::size_t remain = size - index;
    auto begins = [&](const char* word) {
        const std::size_t length = std::char_traits<char>::length(word);
        if (remain < length) {
            return false;
        }
        for (std::size_t at = 0; at < length; ++at) {
            const char left = static_cast<char>(std::tolower(data[index + at]));
            if (left != word[at]) {
                return false;
            }
        }
        return true;
    };
    if (begins("<svg")) {
        return true;
    }
    if (!begins("<?xml") && !begins("<!doctype")) {
        return false;
    }
    const std::size_t limit = std::min(size, index + static_cast<std::size_t>(8192));
    for (std::size_t at = index; at + 4 < limit; ++at) {
        if (std::tolower(data[at]) == '<' && std::tolower(data[at + 1]) == 's' && std::tolower(data[at + 2]) == 'v' && std::tolower(data[at + 3]) == 'g') {
            return true;
        }
    }
    return false;
}

bool RoboImageDecodeSvg(RoboImageRecord& image, const void* bytes, std::size_t size) {
    image.vector.assign(static_cast<const char*>(bytes), size);
    std::string text = image.vector;
    NSVGimage* parsed = nsvgParse(text.data(), "px", 96.0f);
    if (parsed == nullptr) {
        return false;
    }
    int width = static_cast<int>(std::lround(parsed->width));
    int height = static_cast<int>(std::lround(parsed->height));
    if (width < 1) {
        width = image.hasWidth ? image.width : 64;
    }
    if (height < 1) {
        height = image.hasHeight ? image.height : 64;
    }
    if (width > ROBO_IMAGE_MAX_EDGE || height > ROBO_IMAGE_MAX_EDGE) {
        nsvgDelete(parsed);
        return false;
    }
    const float sourceWidth = parsed->width > 0.0f ? parsed->width : static_cast<float>(width);
    const float sourceHeight = parsed->height > 0.0f ? parsed->height : static_cast<float>(height);
    const float scale = static_cast<float>(width) / sourceWidth;
    (void)sourceHeight;
    NSVGrasterizer* raster = nsvgCreateRasterizer();
    if (raster == nullptr) {
        nsvgDelete(parsed);
        return false;
    }
    std::vector<RoboU8> rgba(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0);
    nsvgRasterize(raster, parsed, 0.0f, 0.0f, scale, rgba.data(), width, height, width * 4);
    nsvgDeleteRasterizer(raster);
    nsvgDelete(parsed);
    return RoboImageCommit(image, width, height, rgba.data(), width * 4, "svg");
}

#if defined(_WIN32)

struct RoboImageCom {
    bool release = false;
    RoboImageCom() {
        const HRESULT started = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        release = SUCCEEDED(started);
    }
    ~RoboImageCom() {
        if (release) {
            CoUninitialize();
        }
    }
};

const char* RoboImageContainerName(const GUID& container, const void* bytes, std::size_t size) {
    if (container == GUID_ContainerFormatPng) return "png";
    if (container == GUID_ContainerFormatJpeg) return "jpeg";
    if (container == GUID_ContainerFormatWebp) return "webp";
    if (container == GUID_ContainerFormatGif) return "gif";
    if (container == GUID_ContainerFormatBmp) return "bmp";
    if (container == GUID_ContainerFormatTiff) return "tiff";
    if (container == GUID_ContainerFormatIco) return "ico";
    if (container == GUID_ContainerFormatWmp) return "wmp";
    if (container == GUID_ContainerFormatHeif) {
        const auto* data = static_cast<const unsigned char*>(bytes);
        const std::size_t limit = std::min(size, static_cast<std::size_t>(64));
        for (std::size_t index = 0; index + 4 <= limit; ++index) {
            if (data[index] == 'h' && data[index + 1] == 'e' && data[index + 2] == 'i' && data[index + 3] == 'c') {
                return "heic";
            }
        }
        return "heif";
    }
    return "bitmap";
}

int RoboImageQueryInt(IWICMetadataQueryReader* reader, const wchar_t* name, int fallback) {
    if (reader == nullptr) {
        return fallback;
    }
    PROPVARIANT value;
    PropVariantInit(&value);
    if (FAILED(reader->GetMetadataByName(name, &value))) {
        PropVariantClear(&value);
        return fallback;
    }
    int number = fallback;
    if (value.vt == VT_UI1) number = value.bVal;
    else if (value.vt == VT_UI2) number = value.uiVal;
    else if (value.vt == VT_UI4) number = static_cast<int>(value.ulVal);
    else if (value.vt == VT_I2) number = value.iVal;
    else if (value.vt == VT_I4) number = value.lVal;
    PropVariantClear(&value);
    return number;
}

bool RoboImageCopyFrame(IWICImagingFactory* factory, IWICBitmapFrameDecode* frame, std::vector<RoboU8>& rgba, UINT& width, UINT& height) {
    width = 0;
    height = 0;
    if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0) {
        return false;
    }
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter))) {
        return false;
    }
    if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        return false;
    }
    rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0);
    return SUCCEEDED(converter->CopyPixels(nullptr, width * 4u, static_cast<UINT>(rgba.size()), rgba.data()));
}

void RoboImageClearRect(std::vector<RoboU32>& canvas, int canvasWidth, int canvasHeight, int left, int top, int width, int height) {
    for (int y = top; y < top + height; ++y) {
        if (y < 0 || y >= canvasHeight) {
            continue;
        }
        for (int x = left; x < left + width; ++x) {
            if (x < 0 || x >= canvasWidth) {
                continue;
            }
            canvas[static_cast<std::size_t>(y) * static_cast<std::size_t>(canvasWidth) + static_cast<std::size_t>(x)] = 0;
        }
    }
}

void RoboImageBlit(std::vector<RoboU32>& canvas, int canvasWidth, int canvasHeight, int left, int top, const std::vector<RoboU8>& rgba, int width, int height) {
    for (int y = 0; y < height; ++y) {
        const int destY = top + y;
        if (destY < 0 || destY >= canvasHeight) {
            continue;
        }
        for (int x = 0; x < width; ++x) {
            const int destX = left + x;
            if (destX < 0 || destX >= canvasWidth) {
                continue;
            }
            const RoboU8* pixel = rgba.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4u;
            if (pixel[3] == 0) {
                continue;
            }
            canvas[static_cast<std::size_t>(destY) * static_cast<std::size_t>(canvasWidth) + static_cast<std::size_t>(destX)] =
                (static_cast<RoboU32>(pixel[3]) << 24) | (static_cast<RoboU32>(pixel[0]) << 16) | (static_cast<RoboU32>(pixel[1]) << 8) | pixel[2];
        }
    }
}

bool RoboImageDecodeWic(RoboImageRecord& image, const void* bytes, std::size_t size) {
    if (bytes == nullptr || size == 0 || size > 0xFFFFFFFFu) {
        return false;
    }
    RoboImageCom com;
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
        return false;
    }
    Microsoft::WRL::ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream))) {
        return false;
    }
    if (FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(static_cast<const BYTE*>(bytes)), static_cast<DWORD>(size)))) {
        return false;
    }
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder))) {
        return false;
    }
    UINT count = 0;
    if (FAILED(decoder->GetFrameCount(&count)) || count == 0) {
        return false;
    }
    if (count > 240u) {
        count = 240u;
    }
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> first;
    if (FAILED(decoder->GetFrame(0, &first))) {
        return false;
    }
    UINT screenWidth = 0;
    UINT screenHeight = 0;
    if (FAILED(first->GetSize(&screenWidth, &screenHeight))) {
        return false;
    }
    Microsoft::WRL::ComPtr<IWICMetadataQueryReader> catalog;
    if (SUCCEEDED(decoder->GetMetadataQueryReader(&catalog))) {
        const int logicalWidth = RoboImageQueryInt(catalog.Get(), L"/logscrdesc/Width", 0);
        const int logicalHeight = RoboImageQueryInt(catalog.Get(), L"/logscrdesc/Height", 0);
        if (logicalWidth > 0 && logicalHeight > 0) {
            screenWidth = static_cast<UINT>(logicalWidth);
            screenHeight = static_cast<UINT>(logicalHeight);
        }
    }
    if (screenWidth == 0 || screenHeight == 0 || screenWidth > static_cast<UINT>(ROBO_IMAGE_MAX_EDGE) || screenHeight > static_cast<UINT>(ROBO_IMAGE_MAX_EDGE)) {
        return false;
    }
    std::vector<RoboU32> canvas(static_cast<std::size_t>(screenWidth) * static_cast<std::size_t>(screenHeight), 0);
    std::vector<RoboU32> snapshot;
    std::vector<RoboU32> packed;
    std::vector<int> delays;
    int previousDisposal = 1;
    int previousLeft = 0;
    int previousTop = 0;
    int previousWidth = 0;
    int previousHeight = 0;
    for (UINT index = 0; index < count; ++index) {
        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame = index == 0 ? first : Microsoft::WRL::ComPtr<IWICBitmapFrameDecode>{};
        if (index != 0 && FAILED(decoder->GetFrame(index, &frame))) {
            return false;
        }
        int left = 0;
        int top = 0;
        int disposal = 1;
        int delayCs = -1;
        Microsoft::WRL::ComPtr<IWICMetadataQueryReader> reader;
        if (SUCCEEDED(frame->GetMetadataQueryReader(&reader))) {
            left = RoboImageQueryInt(reader.Get(), L"/imgdesc/Left", 0);
            top = RoboImageQueryInt(reader.Get(), L"/imgdesc/Top", 0);
            disposal = RoboImageQueryInt(reader.Get(), L"/grctlext/Disposal", 1);
            delayCs = RoboImageQueryInt(reader.Get(), L"/grctlext/Delay", -1);
        }
        if (index > 0 && previousDisposal == 2) {
            RoboImageClearRect(canvas, static_cast<int>(screenWidth), static_cast<int>(screenHeight), previousLeft, previousTop, previousWidth, previousHeight);
        } else if (index > 0 && previousDisposal == 3 && !snapshot.empty()) {
            canvas = snapshot;
        }
        if (disposal == 3) {
            snapshot = canvas;
        }
        std::vector<RoboU8> rgba;
        UINT frameWidth = 0;
        UINT frameHeight = 0;
        if (!RoboImageCopyFrame(factory.Get(), frame.Get(), rgba, frameWidth, frameHeight)) {
            return false;
        }
        RoboImageBlit(canvas, static_cast<int>(screenWidth), static_cast<int>(screenHeight), left, top, rgba, static_cast<int>(frameWidth), static_cast<int>(frameHeight));
        packed.insert(packed.end(), canvas.begin(), canvas.end());
        int delayMs = 0;
        if (delayCs > 0) {
            delayMs = delayCs * 10;
        } else if (delayCs == 0 || count > 1) {
            delayMs = 100;
        }
        delays.push_back(delayMs);
        previousDisposal = disposal;
        previousLeft = left;
        previousTop = top;
        previousWidth = static_cast<int>(frameWidth);
        previousHeight = static_cast<int>(frameHeight);
    }
    std::vector<RoboU8> firstRgba(static_cast<std::size_t>(screenWidth) * static_cast<std::size_t>(screenHeight) * 4u);
    for (std::size_t index = 0; index < canvas.size(); ++index) {
        const RoboU32 pixel = packed[index];
        firstRgba[index * 4] = static_cast<RoboU8>((pixel >> 16) & 0xFF);
        firstRgba[index * 4 + 1] = static_cast<RoboU8>((pixel >> 8) & 0xFF);
        firstRgba[index * 4 + 2] = static_cast<RoboU8>(pixel & 0xFF);
        firstRgba[index * 4 + 3] = static_cast<RoboU8>((pixel >> 24) & 0xFF);
    }
    GUID container{};
    const char* format = "bitmap";
    if (SUCCEEDED(decoder->GetContainerFormat(&container))) {
        format = RoboImageContainerName(container, bytes, size);
    }
    if (!RoboImageCommit(image, static_cast<int>(screenWidth), static_cast<int>(screenHeight), firstRgba.data(), static_cast<int>(screenWidth) * 4, format)) {
        return false;
    }
    image.frameCount = static_cast<int>(count);
    image.delays = std::move(delays);
    image.frames = std::move(packed);
    return true;
}

void RoboImageScaleSvg(NSVGimage* picture, float scaleX, float scaleY) {
    for (NSVGshape* shape = picture->shapes; shape != nullptr; shape = shape->next) {
        for (NSVGpath* path = shape->paths; path != nullptr; path = path->next) {
            for (int point = 0; point < path->npts; ++point) {
                path->pts[point * 2] *= scaleX;
                path->pts[point * 2 + 1] *= scaleY;
            }
        }
    }
    picture->width *= scaleX;
    picture->height *= scaleY;
}

bool RoboImagePaintVector(const RoboImageRecord& image, RoboImageFrame& frame, int destWidth, int destHeight, int originX, int originY) {
    if (image.vector.empty() || destWidth < 1 || destHeight < 1 || destWidth > ROBO_IMAGE_MAX_EDGE || destHeight > ROBO_IMAGE_MAX_EDGE) {
        return false;
    }
    std::string text = image.vector;
    NSVGimage* parsed = nsvgParse(text.data(), "px", 96.0f);
    if (parsed == nullptr || parsed->width <= 0.0f || parsed->height <= 0.0f) {
        nsvgDelete(parsed);
        return false;
    }
    const float scaleX = static_cast<float>(destWidth) / parsed->width;
    const float scaleY = static_cast<float>(destHeight) / parsed->height;
    RoboImageScaleSvg(parsed, scaleX, scaleY);
    NSVGrasterizer* raster = nsvgCreateRasterizer();
    if (raster == nullptr) {
        nsvgDelete(parsed);
        return false;
    }
    std::vector<RoboU8> rgba(static_cast<std::size_t>(destWidth) * static_cast<std::size_t>(destHeight) * 4u, 0);
    nsvgRasterize(raster, parsed, 0.0f, 0.0f, 1.0f, rgba.data(), destWidth, destHeight, destWidth * 4);
    nsvgDeleteRasterizer(raster);
    nsvgDelete(parsed);
    for (int y = 0; y < destHeight; ++y) {
        const int destY = originY + y;
        if (destY < 0 || destY >= frame.height) {
            continue;
        }
        for (int x = 0; x < destWidth; ++x) {
            const int destX = originX + x;
            if (destX < 0 || destX >= frame.width) {
                continue;
            }
            const RoboU8* pixel = rgba.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(destWidth) + static_cast<std::size_t>(x)) * 4u;
            frame.pixels[static_cast<std::size_t>(destY) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(destX)] =
                (static_cast<RoboU32>(pixel[3]) << 24) | (static_cast<RoboU32>(pixel[0]) << 16) | (static_cast<RoboU32>(pixel[1]) << 8) | pixel[2];
        }
    }
    return true;
}

#else

bool RoboImageDecodeWic(RoboImageRecord&, const void*, std::size_t) { return false; }

#endif

}  // namespace

bool RoboImageRead(const RoboXMLNode& node, RoboImageRecord& image, std::string& diagnostic) {
    image = RoboImageRecord{};
    diagnostic.clear();
    if (RoboImageLower(node.name) != "image") {
        diagnostic = RoboImageFail("R7304", "'" + node.name + "' is not an Image. Use Image.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!RoboImageKnown(entry.first)) {
            diagnostic = RoboImageFail("R7301", "Image attribute '" + entry.first + "' is unknown. Use source, fit, or alt.");
            return false;
        }
    }
    if (!node.children.empty()) {
        diagnostic = RoboImageFail("R7304", "Image does not take children.");
        return false;
    }
    image.source = RoboImageAttr(node, "source", "src");
    image.alt = RoboImageAttr(node, "alt", "description");
    image.fit = RoboImageAttr(node, "fit", "objectFit");
    if (image.fit.empty()) {
        image.fit = RoboXMLAttr(node, "object-fit");
    }
    if (!RoboImageFit(image.fit)) {
        diagnostic = RoboImageFail("R7302", "Image fit '" + image.fit + "' is unknown. Use contain, cover, fill, none, or scale-down.");
        return false;
    }
    image.onLoad = RoboXMLAttr(node, "onLoad");
    image.onError = RoboXMLAttr(node, "onError");
    const std::string width = RoboXMLAttr(node, "width");
    if (!width.empty()) {
        int number = 0;
        if (!RoboImageInteger(width, number) || number <= 0) {
            diagnostic = RoboImageFail("R7303", "Image width '" + width + "' is not a size. Use a whole number of pixels.");
            return false;
        }
        image.width = number;
        image.hasWidth = true;
    }
    const std::string height = RoboXMLAttr(node, "height");
    if (!height.empty()) {
        int number = 0;
        if (!RoboImageInteger(height, number) || number <= 0) {
            diagnostic = RoboImageFail("R7303", "Image height '" + height + "' is not a size. Use a whole number of pixels.");
            return false;
        }
        image.height = number;
        image.hasHeight = true;
    }
    return true;
}

bool RoboImageDecode(RoboImageRecord& image, const void* bytes, std::size_t size) {
    image.pixels.clear();
    image.frames.clear();
    image.delays.clear();
    image.vector.clear();
    image.sourceWidth = 0;
    image.sourceHeight = 0;
    image.frame = 0;
    image.frameCount = 0;
    image.format.clear();
    image.fired.clear();
    const RoboU8* raw = static_cast<const RoboU8*>(bytes);
    if (raw != nullptr && RoboImageStartsSvg(raw, size)) {
        if (RoboImageDecodeSvg(image, bytes, size)) {
            return true;
        }
        RoboImageFire(image, "OnError");
        return false;
    }
    if (RoboImageValidate(bytes, size)) {
        const RoboImageView view = RoboImageMakeView(bytes);
        const int width = static_cast<int>(view.header->width);
        const int height = static_cast<int>(view.header->height);
        const int stride = static_cast<int>(view.header->stride);
        if (RoboImageCommit(image, width, height, view.pixels, stride, "robi")) {
            return true;
        }
    }
    if (RoboImageDecodeWic(image, bytes, size)) {
        return true;
    }
    RoboImageFire(image, "OnError");
    return false;
}

void RoboImagePaint(const RoboImageRecord& image, RoboImageFrame& frame) {
    const int sourceWidth = image.sourceWidth;
    const int sourceHeight = image.sourceHeight;
    int frameWidth = image.hasWidth ? image.width : sourceWidth;
    int frameHeight = image.hasHeight ? image.height : sourceHeight;
    if (frameWidth < 0) {
        frameWidth = 0;
    }
    if (frameHeight < 0) {
        frameHeight = 0;
    }
    frame.width = frameWidth;
    frame.height = frameHeight;
    frame.pixels.assign(static_cast<std::size_t>(frameWidth) * static_cast<std::size_t>(frameHeight), 0);
    if (sourceWidth <= 0 || sourceHeight <= 0 || frameWidth <= 0 || frameHeight <= 0 || image.pixels.empty()) {
        return;
    }
    int destWidth = sourceWidth;
    int destHeight = sourceHeight;
    int originX = 0;
    int originY = 0;
    if (image.fit == "fill") {
        destWidth = frameWidth;
        destHeight = frameHeight;
    } else if (image.fit != "none") {
        const long long widthRatio = static_cast<long long>(frameWidth) * sourceHeight;
        const long long heightRatio = static_cast<long long>(frameHeight) * sourceWidth;
        const bool widthLimits = image.fit == "cover" ? widthRatio >= heightRatio : widthRatio <= heightRatio;
        if (widthLimits) {
            destWidth = frameWidth;
            destHeight = static_cast<int>(static_cast<long long>(sourceHeight) * frameWidth / sourceWidth);
        } else {
            destHeight = frameHeight;
            destWidth = static_cast<int>(static_cast<long long>(sourceWidth) * frameHeight / sourceHeight);
        }
        if (destWidth < 1) {
            destWidth = 1;
        }
        if (destHeight < 1) {
            destHeight = 1;
        }
        if (image.fit == "scale-down" && frameWidth >= sourceWidth && frameHeight >= sourceHeight) {
            destWidth = sourceWidth;
            destHeight = sourceHeight;
        }
        originX = (frameWidth - destWidth) / 2;
        originY = (frameHeight - destHeight) / 2;
    }
    if (RoboImagePaintVector(image, frame, destWidth, destHeight, originX, originY)) {
        return;
    }
    for (int y = 0; y < frameHeight; ++y) {
        const int localY = y - originY;
        if (localY < 0 || localY >= destHeight) {
            continue;
        }
        int sourceY = localY * sourceHeight / destHeight;
        if (sourceY >= sourceHeight) {
            sourceY = sourceHeight - 1;
        }
        for (int x = 0; x < frameWidth; ++x) {
            const int localX = x - originX;
            if (localX < 0 || localX >= destWidth) {
                continue;
            }
            int sourceX = localX * sourceWidth / destWidth;
            if (sourceX >= sourceWidth) {
                sourceX = sourceWidth - 1;
            }
            frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frameWidth) + static_cast<std::size_t>(x)] =
                image.pixels[static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(sourceWidth) + static_cast<std::size_t>(sourceX)];
        }
    }
}

bool RoboImageSelectFrame(RoboImageRecord& image, int index) {
    if (index < 0 || index >= image.frameCount || image.sourceWidth < 1 || image.sourceHeight < 1) {
        return false;
    }
    const std::size_t count = static_cast<std::size_t>(image.sourceWidth) * static_cast<std::size_t>(image.sourceHeight);
    const std::size_t begin = static_cast<std::size_t>(index) * count;
    if (image.frames.size() < begin + count) {
        return false;
    }
    image.pixels.assign(image.frames.begin() + static_cast<std::ptrdiff_t>(begin), image.frames.begin() + static_cast<std::ptrdiff_t>(begin + count));
    image.frame = index;
    return true;
}

int RoboImageFrameDelay(const RoboImageRecord& image, int index) {
    if (index < 0 || index >= static_cast<int>(image.delays.size())) {
        return -1;
    }
    return image.delays[static_cast<std::size_t>(index)];
}

bool RoboImagePixel(const RoboImageFrame& frame, int x, int y, RoboU32& color) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return false;
    }
    color = frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)];
    return true;
}
