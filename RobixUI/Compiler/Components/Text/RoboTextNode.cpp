#include "RoboText.hpp"

#include "../../../RoboShared/RoboUTF8.hpp"

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

constexpr int ROBO_TEXT_SPAN_MAX = 48;
constexpr int ROBO_TEXT_DEFAULT_SIZE = 16;
constexpr RoboU32 ROBO_TEXT_DEFAULT_COLOR = 0xFFFFFFFFu;

const char* const ROBO_TEXT_ATTRIBUTES[] = {
    "id", "value", "content", "text", "family", "font", "fontfamily", "size", "fontsize", "weight", "fontweight",
    "color", "wrap", "textwrap", "align", "textalign", "width",
};

const char* const ROBO_TEXT_SPAN_ATTRIBUTES[] = {
    "value", "content", "text", "color", "size", "fontsize", "weight", "fontweight",
};

std::string RoboTextLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string RoboTextFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

bool RoboTextKnown(const std::string& key, const char* const* names, int count) {
    const std::string folded = RoboTextLower(key);
    for (int index = 0; index < count; ++index) {
        if (folded == names[index]) {
            return true;
        }
    }
    return false;
}

std::string RoboTextAttr(const RoboXMLNode& node, const char* key, const char* alias = nullptr) {
    const std::string value = RoboXMLAttr(node, key);
    if (!value.empty() || alias == nullptr) {
        return value;
    }
    return RoboXMLAttr(node, alias);
}

bool RoboTextInteger(const std::string& text, int& number) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || end == nullptr || *end != '\0') {
        return false;
    }
    number = static_cast<int>(parsed);
    return true;
}

int RoboTextHex(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

bool RoboTextColor(const std::string& text, RoboU32& color) {
    if (text.size() < 4 || text[0] != '#') {
        return false;
    }
    std::string hex = text.substr(1);
    if (hex.size() == 3 || hex.size() == 4) {
        std::string expanded;
        for (char ch : hex) {
            expanded.push_back(ch);
            expanded.push_back(ch);
        }
        hex = expanded;
    }
    if (hex.size() != 6 && hex.size() != 8) {
        return false;
    }
    RoboU32 value = 0;
    for (char ch : hex) {
        const int digit = RoboTextHex(ch);
        if (digit < 0) {
            return false;
        }
        value = (value << 4) | static_cast<RoboU32>(digit);
    }
    if (hex.size() == 6) {
        value |= 0xFF000000u;
    }
    color = value;
    return true;
}

bool RoboTextWeightSuffix(const std::string& weight, std::string& suffix) {
    const std::string folded = RoboTextLower(weight);
    if (folded.empty() || folded == "regular" || folded == "normal" || folded == "400") {
        suffix = "Regular";
        return true;
    }
    if (folded == "medium" || folded == "500") {
        suffix = "Medium";
        return true;
    }
    if (folded == "semibold" || folded == "semi-bold" || folded == "600") {
        suffix = "SemiBold";
        return true;
    }
    if (folded == "bold" || folded == "700") {
        suffix = "Bold";
        return true;
    }
    return false;
}

std::string RoboTextStem(const std::string& family) {
    const std::string folded = RoboTextLower(family);
    const char* suffixes[] = {"-semibold", "-medium", "-regular", "-bold"};
    for (const char* suffix : suffixes) {
        const std::string tail = suffix;
        if (folded.size() > tail.size() && folded.compare(folded.size() - tail.size(), tail.size(), tail) == 0) {
            return family.substr(0, family.size() - tail.size());
        }
    }
    return family;
}

std::string RoboTextFace(const std::string& family, const std::string& weight) {
    const std::string stem = family.empty() ? "Inter" : RoboTextStem(family);
    std::string suffix;
    if (!RoboTextWeightSuffix(weight, suffix)) {
        suffix = "Regular";
    }
    if (weight.empty() && family.find('-') != std::string::npos) {
        return family;
    }
    return stem + "-" + suffix;
}

std::string RoboTextTrim(const std::string& value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

bool RoboTextReadRun(const RoboXMLNode& node, const RoboTextNode& text, RoboTextRun& run, std::string& diagnostic) {
    for (const auto& entry : node.attrs) {
        if (!RoboTextKnown(entry.first, ROBO_TEXT_SPAN_ATTRIBUTES,
                           static_cast<int>(sizeof(ROBO_TEXT_SPAN_ATTRIBUTES) / sizeof(ROBO_TEXT_SPAN_ATTRIBUTES[0])))) {
            diagnostic = RoboTextFail("R7201", "Span attribute '" + entry.first + "' is unknown. Use color, size, or value.");
            return false;
        }
    }
    run.value = RoboTextAttr(node, "value");
    if (run.value.empty()) {
        run.value = RoboTextAttr(node, "content");
    }
    if (run.value.empty()) {
        run.value = RoboTextTrim(node.text);
    }
    run.color = text.color;
    const std::string color = RoboXMLAttr(node, "color");
    if (!color.empty() && !RoboTextColor(color, run.color)) {
        diagnostic = RoboTextFail("R7203", "Span color '" + color + "' is not a color. Use #RRGGBB.");
        return false;
    }
    run.size = text.size;
    run.hasSize = false;
    const std::string size = RoboTextAttr(node, "size", "fontSize");
    if (!size.empty()) {
        int number = 0;
        if (!RoboTextInteger(size, number) || number <= 0) {
            diagnostic = RoboTextFail("R7203", "Span size '" + size + "' is not a size. Use a whole number of pixels.");
            return false;
        }
        run.size = number;
        run.hasSize = true;
    }
    run.weight = RoboTextAttr(node, "weight", "fontWeight");
    if (run.weight.empty()) {
        run.weight = text.weight;
    }
    std::string suffix;
    if (!RoboTextWeightSuffix(run.weight, suffix)) {
        diagnostic = RoboTextFail("R7202", "Span weight '" + run.weight + "' is unknown. Use regular, medium, semibold, or bold.");
        return false;
    }
    return true;
}

int RoboTextScale(int value, int size, int base) {
    if (base <= 0 || size <= 0 || size == base) {
        return value;
    }
    if (value >= 0) {
        return (value * size + base / 2) / base;
    }
    return (value * size - base / 2) / base;
}

RoboFontView RoboTextFont(const RoboFontPackView& pack, const std::string& face, int size) {
    if (pack.header == nullptr || face.empty()) {
        return RoboFontView{};
    }
    return RoboFontPackFindSize(pack, face.c_str(), static_cast<RoboU16>(size));
}

int RoboTextAdvance(const RoboFontView& font, RoboU32 code, int size) {
    if (font.header == nullptr) {
        return 0;
    }
    const int base = RoboFontBaseSize(font);
    if (code == '\n' || code == '\r') {
        return 0;
    }
    if (code == '\t') {
        return RoboTextAdvance(font, ' ', size) * 4;
    }
    const RoboFontGlyph* glyph = RoboFontFindGlyph(font, code);
    if (glyph == nullptr) {
        glyph = RoboFontFindGlyph(font, ROBO_UNICODE_REPLACEMENT);
    }
    if (glyph != nullptr) {
        return RoboTextScale(glyph->advance, size, base);
    }
    return RoboTextScale(static_cast<int>(font.header->ascent) / 2, size, base);
}

int RoboTextWidthOf(const RoboFontView& font, const std::string& value, int size) {
    int width = 0;
    const char* cursor = value.c_str();
    for (;;) {
        const RoboU32 code = RoboUTF8Next(&cursor);
        if (code == 0 || code == '\n') {
            break;
        }
        width += RoboTextAdvance(font, code, size);
    }
    return width;
}

int RoboTextLineHeight(const RoboFontView& font, int size) {
    if (font.header == nullptr) {
        return 0;
    }
    const int base = RoboFontBaseSize(font);
    return RoboTextScale(font.header->ascent, size, base) + RoboTextScale(font.header->descent, size, base) +
           RoboTextScale(font.header->lineGap, size, base);
}

struct RoboTextAtom {
    std::string value;
    std::string face;
    RoboU32 color = ROBO_TEXT_DEFAULT_COLOR;
    int size = ROBO_TEXT_DEFAULT_SIZE;
    bool space = false;
    bool breakLine = false;
};

struct RoboTextPiece {
    std::string value;
    std::string face;
    RoboU32 color = ROBO_TEXT_DEFAULT_COLOR;
    int size = ROBO_TEXT_DEFAULT_SIZE;
    int width = 0;
};

struct RoboTextLine {
    std::vector<RoboTextPiece> pieces;
    int width = 0;
    int height = 0;
};

void RoboTextPushCode(std::vector<RoboTextAtom>& atoms, const RoboTextRun& run, const std::string& face, RoboU32 code,
                      const std::string& wrap) {
    if (code == '\n') {
        RoboTextAtom atom;
        atom.breakLine = true;
        atom.face = face;
        atom.color = run.color;
        atom.size = run.size;
        atoms.push_back(atom);
        return;
    }
    if (code == '\r') {
        return;
    }
    const bool space = RoboUTF8IsSpace(code) == ROBO_TRUE;
    if (wrap == "char" || wrap == "character") {
        RoboTextAtom atom;
        atom.value.push_back(static_cast<char>(code));
        atom.face = face;
        atom.color = run.color;
        atom.size = run.size;
        atom.space = space;
        atoms.push_back(atom);
        return;
    }
    if (space && (wrap == "word" || wrap == "balance" || wrap == "pretty")) {
        RoboTextAtom atom;
        atom.value.push_back(static_cast<char>(code));
        atom.face = face;
        atom.color = run.color;
        atom.size = run.size;
        atom.space = true;
        atoms.push_back(atom);
        return;
    }
    if (atoms.empty() || atoms.back().breakLine || atoms.back().space || atoms.back().color != run.color ||
        atoms.back().size != run.size || atoms.back().face != face) {
        RoboTextAtom atom;
        atom.face = face;
        atom.color = run.color;
        atom.size = run.size;
        atoms.push_back(atom);
    }
    atoms.back().value.push_back(static_cast<char>(code));
}

void RoboTextAtoms(const RoboTextNode& text, std::vector<RoboTextAtom>& atoms) {
    const std::string wrap = text.wrap == "none" || text.wrap == "nowrap" ? "none" : text.wrap;
    for (const RoboTextRun& run : text.runs) {
        const std::string face = RoboTextFace(text.family, run.weight.empty() ? text.weight : run.weight);
        const char* cursor = run.value.c_str();
        for (;;) {
            const char* before = cursor;
            const RoboU32 code = RoboUTF8Next(&cursor);
            if (code == 0) {
                break;
            }
            if (code < 128) {
                RoboTextPushCode(atoms, run, face, code, wrap);
                continue;
            }
            const std::string bytes(before, cursor);
            if (atoms.empty() || atoms.back().breakLine || atoms.back().space || atoms.back().color != run.color ||
                atoms.back().size != run.size || atoms.back().face != face || wrap == "char" || wrap == "character") {
                RoboTextAtom atom;
                atom.face = face;
                atom.color = run.color;
                atom.size = run.size;
                atoms.push_back(atom);
            }
            atoms.back().value += bytes;
        }
    }
}

std::vector<RoboTextLine> RoboTextLines(const RoboTextNode& text, const RoboFontPackView& pack) {
    std::vector<RoboTextAtom> atoms;
    RoboTextAtoms(text, atoms);
    const bool wrapping = text.hasWidth && text.wrap != "none" && text.wrap != "nowrap";
    std::vector<RoboTextLine> lines;
    RoboTextLine line;
    auto flush = [&]() {
        lines.push_back(line);
        line = RoboTextLine{};
    };
    for (const RoboTextAtom& atom : atoms) {
        if (atom.breakLine) {
            flush();
            continue;
        }
        const RoboFontView font = RoboTextFont(pack, atom.face, atom.size);
        const int width = RoboTextWidthOf(font, atom.value, atom.size);
        if (atom.space && line.pieces.empty()) {
            continue;
        }
        if (wrapping && !line.pieces.empty() && line.width + width > text.width) {
            flush();
            if (atom.space) {
                continue;
            }
        }
        RoboTextPiece piece;
        piece.value = atom.value;
        piece.face = atom.face;
        piece.color = atom.color;
        piece.size = atom.size;
        piece.width = width;
        line.pieces.push_back(piece);
        line.width += width;
        const int height = RoboTextLineHeight(font, atom.size);
        if (height > line.height) {
            line.height = height;
        }
    }
    if (!line.pieces.empty() || lines.empty()) {
        flush();
    }
    if (lines.size() == 1 && lines[0].pieces.empty()) {
        lines.clear();
    }
    return lines;
}

void RoboTextBlit(RoboTextFrame& frame, const RoboFontView& font, const RoboFontGlyph* glyph, int dstX, int dstY,
                  int size, RoboU32 color) {
    if (glyph == nullptr || glyph->width == 0 || glyph->height == 0 || font.bitmaps == nullptr) {
        return;
    }
    const int base = RoboFontBaseSize(font);
    const int outW = RoboTextScale(glyph->width, size, base);
    const int outH = RoboTextScale(glyph->height, size, base);
    if (outW <= 0 || outH <= 0) {
        return;
    }
    const RoboU8* bitmap = font.bitmaps + glyph->bitmapOffset;
    const RoboU8 sourceA = static_cast<RoboU8>((color >> 24) & 0xFFu);
    const RoboU8 sourceR = static_cast<RoboU8>((color >> 16) & 0xFFu);
    const RoboU8 sourceG = static_cast<RoboU8>((color >> 8) & 0xFFu);
    const RoboU8 sourceB = static_cast<RoboU8>(color & 0xFFu);
    for (int row = 0; row < outH; ++row) {
        for (int col = 0; col < outW; ++col) {
            const int srcX = col * static_cast<int>(glyph->width) / outW;
            const int srcY = row * static_cast<int>(glyph->height) / outH;
            const RoboU8 coverage = bitmap[srcY * glyph->width + srcX];
            if (coverage == 0 || sourceA == 0) {
                continue;
            }
            const int x = dstX + col;
            const int y = dstY + row;
            if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
                continue;
            }
            const int alpha = coverage * sourceA / 255;
            RoboU32& pixel = frame.pixels[static_cast<std::size_t>(y * frame.width + x)];
            if (alpha >= 255) {
                pixel = color | 0xFF000000u;
                continue;
            }
            const RoboU8 destA = static_cast<RoboU8>((pixel >> 24) & 0xFFu);
            const RoboU8 destR = static_cast<RoboU8>((pixel >> 16) & 0xFFu);
            const RoboU8 destG = static_cast<RoboU8>((pixel >> 8) & 0xFFu);
            const RoboU8 destB = static_cast<RoboU8>(pixel & 0xFFu);
            const int inv = 255 - alpha;
            const RoboU8 outA = static_cast<RoboU8>(alpha + destA * inv / 255);
            const RoboU8 outR = static_cast<RoboU8>((sourceR * alpha + destR * inv) / 255);
            const RoboU8 outG = static_cast<RoboU8>((sourceG * alpha + destG * inv) / 255);
            const RoboU8 outB = static_cast<RoboU8>((sourceB * alpha + destB * inv) / 255);
            pixel = (static_cast<RoboU32>(outA) << 24) | (static_cast<RoboU32>(outR) << 16) |
                    (static_cast<RoboU32>(outG) << 8) | outB;
        }
    }
}

void RoboTextDrawPiece(RoboTextFrame& frame, const RoboFontPackView& pack, const RoboTextPiece& piece, int x, int baseline) {
    const RoboFontView font = RoboTextFont(pack, piece.face, piece.size);
    if (font.header == nullptr) {
        return;
    }
    const int base = RoboFontBaseSize(font);
    const char* cursor = piece.value.c_str();
    int pen = x;
    for (;;) {
        const RoboU32 code = RoboUTF8Next(&cursor);
        if (code == 0 || code == '\n') {
            break;
        }
        const RoboFontGlyph* glyph = RoboFontFindGlyph(font, code);
        if (glyph == nullptr) {
            pen += RoboTextAdvance(font, code, piece.size);
            continue;
        }
        const int bearingX = RoboTextScale(glyph->bearingX, piece.size, base);
        const int bearingY = RoboTextScale(glyph->bearingY, piece.size, base);
        RoboTextBlit(frame, font, glyph, pen + bearingX, baseline - bearingY, piece.size, piece.color);
        pen += RoboTextScale(glyph->advance, piece.size, base);
    }
}

}  // namespace

bool RoboTextRead(const RoboXMLNode& node, RoboTextNode& text, std::string& diagnostic) {
    text = RoboTextNode{};
    diagnostic.clear();
    if (RoboTextLower(node.name) != "text") {
        diagnostic = RoboTextFail("R7204", "'" + node.name + "' is not a Text. Use Text.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!RoboTextKnown(entry.first, ROBO_TEXT_ATTRIBUTES,
                           static_cast<int>(sizeof(ROBO_TEXT_ATTRIBUTES) / sizeof(ROBO_TEXT_ATTRIBUTES[0])))) {
            diagnostic = RoboTextFail("R7201", "Text attribute '" + entry.first + "' is unknown. Use value, family, size, weight, color, wrap, or align.");
            return false;
        }
    }
    text.value = RoboTextAttr(node, "value");
    if (text.value.empty()) {
        text.value = RoboTextAttr(node, "content");
    }
    if (text.value.empty()) {
        text.value = RoboTextAttr(node, "text");
    }
    text.family = RoboTextAttr(node, "family");
    if (text.family.empty()) {
        text.family = RoboTextAttr(node, "font", "fontFamily");
    }
    if (text.family.empty()) {
        text.family = "Inter";
    }
    text.weight = RoboTextAttr(node, "weight", "fontWeight");
    std::string suffix;
    if (!RoboTextWeightSuffix(text.weight, suffix)) {
        diagnostic = RoboTextFail("R7202", "Text weight '" + text.weight + "' is unknown. Use regular, medium, semibold, or bold.");
        return false;
    }
    text.face = RoboTextFace(text.family, text.weight);
    const std::string size = RoboTextAttr(node, "size", "fontSize");
    if (!size.empty()) {
        int number = 0;
        if (!RoboTextInteger(size, number) || number <= 0) {
            diagnostic = RoboTextFail("R7203", "Text size '" + size + "' is not a size. Use a whole number of pixels.");
            return false;
        }
        text.size = number;
    }
    const std::string color = RoboXMLAttr(node, "color");
    if (!color.empty() && !RoboTextColor(color, text.color)) {
        diagnostic = RoboTextFail("R7203", "Text color '" + color + "' is not a color. Use #RRGGBB.");
        return false;
    }
    text.wrap = RoboTextAttr(node, "wrap", "textWrap");
    if (text.wrap.empty()) {
        text.wrap = "word";
    }
    text.wrap = RoboTextLower(text.wrap);
    if (text.wrap != "none" && text.wrap != "nowrap" && text.wrap != "word" && text.wrap != "char" &&
        text.wrap != "character" && text.wrap != "balance" && text.wrap != "pretty") {
        diagnostic = RoboTextFail("R7202", "Text wrap '" + text.wrap + "' is unknown. Use none, word, char, balance, or pretty.");
        return false;
    }
    text.align = RoboTextAttr(node, "align", "textAlign");
    if (text.align.empty()) {
        text.align = "start";
    }
    text.align = RoboTextLower(text.align);
    if (text.align == "left") {
        text.align = "start";
    } else if (text.align == "right") {
        text.align = "end";
    }
    if (text.align != "start" && text.align != "center" && text.align != "end") {
        diagnostic = RoboTextFail("R7202", "Text align '" + text.align + "' is unknown. Use start, center, or end.");
        return false;
    }
    const std::string width = RoboXMLAttr(node, "width");
    if (!width.empty()) {
        int number = 0;
        if (!RoboTextInteger(width, number) || number < 0) {
            diagnostic = RoboTextFail("R7203", "Text width '" + width + "' is not a size. Use a whole number of pixels.");
            return false;
        }
        text.width = number;
        text.hasWidth = true;
    }
    if (!node.children.empty()) {
        if (static_cast<int>(node.children.size()) > ROBO_TEXT_SPAN_MAX) {
            diagnostic = RoboTextFail("R7205", "Text has more than 48 Span children.");
            return false;
        }
        for (const RoboXMLNode& child : node.children) {
            if (RoboTextLower(child.name) != "span") {
                diagnostic = RoboTextFail("R7204", "'" + child.name + "' is not a Span. Text only contains Span.");
                return false;
            }
            RoboTextRun run;
            if (!RoboTextReadRun(child, text, run, diagnostic)) {
                return false;
            }
            text.runs.push_back(run);
        }
    } else {
        RoboTextRun run;
        run.value = text.value.empty() ? RoboTextTrim(node.text) : text.value;
        if (text.value.empty()) {
            text.value = run.value;
        }
        run.color = text.color;
        run.size = text.size;
        run.weight = text.weight;
        text.runs.push_back(run);
    }
    return true;
}

void RoboTextMeasure(RoboTextNode& text, const RoboFontPackView& pack) {
    const std::vector<RoboTextLine> lines = RoboTextLines(text, pack);
    text.measuredWidth = 0;
    text.measuredHeight = 0;
    for (const RoboTextLine& line : lines) {
        if (line.width > text.measuredWidth) {
            text.measuredWidth = line.width;
        }
        text.measuredHeight += line.height;
    }
}

void RoboTextPaint(const RoboTextNode& text, const RoboFontPackView& pack, RoboTextFrame& frame) {
    const std::vector<RoboTextLine> lines = RoboTextLines(text, pack);
    int contentWidth = 0;
    int contentHeight = 0;
    for (const RoboTextLine& line : lines) {
        const int box = text.hasWidth ? text.width : line.width;
        if (box > contentWidth) {
            contentWidth = box;
        }
        if (line.width > contentWidth) {
            contentWidth = line.width;
        }
        contentHeight += line.height;
    }
    frame.width = contentWidth > 0 ? contentWidth : 1;
    frame.height = contentHeight > 0 ? contentHeight : 1;
    frame.pixels.assign(static_cast<std::size_t>(frame.width * frame.height), 0);
    int y = 0;
    for (const RoboTextLine& line : lines) {
        const int box = text.hasWidth && text.width > line.width ? text.width : line.width;
        int x = 0;
        if (text.align == "center") {
            x = (box - line.width) / 2;
        } else if (text.align == "end") {
            x = box - line.width;
        }
        const RoboFontView first = line.pieces.empty() ? RoboFontView{} : RoboTextFont(pack, line.pieces[0].face, line.pieces[0].size);
        const int ascent = first.header == nullptr ? 0 : RoboTextScale(first.header->ascent, line.pieces[0].size, RoboFontBaseSize(first));
        int pen = x;
        for (const RoboTextPiece& piece : line.pieces) {
            RoboTextDrawPiece(frame, pack, piece, pen, y + ascent);
            pen += piece.width;
        }
        y += line.height;
    }
}

bool RoboTextPixel(const RoboTextFrame& frame, int x, int y, RoboU32& color) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return false;
    }
    color = frame.pixels[static_cast<std::size_t>(y * frame.width + x)];
    return true;
}
