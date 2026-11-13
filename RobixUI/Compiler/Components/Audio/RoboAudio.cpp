#include "RoboAudio.hpp"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int ROBO_AUDIO_MAX_STORED_FRAMES = 48000 * 30;

const char* const ROBO_AUDIO_ATTRIBUTES[] = {
    "id", "name", "source", "src", "time", "playing", "volume",
};

struct Ext {
    const char* ext;
    const char* format;
};

const Ext ROBO_AUDIO_EXTENSIONS[] = {
    {"mp3", "mp3"},   {"aac", "aac"},   {"m4a", "m4a"}, {"ogg", "ogg"},   {"opus", "opus"}, {"wma", "wma"},
    {"amr", "amr"},   {"mp2", "mp2"},   {"ac3", "ac3"}, {"eac3", "eac3"}, {"ec3", "eac3"}, {"mp4", "mp4"},
    {"flac", "flac"}, {"alac", "alac"}, {"wv", "wv"},   {"ape", "ape"},   {"tta", "tta"},   {"tak", "tak"},
    {"ofr", "ofr"},   {"als", "als"},   {"wav", "wav"}, {"aiff", "aiff"}, {"aif", "aif"},   {"au", "au"},
    {"pcm", "pcm"},   {"bwf", "bwf"},   {"rf64", "rf64"}, {"caf", "caf"}, {"mid", "mid"},   {"midi", "mid"},
    {"kar", "kar"},   {"mod", "mod"},   {"xm", "xm"},   {"it", "it"},     {"s3m", "s3m"},   {"nsf", "nsf"},
    {"spc", "spc"},   {"dts", "dts"},   {"dtshd", "dtshd"}, {"thd", "thd"}, {"ac4", "ac4"}, {"dsf", "dsf"},
    {"dff", "dff"},   {"awb", "awb"},   {"gsm", "gsm"}, {"ulaw", "ulaw"}, {"alaw", "alaw"}, {"g722", "g722"},
    {"g729", "g729"}, {"spx", "spx"},   {"silk", "silk"}, {"m3u8", "m3u8"}, {"mpd", "mpd"}, {"webm", "webm"},
    {"mka", "mka"},   {"mkv", "mka"},   {"m3u", "m3u"}, {"pls", "pls"},   {"wpl", "wpl"},   {"asx", "asx"},
};

std::string Lower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

std::string Fail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

bool Known(const std::string& key) {
    const std::string folded = Lower(key);
    for (const char* name : ROBO_AUDIO_ATTRIBUTES) {
        if (folded == name) {
            return true;
        }
    }
    return false;
}

bool Integer(const std::string& text, int& number) {
    if (text.empty()) {
        return false;
    }
    int index = 0;
    int sign = 1;
    if (text[0] == '-') {
        sign = -1;
        index = 1;
    }
    if (index >= static_cast<int>(text.size())) {
        return false;
    }
    int value = 0;
    for (; index < static_cast<int>(text.size()); ++index) {
        const char ch = text[static_cast<std::size_t>(index)];
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10 + (ch - '0');
    }
    number = sign * value;
    return true;
}

bool Flag(const std::string& text, bool& value) {
    const std::string folded = Lower(text);
    if (folded == "true" || folded == "yes" || folded == "on" || folded == "1") {
        value = true;
        return true;
    }
    if (folded == "false" || folded == "no" || folded == "off" || folded == "0") {
        value = false;
        return true;
    }
    return false;
}

std::string ExtensionOf(const std::string& source) {
    std::string name = source;
    const std::size_t query = name.find_first_of("?#");
    if (query != std::string::npos) {
        name.resize(query);
    }
    const std::size_t slash = name.find_last_of("/\\");
    const std::size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return {};
    }
    return Lower(name.substr(dot + 1));
}

bool Has(const RoboU8* data, std::size_t size, std::size_t at, std::size_t count) {
    return data != nullptr && count <= size && at <= size - count;
}

bool Match(const RoboU8* data, std::size_t size, std::size_t at, const char* text) {
    const std::size_t count = std::strlen(text);
    if (!Has(data, size, at, count)) {
        return false;
    }
    return std::memcmp(data + at, text, count) == 0;
}

RoboU16 Read16(const RoboU8* data, bool big) {
    if (big) {
        return static_cast<RoboU16>((data[0] << 8) | data[1]);
    }
    return static_cast<RoboU16>(data[0] | (data[1] << 8));
}

RoboU32 Read32(const RoboU8* data, bool big) {
    if (big) {
        return (static_cast<RoboU32>(data[0]) << 24) | (static_cast<RoboU32>(data[1]) << 16) |
               (static_cast<RoboU32>(data[2]) << 8) | data[3];
    }
    return static_cast<RoboU32>(data[0]) | (static_cast<RoboU32>(data[1]) << 8) |
           (static_cast<RoboU32>(data[2]) << 16) | (static_cast<RoboU32>(data[3]) << 24);
}

RoboU64 Read64(const RoboU8* data, bool big) {
    RoboU64 value = 0;
    for (int index = 0; index < 8; ++index) {
        const int from = big ? index : 7 - index;
        value = (value << 8) | data[from];
    }
    return value;
}

int Milliseconds(RoboU64 frames, int rate) {
    if (rate <= 0) {
        return 0;
    }
    const RoboU64 ms = frames * 1000ull / static_cast<RoboU64>(rate);
    if (ms > 2147483647ull) {
        return 2147483647;
    }
    return static_cast<int>(ms);
}

void KeepSamples(RoboAudioClip& clip, std::vector<RoboI16> samples, int rate, int channels, RoboU64 frames) {
    clip.sampleRate = rate;
    clip.channels = channels;
    clip.duration = Milliseconds(frames, rate);
    if (channels < 1 || frames == 0) {
        clip.pcm = false;
        return;
    }
    const RoboU64 keep = frames > static_cast<RoboU64>(ROBO_AUDIO_MAX_STORED_FRAMES)
                             ? static_cast<RoboU64>(ROBO_AUDIO_MAX_STORED_FRAMES)
                             : frames;
    const std::size_t count = static_cast<std::size_t>(keep) * static_cast<std::size_t>(channels);
    if (samples.size() > count) {
        samples.resize(count);
    }
    clip.samples = std::move(samples);
    clip.pcm = !clip.samples.empty();
}

int UlawSample(RoboU8 value) {
    value = static_cast<RoboU8>(~value);
    const int sign = value & 0x80;
    const int exponent = (value >> 4) & 7;
    const int mantissa = value & 0x0F;
    int sample = ((mantissa << 3) + 0x84) << exponent;
    sample -= 0x84;
    return sign != 0 ? -sample : sample;
}

int AlawSample(RoboU8 value) {
    value = static_cast<RoboU8>(value ^ 0x55);
    const int sign = value & 0x80;
    const int exponent = (value >> 4) & 7;
    int sample = (value & 0x0F) << 4;
    if (exponent == 0) {
        sample += 8;
    } else {
        sample += 0x108;
        sample <<= exponent - 1;
    }
    return sign != 0 ? sample : -sample;
}

int Extended80(const RoboU8* data) {
    const int exponent = ((data[0] & 0x7F) << 8) | data[1];
    const RoboU32 mantissa = (static_cast<RoboU32>(data[2]) << 24) | (static_cast<RoboU32>(data[3]) << 16) |
                             (static_cast<RoboU32>(data[4]) << 8) | data[5];
    if ((data[0] & 0x80) != 0 || exponent == 0 || mantissa == 0) {
        return 0;
    }
    const int shift = exponent - 16383 - 31;
    if (shift >= 0) {
        return shift > 8 ? 0 : static_cast<int>(mantissa << shift);
    }
    return static_cast<int>(mantissa >> -shift);
}

bool StreamUrl(const std::string& source) {
    const std::string key = Lower(source);
    return key.rfind("http://", 0) == 0 || key.rfind("https://", 0) == 0;
}

bool Icy(const RoboU8* data, std::size_t size) {
    if (Match(data, size, 0, "ICY")) {
        return true;
    }
    if (!Match(data, size, 0, "HTTP/1.")) {
        return false;
    }
    const std::string text(reinterpret_cast<const char*>(data), size < 512 ? size : 512);
    const std::string folded = Lower(text);
    return folded.find("icy-") != std::string::npos || folded.find("audio/") != std::string::npos;
}

int ModuleMs(int orders, int speed, int tempo) {
    if (orders < 1) {
        orders = 1;
    }
    if (speed < 1) {
        speed = 6;
    }
    if (tempo < 1) {
        tempo = 125;
    }
    return static_cast<int>((static_cast<long long>(orders) * 64 * speed * 2500) / tempo);
}

int SecondsMs(const std::string& text) {
    if (text.empty()) {
        return -1;
    }
    int whole = 0;
    std::size_t index = 0;
    if (text[0] == '-') {
        return -1;
    }
    for (; index < text.size() && text[index] >= '0' && text[index] <= '9'; ++index) {
        whole = whole * 10 + (text[index] - '0');
    }
    int fraction = 0;
    int scale = 1;
    if (index < text.size() && text[index] == '.') {
        ++index;
        for (; index < text.size() && text[index] >= '0' && text[index] <= '9' && scale < 1000; ++index) {
            fraction = fraction * 10 + (text[index] - '0');
            scale *= 10;
        }
    }
    if (index == 0 || (index == 1 && text[0] == '.')) {
        return -1;
    }
    return whole * 1000 + fraction * 1000 / scale;
}

bool ParsePlaylist(RoboAudioClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    clip.format = format;
    const std::string text(reinterpret_cast<const char*>(data), size);
    if (format == "m3u" || format == "m3u8") {
        std::size_t at = 0;
        while (at < text.size()) {
            std::size_t end = text.find('\n', at);
            if (end == std::string::npos) {
                end = text.size();
            }
            std::string line = text.substr(at, end - at);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            const std::string folded = Lower(line);
            if (folded.rfind("#extinf:", 0) == 0) {
                const int ms = SecondsMs(line.substr(8));
                if (ms > 0) {
                    clip.duration += ms;
                }
            } else if (!line.empty() && line[0] != '#') {
                clip.entries.push_back(line);
            }
            at = end + 1;
        }
        return true;
    }
    if (format == "pls") {
        std::size_t at = 0;
        while (at < text.size()) {
            std::size_t end = text.find('\n', at);
            if (end == std::string::npos) {
                end = text.size();
            }
            std::string line = text.substr(at, end - at);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            const std::string folded = Lower(line);
            if (folded.rfind("file", 0) == 0) {
                const std::size_t equal = line.find('=');
                if (equal != std::string::npos && equal + 1 < line.size()) {
                    clip.entries.push_back(line.substr(equal + 1));
                }
            } else if (folded.rfind("length", 0) == 0) {
                const std::size_t equal = line.find('=');
                if (equal != std::string::npos) {
                    const int ms = SecondsMs(line.substr(equal + 1));
                    if (ms > 0) {
                        clip.duration += ms;
                    }
                }
            }
            at = end + 1;
        }
        return !clip.entries.empty() || text.find("[playlist]") != std::string::npos || text.find("[playlist]") != std::string::npos ||
               Lower(text).find("[playlist]") != std::string::npos;
    }
    if (format == "wpl") {
        const std::string folded = Lower(text);
        std::size_t at = 0;
        while ((at = folded.find("src=", at)) != std::string::npos) {
            clip.entries.push_back("item");
            at += 4;
        }
        return folded.find("<smil") != std::string::npos || folded.find("<?wpl") != std::string::npos || !clip.entries.empty();
    }
    if (format == "asx") {
        const std::string folded = Lower(text);
        std::size_t at = 0;
        while ((at = folded.find("href=", at)) != std::string::npos) {
            clip.entries.push_back("item");
            at += 5;
        }
        at = 0;
        while ((at = folded.find("duration=", at)) != std::string::npos) {
            const std::size_t quote = text.find('"', at);
            const std::size_t next = text.find('"', quote == std::string::npos ? at : quote + 1);
            if (quote != std::string::npos && next != std::string::npos) {
                const int ms = SecondsMs(text.substr(quote + 1, next - quote - 1));
                if (ms > 0) {
                    clip.duration += ms;
                }
            }
            at += 9;
        }
        return folded.find("<asx") != std::string::npos;
    }
    if (format == "mpd") {
        const std::string folded = Lower(text);
        const std::size_t key = folded.find("mediapresentationduration");
        if (key != std::string::npos) {
            const std::size_t quote = text.find('"', key);
            const std::size_t next = quote == std::string::npos ? std::string::npos : text.find('"', quote + 1);
            if (quote != std::string::npos && next != std::string::npos) {
                std::string stamp = text.substr(quote + 1, next - quote - 1);
                if (!stamp.empty() && (stamp[0] == 'P' || stamp[0] == 'p')) {
                    stamp = stamp.substr(1);
                }
                if (!stamp.empty() && (stamp[0] == 'T' || stamp[0] == 't')) {
                    stamp = stamp.substr(1);
                }
                int hours = 0;
                int minutes = 0;
                int ms = 0;
                const std::size_t hour = Lower(stamp).find('h');
                const std::size_t minute = Lower(stamp).find('m');
                const std::size_t second = Lower(stamp).find('s');
                if (hour != std::string::npos) {
                    hours = SecondsMs(stamp.substr(0, hour)) / 1000;
                }
                if (minute != std::string::npos) {
                    const std::size_t from = hour == std::string::npos ? 0 : hour + 1;
                    minutes = SecondsMs(stamp.substr(from, minute - from)) / 1000;
                }
                if (second != std::string::npos) {
                    const std::size_t from = minute != std::string::npos ? minute + 1 : (hour == std::string::npos ? 0 : hour + 1);
                    ms = SecondsMs(stamp.substr(from, second - from));
                }
                clip.duration = hours * 3600000 + minutes * 60000 + (ms > 0 ? ms : 0);
            }
        }
        return folded.find("<mpd") != std::string::npos;
    }
    return false;
}

bool ParseWav(RoboAudioClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    const bool rf64 = Match(data, size, 0, "RF64");
    if (!rf64 && !Match(data, size, 0, "RIFF")) {
        return false;
    }
    if (!Match(data, size, 8, "WAVE")) {
        return false;
    }
    int codec = 0;
    int channels = 0;
    int rate = 0;
    int bits = 0;
    RoboU64 dataBytes = 0;
    bool bext = false;
    bool sawFmt = false;
    std::size_t dataAt = 0;
    std::size_t at = 12;
    while (at + 8 <= size) {
        char name[5] = {};
        std::memcpy(name, data + at, 4);
        RoboU32 chunk = Read32(data + at + 4, false);
        std::size_t body = at + 8;
        if (std::strcmp(name, "ds64") == 0 && Has(data, size, body, 28)) {
            dataBytes = Read64(data + body + 8, false);
        }
        if (std::strcmp(name, "bext") == 0) {
            bext = true;
        }
        if (std::strcmp(name, "fmt ") == 0 && Has(data, size, body, 16)) {
            codec = Read16(data + body, false);
            channels = Read16(data + body + 2, false);
            rate = static_cast<int>(Read32(data + body + 4, false));
            bits = Read16(data + body + 14, false);
            sawFmt = true;
        }
        if (std::strcmp(name, "data") == 0) {
            dataAt = body;
            if (chunk != 0xFFFFFFFFu) {
                dataBytes = chunk;
            }
        }
        if (chunk == 0xFFFFFFFFu) {
            break;
        }
        const std::size_t step = 8ull + chunk + (chunk & 1u);
        if (step < 8 || at + step > size) {
            break;
        }
        at += step;
    }
    if (!sawFmt || channels < 1 || rate < 1) {
        return false;
    }
    if (format == "bwf" || (bext && format != "wav" && format != "rf64")) {
        clip.format = "bwf";
    } else if (rf64 || format == "rf64") {
        clip.format = "rf64";
    } else {
        clip.format = "wav";
    }
    const int width = bits <= 8 ? 1 : (bits <= 16 ? 2 : 4);
    RoboU64 frames = 0;
    if (codec == 0x31) {
        clip.format = format == "gsm" ? "gsm" : clip.format;
        frames = dataBytes / 33ull * 160ull;
        clip.sampleRate = rate > 0 ? rate : 8000;
        clip.channels = channels;
        clip.duration = Milliseconds(frames, clip.sampleRate);
        return true;
    }
    if (dataAt == 0 || dataBytes == 0 || !Has(data, size, dataAt, 1)) {
        clip.sampleRate = rate;
        clip.channels = channels;
        return true;
    }
    const std::size_t available = size - dataAt;
    if (dataBytes > available) {
        dataBytes = available;
    }
    frames = dataBytes / static_cast<RoboU64>(width * channels);
    std::vector<RoboI16> samples;
    const RoboU64 keep = frames > static_cast<RoboU64>(ROBO_AUDIO_MAX_STORED_FRAMES)
                             ? static_cast<RoboU64>(ROBO_AUDIO_MAX_STORED_FRAMES)
                             : frames;
    samples.reserve(static_cast<std::size_t>(keep) * static_cast<std::size_t>(channels));
    for (RoboU64 frame = 0; frame < keep; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const std::size_t pos = dataAt + static_cast<std::size_t>((frame * channels + channel) * width);
            int sample = 0;
            if (codec == 1 && bits <= 8) {
                sample = (static_cast<int>(data[pos]) - 128) << 8;
            } else if (codec == 1 && bits <= 16) {
                sample = static_cast<RoboI16>(Read16(data + pos, false));
            } else if (codec == 3 && bits == 32 && Has(data, size, pos, 4)) {
                float unit = 0;
                std::memcpy(&unit, data + pos, 4);
                sample = static_cast<int>(unit * 32767.f);
            } else if (codec == 7) {
                sample = UlawSample(data[pos]);
            } else if (codec == 6) {
                sample = AlawSample(data[pos]);
            } else {
                clip.sampleRate = rate;
                clip.channels = channels;
                clip.duration = Milliseconds(frames, rate);
                return true;
            }
            if (sample > 32767) {
                sample = 32767;
            }
            if (sample < -32768) {
                sample = -32768;
            }
            samples.push_back(static_cast<RoboI16>(sample));
        }
    }
    KeepSamples(clip, std::move(samples), rate, channels, frames);
    return true;
}

bool ParseAiff(RoboAudioClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (!Match(data, size, 0, "FORM") || (!Match(data, size, 8, "AIFF") && !Match(data, size, 8, "AIFC"))) {
        return false;
    }
    int channels = 0;
    int rate = 0;
    int bits = 0;
    RoboU32 frames = 0;
    std::size_t dataAt = 0;
    std::size_t at = 12;
    while (at + 8 <= size) {
        char name[5] = {};
        std::memcpy(name, data + at, 4);
        const RoboU32 chunk = Read32(data + at + 4, true);
        const std::size_t body = at + 8;
        if (std::strcmp(name, "COMM") == 0 && Has(data, size, body, 18)) {
            channels = Read16(data + body, true);
            frames = Read32(data + body + 2, true);
            bits = Read16(data + body + 6, true);
            rate = Extended80(data + body + 8);
        }
        if (std::strcmp(name, "SSND") == 0 && Has(data, size, body, 8)) {
            const RoboU32 offset = Read32(data + body, true);
            dataAt = body + 8 + offset;
        }
        const std::size_t step = 8ull + chunk + (chunk & 1u);
        if (step < 8 || at + step > size) {
            break;
        }
        at += step;
    }
    if (channels < 1 || rate < 1) {
        return false;
    }
    clip.format = format == "aif" ? "aif" : "aiff";
    if (bits != 16 || dataAt == 0 || !Has(data, size, dataAt, 2)) {
        clip.sampleRate = rate;
        clip.channels = channels;
        clip.duration = Milliseconds(frames, rate);
        return frames > 0 || dataAt != 0;
    }
    std::vector<RoboI16> samples;
    const RoboU32 keep = frames > static_cast<RoboU32>(ROBO_AUDIO_MAX_STORED_FRAMES)
                             ? static_cast<RoboU32>(ROBO_AUDIO_MAX_STORED_FRAMES)
                             : frames;
    for (RoboU32 frame = 0; frame < keep; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const std::size_t pos = dataAt + static_cast<std::size_t>((frame * channels + channel) * 2);
            if (!Has(data, size, pos, 2)) {
                break;
            }
            samples.push_back(static_cast<RoboI16>(Read16(data + pos, true)));
        }
    }
    KeepSamples(clip, std::move(samples), rate, channels, frames);
    return true;
}

bool ParseAu(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    if (!Has(data, size, 0, 24) || Read32(data, true) != 0x2E736E64u) {
        return false;
    }
    const RoboU32 offset = Read32(data + 4, true);
    RoboU32 bytes = Read32(data + 8, true);
    const RoboU32 encoding = Read32(data + 12, true);
    const int rate = static_cast<int>(Read32(data + 16, true));
    const int channels = static_cast<int>(Read32(data + 20, true));
    if (rate < 1 || channels < 1 || offset >= size) {
        return false;
    }
    clip.format = "au";
    int width = 1;
    if (encoding == 3) {
        width = 2;
    } else if (encoding != 1 && encoding != 2 && encoding != 27) {
        clip.sampleRate = rate;
        clip.channels = channels;
        if (bytes == 0xFFFFFFFFu) {
            bytes = static_cast<RoboU32>(size - offset);
        }
        clip.duration = Milliseconds(bytes / static_cast<RoboU32>(channels), rate);
        return true;
    }
    if (bytes == 0xFFFFFFFFu || offset + bytes > size) {
        bytes = static_cast<RoboU32>(size - offset);
    }
    const RoboU64 frames = bytes / static_cast<RoboU64>(width * channels);
    std::vector<RoboI16> samples;
    const RoboU64 keep = frames > static_cast<RoboU64>(ROBO_AUDIO_MAX_STORED_FRAMES)
                             ? static_cast<RoboU64>(ROBO_AUDIO_MAX_STORED_FRAMES)
                             : frames;
    for (RoboU64 frame = 0; frame < keep; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const std::size_t pos = offset + static_cast<std::size_t>((frame * channels + channel) * width);
            int sample = 0;
            if (encoding == 2) {
                sample = static_cast<int>(static_cast<RoboI16>(static_cast<RoboI16>(data[pos]) << 8));
            } else if (encoding == 3) {
                sample = static_cast<RoboI16>(Read16(data + pos, true));
            } else if (encoding == 1) {
                sample = UlawSample(data[pos]);
            } else {
                sample = AlawSample(data[pos]);
            }
            samples.push_back(static_cast<RoboI16>(sample));
        }
    }
    KeepSamples(clip, std::move(samples), rate, channels, frames);
    return true;
}

bool ParseCaf(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    if (!Match(data, size, 0, "caff") || size < 8) {
        return false;
    }
    int rate = 0;
    int channels = 0;
    int bits = 0;
    RoboU32 flags = 0;
    std::size_t dataAt = 0;
    RoboU64 dataBytes = 0;
    std::size_t at = 8;
    while (at + 12 <= size) {
        char name[5] = {};
        std::memcpy(name, data + at, 4);
        const RoboU64 chunk = Read64(data + at + 4, true);
        const std::size_t body = at + 12;
        if (std::strcmp(name, "desc") == 0 && Has(data, size, body, 32)) {
            const double unit = [&]() {
                RoboU8 swapped[8];
                for (int index = 0; index < 8; ++index) {
                    swapped[index] = data[body + 7 - index];
                }
                double value = 0;
                std::memcpy(&value, swapped, 8);
                return value;
            }();
            rate = static_cast<int>(unit + 0.5);
            flags = Read32(data + body + 16, true);
            channels = static_cast<int>(Read32(data + body + 24, true));
            bits = static_cast<int>(Read32(data + body + 28, true));
        }
        if (std::strcmp(name, "data") == 0 && Has(data, size, body, 4)) {
            dataAt = body + 4;
            dataBytes = chunk > 4 ? chunk - 4 : 0;
        }
        if (chunk > size || at + 12 + chunk > size) {
            break;
        }
        at += 12 + static_cast<std::size_t>(chunk);
    }
    if (rate < 1 || channels < 1) {
        return false;
    }
    clip.format = "caf";
    const bool little = (flags & 2u) != 0;
    if (bits != 16 || dataAt == 0) {
        clip.sampleRate = rate;
        clip.channels = channels;
        const int width = bits > 0 ? bits / 8 : 2;
        clip.duration = Milliseconds(width > 0 ? dataBytes / static_cast<RoboU64>(width * channels) : 0, rate);
        return true;
    }
    if (dataBytes > size - dataAt) {
        dataBytes = size - dataAt;
    }
    const RoboU64 frames = dataBytes / static_cast<RoboU64>(2 * channels);
    std::vector<RoboI16> samples;
    const RoboU64 keep = frames > static_cast<RoboU64>(ROBO_AUDIO_MAX_STORED_FRAMES)
                             ? static_cast<RoboU64>(ROBO_AUDIO_MAX_STORED_FRAMES)
                             : frames;
    for (RoboU64 frame = 0; frame < keep; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const std::size_t pos = dataAt + static_cast<std::size_t>((frame * channels + channel) * 2);
            if (!Has(data, size, pos, 2)) {
                break;
            }
            samples.push_back(static_cast<RoboI16>(Read16(data + pos, !little)));
        }
    }
    KeepSamples(clip, std::move(samples), rate, channels, frames);
    return true;
}

bool ParseFlac(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    if (!Match(data, size, 0, "fLaC") || size < 42 || (data[4] & 0x7F) != 0) {
        return false;
    }
    const RoboU8* packed = data + 18;
    RoboU64 value = 0;
    for (int index = 0; index < 8; ++index) {
        value = (value << 8) | packed[index];
    }
    const int rate = static_cast<int>(value >> 44);
    const int channels = static_cast<int>(((value >> 41) & 7u) + 1u);
    const RoboU64 frames = value & 0xFFFFFFFFFull;
    if (rate < 1) {
        return false;
    }
    clip.format = "flac";
    clip.sampleRate = rate;
    clip.channels = channels;
    clip.duration = Milliseconds(frames, rate);
    return true;
}

bool ParseMpeg(RoboAudioClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    std::size_t at = 0;
    if (Match(data, size, 0, "ID3") && size >= 10) {
        const int length = (data[6] << 21) | (data[7] << 14) | (data[8] << 7) | data[9];
        at = static_cast<std::size_t>(10 + length);
    }
    const int kRate[4][3] = {{11025, 12000, 8000}, {0, 0, 0}, {22050, 24000, 16000}, {44100, 48000, 32000}};
    const int kBitrate[4][16] = {
        {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
        {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
    };
    RoboU64 frames = 0;
    int rate = 0;
    int layer = 0;
    while (at + 4 <= size) {
        if (data[at] != 0xFF || (data[at + 1] & 0xE0) != 0xE0) {
            ++at;
            continue;
        }
        const int version = (data[at + 1] >> 3) & 3;
        const int layerBits = (data[at + 1] >> 1) & 3;
        const int bitrateIndex = (data[at + 2] >> 4) & 0x0F;
        const int rateIndex = (data[at + 2] >> 2) & 3;
        const int padding = (data[at + 2] >> 1) & 1;
        if (version == 1 || layerBits == 0 || bitrateIndex == 0 || bitrateIndex == 15 || rateIndex == 3) {
            ++at;
            continue;
        }
        const int bitrateRow = layerBits == 1 ? (version == 3 ? 0 : 3) : (layerBits == 2 ? 1 : 2);
        const int bitrate = kBitrate[bitrateRow][bitrateIndex] * 1000;
        const int sampleRate = kRate[version][rateIndex];
        if (bitrate <= 0 || sampleRate <= 0) {
            ++at;
            continue;
        }
        int frameSamples = 1152;
        int frameBytes = 0;
        if (layerBits == 3) {
            frameSamples = 384;
            frameBytes = (12 * bitrate / sampleRate + padding) * 4;
        } else if (layerBits == 1 && version != 3) {
            frameSamples = 576;
            frameBytes = 72 * bitrate / sampleRate + padding;
        } else {
            frameBytes = 144 * bitrate / sampleRate + padding;
        }
        if (frameBytes < 4 || at + static_cast<std::size_t>(frameBytes) > size) {
            break;
        }
        frames += static_cast<RoboU64>(frameSamples);
        rate = sampleRate;
        layer = layerBits;
        at += static_cast<std::size_t>(frameBytes);
    }
    if (rate <= 0 || frames == 0) {
        return false;
    }
    if (format == "mp3" || format == "mp2") {
        clip.format = format;
    } else {
        clip.format = layer == 2 ? "mp2" : "mp3";
    }
    clip.sampleRate = rate;
    clip.channels = 2;
    clip.duration = Milliseconds(frames, rate);
    return true;
}

bool ParseAdts(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    const int kRate[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350};
    std::size_t at = 0;
    RoboU64 frames = 0;
    int rate = 0;
    while (at + 7 <= size) {
        if (data[at] != 0xFF || (data[at + 1] & 0xF0) != 0xF0 || ((data[at + 1] >> 1) & 3) != 0) {
            ++at;
            continue;
        }
        const int index = ((data[at + 2] >> 2) & 0x0F);
        const int length = ((data[at + 3] & 3) << 11) | (data[at + 4] << 3) | ((data[at + 5] >> 5) & 7);
        if (index >= 13 || length < 7 || at + static_cast<std::size_t>(length) > size) {
            break;
        }
        rate = kRate[index];
        frames += 1024;
        at += static_cast<std::size_t>(length);
    }
    if (rate <= 0) {
        return false;
    }
    clip.format = "aac";
    clip.sampleRate = rate;
    clip.channels = 1;
    clip.duration = Milliseconds(frames, rate);
    return true;
}

const int kAc3Words[3][19] = {
    {64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 640, 768, 896, 1024, 1152, 1280},
    {69, 87, 104, 121, 139, 174, 208, 243, 278, 348, 417, 487, 557, 696, 835, 975, 1114, 1253, 1393},
    {96, 120, 144, 168, 192, 240, 288, 336, 384, 480, 576, 672, 768, 960, 1152, 1344, 1536, 1728, 1920},
};

bool ParseAc3(RoboAudioClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    std::size_t at = 0;
    RoboU64 frames = 0;
    int rate = 0;
    bool enhanced = format == "eac3";
    while (at + 6 <= size) {
        if (data[at] != 0x0B || data[at + 1] != 0x77) {
            ++at;
            continue;
        }
        const int bsid = data[at + 5] >> 3;
        if (bsid > 16) {
            ++at;
            continue;
        }
        if (bsid > 10) {
            enhanced = true;
            const int frmsiz = ((data[at + 2] & 7) << 8) | data[at + 3];
            const int bytes = (frmsiz + 1) * 2;
            const int fscod = (data[at + 4] >> 6) & 3;
            const int rates[4] = {48000, 44100, 32000, 48000};
            if (bytes < 8 || at + static_cast<std::size_t>(bytes) > size) {
                break;
            }
            rate = rates[fscod];
            frames += 1536;
            at += static_cast<std::size_t>(bytes);
            continue;
        }
        const int fscod = data[at + 4] >> 6;
        const int frmsizecod = data[at + 4] & 0x3F;
        if (fscod > 2 || frmsizecod > 37) {
            ++at;
            continue;
        }
        const int words = kAc3Words[fscod][frmsizecod / 2];
        const int bytes = words * 2;
        const int rates[3] = {48000, 44100, 32000};
        if (bytes < 8 || at + static_cast<std::size_t>(bytes) > size) {
            break;
        }
        rate = rates[fscod];
        frames += 1536;
        at += static_cast<std::size_t>(bytes);
    }
    if (rate <= 0) {
        return false;
    }
    clip.format = enhanced || format == "eac3" ? "eac3" : "ac3";
    if (format == "ac3") {
        clip.format = "ac3";
    }
    clip.sampleRate = rate;
    clip.channels = enhanced ? 6 : 2;
    clip.duration = Milliseconds(frames, rate);
    return true;
}

bool ParseAmr(RoboAudioClip& clip, const RoboU8* data, std::size_t size, bool wide) {
    const int narrow[16] = {13, 14, 16, 18, 20, 21, 27, 32, 6, 7, 6, 6, 1, 1, 1, 1};
    const int broad[16] = {18, 24, 33, 37, 41, 47, 51, 59, 61, 7, 7, 2, 1, 1, 1, 1};
    std::size_t at = 0;
    if (wide && Match(data, size, 0, "#!AMR-WB\n")) {
        at = 9;
    } else if (!wide && Match(data, size, 0, "#!AMR\n")) {
        at = 6;
    } else {
        return false;
    }
    int frames = 0;
    while (at < size) {
        const int kind = (data[at] >> 3) & 0x0F;
        const int bytes = wide ? broad[kind] : narrow[kind];
        if (bytes < 1 || at + static_cast<std::size_t>(bytes) > size) {
            break;
        }
        at += static_cast<std::size_t>(bytes);
        ++frames;
    }
    if (frames < 1) {
        return false;
    }
    clip.format = wide ? "awb" : "amr";
    clip.sampleRate = wide ? 16000 : 8000;
    clip.channels = 1;
    clip.duration = frames * 20;
    return true;
}

bool ParseRaw(RoboAudioClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (size == 0) {
        return false;
    }
    clip.format = format;
    if (format == "pcm") {
        const RoboU64 frames = size / 2;
        std::vector<RoboI16> samples;
        const RoboU64 keep = frames > static_cast<RoboU64>(ROBO_AUDIO_MAX_STORED_FRAMES) ? static_cast<RoboU64>(ROBO_AUDIO_MAX_STORED_FRAMES) : frames;
        for (RoboU64 frame = 0; frame < keep; ++frame) {
            samples.push_back(static_cast<RoboI16>(Read16(data + frame * 2, false)));
        }
        KeepSamples(clip, std::move(samples), 44100, 1, frames);
        return true;
    }
    if (format == "ulaw" || format == "alaw") {
        const RoboU64 keep = size > static_cast<std::size_t>(ROBO_AUDIO_MAX_STORED_FRAMES) ? static_cast<RoboU64>(ROBO_AUDIO_MAX_STORED_FRAMES) : size;
        std::vector<RoboI16> samples;
        for (RoboU64 index = 0; index < keep; ++index) {
            samples.push_back(static_cast<RoboI16>(format == "ulaw" ? UlawSample(data[index]) : AlawSample(data[index])));
        }
        KeepSamples(clip, std::move(samples), 8000, 1, size);
        return true;
    }
    if (format == "gsm") {
        clip.sampleRate = 8000;
        clip.channels = 1;
        clip.duration = static_cast<int>(size / 33ull) * 20;
        return clip.duration > 0;
    }
    if (format == "g722") {
        clip.sampleRate = 16000;
        clip.channels = 1;
        clip.duration = static_cast<int>(size * 1000ull / 8000ull);
        return true;
    }
    if (format == "g729") {
        clip.sampleRate = 8000;
        clip.channels = 1;
        clip.duration = static_cast<int>(size / 10ull) * 10;
        return clip.duration > 0;
    }
    if (format == "silk") {
        std::size_t body = Match(data, size, 0, "#!SILK_V3") ? 9 : 0;
        if (body > size) {
            return false;
        }
        clip.sampleRate = 8000;
        clip.channels = 1;
        clip.duration = static_cast<int>((size - body) / 20ull) * 20;
        return size > body;
    }
    return false;
}

void WalkMp4(RoboAudioClip& clip, const RoboU8* data, std::size_t size, std::size_t from, std::size_t to, std::string& codec) {
    std::size_t at = from;
    while (at + 8 <= to && at + 8 <= size) {
        RoboU64 box = Read32(data + at, true);
        std::size_t header = 8;
        char name[5] = {};
        std::memcpy(name, data + at + 4, 4);
        if (box == 1 && Has(data, size, at, 16)) {
            box = Read64(data + at + 8, true);
            header = 16;
        }
        if (box == 0) {
            box = to - at;
        }
        if (box < header || at + box > to) {
            break;
        }
        const std::size_t body = at + header;
        const std::size_t end = at + static_cast<std::size_t>(box);
        if (std::strcmp(name, "moov") == 0 || std::strcmp(name, "trak") == 0 || std::strcmp(name, "mdia") == 0 ||
            std::strcmp(name, "minf") == 0 || std::strcmp(name, "stbl") == 0) {
            WalkMp4(clip, data, size, body, end, codec);
        } else if (std::strcmp(name, "mvhd") == 0 && Has(data, size, body, 20)) {
            const int version = data[body];
            if (version == 0 && Has(data, size, body, 20)) {
                const int scale = static_cast<int>(Read32(data + body + 12, true));
                const RoboU32 ticks = Read32(data + body + 16, true);
                if (scale > 0) {
                    clip.duration = Milliseconds(ticks, scale);
                }
            } else if (version == 1 && Has(data, size, body, 32)) {
                const int scale = static_cast<int>(Read32(data + body + 20, true));
                const RoboU64 ticks = Read64(data + body + 24, true);
                if (scale > 0) {
                    clip.duration = Milliseconds(ticks, scale);
                }
            }
        } else if (std::strcmp(name, "stsd") == 0) {
            const char* codecs[] = {"mp4a", "alac", "ac-4", "ec-3", "ac-3", "mlpa", "dtsc", "dtse", "dtsh", "dtsl", "Opus", "fLaC", "samr", "sawb"};
            for (const char* item : codecs) {
                for (std::size_t index = body; index + 4 <= end; ++index) {
                    if (Match(data, size, index, item)) {
                        codec = item;
                        break;
                    }
                }
                if (!codec.empty()) {
                    break;
                }
            }
        }
        at = end;
    }
}

bool ParseMp4(RoboAudioClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (size < 12 || !Match(data, size, 4, "ftyp")) {
        return false;
    }
    std::string codec;
    WalkMp4(clip, data, size, 0, size, codec);
    bool atmos = false;
    for (std::size_t index = 0; index + 5 <= size; ++index) {
        if (Match(data, size, index, "Atmos")) {
            atmos = true;
            break;
        }
    }
    if (codec == "alac") {
        clip.format = "alac";
    } else if (codec == "ac-4") {
        clip.format = "ac4";
    } else if (codec == "mlpa") {
        clip.format = "thd";
    } else if (codec == "dtsh" || codec == "dtsl" || codec == "dtse") {
        clip.format = "dtshd";
    } else if (codec == "dtsc") {
        clip.format = "dts";
    } else if (codec == "ec-3") {
        clip.format = atmos ? "atmos" : "eac3";
    } else if (codec == "ac-3") {
        clip.format = "ac3";
    } else if (codec == "Opus") {
        clip.format = "opus";
    } else if (codec == "fLaC") {
        clip.format = "flac";
    } else if (codec == "sawb") {
        clip.format = "awb";
    } else if (codec == "samr") {
        clip.format = "amr";
    } else if (format == "m4a" || format == "mp4" || format == "aac" || format == "alac") {
        clip.format = format == "aac" ? "aac" : format;
    } else {
        clip.format = "mp4";
    }
    clip.channels = clip.channels == 0 ? 2 : clip.channels;
    return true;
}

bool ParseOgg(RoboAudioClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (!Match(data, size, 0, "OggS")) {
        return false;
    }
    RoboU64 granule = 0;
    int rate = 0;
    int preSkip = 0;
    std::string kind = format == "opus" ? "opus" : (format == "spx" ? "spx" : "ogg");
    std::size_t at = 0;
    while (at + 27 <= size && Match(data, size, at, "OggS")) {
        const RoboU64 page = Read64(data + at + 6, false);
        if (page != ~0ull) {
            granule = page;
        }
        const int segments = data[at + 26];
        if (at + 27 + static_cast<std::size_t>(segments) > size) {
            break;
        }
        int payload = 0;
        for (int index = 0; index < segments; ++index) {
            payload += data[at + 27 + index];
        }
        const std::size_t body = at + 27 + static_cast<std::size_t>(segments);
        if (body + static_cast<std::size_t>(payload) > size) {
            break;
        }
        for (int index = 0; index + 8 < payload; ++index) {
            if (Match(data, size, body + index, "OpusHead") && index + 19 < payload) {
                kind = "opus";
                preSkip = Read16(data + body + index + 10, false);
                rate = 48000;
            } else if (Match(data, size, body + index, "vorbis") && index + 15 < payload) {
                kind = "ogg";
                rate = static_cast<int>(Read32(data + body + index + 11, false));
            } else if (Match(data, size, body + index, "Speex   ") && index + 40 < payload) {
                kind = "spx";
                rate = static_cast<int>(Read32(data + body + index + 36, false));
            }
        }
        at = body + static_cast<std::size_t>(payload);
    }
    if (format == "opus") {
        kind = "opus";
        rate = 48000;
    } else if (format == "spx") {
        kind = "spx";
    } else if (format == "ogg" && kind == "ogg") {
        kind = "ogg";
    }
    clip.format = kind;
    if (rate <= 0) {
        rate = kind == "opus" ? 48000 : 44100;
    }
    clip.sampleRate = rate;
    clip.channels = 1;
    const RoboU64 samples = granule > static_cast<RoboU64>(preSkip) ? granule - static_cast<RoboU64>(preSkip) : 0;
    clip.duration = Milliseconds(samples, rate);
    return true;
}

bool ParseMidi(RoboAudioClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (!Match(data, size, 0, "MThd") || size < 22) {
        return false;
    }
    const int division = Read16(data + 12, true);
    if (division <= 0) {
        return false;
    }
    struct Note {
        int start = 0;
        int end = 0;
        int key = 0;
    };
    std::vector<Note> notes;
    int durationUs = 0;
    std::size_t at = 14;
    while (at + 8 <= size) {
        if (!Match(data, size, at, "MTrk")) {
            break;
        }
        const RoboU32 length = Read32(data + at + 4, true);
        const std::size_t body = at + 8;
        const std::size_t end = body + length <= size ? body + length : size;
        int tempo = 500000;
        int clock = 0;
        int running = 0;
        int held[128];
        for (int& slot : held) {
            slot = -1;
        }
        std::size_t cursor = body;
        while (cursor < end) {
            int delta = 0;
            int guard = 0;
            while (cursor < end && guard < 4) {
                const int byte = data[cursor++];
                delta = (delta << 7) | (byte & 0x7F);
                ++guard;
                if ((byte & 0x80) == 0) {
                    break;
                }
            }
            clock += delta;
            if (cursor >= end) {
                break;
            }
            int status = data[cursor];
            if ((status & 0x80) != 0) {
                ++cursor;
                running = status;
            } else {
                status = running;
            }
            if (status == 0xFF) {
                if (cursor >= end) {
                    break;
                }
                const int meta = data[cursor++];
                int count = 0;
                guard = 0;
                while (cursor < end && guard < 4) {
                    const int byte = data[cursor++];
                    count = (count << 7) | (byte & 0x7F);
                    ++guard;
                    if ((byte & 0x80) == 0) {
                        break;
                    }
                }
                if (meta == 0x51 && count == 3 && cursor + 3 <= end) {
                    tempo = (data[cursor] << 16) | (data[cursor + 1] << 8) | data[cursor + 2];
                }
                if (cursor + static_cast<std::size_t>(count) > end) {
                    break;
                }
                cursor += static_cast<std::size_t>(count);
                continue;
            }
            if ((status & 0xF0) == 0x90 || (status & 0xF0) == 0x80) {
                if (cursor + 1 >= end) {
                    break;
                }
                const int key = data[cursor++];
                const int velocity = data[cursor++];
                const int when = static_cast<int>(static_cast<long long>(clock) * tempo / division);
                if ((status & 0xF0) == 0x90 && velocity > 0) {
                    if (key >= 0 && key < 128) {
                        held[key] = when;
                    }
                } else if (key >= 0 && key < 128 && held[key] >= 0) {
                    notes.push_back({held[key], when, key});
                    held[key] = -1;
                    if (when > durationUs) {
                        durationUs = when;
                    }
                }
                continue;
            }
            if ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) {
                if (cursor < end) {
                    ++cursor;
                }
                continue;
            }
            if (status == 0xF0 || status == 0xF7) {
                while (cursor < end && data[cursor++] != 0xF7) {
                }
                continue;
            }
            if (cursor + 1 < end) {
                cursor += 2;
            } else {
                break;
            }
        }
        at = end;
    }
    clip.format = format == "kar" ? "kar" : "mid";
    clip.sampleRate = 22050;
    clip.channels = 1;
    clip.duration = durationUs / 1000;
    if (notes.empty()) {
        return clip.duration >= 0;
    }
    const int frames = clip.duration > 0 ? static_cast<int>(static_cast<long long>(clip.sampleRate) * clip.duration / 1000) : 0;
    const int keep = frames > ROBO_AUDIO_MAX_STORED_FRAMES ? ROBO_AUDIO_MAX_STORED_FRAMES : frames;
    std::vector<RoboI16> samples(static_cast<std::size_t>(keep), 0);
    for (const Note& note : notes) {
        const int start = static_cast<int>(static_cast<long long>(note.start) * clip.sampleRate / 1000000);
        const int stop = static_cast<int>(static_cast<long long>(note.end) * clip.sampleRate / 1000000);
        const double hz = 440.0 * std::pow(2.0, (note.key - 69) / 12.0);
        for (int index = start; index < stop && index < keep; ++index) {
            const int wave = static_cast<int>(std::sin(2.0 * 3.141592653589793 * hz * (index - start) / clip.sampleRate) * 7000.0);
            int mixed = samples[static_cast<std::size_t>(index)] + wave;
            if (mixed > 32767) {
                mixed = 32767;
            }
            if (mixed < -32768) {
                mixed = -32768;
            }
            samples[static_cast<std::size_t>(index)] = static_cast<RoboI16>(mixed);
        }
    }
    clip.samples = std::move(samples);
    clip.pcm = !clip.samples.empty();
    return true;
}

bool ParseMod(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    if (size < 1084) {
        return false;
    }
    const char* marks[] = {"M.K.", "M!K!", "FLT4", "4CHN", "6CHN", "8CHN"};
    bool found = false;
    for (const char* mark : marks) {
        if (Match(data, size, 1080, mark)) {
            found = true;
        }
    }
    if (!found) {
        return false;
    }
    clip.format = "mod";
    clip.sampleRate = 44100;
    clip.channels = Match(data, size, 1080, "8CHN") ? 8 : (Match(data, size, 1080, "6CHN") ? 6 : 4);
    clip.duration = ModuleMs(data[950], 6, 125);
    return true;
}

bool ParseXm(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    if (!Match(data, size, 0, "Extended Module: ") || size < 80) {
        return false;
    }
    const int orders = Read16(data + 64, false);
    const int speed = Read16(data + 76, false);
    const int tempo = Read16(data + 78, false);
    clip.format = "xm";
    clip.sampleRate = 44100;
    clip.channels = Read16(data + 68, false);
    clip.duration = ModuleMs(orders, speed, tempo);
    return true;
}

bool ParseIt(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    if (!Match(data, size, 0, "IMPM") || size < 0x40) {
        return false;
    }
    clip.format = "it";
    clip.sampleRate = 44100;
    clip.channels = 64;
    clip.duration = ModuleMs(Read16(data + 0x20, false), data[0x3C], data[0x3D]);
    return true;
}

bool ParseS3m(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    if (size < 0x40 || !Match(data, size, 0x2C, "SCRM")) {
        return false;
    }
    clip.format = "s3m";
    clip.sampleRate = 44100;
    clip.channels = 8;
    clip.duration = ModuleMs(Read16(data + 0x20, false), data[0x31], data[0x32]);
    return true;
}

bool ParseNsf(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    if (!Match(data, size, 0, "NESM\x1A") || size < 8) {
        return false;
    }
    clip.format = "nsf";
    clip.songs = data[6] == 0 ? 1 : data[6];
    clip.sampleRate = 44100;
    clip.channels = 1;
    clip.duration = clip.songs * ROBO_AUDIO_NSF_SONG_MS;
    return true;
}

bool ParseSpc(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    if (!Match(data, size, 0, "SNES-SPC700")) {
        return false;
    }
    clip.format = "spc";
    clip.sampleRate = 32000;
    clip.channels = 2;
    clip.duration = ROBO_AUDIO_SPC_MS;
    return true;
}

bool ParseDsf(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    if (!Match(data, size, 0, "DSD ")) {
        return false;
    }
    for (std::size_t at = 0; at + 40 <= size; ++at) {
        if (!Match(data, size, at, "fmt ")) {
            continue;
        }
        const std::size_t body = at + 12;
        if (!Has(data, size, body, 28)) {
            return false;
        }
        const int channels = static_cast<int>(Read32(data + body + 12, false));
        const int rate = static_cast<int>(Read32(data + body + 16, false));
        const RoboU64 frames = Read64(data + body + 24, false);
        if (rate < 1 || channels < 1) {
            return false;
        }
        clip.format = "dsf";
        clip.sampleRate = rate;
        clip.channels = channels;
        clip.duration = Milliseconds(frames, rate);
        return true;
    }
    return false;
}

bool ParseDff(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    if (!Match(data, size, 0, "FRM8") || size < 16) {
        return false;
    }
    int rate = 0;
    int channels = 1;
    RoboU64 bytes = 0;
    std::size_t at = 16;
    while (at + 12 <= size) {
        char name[5] = {};
        std::memcpy(name, data + at, 4);
        const RoboU64 chunk = Read64(data + at + 4, true);
        const std::size_t body = at + 12;
        if (std::strcmp(name, "FS  ") == 0 && Has(data, size, body, 4)) {
            rate = static_cast<int>(Read32(data + body, true));
        } else if (std::strcmp(name, "CHNL") == 0 && Has(data, size, body, 2)) {
            channels = Read16(data + body, true);
        } else if (std::strcmp(name, "DSD ") == 0) {
            bytes = chunk;
        }
        if (chunk > size || at + 12 + chunk > size) {
            break;
        }
        at += 12 + static_cast<std::size_t>(chunk) + (chunk & 1u);
    }
    if (rate < 1) {
        return false;
    }
    clip.format = "dff";
    clip.sampleRate = rate;
    clip.channels = channels < 1 ? 1 : channels;
    clip.duration = Milliseconds(bytes * 8ull / static_cast<RoboU64>(clip.channels), rate);
    return true;
}

bool ParseWma(RoboAudioClip& clip, const RoboU8* data, std::size_t size) {
    const RoboU8 header[] = {0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C};
    const RoboU8 file[] = {0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11, 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65};
    if (size < 16 || std::memcmp(data, header, 16) != 0) {
        return false;
    }
    for (std::size_t at = 0; at + 72 <= size; ++at) {
        if (std::memcmp(data + at, file, 16) == 0) {
            const RoboU64 ticks = Read64(data + at + 64, false);
            clip.format = "wma";
            clip.sampleRate = 44100;
            clip.channels = 2;
            clip.duration = static_cast<int>(ticks / 10000ull);
            return true;
        }
    }
    clip.format = "wma";
    return true;
}

bool ParseEbml(RoboAudioClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (size < 4 || data[0] != 0x1A || data[1] != 0x45 || data[2] != 0xDF || data[3] != 0xA3) {
        return false;
    }
    clip.format = format == "mka" ? "mka" : "webm";
    for (std::size_t at = 0; at + 11 <= size; ++at) {
        if (data[at] == 0x44 && data[at + 1] == 0x89 && data[at + 2] == 0x88) {
            RoboU8 swapped[8];
            for (int index = 0; index < 8; ++index) {
                swapped[index] = data[at + 3 + 7 - index];
            }
            double value = 0;
            std::memcpy(&value, swapped, 8);
            if (value > 0) {
                clip.duration = static_cast<int>(value);
            }
            break;
        }
    }
    clip.sampleRate = 48000;
    clip.channels = 2;
    return true;
}

bool ParseCounted(RoboAudioClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (format == "dts" || format == "dtshd") {
        int frames = 0;
        for (std::size_t at = 0; at + 4 <= size;) {
            const bool sync = data[at] == 0x7F && data[at + 1] == 0xFE && data[at + 2] == 0x80 && data[at + 3] == 0x01;
            if (!sync) {
                ++at;
                continue;
            }
            ++frames;
            at += 96;
        }
        if (frames < 1 && format != "dtshd" && format != "dts") {
            return false;
        }
        clip.format = format.empty() ? "dts" : format;
        clip.sampleRate = 48000;
        clip.channels = 6;
        clip.duration = frames * 512 * 1000 / 48000;
        return frames > 0 || size > 0;
    }
    if (format == "thd") {
        clip.format = "thd";
        clip.sampleRate = 48000;
        clip.channels = 8;
        return size > 0;
    }
    if (format == "ac4") {
        clip.format = "ac4";
        clip.sampleRate = 48000;
        clip.channels = 2;
        return size > 0;
    }
    if (format == "ape" && Match(data, size, 0, "MAC ") && size >= 52) {
        const int version = Read16(data + 4, false);
        std::size_t header = 6;
        if (version >= 3980 && Has(data, size, 8, 4)) {
            header = Read32(data + 8, false);
        }
        if (!Has(data, size, header, 24)) {
            clip.format = "ape";
            return true;
        }
        const RoboU32 blocks = Read32(data + header + 4, false);
        const RoboU32 finalBlocks = Read32(data + header + 8, false);
        const RoboU32 total = Read32(data + header + 12, false);
        const int channels = Read16(data + header + 18, false);
        const int rate = static_cast<int>(Read32(data + header + 20, false));
        const RoboU64 frames = total <= 1 ? finalBlocks : (static_cast<RoboU64>(total) - 1ull) * blocks + finalBlocks;
        clip.format = "ape";
        clip.sampleRate = rate;
        clip.channels = channels;
        clip.duration = Milliseconds(frames, rate);
        return rate > 0;
    }
    if (format == "wv" && Match(data, size, 0, "wvpk") && size >= 28) {
        const RoboU32 frames = Read32(data + 12, false);
        const RoboU32 flags = Read32(data + 24, false);
        const int kRate[16] = {6000, 8000, 9600, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000, 192000, 0};
        const int rate = kRate[(flags >> 23) & 0x0F];
        clip.format = "wv";
        clip.sampleRate = rate;
        clip.channels = ((flags & 4u) != 0) ? 1 : 2;
        clip.duration = Milliseconds(frames, rate);
        return rate > 0;
    }
    if (format == "tta" && Match(data, size, 0, "TTA1") && size >= 18) {
        const int channels = Read16(data + 6, false);
        const int rate = static_cast<int>(Read32(data + 10, false));
        const RoboU32 frames = Read32(data + 14, false);
        clip.format = "tta";
        clip.sampleRate = rate;
        clip.channels = channels;
        clip.duration = Milliseconds(frames, rate);
        return rate > 0 && channels > 0;
    }
    if (format == "tak" && Match(data, size, 0, "tBaK")) {
        clip.format = "tak";
        if (Has(data, size, 4, 10)) {
            const int rate = static_cast<int>(Read32(data + 4, false));
            const RoboU32 frames = Read32(data + 8, false);
            const int channels = Read16(data + 12, false);
            if (rate >= 4000 && rate <= 384000 && channels >= 1 && channels <= 8) {
                clip.sampleRate = rate;
                clip.channels = channels;
                clip.duration = Milliseconds(frames, rate);
            }
        }
        return true;
    }
    if (format == "ofr" && Match(data, size, 0, "OFR ")) {
        clip.format = "ofr";
        if (Has(data, size, 8, 10)) {
            const int rate = static_cast<int>(Read32(data + 8, false));
            const RoboU32 frames = Read32(data + 12, false);
            const int channels = Read16(data + 16, false);
            if (rate >= 4000 && rate <= 384000 && channels >= 1 && channels <= 8) {
                clip.sampleRate = rate;
                clip.channels = channels;
                clip.duration = Milliseconds(frames, rate);
            }
        }
        return true;
    }
    if (format == "als" && Match(data, size, 0, "ALS")) {
        clip.format = "als";
        if (Has(data, size, 4, 10)) {
            const int rate = static_cast<int>(Read32(data + 4, true));
            const RoboU32 frames = Read32(data + 8, true);
            const int channels = Has(data, size, 12, 2) ? Read16(data + 12, true) : 1;
            if (rate > 0) {
                clip.sampleRate = rate;
                clip.channels = channels < 1 ? 1 : channels;
                clip.duration = Milliseconds(frames, rate);
            }
        }
        return true;
    }
    return false;
}

std::string Detect(const RoboU8* data, std::size_t size, const std::string& extension) {
    if (Match(data, size, 0, "RF64")) {
        return "rf64";
    }
    if (Match(data, size, 0, "RIFF") && Match(data, size, 8, "WAVE")) {
        return extension == "bwf" ? "bwf" : "wav";
    }
    if (Match(data, size, 0, "FORM") && (Match(data, size, 8, "AIFF") || Match(data, size, 8, "AIFC"))) {
        return extension == "aif" ? "aif" : "aiff";
    }
    if (Has(data, size, 0, 4) && Read32(data, true) == 0x2E736E64u) {
        return "au";
    }
    if (Match(data, size, 0, "caff")) {
        return "caf";
    }
    if (Match(data, size, 0, "fLaC")) {
        return "flac";
    }
    if (size >= 12 && Match(data, size, 4, "ftyp")) {
        return extension == "m4a" || extension == "aac" || extension == "alac" || extension == "mp4" ? extension : "mp4";
    }
    if (Match(data, size, 0, "OggS")) {
        return extension == "opus" || extension == "spx" ? extension : "ogg";
    }
    if (Match(data, size, 0, "#!AMR-WB\n")) {
        return "awb";
    }
    if (Match(data, size, 0, "#!AMR\n")) {
        return "amr";
    }
    if (Match(data, size, 0, "MAC ")) {
        return "ape";
    }
    if (Match(data, size, 0, "wvpk")) {
        return "wv";
    }
    if (Match(data, size, 0, "TTA1")) {
        return "tta";
    }
    if (Match(data, size, 0, "tBaK")) {
        return "tak";
    }
    if (Match(data, size, 0, "OFR ")) {
        return "ofr";
    }
    if (Match(data, size, 0, "ALS")) {
        return "als";
    }
    if (Match(data, size, 0, "MThd")) {
        return extension == "kar" ? "kar" : "mid";
    }
    if (Match(data, size, 0, "Extended Module: ")) {
        return "xm";
    }
    if (Match(data, size, 0, "IMPM")) {
        return "it";
    }
    if (Match(data, size, 0x2C, "SCRM")) {
        return "s3m";
    }
    if (size >= 1084 && (Match(data, size, 1080, "M.K.") || Match(data, size, 1080, "M!K!") || Match(data, size, 1080, "4CHN"))) {
        return "mod";
    }
    if (Match(data, size, 0, "NESM\x1A")) {
        return "nsf";
    }
    if (Match(data, size, 0, "SNES-SPC700")) {
        return "spc";
    }
    if (Match(data, size, 0, "DSD ")) {
        return "dsf";
    }
    if (Match(data, size, 0, "FRM8")) {
        return "dff";
    }
    if (size >= 16 && data[0] == 0x30 && data[1] == 0x26 && data[2] == 0xB2 && data[3] == 0x75) {
        return "wma";
    }
    if (size >= 4 && data[0] == 0x1A && data[1] == 0x45 && data[2] == 0xDF && data[3] == 0xA3) {
        return extension == "mka" ? "mka" : "webm";
    }
    if (Match(data, size, 0, "#!SILK_V3")) {
        return "silk";
    }
    if (extension == "mp3" || extension == "mp2" || extension == "aac" || Match(data, size, 0, "ID3")) {
        return extension.empty() ? "mp3" : extension;
    }
    if (Has(data, size, 0, 2) && data[0] == 0x0B && data[1] == 0x77) {
        return extension == "eac3" ? "eac3" : "ac3";
    }
    if (Has(data, size, 0, 4) && data[0] == 0x7F && data[1] == 0xFE && data[2] == 0x80 && data[3] == 0x01) {
        return extension == "dtshd" ? "dtshd" : "dts";
    }
    return extension;
}

bool Dispatch(RoboAudioClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (format == "wav" || format == "bwf" || format == "rf64") {
        return ParseWav(clip, data, size, format);
    }
    if (format == "aiff" || format == "aif") {
        return ParseAiff(clip, data, size, format);
    }
    if (format == "au") {
        return ParseAu(clip, data, size);
    }
    if (format == "caf") {
        return ParseCaf(clip, data, size);
    }
    if (format == "flac") {
        return ParseFlac(clip, data, size);
    }
    if (format == "mp3" || format == "mp2") {
        return ParseMpeg(clip, data, size, format);
    }
    if (format == "aac") {
        return ParseAdts(clip, data, size) || ParseMp4(clip, data, size, format);
    }
    if (format == "m4a" || format == "mp4" || format == "alac") {
        return ParseMp4(clip, data, size, format);
    }
    if (format == "ogg" || format == "opus" || format == "spx") {
        return ParseOgg(clip, data, size, format);
    }
    if (format == "amr") {
        return ParseAmr(clip, data, size, false);
    }
    if (format == "awb") {
        return ParseAmr(clip, data, size, true);
    }
    if (format == "ac3" || format == "eac3") {
        return ParseAc3(clip, data, size, format);
    }
    if (format == "pcm" || format == "ulaw" || format == "alaw" || format == "gsm" || format == "g722" || format == "g729" || format == "silk") {
        return ParseRaw(clip, data, size, format);
    }
    if (format == "mid" || format == "kar") {
        return ParseMidi(clip, data, size, format);
    }
    if (format == "mod") {
        return ParseMod(clip, data, size);
    }
    if (format == "xm") {
        return ParseXm(clip, data, size);
    }
    if (format == "it") {
        return ParseIt(clip, data, size);
    }
    if (format == "s3m") {
        return ParseS3m(clip, data, size);
    }
    if (format == "nsf") {
        return ParseNsf(clip, data, size);
    }
    if (format == "spc") {
        return ParseSpc(clip, data, size);
    }
    if (format == "dsf") {
        return ParseDsf(clip, data, size);
    }
    if (format == "dff") {
        return ParseDff(clip, data, size);
    }
    if (format == "wma") {
        return ParseWma(clip, data, size);
    }
    if (format == "webm" || format == "mka") {
        return ParseEbml(clip, data, size, format);
    }
    if (format == "m3u" || format == "m3u8" || format == "pls" || format == "wpl" || format == "asx" || format == "mpd") {
        return ParsePlaylist(clip, data, size, format);
    }
    if (format == "dts" || format == "dtshd" || format == "thd" || format == "ac4" || format == "ape" || format == "wv" ||
        format == "tta" || format == "tak" || format == "ofr" || format == "als" || format == "atmos") {
        return ParseCounted(clip, data, size, format);
    }
    return false;
}

}  // namespace

const char* RoboAudioFormatOf(const std::string& source) {
    const std::string extension = ExtensionOf(source);
    for (const Ext& item : ROBO_AUDIO_EXTENSIONS) {
        if (extension == item.ext) {
            return item.format;
        }
    }
    if (StreamUrl(source)) {
        return "icecast";
    }
    return "";
}

bool RoboAudioRead(const RoboXMLNode& node, RoboAudioNode& audio, std::string& diagnostic) {
    audio = RoboAudioNode{};
    diagnostic.clear();
    if (Lower(node.name) != "audio") {
        diagnostic = Fail("R7904", "'" + node.name + "' is not an Audio. Use Audio.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!Known(entry.first)) {
            diagnostic = Fail("R7901", "Audio attribute '" + entry.first + "' is unknown. Use source, time, playing, and volume.");
            return false;
        }
    }
    if (!node.children.empty()) {
        diagnostic = Fail("R7904", "Audio does not take children.");
        return false;
    }
    audio.source = RoboXMLAttr(node, "source");
    if (audio.source.empty()) {
        audio.source = RoboXMLAttr(node, "src");
    }
    const std::string time = RoboXMLAttr(node, "time");
    if (!time.empty()) {
        int number = 0;
        if (!Integer(time, number) || number < 0) {
            diagnostic = Fail("R7902", "Audio time '" + time + "' is not a clock. Use a whole number of milliseconds.");
            return false;
        }
        audio.time = number;
    }
    const std::string playing = RoboXMLAttr(node, "playing");
    if (!playing.empty() && !Flag(playing, audio.playing)) {
        diagnostic = Fail("R7902", "Audio playing '" + playing + "' is unknown. Use true or false.");
        return false;
    }
    const std::string volume = RoboXMLAttr(node, "volume");
    if (!volume.empty()) {
        int number = 0;
        if (!Integer(volume, number) || number < 0 || number > ROBO_AUDIO_MAX_VOLUME) {
            diagnostic = Fail("R7902", "Audio volume '" + volume + "' is not a volume. Use a whole number from 0 to 100.");
            return false;
        }
        audio.volume = number;
    }
    return true;
}

bool RoboAudioDecode(RoboAudioClip& clip, const void* bytes, std::size_t size, const std::string& source) {
    clip = RoboAudioClip{};
    const RoboU8* data = static_cast<const RoboU8*>(bytes);
    if ((data == nullptr || size == 0) && StreamUrl(source)) {
        clip.format = "icecast";
        clip.live = true;
        return true;
    }
    if (data == nullptr || size == 0) {
        return false;
    }
    if (Icy(data, size)) {
        clip.format = "icecast";
        clip.live = true;
        return true;
    }
    const std::string extension = ExtensionOf(source);
    const std::string hinted = RoboAudioFormatOf(source);
    std::string format = Detect(data, size, extension.empty() ? hinted : extension);
    if (format.empty()) {
        format = hinted;
    }
    if (format.empty()) {
        return false;
    }
    if (!Dispatch(clip, data, size, format)) {
        return false;
    }
    if (clip.format.empty()) {
        clip.format = format;
    }
    return true;
}

bool RoboAudioSeek(RoboAudioClip& clip, int timeMs) {
    if (clip.live) {
        clip.time = 0;
        return true;
    }
    if (timeMs < 0) {
        timeMs = 0;
    }
    if (clip.duration > 0 && timeMs > clip.duration) {
        timeMs = clip.duration;
    }
    clip.time = timeMs;
    return true;
}

void RoboAudioBind(const RoboAudioNode& audio, RoboAudioClip& clip) {
    clip.volume = audio.volume;
    clip.playing = audio.playing;
    RoboAudioSeek(clip, audio.time);
}

bool RoboAudioPlay(const RoboAudioClip& clip, RoboAudioOutput& output) {
    output = RoboAudioOutput{};
    output.format = clip.format;
    output.sampleRate = clip.sampleRate;
    output.channels = clip.channels;
    output.time = clip.time;
    output.volume = clip.volume;
    if (!clip.playing || !clip.pcm || clip.samples.empty() || clip.channels < 1 || clip.sampleRate < 1) {
        return !clip.format.empty();
    }
    const long long frame = static_cast<long long>(clip.time) * clip.sampleRate / 1000;
    const long long index = frame * clip.channels;
    if (index >= static_cast<long long>(clip.samples.size())) {
        return true;
    }
    output.samples.assign(clip.samples.begin() + static_cast<std::size_t>(index), clip.samples.end());
    const int volume = clip.volume < 0 ? 0 : (clip.volume > ROBO_AUDIO_MAX_VOLUME ? ROBO_AUDIO_MAX_VOLUME : clip.volume);
    if (volume != ROBO_AUDIO_MAX_VOLUME) {
        for (RoboI16& sample : output.samples) {
            const int scaled = static_cast<int>(sample) * volume / ROBO_AUDIO_MAX_VOLUME;
            sample = static_cast<RoboI16>(scaled);
        }
    }
    return true;
}
