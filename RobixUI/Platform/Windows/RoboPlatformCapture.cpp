#include "RoboPlatformCapture.hpp"

#include "../../RoboGPU/RoboGPU.hpp"
#include "../../RoboShared/RoboHandoff.hpp"
#include "WindowsDocument.hpp"

#include <malloc.h>
#include <objbase.h>

namespace Robix::UI {
namespace {

struct RoboPlatformSink {
    std::vector<std::uint32_t> words;
    bool run = false;
    int runX = 0;
    int runY = 0;
    int runW = 0;
    std::uint32_t runColor = 0;
    std::uint32_t runCoverage = 0;

    void Flush() {
        if (!run) {
            return;
        }
        words.push_back(2);
        words.push_back(static_cast<std::uint32_t>(runX));
        words.push_back(static_cast<std::uint32_t>(runY));
        words.push_back(static_cast<std::uint32_t>(runW));
        words.push_back(runCoverage);
        words.push_back(runColor & 0x00FFFFFFu);
        run = false;
    }

    void Fill(int x, int y, int width, int height, std::uint32_t color) {
        Flush();
        if (width <= 0 || height <= 0) {
            return;
        }
        words.push_back(1);
        words.push_back(static_cast<std::uint32_t>(x));
        words.push_back(static_cast<std::uint32_t>(y));
        words.push_back(static_cast<std::uint32_t>(width));
        words.push_back(static_cast<std::uint32_t>(height));
        words.push_back(color & 0x00FFFFFFu);
    }

    void Blend(int x, int y, std::uint32_t color, std::uint32_t coverage) {
        color &= 0x00FFFFFFu;
        if (run && runY == y && runColor == color && runCoverage == coverage && runX + runW == x) {
            runW += 1;
            return;
        }
        Flush();
        run = true;
        runX = x;
        runY = y;
        runW = 1;
        runColor = color;
        runCoverage = coverage;
    }
};

void SinkFill(void* context, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height, RoboU32 color) {
    static_cast<RoboPlatformSink*>(context)->Fill(x, y, width, height, color);
}

void SinkBlend(void* context, RoboI32 x, RoboI32 y, RoboU32 color, RoboU8 coverage) {
    static_cast<RoboPlatformSink*>(context)->Blend(x, y, color, coverage);
}

}  // namespace

bool RoboPlatformCapture(const std::string& path, int width, int height, RoboPlatformPaint& paint, std::string& error) {
    static RoboPlatformPaint cached;
    static std::string cachedPath;
    static int cachedWidth = 0;
    static int cachedHeight = 0;
    paint = {};
    if (path.empty() || width <= 0 || height <= 0) {
        error = "the document path is empty";
        return false;
    }
    if (cachedPath == path && cachedWidth == width && cachedHeight == height && !cached.words.empty()) {
        paint = cached;
        return true;
    }
    const HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool ownsApartment = apartment == S_OK;
    int content = height;
    if (LayoutWindowsDocument(path, false, width, height, 96, -1, -1, -1, -1, &content, nullptr) < 0) {
        if (ownsApartment) {
            CoUninitialize();
        }
        error = "the platform could not lay out '" + path + "'";
        return false;
    }
    if (content < height) {
        content = height;
    }
    if (content > 8192) {
        content = 8192;
    }
    const std::uint64_t bytes = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(content) * 4u;
    auto* back = static_cast<std::uint8_t*>(_aligned_malloc(static_cast<std::size_t>(bytes), 16));
    if (back == nullptr) {
        if (ownsApartment) {
            CoUninitialize();
        }
        error = "the platform framebuffer is empty";
        return false;
    }
    RoboGPU gpu{};
    RoboHandoff handoff{};
    handoff.magic = ROBO_HANDOFF_MAGIC;
    handoff.version = ROBO_HANDOFF_VERSION;
    handoff.frameBuffer.base = reinterpret_cast<RoboU64>(back);
    handoff.frameBuffer.width = static_cast<RoboU32>(width);
    handoff.frameBuffer.height = static_cast<RoboU32>(content);
    handoff.frameBuffer.pixelsPerScanLine = static_cast<RoboU32>(width);
    handoff.frameBuffer.bytesPerPixel = 4;
    handoff.frameBuffer.redMask = 0x00FF0000u;
    handoff.frameBuffer.greenMask = 0x0000FF00u;
    handoff.frameBuffer.blueMask = 0x000000FFu;
    RoboGPUInitFromHandoff(&gpu, &handoff, back, bytes);
    RoboPlatformSink sink;
    const RoboGPUPaintSink tap{ SinkFill, SinkBlend, &sink };
    RoboGPUSetPaintSink(&tap);
    PaintWindowsDocumentView(&gpu, 0, width, content);
    RoboGPUSetPaintSink(nullptr);
    sink.Flush();
    _aligned_free(back);
    if (ownsApartment) {
        CoUninitialize();
    }
    if (sink.words.empty()) {
        error = "the platform painted nothing for '" + path + "'";
        return false;
    }
    paint.width = width;
    paint.height = height;
    paint.content = content;
    paint.words = std::move(sink.words);
    cached = paint;
    cachedPath = path;
    cachedWidth = width;
    cachedHeight = height;
    return true;
}

void RoboPlatformReplay(std::uint8_t* pixels, int width, int height, const std::vector<std::uint32_t>& words, int scroll) {
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return;
    }
    auto store = [&](int x, int y, std::uint32_t color) {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return;
        }
        const std::size_t at = static_cast<std::size_t>((y * width + x) * 4);
        pixels[at] = static_cast<std::uint8_t>(color);
        pixels[at + 1] = static_cast<std::uint8_t>(color >> 8);
        pixels[at + 2] = static_cast<std::uint8_t>(color >> 16);
        pixels[at + 3] = 0;
    };
    auto load = [&](int x, int y) -> std::uint32_t {
        const std::size_t at = static_cast<std::size_t>((y * width + x) * 4);
        return static_cast<std::uint32_t>(pixels[at]) | (static_cast<std::uint32_t>(pixels[at + 1]) << 8) |
               (static_cast<std::uint32_t>(pixels[at + 2]) << 16);
    };
    auto blend = [&](std::uint32_t ink, std::uint32_t dst, std::uint32_t coverage) {
        auto channel = [&](std::uint32_t shift) {
            const std::uint32_t source = (ink >> shift) & 255u;
            const std::uint32_t behind = (dst >> shift) & 255u;
            return (source * coverage + behind * (255u - coverage)) / 255u;
        };
        return channel(0) | (channel(8) << 8) | (channel(16) << 16);
    };
    std::size_t index = 0;
    while (index < words.size()) {
        const std::uint32_t kind = words[index];
        if (kind == 1 && index + 5 < words.size()) {
            const int x = static_cast<int>(words[index + 1]);
            int y = static_cast<int>(words[index + 2]) - scroll;
            const int w = static_cast<int>(words[index + 3]);
            int h = static_cast<int>(words[index + 4]);
            const std::uint32_t color = words[index + 5];
            if (y < 0) {
                h += y;
                y = 0;
            }
            if (y + h > height) {
                h = height - y;
            }
            for (int row = 0; row < h; ++row) {
                for (int col = 0; col < w; ++col) {
                    store(x + col, y + row, color);
                }
            }
            index += 6;
            continue;
        }
        if (kind == 2 && index + 5 < words.size()) {
            const int x = static_cast<int>(words[index + 1]);
            const int y = static_cast<int>(words[index + 2]) - scroll;
            const int w = static_cast<int>(words[index + 3]);
            const std::uint32_t coverage = words[index + 4];
            const std::uint32_t color = words[index + 5];
            for (int col = 0; col < w; ++col) {
                if (x + col < 0 || y < 0 || x + col >= width || y >= height) {
                    continue;
                }
                if (coverage >= 255) {
                    store(x + col, y, color);
                } else {
                    store(x + col, y, blend(color, load(x + col, y), coverage));
                }
            }
            index += 6;
            continue;
        }
        break;
    }
}

}  // namespace Robix::UI
