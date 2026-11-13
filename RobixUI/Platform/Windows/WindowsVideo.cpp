#include "WindowsVideo.hpp"

#include <cstddef>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

namespace Robix::UI {
namespace {

bool gMediaReady = false;
std::wstring gOpenPath;
IMFSourceReader* gReader = nullptr;
int gTypeWidth = 0;
int gTypeHeight = 0;
int gWidth = 0;
int gHeight = 0;
int gStride = 0;
int gHeldMs = -1;
std::vector<RoboU32> gHeld;

void Release(IUnknown* object) {
    if (object != nullptr) {
        object->Release();
    }
}

void CloseReader() {
    Release(gReader);
    gReader = nullptr;
    gOpenPath.clear();
    gTypeWidth = 0;
    gTypeHeight = 0;
    gWidth = 0;
    gHeight = 0;
    gStride = 0;
    gHeldMs = -1;
    gHeld.clear();
}

void RememberType(IMFMediaType* current);

bool OpenReader(const std::filesystem::path& file) {
    const std::wstring path = file.wstring();
    if (gReader != nullptr && gOpenPath == path && gTypeWidth > 0 && gTypeHeight > 0) {
        return true;
    }
    CloseReader();
    const HRESULT started = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    (void)started;
    if (!gMediaReady) {
        gMediaReady = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET));
    }
    if (!gMediaReady) {
        return false;
    }
    IMFAttributes* attributes = nullptr;
    if (FAILED(MFCreateAttributes(&attributes, 4)) || attributes == nullptr) {
        return false;
    }
    attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    IMFSourceReader* reader = nullptr;
    const HRESULT opened = MFCreateSourceReaderFromURL(path.c_str(), attributes, &reader);
    Release(attributes);
    if (FAILED(opened) || reader == nullptr) {
        return false;
    }
    IMFMediaType* requested = nullptr;
    if (FAILED(MFCreateMediaType(&requested)) || requested == nullptr) {
        Release(reader);
        return false;
    }
    requested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    requested->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    const HRESULT typed = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, requested);
    Release(requested);
    if (FAILED(typed)) {
        Release(reader);
        return false;
    }
    IMFMediaType* current = nullptr;
    UINT32 width = 0;
    UINT32 height = 0;
    if (FAILED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &current)) || current == nullptr ||
        FAILED(MFGetAttributeSize(current, MF_MT_FRAME_SIZE, &width, &height)) || width == 0 || height == 0 || width > 4096 || height > 4096) {
        Release(current);
        Release(reader);
        return false;
    }
    RememberType(current);
    Release(current);
    if (gTypeWidth < 1 || gTypeHeight < 1) {
        Release(reader);
        return false;
    }
    gReader = reader;
    gOpenPath = path;
    return true;
}

void RememberType(IMFMediaType* current) {
    if (current == nullptr) {
        return;
    }
    UINT32 width = 0;
    UINT32 height = 0;
    if (SUCCEEDED(MFGetAttributeSize(current, MF_MT_FRAME_SIZE, &width, &height)) && width > 0 && height > 0 && width <= 4096 && height <= 4096) {
        gTypeWidth = static_cast<int>(width);
        gTypeHeight = static_cast<int>(height);
    }
    UINT32 rawStride = 0;
    if (SUCCEEDED(current->GetUINT32(MF_MT_DEFAULT_STRIDE, &rawStride))) {
        gStride = static_cast<int>(static_cast<INT32>(rawStride));
    } else {
        gStride = gTypeWidth * 4;
    }
    MFVideoArea area{};
    UINT32 areaBytes = 0;
    if (SUCCEEDED(current->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, reinterpret_cast<UINT8*>(&area), sizeof(area), &areaBytes)) &&
        areaBytes >= sizeof(area) && area.Area.cx > 0 && area.Area.cy > 0 && area.Area.cx <= gTypeWidth && area.Area.cy <= gTypeHeight) {
        gTypeWidth = area.Area.cx;
        gTypeHeight = area.Area.cy;
    } else if ((gTypeHeight % 16) == 8 && gTypeHeight > 16) {
        gTypeHeight -= 8;
    }
}

bool CopySample(IMFSample* sample) {
    IMFMediaBuffer* buffer = nullptr;
    if (FAILED(sample->ConvertToContiguousBuffer(&buffer)) || buffer == nullptr) {
        return false;
    }
    BYTE* data = nullptr;
    DWORD length = 0;
    const bool locked = SUCCEEDED(buffer->Lock(&data, nullptr, &length)) && data != nullptr;
    bool copied = false;
    if (locked && gTypeWidth > 0 && gTypeHeight > 0) {
        int stride = gStride == 0 ? gTypeWidth * 4 : gStride;
        const int rowBytes = stride < 0 ? -stride : stride;
        const bool packed = rowBytes == gTypeWidth * 4 && length >= static_cast<DWORD>(gTypeWidth) * static_cast<DWORD>(gTypeHeight) * 4u;
        const bool strided = rowBytes >= gTypeWidth * 4 && length >= static_cast<DWORD>(rowBytes) * static_cast<DWORD>(gTypeHeight);
        if (packed || strided) {
            const int used = packed ? gTypeWidth * 4 : rowBytes;
            const BYTE* top = stride < 0 ? data + static_cast<std::size_t>(gTypeHeight - 1) * static_cast<std::size_t>(used) : data;
            const int step = stride < 0 ? -used : used;
            const bool half = gTypeWidth >= 1280 && gTypeHeight >= 720 && (gTypeWidth % 2) == 0 && (gTypeHeight % 2) == 0;
            const int outWidth = half ? gTypeWidth / 2 : gTypeWidth;
            const int outHeight = half ? gTypeHeight / 2 : gTypeHeight;
            gHeld.resize(static_cast<std::size_t>(outWidth) * static_cast<std::size_t>(outHeight));
            for (int y = 0; y < outHeight; ++y) {
                const int sourceY = half ? y * 2 : y;
                const BYTE* row = top + static_cast<std::ptrdiff_t>(sourceY) * step;
                const BYTE* below = half ? row + step : row;
                for (int x = 0; x < outWidth; ++x) {
                    const int sourceX = half ? x * 2 : x;
                    const BYTE* pixel = row + static_cast<std::size_t>(sourceX) * 4;
                    int red = pixel[2];
                    int green = pixel[1];
                    int blue = pixel[0];
                    if (half) {
                        const BYTE* across = pixel + 4;
                        const BYTE* lower = below + static_cast<std::size_t>(sourceX) * 4;
                        red += across[2] + lower[2] + lower[6];
                        green += across[1] + lower[1] + lower[5];
                        blue += across[0] + lower[0] + lower[4];
                        red /= 4;
                        green /= 4;
                        blue /= 4;
                    }
                    gHeld[static_cast<std::size_t>(y) * static_cast<std::size_t>(outWidth) + static_cast<std::size_t>(x)] =
                        0xFF000000u | (static_cast<RoboU32>(red) << 16) | (static_cast<RoboU32>(green) << 8) | static_cast<RoboU32>(blue);
                }
            }
            gWidth = outWidth;
            gHeight = outHeight;
            copied = true;
        }
    }
    if (locked) {
        buffer->Unlock();
    }
    Release(buffer);
    return copied;
}

bool ReadForward(int& sampleMs) {
    for (int attempt = 0; attempt < 8; ++attempt) {
        DWORD flags = 0;
        LONGLONG stamp = 0;
        IMFSample* sample = nullptr;
        if (FAILED(gReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, nullptr, &flags, &stamp, &sample))) {
            return false;
        }
        if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0) {
            IMFMediaType* current = nullptr;
            if (SUCCEEDED(gReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &current))) {
                RememberType(current);
            }
            Release(current);
        }
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0 && sample == nullptr) {
            return false;
        }
        if (sample == nullptr) {
            continue;
        }
        const bool copied = CopySample(sample);
        Release(sample);
        if (!copied) {
            return false;
        }
        sampleMs = static_cast<int>(stamp / 10000);
        if (sampleMs < 0) {
            sampleMs = 0;
        }
        return true;
    }
    return false;
}

void SeekReader(int timeMs) {
    PROPVARIANT position{};
    position.vt = VT_I8;
    position.hVal.QuadPart = static_cast<LONGLONG>(timeMs) * 10000LL;
    gReader->SetCurrentPosition(GUID_NULL, position);
    gHeldMs = -1;
    gHeld.clear();
}

}  // namespace

bool WindowsVideoFrame(const std::filesystem::path& file, int timeMs, std::vector<RoboU32>& pixels, int& width, int& height, int& sampleMs) {
    pixels.clear();
    width = 0;
    height = 0;
    sampleMs = 0;
    if (file.empty() || !OpenReader(file)) {
        return false;
    }
    if (timeMs < 0) {
        timeMs = 0;
    }
    const bool jump = gHeldMs < 0 || timeMs + 40 < gHeldMs;
    if (jump) {
        SeekReader(timeMs);
    } else if (timeMs <= gHeldMs + 30 && !gHeld.empty()) {
        width = gWidth;
        height = gHeight;
        sampleMs = gHeldMs;
        return true;
    }
    int seen = gHeldMs;
    int nextMs = 0;
    if (!ReadForward(nextMs)) {
        SeekReader(0);
        if (!ReadForward(nextMs)) {
            nextMs = -1;
        }
    }
    if (nextMs >= 0) {
        seen = nextMs;
        gHeldMs = nextMs;
    }
    (void)seen;
    if (gHeld.empty()) {
        return false;
    }
    pixels = gHeld;
    width = gWidth;
    height = gHeight;
    sampleMs = gHeldMs < 0 ? 0 : gHeldMs;
    return true;
}

}  // namespace Robix::UI
