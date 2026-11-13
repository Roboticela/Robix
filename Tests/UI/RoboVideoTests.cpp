#include "../../RobixUI/Compiler/Components/Video/RoboVideo.hpp"
#include "../../RobixUI/Compiler/RoboXML.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

int gPassed = 0;
int gFailed = 0;

void Check(bool condition, const char* description) {
    if (condition) {
        ++gPassed;
        return;
    }
    ++gFailed;
    std::cerr << "[FAIL] " << description << "\n";
}

bool Load(const std::string& source, RoboVideoNode& video, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboVideoRead(document.root, video, diagnostic);
}

void Add(std::vector<RoboU8>& bytes, const void* data, std::size_t size) {
    const auto* raw = static_cast<const RoboU8*>(data);
    bytes.insert(bytes.end(), raw, raw + size);
}

void Text(std::vector<RoboU8>& bytes, const char* text) {
    Add(bytes, text, std::strlen(text));
}

void U8(std::vector<RoboU8>& bytes, int value) {
    bytes.push_back(static_cast<RoboU8>(value));
}

void U16(std::vector<RoboU8>& bytes, int value, bool big) {
    if (big) {
        U8(bytes, value >> 8);
        U8(bytes, value);
    } else {
        U8(bytes, value);
        U8(bytes, value >> 8);
    }
}

void U32(std::vector<RoboU8>& bytes, std::uint32_t value, bool big) {
    for (int shift = 0; shift < 32; shift += 8) {
        const int from = big ? 24 - shift : shift;
        U8(bytes, static_cast<int>(value >> from));
    }
}

void U64(std::vector<RoboU8>& bytes, std::uint64_t value, bool big) {
    for (int shift = 0; shift < 64; shift += 8) {
        const int from = big ? 56 - shift : shift;
        U8(bytes, static_cast<int>(value >> from));
    }
}

void F64(std::vector<RoboU8>& bytes, double value) {
    RoboU8 raw[8];
    std::memcpy(raw, &value, 8);
    for (int index = 7; index >= 0; --index) {
        bytes.push_back(raw[index]);
    }
}

void Chunk(std::vector<RoboU8>& bytes, const char* name, const std::vector<RoboU8>& body, bool big) {
    Text(bytes, name);
    U32(bytes, static_cast<std::uint32_t>(body.size()), big);
    Add(bytes, body.data(), body.size());
    if ((body.size() & 1u) != 0) {
        U8(bytes, 0);
    }
}

std::vector<RoboU8> Box(const char* name, const std::vector<RoboU8>& body) {
    std::vector<RoboU8> bytes;
    U32(bytes, static_cast<std::uint32_t>(8 + body.size()), true);
    Text(bytes, name);
    Add(bytes, body.data(), body.size());
    return bytes;
}

void Join(std::vector<RoboU8>& bytes, const std::vector<RoboU8>& more) {
    Add(bytes, more.data(), more.size());
}

std::vector<RoboU8> Avi(int width, int height, int usec, RoboU32 first, RoboU32 second) {
    auto putAt = [](std::vector<RoboU8>& raw, int offset, std::uint32_t value) {
        raw[static_cast<std::size_t>(offset)] = static_cast<RoboU8>(value);
        raw[static_cast<std::size_t>(offset + 1)] = static_cast<RoboU8>(value >> 8);
        raw[static_cast<std::size_t>(offset + 2)] = static_cast<RoboU8>(value >> 16);
        raw[static_cast<std::size_t>(offset + 3)] = static_cast<RoboU8>(value >> 24);
    };
    std::vector<RoboU8> avih(56, 0);
    putAt(avih, 0, static_cast<std::uint32_t>(usec));
    putAt(avih, 16, 2);
    putAt(avih, 32, static_cast<std::uint32_t>(width));
    putAt(avih, 36, static_cast<std::uint32_t>(height));
    std::vector<RoboU8> strf(40, 0);
    putAt(strf, 0, 40);
    putAt(strf, 4, static_cast<std::uint32_t>(width));
    putAt(strf, 8, static_cast<std::uint32_t>(height));
    strf[12] = 1;
    strf[14] = 24;
    const int stride = ((width * 24 + 31) / 32) * 4;
    auto frame = [&](RoboU32 color) {
        std::vector<RoboU8> pixels(static_cast<std::size_t>(stride * height), 0);
        const RoboU8 blue = static_cast<RoboU8>(color);
        const RoboU8 green = static_cast<RoboU8>(color >> 8);
        const RoboU8 red = static_cast<RoboU8>(color >> 16);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t pos = static_cast<std::size_t>(y) * static_cast<std::size_t>(stride) + static_cast<std::size_t>(x) * 3;
                pixels[pos] = blue;
                pixels[pos + 1] = green;
                pixels[pos + 2] = red;
            }
        }
        return pixels;
    };
    std::vector<RoboU8> movi;
    Text(movi, "movi");
    Chunk(movi, "00db", frame(first), false);
    Chunk(movi, "00db", frame(second), false);
    std::vector<RoboU8> payload;
    Text(payload, "AVI ");
    Chunk(payload, "avih", avih, false);
    Chunk(payload, "strf", strf, false);
    Chunk(payload, "LIST", movi, false);
    std::vector<RoboU8> bytes;
    Text(bytes, "RIFF");
    U32(bytes, static_cast<std::uint32_t>(payload.size()), false);
    Join(bytes, payload);
    return bytes;
}

std::vector<RoboU8> Mp4(const char* fourcc, const char* note, const char* brand) {
    std::vector<RoboU8> ftyp;
    Text(ftyp, brand);
    U32(ftyp, 0, true);
    Text(ftyp, brand);
    auto stampAt = [](std::vector<RoboU8>& raw, int offset, std::uint32_t value) {
        raw[static_cast<std::size_t>(offset)] = static_cast<RoboU8>(value >> 24);
        raw[static_cast<std::size_t>(offset + 1)] = static_cast<RoboU8>(value >> 16);
        raw[static_cast<std::size_t>(offset + 2)] = static_cast<RoboU8>(value >> 8);
        raw[static_cast<std::size_t>(offset + 3)] = static_cast<RoboU8>(value);
    };
    std::vector<RoboU8> mvhd(32, 0);
    stampAt(mvhd, 12, 1000);
    stampAt(mvhd, 16, 1500);
    std::vector<RoboU8> tkhd(84, 0);
    stampAt(tkhd, 76, 320u << 16);
    stampAt(tkhd, 80, 180u << 16);
    std::vector<RoboU8> stsd;
    Text(stsd, fourcc);
    std::vector<RoboU8> stbl;
    Join(stbl, Box("stsd", stsd));
    std::vector<RoboU8> minf;
    Join(minf, Box("stbl", stbl));
    std::vector<RoboU8> mdia;
    Join(mdia, Box("minf", minf));
    std::vector<RoboU8> trak;
    Join(trak, Box("tkhd", tkhd));
    Join(trak, Box("mdia", mdia));
    std::vector<RoboU8> moov;
    Join(moov, Box("mvhd", mvhd));
    Join(moov, Box("trak", trak));
    std::vector<RoboU8> bytes;
    Join(bytes, Box("ftyp", ftyp));
    Join(bytes, Box("moov", moov));
    if (note != nullptr && note[0] != '\0') {
        Join(bytes, Box("free", std::vector<RoboU8>(note, note + std::strlen(note))));
    }
    return bytes;
}

std::vector<RoboU8> Gif() {
    std::vector<RoboU8> bytes;
    Text(bytes, "GIF89a");
    U16(bytes, 2, false);
    U16(bytes, 2, false);
    U8(bytes, 0);
    U8(bytes, 0);
    U8(bytes, 0);
    auto frame = [&](int delay) {
        Text(bytes, "\x21\xF9\x04");
        U8(bytes, 0);
        U16(bytes, delay, false);
        U8(bytes, 0);
        U8(bytes, 0);
        Text(bytes, "\x2C");
        U16(bytes, 0, false);
        U16(bytes, 0, false);
        U16(bytes, 2, false);
        U16(bytes, 2, false);
        U8(bytes, 0);
        U8(bytes, 2);
        U8(bytes, 0);
    };
    frame(10);
    frame(20);
    U8(bytes, 0x3B);
    return bytes;
}

std::vector<RoboU8> PngChunk(const char* name, const std::vector<RoboU8>& body) {
    std::vector<RoboU8> bytes;
    U32(bytes, static_cast<std::uint32_t>(body.size()), true);
    Text(bytes, name);
    Add(bytes, body.data(), body.size());
    U32(bytes, 0, true);
    return bytes;
}

std::vector<RoboU8> Apng() {
    std::vector<RoboU8> bytes{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<RoboU8> ihdr(13, 0);
    auto be = [&](std::vector<RoboU8>& raw, int offset, std::uint32_t value) {
        raw[static_cast<std::size_t>(offset)] = static_cast<RoboU8>(value >> 24);
        raw[static_cast<std::size_t>(offset + 1)] = static_cast<RoboU8>(value >> 16);
        raw[static_cast<std::size_t>(offset + 2)] = static_cast<RoboU8>(value >> 8);
        raw[static_cast<std::size_t>(offset + 3)] = static_cast<RoboU8>(value);
    };
    be(ihdr, 0, 2);
    be(ihdr, 4, 2);
    Join(bytes, PngChunk("IHDR", ihdr));
    std::vector<RoboU8> actl(8, 0);
    be(actl, 0, 2);
    Join(bytes, PngChunk("acTL", actl));
    auto fctl = [&](int numer, int denom) {
        std::vector<RoboU8> body(26, 0);
        be(body, 4, 2);
        be(body, 8, 2);
        body[20] = static_cast<RoboU8>(numer >> 8);
        body[21] = static_cast<RoboU8>(numer);
        body[22] = static_cast<RoboU8>(denom >> 8);
        body[23] = static_cast<RoboU8>(denom);
        Join(bytes, PngChunk("fcTL", body));
    };
    fctl(1, 10);
    fctl(2, 10);
    return bytes;
}

std::vector<RoboU8> Webp() {
    std::vector<RoboU8> body;
    auto anmf = [&](int duration) {
        std::vector<RoboU8> payload(16, 0);
        payload[12] = static_cast<RoboU8>(duration);
        payload[13] = static_cast<RoboU8>(duration >> 8);
        payload[14] = static_cast<RoboU8>(duration >> 16);
        Chunk(body, "ANMF", payload, false);
    };
    anmf(100);
    anmf(250);
    std::vector<RoboU8> bytes;
    Text(bytes, "RIFF");
    U32(bytes, static_cast<std::uint32_t>(4 + body.size()), false);
    Text(bytes, "WEBP");
    Join(bytes, body);
    return bytes;
}

void Expect(const std::vector<RoboU8>& bytes, const char* source, const char* format, const char* codec, int duration, int width,
            int height, const char* projection) {
    RoboVideoClip clip;
    const bool open = RoboVideoDecode(clip, bytes.data(), bytes.size(), source);
    const bool match = open && clip.format == format && (codec == nullptr || clip.codec == codec) &&
                       (duration < 0 || clip.duration == duration) && (width < 0 || clip.width == width) &&
                       (height < 0 || clip.height == height) && (projection == nullptr || clip.projection == projection);
    if (!match) {
        std::cerr << "  open=" << open << " format=" << clip.format << " codec=" << clip.codec << " duration=" << clip.duration
                  << " size=" << clip.width << "x" << clip.height << " projection=" << clip.projection << "\n";
    }
    Check(match, source);
}

}  // namespace

int main() {
    RoboVideoNode video;
    std::string diagnostic;
    Check(Load("<Video source=\"Video/Harbor.avi\" time=\"100\" playing=\"false\" width=\"720\" height=\"180\"/>", video, diagnostic),
          "a video document is accepted");
    Check(video.source == "Video/Harbor.avi" && video.time == 100 && !video.playing && video.width == 720 && video.height == 180,
          "source, time, playing, and the box are read");
    Check(Load("<Video src=\"clip.mp4\" playing=\"yes\"/>", video, diagnostic) && video.source == "clip.mp4" && video.playing,
          "src and playing yes are accepted");
    Check(!Load("<Video Tilt=\"1\"/>", video, diagnostic) && diagnostic.find("R7801") != std::string::npos, "an unknown attribute is rejected");
    Check(!Load("<Video time=\"-4\"/>", video, diagnostic) && diagnostic.find("R7802") != std::string::npos, "a negative clock is rejected");
    Check(!Load("<Video time=\"soon\"/>", video, diagnostic) && diagnostic.find("R7802") != std::string::npos, "a clock word is rejected");
    Check(!Load("<Video playing=\"maybe\"/>", video, diagnostic) && diagnostic.find("R7802") != std::string::npos, "a bad playing value is rejected");
    Check(!Load("<Video width=\"0\"/>", video, diagnostic) && diagnostic.find("R7802") != std::string::npos, "a bad width is rejected");
    Check(!Load("<Video height=\"tall\"/>", video, diagnostic) && diagnostic.find("R7802") != std::string::npos, "a bad height is rejected");
    Check(!Load("<Video><Text value=\"x\"/></Video>", video, diagnostic) && diagnostic.find("R7804") != std::string::npos,
          "a child is rejected");
    Check(!Load("<Screen/>", video, diagnostic) && diagnostic.find("R7804") != std::string::npos, "the wrong tag is rejected");

    const std::vector<RoboU8> avi = Avi(4, 4, 100000, 0xFFFF0000u, 0xFF0000FFu);
    RoboVideoClip clip;
    Check(RoboVideoDecode(clip, avi.data(), avi.size(), "Harbor.avi"), "an uncompressed avi opens");
    Check(clip.format == "avi" && clip.codec == "rgb" && clip.width == 4 && clip.height == 4 && clip.duration == 200 && clip.frameCount == 2,
          "the avi clock is two frames of 100 ms");
    RoboVideoBox box;
    box.width = 4;
    box.height = 4;
    Check(RoboVideoSeek(clip, 0) && clip.time == 0 && RoboVideoPaint(clip, box), "time 0 paints the first frame");
    RoboU32 color = 0;
    Check(RoboVideoPixel(box, 0, 0, color) && color == 0xFFFF0000u, "the pixel at 0 ms is red");
    Check(RoboVideoSeek(clip, 100) && clip.time == 100 && RoboVideoPaint(clip, box), "time 100 paints the next frame");
    Check(RoboVideoPixel(box, 3, 2, color) && color == 0xFF0000FFu, "the pixel at 100 ms is blue");
    Check(RoboVideoSeek(clip, 5000) && clip.time == 200 && RoboVideoPaint(clip, box) && RoboVideoPixel(box, 1, 1, color) && color == 0xFF0000FFu,
          "a seek past the end stays on the last frame");
    Check(!RoboVideoPixel(box, 4, 0, color), "a pixel outside the box is rejected");
    video.time = 100;
    video.playing = false;
    RoboVideoBind(video, clip);
    Check(clip.time == 100 && !clip.playing, "bind copies the clock");

    const char* names[][2] = {
        {"clip.mp4", "mp4"},   {"clip.mkv", "mkv"},   {"clip.avi", "avi"},     {"clip.mov", "mov"},   {"clip.wmv", "wmv"},
        {"clip.flv", "flv"},   {"clip.webm", "webm"}, {"clip.mpeg", "mpeg"},   {"clip.mpg", "mpg"},   {"clip.m4v", "m4v"},
        {"clip.3gp", "3gp"},   {"clip.3g2", "3g2"},   {"clip.vob", "vob"},     {"clip.mxf", "mxf"},   {"clip.r3d", "r3d"},
        {"clip.braw", "braw"}, {"clip.ari", "ari"},   {"clip.ts", "ts"},       {"clip.m2v", "m2v"},   {"clip.m2ts", "m2ts"},
        {"clip.mts", "mts"},   {"clip.m3u8", "m3u8"}, {"clip.mpd", "mpd"},     {"clip.m4s", "m4s"},   {"clip.asf", "asf"},
        {"clip.rm", "rm"},     {"clip.rmvb", "rmvb"}, {"clip.dat", "dat"},     {"tape.dvr-ms", "dvr-ms"}, {"clip.wtv", "wtv"},
        {"clip.nsv", "nsv"},   {"clip.fli", "fli"},   {"clip.flc", "flc"},     {"clip.amv", "amv"},   {"clip.mtv", "mtv"},
        {"clip.gif", "gif"},   {"clip.apng", "apng"}, {"clip.webp", "webp"},   {"clip.avif", "avif"}, {"clip.heif", "heif"},
        {"clip.evo", "evo"},   {"clip.heic", "heif"},
    };
    for (const auto& name : names) {
        Check(std::string(RoboVideoFormatOf(name[0])) == name[1], name[0]);
    }
    Check(std::string(RoboVideoFormatOf("notes.txt")).empty(), "an unknown extension is rejected");

    Expect(Mp4("avc1", "", "isom"), "film.mp4", "mp4", "h264", 1500, 320, 180, "");
    Expect(Mp4("hvc1", "", "isom"), "film.mov", "mov", "h265", 1500, 320, 180, "");
    Expect(Mp4("vvc1", "", "isom"), "film.mp4", "mp4", "h266", 1500, -1, -1, "");
    Expect(Mp4("av01", "", "isom"), "film.mp4", "mp4", "av1", -1, -1, -1, "");
    Expect(Mp4("vp09", "", "isom"), "film.mp4", "mp4", "vp9", -1, -1, -1, "");
    Expect(Mp4("vp08", "", "isom"), "film.mp4", "mp4", "vp8", -1, -1, -1, "");
    Expect(Mp4("mp4v", "", "isom"), "film.mp4", "mp4", "mpeg4", -1, -1, -1, "");
    Expect(Mp4("theo", "", "isom"), "film.mp4", "mp4", "theora", -1, -1, -1, "");
    Expect(Mp4("MJPG", "", "isom"), "film.mov", "mov", "mjpeg", -1, -1, -1, "");
    Expect(Mp4("apch", "", "qt  "), "film.mov", "mov", "prores", -1, -1, -1, "");
    Expect(Mp4("AVdn", "", "qt  "), "film.mov", "mov", "dnxhd", -1, -1, -1, "");
    Expect(Mp4("AVdh", "", "qt  "), "film.mov", "mov", "dnxhr", -1, -1, -1, "");
    Expect(Mp4("CFHD", "", "qt  "), "film.mov", "mov", "cineform", -1, -1, -1, "");
    Expect(Mp4("avc1", "XAVC", "isom"), "film.mp4", "mp4", "xavc", -1, -1, -1, "");
    Expect(Mp4("ai5q", "", "isom"), "film.mxf", "mp4", "avc-intra", -1, -1, -1, "");
    Expect(Mp4("H263", "", "3gp4"), "film.3gp", "3gp", "h263", -1, -1, -1, "");
    Expect(Mp4("mp1v", "", "isom"), "film.m4v", "m4v", "mpeg1", -1, -1, -1, "");
    Expect(Mp4("mp2v", "", "isom"), "film.m4v", "m4v", "mpeg2", -1, -1, -1, "");
    Expect(Mp4("DIVX", "", "isom"), "film.mp4", "mp4", "divx", -1, -1, -1, "");
    Expect(Mp4("XVID", "", "isom"), "film.mp4", "mp4", "xvid", -1, -1, -1, "");
    Expect(Mp4("wvc1", "", "isom"), "film.mp4", "mp4", "vc1", -1, -1, -1, "");
    Expect(Mp4("WMV3", "", "isom"), "film.mp4", "mp4", "wmv9", -1, -1, -1, "");
    Expect(Mp4("avc1", "", "3g2a"), "film.3g2", "3g2", "h264", -1, -1, -1, "");
    Expect(Mp4("avc1", "", "iso6"), "film.m4s", "m4s", "h264", -1, -1, -1, "");
    Expect(Mp4("av01", "equirectangular", "isom"), "sphere.mp4", "mp4", "av1", -1, -1, -1, "equirectangular");
    Expect(Mp4("avc1", "cubemap", "isom"), "cube.mp4", "mp4", "h264", -1, -1, -1, "cubemap");
    Expect(Mp4("avc1", "VR180", "isom"), "half.mp4", "mp4", "h264", -1, -1, -1, "vr180");
    Expect(Mp4("avif", "", "avif"), "still.avif", "avif", "h264", -1, -1, -1, "");
    Expect(Mp4("mif1", "", "mif1"), "still.heif", "heif", "h264", -1, -1, -1, "");

    std::vector<RoboU8> ebml{0x1A, 0x45, 0xDF, 0xA3, 0x44, 0x89, 0x88};
    F64(ebml, 1500.0);
    Text(ebml, "V_MPEG4/ISO/AVC");
    Text(ebml, "equirectangular");
    Expect(ebml, "film.mkv", "mkv", "h264", 1500, -1, -1, "equirectangular");
    std::vector<RoboU8> webm{0x1A, 0x45, 0xDF, 0xA3, 0x44, 0x89, 0x88};
    F64(webm, 800.0);
    Text(webm, "V_VP9");
    Expect(webm, "film.webm", "webm", "vp9", 800, -1, -1, "");

    std::vector<RoboU8> flv;
    Text(flv, "FLV");
    U8(flv, 1);
    U8(flv, 5);
    U32(flv, 9, true);
    U32(flv, 0, true);
    U8(flv, 9);
    U8(flv, 0);
    U8(flv, 0);
    U8(flv, 1);
    U8(flv, 0);
    U8(flv, 0);
    U8(flv, 250);
    U8(flv, 0);
    U8(flv, 0);
    U8(flv, 0);
    U8(flv, 0);
    U8(flv, 0);
    U32(flv, 0, true);
    Expect(flv, "film.flv", "flv", "video", 250, -1, -1, nullptr);

    std::vector<RoboU8> sequence{0, 0, 1, 0xB3, 0x14, 0x00, 0xF0};
    Expect(sequence, "film.m2v", "m2v", "mpeg2", -1, 320, 240, nullptr);
    std::vector<RoboU8> pack{0, 0, 1, 0xBA, 0, 0, 0, 0};
    Expect(pack, "film.mpg", "mpg", "mpeg1", -1, -1, -1, nullptr);
    Expect(pack, "film.mpeg", "mpeg", "mpeg1", -1, -1, -1, nullptr);
    Expect(pack, "film.vob", "vob", "mpeg2", -1, -1, -1, nullptr);
    Expect(pack, "film.evo", "evo", "mpeg2", -1, -1, -1, nullptr);
    Expect(std::vector<RoboU8>{1, 2, 3, 4}, "film.dat", "dat", "mpeg2", -1, -1, -1, nullptr);

    std::vector<RoboU8> ts(188, 0);
    ts[0] = 0x47;
    Expect(ts, "film.ts", "ts", "h264", -1, -1, -1, nullptr);
    Expect(ts, "film.m2ts", "m2ts", "h264", -1, -1, -1, nullptr);
    Expect(ts, "film.mts", "mts", "h264", -1, -1, -1, nullptr);

    const RoboU8 asfHeader[] = {0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C};
    const RoboU8 asfFile[] = {0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11, 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65};
    std::vector<RoboU8> file;
    Add(file, asfFile, 16);
    file.resize(64, 0);
    U64(file, 1500ull * 10000ull, false);
    std::vector<RoboU8> placed;
    Add(placed, asfHeader, 16);
    Join(placed, file);
    Expect(placed, "film.wmv", "wmv", "vc1", 1500, -1, -1, nullptr);
    Expect(placed, "film.asf", "asf", "vc1", 1500, -1, -1, nullptr);
    Expect(placed, "tape.dvr-ms", "dvr-ms", "vc1", 1500, -1, -1, nullptr);

    std::vector<RoboU8> mxf{0x06, 0x0E, 0x2B, 0x34};
    Text(mxf, "AVdn");
    Expect(mxf, "film.mxf", "mxf", "dnxhd", -1, -1, -1, nullptr);
    std::vector<RoboU8> dnxhr{0x06, 0x0E, 0x2B, 0x34};
    Text(dnxhr, "AVdh");
    Expect(dnxhr, "film.mxf", "mxf", "dnxhr", -1, -1, -1, nullptr);
    std::vector<RoboU8> xavc{0x06, 0x0E, 0x2B, 0x34};
    Text(xavc, "XAVC");
    Expect(xavc, "film.mxf", "mxf", "xavc", -1, -1, -1, nullptr);
    std::vector<RoboU8> intra{0x06, 0x0E, 0x2B, 0x34};
    Text(intra, "ai5q");
    Expect(intra, "film.mxf", "mxf", "avc-intra", -1, -1, -1, nullptr);

    Expect(std::vector<RoboU8>{'R', 'E', 'D', '1'}, "film.r3d", "r3d", "redcode", -1, -1, -1, nullptr);
    Expect(std::vector<RoboU8>{'B', 'R', 'A', 'W'}, "film.braw", "braw", "braw", -1, -1, -1, nullptr);
    Expect(std::vector<RoboU8>{'A', 'R', 'R', 'I'}, "film.ari", "ari", "arriraw", -1, -1, -1, nullptr);
    Expect(std::vector<RoboU8>{'.', 'R', 'M', 'F'}, "film.rm", "rm", "rv", -1, -1, -1, nullptr);
    Expect(std::vector<RoboU8>{'.', 'R', 'M', 'F'}, "film.rmvb", "rmvb", "rv", -1, -1, -1, nullptr);
    Expect(std::vector<RoboU8>{'N', 'S', 'V', 'f'}, "film.nsv", "nsv", nullptr, -1, -1, -1, nullptr);
    Expect(std::vector<RoboU8>{'A', 'M', 'V', 0}, "film.amv", "amv", "mjpeg", -1, -1, -1, nullptr);
    Expect(std::vector<RoboU8>{'M', 'T', 'V', 0}, "film.mtv", "mtv", nullptr, -1, -1, -1, nullptr);
    Expect(std::vector<RoboU8>(20, 1), "tape.wtv", "wtv", "mpeg2", -1, -1, -1, nullptr);

    std::vector<RoboU8> fli(128, 0);
    fli[4] = 0x11;
    fli[5] = 0xAF;
    fli[6] = 10;
    fli[8] = 32;
    fli[10] = 24;
    fli[16] = 7;
    Expect(fli, "film.fli", "fli", "fli", 1000, 32, 24, nullptr);
    std::vector<RoboU8> flc(128, 0);
    flc[4] = 0x12;
    flc[5] = 0xAF;
    flc[6] = 4;
    flc[8] = 16;
    flc[10] = 16;
    flc[16] = 50;
    Expect(flc, "film.flc", "flc", "flc", 200, 16, 16, nullptr);

    Expect(Gif(), "film.gif", "gif", "gif", 300, 2, 2, nullptr);
    Expect(Apng(), "film.apng", "apng", "apng", 300, 2, 2, nullptr);
    Expect(Webp(), "film.webp", "webp", "webp", 350, -1, -1, nullptr);

    const char* hls = "#EXTM3U\n#EXTINF:1.5,\npart.ts\n#EXTINF:2,\npart2.ts\n";
    Expect(std::vector<RoboU8>(hls, hls + std::strlen(hls)), "live.m3u8", "m3u8", nullptr, 3500, -1, -1, nullptr);
    const char* live = "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1\nmedia.m3u8\n";
    std::vector<RoboU8> liveBytes(live, live + std::strlen(live));
    RoboVideoClip stream;
    Check(RoboVideoDecode(stream, liveBytes.data(), liveBytes.size(), "live.m3u8") && stream.live && stream.duration == 0,
          "a live hls playlist stays open");
    Check(RoboVideoSeek(stream, 4000) && stream.time == 0, "a live seek stays at 0");
    const char* dash = "<MPD mediaPresentationDuration=\"PT1H2M3.5S\"></MPD>";
    Expect(std::vector<RoboU8>(dash, dash + std::strlen(dash)), "film.mpd", "mpd", nullptr, 3723500, -1, -1, nullptr);

    std::vector<RoboU8> empty{1, 2, 3, 4};
    Check(!RoboVideoDecode(clip, empty.data(), empty.size(), "notes.txt"), "an unknown file stays closed");
    Check(!RoboVideoDecode(clip, nullptr, 0, "film.avi"), "an empty file stays closed");

    std::cout << gPassed << " passed, " << gFailed << " failed\n";
    return gFailed == 0 ? 0 : 1;
}
