#include "WindowsDocument.hpp"
#include "../ViewZoom.hpp"
#include "WindowsAudio.hpp"
#include "WindowsIcon.hpp"
#include "WindowsVideo.hpp"

#include "../../Compiler/RoboCompilerShared.hpp"
#include "../../Compiler/RoboCompilerWebShared.hpp"

#include "../../Compiler/Foundation/FoundationPlatform.hpp"
#include "../../Compiler/Compose.hpp"
#include "../../Compiler/RoboXML.hpp"
#include "../../Compiler/Components/Canvas/RoboCanvas.hpp"
#include "../../Compiler/Components/Image/RoboImage.hpp"
#include "../../Compiler/Components/Video/RoboVideo.hpp"
#include "../../RoboGPU/RoboGPU.hpp"
#include "../../RoboGPU/RoboText.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>

void RoboWindowsInstallFontRaster(const RoboU8* pack, RoboSize packBytes);
void RoboWindowsFontExtent(int face, int size, int* above, int* below);

namespace Robix::UI {
namespace {

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

std::string ReadText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void LowerKeys(RoboXMLNode& node) {
    std::vector<std::pair<std::string, std::string>> extra;
    for (const auto& entry : node.attrs) {
        const std::string key = Lower(entry.first);
        if (node.attrs.find(key) == node.attrs.end()) {
            extra.emplace_back(key, entry.second);
        }
    }
    for (const auto& entry : extra) {
        node.attrs[entry.first] = entry.second;
    }
    for (RoboXMLNode& child : node.children) {
        LowerKeys(child);
    }
}

const RoboXMLNode* FindScreen(const RoboXMLNode& node) {
    const std::string name = Lower(node.name);
    if (name == "screen" || name == "window" || name == "page") {
        return &node;
    }
    for (const RoboXMLNode& child : node.children) {
        if (const RoboXMLNode* found = FindScreen(child)) {
            return found;
        }
    }
    return nullptr;
}

unsigned gDpi = 96;
std::filesystem::path gAssets;
std::filesystem::path gDocumentDir;

struct CachedPicture {
    std::string key;
    RoboImageRecord image;
    bool ok = false;
    int cacheW = 0;
    int cacheH = 0;
    int cacheFrame = -1;
    int shownFrame = -1;
    std::string cacheFit;
    std::vector<RoboU32> cache;
    bool videoPlaying = false;
    std::filesystem::path videoFile;
    int videoStart = 0;
    int videoDuration = 0;
    int videoShown = -1;
    unsigned videoAnchor = 0;
};

std::vector<CachedPicture> gPictures;

struct CachedCanvas {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
    int cacheW = 0;
    int cacheH = 0;
    std::vector<RoboU32> cache;
};

std::vector<CachedCanvas> gCanvases;
unsigned gPictureClock = 0;
bool gVideoDirty = false;

int PictureFor(const std::filesystem::path& file) {
    const std::string key = file.string();
    for (int index = 0; index < static_cast<int>(gPictures.size()); ++index) {
        if (gPictures[static_cast<std::size_t>(index)].key == key) {
            return gPictures[static_cast<std::size_t>(index)].ok ? index : -1;
        }
    }
    CachedPicture picture;
    picture.key = key;
    std::ifstream in(file, std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    picture.ok = !bytes.empty() && RoboImageDecode(picture.image, bytes.data(), bytes.size());
    gPictures.push_back(std::move(picture));
    return gPictures.back().ok ? static_cast<int>(gPictures.size()) - 1 : -1;
}

int VideoClock(const RoboXMLNode& node) {
    const std::string time = RoboXMLAttr(node, "time");
    int value = 0;
    for (char ch : time) {
        if (ch < '0' || ch > '9') {
            return 0;
        }
        value = value * 10 + (ch - '0');
    }
    return value;
}

bool VideoPlaying(const RoboXMLNode& node) {
    const std::string playing = Lower(RoboXMLAttr(node, "playing"));
    return playing == "true" || playing == "yes" || playing == "on" || playing == "1";
}

int VideoFor(const std::filesystem::path& file, int timeMs, bool playing) {
    const std::string key = std::string("video|") + file.string() + "|" + std::to_string(timeMs) + "|" + (playing ? "1" : "0");
    for (int index = 0; index < static_cast<int>(gPictures.size()); ++index) {
        if (gPictures[static_cast<std::size_t>(index)].key == key) {
            return gPictures[static_cast<std::size_t>(index)].ok ? index : -1;
        }
    }
    CachedPicture picture;
    picture.key = key;
    std::ifstream in(file, std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    RoboVideoClip clip;
    if (!bytes.empty() && RoboVideoDecode(clip, bytes.data(), bytes.size(), file.string())) {
        RoboVideoNode video;
        video.time = timeMs;
        video.playing = playing;
        RoboVideoBind(video, clip);
        const std::size_t count = static_cast<std::size_t>(std::max(0, clip.width)) * static_cast<std::size_t>(std::max(0, clip.height));
        const int stored = count == 0 ? 0 : static_cast<int>(clip.frames.size() / count);
        if (stored > 0) {
            picture.image.sourceWidth = clip.width;
            picture.image.sourceHeight = clip.height;
            picture.image.format = clip.format;
            const int step = clip.frameMs > 0 ? clip.frameMs : 1;
            if (playing && stored > 1) {
                picture.image.pixels.assign(clip.frames.begin(), clip.frames.begin() + static_cast<std::ptrdiff_t>(count));
                picture.image.frames = clip.frames;
                picture.image.frameCount = stored;
                picture.image.delays.assign(static_cast<std::size_t>(stored), step);
            } else {
                int index = clip.time / step;
                if (index >= stored) {
                    index = stored - 1;
                }
                if (index < 0) {
                    index = 0;
                }
                const std::size_t begin = static_cast<std::size_t>(index) * count;
                picture.image.pixels.assign(clip.frames.begin() + static_cast<std::ptrdiff_t>(begin),
                                            clip.frames.begin() + static_cast<std::ptrdiff_t>(begin + count));
                picture.image.frameCount = 1;
            }
            picture.ok = true;
        } else {
            int frameWidth = 0;
            int frameHeight = 0;
            int sampleMs = 0;
            if (WindowsVideoFrame(file, timeMs, picture.image.pixels, frameWidth, frameHeight, sampleMs)) {
                picture.image.sourceWidth = frameWidth;
                picture.image.sourceHeight = frameHeight;
                picture.image.format = clip.format;
                picture.image.frameCount = 1;
                picture.videoPlaying = playing;
                picture.videoFile = file;
                picture.videoStart = timeMs;
                picture.videoDuration = clip.duration;
                picture.videoShown = sampleMs;
                picture.ok = true;
            }
        }
    }
    gPictures.push_back(std::move(picture));
    return gPictures.back().ok ? static_cast<int>(gPictures.size()) - 1 : -1;
}

std::filesystem::path FindAssets(const std::string& document) {
    auto climb = [](std::filesystem::path dir) {
        for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
            std::error_code error;
            if (std::filesystem::exists(dir / "Assets" / "Icons.rpack", error) ||
                std::filesystem::exists(dir / "Assets" / "Icons", error)) {
                return dir / "Assets";
            }
            if (!dir.has_parent_path() || dir.parent_path() == dir) {
                break;
            }
            dir = dir.parent_path();
        }
        return std::filesystem::path{};
    };
    std::filesystem::path found;
    if (!document.empty()) {
        found = climb(std::filesystem::path(document).parent_path());
    }
    if (found.empty()) {
        std::error_code error;
        found = climb(std::filesystem::current_path(error));
    }
    return found;
}

std::string TrimCopy(std::string text) {
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(start, end - start);
}

bool ButtonOn(const RoboXMLNode& node, const char* key);

std::string ButtonSpinner() {
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 32 32\" fill=\"none\">"
           "<circle cx=\"16\" cy=\"16\" r=\"14\" stroke=\"currentColor\" stroke-opacity=\"0.35\" stroke-width=\"2\"/>"
           "<circle cx=\"16\" cy=\"16\" r=\"14\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" "
           "stroke-dasharray=\"24.63 63.33\"/></svg>";
}

std::string ButtonSlot(const RoboXMLNode& node) {
    std::string slot = RoboXMLAttr(node, "slot");
    if (slot.empty()) {
        slot = RoboXMLAttr(node, "side");
    }
    if (slot.empty()) {
        slot = RoboXMLAttr(node, "place");
    }
    return Lower(slot);
}

std::string TextCopy(const RoboXMLNode& node) {
    const std::string body = TrimCopy(node.text);
    if (!body.empty()) {
        return body;
    }
    const char* keys[] = {"value", "content", "text", "label"};
    for (const char* key : keys) {
        const std::string value = TrimCopy(RoboXMLAttr(node, key));
        if (!value.empty()) {
            return value;
        }
    }
    return {};
}

std::string ButtonLabel(const RoboXMLNode& node) {
    if (ButtonOn(node, "loading")) {
        const std::string loading = RoboXMLAttr(node, "loadingtext");
        return loading.empty() ? "Loading..." : loading;
    }
    return TextCopy(node);
}

void ResolveButtonIcons(const RoboXMLNode& node, std::string& leading, std::string& trailing) {
    leading.clear();
    trailing.clear();
    if (ButtonOn(node, "loading")) {
        leading = ButtonSpinner();
        return;
    }
    leading = RoboWebSvg(gAssets, RoboXMLAttr(node, "leading"));
    trailing = RoboWebSvg(gAssets, RoboXMLAttr(node, "trailing"));
    for (const RoboXMLNode& child : node.children) {
        const std::string childName = Lower(child.name);
        if (childName != "icon" && childName != "svg") {
            continue;
        }
        const std::string icon = RoboWebIconMarkup(gAssets, child);
        if (icon.empty()) {
            continue;
        }
        const std::string slot = ButtonSlot(child);
        if (slot == "trailing" || slot == "right" || slot == "end") {
            trailing = icon;
        } else if (slot == "leading" || slot == "left" || slot == "start" || leading.empty()) {
            leading = icon;
        } else {
            trailing = icon;
        }
    }
}

double gPaintScale = 1;
int gScrollX = 0;

struct ScrollRegion {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int contentWidth = 0;
    int contentHeight = 0;
    int scrollX = 0;
    int scrollY = 0;
    int paintedX = 0;
    int paintedY = 0;
    int opBegin = 0;
    int opEnd = 0;
    int opacityX = 0;
    int opacityY = 0;
    int shownX = 0;
    int shownY = 0;
    unsigned idleX = 0;
    unsigned idleY = 0;
    int hotX = 0;
    int hotY = 0;
    bool dragX = false;
    bool dragY = false;
};

std::vector<ScrollRegion> gRegions;
int gRegionDrag = -1;
int gRegionGrab = 0;
int gRegionAxis = 0;

struct FieldEdit {
    std::string value;
    int caret = 0;
    int selA = 0;
    int selB = 0;
    int anchor = 0;
};

struct Editor {
    std::string id;
    std::string value;
    std::string placeholder;
    int caret = 0;
    int selA = 0;
    int selB = 0;
    int anchor = 0;
    int columnX = -1;
    std::vector<FieldEdit> undo;
    std::vector<FieldEdit> redo;
    bool secret = false;
    bool multiline = false;
    bool disabled = false;
    bool focused = false;
    RoboU32 caretColor = 0xFFF4F7FBu;
    RoboU32 ink = 0xFFF4F7FBu;
    RoboU32 hint = 0xFF8E8E93u;
    int op = -1;
    int fieldX = 0;
    int fieldY = 0;
    int fieldW = 0;
    int fieldH = 0;
};

std::vector<Editor> gEditors;
const std::vector<Editor>* gEditorKept = nullptr;
unsigned gFieldCaretTick = 0;
int gFieldCaretCover = 255;
int gFieldCaretEditor = -1;
unsigned gFieldRevision = 0;

void FieldTouch() {
    gFieldCaretTick = GetTickCount();
    gFieldCaretCover = 255;
}

int ScaleLen(int value, double scale) {
    if (value == 0 || scale == 1.0) {
        return value;
    }
    const int scaled = static_cast<int>(std::lround(static_cast<double>(value) * scale));
    if (scaled == 0) {
        return value < 0 ? -1 : 1;
    }
    return scaled;
}

int Px(int value) {
    int scaled = value;
    if (value != 0 && gDpi != 96) {
        scaled = static_cast<int>((static_cast<long long>(value) * static_cast<long long>(gDpi) + 48) / 96);
        if (scaled == 0) {
            scaled = value < 0 ? -1 : 1;
        }
    }
    if (gPaintScale != 1.0) {
        return ScaleLen(scaled, gPaintScale);
    }
    return scaled;
}

bool IsDigits(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return true;
}

int LengthOf(const std::string& raw) {
    if (raw.empty()) {
        return 0;
    }
    std::size_t begin = 0;
    int sign = 1;
    if (raw[0] == '+') {
        begin = 1;
    } else if (raw[0] == '-') {
        sign = -1;
        begin = 1;
    }
    const std::string body = raw.substr(begin);
    if (IsDigits(body)) {
        const int magnitude = Px(std::stoi(body));
        return sign < 0 ? -magnitude : magnitude;
    }
    if (sign < 0) {
        return 0;
    }
    const RoboI32 space = RoboFoundationPlatformSpace(raw.c_str());
    if (space > 0) {
        return Px(space);
    }
    const RoboI32 radius = RoboFoundationPlatformRadius(raw.c_str());
    return Px(radius > 0 ? radius : 0);
}

int MeasureLength(const std::string& raw, int basis) {
    const std::string folded = Lower(raw);
    if (folded == "full" || folded == "fill") {
        return std::max(0, basis);
    }
    if (raw.size() > 1 && raw.back() == '%') {
        const std::string number = raw.substr(0, raw.size() - 1);
        std::size_t begin = 0;
        int sign = 1;
        if (!number.empty() && number[0] == '+') {
            begin = 1;
        } else if (!number.empty() && number[0] == '-') {
            sign = -1;
            begin = 1;
        }
        const std::string digits = number.substr(begin);
        if (basis > 0 && IsDigits(digits)) {
            return sign * basis * std::stoi(digits) / 100;
        }
    }
    return LengthOf(raw);
}

int AttrLength(const RoboXMLNode& node, const char* key, int fallback = 0) {
    const std::string value = RoboXMLAttr(node, key);
    return value.empty() ? fallback : LengthOf(value);
}

bool IsText(const std::string& name) {
    return name == "text" || name == "heading" || name == "subhead" || name == "body" || name == "paragraph" || name == "p" ||
           name == "caption" || name == "eyebrow" || name == "label" || name == "span" || name == "h1" || name == "h2" ||
           name == "h3" || name == "h4" || name == "h5" || name == "h6";
}

bool IsBox(const std::string& name) {
    return name == "screen" || name == "window" || name == "page" || name == "container" || name == "view" ||
           name == "board" || name == "box" || name == "flex" || name == "stack" || name == "canvas" || name == "scrollview" ||
           name == "list";
}

bool Named(const std::string& name, const std::string& face, const char* key) {
    return name == key || face == key;
}

int FaceOf(const std::string& font) {
    const std::string key = Lower(font);
    if (key.find("semibold") != std::string::npos) {
        return 2;
    }
    if (key.find("medium") != std::string::npos) {
        return 1;
    }
    if (key.find("bold") != std::string::npos) {
        return 3;
    }
    if (key.find("italic") != std::string::npos) {
        return 4;
    }
    return 0;
}

struct TypeStyle {
    int size = 17;
    int leading = 24;
    int face = 0;
    const char* color = "text-primary";
};

TypeStyle StyleOf(const RoboXMLNode& node) {
    const std::string name = RoboUIFace(node);
    std::string role = "menubar";
    const char* color = "text-primary";
    if (name == "heading" || name == "h1" || name == "h2" || name == "h3" || name == "h4" || name == "h5" || name == "h6") {
        std::string level = Lower(RoboXMLAttr(node, "level", "1"));
        if (name.size() == 2 && name[0] == 'h') {
            level = name.substr(1);
        }
        if (level == "display") {
            role = "display";
        } else if (level == "2") {
            role = "h2";
        } else if (level == "3") {
            role = "h3";
        } else if (level == "4" || level == "5" || level == "6") {
            role = "h1";
        } else {
            role = "h1";
        }
        color = "text-primary";
    } else if (name == "subhead") {
        role = "subhead";
    } else if (name == "caption") {
        role = "caption";
        color = "text-tertiary";
    } else if (name == "eyebrow") {
        role = "eyebrow";
        color = "text-tertiary";
    } else if (name == "label") {
        role = "button";
        color = "text-primary";
    } else if (name == "body") {
        const std::string size = Lower(RoboXMLAttr(node, "size"));
        role = (size == "sm" || size == "small") ? "body" : "body-lg";
    }
    TypeStyle style;
    style.color = color;
    style.face = FaceOf(RoboXMLAttr(node, "font"));
    RoboFoundationTypeMetrics metrics{};
    if (RoboFoundationPlatformType(role.c_str(), &metrics) == ROBO_TRUE) {
        style.size = metrics.sizePx;
        style.leading = std::max(style.size, style.size * metrics.leadingMilli / 1000);
    }
    if (name == "heading" || (name.size() == 2 && name[0] == 'h')) {
        const std::string level = name.size() == 2 ? name.substr(1) : Lower(RoboXMLAttr(node, "level", "1"));
        if (level == "1" || level == "display") {
            style.size = 22;
        } else if (level == "2") {
            style.size = 17;
        } else if (level == "3") {
            style.size = 15;
        } else {
            style.size = 13;
        }
        style.leading = style.size + style.size / 3;
        if (style.face == 0) {
            style.face = 2;
        }
    }
    style.size = Px(style.size);
    style.leading = Px(style.leading);
    const std::string fontSize = RoboXMLAttr(node, "fontsize");
    int forced = fontSize.empty() ? 0 : LengthOf(fontSize);
    if (forced <= 0) {
        const std::string sizeName = RoboXMLAttr(node, "size");
        if (!sizeName.empty() && sizeName.find_first_not_of("0123456789") == std::string::npos) {
            forced = LengthOf(sizeName);
        }
    }
    if (forced > 0) {
        style.size = forced;
        style.leading = forced + std::max(1, forced / 4);
    }
    return style;
}

RoboFontView FaceFont(int face, int size, RoboFontHeader& header) {
    header = {};
    header.magic = ROBO_FONT_MAGIC;
    header.ascent = static_cast<RoboU16>(size);
    header.glyphCount = 0;
    header.firstCodepoint = static_cast<RoboU16>(face);
    RoboFontView font{};
    font.header = &header;
    return font;
}

int MeasureText(const std::string& text, int size, int face) {
    RoboFontHeader header{};
    const RoboFontView font = FaceFont(face, size, header);
    return RoboTextMeasureUTF8(font, text.c_str(), static_cast<RoboU16>(size));
}

struct PaintOp {
    bool text = false;
    bool stroke = false;
    bool button = false;
    bool focus = false;
    bool drawing = false;
    int borderPx = 0;
    int fade = 255;
    RoboU32 border = 0;
    std::string drawingSvg;
    std::string leadingSvg;
    std::string trailingSvg;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int radius = 0;
    RoboU32 fill = 0;
    RoboU32 color = 0;
    int size = 0;
    int wrapWidth = 0;
    int wrapSize = 0;
    int face = 0;
    int align = 0;
    int wrap = 1;
    int maxLines = 0;
    int overflow = 0;
    int whiteSpace = 1;
    bool breakLong = true;
    std::string value;
    std::string role;
    int control = -1;
    int kind = 0;
    int preview = 0;
    bool quiet = false;
    bool iconButton = false;
    bool loading = false;
    int select = 0;
    int picture = -1;
    std::string pictureFit;
    int canvas = -1;
    int editor = -1;
};

int PictureChannel(RoboU32 pixel, int shift) {
    return static_cast<int>((pixel >> shift) & 0xFFu);
}

RoboU32 PictureMix(const RoboU32* pixels, int sourceWidth, int sourceHeight, float sourceX, float sourceY) {
    if (pixels == nullptr || sourceWidth < 1 || sourceHeight < 1) {
        return 0;
    }
    if (sourceX < 0.0f) {
        sourceX = 0.0f;
    }
    if (sourceY < 0.0f) {
        sourceY = 0.0f;
    }
    if (sourceX > static_cast<float>(sourceWidth - 1)) {
        sourceX = static_cast<float>(sourceWidth - 1);
    }
    if (sourceY > static_cast<float>(sourceHeight - 1)) {
        sourceY = static_cast<float>(sourceHeight - 1);
    }
    const int left = static_cast<int>(sourceX);
    const int top = static_cast<int>(sourceY);
    const int right = left + 1 < sourceWidth ? left + 1 : left;
    const int bottom = top + 1 < sourceHeight ? top + 1 : top;
    const float across = sourceX - static_cast<float>(left);
    const float down = sourceY - static_cast<float>(top);
    const RoboU32 samples[4] = {
        pixels[static_cast<std::size_t>(top) * static_cast<std::size_t>(sourceWidth) + static_cast<std::size_t>(left)],
        pixels[static_cast<std::size_t>(top) * static_cast<std::size_t>(sourceWidth) + static_cast<std::size_t>(right)],
        pixels[static_cast<std::size_t>(bottom) * static_cast<std::size_t>(sourceWidth) + static_cast<std::size_t>(left)],
        pixels[static_cast<std::size_t>(bottom) * static_cast<std::size_t>(sourceWidth) + static_cast<std::size_t>(right)],
    };
    int channels[4];
    for (int shiftIndex = 0; shiftIndex < 4; ++shiftIndex) {
        const int shift = shiftIndex * 8;
        const float topMix = PictureChannel(samples[0], shift) * (1.0f - across) + PictureChannel(samples[1], shift) * across;
        const float bottomMix = PictureChannel(samples[2], shift) * (1.0f - across) + PictureChannel(samples[3], shift) * across;
        channels[shiftIndex] = static_cast<int>(topMix * (1.0f - down) + bottomMix * down + 0.5f);
    }
    return (static_cast<RoboU32>(channels[3]) << 24) | (static_cast<RoboU32>(channels[2]) << 16) |
           (static_cast<RoboU32>(channels[1]) << 8) | static_cast<RoboU32>(channels[0]);
}

int PictureFrameIndex(const RoboImageRecord& image, unsigned clock) {
    if (image.frameCount <= 1) {
        return 0;
    }
    int total = 0;
    for (int index = 0; index < image.frameCount; ++index) {
        const int delay = index < static_cast<int>(image.delays.size()) && image.delays[static_cast<std::size_t>(index)] > 0
                              ? image.delays[static_cast<std::size_t>(index)]
                              : 100;
        total += delay;
    }
    if (total < 1) {
        return 0;
    }
    int at = static_cast<int>(clock % static_cast<unsigned>(total));
    int cursor = 0;
    for (int index = 0; index < image.frameCount; ++index) {
        const int delay = index < static_cast<int>(image.delays.size()) && image.delays[static_cast<std::size_t>(index)] > 0
                              ? image.delays[static_cast<std::size_t>(index)]
                              : 100;
        if (at < cursor + delay) {
            return index;
        }
        cursor += delay;
    }
    return 0;
}

const RoboU32* PicturePixels(const RoboImageRecord& image, int frame, std::size_t& count) {
    count = static_cast<std::size_t>(std::max(0, image.sourceWidth)) * static_cast<std::size_t>(std::max(0, image.sourceHeight));
    if (count == 0) {
        return nullptr;
    }
    if (frame > 0) {
        const std::size_t begin = static_cast<std::size_t>(frame) * count;
        if (begin + count <= image.frames.size()) {
            return image.frames.data() + begin;
        }
    }
    if (image.pixels.size() < count) {
        return nullptr;
    }
    return image.pixels.data();
}

void BlendPicture(RoboGPU* gpu, const PaintOp& op, int y, const std::vector<RoboU32>& pixels) {
    const int width = std::min(op.width, static_cast<int>(pixels.size()));
    for (int row = 0; row < op.height; ++row) {
        for (int column = 0; column < op.width; ++column) {
            const std::size_t index = static_cast<std::size_t>(row) * static_cast<std::size_t>(op.width) + static_cast<std::size_t>(column);
            if (index >= pixels.size()) {
                return;
            }
            const RoboU32 pixel = pixels[index];
            const RoboU8 alpha = static_cast<RoboU8>((pixel >> 24) & 0xFF);
            if (alpha == 0) {
                continue;
            }
            RoboGPUBlendPixel(gpu, op.x + column, y + row, pixel, alpha);
        }
    }
    (void)width;
}

void PaintPicture(RoboGPU* gpu, const PaintOp& op, int y) {
    if (gpu == nullptr || op.picture < 0 || op.picture >= static_cast<int>(gPictures.size()) || op.width <= 0 || op.height <= 0) {
        return;
    }
    CachedPicture& cached = gPictures[static_cast<std::size_t>(op.picture)];
    const RoboImageRecord& image = cached.image;
    const std::string fit = op.pictureFit.empty() ? "contain" : op.pictureFit;
    const int frameIndex = PictureFrameIndex(image, gPictureClock);
    if (cached.cacheW == op.width && cached.cacheH == op.height && cached.cacheFrame == frameIndex && cached.cacheFit == fit &&
        cached.cache.size() == static_cast<std::size_t>(op.width) * static_cast<std::size_t>(op.height)) {
        BlendPicture(gpu, op, y, cached.cache);
        return;
    }
    cached.cache.assign(static_cast<std::size_t>(op.width) * static_cast<std::size_t>(op.height), 0);
    cached.cacheW = op.width;
    cached.cacheH = op.height;
    cached.cacheFrame = frameIndex;
    cached.cacheFit = fit;
    if (!image.vector.empty()) {
        RoboImageRecord drawn = image;
        drawn.fit = fit;
        drawn.hasWidth = true;
        drawn.width = op.width;
        drawn.hasHeight = true;
        drawn.height = op.height;
        RoboImageFrame frame;
        RoboImagePaint(drawn, frame);
        const int width = std::min(op.width, frame.width);
        const int height = std::min(op.height, frame.height);
        for (int row = 0; row < height; ++row) {
            for (int column = 0; column < width; ++column) {
                cached.cache[static_cast<std::size_t>(row) * static_cast<std::size_t>(op.width) + static_cast<std::size_t>(column)] =
                    frame.pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(column)];
            }
        }
        BlendPicture(gpu, op, y, cached.cache);
        return;
    }
    std::size_t count = 0;
    const RoboU32* pixels = PicturePixels(image, frameIndex, count);
    const int sourceWidth = image.sourceWidth;
    const int sourceHeight = image.sourceHeight;
    if (pixels == nullptr || sourceWidth <= 0 || sourceHeight <= 0) {
        BlendPicture(gpu, op, y, cached.cache);
        return;
    }
    int destWidth = sourceWidth;
    int destHeight = sourceHeight;
    int originX = 0;
    int originY = 0;
    if (fit == "fill") {
        destWidth = op.width;
        destHeight = op.height;
    } else if (fit != "none") {
        const long long widthRatio = static_cast<long long>(op.width) * sourceHeight;
        const long long heightRatio = static_cast<long long>(op.height) * sourceWidth;
        const bool widthLimits = fit == "cover" ? widthRatio >= heightRatio : widthRatio <= heightRatio;
        if (widthLimits) {
            destWidth = op.width;
            destHeight = static_cast<int>(static_cast<long long>(sourceHeight) * op.width / sourceWidth);
        } else {
            destHeight = op.height;
            destWidth = static_cast<int>(static_cast<long long>(sourceWidth) * op.height / sourceHeight);
        }
        if (destWidth < 1) {
            destWidth = 1;
        }
        if (destHeight < 1) {
            destHeight = 1;
        }
        if (fit == "scale-down" && op.width >= sourceWidth && op.height >= sourceHeight) {
            destWidth = sourceWidth;
            destHeight = sourceHeight;
        }
        originX = (op.width - destWidth) / 2;
        originY = (op.height - destHeight) / 2;
    }
    for (int row = 0; row < op.height; ++row) {
        const int localY = row - originY;
        if (localY < 0 || localY >= destHeight) {
            continue;
        }
        const float sourceY = (static_cast<float>(localY) + 0.5f) * static_cast<float>(sourceHeight) / static_cast<float>(destHeight) - 0.5f;
        for (int column = 0; column < op.width; ++column) {
            const int localX = column - originX;
            if (localX < 0 || localX >= destWidth) {
                continue;
            }
            const float sourceX = (static_cast<float>(localX) + 0.5f) * static_cast<float>(sourceWidth) / static_cast<float>(destWidth) - 0.5f;
            cached.cache[static_cast<std::size_t>(row) * static_cast<std::size_t>(op.width) + static_cast<std::size_t>(column)] =
                PictureMix(pixels, sourceWidth, sourceHeight, sourceX, sourceY);
        }
    }
    BlendPicture(gpu, op, y, cached.cache);
}

void PaintCanvas(RoboGPU* gpu, const PaintOp& op, int y) {
    if (gpu == nullptr || op.canvas < 0 || op.canvas >= static_cast<int>(gCanvases.size()) || op.width <= 0 || op.height <= 0) {
        return;
    }
    CachedCanvas& cached = gCanvases[static_cast<std::size_t>(op.canvas)];
    if (cached.width <= 0 || cached.height <= 0 || cached.pixels.empty()) {
        return;
    }
    if (cached.width == op.width && cached.height == op.height) {
        BlendPicture(gpu, op, y, cached.pixels);
        return;
    }
    if (cached.cacheW != op.width || cached.cacheH != op.height ||
        cached.cache.size() != static_cast<std::size_t>(op.width) * static_cast<std::size_t>(op.height)) {
        cached.cache.assign(static_cast<std::size_t>(op.width) * static_cast<std::size_t>(op.height), 0);
        cached.cacheW = op.width;
        cached.cacheH = op.height;
        for (int row = 0; row < op.height; ++row) {
            const float sourceY = (static_cast<float>(row) + 0.5f) * static_cast<float>(cached.height) / static_cast<float>(op.height) - 0.5f;
            for (int column = 0; column < op.width; ++column) {
                const float sourceX = (static_cast<float>(column) + 0.5f) * static_cast<float>(cached.width) / static_cast<float>(op.width) - 0.5f;
                cached.cache[static_cast<std::size_t>(row) * static_cast<std::size_t>(op.width) + static_cast<std::size_t>(column)] =
                    PictureMix(cached.pixels.data(), cached.width, cached.height, sourceX, sourceY);
            }
        }
    }
    BlendPicture(gpu, op, y, cached.cache);
}

int TextSelectMode(const RoboXMLNode& node, bool content) {
    std::string value = Lower(RoboXMLAttr(node, "selectable"));
    if (value.empty()) {
        value = Lower(RoboXMLAttr(node, "select"));
    }
    if (value.empty()) {
        value = Lower(RoboXMLAttr(node, "userselect"));
    }
    if (value.empty()) {
        value = Lower(RoboXMLAttr(node, "selection"));
    }
    if (value == "false" || value == "0" || value == "no" || value == "off" || value == "none") {
        return 0;
    }
    if (value == "all") {
        return 2;
    }
    if (value == "true" || value == "1" || value == "yes" || value == "on" || value == "text") {
        return 1;
    }
    return content ? 1 : 0;
}

const char* ButtonFill(const std::string& variant) {
    if (variant == "secondary" || variant == "ghost" || variant == "action") {
        return nullptr;
    }
    if (variant == "danger" || variant == "destructive") {
        return "danger";
    }
    return "brand-primary";
}

const char* ButtonInk(const std::string& variant) {
    if (variant == "secondary") {
        return "brand-primary";
    }
    if (variant == "ghost") {
        return "text-tertiary";
    }
    if (variant == "action") {
        return "text-primary";
    }
    return "text-on-brand";
}

int ButtonHeight(const RoboXMLNode& node) {
    const int asked = AttrLength(node, "height");
    if (asked > 0) {
        return asked;
    }
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    if (size == "sm" || size == "small") {
        return Px(22);
    }
    if (size == "lg" || size == "large") {
        return Px(32);
    }
    return Px(28);
}

int ButtonPad(const RoboXMLNode& node) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    if (size == "sm" || size == "small") {
        return Px(8);
    }
    if (size == "lg" || size == "large") {
        return Px(16);
    }
    return Px(12);
}

int ButtonFontSize(const RoboXMLNode& node) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    return Px(size == "lg" || size == "large" ? 14 : 13);
}

bool ButtonOn(const RoboXMLNode& node, const char* key) {
    const std::string value = Lower(RoboXMLAttr(node, key));
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

void FillRound(RoboGPU* gpu, int x, int y, int width, int height, int radius, RoboU32 color) {
    if (width <= 0 || height <= 0) {
        return;
    }
    radius = std::max(0, std::min(radius, std::min(width, height) / 2));
    if (radius <= 1) {
        RoboGPUFillRect(gpu, x, y, width, height, color);
        return;
    }
    RoboGPUFillRect(gpu, x + radius, y, width - radius * 2, height, color);
    RoboGPUFillRect(gpu, x, y + radius, radius, height - radius * 2, color);
    RoboGPUFillRect(gpu, x + width - radius, y + radius, radius, height - radius * 2, color);
    for (int row = 0; row < radius; ++row) {
        int inside = 0;
        const int dy = radius - row;
        while (inside * inside + dy * dy <= radius * radius) {
            ++inside;
        }
        inside = std::max(0, inside - 1);
        const int cap = radius - inside;
        if (cap > 0) {
            RoboGPUFillRect(gpu, x + cap, y + row, radius - cap, 1, color);
            RoboGPUFillRect(gpu, x + width - radius, y + row, radius - cap, 1, color);
            RoboGPUFillRect(gpu, x + cap, y + height - 1 - row, radius - cap, 1, color);
            RoboGPUFillRect(gpu, x + width - radius, y + height - 1 - row, radius - cap, 1, color);
        }
    }
}

int Padding(const RoboXMLNode& node) {
    if (!RoboXMLAttr(node, "padding").empty()) {
        return AttrLength(node, "padding");
    }
    return AttrLength(node, "paddingx");
}

int Gap(const RoboXMLNode& node) {
    if (!RoboXMLAttr(node, "gap").empty()) {
        return AttrLength(node, "gap");
    }
    return AttrLength(node, "spacing");
}

bool RowDirection(const RoboXMLNode& node) {
    const std::string display = Lower(RoboXMLAttr(node, "display"));
    const std::string direction = Lower(RoboXMLAttr(node, "direction"));
    return display == "horizontal" || direction == "row" || direction == "row-reverse";
}

struct HitState {
    std::vector<WindowControl>* controls = nullptr;
    int hot = -1;
    int pressed = -1;
    int next = 0;
    int pointerX = -1;
    int pointerY = -1;
    bool pointerTaken = false;
};

int PlaceNode(const RoboXMLNode& node, int x, int y, int width, bool light, std::vector<PaintOp>& ops, HitState* hits, int slotHeight = -1);

int IconBox(const RoboXMLNode& node) {
    int size = AttrLength(node, "size", 0);
    if (size <= 0) {
        size = AttrLength(node, "width", 0);
    }
    return size <= 0 ? Px(24) : size;
}

std::string IconDrawing(const RoboXMLNode& node) {
    const std::string name = RoboUIFace(node);
    if (name == "svg") {
        std::string svg;
        RoboCompilerSerializeSvgNode(node, svg, true);
        if (!svg.empty()) {
            return svg;
        }
    }
    std::string icon = RoboXMLAttr(node, "name");
    if (icon.empty()) {
        icon = RoboXMLAttr(node, "icon");
    }
    if (icon.empty()) {
        icon = RoboXMLAttr(node, "src");
    }
    if (icon.empty() && name != "icon") {
        icon = node.name;
    }
    return RoboWebSvg(gAssets, icon);
}

int IconButtonExtent(const RoboXMLNode& node) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    if (size == "xs" || size == "extra-small") {
        return Px(28);
    }
    if (size == "sm" || size == "small") {
        return Px(32);
    }
    if (size == "lg" || size == "large") {
        return Px(48);
    }
    if (size == "xl" || size == "extra-large") {
        return Px(56);
    }
    if (size == "2xl") {
        return Px(64);
    }
    if (size == "3xl") {
        return Px(72);
    }
    return Px(28);
}

std::string IconButtonMark(bool success) {
    const char* path = success ? "M5.5 12.5L10 17L18.5 7.5" : "M6 6L18 18M18 6L6 18";
    std::string svg = "<svg viewBox=\"0 0 24 24\" fill=\"none\"><path d=\"";
    svg += path;
    svg += "\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\"";
    if (success) {
        svg += " stroke-linejoin=\"round\"";
    }
    svg += "></path></svg>";
    return svg;
}

RoboU32 ThemeColor(const char* name, bool light);

void StyleIconButton(PaintOp& op, bool light, bool hot, bool pressed, bool focused) {
    const bool disabled = op.quiet;
    int look = 0;
    if (!disabled && pressed) {
        look = 3;
    } else if (!disabled && hot) {
        look = 1;
    }
    op.fade = disabled ? 140 : 255;
    op.face = 1;
    op.borderPx = 0;
    op.fill = 0;
    op.border = 0;
    op.focus = focused && !disabled;
    const RoboU32 ink = ThemeColor("text-primary", light);
    const RoboU32 onBrand = ThemeColor("text-on-brand", light);
    if (op.kind == 11) {
        op.fill = ThemeColor(look == 3 ? "bg-active" : look == 1 ? "bg-hover" : "bg-secondary", light);
        op.color = ink;
    } else if (op.kind == 12) {
        op.border = ThemeColor(look == 3 ? "text-primary" : look == 1 ? "border-strong" : "border-default", light);
        op.borderPx = std::max(1, Px(1));
        op.color = ink;
        if (look == 3) {
            op.fill = ThemeColor("bg-active", light);
        } else if (look == 1) {
            op.fill = ThemeColor("bg-hover", light);
        }
    } else if (op.kind == 13) {
        const char* token = look == 3 ? "brand-primary-active" : look == 1 ? "brand-primary-hover" : "brand-primary";
        op.fill = ThemeColor(token, light);
        op.color = onBrand;
    } else if (op.kind == 14) {
        op.fill = ThemeColor(look == 1 ? "danger-hover" : "danger", light);
        op.color = onBrand;
        if (look == 3) {
            const RoboU32 red = ((op.fill >> 16) & 0xFFu) * 78u / 100u;
            const RoboU32 green = ((op.fill >> 8) & 0xFFu) * 78u / 100u;
            const RoboU32 blue = (op.fill & 0xFFu) * 78u / 100u;
            op.fill = 0xFF000000u | (red << 16) | (green << 8) | blue;
        }
    } else {
        op.color = ink;
        if (look == 3) {
            op.fill = ThemeColor("bg-active", light);
        } else if (look == 1 || op.focus) {
            op.fill = ThemeColor("bg-hover", light);
        }
    }
    if (op.kind == 11 && op.focus && look != 3) {
        op.fill = ThemeColor("bg-hover", light);
    } else if (op.kind == 12 && op.focus && look != 3) {
        op.border = ThemeColor("border-focus", light);
        op.borderPx = std::max(1, Px(1));
        if (look != 1) {
            op.fill = 0;
        }
    } else if (op.kind == 13 && op.focus && look != 3) {
        op.fill = ThemeColor("brand-primary-hover", light);
    } else if (op.kind == 14 && op.focus && look != 3) {
        op.fill = ThemeColor("danger-hover", light);
    }
    if (op.preview == 2 && op.kind != 13 && op.kind != 14) {
        op.color = ThemeColor("success", light);
    } else if (op.preview == 3 && op.kind != 13 && op.kind != 14) {
        op.color = ThemeColor("danger", light);
    }
}

void SwitchMetrics(const RoboXMLNode& node, int& trackW, int& trackH, int& thumb) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    if (size == "sm" || size == "small") {
        trackW = Px(32);
        trackH = Px(18);
        thumb = Px(14);
    } else if (size == "lg" || size == "large") {
        trackW = Px(46);
        trackH = Px(26);
        thumb = Px(22);
    } else {
        trackW = Px(38);
        trackH = Px(22);
        thumb = Px(18);
    }
}

void PaintSwitchControl(std::vector<PaintOp>& ops, const RoboXMLNode& node, int x, int y, bool light) {
    int trackW = 0;
    int trackH = 0;
    int thumb = 0;
    SwitchMetrics(node, trackW, trackH, thumb);
    const std::string variant = Lower(RoboXMLAttr(node, "variant", "primary"));
    const bool checked = ButtonOn(node, "checked");
    const bool loading = ButtonOn(node, "loading");
    const bool disabled = ButtonOn(node, "disabled") || loading;
    const int fade = disabled && !loading ? 140 : 255;
    PaintOp track;
    track.button = true;
    track.x = x;
    track.y = y;
    track.width = trackW;
    track.height = trackH;
    track.fade = fade;
    if (checked && variant == "danger") {
        track.fill = ThemeColor("danger", light);
    } else if (checked) {
        track.fill = ThemeColor("brand-primary", light);
        if (variant == "outline") {
            track.border = track.fill;
            track.borderPx = std::max(1, Px(1));
        }
    } else if (variant == "subtle") {
        track.fill = ThemeColor("bg-tertiary", light);
    } else if (variant == "outline") {
        track.border = ThemeColor("border-default", light);
        track.borderPx = std::max(1, Px(1));
    } else if (variant == "ghost") {
        track.fill = 0;
    } else if (light) {
        track.fill = ThemeColor("border-default", light);
    } else {
        track.fill = ThemeColor("bg-active", light);
    }
    ops.push_back(track);
    const int inset = std::max(1, Px(2));
    const int travel = std::max(0, trackW - inset * 2 - thumb);
    PaintOp knob;
    knob.button = true;
    knob.x = x + inset + (checked ? travel : 0);
    knob.y = y + inset;
    knob.width = thumb;
    knob.height = thumb;
    knob.fade = fade;
    if (!checked && variant == "ghost") {
        knob.fill = ThemeColor("bg-elevated", light);
        knob.border = ThemeColor("border-default", light);
        knob.borderPx = std::max(1, Px(1));
    } else {
        knob.fill = 0xFFFFFFFFu;
    }
    if (loading) {
        knob.loading = true;
        knob.color = ThemeColor("brand-primary", light);
    }
    ops.push_back(knob);
}

struct SegmentFace {
    std::string label;
    std::string icon;
    std::string badge;
    bool loading = false;
    bool disabled = false;
};

void SegmentFaces(const RoboXMLNode& node, std::vector<SegmentFace>& faces, int& selected) {
    const std::string wanted = RoboXMLAttr(node, "value");
    selected = 0;
    int index = 0;
    for (const RoboXMLNode& child : node.children) {
        if (Lower(child.name) != "segment") {
            continue;
        }
        SegmentFace face;
        face.label = RoboXMLAttr(child, "label");
        if (face.label.empty()) {
            face.label = TextCopy(child);
        }
        face.icon = RoboWebSvg(gAssets, RoboXMLAttr(child, "icon"));
        face.badge = RoboXMLAttr(child, "badge");
        const std::string status = Lower(RoboXMLAttr(child, "status"));
        face.loading = ButtonOn(child, "loading") || status == "loading";
        face.disabled = ButtonOn(child, "disabled");
        if (!wanted.empty() && RoboXMLAttr(child, "value") == wanted) {
            selected = index;
        }
        faces.push_back(std::move(face));
        ++index;
    }
}

int SegmentTrackHeight(const std::string& size) {
    if (size == "sm" || size == "small") {
        return Px(36);
    }
    if (size == "lg" || size == "large") {
        return Px(50);
    }
    return Px(44);
}

int SegmentFontSize(const std::string& size) {
    return std::max(1, size == "sm" || size == "small" ? Px(15) : Px(17));
}

int SegmentCellWidth(const SegmentFace& face, const std::string& content, const std::string& size) {
    const bool icon = content == "icon";
    int minimum = Px(72);
    int pad = Px(16);
    if (icon) {
        minimum = size == "lg" || size == "large" ? Px(50) : size == "sm" || size == "small" ? Px(36) : Px(44);
        pad = size == "lg" || size == "large" ? Px(14) : size == "sm" || size == "small" ? Px(10) : Px(12);
    } else if (size == "sm" || size == "small") {
        pad = Px(12);
    } else if (size == "lg" || size == "large") {
        pad = Px(20);
    }
    int width = pad * 2;
    const int font = SegmentFontSize(size);
    if (icon || content == "both") {
        if (!face.icon.empty()) {
            width += font;
        }
    }
    if (!icon || face.icon.empty()) {
        width += MeasureText(face.label, font, 1);
        if (!face.badge.empty()) {
            width += Px(8) + MeasureText(face.badge, std::max(1, Px(12)), 1);
        }
    }
    if (content == "both" && !face.icon.empty()) {
        width += Px(8);
    }
    return std::max(minimum, width);
}

RoboU32 MixColor(RoboU32 first, RoboU32 second, int firstPercent) {
    auto channel = [&](int shift) {
        const int a = static_cast<int>((first >> shift) & 255u);
        const int b = static_cast<int>((second >> shift) & 255u);
        return (a * firstPercent + b * (100 - firstPercent)) / 100;
    };
    return 0xFF000000u | (static_cast<RoboU32>(channel(16)) << 16) | (static_cast<RoboU32>(channel(8)) << 8) |
           static_cast<RoboU32>(channel(0));
}

int SegmentNaturalWidth(const RoboXMLNode& node) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    const std::string content = Lower(RoboXMLAttr(node, "content", "label"));
    const bool grouped = Lower(RoboXMLAttr(node, "mode", "inset")) == "grouped";
    std::vector<SegmentFace> faces;
    int selected = 0;
    SegmentFaces(node, faces, selected);
    int width = grouped ? 0 : Px(4);
    for (const SegmentFace& face : faces) {
        width += SegmentCellWidth(face, content == "icon" || content == "both" ? content : "label", size);
    }
    const std::string label = RoboXMLAttr(node, "label");
    if (!label.empty()) {
        width = std::max(width, MeasureText(label, std::max(1, Px(12)), 0));
    }
    return std::max(1, width);
}

int PaintSegmented(std::vector<PaintOp>& ops, const RoboXMLNode& node, int x, int y, int width, bool light) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    std::string content = Lower(RoboXMLAttr(node, "content", "label"));
    if (content != "icon" && content != "both") {
        content = "label";
    }
    std::string variant = Lower(RoboXMLAttr(node, "variant", "subtle"));
    if (variant != "outline" && variant != "ghost" && variant != "primary" && variant != "pill") {
        variant = "subtle";
    }
    const bool grouped = Lower(RoboXMLAttr(node, "mode", "inset")) == "grouped";
    const bool disabled = ButtonOn(node, "disabled");
    std::vector<SegmentFace> faces;
    int selected = 0;
    SegmentFaces(node, faces, selected);
    const int count = static_cast<int>(faces.size());
    if (count == 0) {
        return 0;
    }
    if (selected < 0 || selected >= count) {
        selected = 0;
    }
    int top = y;
    const std::string label = RoboXMLAttr(node, "label");
    if (!label.empty()) {
        PaintOp caption;
        caption.text = true;
        caption.select = TextSelectMode(node, true);
        caption.value = label;
        caption.x = x;
        caption.y = top;
        caption.width = width;
        caption.height = Px(12);
        caption.size = std::max(1, Px(12));
        caption.color = ThemeColor("text-tertiary", light);
        ops.push_back(caption);
        top += Px(12) + Px(8);
    }
    const int trackH = SegmentTrackHeight(size);
    const int inset = grouped ? 0 : std::max(1, Px(2));
    const int radius = variant == "pill" ? trackH / 2 : (size == "sm" || size == "small" ? Px(10) : Px(12));
    PaintOp track;
    track.x = x;
    track.y = top;
    track.width = width;
    track.height = trackH;
    track.radius = radius;
    if (variant == "outline") {
        track.fill = ThemeColor("border-default", light);
        ops.push_back(track);
        PaintOp hole;
        hole.x = x + std::max(1, Px(1));
        hole.y = top + std::max(1, Px(1));
        hole.width = std::max(1, width - std::max(1, Px(1)) * 2);
        hole.height = std::max(1, trackH - std::max(1, Px(1)) * 2);
        hole.radius = std::max(0, radius - std::max(1, Px(1)));
        hole.fill = ThemeColor("bg-primary", light);
        ops.push_back(hole);
    } else if (variant != "ghost") {
        track.fill = ThemeColor("bg-tertiary", light);
        ops.push_back(track);
    }
    const int inner = std::max(1, width - inset * 2);
    const int cell = std::max(1, inner / count);
    RoboU32 thumb = ThemeColor("bg-elevated", light);
    if (variant == "primary") {
        thumb = ThemeColor("brand-primary", light);
    } else if (!light) {
        thumb = MixColor(ThemeColor("bg-tertiary", light), 0xFFFFFFFFu, 72);
    } else if (variant == "outline") {
        thumb = ThemeColor("bg-secondary", light);
    } else if (variant == "ghost") {
        thumb = ThemeColor("bg-tertiary", light);
    }
    if (!grouped) {
        PaintOp slider;
        slider.x = x + inset + cell * selected;
        slider.y = top + inset;
        slider.width = selected == count - 1 ? inner - cell * selected : cell;
        slider.height = std::max(1, trackH - inset * 2);
        slider.radius = variant == "pill" ? slider.height / 2 : std::max(0, radius - inset);
        slider.fill = thumb;
        ops.push_back(slider);
    }
    const RoboU32 rule = ThemeColor(light ? "border-default" : "border-strong", light);
    if (variant != "ghost") {
        for (int index = 1; index < count; ++index) {
            if (index == selected || index == selected + 1) {
                continue;
            }
            PaintOp line;
            line.x = x + inset + cell * index;
            line.y = top + trackH / 4;
            line.width = std::max(1, Px(1));
            line.height = trackH / 2;
            line.fill = rule;
            ops.push_back(line);
        }
    }
    const int font = SegmentFontSize(size);
    for (int index = 0; index < count; ++index) {
        const SegmentFace& face = faces[static_cast<std::size_t>(index)];
        const int cellX = x + inset + cell * index;
        const int cellW = index == count - 1 ? inner - cell * index : cell;
        const bool on = index == selected;
        if (grouped && on) {
            PaintOp chosen;
            chosen.x = cellX;
            chosen.y = top;
            chosen.width = cellW;
            chosen.height = trackH;
            chosen.radius = index == 0 || index == count - 1 ? radius : 0;
            chosen.fill = thumb;
            ops.push_back(chosen);
        }
        RoboU32 ink = on ? ThemeColor(variant == "primary" ? "text-on-brand" : "text-primary", light)
                         : ThemeColor("text-secondary", light);
        if (face.disabled || disabled) {
            ink = ThemeColor("text-tertiary", light);
        }
        if (face.loading) {
            PaintOp spin;
            spin.drawing = true;
            spin.loading = true;
            spin.color = ink;
            const int icon = std::max(1, Px(16));
            spin.x = cellX + std::max(0, (cellW - icon) / 2);
            spin.y = top + std::max(0, (trackH - icon) / 2);
            spin.width = icon;
            spin.height = icon;
            ops.push_back(spin);
            continue;
        }
        const bool showIcon = (content == "icon" || content == "both") && !face.icon.empty();
        const bool showLabel = content != "icon" || face.icon.empty();
        int contentW = 0;
        if (showIcon) {
            contentW += font;
        }
        int textW = 0;
        if (showLabel) {
            textW = MeasureText(face.label, font, on ? 1 : 0);
            contentW += textW;
            if (showIcon) {
                contentW += Px(8);
            }
            if (!face.badge.empty()) {
                contentW += Px(8) + MeasureText(face.badge, std::max(1, Px(12)), 1);
            }
        }
        int cursor = cellX + std::max(0, (cellW - contentW) / 2);
        const int mid = top + trackH / 2;
        if (showIcon) {
            PaintOp glyph;
            glyph.drawing = true;
            glyph.drawingSvg = face.icon;
            glyph.color = ink;
            glyph.x = cursor;
            glyph.y = mid - font / 2;
            glyph.width = font;
            glyph.height = font;
            ops.push_back(glyph);
            cursor += font + (showLabel ? Px(8) : 0);
        }
        if (showLabel) {
            PaintOp text;
            text.text = true;
            text.value = face.label;
            text.x = cursor;
            text.y = mid - font / 2;
            text.width = textW;
            text.height = font;
            text.size = font;
            text.face = on ? 1 : 0;
            text.color = ink;
            ops.push_back(text);
            cursor += textW;
            if (!face.badge.empty()) {
                PaintOp badge;
                badge.text = true;
                badge.value = face.badge;
                badge.x = cursor + Px(8);
                badge.y = mid - Px(12) / 2;
                badge.width = MeasureText(face.badge, std::max(1, Px(12)), 1);
                badge.height = Px(12);
                badge.size = std::max(1, Px(12));
                badge.face = 1;
                badge.color = ink;
                ops.push_back(badge);
            }
        }
    }
    return (top - y) + trackH;
}

bool IsIcon(const RoboXMLNode& node) {
    const std::string name = RoboUIFace(node);
    return name == "icon" || name == "svg" || RoboCompilerIsIconNode(node);
}

bool FlagOn(const std::string& raw) {
    const std::string value = Lower(raw);
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

bool FillsBox(const RoboXMLNode& node) {
    if (FlagOn(RoboXMLAttr(node, "fill")) || FlagOn(RoboXMLAttr(node, "fillparent"))) {
        return true;
    }
    return Lower(RoboXMLAttr(node, "size")) == "fill";
}

int LimitLength(const RoboXMLNode& node, bool vertical, int value, int basis) {
    if (value < 0) {
        return value;
    }
    const int span = basis > 0 ? basis : value;
    const std::string minRaw = RoboXMLAttr(node, vertical ? "minheight" : "minwidth");
    const std::string maxRaw = RoboXMLAttr(node, vertical ? "maxheight" : "maxwidth");
    if (!minRaw.empty()) {
        const int floor = MeasureLength(minRaw, span);
        if (floor > value) {
            value = floor;
        }
    }
    if (!maxRaw.empty()) {
        const int cap = MeasureLength(maxRaw, span);
        if (cap > 0 && cap < value) {
            value = cap;
        }
    }
    return std::max(0, value);
}

int AskedLength(const RoboXMLNode& node, const char* key, int basis) {
    const bool vertical = key != nullptr && std::string(key) == "height";
    const std::string raw = RoboXMLAttr(node, key);
    const std::string size = Lower(RoboXMLAttr(node, "size"));
    if (raw.empty() || Lower(raw) == "auto") {
        if (FillsBox(node) && basis > 0) {
            return basis;
        }
        if (size == "min") {
            const std::string minRaw = RoboXMLAttr(node, vertical ? "minheight" : "minwidth");
            if (!minRaw.empty()) {
                return std::max(0, MeasureLength(minRaw, basis > 0 ? basis : 1));
            }
        }
        if (size == "max") {
            const std::string maxRaw = RoboXMLAttr(node, vertical ? "maxheight" : "maxwidth");
            if (!maxRaw.empty()) {
                return std::max(0, MeasureLength(maxRaw, basis > 0 ? basis : 1));
            }
        }
        return -1;
    }
    return LimitLength(node, vertical, std::max(0, MeasureLength(raw, basis)), basis);
}

int ContentWidth(const RoboXMLNode& node, int maxWidth) {
    const int cap = AttrLength(node, "maxwidth");
    if (cap > 0 && cap < maxWidth) {
        maxWidth = cap;
    }
    const int asked = AskedLength(node, "width", maxWidth);
    if (asked >= 0) {
        return std::min(maxWidth, std::max(1, asked));
    }
    if (FillsBox(node)) {
        return std::max(1, maxWidth);
    }
    const std::string name = Lower(node.name);
    const std::string face = RoboUIFace(node);
    if (name == "audio") {
        return 0;
    }
    if (name == "image" || name == "video") {
        int natural = AttrLength(node, "height");
        if (natural <= 0) {
            natural = Px(160);
        }
        return std::min(maxWidth, std::max(1, natural));
    }
    if (name == "view3d" || name == "scene3d") {
        return maxWidth;
    }
    if (Named(name, face, "anchor")) {
        std::string label = TextCopy(node);
        const std::string sizeName = Lower(RoboXMLAttr(node, "size", "inherit"));
        int size = 14;
        if (sizeName == "sm" || sizeName == "small") size = 12;
        else if (sizeName == "md" || sizeName == "medium" || sizeName == "lg" || sizeName == "large") size = 17;
        const std::string variant = Lower(RoboXMLAttr(node, "variant", "inline"));
        const int weight = (variant == "arrow" || variant == "standalone" || variant == "more") ? 1 : 0;
        return std::min(maxWidth, std::max(1, MeasureText(label, size, weight) + Px(28)));
    }
    if (IsText(name)) {
        const TypeStyle style = StyleOf(node);
        return std::min(maxWidth, std::max(1, MeasureText(TextCopy(node), style.size, style.face)));
    }
    if (IsIcon(node)) {
        return std::min(maxWidth, IconBox(node));
    }
    if (Named(name, face, "iconbutton")) {
        return std::min(maxWidth, IconButtonExtent(node));
    }
    if (Named(name, face, "switch") || Lower(RoboXMLAttr(node, "role")) == "switch") {
        int trackW = 0;
        int trackH = 0;
        int thumb = 0;
        SwitchMetrics(node, trackW, trackH, thumb);
        return std::min(maxWidth, trackW);
    }
    if (Named(name, face, "switchitem") || Named(name, face, "switchgroup")) {
        return maxWidth;
    }
    if (Named(name, face, "segmentedcontrol")) {
        if (ButtonOn(node, "fullWidth")) {
            return maxWidth;
        }
        return std::min(maxWidth, SegmentNaturalWidth(node));
    }
    if (Named(name, face, "segmentedtoolbar")) {
        return maxWidth;
    }
    if (Named(name, face, "button") || (Lower(RoboXMLAttr(node, "role")) == "button" && !Named(name, face, "iconbutton"))) {
        const std::string variant = Lower(RoboXMLAttr(node, "variant", "primary"));
        const int face = variant == "ghost" ? 0 : 1;
        const std::string label = ButtonLabel(node);
        int width = MeasureText(label, ButtonFontSize(node), face) + ButtonPad(node) * 2;
        const int icon = Px(16);
        const int gap = Px(8);
        std::string leading;
        std::string trailing;
        ResolveButtonIcons(node, leading, trailing);
        if (!leading.empty()) {
            width += icon + gap;
        }
        if (!trailing.empty()) {
            width += icon + gap;
        }
        const std::string full = Lower(RoboXMLAttr(node, "fullwidth", "true"));
        if (full != "false" && full != "0" && full != "no" && full != "off") {
            return std::max(width, maxWidth);
        }
        return std::min(maxWidth, width);
    }
    if (!IsBox(name)) {
        int width = 0;
        int count = 0;
        for (const RoboXMLNode& child : node.children) {
            width += ContentWidth(child, maxWidth);
            ++count;
        }
        if (count > 1) {
            width += Gap(node) * (count - 1);
        }
        return std::min(maxWidth, std::max(width, 0));
    }
    int used = 0;
    const int inner = std::max(0, maxWidth - Padding(node) * 2);
    const int gap = Gap(node);
    if (RowDirection(node)) {
        int count = 0;
        for (const RoboXMLNode& child : node.children) {
            used += ContentWidth(child, inner);
            ++count;
        }
        if (count > 1) {
            used += gap * (count - 1);
        }
    } else {
        for (const RoboXMLNode& child : node.children) {
            used = std::max(used, ContentWidth(child, inner));
        }
    }
    return std::min(maxWidth, used + Padding(node) * 2);
}

int HexDigit(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

RoboU32 ThemeColor(const char* name, bool light) {
    if (name != nullptr && name[0] == '#') {
        const std::string hex(name + 1);
        std::string expanded = hex;
        if (hex.size() == 3 || hex.size() == 4) {
            expanded.clear();
            for (char ch : hex) {
                expanded.push_back(ch);
                expanded.push_back(ch);
            }
        }
        RoboU32 value = 0;
        bool ok = expanded.size() == 6 || expanded.size() == 8;
        for (char ch : expanded) {
            const int digit = HexDigit(ch);
            if (digit < 0) {
                ok = false;
                break;
            }
            value = (value << 4) | static_cast<RoboU32>(digit);
        }
        if (ok && expanded.size() == 6) {
            value |= 0xFF000000u;
        }
        if (ok) {
            return value;
        }
    }
    return RoboFoundationPlatformColorForTheme(name, light ? ROBO_TRUE : ROBO_FALSE);
}

void RecolorButton(PaintOp& op, bool light, bool hot, bool pressed, bool focused) {
    const bool disabled = op.quiet;
    const bool showActive = op.preview == 3 || (pressed && !disabled);
    const bool showHover = !showActive && (op.preview == 1 || (hot && !disabled));
    int look = 0;
    if (showActive) {
        look = 3;
    } else if (showHover) {
        look = 1;
    }
    op.focus = (op.preview == 2 || focused) && !disabled;
    op.fade = disabled && !op.loading ? 140 : 255;
    op.borderPx = 0;
    op.fill = 0;
    op.border = 0;
    const RoboU32 brand = ThemeColor("brand-primary", light);
    const RoboU32 onBrand = ThemeColor("text-on-brand", light);
    if (op.kind == 1) {
        op.border = brand;
        op.borderPx = std::max(1, Px(1));
        op.color = brand;
        if (look == 3) {
            op.fill = ThemeColor("brand-primary-active", light);
            op.border = op.fill;
            op.color = onBrand;
        } else if (look == 1) {
            op.fill = brand;
            op.color = onBrand;
        }
    } else if (op.kind == 2) {
        op.fill = ThemeColor(light ? "bg-elevated" : "bg-secondary", light);
        op.border = ThemeColor(light ? "border-default" : "border-strong", light);
        op.borderPx = std::max(1, Px(1));
        op.color = ThemeColor("text-primary", light);
        if (look == 3) {
            op.fill = ThemeColor("bg-active", light);
            op.border = ThemeColor("text-primary", light);
        } else if (look == 1) {
            op.fill = ThemeColor("bg-hover", light);
            op.border = ThemeColor("text-tertiary", light);
        }
    } else if (op.kind == 3) {
        op.color = ThemeColor("text-tertiary", light);
        if (look == 3) {
            op.fill = ThemeColor("brand-primary-subtle", light);
            op.color = ThemeColor("brand-primary-active", light);
        } else if (look == 1) {
            op.color = brand;
        }
    } else if (op.kind == 4) {
        op.fill = ThemeColor(look == 1 ? "danger-hover" : "danger", light);
        op.color = onBrand;
        if (look == 3) {
            const RoboU32 red = ((op.fill >> 16) & 0xFFu) * 78u / 100u;
            const RoboU32 green = ((op.fill >> 8) & 0xFFu) * 78u / 100u;
            const RoboU32 blue = (op.fill & 0xFFu) * 78u / 100u;
            op.fill = 0xFF000000u | (red << 16) | (green << 8) | blue;
        }
    } else {
        const char* token = "brand-primary";
        if (look == 1) {
            token = "brand-primary-hover";
        } else if (look == 3) {
            token = "brand-primary-active";
        }
        op.fill = ThemeColor(token, light);
        op.color = onBrand;
    }
    if (op.focus && op.borderPx > 0) {
        op.border = brand;
    }
}

void StyleButton(const RoboXMLNode& node, bool light, bool hot, bool pressed, PaintOp& op) {
    std::string variant = Lower(RoboXMLAttr(node, "variant", "primary"));
    if (variant == "destructive" || variant == "danger") {
        variant = "danger";
    } else if (variant == "bordered" || variant == "outlined" || variant == "outline" || variant == "secondary") {
        variant = "action";
    } else if (variant == "tinted") {
        variant = "secondary";
    } else if (variant == "plain" || variant == "text" || variant == "ghost" || variant == "link") {
        variant = "ghost";
    } else if (variant == "filled" || variant == "elevated" || variant == "default") {
        variant = "primary";
    }
    const std::string preview = Lower(RoboXMLAttr(node, "preview"));
    op.kind = variant == "secondary" ? 1 : variant == "action" ? 2 : variant == "ghost" ? 3 : variant == "danger" ? 4 : 0;
    op.preview = preview == "hover" ? 1 : preview == "focus" ? 2 : preview == "active" ? 3 : 0;
    op.quiet = ButtonOn(node, "disabled") || ButtonOn(node, "loading");
    op.face = op.kind == 3 ? 0 : 1;
    op.size = ButtonFontSize(node);
    RecolorButton(op, light, hot, pressed, false);
}

struct TextLine {
    std::string text;
    int width = 0;
    int origin = 0;
};

struct TextFlow {
    int mode = 1;
    int maxLines = 0;
    int overflow = 0;
    int whiteSpace = 1;
    bool breakLong = true;
};

int Utf8Span(const std::string& text, std::size_t index) {
    if (index >= text.size()) {
        return 1;
    }
    const auto lead = static_cast<unsigned char>(text[index]);
    int span = 1;
    if ((lead & 0xE0u) == 0xC0u) {
        span = 2;
    } else if ((lead & 0xF0u) == 0xE0u) {
        span = 3;
    } else if ((lead & 0xF8u) == 0xF0u) {
        span = 4;
    }
    if (index + static_cast<std::size_t>(span) > text.size()) {
        return 1;
    }
    return span;
}

void PopUtf8(std::string& text) {
    if (text.empty()) {
        return;
    }
    std::size_t index = text.size() - 1;
    while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0u) == 0x80u) {
        --index;
    }
    text.erase(index);
}

void FitLine(std::string& line, int width, int size, int face, bool dots) {
    const std::string mark = dots ? "\xE2\x80\xA6" : "";
    if (width <= 0) {
        return;
    }
    if (MeasureText(line + mark, size, face) <= width) {
        if (dots && mark.size() > 0 && line.find('\xE2') == std::string::npos) {
            line += mark;
        }
        return;
    }
    while (!line.empty() && MeasureText(line + mark, size, face) > width) {
        PopUtf8(line);
    }
    line += mark;
    if (MeasureText(line, size, face) > width) {
        line = mark;
    }
}

TextFlow FlowOf(const RoboXMLNode& node) {
    TextFlow flow;
    std::string wrap = Lower(RoboXMLAttr(node, "textwrap"));
    if (wrap.empty()) {
        wrap = Lower(RoboXMLAttr(node, "wrap"));
    }
    if (wrap.empty()) {
        wrap = "word";
    }
    if (wrap == "none" || wrap == "nowrap" || wrap == "false") {
        flow.mode = 0;
    } else if (wrap == "char" || wrap == "character") {
        flow.mode = 2;
    } else if (wrap == "balance") {
        flow.mode = 3;
    } else if (wrap == "pretty") {
        flow.mode = 4;
    } else {
        flow.mode = 1;
    }
    std::string space = Lower(RoboXMLAttr(node, "whitespace"));
    if (space.empty()) {
        space = Lower(RoboXMLAttr(node, "white-space"));
    }
    if (space == "nowrap") {
        flow.mode = 0;
        flow.whiteSpace = 0;
    } else if (space == "pre") {
        flow.whiteSpace = 2;
    } else if (space == "pre-line" || space == "preline") {
        flow.whiteSpace = 3;
    } else if (space == "normal") {
        flow.whiteSpace = 0;
    } else if (space == "pre-wrap" || space == "prewrap") {
        flow.whiteSpace = 1;
    } else {
        flow.whiteSpace = flow.mode == 0 ? 0 : 1;
    }
    std::string lines = RoboXMLAttr(node, "lines");
    if (lines.empty()) {
        lines = RoboXMLAttr(node, "maxlines");
    }
    if (IsDigits(lines)) {
        flow.maxLines = std::max(1, std::stoi(lines));
    }
    std::string overflow = Lower(RoboXMLAttr(node, "textoverflow"));
    if (overflow.empty()) {
        overflow = Lower(RoboXMLAttr(node, "overflow"));
    }
    if (overflow == "ellipsis") {
        flow.overflow = 1;
    } else if (overflow == "clip" || overflow == "hidden") {
        flow.overflow = 2;
    } else if (flow.maxLines > 0 || flow.mode == 0) {
        flow.overflow = flow.mode == 0 ? 0 : 1;
    }
    const std::string longBreak = Lower(RoboXMLAttr(node, "overflowwrap"));
    if (longBreak == "normal") {
        flow.breakLong = false;
    } else if (longBreak == "anywhere" || longBreak == "break-word" || longBreak == "breakword") {
        flow.breakLong = true;
    } else {
        flow.breakLong = false;
    }
    return flow;
}

void AppendChars(const std::string& paragraph, int base, int width, int size, int face, std::vector<TextLine>& lines) {
    std::string line;
    int lineWidth = 0;
    int lineOrigin = base;
    for (std::size_t index = 0; index < paragraph.size();) {
        const int span = Utf8Span(paragraph, index);
        const std::string piece = paragraph.substr(index, static_cast<std::size_t>(span));
        const int pieceAt = base + static_cast<int>(index);
        index += static_cast<std::size_t>(span);
        if (piece == "\n") {
            lines.push_back(TextLine{line, lineWidth, lineOrigin});
            line.clear();
            lineWidth = 0;
            lineOrigin = base + static_cast<int>(index);
            continue;
        }
        const int pieceWidth = MeasureText(piece, size, face);
        if (width > 0 && !line.empty() && lineWidth + pieceWidth > width) {
            lines.push_back(TextLine{line, lineWidth, lineOrigin});
            line = piece;
            lineWidth = pieceWidth;
            lineOrigin = pieceAt;
            continue;
        }
        if (line.empty()) {
            lineOrigin = pieceAt;
        }
        line += piece;
        lineWidth += pieceWidth;
    }
    lines.push_back(TextLine{line, lineWidth, lineOrigin});
}

void AppendWrapped(const std::string& paragraph, int base, int width, int size, int face, bool breakLong, std::vector<TextLine>& lines) {
    std::string line;
    int lineWidth = 0;
    int lineOrigin = base;
    std::size_t index = 0;
    const auto flush = [&]() {
        if (line.empty()) {
            return;
        }
        lines.push_back(TextLine{line, lineWidth, lineOrigin});
        line.clear();
        lineWidth = 0;
    };
    while (index < paragraph.size()) {
        while (index < paragraph.size()) {
            const char ch = paragraph[index];
            if (ch != ' ' && ch != '\t' && ch != '\r') {
                break;
            }
            ++index;
        }
        const std::size_t wordAt = index;
        while (index < paragraph.size()) {
            const char ch = paragraph[index];
            if (ch == ' ' || ch == '\t' || ch == '\r') {
                break;
            }
            index += static_cast<std::size_t>(Utf8Span(paragraph, index));
        }
        if (wordAt == index) {
            break;
        }
        const std::string piece = paragraph.substr(wordAt, index - wordAt);
        const int pieceAt = base + static_cast<int>(wordAt);
        const int pieceWidth = MeasureText(piece, size, face);
        if (line.empty()) {
            if (breakLong && width > 0 && pieceWidth > width) {
                AppendChars(piece, pieceAt, width, size, face, lines);
                if (!lines.empty()) {
                    line = lines.back().text;
                    lineWidth = lines.back().width;
                    lineOrigin = lines.back().origin;
                    lines.pop_back();
                }
            } else {
                line = piece;
                lineWidth = pieceWidth;
                lineOrigin = pieceAt;
            }
            continue;
        }
        const int gap = MeasureText(" ", size, face);
        if (width > 0 && lineWidth + gap + pieceWidth > width) {
            flush();
            if (breakLong && pieceWidth > width) {
                AppendChars(piece, pieceAt, width, size, face, lines);
                if (!lines.empty()) {
                    line = lines.back().text;
                    lineWidth = lines.back().width;
                    lineOrigin = lines.back().origin;
                    lines.pop_back();
                }
            } else {
                line = piece;
                lineWidth = pieceWidth;
                lineOrigin = pieceAt;
            }
            continue;
        }
        line.push_back(' ');
        line += piece;
        lineWidth += gap + pieceWidth;
    }
    flush();
}

std::string CollapseSpaces(const std::string& value, bool keepBreaks) {
    std::string out;
    bool space = false;
    for (std::size_t index = 0; index < value.size();) {
        const int span = Utf8Span(value, index);
        const std::string piece = value.substr(index, static_cast<std::size_t>(span));
        index += static_cast<std::size_t>(span);
        const bool br = piece == "\n";
        const bool blank = piece == " " || piece == "\t" || piece == "\r" || (!keepBreaks && br);
        if (blank) {
            space = !out.empty();
            continue;
        }
        if (br) {
            if (!out.empty() && out.back() == ' ') {
                out.pop_back();
            }
            out.push_back('\n');
            space = false;
            continue;
        }
        if (space) {
            out.push_back(' ');
            space = false;
        }
        out += piece;
    }
    return out;
}

void WrapSource(const std::string& value, int width, int size, int face, int mode, bool breakLong, std::vector<TextLine>& lines) {
    lines.clear();
    if (mode == 0) {
        std::string flat = value;
        for (char& ch : flat) {
            if (ch == '\n' || ch == '\t' || ch == '\r') {
                ch = ' ';
            }
        }
        lines.push_back(TextLine{flat, MeasureText(flat, size, face), 0});
        return;
    }
    if (mode == 2) {
        AppendChars(value, 0, width, size, face, lines);
    } else {
        std::size_t start = 0;
        while (start <= value.size()) {
            const std::size_t mark = value.find('\n', start);
            const std::string paragraph = value.substr(start, mark == std::string::npos ? std::string::npos : mark - start);
            const std::size_t before = lines.size();
            AppendWrapped(paragraph, static_cast<int>(start), width, size, face, breakLong, lines);
            if (lines.size() == before) {
                lines.push_back(TextLine{});
            }
            if (mark == std::string::npos) {
                break;
            }
            start = mark + 1;
        }
    }
    if (lines.empty()) {
        lines.push_back(TextLine{});
    }
}

void ClampLines(std::vector<TextLine>& lines, int width, int size, int face, const TextFlow& flow, bool more) {
    if (flow.mode == 0 && !lines.empty() && width > 0 && lines[0].width > width && flow.overflow != 0) {
        FitLine(lines[0].text, width, size, face, flow.overflow == 1);
        lines[0].width = MeasureText(lines[0].text, size, face);
    }
    if (flow.maxLines <= 0 || static_cast<int>(lines.size()) <= flow.maxLines) {
        return;
    }
    const bool ellipsis = flow.overflow != 2;
    lines.resize(static_cast<std::size_t>(flow.maxLines));
    if (more || ellipsis) {
        FitLine(lines.back().text, width, size, face, ellipsis);
        lines.back().width = MeasureText(lines.back().text, size, face);
    }
}

void BalanceLines(std::vector<TextLine>& lines, int width, int size, int face) {
    if (lines.size() < 2 || width <= 0) {
        return;
    }
    TextLine& previous = lines[lines.size() - 2];
    TextLine& last = lines.back();
    const std::size_t cut = previous.text.rfind(' ');
    if (cut == std::string::npos || last.text.find(' ') != std::string::npos) {
        return;
    }
    const std::string moved = previous.text.substr(cut + 1);
    const std::string stay = previous.text.substr(0, cut);
    const std::string joined = moved + " " + last.text;
    if (stay.empty() || moved.empty() || MeasureText(joined, size, face) > width) {
        return;
    }
    last.origin = previous.origin + static_cast<int>(stay.size()) + 1;
    last.text = joined;
    last.width = MeasureText(joined, size, face);
    previous.text = stay;
    previous.width = MeasureText(stay, size, face);
}

void MarkLineOrigins(const std::string& source, std::vector<TextLine>& lines) {
    std::size_t cursor = 0;
    for (TextLine& line : lines) {
        if (line.text.empty()) {
            line.origin = static_cast<int>(cursor);
            continue;
        }
        const std::size_t found = source.find(line.text, cursor);
        if (found == std::string::npos) {
            line.origin = static_cast<int>(cursor);
            cursor += line.text.size();
        } else {
            line.origin = static_cast<int>(found);
            cursor = found + line.text.size();
        }
    }
}

void TextLines(const std::string& value, int width, int size, int face, const TextFlow& flow, std::vector<TextLine>& lines) {
    std::string source = value;
    if (flow.whiteSpace == 2) {
        TextFlow hard = flow;
        hard.mode = 2;
        hard.breakLong = false;
        source = value;
        WrapSource(source, 0, size, face, 2, false, lines);
        for (TextLine& line : lines) {
            line.width = MeasureText(line.text, size, face);
        }
        ClampLines(lines, width, size, face, flow, false);
        return;
    }
    if (flow.whiteSpace == 0) {
        source = CollapseSpaces(value, false);
    } else if (flow.whiteSpace == 3) {
        source = CollapseSpaces(value, true);
    }
    const int mode = flow.mode == 3 || flow.mode == 4 ? 1 : flow.mode;
    int useWidth = width;
    if ((flow.mode == 3 || flow.mode == 4) && width > 8) {
        std::vector<TextLine> full;
        WrapSource(source, width, size, face, 1, flow.breakLong, full);
        const int count = std::max(1, static_cast<int>(full.size()));
        int low = 8;
        int high = width;
        int best = width;
        while (low <= high) {
            const int mid = (low + high) / 2;
            std::vector<TextLine> trial;
            WrapSource(source, mid, size, face, 1, flow.breakLong, trial);
            if (static_cast<int>(trial.size()) <= count) {
                best = mid;
                high = mid - 1;
            } else {
                low = mid + 1;
            }
        }
        useWidth = best;
    }
    WrapSource(source, useWidth, size, face, mode, flow.breakLong, lines);
    if (flow.mode == 4) {
        BalanceLines(lines, width, size, face);
    }
    const bool more = flow.maxLines > 0 && static_cast<int>(lines.size()) > flow.maxLines;
    ClampLines(lines, width, size, face, flow, more);
}

int TextBlockHeight(const std::string& value, int width, int size, int face, int leading, const TextFlow& flow) {
    std::vector<TextLine> lines;
    TextLines(value, width, size, face, flow, lines);
    return std::max(leading, size) * static_cast<int>(std::max<std::size_t>(1, lines.size()));
}

struct BoxEdge {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
};

BoxEdge BoxEdges(const RoboXMLNode& node, const char* all, const char* axisX, const char* axisY, const char* top,
                 const char* right, const char* bottom, const char* left, int basis) {
    BoxEdge edge;
    const int uniform = MeasureLength(RoboXMLAttr(node, all), basis);
    const std::string rawX = RoboXMLAttr(node, axisX);
    const std::string rawY = RoboXMLAttr(node, axisY);
    const int xAxis = rawX.empty() ? uniform : MeasureLength(rawX, basis);
    const int yAxis = rawY.empty() ? uniform : MeasureLength(rawY, basis);
    const auto pick = [&](const char* key, int axis) {
        const std::string raw = RoboXMLAttr(node, key);
        return raw.empty() ? axis : MeasureLength(raw, basis);
    };
    edge.top = pick(top, yAxis);
    edge.right = pick(right, xAxis);
    edge.bottom = pick(bottom, yAxis);
    edge.left = pick(left, xAxis);
    return edge;
}

int PackAlign(const std::string& raw) {
    const std::string key = Lower(raw);
    if (key == "center" || key == "middle") {
        return 1;
    }
    if (key == "end" || key == "flex-end" || key == "right" || key == "bottom") {
        return 2;
    }
    if (key == "stretch") {
        return 3;
    }
    if (key == "space-between") {
        return 4;
    }
    if (key == "space-around") {
        return 5;
    }
    if (key == "space-evenly") {
        return 6;
    }
    return 0;
}

void ShiftRange(std::vector<PaintOp>& ops, std::size_t begin, std::size_t end, HitState* hits, int controlBegin,
                int controlEnd, int dx, int dy) {
    for (std::size_t index = begin; index < end && index < ops.size(); ++index) {
        ops[index].x += dx;
        ops[index].y += dy;
    }
    if (hits == nullptr || hits->controls == nullptr || (dx == 0 && dy == 0)) {
        return;
    }
    const int limit = std::min(controlEnd, static_cast<int>(hits->controls->size()));
    for (int index = controlBegin; index < limit; ++index) {
        (*hits->controls)[index].x += dx;
        (*hits->controls)[index].y += dy;
    }
}

RoboU32 FieldHex(const std::string& text, RoboU32 fallback) {
    if (text.size() != 7 || text[0] != '#') {
        return fallback;
    }
    RoboU32 value = 0;
    for (int index = 1; index <= 6; ++index) {
        const int digit = HexDigit(text[static_cast<std::size_t>(index)]);
        if (digit < 0) {
            return fallback;
        }
        value = (value << 4) | static_cast<RoboU32>(digit);
    }
    return 0xFF000000u | value;
}

bool FieldFlag(const std::string& text) {
    const std::string folded = Lower(text);
    return folded == "true" || folded == "yes" || folded == "on" || folded == "1";
}

std::string EditorShown(const Editor& editor) {
    if (editor.value.empty()) {
        return editor.placeholder;
    }
    if (!editor.secret) {
        return editor.value;
    }
    std::string shown;
    for (std::size_t index = 0; index < editor.value.size();) {
        shown.push_back('*');
        index += static_cast<std::size_t>(Utf8Span(editor.value, index));
    }
    return shown;
}

void TextInk(int face, int size, int& above, int& below);

int PlaceField(const RoboXMLNode& node, int x, int y, int width, bool light, std::vector<PaintOp>& ops) {
    const bool multiline = FieldFlag(RoboXMLAttr(node, "multiline"));
    int boxW = AttrLength(node, "width");
    if (boxW <= 0 || boxW > width) {
        boxW = std::max(1, width);
    }
    int boxH = AttrLength(node, "height");
    if (boxH <= 0) {
        boxH = multiline ? Px(96) : Px(40);
    }
    const int radius = Px(6);
    const int pad = Px(10);
    PaintOp edge;
    edge.x = x;
    edge.y = y;
    edge.width = boxW;
    edge.height = boxH;
    edge.radius = radius;
    edge.fill = ThemeColor("border-default", light);
    ops.push_back(edge);
    PaintOp well;
    well.x = x + 1;
    well.y = y + 1;
    well.width = std::max(1, boxW - 2);
    well.height = std::max(1, boxH - 2);
    well.radius = std::max(0, radius - 1);
    const bool disabled = FieldFlag(RoboXMLAttr(node, "disabled"));
    well.fill = ThemeColor(disabled ? "bg-secondary" : "bg-elevated", light);
    ops.push_back(well);
    Editor editor;
    editor.id = RoboXMLAttr(node, "id");
    if (editor.id.empty()) {
        editor.id = "field-" + std::to_string(gEditors.size());
    }
    editor.value = RoboXMLAttr(node, "value");
    if (editor.value.empty()) {
        editor.value = RoboXMLAttr(node, "content");
    }
    if (editor.value.empty()) {
        editor.value = RoboXMLAttr(node, "text");
    }
    editor.placeholder = RoboXMLAttr(node, "placeholder");
    const std::string mask = Lower(RoboXMLAttr(node, "mask"));
    editor.secret = mask == "password" || mask == "hidden" || mask == "*";
    editor.multiline = multiline;
    editor.disabled = disabled;
    editor.ink = ThemeColor(disabled ? "text-disabled" : "text-primary", light);
    editor.hint = ThemeColor("text-tertiary", light);
    const std::string cursor = RoboXMLAttr(node, "cursorcolor").empty() ? RoboXMLAttr(node, "cursor-color") : RoboXMLAttr(node, "cursorcolor");
    editor.caretColor = FieldHex(cursor, editor.ink);
    editor.caret = static_cast<int>(editor.value.size());
    editor.selA = editor.caret;
    editor.selB = editor.caret;
    bool restored = false;
    if (gEditorKept != nullptr) {
        for (const Editor& kept : *gEditorKept) {
            if (kept.id != editor.id) {
                continue;
            }
            editor.value = kept.value;
            editor.caret = std::max(0, std::min(kept.caret, static_cast<int>(editor.value.size())));
            editor.selA = std::max(0, std::min(kept.selA, static_cast<int>(editor.value.size())));
            editor.selB = std::max(0, std::min(kept.selB, static_cast<int>(editor.value.size())));
            editor.anchor = std::max(0, std::min(kept.anchor, static_cast<int>(editor.value.size())));
            editor.columnX = kept.columnX;
            editor.undo = kept.undo;
            editor.redo = kept.redo;
            editor.focused = kept.focused;
            restored = true;
            break;
        }
    }
    if (!restored) {
        const std::string start = RoboXMLAttr(node, "selectionstart");
        const std::string end = RoboXMLAttr(node, "selectionend");
        if (!start.empty() && !end.empty() && IsDigits(start) && IsDigits(end)) {
            editor.selA = std::min(std::stoi(start), static_cast<int>(editor.value.size()));
            editor.selB = std::min(std::stoi(end), static_cast<int>(editor.value.size()));
            editor.caret = editor.selB;
        }
    }
    editor.fieldX = x;
    editor.fieldY = y;
    editor.fieldW = boxW;
    editor.fieldH = boxH;
    const int editorIndex = static_cast<int>(gEditors.size());
    gEditors.push_back(editor);
    PaintOp label;
    label.text = true;
    label.select = 0;
    label.editor = editorIndex;
    int fontSize = AttrLength(node, "fontsize");
    if (fontSize <= 0) {
        fontSize = 14;
    }
    fontSize = Px(fontSize);
    label.x = x + pad;
    if (multiline) {
        label.y = y + pad;
    } else {
        int above = 0;
        int below = 0;
        TextInk(0, fontSize, above, below);
        const int ink = std::max(1, above + below);
        const int inkTop = y + std::max(0, (boxH - ink) / 2);
        label.y = inkTop + above - fontSize;
    }
    label.width = std::max(1, boxW - pad * 2);
    label.height = std::max(1, boxH - (label.y - y) - pad);
    label.size = fontSize;
    label.face = 0;
    label.wrap = multiline ? 1 : 0;
    label.wrapWidth = label.width;
    label.overflow = 0;
    label.breakLong = true;
    label.value = EditorShown(gEditors[static_cast<std::size_t>(editorIndex)]);
    label.color = gEditors[static_cast<std::size_t>(editorIndex)].value.empty() ? editor.hint : editor.ink;
    ops.push_back(label);
    gEditors[static_cast<std::size_t>(editorIndex)].op = static_cast<int>(ops.size()) - 1;
    return boxH;
}

int PlaceNode(const RoboXMLNode& node, int x, int y, int width, bool light, std::vector<PaintOp>& ops, HitState* hits, int slotHeight) {
    const std::string name = Lower(node.name);
    const std::string face = RoboUIFace(node);
    std::string shown = Lower(RoboXMLAttr(node, "visibility"));
    if (shown.empty()) shown = Lower(RoboXMLAttr(node, "visible"));
    if (shown == "false" || shown == "0" || shown == "no" || shown == "off" || shown == "hidden" ||
        (shown.empty() && Lower(RoboXMLAttr(node, "hidden")) == "true")) {
        return 0;
    }
    if (name == "audio") {
        return 0;
    }
    if (name == "video") {
        int boxH = AttrLength(node, "height");
        if (boxH <= 0) {
            boxH = AttrLength(node, "width");
        }
        if (boxH <= 0) {
            boxH = Px(180);
        }
        int boxW = width;
        const int askedW = AttrLength(node, "width");
        if (askedW > 0) {
            boxW = std::min(width, askedW);
        }
        PaintOp box;
        box.x = x;
        box.y = y;
        box.width = boxW;
        box.height = boxH;
        box.fill = ThemeColor("bg-secondary", light);
        const std::string source = RoboXMLAttr(node, "source").empty() ? RoboXMLAttr(node, "src") : RoboXMLAttr(node, "source");
        if (!source.empty() && !gDocumentDir.empty()) {
            std::filesystem::path file(source);
            if (file.is_relative()) {
                file = gDocumentDir / file;
            }
            box.picture = VideoFor(file, VideoClock(node), VideoPlaying(node));
        }
        box.pictureFit = "contain";
        ops.push_back(box);
        return boxH;
    }
    if (name == "input" || name == "textfield") {
        return PlaceField(node, x, y, width, light, ops);
    }
        if (name == "canvas") {
            RoboCanvasNode drawn;
            std::string diagnostic;
            if (RoboCanvasRead(node, drawn, diagnostic)) {
                int boxW = AttrLength(node, "width");
                int boxH = AttrLength(node, "height");
                if (boxW <= 0) {
                    boxW = drawn.width;
                }
                if (boxH <= 0) {
                    boxH = drawn.height;
                }
                if (width > 0 && boxW > width) {
                    boxH = std::max(1, boxH * width / std::max(1, boxW));
                    boxW = width;
                }
                RoboFontPackView pack{};
                RoboCanvasFrame frame;
                RoboCanvasPaint(drawn, pack, frame);
                CachedCanvas cached;
                cached.width = frame.width;
                cached.height = frame.height;
                cached.pixels = std::move(frame.pixels);
                const int canvasIndex = static_cast<int>(gCanvases.size());
                gCanvases.push_back(std::move(cached));
                PaintOp box;
                box.x = x;
                box.y = y;
                box.width = std::max(1, boxW);
                box.height = std::max(1, boxH);
                box.canvas = canvasIndex;
                ops.push_back(box);
                const auto along = [](int value, int from, int to) {
                    if (from <= 0 || to == from) {
                        return value;
                    }
                    return static_cast<int>((static_cast<long long>(value) * to + from / 2) / from);
                };
                for (const RoboCanvasCommand& command : drawn.commands) {
                    if (command.kind != RoboCanvasKind::Text || command.value.empty()) {
                        continue;
                    }
                    PaintOp label;
                    label.text = true;
                    label.x = x + along(command.x, drawn.width, box.width);
                    label.y = y + along(command.y, drawn.height, box.height);
                    label.size = std::max(1, along(command.size, drawn.height, box.height));
                    label.face = FaceOf(command.family);
                    label.color = command.color == 0 ? 0xFF000000u : command.color;
                    label.value = command.value;
                    label.wrap = 0;
                    label.overflow = 0;
                    label.breakLong = false;
                    label.width = std::max(1, MeasureText(label.value, label.size, label.face));
                    label.wrapWidth = label.width;
                    label.height = label.size;
                    ops.push_back(label);
                }
                return box.height;
            }
    }
    if (name == "image") {
        int boxH = AttrLength(node, "height");
        if (boxH <= 0) {
            boxH = AttrLength(node, "width");
        }
        if (boxH <= 0) {
            boxH = Px(120);
        }
        int boxW = width;
        const int askedW = AttrLength(node, "width");
        if (askedW > 0) {
            boxW = std::min(width, askedW);
        }
        PaintOp box;
        box.x = x;
        box.y = y;
        box.width = boxW;
        box.height = boxH;
        box.radius = AttrLength(node, "radius");
        const std::string fillName = RoboXMLAttr(node, "background");
        const std::string source = RoboXMLAttr(node, "source").empty() ? RoboXMLAttr(node, "src") : RoboXMLAttr(node, "source");
        if (!source.empty() && !gDocumentDir.empty()) {
            std::filesystem::path file(source);
            if (file.is_relative()) {
                file = gDocumentDir / file;
            }
            box.picture = PictureFor(file);
        }
        if (!fillName.empty()) {
            box.fill = ThemeColor(fillName.c_str(), light);
        } else if (box.picture < 0) {
            box.fill = ThemeColor("bg-secondary", light);
        }
        std::string fit = Lower(RoboXMLAttr(node, "fit"));
        if (fit.empty()) {
            fit = Lower(RoboXMLAttr(node, "objectfit"));
        }
        if (fit.empty()) {
            fit = "contain";
        }
        box.pictureFit = fit;
        ops.push_back(box);
        const std::string caption = TrimCopy(RoboXMLAttr(node, "alt"));
        if (box.picture < 0 && !caption.empty()) {
            const TypeStyle style = StyleOf(node);
            PaintOp op;
            op.text = true;
            op.select = TextSelectMode(node, true);
            op.x = x + Px(8);
            op.y = y + std::max(0, (boxH - style.size) / 2);
            op.size = style.size;
            op.face = style.face;
            op.color = RoboFoundationPlatformColorForTheme(style.color, light ? ROBO_TRUE : ROBO_FALSE);
            op.value = caption;
            op.height = style.size;
            ops.push_back(op);
        }
        return boxH;
    }
    if (name == "view3d" || name == "scene3d") {
        int boxH = AttrLength(node, "height");
        if (boxH <= 0) {
            boxH = 160;
        }
        PaintOp box;
        box.x = x;
        box.y = y;
        box.width = width;
        box.height = boxH;
        box.radius = AttrLength(node, "radius");
        if (box.radius <= 0) {
            box.radius = 12;
        }
        box.fill = ThemeColor("bg-secondary", light);
        ops.push_back(box);
        PaintOp op;
        op.text = true;
        op.select = TextSelectMode(node, true);
        op.x = x + 16;
        op.y = y + 16;
        op.width = std::max(1, width - 32);
        op.height = 28;
        op.size = 16;
        op.face = 1;
        op.color = ThemeColor("text-primary", light);
        const std::string source = RoboXMLAttr(node, "source");
        op.value = source.empty() ? "Robix3D" : source;
        ops.push_back(op);
        return boxH;
    }
    if (Named(name, face, "anchor")) {
        std::string label = TextCopy(node);
        const std::string sizeName = Lower(RoboXMLAttr(node, "size", "inherit"));
        int size = 14;
        if (sizeName == "sm" || sizeName == "small") size = 12;
        else if (sizeName == "md" || sizeName == "medium" || sizeName == "lg" || sizeName == "large") size = 17;
        const std::string variant = Lower(RoboXMLAttr(node, "variant", "inline"));
        const int weight = (variant == "arrow" || variant == "standalone" || variant == "more" || !RoboXMLAttr(node, "arrow").empty()) ? 1 : 0;
        std::string color = "text-link";
        if (Lower(RoboXMLAttr(node, "disabled")) == "true") color = "text-disabled";
        else if (variant == "subtle" && RoboXMLAttr(node, "arrow").empty()) color = "text-tertiary";
        else if (variant == "plain" && RoboXMLAttr(node, "arrow").empty()) color = "text-primary";
        else if (Lower(RoboXMLAttr(node, "tone")) == "danger") color = "danger";
        const int height = std::max(size + 4, (size * 14) / 10);
        PaintOp op;
        op.text = true;
        op.x = x;
        op.y = y;
        op.width = width;
        op.height = height;
        op.size = size;
        op.face = weight;
        op.color = RoboFoundationPlatformColorForTheme(color.c_str(), light ? ROBO_TRUE : ROBO_FALSE);
        op.value = label;
        ops.push_back(op);
        return height;
    }
    if (IsText(name)) {
        const TypeStyle style = StyleOf(node);
        std::string value = TextCopy(node);
        const bool placeholder = value.empty();
        if (placeholder) {
            value = TrimCopy(RoboXMLAttr(node, "placeholder"));
        }
        const std::string role = Lower(RoboXMLAttr(node, "role"));
        const std::string as = Lower(RoboXMLAttr(node, "as"));
        const bool field = role == "textbox" || role == "searchbox" || as == "input" || as == "textarea";
        int fieldH = 0;
        if (field) {
            fieldH = AttrLength(node, "height");
            if (fieldH <= 0) {
                fieldH = as == "textarea" ? Px(72) : Px(28);
            }
            const int radius = Px(6);
            PaintOp edge;
            edge.x = x;
            edge.y = y;
            edge.width = width;
            edge.height = fieldH;
            edge.radius = radius;
            edge.fill = ThemeColor("border-default", light);
            ops.push_back(edge);
            PaintOp well;
            well.x = x + 1;
            well.y = y + 1;
            well.width = std::max(1, width - 2);
            well.height = std::max(1, fieldH - 2);
            well.radius = std::max(0, radius - 1);
            well.fill = ThemeColor(light ? "bg-primary" : "bg-elevated", light);
            ops.push_back(well);
        }
        int used = 0;
        if (!value.empty()) {
            const std::string alignName = Lower(RoboXMLAttr(node, "textalign").empty() ? RoboXMLAttr(node, "align") : RoboXMLAttr(node, "textalign"));
            PaintOp op;
            op.text = true;
            op.select = field ? 0 : TextSelectMode(node, true);
            op.role = "Text";
            op.x = field ? x + Px(8) : x;
            op.y = field ? y + std::max(0, (fieldH - style.size) / 2) : y;
            op.width = field ? std::max(1, width - Px(16)) : width;
            op.size = style.size;
            op.face = style.face;
            op.align = (alignName == "center" || alignName == "middle") ? 1 : (alignName == "end" || alignName == "right") ? 2 : alignName == "justify" ? 3 : 0;
            const TextFlow flow = FlowOf(node);
            op.wrap = flow.mode;
            op.maxLines = flow.maxLines;
            op.overflow = flow.overflow;
            op.whiteSpace = flow.whiteSpace;
            op.breakLong = flow.breakLong;
            const std::string colorName = RoboXMLAttr(node, "color");
            const char* fallback = placeholder ? "text-tertiary" : style.color;
            op.color = RoboFoundationPlatformColorForTheme(colorName.empty() ? fallback : colorName.c_str(), light ? ROBO_TRUE : ROBO_FALSE);
            op.value = value;
            op.height = TextBlockHeight(value, op.width, style.size, style.face, style.leading, flow);
            ops.push_back(op);
            used = field ? fieldH : op.height;
        }
        for (const RoboXMLNode& child : node.children) {
            used += PlaceNode(child, x, y + used, width, light, ops, hits);
        }
        return field ? std::max(used, fieldH) : used;
    }
    if (IsIcon(node)) {
        const int box = IconBox(node);
        PaintOp op;
        op.drawing = true;
        op.role = "Icon";
        op.x = x;
        op.y = y;
        op.width = box;
        op.height = box;
        op.color = RoboFoundationPlatformColorForTheme("text-primary", light ? ROBO_TRUE : ROBO_FALSE);
        op.drawingSvg = IconDrawing(node);
        ops.push_back(op);
        return box;
    }
    if (Named(name, face, "button") || (Lower(RoboXMLAttr(node, "role")) == "button" && !Named(name, face, "iconbutton"))) {
        const int height = ButtonHeight(node);
        const int control = hits == nullptr ? -1 : hits->next;
        const bool underPointer = hits != nullptr && hits->pointerX >= x && hits->pointerX < x + width &&
                                  hits->pointerY >= y && hits->pointerY < y + height && !hits->pointerTaken;
        const bool hot = underPointer || (hits != nullptr && hits->pointerX < 0 && control == hits->hot);
        const bool pressed = hits != nullptr && control == hits->pressed;
        if (underPointer && hits != nullptr) {
            hits->pointerTaken = true;
        }
        PaintOp op;
        op.button = true;
        op.role = "Button";
        op.control = control;
        op.x = x;
        op.y = y;
        op.width = width;
        op.height = height;
        int radius = AttrLength(node, "radius");
        const std::string shape = Lower(RoboXMLAttr(node, "shape"));
        if (radius <= 0) {
            radius = (shape == "pill" || shape == "capsule" || shape == "circle") ? height / 2 : Px(6);
        }
        op.radius = radius;
        StyleButton(node, light, hot, pressed, op);
        op.loading = ButtonOn(node, "loading");
        op.value = ButtonLabel(node);
        ResolveButtonIcons(node, op.leadingSvg, op.trailingSvg);
        ops.push_back(op);
        if (hits != nullptr) {
            if (hits->controls != nullptr) {
                const bool enabled = !ButtonOn(node, "disabled") && !ButtonOn(node, "loading");
                hits->controls->push_back(WindowControl{x, y, width, height, enabled});
            }
            ++hits->next;
        }
        return height;
    }
    if (Named(name, face, "iconbutton")) {
        const int box = IconButtonExtent(node);
        const int control = hits == nullptr ? -1 : hits->next;
        const bool underPointer = hits != nullptr && hits->pointerX >= x && hits->pointerX < x + box &&
                                  hits->pointerY >= y && hits->pointerY < y + box && !hits->pointerTaken;
        const bool hot = underPointer || (hits != nullptr && hits->pointerX < 0 && control == hits->hot);
        const bool pressed = hits != nullptr && control == hits->pressed;
        if (underPointer && hits != nullptr) {
            hits->pointerTaken = true;
        }
        std::string status = Lower(RoboXMLAttr(node, "status"));
        if (status.empty() && ButtonOn(node, "loading")) {
            status = "loading";
        }
        std::string iconName = RoboXMLAttr(node, "icon");
        if (iconName.empty()) {
            iconName = RoboXMLAttr(node, "name");
        }
        std::string glyph = RoboWebSvg(gAssets, iconName);
        for (const RoboXMLNode& child : node.children) {
            const std::string childName = Lower(child.name);
            if (childName != "icon" && childName != "svg") {
                continue;
            }
            const std::string markup = RoboWebIconMarkup(gAssets, child);
            if (!markup.empty()) {
                glyph = markup;
                break;
            }
        }
        if (status == "loading") {
            glyph = ButtonSpinner();
        } else if (status == "success") {
            glyph = IconButtonMark(true);
        } else if (status == "error") {
            glyph = IconButtonMark(false);
        }
        PaintOp op;
        op.button = true;
        op.role = "Button";
        op.iconButton = true;
        op.control = control;
        op.x = x;
        op.y = y;
        op.width = box;
        op.height = box;
        int radius = AttrLength(node, "radius");
        const std::string shape = Lower(RoboXMLAttr(node, "shape"));
        if (radius <= 0) {
            radius = (shape == "circle" || shape == "pill") ? box / 2 : Px(6);
        }
        op.radius = radius;
        std::string variant = Lower(RoboXMLAttr(node, "variant", "ghost"));
        op.kind = variant == "subtle" ? 11 : variant == "outline" ? 12 : variant == "primary" ? 13 : variant == "danger" ? 14 : 10;
        op.preview = status == "loading" ? 1 : status == "success" ? 2 : status == "error" ? 3 : 0;
        op.quiet = ButtonOn(node, "disabled") || status == "loading";
        op.loading = status == "loading";
        StyleIconButton(op, light, hot, pressed, false);
        op.leadingSvg = glyph;
        ops.push_back(op);
        const std::string badge = RoboXMLAttr(node, "badge");
        if (!badge.empty() && badge != "0") {
            PaintOp dot;
            dot.x = x + box - Px(16);
            dot.y = y + Px(2);
            dot.width = Px(18);
            dot.height = Px(18);
            dot.radius = Px(9);
            dot.fill = ThemeColor("danger", light);
            ops.push_back(dot);
            PaintOp count;
            count.text = true;
            count.value = badge;
            count.x = dot.x + Px(4);
            count.y = dot.y + Px(4);
            count.width = Px(12);
            count.height = Px(12);
            count.size = std::max(1, Px(10));
            count.face = 1;
            count.color = ThemeColor("text-on-brand", light);
            ops.push_back(count);
        }
        if (hits != nullptr) {
            if (hits->controls != nullptr) {
                const bool enabled = !ButtonOn(node, "disabled") && status != "loading";
                hits->controls->push_back(WindowControl{x, y, box, box, enabled});
            }
            ++hits->next;
        }
        return box;
    }
    if (Named(name, face, "switch") || Lower(RoboXMLAttr(node, "role")) == "switch") {
        int trackW = 0;
        int trackH = 0;
        int thumb = 0;
        SwitchMetrics(node, trackW, trackH, thumb);
        PaintSwitchControl(ops, node, x, y, light);
        return trackH;
    }
    if (Named(name, face, "switchitem")) {
        const std::string title = RoboXMLAttr(node, "title");
        const std::string description = RoboXMLAttr(node, "description");
        const int titleSize = std::max(1, Px(17));
        const int descSize = std::max(1, Px(12));
        const int titleH = title.empty() ? 0 : titleSize;
        const int descH = description.empty() ? 0 : descSize;
        const int block = titleH + (descH > 0 ? Px(2) + descH : 0);
        const int height = std::max(Px(56), block + Px(24));
        int trackW = 0;
        int trackH = 0;
        int thumb = 0;
        SwitchMetrics(node, trackW, trackH, thumb);
        if (titleH > 0) {
            PaintOp label;
            label.text = true;
            label.select = TextSelectMode(node, true);
            label.value = title;
            label.x = x;
            label.y = y + (height - block) / 2;
            label.width = std::max(0, width - trackW - Px(16));
            label.height = titleH;
            label.size = titleSize;
            label.face = 0;
            label.color = ThemeColor("text-primary", light);
            ops.push_back(label);
        }
        if (descH > 0) {
            PaintOp copy;
            copy.text = true;
            copy.select = TextSelectMode(node, true);
            copy.value = description;
            copy.x = x;
            copy.y = y + (height - block) / 2 + titleH + Px(2);
            copy.width = std::max(0, width - trackW - Px(16));
            copy.height = descH;
            copy.size = descSize;
            copy.face = 0;
            copy.color = ThemeColor("text-tertiary", light);
            ops.push_back(copy);
        }
        PaintSwitchControl(ops, node, x + std::max(0, width - trackW), y + std::max(0, (height - trackH) / 2), light);
        return height;
    }
    if (Named(name, face, "switchgroup")) {
        int cursorY = y;
        bool first = true;
        for (const RoboXMLNode& child : node.children) {
            if (!first) {
                PaintOp line;
                line.x = x;
                line.y = cursorY;
                line.width = width;
                line.height = std::max(1, Px(1));
                line.fill = ThemeColor("border-default", light);
                ops.push_back(line);
            }
            cursorY += PlaceNode(child, x, cursorY, width, light, ops, hits);
            first = false;
        }
        return std::max(0, cursorY - y);
    }
    if (Named(name, face, "segmentedcontrol")) {
        int used = width;
        if (!ButtonOn(node, "fullWidth")) {
            used = std::min(width, SegmentNaturalWidth(node));
        }
        return PaintSegmented(ops, node, x, y, std::max(1, used), light);
    }
    if (Named(name, face, "segmentedtoolbar")) {
        const int gap = Px(8);
        int cursorX = x;
        int lineHeight = 0;
        int cursorY = y;
        int bottom = y;
        for (const RoboXMLNode& child : node.children) {
            const int childWidth = std::min(ContentWidth(child, std::max(1, width - (cursorX - x))), std::max(1, width));
            if (cursorX > x && cursorX + childWidth > x + width) {
                cursorX = x;
                cursorY += lineHeight + gap;
                lineHeight = 0;
            }
            const int childHeight = PlaceNode(child, cursorX, cursorY, std::max(1, childWidth), light, ops, hits);
            lineHeight = std::max(lineHeight, childHeight);
            bottom = std::max(bottom, cursorY + childHeight);
            cursorX += childWidth + gap;
        }
        return std::max(0, bottom - y);
    }
    const int cap = AttrLength(node, "maxwidth");
    if (cap > 0 && cap < width) {
        width = cap;
    }
    const std::string minRaw = RoboXMLAttr(node, "minwidth");
    if (!minRaw.empty()) {
        const int floor = MeasureLength(minRaw, width > 0 ? width : 1);
        if (floor > width) {
            width = floor;
        }
    }
    BoxEdge pad = BoxEdges(node, "padding", "paddingx", "paddingy", "paddingtop", "paddingright", "paddingbottom", "paddingleft", width);
    if (RoboXMLAttr(node, "paddingx").empty() && !RoboXMLAttr(node, "paddinghorizontal").empty()) {
        const int axis = MeasureLength(RoboXMLAttr(node, "paddinghorizontal"), width);
        if (RoboXMLAttr(node, "paddingleft").empty()) pad.left = axis;
        if (RoboXMLAttr(node, "paddingright").empty()) pad.right = axis;
    }
    if (RoboXMLAttr(node, "paddingy").empty() && !RoboXMLAttr(node, "paddingvertical").empty()) {
        const int axis = MeasureLength(RoboXMLAttr(node, "paddingvertical"), width);
        if (RoboXMLAttr(node, "paddingtop").empty()) pad.top = axis;
        if (RoboXMLAttr(node, "paddingbottom").empty()) pad.bottom = axis;
    }
    int inner = std::max(0, width - pad.left - pad.right);
    if (name == "scrollview") {
        const int contentWide = AttrLength(node, "contentwidth");
        if (contentWide > inner) {
            inner = contentWide;
        }
    }
    const int originX = x + pad.left;
    const int originY = y + pad.top;
    int childSlot = -1;
    {
        const std::string raw = RoboXMLAttr(node, "height");
        const std::string folded = Lower(raw);
        if (!raw.empty() && folded != "auto") {
            const int basis = slotHeight > 0 ? slotHeight : 0;
            int asked = 0;
            if (folded == "full" || folded == "fill" || (!raw.empty() && raw.back() == '%')) {
                asked = basis > 0 ? MeasureLength(raw, basis) : 0;
            } else {
                asked = LengthOf(raw);
            }
            if (asked > 0) {
                childSlot = std::max(0, asked - pad.top - pad.bottom);
            }
        }
    }
    const std::string display = Lower(RoboXMLAttr(node, "display"));
    const std::string direction = Lower(RoboXMLAttr(node, "direction"));
    const bool stack = display == "stack" || name == "stack";
    const bool grid = !stack && (name == "grid" || display == "grid" || !RoboXMLAttr(node, "columns").empty());
    const bool axisRow = !stack && !grid && (name == "row" || display == "horizontal" || direction == "row" || direction == "row-reverse");
    const bool reverse = direction.find("reverse") != std::string::npos || Lower(RoboXMLAttr(node, "reverse")) == "true";
    std::string wrapName = Lower(RoboXMLAttr(node, "wrap"));
    if (wrapName.empty()) {
        wrapName = Lower(RoboXMLAttr(node, "flexwrap"));
    }
    const std::string overflowAsked = Lower(RoboXMLAttr(node, "overflow"));
    const bool wrapReverse = wrapName == "wrap-reverse" || wrapName == "wrapreverse";
    const bool wrap = !stack && !grid && (wrapName == "wrap" || wrapName == "true" || wrapName == "yes" || wrapReverse || overflowAsked == "wrap");
    const bool row = axisRow || wrap;
    int columns = 1;
    const std::string columnText = RoboXMLAttr(node, "columns");
    if (IsDigits(columnText)) {
        columns = std::max(1, std::stoi(columnText));
    }
    std::string alignRaw = RoboXMLAttr(node, "align");
    if (alignRaw.empty()) {
        alignRaw = RoboXMLAttr(node, "alignitems");
    }
    std::string justifyRaw = RoboXMLAttr(node, "justify");
    if (justifyRaw.empty()) {
        justifyRaw = RoboXMLAttr(node, "justifycontent");
    }
    const std::string place = Lower(RoboXMLAttr(node, "place"));
    if (!place.empty()) {
        const bool top = place.rfind("top", 0) == 0;
        const bool bottom = place.rfind("bottom", 0) == 0;
        const bool left = place.find("left") != std::string::npos || place == "left";
        const bool right = place.find("right") != std::string::npos || place == "right";
        if (alignRaw.empty()) {
            alignRaw = row ? (top ? "start" : bottom ? "end" : "center") : (left ? "start" : right ? "end" : "center");
        }
        if (justifyRaw.empty()) {
            justifyRaw = row ? (left ? "start" : right ? "end" : "center") : (top ? "start" : bottom ? "end" : "center");
        }
    }
    const int align = alignRaw.empty() ? 3 : PackAlign(alignRaw);
    const int justify = PackAlign(justifyRaw);
    std::string itemRaw = RoboXMLAttr(node, "justifyitems");
    const int justifyItems = itemRaw.empty() ? 3 : PackAlign(itemRaw);
    const int rowGap = RoboXMLAttr(node, "rowgap").empty() ? Gap(node) : AttrLength(node, "rowgap");
    const int colGap = !RoboXMLAttr(node, "columngap").empty() ? AttrLength(node, "columngap")
                       : !RoboXMLAttr(node, "colgap").empty()  ? AttrLength(node, "colgap")
                                                               : Gap(node);
    const std::size_t background = ops.size();
    bool hasBackground = false;
    std::string fillName = RoboXMLAttr(node, "background");
    if (fillName.empty()) {
        fillName = RoboXMLAttr(node, "backgroundcolor");
    }
    if (fillName.empty()) {
        fillName = RoboXMLAttr(node, "bg");
    }
    if (!fillName.empty()) {
        PaintOp box;
        box.role = node.name;
        box.x = x;
        box.y = y;
        box.width = width;
        box.radius = AttrLength(node, "radius");
        box.fill = ThemeColor(fillName.c_str(), light);
        if (box.fill != 0) {
            ops.push_back(box);
            hasBackground = true;
        }
    }
    const std::size_t childBegin = ops.size();
    struct Slot {
        std::size_t opBegin = 0;
        std::size_t opEnd = 0;
        int controlBegin = 0;
        int controlEnd = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int across = 0;
    };
    std::vector<Slot> slots;
    std::vector<const RoboXMLNode*> flow;
    std::vector<const RoboXMLNode*> layered;
    for (const RoboXMLNode& child : node.children) {
        const std::string position = Lower(RoboXMLAttr(child, "position"));
        if (position == "absolute" || position == "fixed" || position == "abs") {
            layered.push_back(&child);
        } else {
            flow.push_back(&child);
        }
    }
    std::stable_sort(flow.begin(), flow.end(), [](const RoboXMLNode* left, const RoboXMLNode* right) {
        const auto number = [](const RoboXMLNode* item) {
            const std::string text = RoboXMLAttr(*item, "order");
            return !text.empty() && IsDigits(text) ? std::stoi(text) : 0;
        };
        return number(left) < number(right);
    });
    if (reverse) {
        std::reverse(flow.begin(), flow.end());
    }
    const auto controlCount = [&]() { return hits != nullptr && hits->controls != nullptr ? static_cast<int>(hits->controls->size()) : 0; };
    const auto selfAlign = [&](const RoboXMLNode& child) {
        std::string raw = RoboXMLAttr(child, "alignself");
        if (raw.empty()) {
            raw = RoboXMLAttr(child, "self");
        }
        if (raw.empty() || Lower(raw) == "auto") {
            return align;
        }
        return PackAlign(raw);
    };
    const auto columnSpan = [&](const RoboXMLNode& child) {
        std::string raw = RoboXMLAttr(child, "columnspan");
        if (raw.empty()) {
            raw = RoboXMLAttr(child, "colspan");
        }
        if (raw.empty()) {
            const std::string gridColumn = Lower(RoboXMLAttr(child, "gridcolumn"));
            if (gridColumn.rfind("span", 0) == 0) {
                raw.clear();
                for (char ch : gridColumn) {
                    if (ch >= '0' && ch <= '9') {
                        raw.push_back(ch);
                    }
                }
            }
        }
        int span = raw.empty() || !IsDigits(raw) ? 1 : std::stoi(raw);
        if (span < 1) {
            span = 1;
        }
        return std::min(span, columns);
    };
    const auto placeChild = [&](const RoboXMLNode& child, int childX, int childY, int childWidth) {
        const std::size_t begin = ops.size();
        const int controls = controlCount();
        const int childHeight = PlaceNode(child, childX, childY, std::max(1, childWidth), light, ops, hits, childSlot);
        slots.push_back(Slot{begin, ops.size(), controls, controlCount(), childX, childY, childWidth, childHeight, selfAlign(child)});
        return childHeight;
    };
    int contentBottom = originY;
    if (stack) {
        int bottom = originY;
        for (const RoboXMLNode* child : flow) {
            int used = AskedLength(*child, "width", inner);
            if (used < 0) {
                used = std::max(1, ContentWidth(*child, inner));
            }
            const int childHeight = placeChild(*child, originX, originY, std::max(1, used));
            bottom = std::max(bottom, originY + childHeight);
        }
        contentBottom = bottom;
    } else if (grid) {
        const int cell = std::max(1, (inner - (columns - 1) * colGap) / std::max(1, columns));
        int column = 0;
        int rowTop = originY;
        int rowHeight = 0;
        std::vector<int> rowSlots;
        const auto finishRow = [&](bool more) {
            for (int index : rowSlots) {
                Slot& slot = slots[static_cast<std::size_t>(index)];
                int dy = 0;
                if (slot.across == 1) {
                    dy = std::max(0, (rowHeight - slot.height) / 2);
                } else if (slot.across == 2) {
                    dy = std::max(0, rowHeight - slot.height);
                }
                if (dy != 0) {
                    ShiftRange(ops, slot.opBegin, slot.opEnd, hits, slot.controlBegin, slot.controlEnd, 0, dy);
                }
            }
            rowSlots.clear();
            rowTop += rowHeight;
            if (more) {
                rowTop += rowGap;
            }
            rowHeight = 0;
            column = 0;
        };
        for (const RoboXMLNode* child : flow) {
            const BoxEdge margin = BoxEdges(*child, "margin", "marginx", "marginy", "margintop", "marginright", "marginbottom", "marginleft", inner);
            int span = columnSpan(*child);
            if (column > 0 && column + span > columns) {
                finishRow(true);
            }
            if (span > columns) {
                span = columns;
            }
            const int cellX = originX + column * (cell + colGap);
            const int cellWidth = cell * span + colGap * std::max(0, span - 1);
            int used = std::max(1, cellWidth - margin.left - margin.right);
            const int asked = AskedLength(*child, "width", used);
            const int across = justifyItems;
            if (asked >= 0) {
                used = std::min(used, asked);
            } else if (across != 3) {
                used = std::min(used, ContentWidth(*child, used));
            }
            int dx = margin.left;
            if (across == 1) {
                dx += std::max(0, (cellWidth - margin.left - margin.right - used) / 2);
            } else if (across == 2) {
                dx += std::max(0, cellWidth - margin.left - margin.right - used);
            }
            const int childHeight = placeChild(*child, cellX + dx, rowTop + margin.top, used);
            rowHeight = std::max(rowHeight, childHeight + margin.top + margin.bottom);
            rowSlots.push_back(static_cast<int>(slots.size()) - 1);
            column += span;
            if (column >= columns) {
                finishRow(true);
            }
        }
        if (!rowSlots.empty()) {
            finishRow(false);
        } else if (!flow.empty()) {
            rowTop -= rowGap;
        }
        contentBottom = rowTop;
    } else if (row) {
        int cursorX = originX;
        int lineTop = originY;
        int lineHeight = 0;
        int lineUsed = 0;
        int lineCount = 0;
        std::vector<int> line;
        const auto finishLine = [&](bool more) {
            int free = inner - lineUsed;
            if (lineCount > 1) {
                free -= colGap * (lineCount - 1);
            }
            if (free < 0) {
                free = 0;
            }
            int lead = 0;
            int extra = 0;
            if (justify == 1) {
                lead = free / 2;
            } else if (justify == 2) {
                lead = free;
            } else if (justify == 4 && lineCount > 1) {
                extra = free / (lineCount - 1);
            } else if (justify == 5 && lineCount > 0) {
                lead = free / (lineCount * 2);
                extra = free / lineCount;
            } else if (justify == 6 && lineCount > 0) {
                lead = free / (lineCount + 1);
                extra = lead;
            }
            int shift = lead;
            for (int index : line) {
                Slot& slot = slots[static_cast<std::size_t>(index)];
                int dy = 0;
                if (slot.across == 1) {
                    dy = std::max(0, (lineHeight - slot.height) / 2);
                } else if (slot.across == 2) {
                    dy = std::max(0, lineHeight - slot.height);
                }
                ShiftRange(ops, slot.opBegin, slot.opEnd, hits, slot.controlBegin, slot.controlEnd, shift, dy);
                shift += extra;
            }
            line.clear();
            lineTop += lineHeight;
            if (more) {
                lineTop += rowGap;
            }
            lineHeight = 0;
            lineUsed = 0;
            lineCount = 0;
            cursorX = originX;
        };
        std::vector<int> rowWidths;
        if (!wrap) {
            const auto flexNumber = [](const RoboXMLNode& item, const char* key, const char* alias) {
                std::string text = RoboXMLAttr(item, key);
                if (text.empty()) {
                    text = RoboXMLAttr(item, alias);
                }
                return !text.empty() && IsDigits(text) ? std::stoi(text) : 0;
            };
            int usedSum = 0;
            int growSum = 0;
            int shrinkSum = 0;
            int count = 0;
            for (const RoboXMLNode* child : flow) {
                const BoxEdge margin = BoxEdges(*child, "margin", "marginx", "marginy", "margintop", "marginright", "marginbottom", "marginleft", inner);
                int used = ContentWidth(*child, std::max(1, inner - margin.left - margin.right));
                const int asked = AskedLength(*child, "width", inner);
                if (asked >= 0) {
                    used = std::min(inner, asked);
                }
                rowWidths.push_back(std::max(1, used));
                usedSum += used + margin.left + margin.right;
                growSum += flexNumber(*child, "grow", "flex");
                shrinkSum += flexNumber(*child, "shrink", "flexshrink");
                ++count;
            }
            if (count > 1) {
                usedSum += colGap * (count - 1);
            }
            const int free = inner - usedSum;
            if (free > 0 && growSum > 0) {
                int left = free;
                int growLeft = growSum;
                int index = 0;
                for (const RoboXMLNode* child : flow) {
                    const int grow = flexNumber(*child, "grow", "flex");
                    if (grow > 0) {
                        const int share = growLeft == grow ? left : left * grow / growLeft;
                        left -= share;
                        growLeft -= grow;
                        rowWidths[static_cast<std::size_t>(index)] += share;
                    }
                    ++index;
                }
            } else if (free < 0 && shrinkSum > 0) {
                int left = -free;
                int shrinkLeft = shrinkSum;
                int index = 0;
                for (const RoboXMLNode* child : flow) {
                    const int shrink = flexNumber(*child, "shrink", "flexshrink");
                    if (shrink > 0) {
                        int share = shrinkLeft == shrink ? left : left * shrink / shrinkLeft;
                        int& width = rowWidths[static_cast<std::size_t>(index)];
                        if (share > width - 1) {
                            share = width - 1;
                        }
                        left -= share;
                        shrinkLeft -= shrink;
                        width -= share;
                    }
                    ++index;
                }
            }
        }
        int rowIndex = 0;
        for (const RoboXMLNode* child : flow) {
            const BoxEdge margin = BoxEdges(*child, "margin", "marginx", "marginy", "margintop", "marginright", "marginbottom", "marginleft", inner);
            int used = ContentWidth(*child, std::max(1, inner - margin.left - margin.right));
            const int asked = AskedLength(*child, "width", inner);
            if (!rowWidths.empty()) {
                used = rowWidths[static_cast<std::size_t>(rowIndex)];
            } else if (asked >= 0) {
                used = std::min(inner, asked);
            }
            ++rowIndex;
            const int outer = used + margin.left + margin.right;
            if (wrap && lineCount > 0 && cursorX + outer > originX + inner) {
                finishLine(true);
            }
            const int childHeight = placeChild(*child, cursorX + margin.left, lineTop + margin.top, std::max(1, used));
            lineHeight = std::max(lineHeight, childHeight + margin.top + margin.bottom);
            lineUsed += outer;
            ++lineCount;
            line.push_back(static_cast<int>(slots.size()) - 1);
            cursorX += outer + colGap;
        }
        if (!line.empty()) {
            finishLine(false);
        }
        contentBottom = lineTop;
    } else {
        int cursorY = originY;
        for (const RoboXMLNode* child : flow) {
            const BoxEdge margin = BoxEdges(*child, "margin", "marginx", "marginy", "margintop", "marginright", "marginbottom", "marginleft", inner);
            int used = std::max(0, inner - margin.left - margin.right);
            const int asked = AskedLength(*child, "width", used);
            const int across = selfAlign(*child);
            const bool hug = Lower(RoboXMLAttr(*child, "size")) == "content";
            const bool textChild = IsText(Lower(child->name));
            if (asked >= 0) {
                used = std::min(used, asked);
            } else if (textChild) {
                used = std::max(0, inner - margin.left - margin.right);
            } else if (across != 3 || hug) {
                used = std::min(used, ContentWidth(*child, used));
            }
            int dx = margin.left;
            if (across == 1) {
                dx += std::max(0, (inner - margin.left - margin.right - used) / 2);
            } else if (across == 2) {
                dx += std::max(0, inner - margin.left - margin.right - used);
            }
            const int childHeight = placeChild(*child, originX + dx, cursorY + margin.top, std::max(1, used));
            cursorY += childHeight + margin.top + margin.bottom + rowGap;
        }
        if (!flow.empty() && rowGap > 0) {
            cursorY -= rowGap;
        }
        contentBottom = cursorY;
    }
    int total = std::max(0, contentBottom - y + pad.bottom);
    const int heightBasis = slotHeight > 0 ? slotHeight : std::max(total, 1);
    const int askedHeight = AskedLength(node, "height", heightBasis);
    const int minHeight = AttrLength(node, "minheight");
    int boxHeight = total;
    if (askedHeight > boxHeight) {
        boxHeight = askedHeight;
    }
    if (minHeight > boxHeight) {
        boxHeight = minHeight;
    }
    if (askedHeight <= 0 && width > 0) {
        const std::string ratio = RoboXMLAttr(node, "aspectratio");
        int num = 0;
        int den = 1;
        bool split = false;
        std::string left;
        std::string right;
        for (char ch : ratio) {
            if (ch == ':' || ch == '/') {
                split = true;
                continue;
            }
            if (!split) {
                left.push_back(ch);
            } else {
                right.push_back(ch);
            }
        }
        if (!left.empty() && IsDigits(left)) {
            num = std::stoi(left);
            if (split && !right.empty() && IsDigits(right)) {
                den = std::stoi(right);
            }
            if (num > 0 && den > 0) {
                const int aspectHeight = (width * den + num / 2) / num;
                if (aspectHeight > boxHeight) {
                    boxHeight = aspectHeight;
                }
            }
        }
    }
    if (!grid && !row && justify != 0 && justify != 3 && boxHeight > total && !slots.empty()) {
        const int free = boxHeight - total;
        const int count = static_cast<int>(slots.size());
        int lead = 0;
        int extra = 0;
        if (justify == 1) {
            lead = free / 2;
        } else if (justify == 2) {
            lead = free;
        } else if (justify == 4 && count > 1) {
            extra = free / (count - 1);
        } else if (justify == 5 && count > 0) {
            lead = free / (count * 2);
            extra = free / count;
        } else if (justify == 6 && count > 0) {
            lead = free / (count + 1);
            extra = lead;
        }
        int shift = lead;
        for (Slot& slot : slots) {
            ShiftRange(ops, slot.opBegin, slot.opEnd, hits, slot.controlBegin, slot.controlEnd, 0, shift);
            shift += extra;
        }
    }
    if (row && align != 3 && boxHeight > total && !slots.empty()) {
        const int free = boxHeight - total;
        int dy = 0;
        if (align == 1) {
            dy = free / 2;
        } else if (align == 2) {
            dy = free;
        }
        if (dy != 0) {
            for (Slot& slot : slots) {
                ShiftRange(ops, slot.opBegin, slot.opEnd, hits, slot.controlBegin, slot.controlEnd, 0, dy);
                slot.y += dy;
            }
        }
    }
    const int innerHeight = std::max(0, boxHeight - pad.top - pad.bottom);
    for (const RoboXMLNode* child : layered) {
        int used = AskedLength(*child, "width", inner);
        if (used < 0) {
            used = ContentWidth(*child, inner);
        }
        const bool hasLeft = !RoboXMLAttr(*child, "left").empty();
        const bool hasRight = !RoboXMLAttr(*child, "right").empty();
        const bool hasTop = !RoboXMLAttr(*child, "top").empty();
        const bool hasBottom = !RoboXMLAttr(*child, "bottom").empty();
        int left = originX;
        int top = originY;
        if (hasLeft) {
            left = originX + MeasureLength(RoboXMLAttr(*child, "left"), inner);
        } else if (!RoboXMLAttr(*child, "x").empty()) {
            left = originX + MeasureLength(RoboXMLAttr(*child, "x"), inner);
        }
        if (hasRight && !hasLeft) {
            left = originX + inner - MeasureLength(RoboXMLAttr(*child, "right"), inner) - used;
        }
        if (hasTop) {
            top = originY + MeasureLength(RoboXMLAttr(*child, "top"), innerHeight);
        } else if (!RoboXMLAttr(*child, "y").empty()) {
            top = originY + MeasureLength(RoboXMLAttr(*child, "y"), innerHeight);
        }
        const std::size_t begin = ops.size();
        const int controls = controlCount();
        const int childHeight = PlaceNode(*child, left, top, std::max(1, used), light, ops, hits);
        if (hasBottom && !hasTop) {
            const int target = originY + innerHeight - MeasureLength(RoboXMLAttr(*child, "bottom"), innerHeight) - childHeight;
            ShiftRange(ops, begin, ops.size(), hits, controls, controlCount(), 0, target - top);
        }
    }
    if (hasBackground && background < ops.size()) {
        ops[background].height = boxHeight;
    }
    const std::string overflowName = Lower(RoboXMLAttr(node, "overflow"));
    std::string overflowX = Lower(RoboXMLAttr(node, "overflowx"));
    if (overflowX.empty()) {
        overflowX = Lower(RoboXMLAttr(node, "overflow-x"));
    }
    std::string overflowY = Lower(RoboXMLAttr(node, "overflowy"));
    if (overflowY.empty()) {
        overflowY = Lower(RoboXMLAttr(node, "overflow-y"));
    }
    if (overflowX.empty()) {
        overflowX = overflowName;
    }
    if (overflowY.empty()) {
        overflowY = overflowName;
    }
    if (name == "scrollview") {
        std::string axis = Lower(RoboXMLAttr(node, "axis"));
        if (axis.empty()) {
            axis = "vertical";
        }
        if (overflowX.empty() && overflowY.empty()) {
            if (axis == "horizontal") {
                overflowX = "scroll";
                overflowY = "hidden";
            } else if (axis == "both") {
                overflowX = "scroll";
                overflowY = "scroll";
            } else {
                overflowX = "hidden";
                overflowY = "scroll";
            }
        }
    }
    const bool scrolls = overflowX == "scroll" || overflowX == "auto" || overflowY == "scroll" || overflowY == "auto";
    const std::string clipFlag = Lower(RoboXMLAttr(node, "clip"));
    const bool clipChildren = !scrolls && (overflowName == "clip" || overflowName == "hidden" || clipFlag == "true" ||
                                            clipFlag == "1" || clipFlag == "yes");
    const bool root = x == 0 && y == 0 && (name == "screen" || name == "window" || name == "page");
    if (scrolls && !root && askedHeight > 0 && boxHeight > askedHeight) {
        boxHeight = askedHeight;
        if (hasBackground && background < ops.size()) {
            ops[background].height = boxHeight;
        }
    }
    if (scrolls && !root) {
        int right = x;
        int bottom = y;
        for (std::size_t index = childBegin; index < ops.size(); ++index) {
            const PaintOp& op = ops[index];
            right = std::max(right, op.x + op.width);
            bottom = std::max(bottom, op.y + op.height);
        }
        if (right > x + width || bottom > y + boxHeight) {
            ScrollRegion region;
            region.x = x;
            region.y = y;
            region.width = width;
            region.height = boxHeight;
            region.contentWidth = std::max(width, right - x);
            region.contentHeight = std::max(boxHeight, bottom - y);
            region.opBegin = static_cast<int>(childBegin);
            region.opEnd = static_cast<int>(ops.size());
            if (name == "scrollview") {
                const std::string axis = Lower(RoboXMLAttr(node, "axis").empty() ? "vertical" : RoboXMLAttr(node, "axis"));
                if (axis == "vertical") {
                    region.contentWidth = region.width;
                } else if (axis == "horizontal") {
                    region.contentHeight = region.height;
                }
                const auto whole = [](const std::string& text, int& number) {
                    if (text.empty() || !IsDigits(text)) {
                        return false;
                    }
                    number = std::stoi(text);
                    return true;
                };
                int shared = 0;
                int offsetX = 0;
                int offsetY = 0;
                whole(RoboXMLAttr(node, "offsetx"), offsetX);
                whole(RoboXMLAttr(node, "offsety"), offsetY);
                if (whole(RoboXMLAttr(node, "offset"), shared)) {
                    if (axis == "horizontal") {
                        offsetX = shared;
                    } else if (axis != "both" || RoboXMLAttr(node, "offsety").empty()) {
                        offsetY = shared;
                    }
                }
                region.scrollX = std::max(0, std::min(offsetX, std::max(0, region.contentWidth - region.width)));
                region.scrollY = std::max(0, std::min(offsetY, std::max(0, region.contentHeight - region.height)));
                region.paintedX = region.scrollX;
                region.paintedY = region.scrollY;
            }
            gRegions.push_back(region);
        }
    }
    if (clipChildren) {
        const int right = x + width;
        const int bottom = y + boxHeight;
        for (std::size_t index = childBegin; index < ops.size(); ++index) {
            PaintOp& op = ops[index];
            const int opRight = op.x + op.width;
            const int opBottom = op.y + op.height;
            if (op.x >= right || op.y >= bottom || opRight <= x || opBottom <= y) {
                op.width = 0;
                op.height = 0;
                op.value.clear();
                continue;
            }
            if (op.x < x) {
                op.width -= x - op.x;
                op.x = x;
            }
            if (op.y < y) {
                op.height -= y - op.y;
                op.y = y;
            }
            if (op.x + op.width > right) {
                op.width = right - op.x;
            }
            if (op.y + op.height > bottom) {
                op.height = bottom - op.y;
            }
        }
    }
    return boxHeight;
}

int Isqrt(int value) {
    if (value <= 0) {
        return 0;
    }
    int root = value;
    int next = (root + 1) / 2;
    while (next < root) {
        root = next;
        next = (root + value / root) / 2;
    }
    return root;
}

bool InPill(int localX, int localY, int width, int height) {
    if (localX < 0 || localY < 0 || localX >= width || localY >= height || height < 2) {
        return false;
    }
    const int radius = height / 2;
    const int centerY = localY - radius;
    const int distanceY = centerY < 0 ? -centerY : centerY;
    if (distanceY > radius) {
        return false;
    }
    const int span = Isqrt(radius * radius - distanceY * distanceY);
    const int inset = radius - span;
    return localX >= inset && localX < width - inset;
}

RoboU8 PillCoverage(int localX, int localY, int width, int height) {
    if (localX < 0 || localY < 0 || localX >= width || localY >= height || height < 2) {
        return 0;
    }
    const int width4 = width * 4;
    const int radius = height * 2;
    int maxInset = 0;
    int minInset = radius;
    unsigned insideRows = 0;
    for (int sampleY = 0; sampleY < 4; ++sampleY) {
        const int y = localY * 4 + sampleY;
        const int dy = y - radius;
        const int ady = dy < 0 ? -dy : dy;
        if (ady > radius) {
            continue;
        }
        const int span = Isqrt(radius * radius - ady * ady);
        const int inset = radius - span;
        if (inset > maxInset) {
            maxInset = inset;
        }
        if (inset < minInset) {
            minInset = inset;
        }
        ++insideRows;
    }
    if (insideRows == 0) {
        return 0;
    }
    const int x0 = localX * 4;
    if (insideRows == 4 && x0 >= maxInset && x0 + 3 < width4 - maxInset) {
        return 255;
    }
    if (x0 + 3 < minInset || x0 >= width4 - minInset) {
        return 0;
    }
    unsigned hits = 0;
    for (int sampleY = 0; sampleY < 4; ++sampleY) {
        for (int sampleX = 0; sampleX < 4; ++sampleX) {
            if (InPill(x0 + sampleX, localY * 4 + sampleY, width4, height * 4)) {
                ++hits;
            }
        }
    }
    return static_cast<RoboU8>((hits * 255u) / 16u);
}

void FillPill(RoboGPU* gpu, int x, int y, int width, int height, RoboU32 color, int coverage) {
    if (gpu == nullptr || width <= 0 || height <= 0 || coverage <= 0) {
        return;
    }
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const unsigned edge = PillCoverage(col, row, width, height);
            if (edge == 0) {
                continue;
            }
            RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>(edge * static_cast<unsigned>(coverage) / 255u));
        }
    }
}

void StrokePill(RoboGPU* gpu, int x, int y, int width, int height, int thickness, RoboU32 color, int coverage) {
    if (gpu == nullptr || thickness <= 0 || coverage <= 0) {
        return;
    }
    const int outerW = width + thickness * 2;
    const int outerH = height + thickness * 2;
    for (int row = 0; row < outerH; ++row) {
        for (int col = 0; col < outerW; ++col) {
            const unsigned outer = PillCoverage(col, row, outerW, outerH);
            const unsigned inner = PillCoverage(col - thickness, row - thickness, width, height);
            const unsigned edge = outer > inner ? outer - inner : 0;
            if (edge == 0) {
                continue;
            }
            RoboGPUBlendPixel(gpu, x - thickness + col, y - thickness + row, color,
                              static_cast<RoboU8>(edge * static_cast<unsigned>(coverage) / 255u));
        }
    }
}

void GlowPill(RoboGPU* gpu, int x, int y, int width, int height, RoboU32 color, int fade) {
    const int reach = std::max(3, Px(5));
    const int gapQ = Px(1) * 4;
    const int reachQ = reach * 4;
    const int span = reachQ - gapQ;
    if (span <= 0) {
        return;
    }
    const int widthQ = width * 4;
    const int heightQ = height * 4;
    const int radius = heightQ / 2;
    const int straight = widthQ - heightQ;
    for (int row = -reach; row < height + reach; ++row) {
        for (int col = -reach; col < width + reach; ++col) {
            unsigned sum = 0;
            for (int sampleY = 0; sampleY < 4; ++sampleY) {
                for (int sampleX = 0; sampleX < 4; ++sampleX) {
                    int localX = col * 4 + sampleX - radius;
                    const int localY = row * 4 + sampleY - radius;
                    if (localX < 0) {
                    } else if (straight > 0 && localX > straight) {
                        localX -= straight;
                    } else {
                        localX = 0;
                    }
                    const int outside = Isqrt(localX * localX + localY * localY) - radius;
                    if (outside < gapQ || outside >= reachQ) {
                        continue;
                    }
                    const int remain = reachQ - outside;
                    sum += static_cast<unsigned>(remain * remain * 128 / (span * span));
                }
            }
            if (sum == 0) {
                continue;
            }
            RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>(sum * static_cast<unsigned>(fade) / (16u * 255u)));
        }
    }
}

int gSpinX = 1024;
int gSpinY = 0;
int gSpinEndX = -193;
int gSpinEndY = 1006;

void RotateSpin(int& x, int& y) {
    const int cosine = 974;
    const int sine = 316;
    const int nextX = (x * cosine - y * sine) / 1024;
    const int nextY = (x * sine + y * cosine) / 1024;
    const int length = Isqrt(nextX * nextX + nextY * nextY);
    if (length <= 0) {
        x = 1024;
        y = 0;
        return;
    }
    x = nextX * 1024 / length;
    y = nextY * 1024 / length;
}

bool SpinArc(int quarterX, int quarterY) {
    const int fromStart = gSpinX * quarterY - gSpinY * quarterX;
    const int toEnd = quarterX * gSpinEndY - quarterY * gSpinEndX;
    return fromStart >= 0 && toEnd >= 0;
}

void DrawSpin(RoboGPU* gpu, int centerX, int centerY, RoboU32 track, int trackCoverage, RoboU32 tick, int tickCoverage,
              int sizePx) {
    const int size = sizePx > 0 ? sizePx : 16;
    const int radiusQ = 7 * size / 4;
    int halfQ = size / 8;
    if (halfQ < 1) {
        halfQ = 1;
    }
    const int outer = radiusQ + halfQ;
    const int inner = radiusQ > halfQ ? radiusQ - halfQ : 1;
    const int outer2 = outer * outer;
    const int inner2 = inner * inner;
    const int capR2 = halfQ * halfQ;
    const int capX = gSpinX * radiusQ / 1024;
    const int capY = gSpinY * radiusQ / 1024;
    const int capEndX = gSpinEndX * radiusQ / 1024;
    const int capEndY = gSpinEndY * radiusQ / 1024;
    const int extent = size / 2 + 1;
    if (trackCoverage < 0) {
        trackCoverage = 0;
    }
    if (tickCoverage < 0) {
        tickCoverage = 0;
    }
    if (trackCoverage > 255) {
        trackCoverage = 255;
    }
    if (tickCoverage > 255) {
        tickCoverage = 255;
    }
    for (int row = -extent; row <= extent; ++row) {
        for (int col = -extent; col <= extent; ++col) {
            unsigned trackHits = 0;
            unsigned tickHits = 0;
            for (int sampleY = 0; sampleY < 4; ++sampleY) {
                for (int sampleX = 0; sampleX < 4; ++sampleX) {
                    const int quarterX = col * 4 + sampleX - 2;
                    const int quarterY = row * 4 + sampleY - 2;
                    const int distance = quarterX * quarterX + quarterY * quarterY;
                    const bool ring = distance >= inner2 && distance <= outer2;
                    const int startCap = (quarterX - capX) * (quarterX - capX) + (quarterY - capY) * (quarterY - capY);
                    const int endCap = (quarterX - capEndX) * (quarterX - capEndX) + (quarterY - capEndY) * (quarterY - capEndY);
                    const bool cap = startCap <= capR2 || endCap <= capR2;
                    if (!ring && !cap) {
                        continue;
                    }
                    if ((ring && SpinArc(quarterX, quarterY)) || cap) {
                        ++tickHits;
                    } else if (ring) {
                        ++trackHits;
                    }
                }
            }
            if (trackHits != 0) {
                RoboGPUBlendPixel(gpu, centerX + col, centerY + row, track,
                                  static_cast<RoboU8>(trackCoverage * static_cast<int>(trackHits) / 16));
            }
            if (tickHits != 0) {
                RoboGPUBlendPixel(gpu, centerX + col, centerY + row, tick,
                                  static_cast<RoboU8>(tickCoverage * static_cast<int>(tickHits) / 16));
            }
        }
    }
}

int CenteredTextTop(int top, int boxHeight, int fontSize) {
    if (fontSize <= 0) {
        return top;
    }
    const int mid = top + boxHeight / 2;
    const int baseline = mid + fontSize * 745 / 2048;
    return baseline - fontSize;
}

void PaintSpin(RoboGPU* gpu, int centerX, int centerY, int size, RoboU32 ink, int fade, bool light) {
    const RoboU32 tick = ink != 0 ? ink : ThemeColor("brand-primary", light);
    int cover = fade > 0 ? fade : 255;
    DrawSpin(gpu, centerX, centerY, tick, cover * 35 / 100, tick, cover, size);
}

void TextBounds(int& runA, int& offA, int& runB, int& offB);
void MarkBox(int face, int size, int lineY, int& top, int& height);
int TextAnchorRun();

RoboU32 FieldMix(RoboU32 fromColor, RoboU32 toColor, int part) {
    const auto channel = [&](int shift) {
        const int a = static_cast<int>((fromColor >> shift) & 255u);
        const int b = static_cast<int>((toColor >> shift) & 255u);
        return a + (b - a) * part / 100;
    };
    return 0xFF000000u | (static_cast<RoboU32>(channel(16)) << 16) | (static_cast<RoboU32>(channel(8)) << 8) |
           static_cast<RoboU32>(channel(0));
}

void FieldInkBox(int face, int size, int lineY, int& top, int& height) {
    MarkBox(face, size, lineY, top, height);
    const int pad = std::max(1, size / 8);
    top += pad;
    height = std::max(1, height - pad * 2);
}

int FieldCaretThick(int size, int before, int after) {
    int thick = std::max(1, (size + 12) / 18);
    const int neighbor = before > 0 && after > 0 ? std::min(before, after) : std::max(before, after);
    if (neighbor > 0) {
        thick = std::min(thick, std::max(1, neighbor / 4));
    }
    return std::min(thick, 4);
}

void PaintFieldCaret(RoboGPU* gpu, int x, int y, int thick, int height, RoboU32 color, int cover) {
    if (gpu == nullptr || cover <= 0 || thick <= 0 || height <= 0) {
        return;
    }
    const int radius = thick / 2;
    x -= thick / 2;
    for (int row = 0; row < height; ++row) {
        int dy = 0;
        if (radius > 0 && row < radius) {
            dy = radius - row;
        } else if (radius > 0 && row >= height - radius) {
            dy = row - (height - 1 - radius);
        }
        for (int col = 0; col < thick; ++col) {
            if (radius > 0) {
                const int dx = col < radius ? radius - col : col - radius;
                if (dx * dx + dy * dy > radius * radius) {
                    continue;
                }
            }
            RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>(cover));
        }
    }
}
int TextFocusRun();
bool SceneLight();
RoboU32 SceneBackground();

void PaintTextMark(RoboGPU* gpu, int run, const PaintOp& op, int lineX, int lineY, const TextLine& line, int lineIndex, int lineCount) {
    if (gpu == nullptr || op.select == 0 || TextAnchorRun() < 0 || TextFocusRun() < 0 || line.text.empty()) {
        return;
    }
    int runA = 0;
    int offA = 0;
    int runB = 0;
    int offB = 0;
    TextBounds(runA, offA, runB, offB);
    if (run < runA || run > runB || (runA == runB && offA == offB)) {
        return;
    }
    int from = 0;
    int to = static_cast<int>(op.value.size());
    if (run == runA) {
        from = offA;
    }
    if (run == runB) {
        to = offB;
    }
    const int local0 = std::max(0, from - line.origin);
    const int local1 = std::min(static_cast<int>(line.text.size()), to - line.origin);
    if (local1 <= local0) {
        return;
    }
    const int x0 = lineX + MeasureText(line.text.substr(0, static_cast<std::size_t>(local0)), op.size, op.face);
    int width = std::max(1, MeasureText(line.text.substr(static_cast<std::size_t>(local0), static_cast<std::size_t>(local1 - local0)), op.size, op.face));
    if (local0 == 0 && local1 >= static_cast<int>(line.text.size()) && lineIndex + 1 < lineCount) {
        const int edge = op.x + op.width;
        if (edge > x0) {
            width = edge - x0;
        }
    }
    int top = lineY;
    int markHeight = std::max(1, op.size);
    MarkBox(op.face, op.size, lineY, top, markHeight);
    const RoboU32 brand = ThemeColor("brand-primary", SceneLight());
    const RoboU32 back = SceneBackground();
    const auto mix = [](RoboU32 fromColor, RoboU32 toColor, int part) {
        const auto channel = [&](int shift) {
            const int a = static_cast<int>((fromColor >> shift) & 255u);
            const int b = static_cast<int>((toColor >> shift) & 255u);
            return a + (b - a) * part / 100;
        };
        return 0xFF000000u | (static_cast<RoboU32>(channel(16)) << 16) | (static_cast<RoboU32>(channel(8)) << 8) |
               static_cast<RoboU32>(channel(0));
    };
    const int radius = std::max(2, op.size / 8);
    FillRound(gpu, x0, top, width, markHeight, radius, mix(back, brand, 45));
}


void PaintOne(RoboGPU* gpu, const PaintOp& op, int scrollY, int viewHeight, bool light, int run) {
    const int y = op.y - scrollY;
    if (y + op.height < 0 || y > viewHeight) {
        return;
    }
    if (op.button) {
            if (op.fill != 0) {
                int cover = op.fade;
                const unsigned alpha = (op.fill >> 24) & 255u;
                if (alpha > 0 && alpha < 255) {
                    cover = cover * static_cast<int>(alpha) / 255;
                }
                FillPill(gpu, op.x, y, op.width, op.height, op.fill, cover);
            }
            if (op.borderPx > 0 && op.border != 0) {
                StrokePill(gpu, op.x, y, op.width, op.height, op.borderPx, op.border, op.fade);
            }
        } else if (!op.text && op.fill != 0) {
            FillRound(gpu, op.x, y, op.width, op.height, op.radius, op.fill);
        }
        if (op.picture >= 0) {
            PaintPicture(gpu, op, y);
        }
        if (op.canvas >= 0) {
            PaintCanvas(gpu, op, y);
        }
        if (op.drawing && op.loading) {
            const int size = std::max(8, std::min(op.width, op.height));
            PaintSpin(gpu, op.x + op.width / 2, y + op.height / 2, size, op.color, op.fade, light);
            return;
        }
        if (op.drawing && !op.drawingSvg.empty()) {
            PaintWindowsSvg(gpu, op.x, y, op.width, op.drawingSvg, op.color);
            return;
        }
        if (op.value.empty() && op.leadingSvg.empty() && op.trailingSvg.empty()) {
            return;
        }
        RoboFontHeader header{};
        const RoboFontView font = FaceFont(op.face, op.size, header);
        int textX = op.x;
        int textY = y;
        if (op.button) {
            const int icon = Px(16);
            const int gap = Px(8);
            const int measured = op.value.empty() ? 0 : MeasureText(op.value, op.size, op.face);
            int content = measured;
            if (!op.leadingSvg.empty()) {
                content += icon + gap;
            }
            if (!op.trailingSvg.empty()) {
                content += (content > 0 ? gap : 0) + icon;
            }
            int cursor = op.x + std::max(0, (op.width - content) / 2);
            const int mid = y + op.height / 2;
            if (op.loading && op.leadingSvg.empty() && op.value.empty()) {
                const int spin = std::max(8, std::min(op.width, op.height) - 4);
                PaintSpin(gpu, op.x + op.width / 2, mid, spin, op.color, op.fade, light);
            } else if (!op.leadingSvg.empty() || op.loading) {
                if (op.loading) {
                    PaintSpin(gpu, cursor + icon / 2, mid, icon, op.color, op.fade, light);
                    cursor += icon + gap;
                } else {
                    PaintWindowsSvg(gpu, cursor, mid - icon / 2, icon, op.leadingSvg, op.color);
                    cursor += icon + gap;
                }
            }
            textX = cursor;
            textY = CenteredTextTop(y, op.height, op.size);
            if (!op.trailingSvg.empty()) {
                PaintWindowsSvg(gpu, cursor + measured + gap, mid - icon / 2, icon, op.trailingSvg, op.color);
            }
        } else if (!op.text) {
            const int measured = MeasureText(op.value, op.size, op.face);
            textX = op.x + std::max(0, (op.width - measured) / 2);
            textY = CenteredTextTop(y, op.height, op.size);
        }
        if (op.value.empty() && op.editor >= 0 && op.editor < static_cast<int>(gEditors.size())) {
            const Editor& editor = gEditors[static_cast<std::size_t>(op.editor)];
            if (editor.focused && !editor.disabled && gFieldCaretCover > 0) {
                int top = y;
                int ink = std::max(1, op.size);
                FieldInkBox(op.face, op.size, y, top, ink);
                PaintFieldCaret(gpu, op.x, top, FieldCaretThick(op.size, op.size, op.size), ink, editor.caretColor, gFieldCaretCover);
            }
        }
        if (!op.value.empty()) {
            if (op.text) {
                std::vector<TextLine> lines;
                TextFlow flow;
                flow.mode = op.wrap;
                flow.maxLines = op.maxLines;
                flow.overflow = op.overflow;
                flow.whiteSpace = op.whiteSpace;
                flow.breakLong = op.breakLong;
                const int wrapWidth = op.wrapWidth > 0 ? op.wrapWidth : op.width;
                const int wrapSize = op.wrapSize > 0 ? op.wrapSize : op.size;
                TextLines(op.value, wrapWidth, wrapSize, op.face, flow, lines);
                const int step = std::max(1, op.height / static_cast<int>(std::max<std::size_t>(1, lines.size())));
                int lineY = y;
                const auto paintWidth = [&](int measured) {
                    if (op.wrapSize > 0 && op.size > 0 && op.wrapSize != op.size) {
                        return std::max(1, (measured * op.size + op.wrapSize / 2) / op.wrapSize);
                    }
                    return measured;
                };
                for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
                    const TextLine& line = lines[lineIndex];
                    const int drawn = paintWidth(line.width);
                    int lineX = textX;
                    if (op.align == 1) {
                        lineX = op.x + std::max(0, (op.width - drawn) / 2);
                    } else if (op.align == 2) {
                        lineX = op.x + std::max(0, op.width - drawn);
                    }
                    const bool justify = op.align == 3 && lineIndex + 1 < lines.size() && line.width < op.width &&
                                         line.text.find(' ') != std::string::npos;
                    if (op.editor >= 0 && op.editor < static_cast<int>(gEditors.size())) {
                        const Editor& editor = gEditors[static_cast<std::size_t>(op.editor)];
                        const int from = std::min(editor.selA, editor.selB);
                        const int to = std::max(editor.selA, editor.selB);
                        const int local0 = std::max(0, from - line.origin);
                        const int local1 = std::min(static_cast<int>(line.text.size()), to - line.origin);
                        if (local1 > local0 && !editor.value.empty()) {
                            const int x0 = lineX + MeasureText(line.text.substr(0, static_cast<std::size_t>(local0)), op.size, op.face);
                            const int x1 = lineX + MeasureText(line.text.substr(0, static_cast<std::size_t>(local1)), op.size, op.face);
                            int top = lineY;
                            int markHeight = std::max(1, op.size);
                            MarkBox(op.face, op.size, lineY, top, markHeight);
                            const int radius = std::max(2, op.size / 8);
                            FillRound(gpu, x0, top, std::max(1, x1 - x0), markHeight, radius, FieldMix(SceneBackground(), ThemeColor("brand-primary", light), 45));
                        }
                    }
                    PaintTextMark(gpu, run, op, lineX, lineY, line, static_cast<int>(lineIndex), static_cast<int>(lines.size()));
                    if (!line.text.empty() && justify) {
                        int gaps = 0;
                        for (char ch : line.text) {
                            if (ch == ' ') {
                                ++gaps;
                            }
                        }
                        const int extra = gaps > 0 ? (op.width - line.width) / gaps : 0;
                        int cursor = textX;
                        std::string word;
                        const auto drawWord = [&]() {
                            if (word.empty()) {
                                return;
                            }
                            RoboTextDrawUTF8(gpu, font, cursor, lineY, word.c_str(), op.color, static_cast<RoboU16>(op.size));
                            cursor += MeasureText(word, op.size, op.face);
                            word.clear();
                        };
                        for (char ch : line.text) {
                            if (ch == ' ') {
                                drawWord();
                                cursor += MeasureText(" ", op.size, op.face) + extra;
                            } else {
                                word.push_back(ch);
                            }
                        }
                        drawWord();
                    } else if (!line.text.empty()) {
                        RoboTextDrawUTF8(gpu, font, lineX, lineY, line.text.c_str(), op.color, static_cast<RoboU16>(op.size));
                    }
                    if (op.editor >= 0 && op.editor < static_cast<int>(gEditors.size())) {
                        const Editor& editor = gEditors[static_cast<std::size_t>(op.editor)];
                        if (editor.focused && !editor.disabled && gFieldCaretCover > 0 && editor.caret >= line.origin &&
                            editor.caret <= line.origin + static_cast<int>(line.text.size()) &&
                            (lineIndex + 1 == lines.size() || editor.caret < line.origin + static_cast<int>(line.text.size()))) {
                            const int local = std::max(0, editor.caret - line.origin);
                            const int caretX = lineX + MeasureText(line.text.substr(0, static_cast<std::size_t>(local)), op.size, op.face);
                            int beforeStart = local;
                            if (beforeStart > 0) {
                                --beforeStart;
                                while (beforeStart > 0 && (static_cast<unsigned char>(line.text[static_cast<std::size_t>(beforeStart)]) & 0xC0u) == 0x80u) {
                                    --beforeStart;
                                }
                            }
                            const int before = local > beforeStart ? MeasureText(line.text.substr(static_cast<std::size_t>(beforeStart), static_cast<std::size_t>(local - beforeStart)), op.size, op.face) : 0;
                            const int afterSpan = local < static_cast<int>(line.text.size()) ? Utf8Span(line.text, static_cast<std::size_t>(local)) : 0;
                            const int after = afterSpan > 0 ? MeasureText(line.text.substr(static_cast<std::size_t>(local), static_cast<std::size_t>(afterSpan)), op.size, op.face) : 0;
                            int top = lineY;
                            int ink = std::max(1, op.size);
                            FieldInkBox(op.face, op.size, lineY, top, ink);
                            PaintFieldCaret(gpu, caretX, top, FieldCaretThick(op.size, before, after), ink, editor.caretColor, gFieldCaretCover);
                        }
                    }
                    lineY += step;
                }
            } else {
                RoboTextDrawUTF8(gpu, font, textX, textY, op.value.c_str(), op.color, static_cast<RoboU16>(op.size));
            }
        }
}

int ScrollBarThick() { return std::max(8, Px(8)); }

bool ClipDrawn(PaintOp& op, int left, int top, int right, int bottom) {
    const int opRight = op.x + op.width;
    const int opBottom = op.y + op.height;
    if (op.x >= right || op.y >= bottom || opRight <= left || opBottom <= top) {
        op.width = 0;
        op.height = 0;
        return false;
    }
    if (op.x < left) {
        op.width -= left - op.x;
        op.x = left;
    }
    if (op.y < top) {
        op.height -= top - op.y;
        op.y = top;
    }
    if (op.x + op.width > right) {
        op.width = right - op.x;
    }
    if (op.y + op.height > bottom) {
        op.height = bottom - op.y;
    }
    return op.width > 0 && op.height > 0;
}

bool PrepareDrawn(PaintOp& op, int index) {
    for (int regionIndex = static_cast<int>(gRegions.size()) - 1; regionIndex >= 0; --regionIndex) {
        const ScrollRegion& region = gRegions[static_cast<std::size_t>(regionIndex)];
        if (index < region.opBegin || index >= region.opEnd) {
            continue;
        }
        op.x -= region.scrollX;
        op.y -= region.scrollY;
        int right = region.x + region.width;
        int bottom = region.y + region.height;
        if (region.contentHeight > region.height) {
            right -= ScrollBarThick();
        }
        if (region.contentWidth > region.width) {
            bottom -= ScrollBarThick();
        }
        if (!ClipDrawn(op, region.x, region.y, right, bottom)) {
            return false;
        }
    }
    op.x -= gScrollX;
    return op.width > 0 && op.height > 0;
}

bool PushRegionClip(RoboGPU* gpu, int index, int scrollY, double scale) {
    if (!(scale > 0.0)) {
        scale = 1.0;
    }
    for (int regionIndex = static_cast<int>(gRegions.size()) - 1; regionIndex >= 0; --regionIndex) {
        const ScrollRegion& region = gRegions[static_cast<std::size_t>(regionIndex)];
        if (index < region.opBegin || index >= region.opEnd) {
            continue;
        }
        int spanX = region.width;
        int spanY = region.height;
        if (region.contentHeight > region.height) {
            spanX -= ScrollBarThick();
        }
        if (region.contentWidth > region.width) {
            spanY -= ScrollBarThick();
        }
        if (spanX < 1) {
            spanX = 1;
        }
        if (spanY < 1) {
            spanY = 1;
        }
        const int x = ScaleLen(region.x - gScrollX, scale);
        const int y = ScaleLen(region.y - scrollY, scale);
        const int right = ScaleLen(region.x - gScrollX + spanX, scale);
        const int bottom = ScaleLen(region.y - scrollY + spanY, scale);
        RoboGPUPushScissor(gpu, x, y, std::max(1, right - x), std::max(1, bottom - y));
        return true;
    }
    return false;
}

void PaintPrepared(RoboGPU* gpu, const PaintOp& op, int index, int scrollY, int viewHeight, bool light) {
    PaintOp drawn = op;
    if (!PrepareDrawn(drawn, index)) {
        return;
    }
    const bool clipped = PushRegionClip(gpu, index, scrollY, 1.0);
    PaintOne(gpu, drawn, scrollY, viewHeight, light, index);
    if (clipped) {
        RoboGPUPopScissor(gpu);
    }
}

void PaintRegionBars(RoboGPU* gpu, int scrollY, int viewHeight, double scale) {
    (void)gpu;
    (void)scrollY;
    (void)viewHeight;
    (void)scale;
}

void PaintOps(RoboGPU* gpu, const std::vector<PaintOp>& ops, int scrollY, int viewHeight, bool light) {
    for (std::size_t index = 0; index < ops.size(); ++index) {
        PaintPrepared(gpu, ops[index], static_cast<int>(index), scrollY, viewHeight, light);
    }
    PaintRegionBars(gpu, scrollY, viewHeight, 1.0);
}

struct SceneRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct Scene {
    bool valid = false;
    std::string path;
    std::filesystem::file_time_type stamp{};
    std::uintmax_t bytes = 0;
    bool light = false;
    unsigned dpi = 96;
    int viewWidth = 0;
    int viewHeight = 0;
    int scrollY = 0;
    int hot = -1;
    int pressed = -1;
    int focus = -1;
    int contentHeight = 0;
    int contentWidth = 0;
    int zoomAnchor = 0;
    bool pinchEnabled = true;
    bool zoomEnabled = true;
    RoboU32 background = 0;
    std::vector<PaintOp> ops;
    std::vector<WindowControl> controls;
};

Scene gScene;

int gTextAnchorRun = -1;
int gTextAnchor = 0;
int gTextFocusRun = -1;
int gTextFocus = 0;
int gTextShownA = -1;
int gTextShownB = -1;
int gTextShownOffA = 0;
int gTextShownOffB = 0;

int TextAnchorRun() { return gTextAnchorRun; }
int TextFocusRun() { return gTextFocusRun; }
bool SceneLight() { return gScene.light; }
RoboU32 SceneBackground() { return gScene.background; }

bool TextOrder(int runA, int offA, int runB, int offB) {
    if (runA != runB) {
        return runA < runB;
    }
    return offA < offB;
}

void TextBounds(int& runA, int& offA, int& runB, int& offB) {
    runA = gTextAnchorRun;
    offA = gTextAnchor;
    runB = gTextFocusRun;
    offB = gTextFocus;
    if (!TextOrder(runA, offA, runB, offB)) {
        std::swap(runA, runB);
        std::swap(offA, offB);
    }
}

bool PlaceSelectable(const PaintOp& op, int index, PaintOp& placed) {
    placed = op;
    if (op.select == 0 || !op.text || op.value.empty()) {
        return false;
    }
    for (int regionIndex = static_cast<int>(gRegions.size()) - 1; regionIndex >= 0; --regionIndex) {
        const ScrollRegion& region = gRegions[static_cast<std::size_t>(regionIndex)];
        if (index < region.opBegin || index >= region.opEnd) {
            continue;
        }
        placed.x -= region.scrollX;
        placed.y -= region.scrollY;
    }
    return placed.width > 0 && placed.height > 0;
}

int Utf8Step(const std::string& text, int index, int direction) {
    if (direction >= 0) {
        if (index >= static_cast<int>(text.size())) {
            return index;
        }
        return index + Utf8Span(text, static_cast<std::size_t>(index));
    }
    if (index <= 0) {
        return 0;
    }
    int cursor = 0;
    int previous = 0;
    while (cursor < index && cursor < static_cast<int>(text.size())) {
        previous = cursor;
        cursor += Utf8Span(text, static_cast<std::size_t>(cursor));
    }
    return previous;
}

int CaretInLine(const TextLine& line, int size, int face, int lineX, int docX) {
    int place = line.origin;
    int cursor = lineX;
    int best = line.origin;
    if (docX <= lineX) {
        return line.origin;
    }
    for (std::size_t index = 0; index < line.text.size();) {
        const int span = Utf8Span(line.text, index);
        const int width = MeasureText(line.text.substr(index, static_cast<std::size_t>(span)), size, face);
        if (docX < cursor + width / 2) {
            return place;
        }
        best = place + span;
        cursor += width;
        place += span;
        index += static_cast<std::size_t>(span);
    }
    return best;
}

void TextInk(int face, int size, int& above, int& below);

bool CaretAt(int docX, int docY, int& run, int& offset) {
    run = -1;
    offset = 0;
    for (int index = static_cast<int>(gScene.ops.size()) - 1; index >= 0; --index) {
        PaintOp placed;
        if (!PlaceSelectable(gScene.ops[static_cast<std::size_t>(index)], index, placed)) {
            continue;
        }
        std::vector<TextLine> lines;
        TextFlow flow;
        flow.mode = placed.wrap;
        flow.maxLines = placed.maxLines;
        flow.overflow = placed.overflow;
        flow.whiteSpace = placed.whiteSpace;
        flow.breakLong = placed.breakLong;
        const int wrapWidth = placed.wrapWidth > 0 ? placed.wrapWidth : placed.width;
        const int wrapSize = placed.wrapSize > 0 ? placed.wrapSize : placed.size;
        TextLines(placed.value, wrapWidth, wrapSize, placed.face, flow, lines);
        if (lines.empty()) {
            continue;
        }
        const int count = static_cast<int>(lines.size());
        const int step = std::max(1, placed.height / count);
        int above = 0;
        int below = 0;
        TextInk(placed.face, placed.size, above, below);
        int best = -1;
        int bestDistance = 0;
        for (int lineIndex = 0; lineIndex < count; ++lineIndex) {
            const int lineY = placed.y + lineIndex * step;
            const int slotBottom = lineIndex + 1 == count ? placed.y + placed.height : lineY + step;
            const int inkTop = lineY + placed.size - above;
            const int inkBottom = lineY + placed.size + below;
            const int top = std::min(lineY, inkTop);
            const int bottom = std::max(slotBottom, inkBottom);
            if (docY < top || docY >= bottom) {
                continue;
            }
            const int center = (inkTop + inkBottom) / 2;
            const int distance = docY < center ? center - docY : docY - center;
            if (best < 0 || distance < bestDistance) {
                best = lineIndex;
                bestDistance = distance;
            }
        }
        if (best < 0) {
            continue;
        }
        const TextLine& line = lines[static_cast<std::size_t>(best)];
        int drawn = line.width;
        if (placed.wrapSize > 0 && placed.size > 0 && placed.wrapSize != placed.size) {
            drawn = std::max(1, (line.width * placed.size + placed.wrapSize / 2) / placed.wrapSize);
        }
        int lineX = placed.x;
        if (placed.align == 1) {
            lineX = placed.x + std::max(0, (placed.width - drawn) / 2);
        } else if (placed.align == 2) {
            lineX = placed.x + std::max(0, placed.width - drawn);
        }
        run = index;
        offset = CaretInLine(line, wrapSize, placed.face, lineX, docX);
        return true;
    }
    return false;
}

bool CaretNearest(int docX, int docY, int& run, int& offset) {
    if (CaretAt(docX, docY, run, offset)) {
        return true;
    }
    int bestRun = -1;
    int bestOffset = 0;
    int bestDistance = 0;
    for (int index = static_cast<int>(gScene.ops.size()) - 1; index >= 0; --index) {
        PaintOp placed;
        if (!PlaceSelectable(gScene.ops[static_cast<std::size_t>(index)], index, placed)) {
            continue;
        }
        std::vector<TextLine> lines;
        TextFlow flow;
        flow.mode = placed.wrap;
        flow.maxLines = placed.maxLines;
        flow.overflow = placed.overflow;
        flow.whiteSpace = placed.whiteSpace;
        flow.breakLong = placed.breakLong;
        const int wrapWidth = placed.wrapWidth > 0 ? placed.wrapWidth : placed.width;
        const int wrapSize = placed.wrapSize > 0 ? placed.wrapSize : placed.size;
        TextLines(placed.value, wrapWidth, wrapSize, placed.face, flow, lines);
        const int count = static_cast<int>(lines.size());
        if (count <= 0) {
            continue;
        }
        const int step = std::max(1, placed.height / count);
        for (int lineIndex = 0; lineIndex < count; ++lineIndex) {
            const int lineY = placed.y + lineIndex * step;
            const int center = lineY + step / 2;
            const int distance = docY < center ? center - docY : docY - center;
            if (bestRun >= 0 && distance >= bestDistance) {
                continue;
            }
            const TextLine& line = lines[static_cast<std::size_t>(lineIndex)];
            int lineX = placed.x;
            int drawn = line.width;
            if (placed.align == 1) {
                lineX = placed.x + std::max(0, (placed.width - drawn) / 2);
            } else if (placed.align == 2) {
                lineX = placed.x + std::max(0, placed.width - drawn);
            }
            bestRun = index;
            bestOffset = CaretInLine(line, wrapSize, placed.face, lineX, docX);
            bestDistance = distance;
        }
    }
    if (bestRun < 0) {
        return false;
    }
    run = bestRun;
    offset = bestOffset;
    return true;
}

void WordAround(const std::string& text, int offset, int& from, int& to) {
    from = std::max(0, std::min(offset, static_cast<int>(text.size())));
    to = from;
    auto word = [](unsigned char ch) { return std::isalnum(ch) != 0 || ch == '_' || ch == '\'' || ch >= 0x80; };
    while (from > 0 && word(static_cast<unsigned char>(text[static_cast<std::size_t>(from - 1)]))) {
        --from;
    }
    while (to < static_cast<int>(text.size()) && word(static_cast<unsigned char>(text[static_cast<std::size_t>(to)]))) {
        ++to;
    }
    if (from == to && to < static_cast<int>(text.size())) {
        to += Utf8Span(text, static_cast<std::size_t>(to));
    }
}

void TextInk(int face, int size, int& above, int& below) {
    above = std::max(1, size);
    below = std::max(0, size / 5);
    RoboWindowsFontExtent(face, size, &above, &below);
    if (above <= 0) {
        above = std::max(1, size);
    }
    if (below < 0) {
        below = 0;
    }
}

void MarkBox(int face, int size, int lineY, int& top, int& height) {
    int above = 0;
    int below = 0;
    TextInk(face, size, above, below);
    const int baseline = lineY + std::max(1, size);
    const int pad = std::max(1, size / 8);
    top = baseline - above - pad;
    height = std::max(1, above + below + pad * 2);
}

bool SceneStamp(const std::string& path, std::filesystem::file_time_type& stamp, std::uintmax_t& bytes) {
    std::error_code error;
    stamp = std::filesystem::last_write_time(path, error);
    if (error) {
        return false;
    }
    bytes = std::filesystem::file_size(path, error);
    return !error;
}

bool SceneCurrent(const std::string& path, bool light, unsigned dpi, int viewWidth, int viewHeight) {
    if (!gScene.valid || gScene.path != path || gScene.light != light || gScene.dpi != dpi || gScene.viewWidth != viewWidth ||
        gScene.viewHeight != viewHeight) {
        return false;
    }
    std::filesystem::file_time_type stamp{};
    std::uintmax_t bytes = 0;
    return SceneStamp(path, stamp, bytes) && stamp == gScene.stamp && bytes == gScene.bytes;
}

void PublishControls(int* contentHeight, std::vector<WindowControl>* controls, int viewHeight) {
    if (contentHeight != nullptr) {
        *contentHeight = std::max(viewHeight, gScene.contentHeight);
    }
    if (controls != nullptr) {
        *controls = gScene.controls;
    }
}

SceneRect OpBounds(const PaintOp& op, int scrollY) {
    int shiftX = gScrollX;
    int shiftY = scrollY;
    if (!gScene.ops.empty()) {
        const int index = static_cast<int>(&op - gScene.ops.data());
        if (index >= 0 && index < static_cast<int>(gScene.ops.size())) {
            for (const ScrollRegion& region : gRegions) {
                if (index < region.opBegin || index >= region.opEnd) {
                    continue;
                }
                shiftX += region.scrollX;
                shiftY += region.scrollY;
                break;
            }
        }
    }
    SceneRect bounds{op.x - shiftX, op.y - shiftY, op.width, op.height};
    if (op.button) {
        const int pad = std::max(op.borderPx, Px(14));
        bounds.x -= pad;
        bounds.y -= pad;
        bounds.width += pad * 2;
        bounds.height += pad * 2;
    }
    return bounds;
}

bool BoundsHit(const SceneRect& bounds, const SceneRect& rect) {
    return bounds.width > 0 && bounds.height > 0 && rect.width > 0 && rect.height > 0 && bounds.x < rect.x + rect.width &&
           bounds.x + bounds.width > rect.x && bounds.y < rect.y + rect.height && bounds.y + bounds.height > rect.y;
}

void ClipRect(SceneRect& rect, int viewWidth, int viewHeight) {
    if (rect.x < 0) {
        rect.width += rect.x;
        rect.x = 0;
    }
    if (rect.y < 0) {
        rect.height += rect.y;
        rect.y = 0;
    }
    if (rect.x + rect.width > viewWidth) {
        rect.width = viewWidth - rect.x;
    }
    if (rect.y + rect.height > viewHeight) {
        rect.height = viewHeight - rect.y;
    }
}

void GrowRect(SceneRect& rect, int scrollY, int viewWidth, int viewHeight) {
    for (int pass = 0; pass < 6; ++pass) {
        bool grew = false;
        for (const PaintOp& op : gScene.ops) {
            const SceneRect bounds = OpBounds(op, scrollY);
            if (!BoundsHit(bounds, rect)) {
                continue;
            }
            const int x1 = std::max(rect.x + rect.width, bounds.x + bounds.width);
            const int y1 = std::max(rect.y + rect.height, bounds.y + bounds.height);
            const int x0 = std::min(rect.x, bounds.x);
            const int y0 = std::min(rect.y, bounds.y);
            if (x0 != rect.x || y0 != rect.y || x1 != rect.x + rect.width || y1 != rect.y + rect.height) {
                rect = SceneRect{x0, y0, x1 - x0, y1 - y0};
                grew = true;
            }
        }
        if (!grew) {
            break;
        }
    }
    ClipRect(rect, viewWidth, viewHeight);
}

void ReplayRect(RoboGPU* gpu, const SceneRect& rect, int scrollY, int viewHeight, bool light) {
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }
    RoboGPUFillRect(gpu, rect.x, rect.y, rect.width, rect.height, gScene.background);
    for (std::size_t index = 0; index < gScene.ops.size(); ++index) {
        const PaintOp& op = gScene.ops[index];
        if (BoundsHit(OpBounds(op, scrollY), rect)) {
            PaintPrepared(gpu, op, static_cast<int>(index), scrollY, viewHeight, light);
        }
    }
    PaintRegionBars(gpu, scrollY, viewHeight, 1.0);
}

void ShiftScene(RoboGPU* gpu, int delta, int viewWidth, int viewHeight) {
    if (delta == 0 || viewWidth <= 0 || viewHeight <= 0) {
        return;
    }
    const int distance = delta < 0 ? -delta : delta;
    if (distance >= viewHeight) {
        return;
    }
    if (delta > 0) {
        RoboGPUCopyArea(gpu, 0, 0, 0, delta, viewWidth, viewHeight - delta);
    } else {
        RoboGPUCopyArea(gpu, 0, distance, 0, 0, viewWidth, viewHeight - distance);
    }
    RoboDamageClear(&gpu->damage);
}

bool BuildScene(RoboGPU* gpu, const std::string& path, bool light, int viewWidth, int viewHeight, int scrollY,
                int hotControl, int pressedControl, int pointerX, int pointerY) {
    gCanvases.clear();
    std::vector<Editor> keptEditors = gEditors;
    gEditors.clear();
    gEditorKept = &keptEditors;
    gDocumentDir = std::filesystem::path(path).parent_path();
    const std::string source = ReadText(path);
    RoboXMLDocument document;
    if (source.empty() || !RoboXMLParse(source, document)) {
        gEditorKept = nullptr;
        gScene.valid = false;
        return false;
    }
    RoboUICompose(document.root, path);
    LowerKeys(document.root);
    const RoboXMLNode* screen = FindScreen(document.root);
    if (screen == nullptr) {
        gEditorKept = nullptr;
        gScene.valid = false;
        return false;
    }
    WindowsAudioSync(document.root, gDocumentDir);
    std::vector<WindowControl> controls;
    HitState hits;
    hits.controls = &controls;
    hits.hot = hotControl;
    hits.pressed = pressedControl;
    hits.pointerX = pointerX;
    hits.pointerY = pointerY;
    std::vector<ScrollRegion> previousRegions = gRegions;
    gRegions.clear();
    std::vector<PaintOp> ops;
    int pageWidth = viewWidth;
    const int askedWidth = AskedLength(*screen, "width", std::max(viewWidth, 1));
    if (askedWidth > 0) {
        pageWidth = askedWidth;
    }
    const int height = PlaceNode(*screen, 0, 0, pageWidth, light, ops, &hits, viewHeight);
    gEditorKept = nullptr;
    for (ScrollRegion& region : gRegions) {
        for (const ScrollRegion& previous : previousRegions) {
            if (previous.x != region.x || previous.y != region.y || previous.width != region.width || previous.height != region.height) {
                continue;
            }
            region.scrollX = std::max(0, std::min(previous.scrollX, std::max(0, region.contentWidth - region.width)));
            region.scrollY = std::max(0, std::min(previous.scrollY, std::max(0, region.contentHeight - region.height)));
            break;
        }
    }
    int contentWidth = pageWidth;
    for (std::size_t index = 0; index < ops.size(); ++index) {
        bool nested = false;
        for (const ScrollRegion& region : gRegions) {
            if (static_cast<int>(index) >= region.opBegin && static_cast<int>(index) < region.opEnd) {
                nested = true;
                break;
            }
        }
        if (nested) {
            continue;
        }
        contentWidth = std::max(contentWidth, ops[index].x + ops[index].width);
    }
    for (const ScrollRegion& region : gRegions) {
        contentWidth = std::max(contentWidth, region.x + region.width);
    }
    const int keptAnchorRun = gTextAnchorRun;
    const int keptAnchor = gTextAnchor;
    const int keptFocusRun = gTextFocusRun;
    const int keptFocus = gTextFocus;
    std::string keptAnchorValue;
    std::string keptFocusValue;
    if (keptAnchorRun >= 0 && keptAnchorRun < static_cast<int>(gScene.ops.size()) && gScene.ops[static_cast<std::size_t>(keptAnchorRun)].text) {
        keptAnchorValue = gScene.ops[static_cast<std::size_t>(keptAnchorRun)].value;
    }
    if (keptFocusRun >= 0 && keptFocusRun < static_cast<int>(gScene.ops.size()) && gScene.ops[static_cast<std::size_t>(keptFocusRun)].text) {
        keptFocusValue = gScene.ops[static_cast<std::size_t>(keptFocusRun)].value;
    }
    gScene.ops = std::move(ops);
    gTextAnchorRun = -1;
    gTextFocusRun = -1;
    gTextShownA = -1;
    gTextShownB = -1;
    gTextShownOffA = 0;
    gTextShownOffB = 0;
    const auto sameText = [&](int run, const std::string& value) {
        return run >= 0 && run < static_cast<int>(gScene.ops.size()) && gScene.ops[static_cast<std::size_t>(run)].text &&
               gScene.ops[static_cast<std::size_t>(run)].select != 0 && gScene.ops[static_cast<std::size_t>(run)].value == value;
    };
    if (sameText(keptAnchorRun, keptAnchorValue) && sameText(keptFocusRun, keptFocusValue)) {
        gTextAnchorRun = keptAnchorRun;
        gTextFocusRun = keptFocusRun;
        gTextAnchor = std::max(0, std::min(keptAnchor, static_cast<int>(keptAnchorValue.size())));
        gTextFocus = std::max(0, std::min(keptFocus, static_cast<int>(keptFocusValue.size())));
    }
    gScene.controls = std::move(controls);
    gScene.contentHeight = std::max(height, viewHeight);
    gScene.contentWidth = contentWidth;
    gScene.scrollY = scrollY;
    gScene.hot = hotControl;
    gScene.pressed = pressedControl;
    gScene.focus = -1;
    gScene.path = path;
    gScene.light = light;
    gScene.dpi = gDpi;
    gScene.viewWidth = viewWidth;
    gScene.viewHeight = viewHeight;
    std::string zoomPlace = RoboXMLAttr(*screen, "zoomcontrols");
    if (zoomPlace.empty()) {
        zoomPlace = RoboXMLAttr(*screen, "zoom-controls");
    }
    gScene.zoomAnchor = ViewZoom::Anchor(zoomPlace.c_str());
    const auto flag = [&](std::initializer_list<const char*> keys, bool fallback) {
        for (const char* key : keys) {
            std::string value = RoboXMLAttr(*screen, key);
            if (value.empty()) {
                continue;
            }
            for (char& ch : value) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            if (value == "true" || value == "on" || value == "yes" || value == "1") {
                return true;
            }
            if (value == "false" || value == "off" || value == "no" || value == "0") {
                return false;
            }
        }
        return fallback;
    };
    gScene.pinchEnabled = flag({"pinch", "pinchzoom", "pinch-zoom"}, true);
    gScene.zoomEnabled = flag({"zoom", "viewportzoom", "viewport-zoom"}, true);
    gScene.background = RoboFoundationPlatformColorForTheme("bg-primary", light ? ROBO_TRUE : ROBO_FALSE);
    const std::string pageColor = RoboXMLAttr(*screen, "background");
    if (!pageColor.empty()) {
        const RoboU32 parsed = ThemeColor(pageColor.c_str(), light);
        if (parsed != 0) {
            gScene.background = parsed;
        }
    }
    gScene.valid = SceneStamp(path, gScene.stamp, gScene.bytes);
    if (gpu != nullptr) {
        RoboGPUClear(gpu, gScene.background);
        PaintOps(gpu, gScene.ops, scrollY, viewHeight, light);
    }
    return true;
}

void PaintSceneRect(RoboGPU* gpu, SceneRect rect, int scrollY, int viewHeight, bool light) {
    const bool strip = rect.width > 0 && rect.height > 0 && (rect.width <= 32 || rect.height <= 32);
    if (strip) {
        ClipRect(rect, gScene.viewWidth, viewHeight);
    } else {
        GrowRect(rect, scrollY, gScene.viewWidth, viewHeight);
    }
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }
    if (!strip) {
        const long long area = static_cast<long long>(rect.width) * rect.height;
        const long long full = static_cast<long long>(gScene.viewWidth) * gScene.viewHeight;
        if (full > 0 && area * 5 > full * 3) {
            RoboGPUClear(gpu, gScene.background);
            PaintOps(gpu, gScene.ops, scrollY, viewHeight, light);
            return;
        }
    }
    if (strip) {
        RoboGPUPushScissor(gpu, rect.x, rect.y, rect.width, rect.height);
    }
    ReplayRect(gpu, rect, scrollY, viewHeight, light);
    if (strip) {
        RoboGPUPopScissor(gpu);
    }
}

void FieldVisual(const Editor& editor, int& x, int& y) {
    x = editor.fieldX;
    y = editor.fieldY;
    if (editor.op < 0) {
        return;
    }
    for (const ScrollRegion& region : gRegions) {
        if (editor.op < region.opBegin || editor.op >= region.opEnd) {
            continue;
        }
        x -= region.scrollX;
        y -= region.scrollY;
    }
}

void FieldSync(Editor& editor) {
    if (editor.op < 0 || editor.op >= static_cast<int>(gScene.ops.size())) {
        return;
    }
    PaintOp& op = gScene.ops[static_cast<std::size_t>(editor.op)];
    op.value = EditorShown(editor);
    op.color = editor.value.empty() ? editor.hint : editor.ink;
    if (editor.focused) {
        gFieldCaretEditor = editor.op;
        FieldTouch();
    }
    ++gFieldRevision;
}

int FieldClamp(const Editor& editor, int offset) {
    return std::max(0, std::min(offset, static_cast<int>(editor.value.size())));
}

int FieldShownColumn(const Editor& editor, int byteOffset) {
    if (!editor.secret) {
        return byteOffset;
    }
    int column = 0;
    for (std::size_t index = 0; index < editor.value.size() && static_cast<int>(index) < byteOffset;) {
        index += static_cast<std::size_t>(Utf8Span(editor.value, index));
        ++column;
    }
    return column;
}

int FieldOffsetAt(const Editor& editor, int docX, int docY) {
    if (editor.value.empty() || editor.op < 0 || editor.op >= static_cast<int>(gScene.ops.size())) {
        return 0;
    }
    PaintOp placed = gScene.ops[static_cast<std::size_t>(editor.op)];
    for (const ScrollRegion& region : gRegions) {
        if (editor.op < region.opBegin || editor.op >= region.opEnd) {
            continue;
        }
        placed.x -= region.scrollX;
        placed.y -= region.scrollY;
    }
    const std::string shown = EditorShown(editor);
    std::vector<TextLine> lines;
    TextFlow flow;
    flow.mode = placed.wrap;
    flow.whiteSpace = placed.whiteSpace;
    flow.breakLong = placed.breakLong;
    const int wrapWidth = placed.wrapWidth > 0 ? placed.wrapWidth : placed.width;
    TextLines(shown, wrapWidth, placed.size, placed.face, flow, lines);
    if (lines.empty()) {
        return 0;
    }
    const int count = static_cast<int>(lines.size());
    const int step = std::max(1, placed.height / count);
    int best = 0;
    for (int lineIndex = 0; lineIndex < count; ++lineIndex) {
        const int top = placed.y + lineIndex * step;
        const int bottom = lineIndex + 1 == count ? placed.y + placed.height : top + step;
        if (docY >= top && docY < bottom) {
            best = lineIndex;
            break;
        }
        if (docY >= bottom) {
            best = lineIndex;
        }
    }
    const TextLine& line = lines[static_cast<std::size_t>(best)];
    int byte = line.origin;
    int column = FieldShownColumn(editor, byte);
    int cursor = placed.x;
    while (byte < line.origin + static_cast<int>(line.text.size()) && byte < static_cast<int>(editor.value.size())) {
        const int span = editor.secret ? 1 : Utf8Span(editor.value, static_cast<std::size_t>(byte));
        const int nextColumn = column + (editor.secret ? 1 : span);
        const int next = cursor + MeasureText(shown.substr(static_cast<std::size_t>(column), static_cast<std::size_t>(nextColumn - column)), placed.size, placed.face);
        if (docX < cursor + (next - cursor) / 2) {
            break;
        }
        cursor = next;
        column = nextColumn;
        byte += editor.secret ? Utf8Span(editor.value, static_cast<std::size_t>(byte)) : span;
    }
    return FieldClamp(editor, byte);
}

int FieldAtPoint(int docX, int docY) {
    for (int index = static_cast<int>(gEditors.size()) - 1; index >= 0; --index) {
        const Editor& editor = gEditors[static_cast<std::size_t>(index)];
        int x = 0;
        int y = 0;
        FieldVisual(editor, x, y);
        if (docX >= x && docY >= y && docX < x + editor.fieldW && docY < y + editor.fieldH) {
            return index;
        }
    }
    return -1;
}

bool FieldLines(const Editor& editor, PaintOp& placed, std::vector<TextLine>& lines) {
    if (editor.op < 0 || editor.op >= static_cast<int>(gScene.ops.size())) {
        return false;
    }
    placed = gScene.ops[static_cast<std::size_t>(editor.op)];
    for (const ScrollRegion& region : gRegions) {
        if (editor.op < region.opBegin || editor.op >= region.opEnd) {
            continue;
        }
        placed.x -= region.scrollX;
        placed.y -= region.scrollY;
    }
    TextFlow flow;
    flow.mode = placed.wrap;
    flow.whiteSpace = placed.whiteSpace;
    flow.breakLong = placed.breakLong;
    const int wrapWidth = placed.wrapWidth > 0 ? placed.wrapWidth : placed.width;
    TextLines(editor.value, wrapWidth, placed.size, placed.face, flow, lines);
    return true;
}

int FieldLineOf(const std::vector<TextLine>& lines, int offset) {
    int best = 0;
    for (int index = 0; index < static_cast<int>(lines.size()); ++index) {
        if (lines[static_cast<std::size_t>(index)].origin <= offset) {
            best = index;
        }
    }
    return best;
}

void FieldLineSpan(const std::vector<TextLine>& lines, int line, int& from, int& to) {
    if (lines.empty()) {
        from = 0;
        to = 0;
        return;
    }
    const int index = std::max(0, std::min(line, static_cast<int>(lines.size()) - 1));
    from = lines[static_cast<std::size_t>(index)].origin;
    if (index + 1 < static_cast<int>(lines.size())) {
        to = lines[static_cast<std::size_t>(index + 1)].origin;
    } else {
        to = from + static_cast<int>(lines[static_cast<std::size_t>(index)].text.size());
    }
}

int FieldHitLine(const Editor& editor, const PaintOp& placed, const TextLine& line, int docX) {
    const std::string shown = line.text;
    int byte = line.origin;
    int cursor = placed.x;
    int column = 0;
    while (column < static_cast<int>(shown.size()) && byte < static_cast<int>(editor.value.size())) {
        const int span = Utf8Span(editor.value, static_cast<std::size_t>(byte));
        const int shownSpan = std::min(span, static_cast<int>(shown.size()) - column);
        const int next = cursor + MeasureText(shown.substr(static_cast<std::size_t>(column), static_cast<std::size_t>(std::max(1, shownSpan))), placed.size, placed.face);
        if (docX < cursor + (next - cursor) / 2) {
            break;
        }
        cursor = next;
        column += std::max(1, shownSpan);
        byte += span;
    }
    return FieldClamp(editor, byte);
}

int FieldCaretX(const Editor& editor, const PaintOp& placed, const TextLine& line, int offset) {
    const int local = std::max(0, std::min(offset - line.origin, static_cast<int>(line.text.size())));
    return placed.x + MeasureText(line.text.substr(0, static_cast<std::size_t>(local)), placed.size, placed.face);
}

bool FieldWordByte(unsigned char ch) {
    return std::isalnum(ch) != 0 || ch == '_' || ch == '\'' || ch >= 0x80;
}

int FieldWordMove(const std::string& text, int offset, int direction) {
    int caret = std::max(0, std::min(offset, static_cast<int>(text.size())));
    const auto previous = [&](int index) {
        int prior = index - 1;
        while (prior > 0 && (static_cast<unsigned char>(text[static_cast<std::size_t>(prior)]) & 0xC0u) == 0x80u) {
            --prior;
        }
        return std::max(0, prior);
    };
    if (direction < 0) {
        while (caret > 0 && !FieldWordByte(static_cast<unsigned char>(text[static_cast<std::size_t>(previous(caret))]))) {
            caret = previous(caret);
        }
        while (caret > 0 && FieldWordByte(static_cast<unsigned char>(text[static_cast<std::size_t>(previous(caret))]))) {
            caret = previous(caret);
        }
        return caret;
    }
    while (caret < static_cast<int>(text.size()) && FieldWordByte(static_cast<unsigned char>(text[static_cast<std::size_t>(caret)]))) {
        caret += Utf8Span(text, static_cast<std::size_t>(caret));
    }
    while (caret < static_cast<int>(text.size()) && !FieldWordByte(static_cast<unsigned char>(text[static_cast<std::size_t>(caret)]))) {
        caret += Utf8Span(text, static_cast<std::size_t>(caret));
    }
    return std::min(caret, static_cast<int>(text.size()));
}

void FieldPlaceCaret(Editor& editor, int offset, bool extend) {
    offset = FieldClamp(editor, offset);
    if (!extend) {
        editor.anchor = offset;
    }
    editor.caret = offset;
    editor.selA = extend ? editor.anchor : offset;
    editor.selB = offset;
    FieldSync(editor);
}

void FieldSelectSpan(Editor& editor, int from, int to, int caret) {
    editor.anchor = from;
    editor.selA = FieldClamp(editor, from);
    editor.selB = FieldClamp(editor, to);
    editor.caret = FieldClamp(editor, caret);
    editor.columnX = -1;
    FieldSync(editor);
}

void FieldFocusAt(int index, int docX, int docY, int clicks, bool extend) {
    if (index < 0 || index >= static_cast<int>(gEditors.size())) {
        return;
    }
    const bool same = gEditors[static_cast<std::size_t>(index)].focused;
    for (Editor& editor : gEditors) {
        editor.focused = false;
    }
    Editor& editor = gEditors[static_cast<std::size_t>(index)];
    editor.focused = true;
    if (editor.disabled) {
        FieldSync(editor);
        return;
    }
    const int offset = FieldOffsetAt(editor, docX, docY);
    editor.columnX = -1;
    if (clicks >= 2) {
        PaintOp placed;
        std::vector<TextLine> lines;
        FieldLines(editor, placed, lines);
        if (clicks >= 3 && !lines.empty()) {
            int from = 0;
            int to = 0;
            FieldLineSpan(lines, FieldLineOf(lines, offset), from, to);
            FieldSelectSpan(editor, from, to, to);
            return;
        }
        int from = 0;
        int to = 0;
        WordAround(editor.value, offset, from, to);
        FieldSelectSpan(editor, from, to, to);
        return;
    }
    if (!(extend && same)) {
        editor.anchor = offset;
    }
    FieldPlaceCaret(editor, offset, extend && same);
}

void FieldDragAt(int docX, int docY, int mode) {
    for (Editor& editor : gEditors) {
        if (!editor.focused || editor.disabled) {
            continue;
        }
        const int offset = FieldOffsetAt(editor, docX, docY);
        if (mode >= 2) {
            PaintOp placed;
            std::vector<TextLine> lines;
            FieldLines(editor, placed, lines);
            int from = offset;
            int to = offset;
            int anchorFrom = editor.anchor;
            int anchorTo = editor.anchor;
            if (mode >= 3 && !lines.empty()) {
                FieldLineSpan(lines, FieldLineOf(lines, offset), from, to);
                FieldLineSpan(lines, FieldLineOf(lines, editor.anchor), anchorFrom, anchorTo);
            } else {
                WordAround(editor.value, offset, from, to);
                WordAround(editor.value, editor.anchor, anchorFrom, anchorTo);
            }
            editor.selA = std::min(from, anchorFrom);
            editor.selB = std::max(to, anchorTo);
            editor.caret = offset >= editor.anchor ? editor.selB : editor.selA;
            FieldSync(editor);
            return;
        }
        editor.caret = offset;
        editor.selA = editor.anchor;
        editor.selB = offset;
        editor.columnX = -1;
        FieldSync(editor);
        return;
    }
}

void FieldBlurAll() {
    for (Editor& editor : gEditors) {
        editor.focused = false;
    }
}

bool FieldIsActive() {
    for (const Editor& editor : gEditors) {
        if (editor.focused && !editor.disabled) {
            return true;
        }
    }
    return false;
}

void FieldRemember(Editor& editor) {
    FieldEdit mark;
    mark.value = editor.value;
    mark.caret = editor.caret;
    mark.selA = editor.selA;
    mark.selB = editor.selB;
    mark.anchor = editor.anchor;
    if (!editor.undo.empty() && editor.undo.back().value == mark.value && editor.undo.back().caret == mark.caret &&
        editor.undo.back().selA == mark.selA && editor.undo.back().selB == mark.selB) {
        return;
    }
    editor.undo.push_back(std::move(mark));
    if (editor.undo.size() > 40) {
        editor.undo.erase(editor.undo.begin());
    }
    editor.redo.clear();
}

void FieldRestore(Editor& editor, const FieldEdit& mark) {
    editor.value = mark.value;
    editor.caret = FieldClamp(editor, mark.caret);
    editor.selA = FieldClamp(editor, mark.selA);
    editor.selB = FieldClamp(editor, mark.selB);
    editor.anchor = FieldClamp(editor, mark.anchor);
    editor.columnX = -1;
    FieldSync(editor);
}

bool FieldInsert(const std::string& text) {
    for (Editor& editor : gEditors) {
        if (!editor.focused || editor.disabled) {
            continue;
        }
        std::string incoming = text;
        if (!editor.multiline) {
            std::string flat;
            for (char ch : incoming) {
                if (ch != '\n' && ch != '\r') {
                    flat.push_back(ch);
                }
            }
            if (flat.empty()) {
                return false;
            }
            incoming = flat;
        }
        FieldRemember(editor);
        int from = std::min(editor.selA, editor.selB);
        int to = std::max(editor.selA, editor.selB);
        from = FieldClamp(editor, from);
        to = FieldClamp(editor, to);
        if (from != to) {
            editor.value.erase(static_cast<std::size_t>(from), static_cast<std::size_t>(to - from));
            editor.caret = from;
        }
        editor.value.insert(static_cast<std::size_t>(FieldClamp(editor, editor.caret)), incoming);
        editor.caret = FieldClamp(editor, editor.caret) + static_cast<int>(incoming.size());
        editor.anchor = editor.caret;
        editor.selA = editor.caret;
        editor.selB = editor.caret;
        editor.columnX = -1;
        FieldSync(editor);
        return true;
    }
    return false;
}

bool FieldRemove(bool forward) {
    for (Editor& editor : gEditors) {
        if (!editor.focused || editor.disabled) {
            continue;
        }
        int from = FieldClamp(editor, std::min(editor.selA, editor.selB));
        int to = FieldClamp(editor, std::max(editor.selA, editor.selB));
        if (from == to) {
            if (forward) {
                if (editor.caret >= static_cast<int>(editor.value.size())) {
                    return false;
                }
            } else if (editor.caret <= 0) {
                return false;
            }
        }
        FieldRemember(editor);
        if (from != to) {
            editor.value.erase(static_cast<std::size_t>(from), static_cast<std::size_t>(to - from));
            editor.caret = from;
        } else if (forward) {
            const int span = Utf8Span(editor.value, static_cast<std::size_t>(editor.caret));
            editor.value.erase(static_cast<std::size_t>(editor.caret), static_cast<std::size_t>(span));
        } else {
            int previous = editor.caret - 1;
            while (previous > 0 && (static_cast<unsigned char>(editor.value[static_cast<std::size_t>(previous)]) & 0xC0u) == 0x80u) {
                --previous;
            }
            editor.value.erase(static_cast<std::size_t>(previous), static_cast<std::size_t>(editor.caret - previous));
            editor.caret = previous;
        }
        editor.anchor = editor.caret;
        editor.selA = editor.caret;
        editor.selB = editor.caret;
        editor.columnX = -1;
        FieldSync(editor);
        return true;
    }
    return false;
}

bool FieldRemoveWord(bool forward) {
    for (Editor& editor : gEditors) {
        if (!editor.focused || editor.disabled) {
            continue;
        }
        if (editor.selA != editor.selB) {
            return FieldRemove(forward);
        }
        const int next = FieldWordMove(editor.value, editor.caret, forward ? 1 : -1);
        if (next == editor.caret) {
            return false;
        }
        FieldRemember(editor);
        const int from = std::min(editor.caret, next);
        const int to = std::max(editor.caret, next);
        editor.value.erase(static_cast<std::size_t>(from), static_cast<std::size_t>(to - from));
        editor.caret = from;
        editor.anchor = editor.caret;
        editor.selA = editor.caret;
        editor.selB = editor.caret;
        editor.columnX = -1;
        FieldSync(editor);
        return true;
    }
    return false;
}

bool FieldStep(int delta, bool extend, bool word) {
    for (Editor& editor : gEditors) {
        if (!editor.focused || editor.disabled) {
            continue;
        }
        int next = editor.caret;
        if (!extend && !word && editor.selA != editor.selB) {
            next = delta < 0 ? std::min(editor.selA, editor.selB) : std::max(editor.selA, editor.selB);
        } else if (word) {
            next = FieldWordMove(editor.value, editor.caret, delta);
        } else if (delta < 0) {
            if (next > 0) {
                --next;
                while (next > 0 && (static_cast<unsigned char>(editor.value[static_cast<std::size_t>(next)]) & 0xC0u) == 0x80u) {
                    --next;
                }
            }
        } else if (next < static_cast<int>(editor.value.size())) {
            next += Utf8Span(editor.value, static_cast<std::size_t>(next));
        }
        editor.columnX = -1;
        FieldPlaceCaret(editor, next, extend);
        return true;
    }
    return false;
}

bool FieldStepLine(int delta, bool extend) {
    for (Editor& editor : gEditors) {
        if (!editor.focused || editor.disabled) {
            continue;
        }
        if (!editor.multiline) {
            return true;
        }
        PaintOp placed;
        std::vector<TextLine> lines;
        if (!FieldLines(editor, placed, lines) || lines.empty()) {
            return true;
        }
        const int line = FieldLineOf(lines, editor.caret);
        if (editor.columnX < 0) {
            editor.columnX = FieldCaretX(editor, placed, lines[static_cast<std::size_t>(line)], editor.caret);
        }
        const int target = line + delta;
        int next = editor.caret;
        if (target < 0) {
            next = 0;
        } else if (target >= static_cast<int>(lines.size())) {
            next = static_cast<int>(editor.value.size());
        } else {
            next = FieldHitLine(editor, placed, lines[static_cast<std::size_t>(target)], editor.columnX);
        }
        const int column = editor.columnX;
        FieldPlaceCaret(editor, next, extend);
        editor.columnX = column;
        return true;
    }
    return false;
}

bool FieldStepEnd(bool end, bool extend, bool document) {
    for (Editor& editor : gEditors) {
        if (!editor.focused || editor.disabled) {
            continue;
        }
        int next = end ? static_cast<int>(editor.value.size()) : 0;
        if (!document && editor.multiline) {
            PaintOp placed;
            std::vector<TextLine> lines;
            if (FieldLines(editor, placed, lines) && !lines.empty()) {
                int from = 0;
                int to = 0;
                FieldLineSpan(lines, FieldLineOf(lines, editor.caret), from, to);
                if (end && to > from && to <= static_cast<int>(editor.value.size()) && editor.value[static_cast<std::size_t>(to - 1)] == '\n') {
                    --to;
                }
                next = end ? to : from;
            }
        }
        editor.columnX = -1;
        FieldPlaceCaret(editor, next, extend);
        return true;
    }
    return false;
}

bool FieldHistory(bool redo) {
    for (Editor& editor : gEditors) {
        if (!editor.focused || editor.disabled) {
            continue;
        }
        std::vector<FieldEdit>& from = redo ? editor.redo : editor.undo;
        std::vector<FieldEdit>& to = redo ? editor.undo : editor.redo;
        if (from.empty()) {
            return false;
        }
        FieldEdit current;
        current.value = editor.value;
        current.caret = editor.caret;
        current.selA = editor.selA;
        current.selB = editor.selB;
        current.anchor = editor.anchor;
        to.push_back(std::move(current));
        FieldEdit mark = from.back();
        from.pop_back();
        FieldRestore(editor, mark);
        return true;
    }
    return false;
}

void FieldSelectAll() {
    for (Editor& editor : gEditors) {
        if (!editor.focused || editor.disabled) {
            continue;
        }
        editor.anchor = 0;
        editor.selA = 0;
        editor.selB = static_cast<int>(editor.value.size());
        editor.caret = editor.selB;
        editor.columnX = -1;
        FieldSync(editor);
        return;
    }
}

std::string FieldSelectedText() {
    for (const Editor& editor : gEditors) {
        if (!editor.focused) {
            continue;
        }
        const int from = FieldClamp(editor, std::min(editor.selA, editor.selB));
        const int to = FieldClamp(editor, std::max(editor.selA, editor.selB));
        if (from == to) {
            return {};
        }
        return editor.value.substr(static_cast<std::size_t>(from), static_cast<std::size_t>(to - from));
    }
    return {};
}

struct RegionBar {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int track = 0;
    int trackSpan = 0;
    int thumb = 0;
    int thumbSpan = 0;
    int arrow = 0;
};

bool RegionBarBox(const ScrollRegion& region, bool vertical, RegionBar& bar) {
    const bool yOverflow = region.contentHeight > region.height;
    const bool xOverflow = region.contentWidth > region.width;
    if (vertical ? !yOverflow : !xOverflow) {
        return false;
    }
    const int edge = std::max(1, Px(2));
    const int thick = std::max(1, Px(6));
    const int arrow = std::max(1, Px(6));
    const int gap = std::max(1, Px(3));
    const int minThumb = std::max(thick, Px(28));
    const int corner = (vertical ? xOverflow : yOverflow) ? thick + edge : 0;
    if (vertical) {
        bar.width = thick;
        bar.x = region.x + region.width - edge - thick;
        bar.y = region.y + edge;
        bar.height = region.height - edge * 2 - corner;
    } else {
        bar.height = thick;
        bar.y = region.y + region.height - edge - thick;
        bar.x = region.x + edge;
        bar.width = region.width - edge * 2 - corner;
    }
    if (bar.width <= 0 || bar.height <= 0) {
        return false;
    }
    bar.arrow = arrow;
    const int span = vertical ? bar.height : bar.width;
    int trackSpan = span - (arrow + gap) * 2;
    int track = arrow + gap;
    if (trackSpan < minThumb) {
        trackSpan = span > minThumb ? minThumb : span;
        track = std::max(0, (span - trackSpan) / 2);
    }
    const int content = vertical ? region.contentHeight : region.contentWidth;
    const int view = vertical ? region.height : region.width;
    const int scroll = vertical ? region.scrollY : region.scrollX;
    const int maxScroll = std::max(1, content - view);
    int thumbSpan = content > 0 ? trackSpan * view / content : trackSpan;
    if (thumbSpan < minThumb) {
        thumbSpan = minThumb;
    }
    if (thumbSpan > trackSpan) {
        thumbSpan = trackSpan;
    }
    const int travel = std::max(1, trackSpan - thumbSpan);
    bar.track = track;
    bar.trackSpan = trackSpan;
    bar.thumbSpan = thumbSpan;
    bar.thumb = track + (content > view ? scroll * travel / maxScroll : 0);
    return true;
}

int RegionPart(const RegionBar& bar, bool vertical, int docX, int docY) {
    const int along = vertical ? docY - bar.y : docX - bar.x;
    const int span = vertical ? bar.height : bar.width;
    if (along < bar.arrow) {
        return 1;
    }
    if (along >= span - bar.arrow) {
        return 2;
    }
    if (along >= bar.thumb && along < bar.thumb + bar.thumbSpan) {
        return 3;
    }
    return 4;
}

void WakeRegion(ScrollRegion& region, bool vertical) {
    if (vertical) {
        region.opacityY = 255;
        region.idleY = GetTickCount();
    } else {
        region.opacityX = 255;
        region.idleX = GetTickCount();
    }
}

bool MotionFramePending() {
    if (!gScene.valid) {
        return false;
    }
    for (const PaintOp& op : gScene.ops) {
        if (op.picture < 0 || op.picture >= static_cast<int>(gPictures.size())) {
            continue;
        }
        CachedPicture& cached = gPictures[static_cast<std::size_t>(op.picture)];
        if (cached.image.frameCount <= 1) {
            continue;
        }
        if (PictureFrameIndex(cached.image, gPictureClock) != cached.shownFrame) {
            return true;
        }
    }
    return false;
}

void MotionFramesShown() {
    if (!gScene.valid) {
        return;
    }
    for (const PaintOp& op : gScene.ops) {
        if (op.picture < 0 || op.picture >= static_cast<int>(gPictures.size())) {
            continue;
        }
        CachedPicture& cached = gPictures[static_cast<std::size_t>(op.picture)];
        if (cached.image.frameCount <= 1) {
            continue;
        }
        cached.shownFrame = PictureFrameIndex(cached.image, gPictureClock);
    }
}

}  // namespace

int WindowsDocumentFieldAt(int docX, int docY) { return FieldAtPoint(docX, docY); }
void WindowsDocumentFieldFocus(int index, int docX, int docY, int clicks, bool extend) { FieldFocusAt(index, docX, docY, clicks, extend); }
void WindowsDocumentFieldDrag(int docX, int docY, int mode) { FieldDragAt(docX, docY, mode); }
void WindowsDocumentFieldBlur() { FieldBlurAll(); }
bool WindowsDocumentFieldActive() { return FieldIsActive(); }
bool WindowsDocumentFieldType(const std::string& text) { return FieldInsert(text); }
bool WindowsDocumentFieldDelete(bool forward) { return FieldRemove(forward); }
bool WindowsDocumentFieldDeleteWord(bool forward) { return FieldRemoveWord(forward); }
bool WindowsDocumentFieldMoveCaret(int delta, bool extend, bool word) { return FieldStep(delta, extend, word); }
bool WindowsDocumentFieldMoveLine(int delta, bool extend) { return FieldStepLine(delta, extend); }
bool WindowsDocumentFieldMoveEnd(bool end, bool extend, bool document) { return FieldStepEnd(end, extend, document); }
bool WindowsDocumentFieldUndo(bool redo) { return FieldHistory(redo); }
void WindowsDocumentFieldSelectAll() { FieldSelectAll(); }
std::string WindowsDocumentFieldSelection() { return FieldSelectedText(); }

int WindowsDocumentFieldCaretCover(unsigned now) {
    if (!FieldIsActive()) {
        return 0;
    }
    const unsigned idle = now - gFieldCaretTick;
    if (idle < 650u) {
        return 255;
    }
    const unsigned phase = (idle - 650u) % 1200u;
    if (phase < 220u) {
        return 255 - static_cast<int>(phase) * 255 / 220;
    }
    if (phase < 480u) {
        return 0;
    }
    if (phase < 700u) {
        return static_cast<int>(phase - 480u) * 255 / 220;
    }
    return 255;
}

void WindowsDocumentFieldCaretUse(int cover) { gFieldCaretCover = cover < 0 ? 0 : (cover > 255 ? 255 : cover); }

bool WindowsDocumentFieldCaretRepaint(RoboGPU* gpu, int scrollY, int viewHeight, int& x, int& y, int& width, int& height) {
    x = 0;
    y = 0;
    width = 0;
    height = 0;
    if (gpu == nullptr || !gScene.valid) {
        return false;
    }
    int opIndex = gFieldCaretEditor;
    for (const Editor& editor : gEditors) {
        if (editor.focused && !editor.disabled && editor.op >= 0) {
            opIndex = editor.op;
            break;
        }
    }
    if (opIndex < 0 || opIndex >= static_cast<int>(gScene.ops.size())) {
        gFieldCaretEditor = -1;
        return false;
    }
    const PaintOp& text = gScene.ops[static_cast<std::size_t>(opIndex)];
    x = text.x;
    y = text.y;
    width = text.width;
    height = text.height;
    if (opIndex > 0) {
        const PaintOp& well = gScene.ops[static_cast<std::size_t>(opIndex - 1)];
        if (!well.text && well.fill != 0) {
            x = well.x;
            y = well.y;
            width = well.width;
            height = well.height;
            PaintPrepared(gpu, well, opIndex - 1, scrollY, viewHeight, gScene.light);
        }
    }
    PaintPrepared(gpu, text, opIndex, scrollY, viewHeight, gScene.light);
    bool focused = false;
    for (const Editor& editor : gEditors) {
        if (editor.focused && editor.op == opIndex) {
            focused = true;
            break;
        }
    }
    if (!focused) {
        gFieldCaretEditor = -1;
    }
    return width > 0 && height > 0;
}

bool WindowsDocumentFieldPaintScaled(RoboGPU* gpu, int scrollY, int screenHeight, double scale, int& x, int& y, int& width, int& height) {
    x = 0;
    y = 0;
    width = 0;
    height = 0;
    if (gpu == nullptr || !gScene.valid) {
        return false;
    }
    if (!(scale > 0.0)) {
        scale = 1.0;
    }
    int opIndex = gFieldCaretEditor;
    for (const Editor& editor : gEditors) {
        if (editor.focused && !editor.disabled && editor.op >= 0) {
            opIndex = editor.op;
            break;
        }
    }
    if (opIndex < 0 || opIndex >= static_cast<int>(gScene.ops.size())) {
        return false;
    }
    const double previous = gPaintScale;
    gPaintScale = scale;
    const auto paintScaled = [&](const PaintOp& source, int index) {
        PaintOp prepared = source;
        if (!PrepareDrawn(prepared, index)) {
            return;
        }
        const int docTop = prepared.y - scrollY;
        PaintOp scaled = prepared;
        scaled.x = ScaleLen(prepared.x, scale);
        scaled.y = ScaleLen(docTop, scale);
        scaled.width = std::max(1, ScaleLen(prepared.x + prepared.width, scale) - scaled.x);
        scaled.height = std::max(1, ScaleLen(docTop + prepared.height, scale) - scaled.y);
        scaled.radius = ScaleLen(prepared.radius, scale);
        scaled.wrapWidth = std::max(1, source.width);
        scaled.wrapSize = std::max(1, source.size);
        scaled.size = std::max(1, ScaleLen(std::max(1, prepared.size), scale));
        scaled.borderPx = prepared.borderPx > 0 ? std::max(1, ScaleLen(prepared.borderPx, scale)) : 0;
        PaintOne(gpu, scaled, 0, screenHeight, gScene.light, index);
        if (index == opIndex || width == 0) {
            x = scaled.x;
            y = scaled.y;
            width = scaled.width;
            height = scaled.height;
        }
    };
    if (opIndex > 0) {
        const PaintOp& well = gScene.ops[static_cast<std::size_t>(opIndex - 1)];
        if (!well.text && well.fill != 0) {
            paintScaled(well, opIndex - 1);
        }
    }
    paintScaled(gScene.ops[static_cast<std::size_t>(opIndex)], opIndex);
    gPaintScale = previous;
    return width > 0 && height > 0;
}

unsigned WindowsDocumentFieldRevision() { return gFieldRevision; }

void ContentShift(int index, int& scrollX, int& scrollY) {
    scrollX = 0;
    scrollY = 0;
    for (const ScrollRegion& region : gRegions) {
        if (index < region.opBegin || index >= region.opEnd) {
            continue;
        }
        scrollX = region.scrollX;
        scrollY = region.scrollY;
        return;
    }
}

bool DocumentHits(const PaintOp& op, int x, int y, int width, int height) {
    return op.width > 0 && op.height > 0 && op.x < x + width && op.x + op.width > x && op.y < y + height && op.y + op.height > y;
}

void WindowsDocumentPaintWindowRect(RoboGPU* gpu, int x, int y, int width, int height, int scrollY, int viewWidth, int viewHeight) {
    if (gpu == nullptr || !gScene.valid || width <= 0 || height <= 0 || viewWidth <= 0 || viewHeight <= 0) {
        return;
    }
    int shiftX = 0;
    int shiftY = 0;
    bool found = false;
    for (std::size_t index = 0; index < gScene.ops.size(); ++index) {
        const PaintOp& op = gScene.ops[index];
        if (!DocumentHits(op, x, y, width, height)) {
            continue;
        }
        ContentShift(static_cast<int>(index), shiftX, shiftY);
        found = true;
        if (shiftX != 0 || shiftY != 0) {
            break;
        }
    }
    if (!found) {
        return;
    }
    int left = x - gScrollX - shiftX;
    int top = y - scrollY - shiftY;
    int right = left + width;
    int bottom = top + height;
    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right > viewWidth) {
        right = viewWidth;
    }
    if (bottom > viewHeight) {
        bottom = viewHeight;
    }
    if (right <= left || bottom <= top) {
        return;
    }
    RoboGPUPushScissor(gpu, left, top, right - left, bottom - top);
    RoboGPUFillRect(gpu, left, top, right - left, bottom - top, gScene.background);
    for (std::size_t index = 0; index < gScene.ops.size(); ++index) {
        const PaintOp& op = gScene.ops[index];
        if (!DocumentHits(op, x, y, width, height)) {
            continue;
        }
        PaintPrepared(gpu, op, static_cast<int>(index), scrollY, viewHeight, gScene.light);
    }
    RoboGPUPopScissor(gpu);
}

bool WindowsDocumentMotionDue() { return gVideoDirty || MotionFramePending(); }

void WindowsDocumentMotionMark() {
    gVideoDirty = false;
    MotionFramesShown();
}

bool WindowsDocumentTextAt(int docX, int docY) {
    int run = -1;
    int offset = 0;
    return CaretAt(docX, docY, run, offset);
}

bool WindowsDocumentTextActive() {
    return gTextAnchorRun >= 0 && gTextFocusRun >= 0;
}

void WindowsDocumentTextClear() {
    gTextAnchorRun = -1;
    gTextFocusRun = -1;
    gTextAnchor = 0;
    gTextFocus = 0;
}

bool WindowsDocumentTextBegin(int docX, int docY, int clicks) {
    int run = -1;
    int offset = 0;
    if (!CaretAt(docX, docY, run, offset)) {
        return false;
    }
    const PaintOp& op = gScene.ops[static_cast<std::size_t>(run)];
    gTextAnchorRun = run;
    gTextFocusRun = run;
    if (clicks >= 3 || op.select == 2) {
        gTextAnchor = 0;
        gTextFocus = static_cast<int>(op.value.size());
        return true;
    }
    if (clicks == 2) {
        WordAround(op.value, offset, gTextAnchor, gTextFocus);
        return gTextAnchor != gTextFocus;
    }
    gTextAnchor = offset;
    gTextFocus = offset;
    return true;
}

void WindowsDocumentTextMove(int docX, int docY) {
    if (gTextAnchorRun < 0) {
        return;
    }
    int run = -1;
    int offset = 0;
    if (!CaretNearest(docX, docY, run, offset)) {
        return;
    }
    gTextFocusRun = run;
    gTextFocus = offset;
}

void WindowsDocumentTextEnd() {}

bool WindowsDocumentTextMoveCaret(int delta, bool extend) {
    if (gTextFocusRun < 0 || gTextFocusRun >= static_cast<int>(gScene.ops.size())) {
        return false;
    }
    const PaintOp& op = gScene.ops[static_cast<std::size_t>(gTextFocusRun)];
    const int next = Utf8Step(op.value, gTextFocus, delta);
    gTextFocus = next;
    if (!extend) {
        gTextAnchorRun = gTextFocusRun;
        gTextAnchor = next;
    }
    return true;
}

void WindowsDocumentTextSelectAll() {
    int first = -1;
    int last = -1;
    for (int index = 0; index < static_cast<int>(gScene.ops.size()); ++index) {
        const PaintOp& op = gScene.ops[static_cast<std::size_t>(index)];
        if (op.select == 0 || !op.text || op.value.empty()) {
            continue;
        }
        if (first < 0) {
            first = index;
        }
        last = index;
    }
    if (first < 0) {
        WindowsDocumentTextClear();
        return;
    }
    gTextAnchorRun = first;
    gTextAnchor = 0;
    gTextFocusRun = last;
    gTextFocus = static_cast<int>(gScene.ops[static_cast<std::size_t>(last)].value.size());
}

std::string WindowsDocumentTextSelection() {
    if (!WindowsDocumentTextActive()) {
        return {};
    }
    int runA = 0;
    int offA = 0;
    int runB = 0;
    int offB = 0;
    TextBounds(runA, offA, runB, offB);
    if (runA == runB && offA == offB) {
        return {};
    }
    std::string out;
    for (int run = runA; run <= runB && run < static_cast<int>(gScene.ops.size()); ++run) {
        const PaintOp& op = gScene.ops[static_cast<std::size_t>(run)];
        if (op.select == 0 || !op.text) {
            continue;
        }
        int from = 0;
        int to = static_cast<int>(op.value.size());
        if (run == runA) {
            from = std::max(0, std::min(offA, to));
        }
        if (run == runB) {
            to = std::max(from, std::min(offB, to));
        }
        if (!out.empty()) {
            out.push_back('\n');
        }
        out.append(op.value, static_cast<std::size_t>(from), static_cast<std::size_t>(to - from));
    }
    return out;
}

void TextSpan(int& runA, int& offA, int& runB, int& offB) {
    if (gTextAnchorRun < 0 || gTextFocusRun < 0) {
        runA = -1;
        offA = 0;
        runB = -1;
        offB = 0;
        return;
    }
    TextBounds(runA, offA, runB, offB);
}

int WindowsDocumentTextPatches(WindowPatch* patches, int capacity) {
    int runA = -1;
    int offA = 0;
    int runB = -1;
    int offB = 0;
    TextSpan(runA, offA, runB, offB);
    if (runA == gTextShownA && runB == gTextShownB && offA == gTextShownOffA && offB == gTextShownOffB) {
        return 0;
    }
    int from = runA;
    int to = runB;
    if (gTextShownA >= 0) {
        from = from < 0 ? gTextShownA : std::min(from, gTextShownA);
        to = std::max(to, gTextShownB);
    }
    if (from < 0 || to < from || patches == nullptr || capacity <= 0) {
        return from < 0 ? 0 : -1;
    }
    int count = 0;
    for (int index = from; index <= to && index < static_cast<int>(gScene.ops.size()); ++index) {
        const PaintOp& op = gScene.ops[static_cast<std::size_t>(index)];
        if (!op.text || op.width <= 0 || op.height <= 0) {
            continue;
        }
        if (count >= capacity) {
            return -1;
        }
        patches[count].x = op.x;
        patches[count].y = op.y;
        patches[count].width = op.width;
        patches[count].height = op.height;
        ++count;
    }
    return count;
}

void WindowsDocumentTextMarkPainted() {
    TextSpan(gTextShownA, gTextShownOffA, gTextShownB, gTextShownOffB);
}

void WindowsDocumentRepaintRect(RoboGPU* gpu, int x, int y, int width, int height) {
    if (gpu == nullptr || !gScene.valid || width <= 0 || height <= 0) {
        return;
    }
    const int limit = std::max(gScene.contentHeight, gScene.viewHeight);
    SceneRect rect{x, y, width, height};
    ClipRect(rect, std::max(gScene.viewWidth, gScene.contentWidth), limit);
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }
    int shiftX = 0;
    int shiftY = 0;
    for (std::size_t index = 0; index < gScene.ops.size(); ++index) {
        const PaintOp& op = gScene.ops[index];
        if (!DocumentHits(op, rect.x, rect.y, rect.width, rect.height)) {
            continue;
        }
        ContentShift(static_cast<int>(index), shiftX, shiftY);
        if (shiftX != 0 || shiftY != 0) {
            break;
        }
    }
    RoboU32 backdrop = gScene.background;
    const int midX = rect.x + rect.width / 2;
    const int midY = rect.y + rect.height / 2;
    for (const PaintOp& op : gScene.ops) {
        if (op.text || op.fill == 0 || op.width <= 0 || op.height <= 0) {
            continue;
        }
        if (midX >= op.x && midX < op.x + op.width && midY >= op.y && midY < op.y + op.height) {
            backdrop = op.fill;
        }
    }
    RoboGPUFillRect(gpu, rect.x - gScrollX - shiftX, rect.y - shiftY, rect.width, rect.height, backdrop);
    for (std::size_t index = 0; index < gScene.ops.size(); ++index) {
        const PaintOp& op = gScene.ops[index];
        if (op.width > 0 && op.height > 0 && op.x < rect.x + rect.width && op.x + op.width > rect.x && op.y < rect.y + rect.height &&
            op.y + op.height > rect.y) {
            PaintPrepared(gpu, op, static_cast<int>(index), 0, limit, gScene.light);
        }
    }
    RoboDamageClear(&gpu->damage);
}

void RestoreWindowsDocumentRect(RoboGPU* gpu, int x, int y, int width, int height) {
    if (gpu == nullptr || !gScene.valid || width <= 0 || height <= 0) {
        return;
    }
    gDpi = gScene.dpi;
    PaintSceneRect(gpu, SceneRect{x, y, width, height}, gScene.scrollY, gScene.viewHeight, gScene.light);
}

void PaintWindowsDocument(RoboGPU* gpu, const std::string& path, bool light, int viewWidth, int viewHeight, int scrollY,
                          int* contentHeight, std::vector<WindowControl>* controls, int hotControl, int pressedControl,
                          unsigned dpi, int pointerX, int pointerY) {
    gDpi = dpi == 0 ? 96u : dpi;
    gAssets = FindAssets(path);
    if (gpu == nullptr) {
        return;
    }
    RoboWindowsInstallFontRaster(nullptr, 0);
    if (path.empty() || viewWidth <= 0 || viewHeight <= 0) {
        RoboGPUClear(gpu, RoboFoundationPlatformColorForTheme("bg-primary", light ? ROBO_TRUE : ROBO_FALSE));
        if (contentHeight != nullptr) {
            *contentHeight = viewHeight;
        }
        return;
    }
    if (!SceneCurrent(path, light, gDpi, viewWidth, viewHeight)) {
        if (!BuildScene(gpu, path, light, viewWidth, viewHeight, scrollY, hotControl, pressedControl, pointerX, pointerY)) {
            RoboGPUClear(gpu, RoboFoundationPlatformColorForTheme("bg-primary", light ? ROBO_TRUE : ROBO_FALSE));
            return;
        }
        PublishControls(contentHeight, controls, viewHeight);
        return;
    }
    const int delta = scrollY - gScene.scrollY;
    if (delta != 0 && (delta >= viewHeight || -delta >= viewHeight)) {
        gScene.scrollY = scrollY;
        RoboGPUClear(gpu, gScene.background);
        PaintOps(gpu, gScene.ops, scrollY, viewHeight, light);
    } else if (delta != 0) {
        ShiftScene(gpu, delta, viewWidth, viewHeight);
        gScene.scrollY = scrollY;
        SceneRect exposed{0, delta > 0 ? viewHeight - delta : 0, viewWidth, delta > 0 ? delta : -delta};
        PaintSceneRect(gpu, exposed, scrollY, viewHeight, light);
    }
    if (gScene.hot != hotControl || gScene.pressed != pressedControl) {
        const int changed[4] = {gScene.hot, hotControl, gScene.pressed, pressedControl};
        for (int index : changed) {
            if (index < 0) {
                continue;
            }
            for (PaintOp& op : gScene.ops) {
                if (!op.button || op.control != index) {
                    continue;
                }
                if (op.iconButton) {
                    StyleIconButton(op, light, index == hotControl, index == pressedControl, false);
                } else {
                    RecolorButton(op, light, index == hotControl, index == pressedControl, false);
                }
                PaintSceneRect(gpu, OpBounds(op, scrollY), scrollY, viewHeight, light);
            }
        }
        gScene.hot = hotControl;
        gScene.pressed = pressedControl;
    }
    PublishControls(contentHeight, controls, viewHeight);
}

int LayoutWindowsDocument(const std::string& path, bool light, int viewWidth, int viewHeight, unsigned dpi, int hotControl,
                          int pressedControl, int pointerX, int pointerY, int* contentHeight,
                          std::vector<WindowControl>* controls) {
    RoboWindowsInstallFontRaster(nullptr, 0);
    gDpi = dpi == 0 ? 96u : dpi;
    gAssets = FindAssets(path);
    if (path.empty() || viewWidth <= 0 || viewHeight <= 0) {
        gScene.valid = false;
        if (contentHeight != nullptr) {
            *contentHeight = viewHeight;
        }
        if (controls != nullptr) {
            controls->clear();
        }
        return -1;
    }
    if (SceneCurrent(path, light, gDpi, viewWidth, viewHeight)) {
        if (contentHeight != nullptr) {
            *contentHeight = std::max(viewHeight, gScene.contentHeight);
        }
        return 0;
    }
    if (!BuildScene(nullptr, path, light, viewWidth, viewHeight, 0, hotControl, pressedControl, pointerX, pointerY)) {
        if (contentHeight != nullptr) {
            *contentHeight = viewHeight;
        }
        if (controls != nullptr) {
            controls->clear();
        }
        return -1;
    }
    if (contentHeight != nullptr) {
        *contentHeight = std::max(viewHeight, gScene.contentHeight);
    }
    if (controls != nullptr) {
        *controls = gScene.controls;
    }
    return 1;
}

void PaintWindowsDocumentView(RoboGPU* gpu, int scrollY, int viewWidth, int viewHeight) {
    RoboWindowsInstallFontRaster(nullptr, 0);
    if (gpu == nullptr || !gScene.valid || viewWidth <= 0 || viewHeight <= 0) {
        return;
    }
    RoboGPUResetScissor(gpu);
    RoboGPUClear(gpu, gScene.background);
    PaintOps(gpu, gScene.ops, scrollY, viewHeight, gScene.light);
    gScene.scrollY = scrollY;
    MotionFramesShown();
}

void WindowsDocumentScrollWindow(RoboGPU* gpu, int previousScroll, int scrollY, int viewWidth, int viewHeight, bool full) {
    if (gpu == nullptr || !gScene.valid || viewWidth <= 0 || viewHeight <= 0) {
        return;
    }
    const int delta = scrollY - previousScroll;
    const int distance = delta < 0 ? -delta : delta;
    if (!full && previousScroll >= 0 && delta != 0 && distance < viewHeight) {
        ShiftScene(gpu, delta, viewWidth, viewHeight);
        gScene.scrollY = scrollY;
        SceneRect exposed{0, delta > 0 ? viewHeight - delta : 0, viewWidth, distance};
        PaintSceneRect(gpu, exposed, scrollY, viewHeight, gScene.light);
        return;
    }
    PaintWindowsDocumentView(gpu, scrollY, viewWidth, viewHeight);
}

void PaintWindowsDocumentScaled(RoboGPU* gpu, int scrollY, int screenWidth, int screenHeight, double scale) {
    RoboWindowsInstallFontRaster(nullptr, 0);
    if (gpu == nullptr || !gScene.valid || screenWidth <= 0 || screenHeight <= 0) {
        return;
    }
    if (!(scale > 0.0)) {
        scale = 1.0;
    }
    const double previous = gPaintScale;
    gPaintScale = scale;
    RoboGPUClear(gpu, gScene.background);
    const int docView = std::max(1, static_cast<int>(std::lround(static_cast<double>(screenHeight) / scale)));
    for (std::size_t index = 0; index < gScene.ops.size(); ++index) {
        PaintOp prepared = gScene.ops[index];
        if (!PrepareDrawn(prepared, static_cast<int>(index))) {
            continue;
        }
        const int docTop = prepared.y - scrollY;
        if (docTop + prepared.height < 0 || docTop > docView) {
            continue;
        }
        const bool clipped = PushRegionClip(gpu, static_cast<int>(index), scrollY, scale);
        PaintOp scaled = prepared;
        scaled.x = ScaleLen(prepared.x, scale);
        scaled.y = ScaleLen(docTop, scale);
        scaled.width = std::max(1, ScaleLen(prepared.x + prepared.width, scale) - scaled.x);
        scaled.height = std::max(1, ScaleLen(docTop + prepared.height, scale) - scaled.y);
        scaled.radius = ScaleLen(prepared.radius, scale);
        scaled.wrapWidth = std::max(1, gScene.ops[index].width);
        scaled.wrapSize = std::max(1, gScene.ops[index].size);
        scaled.size = std::max(1, ScaleLen(std::max(1, prepared.size), scale));
        scaled.borderPx = prepared.borderPx > 0 ? std::max(1, ScaleLen(prepared.borderPx, scale)) : 0;
        PaintOne(gpu, scaled, 0, screenHeight, gScene.light, static_cast<int>(index));
        if (clipped) {
            RoboGPUPopScissor(gpu);
        }
    }
    PaintRegionBars(gpu, scrollY, screenHeight, scale);
    gScene.scrollY = scrollY;
    gPaintScale = previous;
    MotionFramesShown();
}

void RasterWindowsDocument(RoboGPU* gpu) {
    RoboWindowsInstallFontRaster(nullptr, 0);
    if (gpu == nullptr || !gScene.valid) {
        return;
    }
    const int height = std::max(gScene.contentHeight, gScene.viewHeight);
    RoboGPUClear(gpu, gScene.background);
    PaintOps(gpu, gScene.ops, 0, height, gScene.light);
    RoboDamageClear(&gpu->damage);
    MotionFramesShown();
}

int FocusReach() {
    return std::max(1, Px(2)) + std::max(1, Px(2)) + std::max(2, Px(5));
}

RoboU32 ReadPixel(const RoboGPU* gpu, int x, int y) {
    if (gpu == nullptr || gpu->back == nullptr || x < 0 || y < 0 || x >= static_cast<int>(gpu->info.width) ||
        y >= static_cast<int>(gpu->info.height)) {
        return gScene.background;
    }
    const auto* row = reinterpret_cast<const RoboU32*>(RoboFrameBufferRow(gpu->back, RoboGPUPitch(gpu), y));
    return row[x];
}

void WritePixel(RoboGPU* gpu, int x, int y, RoboU32 color) {
    if (gpu == nullptr || gpu->back == nullptr || x < 0 || y < 0 || x >= static_cast<int>(gpu->info.width) ||
        y >= static_cast<int>(gpu->info.height)) {
        return;
    }
    auto* row = reinterpret_cast<RoboU32*>(RoboFrameBufferRow(gpu->back, RoboGPUPitch(gpu), y));
    row[x] = color;
}

void ClearButtonShape(RoboGPU* gpu, const PaintOp& op) {
    const int pad = std::max(1, op.borderPx);
    const RoboU32 backdrop = ReadPixel(gpu, op.x, op.y);
    const int outerW = op.width + pad * 2;
    const int outerH = op.height + pad * 2;
    for (int row = 0; row < outerH; ++row) {
        for (int col = 0; col < outerW; ++col) {
            if (PillCoverage(col, row, outerW, outerH) == 0) {
                continue;
            }
            WritePixel(gpu, op.x - pad + col, op.y - pad + row, backdrop);
        }
    }
}

bool OpInside(const PaintOp& op, const SceneRect& rect) {
    return op.width > 0 && op.height > 0 && op.x >= rect.x - 1 && op.y >= rect.y - 1 &&
           op.x + op.width <= rect.x + rect.width + 1 && op.y + op.height <= rect.y + rect.height + 1;
}

bool OpHits(const PaintOp& op, const SceneRect& rect) {
    return op.width > 0 && op.height > 0 && op.x < rect.x + rect.width && op.x + op.width > rect.x &&
           op.y < rect.y + rect.height && op.y + op.height > rect.y;
}

int CollectWindowsDocumentPatches(int hotControl, int pressedControl, int focusControl, WindowPatch* patches, int capacity) {
    if (!gScene.valid || (gScene.hot == hotControl && gScene.pressed == pressedControl && gScene.focus == focusControl)) {
        return 0;
    }
    int count = 0;
    bool overflow = false;
    const int changed[6] = {gScene.hot, hotControl, gScene.pressed, pressedControl, gScene.focus, focusControl};
    const int reach = FocusReach();
    for (int index : changed) {
        if (index < 0) {
            continue;
        }
        for (PaintOp& op : gScene.ops) {
            if (!op.button || op.control != index) {
                continue;
            }
            const bool focused = index == focusControl;
            if (op.iconButton) {
                StyleIconButton(op, gScene.light, index == hotControl, index == pressedControl, focused);
            } else {
                RecolorButton(op, gScene.light, index == hotControl, index == pressedControl, focused);
            }
            if (patches == nullptr || count >= capacity) {
                overflow = true;
                continue;
            }
            patches[count].x = op.x - reach;
            patches[count].y = op.y - reach;
            patches[count].width = op.width + reach * 2;
            patches[count].height = op.height + reach * 2;
            ++count;
        }
    }
    gScene.hot = hotControl;
    gScene.pressed = pressedControl;
    gScene.focus = focusControl;
    return overflow ? -1 : count;
}

void PaintWindowsDocumentRect(RoboGPU* gpu, int x, int y, int width, int height) {
    if (gpu == nullptr || !gScene.valid || width <= 0 || height <= 0) {
        return;
    }
    const int limit = std::max(gScene.contentHeight, gScene.viewHeight);
    SceneRect rect{x, y, width, height};
    ClipRect(rect, gScene.viewWidth, limit);
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }
    const RoboU32 backdrop = ReadPixel(gpu, rect.x, rect.y);
    RoboGPUPushScissor(gpu, rect.x, rect.y, rect.width, rect.height);
    for (int row = 0; row < rect.height; ++row) {
        for (int col = 0; col < rect.width; ++col) {
            WritePixel(gpu, rect.x + col, rect.y + row, backdrop);
        }
    }
    for (std::size_t index = 0; index < gScene.ops.size(); ++index) {
        const PaintOp& op = gScene.ops[index];
        if (OpHits(op, rect)) {
            PaintPrepared(gpu, op, static_cast<int>(index), 0, limit, gScene.light);
        }
    }
    RoboGPUPopScissor(gpu);
    RoboDamageClear(&gpu->damage);
}

void PaintWindowsFocusRing(RoboGPU* gpu, int x, int y, int width, int height) {
    if (gpu == nullptr || width <= 0 || height <= 0) {
        return;
    }
    const int offset = std::max(1, Px(2));
    const int thick = std::max(1, Px(2));
    const int blur = std::max(2, Px(5));
    const int reach = offset + thick + blur;
    const RoboU32 brand = ThemeColor("brand-primary", gScene.light);
    const int widthQ = width * 4;
    const int heightQ = height * 4;
    const int radius = heightQ / 2;
    const int straight = widthQ - heightQ;
    const auto outsideOf = [&](int localX, int localY) {
        if (localX < 0) {
        } else if (straight > 0 && localX > straight) {
            localX -= straight;
        } else {
            localX = 0;
        }
        return Isqrt(localX * localX + localY * localY) - radius;
    };
    const int blurQ = blur * 4;
    const int ring0 = offset * 4;
    const int ring1 = (offset + thick) * 4;
    for (int row = -reach; row < height + reach; ++row) {
        for (int col = -reach; col < width + reach; ++col) {
            unsigned shadow = 0;
            unsigned ring = 0;
            for (int sampleY = 0; sampleY < 4; ++sampleY) {
                for (int sampleX = 0; sampleX < 4; ++sampleX) {
                    const int outside = outsideOf(col * 4 + sampleX - radius, row * 4 + sampleY - radius);
                    if (outside >= 0 && outside < blurQ) {
                        shadow += static_cast<unsigned>(blurQ - outside);
                    }
                    if (outside >= ring0 && outside < ring1) {
                        ++ring;
                    }
                }
            }
            if (shadow != 0) {
                const int cover = static_cast<int>(shadow) * 92 / (16 * blurQ);
                if (cover > 0) {
                    RoboGPUBlendPixel(gpu, x + col, y + row, brand, static_cast<RoboU8>(cover > 92 ? 92 : cover));
                }
            }
            if (ring != 0) {
                const int cover = static_cast<int>(ring) * 122 / 16;
                RoboGPUBlendPixel(gpu, x + col, y + row, brand, static_cast<RoboU8>(cover > 122 ? 122 : cover));
            }
        }
    }
    RoboGPUMarkDamage(gpu, x - reach, y - reach, width + reach * 2, height + reach * 2);
}

bool WindowsDocumentNeedsSpin() {
    if (!gScene.valid) {
        return false;
    }
    for (const PaintOp& op : gScene.ops) {
        if (op.loading) {
            return true;
        }
    }
    return false;
}

void WindowsDocumentAdvanceSpin() {
    RotateSpin(gSpinX, gSpinY);
    RotateSpin(gSpinEndX, gSpinEndY);
}

int WindowsDocumentSpinRects(WindowPatch* patches, int capacity) {
    if (!gScene.valid || patches == nullptr || capacity <= 0) {
        return 0;
    }
    int count = 0;
    for (const PaintOp& op : gScene.ops) {
        if (!op.loading || op.width <= 0 || op.height <= 0) {
            continue;
        }
        if (count >= capacity) {
            break;
        }
        patches[count].x = op.x;
        patches[count].y = op.y;
        patches[count].width = op.width;
        patches[count].height = op.height;
        ++count;
    }
    return count;
}

void WindowsDocumentSetPictureClock(unsigned clock) {
    gPictureClock = clock;
    for (CachedPicture& picture : gPictures) {
        if (!picture.ok || !picture.videoPlaying || picture.videoFile.empty() || picture.image.sourceWidth < 1 || picture.image.sourceHeight < 1) {
            continue;
        }
        if (picture.videoAnchor == 0) {
            picture.videoAnchor = clock == 0 ? 1 : clock;
        }
        int at = picture.videoStart + static_cast<int>(clock - picture.videoAnchor);
        if (at < 0) {
            at = 0;
        }
        if (picture.videoDuration > 1 && at >= picture.videoDuration) {
            at %= picture.videoDuration;
        }
        if (picture.videoShown >= 0 && at + 20 >= picture.videoShown && at <= picture.videoShown + 30) {
            continue;
        }
        std::vector<RoboU32> pixels;
        int width = 0;
        int height = 0;
        int sampleMs = 0;
        if (!WindowsVideoFrame(picture.videoFile, at, pixels, width, height, sampleMs) || pixels.empty()) {
            continue;
        }
        if (sampleMs == picture.videoShown && width == picture.image.sourceWidth && height == picture.image.sourceHeight) {
            continue;
        }
        picture.image.pixels = std::move(pixels);
        picture.image.sourceWidth = width;
        picture.image.sourceHeight = height;
        picture.cacheW = 0;
        picture.cacheFrame = -1;
        picture.videoShown = sampleMs;
        gVideoDirty = true;
    }
}

bool WindowsDocumentNeedsMotion() {
    if (!gScene.valid) {
        return false;
    }
    for (const CachedPicture& picture : gPictures) {
        if (picture.ok && picture.videoPlaying) {
            return true;
        }
    }
    for (const PaintOp& op : gScene.ops) {
        if (op.picture < 0 || op.picture >= static_cast<int>(gPictures.size())) {
            continue;
        }
        if (gPictures[static_cast<std::size_t>(op.picture)].image.frameCount > 1) {
            return true;
        }
    }
    return false;
}

int WindowsDocumentMotionRects(WindowPatch* patches, int capacity) {
    if (!gScene.valid || patches == nullptr || capacity <= 0) {
        return 0;
    }
    int count = 0;
    for (const PaintOp& op : gScene.ops) {
        if (op.picture < 0 || op.picture >= static_cast<int>(gPictures.size()) || op.width <= 0 || op.height <= 0) {
            continue;
        }
        if (gPictures[static_cast<std::size_t>(op.picture)].image.frameCount <= 1 &&
            !gPictures[static_cast<std::size_t>(op.picture)].videoPlaying) {
            continue;
        }
        if (count >= capacity) {
            break;
        }
        patches[count].x = op.x;
        patches[count].y = op.y;
        patches[count].width = op.width;
        patches[count].height = op.height;
        ++count;
    }
    return count;
}

int WindowsDocumentZoomAnchor() { return gScene.valid ? gScene.zoomAnchor : 0; }

bool WindowsDocumentPinchEnabled() { return gScene.valid ? gScene.pinchEnabled : true; }

bool WindowsDocumentZoomEnabled() { return gScene.valid ? gScene.zoomEnabled : true; }

int WindowsDocumentContentWidth() { return gScene.valid ? gScene.contentWidth : 0; }

void WindowsDocumentSetScrollX(int scrollX) { gScrollX = scrollX < 0 ? 0 : scrollX; }

int InnermostRegion(int docX, int docY) {
    for (int index = static_cast<int>(gRegions.size()) - 1; index >= 0; --index) {
        const ScrollRegion& region = gRegions[static_cast<std::size_t>(index)];
        if (docX >= region.x && docX < region.x + region.width && docY >= region.y && docY < region.y + region.height) {
            return index;
        }
    }
    return -1;
}

int WindowsDocumentScrollBy(int docX, int docY, int dx, int dy) {
    const int index = InnermostRegion(docX, docY);
    if (index < 0) {
        return 0;
    }
    ScrollRegion& region = gRegions[static_cast<std::size_t>(index)];
    bool used = false;
    if (dx != 0 && region.contentWidth > region.width) {
        const int before = region.scrollX;
        const int maxScroll = region.contentWidth - region.width;
        region.scrollX += dx;
        if (region.scrollX < 0) {
            region.scrollX = 0;
        }
        if (region.scrollX > maxScroll) {
            region.scrollX = maxScroll;
        }
        if (region.scrollX != before) {
            used = true;
            WakeRegion(region, false);
        }
    }
    if (dy != 0 && region.contentHeight > region.height) {
        const int before = region.scrollY;
        const int maxScroll = region.contentHeight - region.height;
        region.scrollY += dy;
        if (region.scrollY < 0) {
            region.scrollY = 0;
        }
        if (region.scrollY > maxScroll) {
            region.scrollY = maxScroll;
        }
        if (region.scrollY != before) {
            used = true;
            WakeRegion(region, true);
        }
    }
    return used ? 1 : 0;
}

int WindowsDocumentScrollHit(int docX, int docY) {
    gRegionDrag = -1;
    for (ScrollRegion& region : gRegions) {
        region.dragX = false;
        region.dragY = false;
    }
    for (int index = static_cast<int>(gRegions.size()) - 1; index >= 0; --index) {
        ScrollRegion& region = gRegions[static_cast<std::size_t>(index)];
        for (int axis = 0; axis < 2; ++axis) {
            const bool vertical = axis == 0;
            RegionBar bar;
            if (!RegionBarBox(region, vertical, bar)) {
                continue;
            }
            if (docX < bar.x || docX >= bar.x + bar.width || docY < bar.y || docY >= bar.y + bar.height) {
                continue;
            }
            const int part = RegionPart(bar, vertical, docX, docY);
            WakeRegion(region, vertical);
            if (part == 3) {
                gRegionDrag = index;
                gRegionAxis = axis;
                gRegionGrab = (vertical ? docY - bar.y : docX - bar.x) - bar.thumb;
                if (vertical) {
                    region.dragY = true;
                } else {
                    region.dragX = true;
                }
                return vertical ? 5 : 3;
            }
            const int maxScroll = std::max(1, (vertical ? region.contentHeight - region.height : region.contentWidth - region.width));
            const int step = std::max(1, Px(56));
            int& scroll = vertical ? region.scrollY : region.scrollX;
            if (part == 1) {
                scroll = std::max(0, scroll - step);
            } else if (part == 2) {
                scroll = std::min(maxScroll, scroll + step);
            } else {
                const int along = (vertical ? docY - bar.y : docX - bar.x) - bar.thumbSpan / 2;
                int pos = along - bar.track;
                const int travel = std::max(1, bar.trackSpan - bar.thumbSpan);
                if (pos < 0) {
                    pos = 0;
                }
                if (pos > travel) {
                    pos = travel;
                }
                scroll = maxScroll * pos / travel;
            }
            return 4;
        }
    }
    return 0;
}

int WindowsDocumentScrollDrag(int docX, int docY) {
    if (gRegionDrag < 0 || gRegionDrag >= static_cast<int>(gRegions.size())) {
        return 0;
    }
    ScrollRegion& region = gRegions[static_cast<std::size_t>(gRegionDrag)];
    const bool vertical = gRegionAxis == 0;
    RegionBar bar;
    if (!RegionBarBox(region, vertical, bar)) {
        return 0;
    }
    const int along = vertical ? docY - bar.y : docX - bar.x;
    const int travel = std::max(1, bar.trackSpan - bar.thumbSpan);
    int pos = along - gRegionGrab - bar.track;
    if (pos < 0) {
        pos = 0;
    }
    if (pos > travel) {
        pos = travel;
    }
    if (travel > 4 && pos >= travel - 1) {
        pos = travel;
    }
    const int maxScroll = std::max(0, (vertical ? region.contentHeight - region.height : region.contentWidth - region.width));
    int& scroll = vertical ? region.scrollY : region.scrollX;
    const int before = scroll;
    if (pos <= 0) {
        scroll = 0;
    } else if (pos >= travel) {
        scroll = maxScroll;
    } else {
        scroll = static_cast<int>((static_cast<long long>(pos) * maxScroll + travel / 2) / travel);
    }
    WakeRegion(region, vertical);
    return scroll != before ? 1 : 0;
}

void WindowsDocumentScrollEnd() {
    if (gRegionDrag >= 0 && gRegionDrag < static_cast<int>(gRegions.size())) {
        ScrollRegion& region = gRegions[static_cast<std::size_t>(gRegionDrag)];
        region.dragX = false;
        region.dragY = false;
        region.idleX = GetTickCount();
        region.idleY = GetTickCount();
    }
    gRegionDrag = -1;
}

int WindowsDocumentRegionHover(int docX, int docY) {
    int changed = 0;
    for (ScrollRegion& region : gRegions) {
        const int oldX = region.hotX;
        const int oldY = region.hotY;
        region.hotX = 0;
        region.hotY = 0;
        for (int axis = 0; axis < 2; ++axis) {
            const bool vertical = axis == 0;
            RegionBar bar;
            if (!RegionBarBox(region, vertical, bar)) {
                continue;
            }
            if (docX < bar.x || docX >= bar.x + bar.width || docY < bar.y || docY >= bar.y + bar.height) {
                continue;
            }
            const int part = RegionPart(bar, vertical, docX, docY);
            if (vertical) {
                region.hotY = part;
            } else {
                region.hotX = part;
            }
            const int before = vertical ? region.opacityY : region.opacityX;
            WakeRegion(region, vertical);
            if (before != 255) {
                changed = 1;
            }
        }
        if (region.hotX != oldX || region.hotY != oldY) {
            changed = 1;
        }
    }
    return changed;
}

int WindowsDocumentRegionFade(unsigned now) {
    int changed = 0;
    for (ScrollRegion& region : gRegions) {
        const auto drop = [&](int& opacity, unsigned idle, bool held) {
            if (opacity <= 0 || held || now - idle <= 900u) {
                return;
            }
            const int step = opacity / 6;
            opacity -= step < 8 ? 8 : step;
            if (opacity < 0) {
                opacity = 0;
            }
            changed = 1;
        };
        drop(region.opacityY, region.idleY, region.hotY != 0 || region.dragY);
        drop(region.opacityX, region.idleX, region.hotX != 0 || region.dragX);
    }
    return changed;
}

int WindowsDocumentRegionRects(WindowPatch* patches, int capacity) {
    int count = 0;
    for (const ScrollRegion& region : gRegions) {
        if (region.scrollX == region.paintedX && region.scrollY == region.paintedY) {
            continue;
        }
        if (patches == nullptr || count >= capacity) {
            return -1;
        }
        patches[count].x = region.x;
        patches[count].y = region.y;
        patches[count].width = region.width;
        patches[count].height = region.height;
        ++count;
    }
    return count;
}

void WindowsDocumentPaintMovedRegions(RoboGPU* gpu, int scrollY, int viewWidth, int viewHeight) {
    if (gpu == nullptr || !gScene.valid) {
        return;
    }
    for (const ScrollRegion& region : gRegions) {
        if (region.scrollX == region.paintedX && region.scrollY == region.paintedY) {
            continue;
        }
        const int x = region.x - gScrollX;
        const int y = region.y - scrollY;
        if (region.width <= 0 || region.height <= 0 || y + region.height < 0 || y > viewHeight || x + region.width < 0 || x > viewWidth) {
            continue;
        }
        RoboGPUFillRect(gpu, x, y, region.width, region.height, gScene.background);
        const int plate = region.opBegin - 1;
        if (plate >= 0 && plate < static_cast<int>(gScene.ops.size())) {
            const PaintOp& back = gScene.ops[static_cast<std::size_t>(plate)];
            if (!back.text && back.x == region.x && back.y == region.y) {
                PaintPrepared(gpu, back, plate, scrollY, viewHeight, gScene.light);
            }
        }
        const int end = std::min(region.opEnd, static_cast<int>(gScene.ops.size()));
        for (int index = region.opBegin; index < end; ++index) {
            PaintPrepared(gpu, gScene.ops[static_cast<std::size_t>(index)], index, scrollY, viewHeight, gScene.light);
        }
    }
}

void WindowsDocumentRegionMarkPainted() {
    for (ScrollRegion& region : gRegions) {
        region.paintedX = region.scrollX;
        region.paintedY = region.scrollY;
    }
}

void BlendPill(RoboGPU* gpu, int x, int y, int width, int height, RoboU32 color, int cover) {
    if (gpu == nullptr || cover <= 0 || width <= 0 || height <= 0) {
        return;
    }
    const int radius = std::min(width, height) / 2;
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            if (radius > 0) {
                const int dx = col < radius ? radius - col : (col >= width - radius ? col - (width - 1 - radius) : 0);
                const int dy = row < radius ? radius - row : (row >= height - radius ? row - (height - 1 - radius) : 0);
                if (dx * dx + dy * dy > radius * radius) {
                    continue;
                }
            }
            RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>(cover));
        }
    }
    RoboDamageAdd(&gpu->damage, x, y, width, height);
}

void BlendArrow(RoboGPU* gpu, int x, int y, int size, bool vertical, bool start, RoboU32 color, int cover) {
    if (gpu == nullptr || cover <= 0 || size <= 0) {
        return;
    }
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            const int along = vertical ? row : col;
            const int across = vertical ? col : row;
            const int tip = start ? along : size - 1 - along;
            const int half = size / 2;
            if (std::abs(across - half) * 2 > tip) {
                continue;
            }
            RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>(cover));
        }
    }
    RoboDamageAdd(&gpu->damage, x, y, size, size);
}

void WindowsDocumentPaintRegionBars(RoboGPU* gpu, int scrollY, int viewWidth, int viewHeight, double scale) {
    if (gpu == nullptr || !gScene.valid) {
        return;
    }
    if (!(scale > 0.0)) {
        scale = 1.0;
    }
    const bool light = gScene.light;
    const RoboU32 track = ThemeColor("border-default", light);
    const RoboU32 rest = FieldMix(ThemeColor("border-strong", light), ThemeColor("text-tertiary", light), 45);
    const RoboU32 hover = ThemeColor("text-tertiary", light);
    const RoboU32 press = FieldMix(ThemeColor("text-tertiary", light), ThemeColor("text-primary", light), 58);
    const auto place = [&](int doc, int origin, bool across) {
        return ScaleLen(doc - origin, scale);
    };
    for (ScrollRegion& region : gRegions) {
        for (int axis = 0; axis < 2; ++axis) {
            const bool vertical = axis == 0;
            RegionBar bar;
            const bool shaped = RegionBarBox(region, vertical, bar);
            int& opacity = vertical ? region.opacityY : region.opacityX;
            int& shown = vertical ? region.shownY : region.shownX;
            if (!shaped || (opacity <= 0 && shown <= 0)) {
                shown = 0;
                continue;
            }
            const int x = place(bar.x, gScrollX, true);
            const int y = place(bar.y, scrollY, false);
            const int width = std::max(1, place(bar.x + bar.width, gScrollX, true) - x);
            const int height = std::max(1, place(bar.y + bar.height, scrollY, false) - y);
            if (opacity < shown) {
                RoboGPUPushScissor(gpu, x, y, width, height);
                const int plate = region.opBegin - 1;
                if (plate >= 0 && plate < static_cast<int>(gScene.ops.size())) {
                    const PaintOp& back = gScene.ops[static_cast<std::size_t>(plate)];
                    if (!back.text && back.x == region.x && back.y == region.y) {
                        PaintPrepared(gpu, back, plate, scrollY, viewHeight, light);
                    }
                }
                const int end = std::min(region.opEnd, static_cast<int>(gScene.ops.size()));
                for (int index = region.opBegin; index < end; ++index) {
                    PaintPrepared(gpu, gScene.ops[static_cast<std::size_t>(index)], index, scrollY, viewHeight, light);
                }
                RoboGPUPopScissor(gpu);
            }
            shown = opacity;
            if (opacity <= 0) {
                continue;
            }
            const int hot = vertical ? region.hotY : region.hotX;
            const bool drag = vertical ? region.dragY : region.dragX;
            const int arrow = std::max(1, ScaleLen(bar.arrow, scale));
            const int trackPos = place((vertical ? bar.y : bar.x) + bar.track, vertical ? scrollY : gScrollX, vertical);
            const int thumbPos = place((vertical ? bar.y : bar.x) + bar.thumb, vertical ? scrollY : gScrollX, vertical);
            const int trackSpan = std::max(1, ScaleLen(bar.trackSpan, scale));
            const int thumbSpan = std::max(1, ScaleLen(bar.thumbSpan, scale));
            const RoboU32 thumbInk = drag ? press : (hot == 3 ? hover : rest);
            const RoboU32 startInk = hot == 1 ? press : rest;
            const RoboU32 endInk = hot == 2 ? press : rest;
            if (vertical) {
                BlendPill(gpu, x, trackPos, width, trackSpan, track, opacity);
                BlendPill(gpu, x, thumbPos, width, thumbSpan, thumbInk, opacity);
                BlendArrow(gpu, x, y, std::min(arrow, width), true, true, startInk, opacity);
                BlendArrow(gpu, x, y + height - arrow, std::min(arrow, width), true, false, endInk, opacity);
            } else {
                BlendPill(gpu, trackPos, y, trackSpan, height, track, opacity);
                BlendPill(gpu, thumbPos, y, thumbSpan, height, thumbInk, opacity);
                BlendArrow(gpu, x, y, std::min(arrow, height), false, true, startInk, opacity);
                BlendArrow(gpu, x + width - arrow, y, std::min(arrow, height), false, false, endInk, opacity);
            }
            (void)viewWidth;
        }
    }
}

std::string CookKind(const std::string& raw) {
    std::string lower = Lower(raw);
    if (lower == "bar") return "Bar";
    if (lower == "icon") return "Icon";
    if (lower == "window") return "Window";
    if (lower == "button" || lower == "iconbutton") return "Button";
    if (lower == "text") return "Text";
    if (lower == "screen") return "Screen";
    return raw;
}

int WindowsDocumentBoxes(const std::string& path, int viewWidth, int viewHeight, std::vector<WindowBox>& boxes) {
    boxes.clear();
    gPaintScale = 1.0;
    int contentHeight = 0;
    const int layout = LayoutWindowsDocument(path, false, viewWidth, viewHeight, 96, -1, -1, -1, -1, &contentHeight, nullptr);
    if (layout < 0 || !gScene.valid) {
        return -1;
    }
    for (const PaintOp& op : gScene.ops) {
        if (op.role.empty() || op.width <= 0 || op.height <= 0) {
            continue;
        }
        WindowBox box;
        box.kind = CookKind(op.role);
        box.x = op.x;
        box.y = op.y;
        box.width = op.width;
        box.height = op.height;
        box.color = static_cast<int>(op.fill & 0x00FFFFFFu);
        box.text = op.value;
        for (char& ch : box.text) {
            if (ch == '\n' || ch == '\r' || ch == '\t') {
                ch = ' ';
            }
        }
        boxes.push_back(std::move(box));
    }
    return static_cast<int>(boxes.size());
}

std::string WindowsDocumentIconSvg(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return {};
    }
    return RoboWebSvg(gAssets, name);
}

}  // namespace Robix::UI
