#include "WindowsAudio.hpp"

#include "../../Compiler/Components/Audio/RoboAudio.hpp"

#include <fstream>
#include <string>
#include <vector>

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

namespace Robix::UI {
namespace {

HWAVEOUT gWave = nullptr;
WAVEHDR gHeader{};
std::vector<RoboI16> gSamples;
std::string gSignature;
bool gMediaReady = false;

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

void StopPlayback() {
    if (gWave == nullptr) {
        gSamples.clear();
        return;
    }
    waveOutReset(gWave);
    if ((gHeader.dwFlags & WHDR_PREPARED) != 0) {
        waveOutUnprepareHeader(gWave, &gHeader, sizeof(gHeader));
    }
    waveOutClose(gWave);
    gWave = nullptr;
    gHeader = {};
    gSamples.clear();
}

void StartPlayback(const RoboAudioOutput& output) {
    StopPlayback();
    if (output.samples.empty() || output.sampleRate < 8000 || output.channels < 1) {
        return;
    }
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = static_cast<WORD>(output.channels);
    format.nSamplesPerSec = static_cast<DWORD>(output.sampleRate);
    format.wBitsPerSample = 16;
    format.nBlockAlign = static_cast<WORD>(output.channels * 2);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    if (waveOutOpen(&gWave, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        gWave = nullptr;
        return;
    }
    gSamples = output.samples;
    gHeader.lpData = reinterpret_cast<LPSTR>(gSamples.data());
    gHeader.dwBufferLength = static_cast<DWORD>(gSamples.size() * sizeof(RoboI16));
    if (waveOutPrepareHeader(gWave, &gHeader, sizeof(gHeader)) != MMSYSERR_NOERROR) {
        StopPlayback();
        return;
    }
    if (waveOutWrite(gWave, &gHeader, sizeof(gHeader)) != MMSYSERR_NOERROR) {
        StopPlayback();
    }
}

bool ReadFoundation(const std::filesystem::path& file, RoboAudioClip& clip) {
    const HRESULT started = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (!gMediaReady) {
        gMediaReady = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET));
    }
    if (!gMediaReady) {
        if (started == S_OK) {
            CoUninitialize();
        }
        return false;
    }
    IMFSourceReader* reader = nullptr;
    if (FAILED(MFCreateSourceReaderFromURL(file.wstring().c_str(), nullptr, &reader)) || reader == nullptr) {
        if (started == S_OK) {
            CoUninitialize();
        }
        return false;
    }
    IMFMediaType* requested = nullptr;
    if (FAILED(MFCreateMediaType(&requested)) || requested == nullptr) {
        reader->Release();
        if (started == S_OK) {
            CoUninitialize();
        }
        return false;
    }
    requested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    requested->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    requested->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    const bool typed = SUCCEEDED(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, requested));
    requested->Release();
    if (!typed) {
        reader->Release();
        if (started == S_OK) {
            CoUninitialize();
        }
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
    std::vector<RoboI16> samples;
    const std::size_t cap = (rate > 0 && channels > 0) ? static_cast<std::size_t>(rate) * channels * 20u : 48000u * 2u * 20u;
    while (samples.size() < cap) {
        IMFSample* sample = nullptr;
        DWORD flags = 0;
        if (FAILED(reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &flags, nullptr, &sample))) {
            break;
        }
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
            if (sample != nullptr) {
                sample->Release();
            }
            break;
        }
        if (sample == nullptr) {
            continue;
        }
        IMFMediaBuffer* buffer = nullptr;
        if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer)) && buffer != nullptr) {
            BYTE* data = nullptr;
            DWORD length = 0;
            if (SUCCEEDED(buffer->Lock(&data, nullptr, &length)) && data != nullptr && length > 1) {
                const std::size_t count = length / sizeof(RoboI16);
                const auto* pcm = reinterpret_cast<const RoboI16*>(data);
                samples.insert(samples.end(), pcm, pcm + count);
                buffer->Unlock();
            }
            buffer->Release();
        }
        sample->Release();
    }
    reader->Release();
    if (started == S_OK) {
        CoUninitialize();
    }
    if (samples.empty() || rate == 0 || channels == 0) {
        return false;
    }
    if (clip.duration <= 0) {
        clip.duration = static_cast<int>(static_cast<unsigned long long>(samples.size()) / channels * 1000ull / rate);
    }
    clip.sampleRate = static_cast<int>(rate);
    clip.channels = static_cast<int>(channels);
    clip.samples = std::move(samples);
    clip.pcm = true;
    return true;
}

void Collect(const RoboXMLNode& node, std::vector<const RoboXMLNode*>& found) {
    if (Lower(node.name) == "audio") {
        found.push_back(&node);
    }
    for (const RoboXMLNode& child : node.children) {
        Collect(child, found);
    }
}

}  // namespace

void WindowsAudioSync(const RoboXMLNode& root, const std::filesystem::path& directory) {
    std::vector<const RoboXMLNode*> nodes;
    Collect(root, nodes);
    const RoboXMLNode* chosen = nullptr;
    RoboAudioNode audio;
    std::string diagnostic;
    for (const RoboXMLNode* node : nodes) {
        RoboAudioNode candidate;
        if (node != nullptr && RoboAudioRead(*node, candidate, diagnostic) && candidate.playing) {
            chosen = node;
            audio = candidate;
            break;
        }
    }
    if (chosen == nullptr) {
        if (!gSignature.empty()) {
            StopPlayback();
            gSignature.clear();
        }
        return;
    }
    std::filesystem::path file(audio.source);
    if (file.is_relative()) {
        file = directory / file;
    }
    std::error_code error;
    const auto stamp = std::filesystem::exists(file, error) ? std::filesystem::file_size(file, error) : 0;
    const std::string signature = audio.source + "|" + std::to_string(audio.time) + "|" + std::to_string(audio.volume) + "|" +
                                  std::to_string(stamp);
    if (signature == gSignature) {
        return;
    }
    gSignature = signature;
    std::ifstream input(file, std::ios::binary);
    std::vector<RoboU8> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    RoboAudioClip clip;
    if (!RoboAudioDecode(clip, bytes.data(), bytes.size(), audio.source)) {
        StopPlayback();
        return;
    }
    if (!clip.pcm && std::filesystem::is_regular_file(file, error)) {
        ReadFoundation(file, clip);
    }
    RoboAudioBind(audio, clip);
    RoboAudioOutput output;
    if (!RoboAudioPlay(clip, output)) {
        StopPlayback();
        return;
    }
    StartPlayback(output);
}

}  // namespace Robix::UI
