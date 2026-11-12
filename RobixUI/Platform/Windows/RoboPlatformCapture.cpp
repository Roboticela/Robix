#include "RoboPlatformCapture.hpp"

#include "../../RoboGPU/RoboGPU.hpp"
#include "../../RoboShared/RoboHandoff.hpp"
#include "WindowsDocument.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <malloc.h>
#include <objbase.h>

namespace {
using RoboU16 = std::uint16_t;
#include "../../RoboGPU/RoboGPUGamma.inc"

const std::uint8_t* GammaFrom() {
    static std::uint8_t table[65536];
    static bool ready = false;
    if (!ready) {
        std::uint32_t start = 0;
        for (std::uint32_t channel = 0; channel < 256u; ++channel) {
            const std::uint32_t end = kRoboGPUToLinear[channel];
            for (std::uint32_t value = start; value <= end && value < 65536u; ++value) {
                table[value] = static_cast<std::uint8_t>(channel);
            }
            start = end + 1u;
        }
        ready = true;
    }
    return table;
}
}  // namespace

namespace Robix::UI {
namespace {

std::uint32_t MixChannel(std::uint32_t ink, std::uint32_t dst, std::uint32_t coverage, std::uint32_t shift) {
    const std::uint32_t source = (ink >> shift) & 255u;
    const std::uint32_t behind = (dst >> shift) & 255u;
    const std::uint32_t linear = (static_cast<std::uint32_t>(kRoboGPUToLinear[source]) * coverage +
                                  static_cast<std::uint32_t>(kRoboGPUToLinear[behind]) * (255u - coverage) + 127u) /
                                 255u;
    return GammaFrom()[linear > 65535u ? 65535u : linear];
}

std::uint32_t MixInk(std::uint32_t ink, std::uint32_t dst, std::uint32_t coverage) {
    if (coverage >= 255u) {
        return ink & 0x00FFFFFFu;
    }
    return MixChannel(ink, dst, coverage, 0) | (MixChannel(ink, dst, coverage, 8) << 8) |
           (MixChannel(ink, dst, coverage, 16) << 16);
}

struct RoboPlatformSink {
    std::vector<std::uint32_t> words;
    std::vector<std::uint32_t> column;
    int columnX = 1590;
    int columnW = 10;
    int columnRows = 0;
    bool run = false;
    int runX = 0;
    int runY = 0;
    int runW = 0;
    std::uint32_t runColor = 0;
    std::uint32_t runCoverage = 0;

    void Plot(int x, int y, std::uint32_t color) {
        if (columnRows <= 0 || y < 0 || y >= columnRows || x < columnX || x >= columnX + columnW) {
            return;
        }
        column[static_cast<std::size_t>(y * columnW + (x - columnX))] = color & 0x00FFFFFFu;
    }

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
        int left = x;
        int right = x + width;
        int top = y;
        int bottom = y + height;
        if (left < columnX) left = columnX;
        if (right > columnX + columnW) right = columnX + columnW;
        if (top < 0) top = 0;
        if (bottom > columnRows) bottom = columnRows;
        for (int row = top; row < bottom; ++row) {
            for (int col = left; col < right; ++col) {
                Plot(col, row, color);
            }
        }
    }

    void Blend(int x, int y, std::uint32_t color, std::uint32_t coverage) {
        color &= 0x00FFFFFFu;
        if (run && runY == y && runColor == color && runCoverage == coverage && runX + runW == x) {
            runW += 1;
            if (columnRows > 0 && y >= 0 && y < columnRows && x >= columnX && x < columnX + columnW) {
                const std::size_t at = static_cast<std::size_t>(y * columnW + (x - columnX));
                column[at] = MixInk(color, column[at], coverage);
            }
            return;
        }
        Flush();
        run = true;
        runX = x;
        runY = y;
        runW = 1;
        runColor = color;
        runCoverage = coverage;
        if (columnRows <= 0 || y < 0 || y >= columnRows || x < columnX || x >= columnX + columnW) {
            return;
        }
        const std::size_t at = static_cast<std::size_t>(y * columnW + (x - columnX));
        column[at] = MixInk(color, column[at], coverage);
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
    sink.columnRows = content;
    sink.column.assign(static_cast<std::size_t>(content) * static_cast<std::size_t>(sink.columnW), 0u);
    const RoboGPUPaintSink tap{ SinkFill, SinkBlend, &sink };
    RoboGPUSetPaintSink(&tap);
    PaintWindowsDocumentView(&gpu, 0, width, content);
    RoboGPUSetPaintSink(nullptr);
    sink.Flush();
    WindowPatch motionRect{};
    const int motionCount = WindowsDocumentMotionRects(&motionRect, 1);
    std::vector<std::uint32_t> motion;
    int motionFrames = 0;
    int motionDelay = 0;
    int probeX = 0;
    int probeY = 0;
    if (motionCount > 0 && motionRect.width > 0 && motionRect.height > 0 && motionRect.width <= 960 &&
        motionRect.height <= 540 && motionRect.x >= 0 && motionRect.y >= 0 && motionRect.x + motionRect.width <= width &&
        motionRect.y + motionRect.height <= content) {
        auto grab = [&](std::vector<std::uint32_t>& into) {
            into.assign(static_cast<std::size_t>(motionRect.width) * static_cast<std::size_t>(motionRect.height), 0u);
            for (int row = 0; row < motionRect.height; ++row) {
                const auto* line = reinterpret_cast<const std::uint32_t*>(
                    RoboFrameBufferRow(back, RoboGPUPitch(&gpu), motionRect.y + row));
                for (int col = 0; col < motionRect.width; ++col) {
                    into[static_cast<std::size_t>(row * motionRect.width + col)] = line[motionRect.x + col] & 0x00FFFFFFu;
                }
            }
        };
        auto paintMotion = [&]() {
            RoboGPUClear(&gpu, 0);
            PaintWindowsDocumentRect(&gpu, motionRect.x, motionRect.y, motionRect.width, motionRect.height);
        };
        std::vector<std::uint32_t> first;
        std::vector<std::uint32_t> next;
        WindowsDocumentSetPictureClock(0);
        paintMotion();
        grab(first);
        for (int step = 10; step <= 400 && motionDelay == 0; step += 10) {
            WindowsDocumentSetPictureClock(static_cast<unsigned>(step));
            paintMotion();
            grab(next);
            if (next != first) {
                motionDelay = step;
            }
        }
        if (motionDelay > 0) {
            motion = first;
            motionFrames = 1;
            for (int index = 1; index < 12; ++index) {
                WindowsDocumentSetPictureClock(static_cast<unsigned>(index * motionDelay));
                paintMotion();
                grab(next);
                if (next == first) {
                    break;
                }
                bool seen = false;
                for (int frame = 0; frame < motionFrames; ++frame) {
                    const std::size_t begin = static_cast<std::size_t>(frame) * first.size();
                    if (std::equal(next.begin(), next.end(), motion.begin() + static_cast<std::ptrdiff_t>(begin))) {
                        seen = true;
                        break;
                    }
                }
                if (seen) {
                    break;
                }
                motion.insert(motion.end(), next.begin(), next.end());
                motionFrames += 1;
            }
            const std::size_t span = first.size();
            if (motionFrames > 1) {
                for (std::size_t pixel = 0; pixel < span; ++pixel) {
                    if (motion[pixel] != motion[span + pixel]) {
                        probeX = static_cast<int>(pixel % static_cast<std::size_t>(motionRect.width));
                        probeY = static_cast<int>(pixel / static_cast<std::size_t>(motionRect.width));
                        break;
                    }
                }
            }
        }
        WindowsDocumentSetPictureClock(0);
    }
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
    paint.contentWidth = WindowsDocumentContentWidth();
    if (paint.contentWidth < width) {
        paint.contentWidth = width;
    }
    paint.columnX = sink.columnX;
    paint.columnW = sink.columnW;
    paint.column = std::move(sink.column);
    paint.words = std::move(sink.words);
    paint.motionX = motionRect.x;
    paint.motionY = motionRect.y;
    paint.motionW = motionRect.width;
    paint.motionH = motionRect.height;
    paint.motionFrames = motionFrames;
    paint.motionDelay = motionDelay;
    paint.probeX = probeX;
    paint.probeY = probeY;
    paint.motion = std::move(motion);
    cached = paint;
    cachedPath = path;
    cachedWidth = width;
    cachedHeight = height;
    return true;
}

int PillHits(int localX, int localY, int pillW, int pillH) {
    const int radius = (pillW < pillH ? pillW : pillH) * 2;
    const int left = radius;
    const int right = pillW * 4 - radius;
    const int top = radius;
    const int bottom = pillH * 4 - radius;
    const int limit = radius * radius;
    int hits = 0;
    for (int sampleY = 0; sampleY < 4; ++sampleY) {
        for (int sampleX = 0; sampleX < 4; ++sampleX) {
            const int px = localX * 4 + sampleX;
            const int py = localY * 4 + sampleY;
            const int cx = px < left ? left : (px > right ? right : px);
            const int cy = py < top ? top : (py > bottom ? bottom : py);
            const int dx = px - cx;
            const int dy = py - cy;
            if (dx * dx + dy * dy <= limit) {
                ++hits;
            }
        }
    }
    return hits;
}

int ArrowHits(int localX, int localY, int size, bool up) {
    const int span = size * 4;
    const int tipX = span / 2;
    const int base = span - 2;
    const int tipY = up ? 2 : base;
    const int baseY = up ? base : 2;
    auto cross = [](int x0, int y0, int x1, int y1, int x, int y) {
        return (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
    };
    int hits = 0;
    for (int sampleY = 0; sampleY < 4; ++sampleY) {
        for (int sampleX = 0; sampleX < 4; ++sampleX) {
            const int px = localX * 4 + sampleX;
            const int py = localY * 4 + sampleY;
            const int a = cross(tipX, tipY, 2, baseY, px, py);
            const int b = cross(2, baseY, span - 2, baseY, px, py);
            const int c = cross(span - 2, baseY, tipX, tipY, px, py);
            if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) {
                ++hits;
            }
        }
    }
    return hits;
}

void RoboPlatformReplay(std::uint8_t* pixels, int width, int height, const RoboPlatformPaint& paint, int scroll) {
    const std::vector<std::uint32_t>& words = paint.words;
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
    static std::vector<std::uint8_t> sheet;
    static std::uint32_t sheetStamp = 0;
    bool copied = false;
    if (width == paint.width && height == paint.height && paint.content > height) {
        const std::size_t bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(paint.content) * 4u;
        const std::uint32_t stamp = static_cast<std::uint32_t>(words.size()) ^
                                    (words.empty() ? 0u : words.front()) ^
                                    (words.empty() ? 0u : words.back()) ^
                                    static_cast<std::uint32_t>(paint.content);
        if (sheetStamp != stamp || sheet.size() != bytes) {
            sheet.assign(bytes, 0);
            RoboPlatformReplay(sheet.data(), width, paint.content, paint, 0);
            sheetStamp = stamp;
        }
        const std::size_t rowBytes = static_cast<std::size_t>(width) * 4u;
        for (int row = 0; row < height; ++row) {
            const int doc = row + scroll;
            if (doc < 0 || doc >= paint.content) {
                continue;
            }
            std::memcpy(pixels + static_cast<std::size_t>(row) * rowBytes,
                        sheet.data() + static_cast<std::size_t>(doc) * rowBytes, rowBytes);
        }
        copied = true;
    }
    std::size_t index = 0;
    while (!copied && index < words.size()) {
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
                    store(x + col, y, MixInk(color, load(x + col, y), coverage));
                }
            }
            index += 6;
            continue;
        }
        break;
    }
    if (!copied && paint.columnW > 0 && !paint.column.empty()) {
        for (int row = 0; row < height; ++row) {
            const int doc = row + scroll;
            if (doc < 0 || doc >= paint.content) {
                continue;
            }
            for (int col = 0; col < paint.columnW; ++col) {
                const std::size_t at = static_cast<std::size_t>(doc * paint.columnW + col);
                if (at < paint.column.size()) {
                    store(paint.columnX + col, row, paint.column[at]);
                }
            }
        }
    }
    const int maxScroll = paint.content > height ? paint.content - height : 0;
    if (maxScroll <= 0 || width < 16 || height < 32) {
        return;
    }
    const int thick = 6;
    const int edge = 2;
    const int arrow = 6;
    const int gap = 3;
    const int minThumb = 28;
    const int foot = paint.contentWidth > width ? thick + edge : 0;
    const int barX = width - edge - thick;
    const int barY = edge;
    const int barH = height - edge * 2 - foot;
    int trackY = barY + arrow + gap;
    int trackH = barH - (arrow + gap) * 2;
    if (trackH < minThumb) {
        trackH = barH > minThumb ? minThumb : barH;
        trackY = barY + (barH - trackH) / 2;
    }
    int thumbH = static_cast<int>(static_cast<long long>(trackH) * height / (paint.content > 0 ? paint.content : 1));
    if (thumbH < minThumb) {
        thumbH = minThumb;
    }
    if (thumbH > trackH) {
        thumbH = trackH;
    }
    const int travel = trackH - thumbH > 1 ? trackH - thumbH : 1;
    int thumbY = trackY;
    if (maxScroll > 0) {
        thumbY += static_cast<int>(static_cast<long long>(scroll) * travel / maxScroll);
    }
    const std::uint32_t trackColor = 0x38383Au;
    const std::uint32_t thumbColor = 0x747477u;
    auto cover = [&](int x, int y, int coverage, std::uint32_t color) {
        if (coverage <= 0 || x < 0 || y < 0 || x >= width || y >= height) {
            return;
        }
        if (coverage >= 255) {
            store(x, y, color);
            return;
        }
        store(x, y, blend(color, load(x, y), static_cast<std::uint32_t>(coverage)));
    };
    auto pill = [&](int x, int y, int pillW, int pillH, std::uint32_t color) {
        for (int row = 0; row < pillH; ++row) {
            for (int col = 0; col < pillW; ++col) {
                cover(x + col, y + row, PillHits(col, row, pillW, pillH) * 255 / 16, color);
            }
        }
    };
    auto triangle = [&](int x, int y, bool up) {
        for (int row = 0; row < arrow; ++row) {
            for (int col = 0; col < arrow; ++col) {
                cover(x + col, y + row, ArrowHits(col, row, arrow, up) * 255 / 16, thumbColor);
            }
        }
    };
    pill(barX, trackY, thick, trackH, trackColor);
    pill(barX, thumbY, thick, thumbH, thumbColor);
    triangle(barX, barY, true);
    triangle(barX, barY + barH - arrow, false);
}

int RoboPlatformMotionIndex(const RoboPlatformPaint& paint, int clock) {
    if (paint.motionFrames <= 1 || paint.motionDelay <= 0) {
        return 0;
    }
    int index = clock / paint.motionDelay;
    if (index < 0) {
        index = 0;
    }
    return index % paint.motionFrames;
}

int RoboPlatformBlitMotion(std::uint8_t* pixels, int width, int height, const RoboPlatformPaint& paint, int scroll, int clock) {
    if (pixels == nullptr || paint.motionFrames <= 1 || paint.motionW <= 0 || paint.motionH <= 0) {
        return 0;
    }
    const int index = RoboPlatformMotionIndex(paint, clock);
    const std::size_t span = static_cast<std::size_t>(paint.motionW) * static_cast<std::size_t>(paint.motionH);
    const std::size_t begin = static_cast<std::size_t>(index) * span;
    if (begin + span > paint.motion.size()) {
        return 0;
    }
    for (int row = 0; row < paint.motionH; ++row) {
        const int y = paint.motionY + row - scroll;
        if (y < 0 || y >= height) {
            continue;
        }
        for (int col = 0; col < paint.motionW; ++col) {
            const int x = paint.motionX + col;
            if (x < 0 || x >= width) {
                continue;
            }
            const std::uint32_t color = paint.motion[begin + static_cast<std::size_t>(row * paint.motionW + col)];
            const std::size_t at = static_cast<std::size_t>((y * width + x) * 4);
            pixels[at] = static_cast<std::uint8_t>(color);
            pixels[at + 1] = static_cast<std::uint8_t>(color >> 8);
            pixels[at + 2] = static_cast<std::uint8_t>(color >> 16);
            pixels[at + 3] = 0;
        }
    }
    return 1;
}

int RoboPlatformSample(std::uint8_t* pixels, int width, int height, const RoboPlatformPaint& paint, int clock) {
    if (paint.motionFrames <= 1 || paint.motionW <= 0 || paint.motionH <= 0) {
        return 0;
    }
    RoboPlatformBlitMotion(pixels, width, height, paint, 0, clock);
    const int index = RoboPlatformMotionIndex(paint, clock);
    const std::size_t span = static_cast<std::size_t>(paint.motionW) * static_cast<std::size_t>(paint.motionH);
    const std::size_t at = static_cast<std::size_t>(index) * span +
                           static_cast<std::size_t>(paint.probeY * paint.motionW + paint.probeX);
    if (at >= paint.motion.size()) {
        return 0;
    }
    return static_cast<int>(paint.motion[at]);
}

int RoboPlatformSpot(const RoboPlatformPaint& paint, int kind) {
    if (kind == 0) return paint.motionX;
    if (kind == 1) return paint.motionY;
    if (kind == 2) return paint.motionW;
    if (kind == 3) return paint.motionH;
    if (kind == 4) return paint.motionFrames;
    if (kind == 5) return paint.motionDelay;
    if (kind == 6) return paint.probeX;
    if (kind == 7) return paint.probeY;
    return 0;
}

}  // namespace Robix::UI
