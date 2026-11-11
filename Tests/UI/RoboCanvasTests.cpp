#include "../../RobixUI/Compiler/Components/Canvas/RoboCanvas.hpp"
#include "../../RobixUI/Compiler/RoboXML.hpp"
#include "../../RobixUI/RoboShared/RoboFontPack.hpp"

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

bool Load(const std::string& source, RoboCanvasNode& canvas, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboCanvasRead(document.root, canvas, diagnostic);
}

template <typename T>
void Append(std::vector<RoboU8>& bytes, const T& value) {
    const auto* raw = reinterpret_cast<const RoboU8*>(&value);
    bytes.insert(bytes.end(), raw, raw + sizeof(T));
}

RoboFontPackView CookedPack(std::vector<RoboU8>& bytes) {
    bytes.clear();
    RoboFontPackHeader header{};
    header.magic = ROBO_FONT_PACK_MAGIC;
    header.version = ROBO_FONT_PACK_VERSION;
    header.fontCount = 1;
    header.defaultIndex = 0;
    header.entryOffset = static_cast<RoboU32>(sizeof(RoboFontPackHeader));
    Append(bytes, header);
    RoboFontPackEntry entry{};
    std::memcpy(entry.name, "Test-Regular", 12);
    entry.offset = static_cast<RoboU32>(sizeof(RoboFontPackHeader) + sizeof(RoboFontPackEntry));
    const RoboU32 bitmapBytes = 4 * 8;
    entry.size = static_cast<RoboU32>(sizeof(RoboFontHeader) + sizeof(RoboFontGlyph) * 2 + bitmapBytes);
    Append(bytes, entry);
    RoboFontHeader font{};
    font.magic = ROBO_FONT_MAGIC;
    font.ascent = 8;
    font.descent = 0;
    font.lineGap = 0;
    font.glyphCount = 2;
    font.firstCodepoint = 32;
    font.lastCodepoint = 65;
    font.unitsPerEm = 8;
    font.reserved0 = 8;
    Append(bytes, font);
    const RoboU32 bitmapOffset = static_cast<RoboU32>(sizeof(RoboFontHeader) + sizeof(RoboFontGlyph) * 2);
    RoboFontGlyph space{};
    space.codepoint = 32;
    space.advance = 2;
    space.bearingX = 0;
    space.bearingY = 0;
    space.width = 0;
    space.height = 0;
    space.bitmapOffset = bitmapOffset;
    Append(bytes, space);
    RoboFontGlyph letter{};
    letter.codepoint = 65;
    letter.advance = 4;
    letter.bearingX = 0;
    letter.bearingY = 8;
    letter.width = 4;
    letter.height = 8;
    letter.bitmapOffset = bitmapOffset;
    Append(bytes, letter);
    bytes.push_back(255);
    for (RoboU32 index = 1; index < bitmapBytes; ++index) {
        bytes.push_back(0);
    }
    return RoboFontPackMakeView(bytes.data(), bytes.size());
}

void TestSchema() {
    RoboCanvasNode canvas;
    std::string diagnostic;
    const char* source =
        "<Canvas Width=\"32\" Height=\"24\">"
        "<Rect X=\"4\" Y=\"4\" Width=\"8\" Height=\"6\" Color=\"#FF0000\"/>"
        "<Disc X=\"20\" Y=\"8\" Radius=\"3\" Color=\"#00FF00\"/>"
        "<Circle X=\"8\" Y=\"18\" R=\"2\" Color=\"#0000FF\"/>"
        "<Line X1=\"0\" Y1=\"22\" X2=\"8\" Y2=\"22\" Color=\"#FFFFFF\"/>"
        "<Text X=\"1\" Y=\"0\" Value=\"A\" Family=\"Test\" Size=\"8\" Color=\"#00FF00\"/>"
        "</Canvas>";
    Check(Load(source, canvas, diagnostic), "canvas schema reads");
    Check(diagnostic.empty(), "canvas schema has no diagnostic");
    Check(canvas.width == 32 && canvas.height == 24, "canvas keeps the box");
    Check(canvas.commands.size() == 5, "canvas keeps the draw list");
    Check(canvas.commands[0].kind == RoboCanvasKind::Rect && canvas.commands[0].width == 8 && canvas.commands[0].color == 0xFFFF0000u,
          "the rect keeps its box and color");
    Check(canvas.commands[1].kind == RoboCanvasKind::Disc && canvas.commands[1].radius == 3, "the disc keeps its radius");
    Check(canvas.commands[2].kind == RoboCanvasKind::Disc && canvas.commands[2].radius == 2, "circle is a disc");
    Check(canvas.commands[3].kind == RoboCanvasKind::Line && canvas.commands[3].x2 == 8 && canvas.commands[3].y2 == 22, "the line keeps both ends");
    Check(canvas.commands[4].kind == RoboCanvasKind::Text && canvas.commands[4].value == "A" && canvas.commands[4].size == 8,
          "the text keeps its value and size");
    Check(!Load("<Canvas Width=\"32\" Height=\"24\" Fit=\"cover\"><Rect Width=\"2\" Height=\"2\"/></Canvas>", canvas, diagnostic),
          "an unknown canvas attribute is rejected");
    Check(diagnostic.find("R7601") != std::string::npos, "an unknown canvas attribute uses R7601");
    Check(!Load("<Canvas Width=\"32\" Height=\"24\"><Rect Width=\"2\" Height=\"2\" Color=\"red\"/></Canvas>", canvas, diagnostic),
          "a named rect color is rejected");
    Check(diagnostic.find("R7602") != std::string::npos, "a bad draw color uses R7602");
    Check(!Load("<Canvas Width=\"0\" Height=\"24\"><Rect Width=\"2\" Height=\"2\"/></Canvas>", canvas, diagnostic), "a zero canvas width is rejected");
    Check(diagnostic.find("R7603") != std::string::npos, "a bad canvas size uses R7603");
    Check(!Load("<Canvas Width=\"32\" Height=\"wide\"><Rect Width=\"2\" Height=\"2\"/></Canvas>", canvas, diagnostic), "a word canvas height is rejected");
    Check(!Load("<Canvas Width=\"32\" Height=\"24\"><Rect Width=\"0\" Height=\"2\"/></Canvas>", canvas, diagnostic), "a zero rect is rejected");
    Check(!Load("<Canvas Width=\"32\" Height=\"24\"><Disc Radius=\"0\"/></Canvas>", canvas, diagnostic), "a zero disc is rejected");
    Check(!Load("<View Width=\"32\" Height=\"24\"><Rect Width=\"2\" Height=\"2\"/></View>", canvas, diagnostic), "a view is not a canvas");
    Check(diagnostic.find("R7604") != std::string::npos, "the wrong canvas tag uses R7604");
    Check(!Load("<Canvas Width=\"32\" Height=\"24\"><Path D=\"M0 0\"/></Canvas>", canvas, diagnostic), "a path is not a draw");
    Check(!Load("<Canvas Width=\"32\" Height=\"24\"/>", canvas, diagnostic), "a canvas without a draw list is rejected");
    Check(diagnostic.find("R7605") != std::string::npos, "a missing draw list uses R7605");
    Check(!Load("<Canvas Width=\"32\" Height=\"24\"><Text/></Canvas>", canvas, diagnostic), "text without a value is rejected");
    std::string many = "<Canvas Width=\"8\" Height=\"8\">";
    for (int index = 0; index < 65; ++index) {
        many += "<Rect Width=\"1\" Height=\"1\"/>";
    }
    many += "</Canvas>";
    Check(!Load(many, canvas, diagnostic), "more than 64 draws are rejected");
    Check(diagnostic.find("R7606") != std::string::npos, "too many draws use R7606");
}

void TestPaint() {
    RoboCanvasNode canvas;
    std::string diagnostic;
    Check(Load("<Canvas Width=\"32\" Height=\"24\"><Rect X=\"4\" Y=\"4\" Width=\"8\" Height=\"6\" Color=\"#FF0000\"/></Canvas>", canvas, diagnostic),
          "the rect fixture reads");
    std::vector<RoboU8> packBytes;
    const RoboFontPackView pack = CookedPack(packBytes);
    RoboCanvasFrame frame;
    RoboCanvasPaint(canvas, pack, frame);
    RoboU32 color = 0;
    Check(frame.width == 32 && frame.height == 24, "the canvas frame is the element box");
    Check(RoboCanvasPixel(frame, 4, 4, color) && color == 0xFFFF0000u, "a pixel on the rect is the fill");
    Check(RoboCanvasPixel(frame, 11, 9, color) && color == 0xFFFF0000u, "the far corner of the rect is the fill");
    Check(RoboCanvasPixel(frame, 0, 0, color) && color == 0, "a pixel outside the rect stays clear");
    Check(RoboCanvasPixel(frame, 12, 4, color) && color == 0, "the pixel beside the rect stays clear");
    Check(!RoboCanvasPixel(frame, 32, 0, color), "a pixel outside the canvas is rejected");
    Check(!RoboCanvasPixel(frame, -1, 0, color), "a negative canvas pixel is rejected");

    Check(Load("<Canvas Width=\"16\" Height=\"16\"><Disc X=\"8\" Y=\"8\" Radius=\"3\" Color=\"#00FF00\"/></Canvas>", canvas, diagnostic),
          "the disc fixture reads");
    RoboCanvasPaint(canvas, pack, frame);
    Check(RoboCanvasPixel(frame, 8, 8, color) && color == 0xFF00FF00u, "the disc center is the fill");
    Check(RoboCanvasPixel(frame, 0, 0, color) && color == 0, "a pixel outside the disc stays clear");

    Check(Load("<Canvas Width=\"16\" Height=\"8\"><Line X1=\"1\" Y1=\"3\" X2=\"6\" Y2=\"3\" Color=\"#0000FF\"/></Canvas>", canvas, diagnostic),
          "the line fixture reads");
    RoboCanvasPaint(canvas, pack, frame);
    Check(RoboCanvasPixel(frame, 1, 3, color) && color == 0xFF0000FFu, "the line start is the stroke");
    Check(RoboCanvasPixel(frame, 6, 3, color) && color == 0xFF0000FFu, "the line end is the stroke");
    Check(RoboCanvasPixel(frame, 1, 2, color) && color == 0, "a pixel off the line stays clear");

    Check(Load("<Canvas Width=\"16\" Height=\"16\"><Text X=\"0\" Y=\"0\" Value=\"A\" Family=\"Test\" Size=\"8\" Color=\"#FF0000\"/></Canvas>", canvas,
               diagnostic),
          "the text fixture reads");
    RoboCanvasPaint(canvas, pack, frame);
    Check(RoboCanvasPixel(frame, 0, 0, color) && color == 0xFFFF0000u, "the text ink pixel uses the draw color");
    Check(RoboCanvasPixel(frame, 1, 0, color) && color == 0, "the pixel beside the text ink stays clear");
}

}  // namespace

int main() {
    TestSchema();
    TestPaint();
    if (gFailed != 0) {
        std::cerr << gFailed << " failed, " << gPassed << " passed\n";
        return 1;
    }
    std::cout << gPassed << " passed\n";
    return 0;
}
