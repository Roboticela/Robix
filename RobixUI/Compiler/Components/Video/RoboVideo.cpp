#include "RoboVideo.hpp"

#include <cstring>
#include <string>
#include <vector>

namespace {

const char* const ROBO_VIDEO_ATTRIBUTES[] = {
    "id", "name", "source", "src", "time", "playing", "width", "height",
};

struct Ext {
    const char* ext;
    const char* format;
};

const Ext ROBO_VIDEO_EXTENSIONS[] = {
    {"mp4", "mp4"},   {"mkv", "mkv"},   {"avi", "avi"},     {"mov", "mov"},   {"wmv", "wmv"},   {"flv", "flv"},
    {"webm", "webm"}, {"mpeg", "mpeg"}, {"mpg", "mpg"},     {"m4v", "m4v"},   {"3gp", "3gp"},   {"3g2", "3g2"},
    {"vob", "vob"},   {"mxf", "mxf"},   {"r3d", "r3d"},     {"braw", "braw"}, {"ari", "ari"},   {"ts", "ts"},
    {"m2v", "m2v"},   {"m2ts", "m2ts"}, {"mts", "mts"},     {"m3u8", "m3u8"}, {"mpd", "mpd"},   {"m4s", "m4s"},
    {"asf", "asf"},   {"rm", "rm"},     {"rmvb", "rmvb"},   {"dat", "dat"},   {"dvr-ms", "dvr-ms"}, {"wtv", "wtv"},
    {"nsv", "nsv"},   {"fli", "fli"},   {"flc", "flc"},     {"amv", "amv"},   {"mtv", "mtv"},   {"gif", "gif"},
    {"apng", "apng"}, {"webp", "webp"}, {"avif", "avif"},   {"heif", "heif"}, {"heic", "heif"}, {"evo", "evo"},
};

struct CodecName {
    const char* fourcc;
    const char* codec;
};

const CodecName ROBO_VIDEO_CODECS[] = {
    {"avc1", "h264"}, {"avc3", "h264"}, {"hvc1", "h265"}, {"hev1", "h265"}, {"vvc1", "h266"}, {"vvi1", "h266"},
    {"av01", "av1"},  {"vp08", "vp8"},  {"vp09", "vp9"},  {"mp4v", "mpeg4"}, {"theo", "theora"}, {"mjpg", "mjpeg"},
    {"MJPG", "mjpeg"}, {"jpeg", "mjpeg"}, {"apch", "prores"}, {"apcn", "prores"}, {"apcs", "prores"}, {"apco", "prores"},
    {"ap4h", "prores"}, {"AVdn", "dnxhd"}, {"AVdh", "dnxhr"}, {"CFHD", "cineform"}, {"ai1p", "avc-intra"},
    {"ai12", "avc-intra"}, {"ai13", "avc-intra"}, {"ai15", "avc-intra"}, {"ai5p", "avc-intra"}, {"ai5q", "avc-intra"},
    {"ai56", "avc-intra"}, {"H263", "h263"}, {"s263", "h263"}, {"mp1v", "mpeg1"}, {"mp2v", "mpeg2"},
    {"DIVX", "divx"}, {"divx", "divx"}, {"XVID", "xvid"}, {"xvid", "xvid"}, {"WMV3", "wmv9"}, {"wvc1", "vc1"},
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
    for (const char* name : ROBO_VIDEO_ATTRIBUTES) {
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

bool Contains(const RoboU8* data, std::size_t size, const char* text) {
    const std::size_t count = std::strlen(text);
    if (count == 0 || size < count) {
        return false;
    }
    for (std::size_t index = 0; index + count <= size; ++index) {
        if (std::memcmp(data + index, text, count) == 0) {
            return true;
        }
    }
    return false;
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

int Milliseconds(unsigned long long frames, int rate) {
    if (rate <= 0) {
        return 0;
    }
    const unsigned long long ms = frames * 1000ull / static_cast<unsigned long long>(rate);
    if (ms > 2147483647ull) {
        return 2147483647;
    }
    return static_cast<int>(ms);
}

int SecondsMs(const std::string& text) {
    if (text.empty() || text[0] == '-') {
        return -1;
    }
    int whole = 0;
    std::size_t index = 0;
    bool digit = false;
    for (; index < text.size() && text[index] >= '0' && text[index] <= '9'; ++index) {
        whole = whole * 10 + (text[index] - '0');
        digit = true;
    }
    int fraction = 0;
    int scale = 1;
    if (index < text.size() && text[index] == '.') {
        ++index;
        for (; index < text.size() && text[index] >= '0' && text[index] <= '9' && scale < 1000; ++index) {
            fraction = fraction * 10 + (text[index] - '0');
            scale *= 10;
            digit = true;
        }
    }
    if (!digit) {
        return -1;
    }
    return whole * 1000 + fraction * 1000 / scale;
}

std::string CodecOf(const char* fourcc) {
    for (const CodecName& item : ROBO_VIDEO_CODECS) {
        if (std::memcmp(fourcc, item.fourcc, 4) == 0) {
            return item.codec;
        }
    }
    return {};
}

void NoteProjection(RoboVideoClip& clip, const RoboU8* data, std::size_t size) {
    if (Contains(data, size, "VR180")) {
        clip.projection = "vr180";
    } else if (Contains(data, size, "cubemap") || Contains(data, size, "Cubemap")) {
        clip.projection = "cubemap";
    } else if (Contains(data, size, "equirectangular") || Contains(data, size, "spherical") || Contains(data, size, "sv3d")) {
        clip.projection = "equirectangular";
    }
}

bool ParseAvi(RoboVideoClip& clip, const RoboU8* data, std::size_t size) {
    if (!Match(data, size, 0, "RIFF") || !Match(data, size, 8, "AVI ")) {
        return false;
    }
    int usec = 0;
    int declared = 0;
    int width = 0;
    int height = 0;
    int bits = 0;
    int compression = -1;
    bool bottomUp = true;
    for (std::size_t at = 12; at + 8 <= size; ++at) {
        if (Match(data, size, at, "avih") && Has(data, size, at + 8, 40)) {
            usec = static_cast<int>(Read32(data + at + 8, false));
            declared = static_cast<int>(Read32(data + at + 24, false));
            width = static_cast<int>(Read32(data + at + 40, false));
            height = static_cast<int>(Read32(data + at + 44, false));
        } else if (Match(data, size, at, "strf") && Has(data, size, at + 8, 20)) {
            const int storedWidth = static_cast<int>(Read32(data + at + 12, false));
            int storedHeight = static_cast<int>(Read32(data + at + 16, false));
            if (storedHeight < 0) {
                bottomUp = false;
                storedHeight = -storedHeight;
            }
            if (storedWidth > 0 && storedHeight > 0) {
                width = storedWidth;
                height = storedHeight;
            }
            bits = Read16(data + at + 22, false);
            if (Has(data, size, at + 8, 20)) {
                compression = static_cast<int>(Read32(data + at + 24, false));
            }
        }
    }
    if (width < 1 || height < 1) {
        return false;
    }
    clip.format = "avi";
    clip.width = width;
    clip.height = height;
    clip.codec = compression == 0 ? "rgb" : (compression == 0x47504A4D ? "mjpeg" : "video");
    const int stride = bits >= 8 ? ((width * bits + 31) / 32) * 4 : 0;
    const int frameMs = usec > 0 ? usec / 1000 : 100;
    std::size_t movi = 0;
    for (std::size_t at = 12; at + 4 <= size; ++at) {
        if (Match(data, size, at, "movi")) {
            movi = at + 4;
            break;
        }
    }
    if (movi > 0 && compression == 0 && (bits == 24 || bits == 32) && stride > 0) {
        std::size_t at = movi;
        while (at + 8 <= size) {
            const bool dib = Match(data, size, at, "00db") || Match(data, size, at, "00dc");
            const RoboU32 chunk = Read32(data + at + 4, false);
            if (!dib || chunk < static_cast<RoboU32>(stride * height) || at + 8 + chunk > size) {
                if (!dib && (data[at] < '0' || data[at] > '9')) {
                    break;
                }
                if (chunk < 8 || at + 8 + chunk > size) {
                    break;
                }
                at += 8 + chunk + (chunk & 1u);
                continue;
            }
            const std::size_t body = at + 8;
            for (int y = 0; y < height; ++y) {
                const int sourceRow = bottomUp ? height - 1 - y : y;
                for (int x = 0; x < width; ++x) {
                    const std::size_t pos = body + static_cast<std::size_t>(sourceRow) * static_cast<std::size_t>(stride) +
                                            static_cast<std::size_t>(x) * (bits / 8);
                    const RoboU32 blue = data[pos];
                    const RoboU32 green = data[pos + 1];
                    const RoboU32 red = data[pos + 2];
                    clip.frames.push_back(0xFF000000u | (red << 16) | (green << 8) | blue);
                }
            }
            ++clip.frameCount;
            at += 8 + chunk + (chunk & 1u);
        }
    }
    if (clip.frameCount < 1) {
        clip.frameCount = declared > 0 ? declared : 1;
    }
    clip.frameMs = frameMs < 1 ? 1 : frameMs;
    clip.duration = clip.frameCount * clip.frameMs;
    return true;
}

void WalkMp4(RoboVideoClip& clip, const RoboU8* data, std::size_t size, std::size_t from, std::size_t to) {
    std::size_t at = from;
    while (at + 8 <= to && at + 8 <= size) {
        unsigned long long box = Read32(data + at, true);
        std::size_t header = 8;
        char name[5] = {};
        std::memcpy(name, data + at + 4, 4);
        if (box == 1 && Has(data, size, at, 16)) {
            unsigned long long large = 0;
            for (int index = 0; index < 8; ++index) {
                large = (large << 8) | data[at + 8 + index];
            }
            box = large;
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
            WalkMp4(clip, data, size, body, end);
        } else if (std::strcmp(name, "mvhd") == 0 && Has(data, size, body, 20) && data[body] == 0) {
            const int scale = static_cast<int>(Read32(data + body + 12, true));
            const RoboU32 ticks = Read32(data + body + 16, true);
            if (scale > 0) {
                clip.duration = Milliseconds(ticks, scale);
            }
        } else if (std::strcmp(name, "tkhd") == 0 && data[body] == 0 && Has(data, size, body, 84)) {
            const int storedWidth = static_cast<int>(Read32(data + body + 76, true) >> 16);
            const int storedHeight = static_cast<int>(Read32(data + body + 80, true) >> 16);
            if (storedWidth > 0 && storedHeight > 0) {
                clip.width = storedWidth;
                clip.height = storedHeight;
            }
        } else if (std::strcmp(name, "stsd") == 0) {
            for (std::size_t index = body; index + 4 <= end; ++index) {
                char fourcc[5] = {};
                std::memcpy(fourcc, data + index, 4);
                const std::string codec = CodecOf(fourcc);
                if (!codec.empty()) {
                    clip.codec = codec;
                    break;
                }
            }
        }
        at = end;
    }
}

bool ParseMp4(RoboVideoClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (size < 12 || !Match(data, size, 4, "ftyp")) {
        return false;
    }
    clip.format = format.empty() ? "mp4" : format;
    if (Contains(data, size, "avif")) {
        clip.format = "avif";
    } else if (Contains(data, size, "heic") || Contains(data, size, "mif1")) {
        clip.format = format == "heif" ? "heif" : clip.format;
    }
    WalkMp4(clip, data, size, 0, size);
    if (Contains(data, size, "XAVC")) {
        clip.codec = "xavc";
    }
    NoteProjection(clip, data, size);
    if (clip.codec.empty()) {
        clip.codec = "h264";
    }
    return true;
}

bool ParseEbml(RoboVideoClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (size < 4 || data[0] != 0x1A || data[1] != 0x45 || data[2] != 0xDF || data[3] != 0xA3) {
        return false;
    }
    clip.format = format == "webm" ? "webm" : "mkv";
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
    if (Contains(data, size, "V_MPEG4/ISO/AVC") || Contains(data, size, "V_MPEGH/ISO/HEVC")) {
        clip.codec = Contains(data, size, "HEVC") ? "h265" : "h264";
    } else if (Contains(data, size, "V_AV1")) {
        clip.codec = "av1";
    } else if (Contains(data, size, "V_VP9")) {
        clip.codec = "vp9";
    } else if (Contains(data, size, "V_VP8")) {
        clip.codec = "vp8";
    } else if (Contains(data, size, "V_THEORA")) {
        clip.codec = "theora";
    }
    NoteProjection(clip, data, size);
    return true;
}

bool ParseFlv(RoboVideoClip& clip, const RoboU8* data, std::size_t size) {
    if (!Match(data, size, 0, "FLV") || size < 9) {
        return false;
    }
    const int header = static_cast<int>(Read32(data + 5, true));
    std::size_t at = header > 0 && static_cast<std::size_t>(header) + 4 < size ? static_cast<std::size_t>(header) + 4 : 13;
    int last = 0;
    while (at + 11 <= size) {
        const int tag = data[at];
        const int bytes = (data[at + 1] << 16) | (data[at + 2] << 8) | data[at + 3];
        const int stamp = ((data[at + 7] << 24) | (data[at + 4] << 16) | (data[at + 5] << 8) | data[at + 6]);
        if (bytes < 0 || at + 11 + static_cast<std::size_t>(bytes) + 4 > size) {
            break;
        }
        if (tag == 9 || tag == 8) {
            last = stamp;
        }
        at += 15 + static_cast<std::size_t>(bytes);
    }
    clip.format = "flv";
    clip.codec = "video";
    clip.duration = last;
    return true;
}

bool ParseGif(RoboVideoClip& clip, const RoboU8* data, std::size_t size) {
    if (!(Match(data, size, 0, "GIF87a") || Match(data, size, 0, "GIF89a")) || size < 13) {
        return false;
    }
    clip.format = "gif";
    clip.codec = "gif";
    clip.width = Read16(data + 6, false);
    clip.height = Read16(data + 8, false);
    std::size_t at = 13;
    if ((data[10] & 0x80) != 0) {
        at += static_cast<std::size_t>(3 * (1 << ((data[10] & 7) + 1)));
    }
    int delay = 0;
    while (at < size) {
        if (data[at] == 0x3B) {
            break;
        }
        if (data[at] == 0x21 && at + 8 < size && data[at + 1] == 0xF9) {
            delay += Read16(data + at + 4, false) * 10;
            at += 8;
            continue;
        }
        if (data[at] == 0x21) {
            at += 2;
            while (at < size && data[at] != 0) {
                at += 1 + data[at];
            }
            if (at < size) {
                ++at;
            }
            continue;
        }
        if (data[at] == 0x2C && at + 10 < size) {
            at += 10;
            if (at < size && (data[at - 1] & 0x80) != 0) {
                at += static_cast<std::size_t>(3 * (1 << ((data[at - 1] & 7) + 1)));
            }
            if (at < size) {
                ++at;
            }
            while (at < size && data[at] != 0) {
                at += 1 + data[at];
            }
            if (at < size) {
                ++at;
            }
            ++clip.frameCount;
            continue;
        }
        break;
    }
    if (clip.frameCount < 1) {
        clip.frameCount = 1;
    }
    clip.duration = delay > 0 ? delay : clip.frameCount * 100;
    clip.frameMs = clip.frameCount > 0 ? clip.duration / clip.frameCount : 100;
    return clip.width > 0 && clip.height > 0;
}

bool ParsePng(RoboVideoClip& clip, const RoboU8* data, std::size_t size) {
    const RoboU8 signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (size < 8 || std::memcmp(data, signature, 8) != 0) {
        return false;
    }
    bool animated = false;
    std::size_t at = 8;
    while (at + 12 <= size) {
        const RoboU32 length = Read32(data + at, true);
        if (at + 12 + length > size) {
            break;
        }
        if (Match(data, size, at + 4, "IHDR") && length >= 8) {
            clip.width = static_cast<int>(Read32(data + at + 8, true));
            clip.height = static_cast<int>(Read32(data + at + 12, true));
        } else if (Match(data, size, at + 4, "acTL")) {
            animated = true;
            if (length >= 4) {
                clip.frameCount = static_cast<int>(Read32(data + at + 8, true));
            }
        } else if (Match(data, size, at + 4, "fcTL") && length >= 26) {
            const int numer = Read16(data + at + 8 + 20, true);
            int denom = Read16(data + at + 8 + 22, true);
            if (denom <= 0) {
                denom = 100;
            }
            clip.duration += numer * 1000 / denom;
        }
        at += 12 + length;
    }
    if (!animated) {
        return false;
    }
    clip.format = "apng";
    clip.codec = "apng";
    if (clip.frameCount < 1) {
        clip.frameCount = 1;
    }
    return true;
}

bool ParseWebp(RoboVideoClip& clip, const RoboU8* data, std::size_t size) {
    if (!Match(data, size, 0, "RIFF") || !Match(data, size, 8, "WEBP")) {
        return false;
    }
    clip.format = "webp";
    clip.codec = "webp";
    for (std::size_t at = 12; at + 8 <= size;) {
        char name[5] = {};
        std::memcpy(name, data + at, 4);
        const RoboU32 chunk = Read32(data + at + 4, false);
        if (std::strcmp(name, "ANMF") == 0 && Has(data, size, at + 8, 16)) {
            const int numer = (data[at + 8 + 12]) | (data[at + 8 + 13] << 8) | (data[at + 8 + 14] << 16);
            clip.duration += numer;
            ++clip.frameCount;
        }
        if (chunk > size || at + 8 + chunk > size) {
            break;
        }
        at += 8 + chunk + (chunk & 1u);
    }
    if (clip.frameCount < 1) {
        clip.frameCount = 1;
    }
    return true;
}

bool ParseFli(RoboVideoClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (size < 128) {
        return false;
    }
    const int magic = Read16(data + 4, false);
    if (magic != 0xAF11 && magic != 0xAF12) {
        return false;
    }
    clip.format = magic == 0xAF12 || format == "flc" ? "flc" : "fli";
    clip.codec = clip.format;
    clip.frameCount = Read16(data + 6, false);
    clip.width = Read16(data + 8, false);
    clip.height = Read16(data + 10, false);
    const int speed = Read16(data + 16, false);
    if (clip.frameCount < 1 || clip.width < 1 || clip.height < 1) {
        return false;
    }
    clip.frameMs = magic == 0xAF11 ? speed * 1000 / 70 : speed;
    if (clip.frameMs < 1) {
        clip.frameMs = 1;
    }
    clip.duration = clip.frameCount * clip.frameMs;
    return true;
}

bool ParseMpeg(RoboVideoClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    bool sequence = false;
    bool pack = false;
    for (std::size_t at = 0; at + 4 <= size && at < 262144; ++at) {
        if (data[at] == 0 && data[at + 1] == 0 && data[at + 2] == 1 && data[at + 3] == 0xB3 && at + 7 <= size) {
            const int word = (data[at + 4] << 16) | (data[at + 5] << 8) | data[at + 6];
            clip.width = (word >> 12) & 0xFFF;
            clip.height = word & 0xFFF;
            sequence = true;
            break;
        }
        if (data[at] == 0 && data[at + 1] == 0 && data[at + 2] == 1 && data[at + 3] == 0xBA) {
            pack = true;
        }
    }
    if (!sequence && !pack && format != "m2v" && format != "vob" && format != "evo" && format != "dat") {
        return false;
    }
    clip.format = format.empty() ? (pack ? "mpg" : "mpeg") : format;
    clip.codec = format == "m2v" || format == "vob" || format == "evo" || format == "dat" ? "mpeg2" : "mpeg1";
    return sequence || pack || size > 0;
}

bool ParseTs(RoboVideoClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (size < 188 || data[0] != 0x47) {
        return false;
    }
    clip.format = format.empty() ? "ts" : format;
    clip.codec = "h264";
    return true;
}

bool ParseAsf(RoboVideoClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    const RoboU8 header[] = {0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C};
    const RoboU8 file[] = {0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11, 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65};
    if (size < 16 || std::memcmp(data, header, 16) != 0) {
        return false;
    }
    clip.format = format == "asf" || format == "dvr-ms" ? format : "wmv";
    clip.codec = "vc1";
    for (std::size_t at = 0; at + 72 <= size; ++at) {
        if (std::memcmp(data + at, file, 16) == 0) {
            unsigned long long ticks = 0;
            for (int index = 7; index >= 0; --index) {
                ticks = (ticks << 8) | data[at + 64 + index];
            }
            clip.duration = static_cast<int>(ticks / 10000ull);
            return true;
        }
    }
    return true;
}

bool ParsePlaylist(RoboVideoClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    const std::string text(reinterpret_cast<const char*>(data), size);
    const std::string folded = Lower(text);
    clip.format = format;
    if (format == "m3u8") {
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
            if (Lower(line).rfind("#extinf:", 0) == 0) {
                const int ms = SecondsMs(line.substr(8));
                if (ms > 0) {
                    clip.duration += ms;
                }
            }
            at = end + 1;
        }
        clip.live = folded.find("#ext-x-playlist-type:vod") == std::string::npos && folded.find(".m3u8") != std::string::npos && clip.duration == 0;
        return folded.find("#extm3u") != std::string::npos || folded.find("#extinf") != std::string::npos;
    }
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

bool ParseMarked(RoboVideoClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    clip.format = format;
    if (format == "mxf") {
        clip.codec = "mpeg2";
        if (Contains(data, size, "AVdh")) {
            clip.codec = "dnxhr";
        } else if (Contains(data, size, "AVdn")) {
            clip.codec = "dnxhd";
        } else if (Contains(data, size, "XAVC")) {
            clip.codec = "xavc";
        } else if (Contains(data, size, "ai5q") || Contains(data, size, "ai1p") || Contains(data, size, "ai12")) {
            clip.codec = "avc-intra";
        }
        return size >= 4 && data[0] == 0x06 && data[1] == 0x0E && data[2] == 0x2B && data[3] == 0x34;
    }
    if (format == "r3d") {
        clip.codec = "redcode";
        return Match(data, size, 0, "RED1") || Match(data, size, 0, "RED2");
    }
    if (format == "braw") {
        clip.codec = "braw";
        return Match(data, size, 0, "BRAW");
    }
    if (format == "ari") {
        clip.codec = "arriraw";
        return Match(data, size, 0, "ARRI");
    }
    if (format == "rm" || format == "rmvb") {
        clip.codec = "rv";
        return Match(data, size, 0, ".RMF");
    }
    if (format == "nsv") {
        return Match(data, size, 0, "NSVf");
    }
    if (format == "amv") {
        clip.codec = "mjpeg";
        return Match(data, size, 0, "AMV");
    }
    if (format == "mtv") {
        return Match(data, size, 0, "AMV") || Match(data, size, 0, "MTV");
    }
    if (format == "wtv") {
        clip.codec = "mpeg2";
        return size > 16;
    }
    return false;
}

std::string Detect(const RoboU8* data, std::size_t size, const std::string& extension) {
    if (Match(data, size, 0, "RIFF") && Match(data, size, 8, "AVI ")) {
        return "avi";
    }
    if (Match(data, size, 0, "RIFF") && Match(data, size, 8, "WEBP")) {
        return "webp";
    }
    if (size >= 12 && Match(data, size, 4, "ftyp")) {
        if (extension == "mov" || extension == "m4v" || extension == "3gp" || extension == "3g2" || extension == "m4s" ||
            extension == "avif" || extension == "heif" || extension == "mp4") {
            return extension == "heif" ? "heif" : extension;
        }
        return "mp4";
    }
    if (size >= 4 && data[0] == 0x1A && data[1] == 0x45 && data[2] == 0xDF && data[3] == 0xA3) {
        return extension == "webm" ? "webm" : "mkv";
    }
    if (Match(data, size, 0, "FLV")) {
        return "flv";
    }
    if (Match(data, size, 0, "GIF87a") || Match(data, size, 0, "GIF89a")) {
        return "gif";
    }
    if (size >= 4 && data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
        return "apng";
    }
    if (size >= 4 && data[0] == 0x06 && data[1] == 0x0E && data[2] == 0x2B && data[3] == 0x34) {
        return "mxf";
    }
    if (size >= 6 && Read16(data + 4, false) == 0xAF11) {
        return "fli";
    }
    if (size >= 6 && Read16(data + 4, false) == 0xAF12) {
        return "flc";
    }
    if (Match(data, size, 0, ".RMF")) {
        return extension == "rmvb" ? "rmvb" : "rm";
    }
    if (Match(data, size, 0, "NSVf")) {
        return "nsv";
    }
    if (Match(data, size, 0, "RED1") || Match(data, size, 0, "RED2")) {
        return "r3d";
    }
    if (Match(data, size, 0, "BRAW")) {
        return "braw";
    }
    if (Match(data, size, 0, "ARRI")) {
        return "ari";
    }
    if (size >= 16 && data[0] == 0x30 && data[1] == 0x26 && data[2] == 0xB2 && data[3] == 0x75) {
        return extension == "asf" || extension == "dvr-ms" ? extension : "wmv";
    }
    if (size >= 188 && data[0] == 0x47 && (extension == "ts" || extension == "m2ts" || extension == "mts" || extension.empty())) {
        return extension.empty() ? "ts" : extension;
    }
    return extension;
}

bool Dispatch(RoboVideoClip& clip, const RoboU8* data, std::size_t size, const std::string& format) {
    if (format == "avi") {
        return ParseAvi(clip, data, size);
    }
    if (format == "mp4" || format == "mov" || format == "m4v" || format == "3gp" || format == "3g2" || format == "m4s" ||
        format == "avif" || format == "heif") {
        return ParseMp4(clip, data, size, format);
    }
    if (format == "mkv" || format == "webm") {
        return ParseEbml(clip, data, size, format);
    }
    if (format == "flv") {
        return ParseFlv(clip, data, size);
    }
    if (format == "gif") {
        return ParseGif(clip, data, size);
    }
    if (format == "apng") {
        return ParsePng(clip, data, size);
    }
    if (format == "webp") {
        return ParseWebp(clip, data, size);
    }
    if (format == "fli" || format == "flc") {
        return ParseFli(clip, data, size, format);
    }
    if (format == "mpeg" || format == "mpg" || format == "m2v" || format == "vob" || format == "evo" || format == "dat") {
        return ParseMpeg(clip, data, size, format);
    }
    if (format == "ts" || format == "m2ts" || format == "mts") {
        return ParseTs(clip, data, size, format);
    }
    if (format == "wmv" || format == "asf" || format == "dvr-ms") {
        return ParseAsf(clip, data, size, format);
    }
    if (format == "m3u8" || format == "mpd") {
        return ParsePlaylist(clip, data, size, format);
    }
    if (format == "mxf" || format == "r3d" || format == "braw" || format == "ari" || format == "rm" || format == "rmvb" ||
        format == "nsv" || format == "amv" || format == "mtv" || format == "wtv") {
        return ParseMarked(clip, data, size, format);
    }
    return false;
}

int FrameIndex(const RoboVideoClip& clip) {
    if (clip.frameCount < 1 || clip.frames.empty() || clip.width < 1 || clip.height < 1) {
        return 0;
    }
    const int step = clip.frameMs > 0 ? clip.frameMs : 1;
    int index = clip.time / step;
    const int stored = static_cast<int>(clip.frames.size() / (static_cast<std::size_t>(clip.width) * static_cast<std::size_t>(clip.height)));
    if (stored < 1) {
        return 0;
    }
    if (index >= stored) {
        index = stored - 1;
    }
    if (index < 0) {
        index = 0;
    }
    return index;
}

}  // namespace

const char* RoboVideoFormatOf(const std::string& source) {
    const std::string extension = ExtensionOf(source);
    for (const Ext& item : ROBO_VIDEO_EXTENSIONS) {
        if (extension == item.ext) {
            return item.format;
        }
    }
    return "";
}

bool RoboVideoRead(const RoboXMLNode& node, RoboVideoNode& video, std::string& diagnostic) {
    video = RoboVideoNode{};
    diagnostic.clear();
    if (Lower(node.name) != "video") {
        diagnostic = Fail("R7804", "'" + node.name + "' is not a Video. Use Video.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!Known(entry.first)) {
            diagnostic = Fail("R7801", "Video attribute '" + entry.first + "' is unknown. Use source, time, and playing.");
            return false;
        }
    }
    if (!node.children.empty()) {
        diagnostic = Fail("R7804", "Video does not take children.");
        return false;
    }
    video.source = RoboXMLAttr(node, "source");
    if (video.source.empty()) {
        video.source = RoboXMLAttr(node, "src");
    }
    const std::string time = RoboXMLAttr(node, "time");
    if (!time.empty()) {
        int number = 0;
        if (!Integer(time, number) || number < 0) {
            diagnostic = Fail("R7802", "Video time '" + time + "' is not a clock. Use a whole number of milliseconds.");
            return false;
        }
        video.time = number;
    }
    const std::string playing = RoboXMLAttr(node, "playing");
    if (!playing.empty() && !Flag(playing, video.playing)) {
        diagnostic = Fail("R7802", "Video playing '" + playing + "' is unknown. Use true or false.");
        return false;
    }
    const std::string width = RoboXMLAttr(node, "width");
    if (!width.empty()) {
        int number = 0;
        if (!Integer(width, number) || number <= 0 || number > 4096) {
            diagnostic = Fail("R7802", "Video width '" + width + "' is not a size. Use a whole number of pixels.");
            return false;
        }
        video.width = number;
        video.hasWidth = true;
    }
    const std::string height = RoboXMLAttr(node, "height");
    if (!height.empty()) {
        int number = 0;
        if (!Integer(height, number) || number <= 0 || number > 4096) {
            diagnostic = Fail("R7802", "Video height '" + height + "' is not a size. Use a whole number of pixels.");
            return false;
        }
        video.height = number;
        video.hasHeight = true;
    }
    return true;
}

bool RoboVideoDecode(RoboVideoClip& clip, const void* bytes, std::size_t size, const std::string& source) {
    clip = RoboVideoClip{};
    const RoboU8* data = static_cast<const RoboU8*>(bytes);
    if (data == nullptr || size == 0) {
        return false;
    }
    const std::string extension = ExtensionOf(source);
    const std::string hinted = RoboVideoFormatOf(source);
    std::string format = Detect(data, size, extension.empty() ? hinted : extension);
    if (format.empty()) {
        format = hinted;
    }
    if (format.empty() || !Dispatch(clip, data, size, format)) {
        return false;
    }
    if (clip.format.empty()) {
        clip.format = format;
    }
    return true;
}

bool RoboVideoSeek(RoboVideoClip& clip, int timeMs) {
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

void RoboVideoBind(const RoboVideoNode& video, RoboVideoClip& clip) {
    clip.playing = video.playing;
    RoboVideoSeek(clip, video.time);
}

bool RoboVideoPaint(const RoboVideoClip& clip, RoboVideoBox& box) {
    if (box.width < 1 || box.height < 1 || clip.width < 1 || clip.height < 1 || clip.frames.empty()) {
        return false;
    }
    const int index = FrameIndex(clip);
    const std::size_t count = static_cast<std::size_t>(clip.width) * static_cast<std::size_t>(clip.height);
    const std::size_t begin = static_cast<std::size_t>(index) * count;
    if (begin + count > clip.frames.size()) {
        return false;
    }
    box.pixels.assign(static_cast<std::size_t>(box.width) * static_cast<std::size_t>(box.height), 0);
    for (int y = 0; y < box.height; ++y) {
        const int sourceY = y * clip.height / box.height;
        for (int x = 0; x < box.width; ++x) {
            const int sourceX = x * clip.width / box.width;
            box.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(box.width) + static_cast<std::size_t>(x)] =
                clip.frames[begin + static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(clip.width) + static_cast<std::size_t>(sourceX)];
        }
    }
    return true;
}

bool RoboVideoPixel(const RoboVideoBox& box, int x, int y, RoboU32& color) {
    if (x < 0 || y < 0 || x >= box.width || y >= box.height) {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(box.width) + static_cast<std::size_t>(x);
    if (index >= box.pixels.size()) {
        return false;
    }
    color = box.pixels[index];
    return true;
}
