#include "../../RobixUI/Compiler/Components/Audio/RoboAudio.hpp"
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

bool Load(const std::string& source, RoboAudioNode& audio, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboAudioRead(document.root, audio, diagnostic);
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

void F64(std::vector<RoboU8>& bytes, double value, bool big) {
    RoboU8 raw[8];
    std::memcpy(raw, &value, 8);
    if (big) {
        for (int index = 7; index >= 0; --index) {
            bytes.push_back(raw[index]);
        }
        return;
    }
    Add(bytes, raw, 8);
}

std::vector<RoboU8> ToneWav() {
    std::vector<RoboI16> samples(44100, 0);
    samples[0] = 1000;
    samples[22050] = 2000;
    const int data = static_cast<int>(samples.size() * 2);
    std::vector<RoboU8> bytes;
    Text(bytes, "RIFF");
    U32(bytes, static_cast<std::uint32_t>(36 + data), false);
    Text(bytes, "WAVEfmt ");
    U32(bytes, 16, false);
    U16(bytes, 1, false);
    U16(bytes, 1, false);
    U32(bytes, 44100, false);
    U32(bytes, 88200, false);
    U16(bytes, 2, false);
    U16(bytes, 16, false);
    Text(bytes, "data");
    U32(bytes, static_cast<std::uint32_t>(data), false);
    Add(bytes, samples.data(), samples.size() * 2);
    return bytes;
}

void Expect(const char* name, const std::vector<RoboU8>& bytes, const char* format, int duration, const char* description) {
    RoboAudioClip clip;
    const bool opened = RoboAudioDecode(clip, bytes.data(), bytes.size(), name);
    Check(opened && clip.format == format && clip.duration == duration, description);
    if (!opened || clip.format != format || clip.duration != duration) {
        std::cerr << "    got " << (opened ? clip.format : "closed") << " " << clip.duration << "\n";
    }
}

void TestSchema() {
    RoboAudioNode audio;
    std::string diagnostic;
    Check(Load("<Audio Source=\"Harbor.wav\" Time=\"250\" Playing=\"true\" Volume=\"40\"/>", audio, diagnostic), "audio schema reads");
    Check(diagnostic.empty() && audio.source == "Harbor.wav" && audio.time == 250 && audio.playing && audio.volume == 40,
          "audio keeps source, time, playing, and volume");
    Check(Load("<Audio Src=\"Harbor.wav\"/>", audio, diagnostic) && audio.source == "Harbor.wav" && audio.time == 0 && !audio.playing &&
              audio.volume == 100,
          "audio defaults the clock, playback, and volume");
    Check(!Load("<Audio Source=\"a.wav\" Fit=\"cover\"/>", audio, diagnostic) && diagnostic.find("R7901") != std::string::npos,
          "an unknown audio attribute is rejected");
    Check(!Load("<Audio Source=\"a.wav\" Volume=\"101\"/>", audio, diagnostic) && diagnostic.find("R7902") != std::string::npos,
          "a volume past 100 is rejected");
    Check(!Load("<Audio Source=\"a.wav\" Volume=\"loud\"/>", audio, diagnostic) && diagnostic.find("R7902") != std::string::npos,
          "a named volume is rejected");
    Check(!Load("<Audio Source=\"a.wav\" Time=\"-1\"/>", audio, diagnostic) && diagnostic.find("R7902") != std::string::npos,
          "a negative time is rejected");
    Check(!Load("<Audio Source=\"a.wav\" Playing=\"maybe\"/>", audio, diagnostic) && diagnostic.find("R7902") != std::string::npos,
          "a bad playing value is rejected");
    Check(!Load("<Sound Source=\"a.wav\"/>", audio, diagnostic) && diagnostic.find("R7904") != std::string::npos,
          "the wrong tag is rejected");
    Check(!Load("<Audio Source=\"a.wav\"><View/></Audio>", audio, diagnostic) && diagnostic.find("R7904") != std::string::npos,
          "an audio child is rejected");
}

void TestClock() {
    const std::vector<RoboU8> wav = ToneWav();
    RoboAudioClip clip;
    Check(RoboAudioDecode(clip, wav.data(), wav.size(), "harbor.wav"), "wav decodes");
    Check(clip.format == "wav" && clip.pcm && clip.sampleRate == 44100 && clip.channels == 1 && clip.duration == 1000,
          "wav duration is one second");
    Check(!clip.samples.empty() && clip.samples[0] == 1000, "the first wav sample is kept");
    Check(RoboAudioSeek(clip, 250) && clip.time == 250, "a seek lands on the requested time");
    Check(RoboAudioSeek(clip, 5000) && clip.time == 1000, "a seek past the end stops at the duration");
    clip.playing = true;
    clip.volume = 50;
    Check(RoboAudioSeek(clip, 500), "seek back into the tone");
    RoboAudioOutput output;
    Check(RoboAudioPlay(clip, output) && output.time == 500 && !output.samples.empty() && output.samples[0] == 1000,
          "playback starts at the seek and applies volume");
    RoboAudioNode node;
    std::string diagnostic;
    Check(Load("<Audio Source=\"harbor.wav\" Time=\"250\" Playing=\"true\" Volume=\"80\"/>", node, diagnostic), "bind source reads");
    RoboAudioBind(node, clip);
    Check(clip.time == 250 && clip.playing && clip.volume == 80, "bind copies the clock onto the clip");
}

void TestFormats() {
    const char* names[] = {"song.mp3", "song.aac", "song.m4a", "song.ogg", "song.opus", "song.wma", "song.amr", "song.mp2",
                           "song.ac3", "song.eac3", "song.mp4", "song.flac", "song.alac", "song.wv", "song.ape", "song.tta",
                           "song.tak", "song.ofr", "song.als", "song.wav", "song.aiff", "song.aif", "song.au", "song.pcm",
                           "song.bwf", "song.rf64", "song.caf", "song.mid", "song.midi", "song.kar", "song.mod", "song.xm",
                           "song.it", "song.s3m", "song.nsf", "song.spc", "song.dts", "song.dtshd", "song.thd", "song.ac4",
                           "song.dsf", "song.dff", "song.awb", "song.gsm", "song.ulaw", "song.alaw", "song.g722", "song.g729",
                           "song.spx", "song.silk", "song.m3u8", "song.mpd", "song.webm", "song.mka", "song.mkv", "song.m3u",
                           "song.pls", "song.wpl", "song.asx"};
    const char* formats[] = {"mp3", "aac", "m4a", "ogg", "opus", "wma", "amr", "mp2", "ac3", "eac3", "mp4", "flac", "alac",
                             "wv", "ape", "tta", "tak", "ofr", "als", "wav", "aiff", "aif", "au", "pcm", "bwf", "rf64", "caf",
                             "mid", "mid", "kar", "mod", "xm", "it", "s3m", "nsf", "spc", "dts", "dtshd", "thd", "ac4", "dsf",
                             "dff", "awb", "gsm", "ulaw", "alaw", "g722", "g729", "spx", "silk", "m3u8", "mpd", "webm", "mka",
                             "mka", "m3u", "pls", "wpl", "asx"};
    for (int index = 0; index < 59; ++index) {
        Check(std::string(RoboAudioFormatOf(names[index])) == formats[index], names[index]);
    }
    Check(std::string(RoboAudioFormatOf("https://harbor.example/live")) == "icecast", "an http stream is icecast");
    Check(std::string(RoboAudioFormatOf("note.txt")).empty(), "an unknown extension is rejected");

    std::vector<RoboU8> bytes(8, 0);
    RoboAudioClip clip;
    Check(!RoboAudioDecode(clip, bytes.data(), bytes.size(), "note.txt"), "unknown bytes do not open");
    Check(!RoboAudioDecode(clip, bytes.data(), bytes.size(), "broken.wav"), "a short wav does not open");

    Expect("tone.wav", ToneWav(), "wav", 1000, "wav reports its duration");

    bytes.clear();
    Text(bytes, "FORM");
    U32(bytes, 46, true);
    Text(bytes, "AIFFCOMM");
    U32(bytes, 18, true);
    U16(bytes, 1, true);
    U32(bytes, 441, true);
    U16(bytes, 16, true);
    const RoboU8 rate80[] = {0x40, 0x0E, 0xAC, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Add(bytes, rate80, 10);
    Text(bytes, "SSND");
    U32(bytes, 8 + 882, true);
    U32(bytes, 0, true);
    U32(bytes, 0, true);
    bytes.resize(bytes.size() + 882, 0);
    Expect("tone.aiff", bytes, "aiff", 10, "aiff reports its duration");
    Expect("tone.aif", bytes, "aif", 10, "aif reports its duration");

    bytes.clear();
    U32(bytes, 0x2E736E64u, true);
    U32(bytes, 24, true);
    U32(bytes, 882, true);
    U32(bytes, 3, true);
    U32(bytes, 44100, true);
    U32(bytes, 1, true);
    bytes.resize(bytes.size() + 882, 0);
    Expect("tone.au", bytes, "au", 10, "au reports its duration");

    bytes.clear();
    Text(bytes, "caff");
    U16(bytes, 1, true);
    U16(bytes, 0, true);
    Text(bytes, "desc");
    U64(bytes, 32, true);
    F64(bytes, 44100.0, true);
    Text(bytes, "lpcm");
    U32(bytes, 2, true);
    U32(bytes, 2, true);
    U32(bytes, 1, true);
    U32(bytes, 1, true);
    U32(bytes, 16, true);
    Text(bytes, "data");
    U64(bytes, 4 + 882, true);
    U32(bytes, 0, true);
    bytes.resize(bytes.size() + 882, 0);
    Expect("tone.caf", bytes, "caf", 10, "caf reports its duration");

    bytes.assign(882, 0);
    bytes[0] = 0xE8;
    bytes[1] = 0x03;
    Expect("tone.pcm", bytes, "pcm", 10, "pcm reports its duration");
    Check(RoboAudioDecode(clip, bytes.data(), bytes.size(), "tone.pcm") && clip.pcm && clip.samples[0] == 1000, "pcm samples are decoded");

    bytes.assign(8, 0xFF);
    Expect("tone.ulaw", bytes, "ulaw", 1, "ulaw reports its duration");
    bytes.assign(8, 0xD5);
    Expect("tone.alaw", bytes, "alaw", 1, "alaw reports its duration");

    bytes.clear();
    Text(bytes, "RF64");
    U32(bytes, 0xFFFFFFFFu, false);
    Text(bytes, "WAVE");
    Text(bytes, "ds64");
    U32(bytes, 28, false);
    U64(bytes, 0, false);
    U64(bytes, 882, false);
    U64(bytes, 441, false);
    U32(bytes, 0, false);
    Text(bytes, "fmt ");
    U32(bytes, 16, false);
    U16(bytes, 1, false);
    U16(bytes, 1, false);
    U32(bytes, 44100, false);
    U32(bytes, 88200, false);
    U16(bytes, 2, false);
    U16(bytes, 16, false);
    Text(bytes, "data");
    U32(bytes, 0xFFFFFFFFu, false);
    bytes.resize(bytes.size() + 882, 0);
    Expect("tone.rf64", bytes, "rf64", 10, "rf64 reports its duration");

    bytes.clear();
    Text(bytes, "RIFF");
    U32(bytes, 4 + 8 + 8 + 8 + 16 + 8 + 882, false);
    Text(bytes, "WAVE");
    Text(bytes, "bext");
    U32(bytes, 8, false);
    bytes.resize(bytes.size() + 8, 0);
    Text(bytes, "fmt ");
    U32(bytes, 16, false);
    U16(bytes, 1, false);
    U16(bytes, 1, false);
    U32(bytes, 44100, false);
    U32(bytes, 88200, false);
    U16(bytes, 2, false);
    U16(bytes, 16, false);
    Text(bytes, "data");
    U32(bytes, 882, false);
    bytes.resize(bytes.size() + 882, 0);
    Expect("tone.bwf", bytes, "bwf", 10, "bwf reports its duration");

    bytes.clear();
    Text(bytes, "fLaC");
    U8(bytes, 0x80);
    U8(bytes, 0x00);
    U8(bytes, 0x00);
    U8(bytes, 0x22);
    bytes.resize(bytes.size() + 10, 0);
    const std::uint64_t packed = (44100ull << 44) | (15ull << 36) | 44100ull;
    for (int shift = 56; shift >= 0; shift -= 8) {
        U8(bytes, static_cast<int>(packed >> shift));
    }
    bytes.resize(bytes.size() + 16, 0);
    Expect("tone.flac", bytes, "flac", 1000, "flac reports its duration");

    bytes.assign(417, 0);
    bytes[0] = 0xFF;
    bytes[1] = 0xFB;
    bytes[2] = 0x90;
    bytes[3] = 0x00;
    Expect("tone.mp3", bytes, "mp3", 26, "mp3 reports its frame duration");
    Expect("tone.mp2", bytes, "mp2", 26, "mp2 keeps its format");

    bytes.assign(16, 0);
    bytes[0] = 0xFF;
    bytes[1] = 0xF1;
    bytes[2] = 0x50;
    bytes[4] = 2;
    Expect("tone.aac", bytes, "aac", 23, "aac reports its frame duration");

    bytes.clear();
    Text(bytes, "#!AMR\n");
    U8(bytes, 0x00);
    bytes.resize(bytes.size() + 12, 0);
    Expect("tone.amr", bytes, "amr", 20, "amr reports a 20 millisecond frame");
    bytes.clear();
    Text(bytes, "#!AMR-WB\n");
    U8(bytes, 0x00);
    bytes.resize(bytes.size() + 17, 0);
    Expect("tone.awb", bytes, "awb", 20, "amr-wb reports a 20 millisecond frame");

    bytes.assign(128, 0);
    bytes[0] = 0x0B;
    bytes[1] = 0x77;
    bytes[4] = 0x00;
    bytes[5] = 0x00;
    Expect("tone.ac3", bytes, "ac3", 32, "ac3 reports its frame duration");
    bytes.assign(64, 0);
    bytes[0] = 0x0B;
    bytes[1] = 0x77;
    bytes[3] = 31;
    bytes[5] = 128;
    Expect("tone.eac3", bytes, "eac3", 32, "e-ac-3 reports its frame duration");

    auto box = [](const char* type, const std::vector<RoboU8>& body) {
        std::vector<RoboU8> out;
        U32(out, static_cast<std::uint32_t>(8 + body.size()), true);
        Text(out, type);
        out.insert(out.end(), body.begin(), body.end());
        return out;
    };
    auto mp4 = [&box](const char* codec, const char* marker) {
        std::vector<RoboU8> brand;
        Text(brand, "M4A ");
        U32(brand, 0, true);
        std::vector<RoboU8> scale;
        U32(scale, 1000, true);
        std::vector<RoboU8> duration;
        U32(duration, 1500, true);
        std::vector<RoboU8> mvhd(12, 0);
        mvhd.insert(mvhd.end(), scale.begin(), scale.end());
        mvhd.insert(mvhd.end(), duration.begin(), duration.end());
        std::vector<RoboU8> stsd;
        U32(stsd, 0, true);
        U32(stsd, 1, true);
        Text(stsd, codec);
        std::vector<RoboU8> moov = box("mvhd", mvhd);
        const std::vector<RoboU8> sample = box("stsd", stsd);
        moov.insert(moov.end(), sample.begin(), sample.end());
        if (marker != nullptr) {
            Text(moov, marker);
        }
        std::vector<RoboU8> file = box("ftyp", brand);
        const std::vector<RoboU8> movie = box("moov", moov);
        file.insert(file.end(), movie.begin(), movie.end());
        return file;
    };
    Expect("tone.mp4", mp4("mp4a", nullptr), "mp4", 1500, "mp4 reports the movie duration");
    Expect("tone.m4a", mp4("mp4a", nullptr), "m4a", 1500, "m4a reports the movie duration");
    Expect("tone.alac", mp4("alac", nullptr), "alac", 1500, "alac reports the movie duration");
    Expect("film.mp4", mp4("ec-3", "Atmos"), "atmos", 1500, "atmos is carried in the movie");

    bytes.clear();
    Text(bytes, "OggS");
    U8(bytes, 0);
    U8(bytes, 0);
    U64(bytes, 44100, false);
    U32(bytes, 1, false);
    U32(bytes, 0, false);
    U32(bytes, 0, false);
    U8(bytes, 1);
    U8(bytes, 19);
    U8(bytes, 1);
    Text(bytes, "vorbis");
    U32(bytes, 0, false);
    U8(bytes, 1);
    U32(bytes, 44100, false);
    bytes.resize(bytes.size() + 4, 0);
    Expect("tone.ogg", bytes, "ogg", 1000, "ogg reports its duration");

    bytes.clear();
    Text(bytes, "OggS");
    U8(bytes, 0);
    U8(bytes, 0);
    U64(bytes, 48000, false);
    U32(bytes, 1, false);
    U32(bytes, 0, false);
    U32(bytes, 0, false);
    U8(bytes, 1);
    U8(bytes, 19);
    Text(bytes, "OpusHead");
    U8(bytes, 1);
    U8(bytes, 1);
    U16(bytes, 0, false);
    U32(bytes, 48000, false);
    bytes.resize(bytes.size() + 2, 0);
    Expect("tone.opus", bytes, "opus", 1000, "opus reports its duration");

    bytes.clear();
    Text(bytes, "OggS");
    U8(bytes, 0);
    U8(bytes, 0);
    U64(bytes, 16000, false);
    U32(bytes, 1, false);
    U32(bytes, 0, false);
    U32(bytes, 0, false);
    U8(bytes, 1);
    U8(bytes, 44);
    Text(bytes, "Speex   ");
    bytes.resize(bytes.size() + 28, 0);
    U32(bytes, 16000, false);
    bytes.resize(bytes.size() + 4, 0);
    Expect("tone.spx", bytes, "spx", 1000, "speex reports its duration");

    bytes.clear();
    Text(bytes, "MThd");
    U32(bytes, 6, true);
    U16(bytes, 0, true);
    U16(bytes, 1, true);
    U16(bytes, 480, true);
    Text(bytes, "MTrk");
    U32(bytes, 20, true);
    U8(bytes, 0x00);
    U8(bytes, 0xFF);
    U8(bytes, 0x51);
    U8(bytes, 0x03);
    U8(bytes, 0x07);
    U8(bytes, 0xA1);
    U8(bytes, 0x20);
    U8(bytes, 0x00);
    U8(bytes, 0x90);
    U8(bytes, 0x3C);
    U8(bytes, 0x40);
    U8(bytes, 0x83);
    U8(bytes, 0x60);
    U8(bytes, 0x80);
    U8(bytes, 0x3C);
    U8(bytes, 0x00);
    U8(bytes, 0x00);
    U8(bytes, 0xFF);
    U8(bytes, 0x2F);
    U8(bytes, 0x00);
    Expect("tone.mid", bytes, "mid", 500, "midi reports the note duration");
    Check(RoboAudioDecode(clip, bytes.data(), bytes.size(), "tone.mid") && clip.pcm && clip.samples.size() > 100 && clip.samples[100] != 0,
          "midi plays the note");
    Expect("tone.kar", bytes, "kar", 500, "kar uses the midi clock");

    bytes.assign(1084, 0);
    bytes[950] = 1;
    Text(bytes, "M.K.");
    bytes.resize(1080);
    bytes.insert(bytes.end(), {'M', '.', 'K', '.'});
    Expect("tone.mod", bytes, "mod", 7680, "mod reports its order duration");

    bytes.assign(80, 0);
    std::memcpy(bytes.data(), "Extended Module: ", 17);
    bytes[64] = 2;
    bytes[76] = 6;
    bytes[78] = 125;
    Expect("tone.xm", bytes, "xm", 15360, "xm reports its order duration");

    bytes.assign(0x40, 0);
    std::memcpy(bytes.data(), "IMPM", 4);
    bytes[0x20] = 1;
    bytes[0x3C] = 6;
    bytes[0x3D] = 125;
    Expect("tone.it", bytes, "it", 7680, "it reports its order duration");

    bytes.assign(0x40, 0);
    std::memcpy(bytes.data() + 0x2C, "SCRM", 4);
    bytes[0x20] = 1;
    bytes[0x31] = 6;
    bytes[0x32] = 125;
    Expect("tone.s3m", bytes, "s3m", 7680, "s3m reports its order duration");

    bytes.assign(8, 0);
    std::memcpy(bytes.data(), "NESM\x1A", 5);
    bytes[6] = 2;
    Expect("tone.nsf", bytes, "nsf", 2 * ROBO_AUDIO_NSF_SONG_MS, "nsf reports a song clock");
    bytes.assign(16, 0);
    std::memcpy(bytes.data(), "SNES-SPC700", 11);
    Expect("tone.spc", bytes, "spc", ROBO_AUDIO_SPC_MS, "spc reports its clock");

    bytes.assign(96, 0);
    bytes[0] = 0x7F;
    bytes[1] = 0xFE;
    bytes[2] = 0x80;
    bytes[3] = 0x01;
    Expect("tone.dts", bytes, "dts", 10, "dts reports its frame duration");
    Expect("tone.dtshd", bytes, "dtshd", 10, "dts-hd keeps its format");
    bytes[0] = 0xF8;
    bytes[1] = 0x72;
    bytes[2] = 0x6F;
    bytes[3] = 0xBA;
    Expect("tone.thd", bytes, "thd", 0, "truehd opens");
    bytes[0] = 0xAC;
    bytes[1] = 0x40;
    Expect("tone.ac4", bytes, "ac4", 0, "ac-4 opens");

    bytes.assign(60, 0);
    std::memcpy(bytes.data(), "DSD ", 4);
    std::memcpy(bytes.data() + 16, "fmt ", 4);
    bytes[40] = 1;
    const std::uint32_t dsdRate = 44100;
    const std::uint64_t dsdFrames = 44100;
    std::memcpy(bytes.data() + 44, &dsdRate, 4);
    std::memcpy(bytes.data() + 52, &dsdFrames, 8);
    Expect("tone.dsf", bytes, "dsf", 1000, "dsf reports its sample count");

    bytes.clear();
    Text(bytes, "FRM8");
    U64(bytes, 64, true);
    Text(bytes, "DSD ");
    Text(bytes, "FS  ");
    U64(bytes, 4, true);
    U32(bytes, 8000, true);
    Text(bytes, "CHNL");
    U64(bytes, 2, true);
    U16(bytes, 1, true);
    Text(bytes, "DSD ");
    U64(bytes, 100, true);
    bytes.resize(bytes.size() + 100, 0);
    Expect("tone.dff", bytes, "dff", 100, "dff reports its sample count");

    bytes.assign(88, 0);
    const RoboU8 asf[] = {0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C};
    const RoboU8 file[] = {0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11, 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65};
    std::memcpy(bytes.data(), asf, 16);
    std::memcpy(bytes.data() + 16, file, 16);
    const std::uint64_t ticks = 10000000ull;
    std::memcpy(bytes.data() + 80, &ticks, 8);
    Expect("tone.wma", bytes, "wma", 1000, "wma reports its play duration");

    bytes.clear();
    U8(bytes, 0x1A);
    U8(bytes, 0x45);
    U8(bytes, 0xDF);
    U8(bytes, 0xA3);
    U8(bytes, 0x44);
    U8(bytes, 0x89);
    U8(bytes, 0x88);
    F64(bytes, 1500.0, true);
    Expect("tone.webm", bytes, "webm", 1500, "webm reports its duration");
    Expect("tone.mka", bytes, "mka", 1500, "mka reports its duration");

    const std::string playlist =
        "#EXTM3U\n#EXTINF:1,A\na.mp3\n#EXTINF:2,B\nb.mp3\n";
    bytes.assign(playlist.begin(), playlist.end());
    Expect("list.m3u", bytes, "m3u", 3000, "m3u sums the entries");
    Expect("list.m3u8", bytes, "m3u8", 3000, "hls sums the entries");
    const std::string pls = "[playlist]\nFile1=a.mp3\nLength1=3\nNumberOfEntries=1\n";
    bytes.assign(pls.begin(), pls.end());
    Expect("list.pls", bytes, "pls", 3000, "pls sums the lengths");
    const std::string wpl = "<?wpl?><smil><body><seq><media src=\"a.mp3\"/><media src=\"b.mp3\"/></seq></body></smil>";
    bytes.assign(wpl.begin(), wpl.end());
    Check(RoboAudioDecode(clip, bytes.data(), bytes.size(), "list.wpl") && clip.format == "wpl" && clip.entries.size() == 2,
          "wpl lists its media");
    const std::string asx = "<asx><entry><ref href=\"a.mp3\"/></entry><Duration=\"2.5\"/></asx>";
    bytes.assign(asx.begin(), asx.end());
    Expect("list.asx", bytes, "asx", 2500, "asx reports its duration");
    const std::string mpd = "<MPD mediaPresentationDuration=\"PT1M2.5S\"></MPD>";
    bytes.assign(mpd.begin(), mpd.end());
    Expect("list.mpd", bytes, "mpd", 62500, "dash reports its presentation duration");

    const std::string icy = "ICY 200 OK\r\nicy-name: Harbor\r\n";
    bytes.assign(icy.begin(), icy.end());
    Check(RoboAudioDecode(clip, bytes.data(), bytes.size(), "live") && clip.format == "icecast" && clip.live && clip.duration == 0,
          "shoutcast and icecast open as a live stream");
    Check(RoboAudioSeek(clip, 500) && clip.time == 0, "a live stream does not seek");
    Check(RoboAudioDecode(clip, nullptr, 0, "https://harbor.example/live") && clip.live, "a stream url opens");

    bytes.assign(66, 0);
    Expect("tone.gsm", bytes, "gsm", 40, "gsm reports two frames");
    bytes.assign(80, 0);
    Expect("tone.g722", bytes, "g722", 10, "g722 reports its bitrate duration");
    bytes.assign(20, 0);
    Expect("tone.g729", bytes, "g729", 20, "g729 reports its frame duration");
    bytes.clear();
    Text(bytes, "#!SILK_V3");
    bytes.resize(bytes.size() + 40, 0);
    Expect("tone.silk", bytes, "silk", 40, "silk reports its frame duration");

    bytes.assign(56, 0);
    std::memcpy(bytes.data(), "MAC ", 4);
    bytes[4] = 0x96;
    bytes[5] = 0x0F;
    bytes[8] = 32;
    const std::uint32_t apeBlocks = 44100;
    const std::uint32_t apeOne = 1;
    const std::uint32_t apeRate = 44100;
    std::memcpy(bytes.data() + 36, &apeBlocks, 4);
    std::memcpy(bytes.data() + 40, &apeBlocks, 4);
    std::memcpy(bytes.data() + 44, &apeOne, 4);
    bytes[50] = 16;
    bytes[52] = 1;
    std::memcpy(bytes.data() + 52, &apeRate, 4);
    bytes[50] = 1;
    Expect("tone.ape", bytes, "ape", 1000, "ape reports its sample count");

    bytes.assign(28, 0);
    std::memcpy(bytes.data(), "wvpk", 4);
    const std::uint32_t waveSamples = 44100;
    const std::uint32_t waveFlags = 9u << 23;
    std::memcpy(bytes.data() + 12, &waveSamples, 4);
    std::memcpy(bytes.data() + 24, &waveFlags, 4);
    Expect("tone.wv", bytes, "wv", 1000, "wavpack reports its sample count");

    bytes.assign(22, 0);
    std::memcpy(bytes.data(), "TTA1", 4);
    bytes[6] = 1;
    U32(bytes, 44100, false);
    bytes.resize(10);
    std::vector<RoboU8> rest;
    U32(rest, 44100, false);
    U32(rest, 44100, false);
    bytes.insert(bytes.end(), rest.begin(), rest.end());
    Expect("tone.tta", bytes, "tta", 1000, "tta reports its sample count");

    bytes.clear();
    Text(bytes, "tBaK");
    U32(bytes, 44100, false);
    U32(bytes, 44100, false);
    U16(bytes, 1, false);
    Expect("tone.tak", bytes, "tak", 1000, "tak reports its sample count");
    bytes.clear();
    Text(bytes, "OFR ");
    U32(bytes, 0, false);
    U32(bytes, 44100, false);
    U32(bytes, 44100, false);
    U16(bytes, 1, false);
    Expect("tone.ofr", bytes, "ofr", 1000, "optimfrog reports its sample count");
    bytes.clear();
    Text(bytes, "ALS");
    U8(bytes, 0);
    U32(bytes, 44100, true);
    U32(bytes, 44100, true);
    U16(bytes, 1, true);
    Expect("tone.als", bytes, "als", 1000, "mpeg-4 als reports its sample count");
}

}  // namespace

int main() {
    TestSchema();
    TestClock();
    TestFormats();
    std::cout << gPassed << " passed, " << gFailed << " failed\n";
    return gFailed == 0 ? 0 : 1;
}
