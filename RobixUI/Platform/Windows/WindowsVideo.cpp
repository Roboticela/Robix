#include "WindowsVideo.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>

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
#include <mmsystem.h>
#include <cstdint>

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

std::mutex gMutex;
std::condition_variable gCv;
bool gStarted = false;
bool gStop = false;
bool gWake = false;
std::wstring gWantPath;
int gWantMs = 0;
int gWantW = 0;
int gWantH = 0;
int gWantDuration = 0;
std::vector<RoboU32> gPublish;
int gPublishW = 0;
int gPublishH = 0;
int gPublishMs = -1;
int gPublishGen = 0;
int gServedGen = -1;
bool gWantSound = false;
HWAVEOUT gSound = nullptr;
WAVEHDR gSoundHeader{};
std::vector<std::int16_t> gSoundPcm;
int gSoundRate = 0;
int gSoundChannels = 0;
std::wstring gSoundPath;
std::wstring gSoundMiss;

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
    attributes->SetUINT32(MF_LOW_LATENCY, TRUE);
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
            gHeld.resize(static_cast<std::size_t>(gTypeWidth) * static_cast<std::size_t>(gTypeHeight));
            for (int y = 0; y < gTypeHeight; ++y) {
                const BYTE* row = top + static_cast<std::ptrdiff_t>(y) * step;
                RoboU32* dest = gHeld.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(gTypeWidth);
                for (int x = 0; x < gTypeWidth; ++x) {
                    const BYTE* pixel = row + static_cast<std::size_t>(x) * 4;
                    dest[x] = 0xFF000000u | (static_cast<RoboU32>(pixel[2]) << 16) | (static_cast<RoboU32>(pixel[1]) << 8) | pixel[0];
                }
            }
            gWidth = gTypeWidth;
            gHeight = gTypeHeight;
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

void ScaleArea(const std::vector<RoboU32>& source, int sourceWidth, int sourceHeight, int destWidth, int destHeight, std::vector<RoboU32>& dest) {
    dest.resize(static_cast<std::size_t>(destWidth) * static_cast<std::size_t>(destHeight));
    if (destWidth == sourceWidth && destHeight == sourceHeight) {
        dest = source;
        return;
    }
    std::vector<int> x0(static_cast<std::size_t>(destWidth));
    std::vector<int> x1(static_cast<std::size_t>(destWidth));
    for (int x = 0; x < destWidth; ++x) {
        x0[static_cast<std::size_t>(x)] = x * sourceWidth / destWidth;
        int end = (x + 1) * sourceWidth / destWidth;
        if (end <= x0[static_cast<std::size_t>(x)]) {
            end = x0[static_cast<std::size_t>(x)] + 1;
        }
        if (end > sourceWidth) {
            end = sourceWidth;
        }
        x1[static_cast<std::size_t>(x)] = end;
    }
    for (int y = 0; y < destHeight; ++y) {
        int y0 = y * sourceHeight / destHeight;
        int y1 = (y + 1) * sourceHeight / destHeight;
        if (y1 <= y0) {
            y1 = y0 + 1;
        }
        if (y1 > sourceHeight) {
            y1 = sourceHeight;
        }
        RoboU32* row = dest.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(destWidth);
        for (int x = 0; x < destWidth; ++x) {
            const int left = x0[static_cast<std::size_t>(x)];
            const int right = x1[static_cast<std::size_t>(x)];
            int red = 0;
            int green = 0;
            int blue = 0;
            int count = 0;
            for (int sampleY = y0; sampleY < y1; ++sampleY) {
                const RoboU32* sourceRow = source.data() + static_cast<std::size_t>(sampleY) * static_cast<std::size_t>(sourceWidth);
                for (int sampleX = left; sampleX < right; ++sampleX) {
                    const RoboU32 pixel = sourceRow[sampleX];
                    red += static_cast<int>((pixel >> 16) & 255u);
                    green += static_cast<int>((pixel >> 8) & 255u);
                    blue += static_cast<int>(pixel & 255u);
                    ++count;
                }
            }
            if (count < 1) {
                count = 1;
            }
            row[x] = 0xFF000000u | (static_cast<RoboU32>(red / count) << 16) | (static_cast<RoboU32>(green / count) << 8) |
                     static_cast<RoboU32>(blue / count);
        }
    }
}

void FitSize(int destWidth, int destHeight, int& outWidth, int& outHeight) {
    outWidth = gWidth;
    outHeight = gHeight;
    if (destWidth < 1 || destHeight < 1 || gWidth < 1 || gHeight < 1) {
        return;
    }
    if (destWidth >= gWidth && destHeight >= gHeight) {
        return;
    }
    if (static_cast<long long>(destWidth) * gHeight <= static_cast<long long>(destHeight) * gWidth) {
        outWidth = destWidth;
        outHeight = std::max(1, gHeight * destWidth / gWidth);
    } else {
        outHeight = destHeight;
        outWidth = std::max(1, gWidth * destHeight / gHeight);
    }
}

bool PullFrame(int& sampleMs) {
    if (!ReadForward(sampleMs)) {
        SeekReader(0);
        if (!ReadForward(sampleMs)) {
            return false;
        }
    }
    gHeldMs = sampleMs;
    return true;
}

void StopSound() {
    if (gSound == nullptr) {
        return;
    }
    waveOutReset(gSound);
    if ((gSoundHeader.dwFlags & WHDR_PREPARED) != 0) {
        waveOutUnprepareHeader(gSound, &gSoundHeader, sizeof(gSoundHeader));
    }
    waveOutClose(gSound);
    gSound = nullptr;
    gSoundHeader = {};
}

void StartSound(int fromMs) {
    if (gSoundPcm.empty() || gSoundRate < 8000 || gSoundChannels < 1) {
        return;
    }
    if (gSound != nullptr) {
        waveOutReset(gSound);
        if ((gSoundHeader.dwFlags & WHDR_PREPARED) != 0) {
            waveOutUnprepareHeader(gSound, &gSoundHeader, sizeof(gSoundHeader));
        }
        gSoundHeader = {};
    } else {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = static_cast<WORD>(gSoundChannels);
        format.nSamplesPerSec = static_cast<DWORD>(gSoundRate);
        format.wBitsPerSample = 16;
        format.nBlockAlign = static_cast<WORD>(gSoundChannels * 2);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        if (waveOutOpen(&gSound, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            gSound = nullptr;
            return;
        }
    }
    std::size_t offset = 0;
    if (fromMs > 0) {
        offset = static_cast<std::size_t>(fromMs) * static_cast<std::size_t>(gSoundRate) / 1000u * static_cast<std::size_t>(gSoundChannels);
    }
    if (offset >= gSoundPcm.size()) {
        offset = 0;
    }
    gSoundHeader.lpData = reinterpret_cast<LPSTR>(gSoundPcm.data() + offset);
    gSoundHeader.dwBufferLength = static_cast<DWORD>((gSoundPcm.size() - offset) * sizeof(std::int16_t));
    if (waveOutPrepareHeader(gSound, &gSoundHeader, sizeof(gSoundHeader)) != MMSYSERR_NOERROR ||
        waveOutWrite(gSound, &gSoundHeader, sizeof(gSoundHeader)) != MMSYSERR_NOERROR) {
        StopSound();
    }
}

bool LoadSound(const std::filesystem::path& file) {
    IMFSourceReader* reader = nullptr;
    if (FAILED(MFCreateSourceReaderFromURL(file.wstring().c_str(), nullptr, &reader)) || reader == nullptr) {
        return false;
    }
    reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE);
    IMFMediaType* requested = nullptr;
    if (FAILED(MFCreateMediaType(&requested)) || requested == nullptr) {
        reader->Release();
        return false;
    }
    requested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    requested->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    requested->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    const bool typed = SUCCEEDED(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, requested));
    requested->Release();
    if (!typed) {
        reader->Release();
        return false;
    }
    IMFMediaType* current = nullptr;
    UINT32 rate = 0;
    UINT32 channels = 0;
    if (SUCCEEDED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &current)) && current != nullptr) {
        current->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        current->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
        current->Release();
    }
    std::vector<std::int16_t> samples;
    const std::size_t cap = (rate > 0 && channels > 0) ? static_cast<std::size_t>(rate) * channels * 30u : 48000u * 2u * 30u;
    while (samples.size() < cap) {
        IMFSample* sample = nullptr;
        DWORD flags = 0;
        if (FAILED(reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &flags, nullptr, &sample))) {
            break;
        }
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
            Release(sample);
            break;
        }
        if (sample == nullptr) {
            continue;
        }
        IMFMediaBuffer* buffer = nullptr;
        if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer)) && buffer != nullptr) {
            BYTE* data = nullptr;
            DWORD length = 0;
            if (SUCCEEDED(buffer->Lock(&data, nullptr, &length)) && data != nullptr && (length / 2) > 0) {
                const auto* pcm = reinterpret_cast<const std::int16_t*>(data);
                samples.insert(samples.end(), pcm, pcm + (length / sizeof(std::int16_t)));
                buffer->Unlock();
            }
            Release(buffer);
        }
        Release(sample);
    }
    reader->Release();
    if (samples.empty() || rate == 0 || channels == 0) {
        return false;
    }
    gSoundPcm = std::move(samples);
    gSoundRate = static_cast<int>(rate);
    gSoundChannels = static_cast<int>(channels);
    return true;
}

void KeepSound(const std::wstring& path, bool want, int fromMs, bool rewind) {
    if (!want) {
        StopSound();
        return;
    }
    const std::filesystem::path file(path);
    if (path != gSoundPath) {
        StopSound();
        gSoundPcm.clear();
        gSoundRate = 0;
        gSoundChannels = 0;
        gSoundPath = path;
        gSoundMiss.clear();
        if (gSoundMiss == path || !LoadSound(file)) {
            gSoundMiss = path;
            return;
        }
        rewind = true;
        fromMs = 0;
    }
    if (gSoundPcm.empty() || path == gSoundMiss) {
        return;
    }
    if (rewind || gSound == nullptr) {
        StartSound(fromMs);
    }
}

void PumpMain() {
    const HRESULT started = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    (void)started;
    std::wstring open;
    int fittedW = 0;
    int fittedH = 0;
    bool haveFit = false;
    std::vector<RoboU32> fitted;
    while (true) {
        std::wstring path;
        int wantMs = 0;
        int destW = 0;
        int destH = 0;
        int duration = 0;
        bool wantSound = false;
        int slack = 8;
        {
            std::unique_lock<std::mutex> lock(gMutex);
            if (gHeldMs >= 0 && gWantMs > gHeldMs + 20) {
                slack = 0;
            }
            gCv.wait_for(lock, std::chrono::milliseconds(slack), [] { return gStop || gWake; });
            gWake = false;
            if (gStop) {
                break;
            }
            path = gWantPath;
            wantMs = gWantMs;
            destW = gWantW;
            destH = gWantH;
            duration = gWantDuration;
            wantSound = gWantSound;
        }
        if (path.empty()) {
            continue;
        }
        const std::filesystem::path file(path);
        if (path != open) {
            CloseReader();
            if (!OpenReader(file)) {
                continue;
            }
            open = path;
            fittedW = 0;
            fittedH = 0;
            haveFit = false;
        }
        bool decoded = false;
        bool rewind = false;
        int rewindMs = 0;
        if (duration > 1 && gHeldMs >= duration) {
            SeekReader(0);
            rewind = true;
            int nextMs = 0;
            decoded = PullFrame(nextMs);
        } else if (!gHeld.empty() && wantMs + 120 < gHeldMs) {
            SeekReader(wantMs);
            rewind = true;
            rewindMs = wantMs;
            int nextMs = 0;
            decoded = PullFrame(nextMs);
        } else if (gHeld.empty() || wantMs > gHeldMs + 2) {
            int nextMs = 0;
            decoded = PullFrame(nextMs);
        }
        if (gHeld.empty() || gWidth < 1 || gHeight < 1) {
            continue;
        }
        int outW = 0;
        int outH = 0;
        FitSize(destW, destH, outW, outH);
        if (!decoded && haveFit && outW == fittedW && outH == fittedH) {
            KeepSound(path, wantSound, rewindMs, rewind);
            continue;
        }
        if (outW == gWidth && outH == gHeight) {
            fitted = gHeld;
        } else {
            ScaleArea(gHeld, gWidth, gHeight, outW, outH, fitted);
        }
        fittedW = outW;
        fittedH = outH;
        haveFit = true;
        {
            std::lock_guard<std::mutex> lock(gMutex);
            gPublish.swap(fitted);
            gPublishW = outW;
            gPublishH = outH;
            gPublishMs = gHeldMs;
            ++gPublishGen;
        }
        gCv.notify_all();
        KeepSound(path, wantSound, rewindMs, rewind);
    }
    CloseReader();
    StopSound();
}

void EnsurePump() {
    bool start = false;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        if (!gStarted) {
            gStarted = true;
            start = true;
        }
    }
    if (start) {
        std::thread(PumpMain).detach();
    }
}

}  // namespace

bool WindowsVideoFrame(const std::filesystem::path& file, int timeMs, std::vector<RoboU32>& pixels, int& width, int& height, int& sampleMs,
                       int destWidth, int destHeight, int durationMs, bool withSound) {
    pixels.clear();
    width = 0;
    height = 0;
    sampleMs = 0;
    if (file.empty()) {
        return false;
    }
    if (timeMs < 0) {
        timeMs = 0;
    }
    EnsurePump();
    std::unique_lock<std::mutex> lock(gMutex);
    gWantPath = file.wstring();
    gWantMs = timeMs;
    if (destWidth > 0 && destHeight > 0) {
        gWantW = destWidth;
        gWantH = destHeight;
    }
    if (durationMs > 0) {
        gWantDuration = durationMs;
    }
    gWantSound = withSound;
    gWake = true;
    gCv.notify_all();
    if (gPublish.empty()) {
        gCv.wait_for(lock, std::chrono::milliseconds(1500), [] { return !gPublish.empty() || gStop; });
    }
    if (gPublish.empty()) {
        return false;
    }
    width = gPublishW;
    height = gPublishH;
    sampleMs = gPublishMs < 0 ? 0 : gPublishMs;
    if (gPublishGen == gServedGen) {
        return true;
    }
    pixels = gPublish;
    gServedGen = gPublishGen;
    return true;
}

}  // namespace Robix::UI
