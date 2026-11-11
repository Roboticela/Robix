#include "../../RobixUI/Compiler/Components/Text/RoboText.hpp"
#include "../../RobixUI/Compiler/RoboXML.hpp"

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

bool Load(const std::string& source, RoboTextNode& text, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboTextRead(document.root, text, diagnostic);
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
    RoboTextNode text;
    std::string diagnostic;
    Check(Load("<Text Value=\"A\" Family=\"Test\" Size=\"8\" Weight=\"bold\" Color=\"#FF0000\" Wrap=\"none\" Align=\"end\" Width=\"20\"/>",
               text, diagnostic),
          "a Text is valid");
    Check(text.value == "A", "value is kept");
    Check(text.family == "Test", "family is kept");
    Check(text.size == 8, "size is kept");
    Check(text.weight == "bold", "weight is kept");
    Check(text.face == "Test-Bold", "weight selects the bold face");
    Check(text.color == 0xFFFF0000u, "color is kept");
    Check(text.wrap == "none", "wrap is kept");
    Check(text.align == "end", "align is kept");
    Check(text.width == 20 && text.hasWidth, "width is the wrap box");
    Check(text.runs.size() == 1 && text.runs[0].value == "A", "the value is one run");

    Check(Load("<Text Content=\"Hi\" Font=\"Inter-Regular\" FontSize=\"19\" FontWeight=\"600\" TextWrap=\"pretty\" TextAlign=\"center\"/>",
               text, diagnostic),
          "the aliases are valid");
    Check(text.value == "Hi", "content is value");
    Check(text.face == "Inter-SemiBold", "fontWeight 600 is semibold");
    Check(text.size == 19, "fontSize is size");
    Check(text.wrap == "pretty", "textWrap is wrap");
    Check(text.align == "center", "textAlign is align");

    Check(Load("<Text Value=\"A\" Align=\"left\"/>", text, diagnostic), "left is a start align");
    Check(text.align == "start", "left resolves to start");
    Check(Load("<Text Value=\"A\" Align=\"right\"/>", text, diagnostic), "right is an end align");
    Check(text.align == "end", "right resolves to end");
    Check(Load("<Text Value=\"A\" Weight=\"medium\"/>", text, diagnostic), "medium weight is valid");
    Check(text.face == "Inter-Medium", "medium selects the medium face");
    Check(Load("<Text Value=\"A\" Weight=\"500\" Family=\"Test\"/>", text, diagnostic), "numeric weight is valid");
    Check(text.face == "Test-Medium", "500 selects the medium face");
    Check(Load("<Text Value=\"A\" Color=\"#0E7\"/>", text, diagnostic), "a short color is valid");
    Check(text.color == 0xFF00EE77u, "a short color expands");
    Check(Load("<Text Value=\"A\" Wrap=\"char\"/>", text, diagnostic), "char wrap is valid");
    Check(text.wrap == "char", "char wrap is kept");
    Check(Load("<Text Value=\"A\" Wrap=\"balance\"/>", text, diagnostic), "balance wrap is valid");
    Check(Load("<Text Value=\"A\" Wrap=\"nowrap\"/>", text, diagnostic), "nowrap is valid");
    Check(text.wrap == "nowrap", "nowrap is kept");

    Check(Load("<Text/>", text, diagnostic), "an empty Text is valid");
    Check(text.family == "Inter" && text.size == 16 && text.wrap == "word" && text.align == "start", "the defaults are Inter, 16, word, start");
    Check(text.color == 0xFFFFFFFFu, "the default color is white");

    Check(!Load("<Text NotAReal=\"1\"/>", text, diagnostic), "an unknown attribute is rejected");
    Check(diagnostic.find("R7201") != std::string::npos, "an unknown attribute uses R7201");
    Check(!Load("<Text Wrap=\"diagonal\"/>", text, diagnostic), "a bad wrap is rejected");
    Check(diagnostic.find("R7202") != std::string::npos, "a bad wrap uses R7202");
    Check(!Load("<Text Align=\"justify\"/>", text, diagnostic), "a bad align is rejected");
    Check(diagnostic.find("R7202") != std::string::npos, "a bad align uses R7202");
    Check(!Load("<Text Weight=\"black\"/>", text, diagnostic), "a bad weight is rejected");
    Check(diagnostic.find("R7202") != std::string::npos, "a bad weight uses R7202");
    Check(!Load("<Text Size=\"8px\"/>", text, diagnostic), "a bad size is rejected");
    Check(diagnostic.find("R7203") != std::string::npos, "a bad size uses R7203");
    Check(!Load("<Text Size=\"0\"/>", text, diagnostic), "a zero size is rejected");
    Check(diagnostic.find("R7203") != std::string::npos, "a zero size uses R7203");
    Check(!Load("<Text Color=\"red\"/>", text, diagnostic), "a bad color is rejected");
    Check(diagnostic.find("R7203") != std::string::npos, "a bad color uses R7203");
    Check(!Load("<Text Width=\"wide\"/>", text, diagnostic), "a bad width is rejected");
    Check(diagnostic.find("R7203") != std::string::npos, "a bad width uses R7203");
    Check(!Load("<View/>", text, diagnostic), "View is not a Text");
    Check(diagnostic.find("R7204") != std::string::npos, "the wrong tag uses R7204");
}

void TestSpan() {
    RoboTextNode text;
    std::string diagnostic;
    Check(Load("<Text Family=\"Test\" Size=\"8\" Color=\"#FFFFFF\">"
               "<Span Color=\"#FF0000\">A</Span>"
               "<Span Color=\"#00FF00\" Size=\"8\" Weight=\"bold\">A</Span>"
               "</Text>",
               text, diagnostic),
          "Span runs are valid");
    Check(text.runs.size() == 2, "both spans are kept");
    Check(text.runs[0].value == "A" && text.runs[0].color == 0xFFFF0000u, "the first span keeps its color");
    Check(text.runs[1].value == "A" && text.runs[1].color == 0xFF00FF00u, "the second span keeps its color");
    Check(text.runs[1].size == 8 && text.runs[1].hasSize, "the second span keeps its size");
    Check(text.runs[1].weight == "bold", "the second span keeps its weight");

    Check(Load("<Text><Span Value=\"A\" Color=\"#112233\"/></Text>", text, diagnostic), "a Span value is valid");
    Check(text.runs.size() == 1 && text.runs[0].value == "A" && text.runs[0].color == 0xFF112233u, "the Span value is the run");

    Check(!Load("<Text><Span NotAReal=\"1\">A</Span></Text>", text, diagnostic), "an unknown Span attribute is rejected");
    Check(diagnostic.find("R7201") != std::string::npos, "an unknown Span attribute uses R7201");
    Check(!Load("<Text><Span Color=\"nope\">A</Span></Text>", text, diagnostic), "a bad Span color is rejected");
    Check(diagnostic.find("R7203") != std::string::npos, "a bad Span color uses R7203");
    Check(!Load("<Text><Span Size=\"big\">A</Span></Text>", text, diagnostic), "a bad Span size is rejected");
    Check(diagnostic.find("R7203") != std::string::npos, "a bad Span size uses R7203");
    Check(!Load("<Text><Span Weight=\"black\">A</Span></Text>", text, diagnostic), "a bad Span weight is rejected");
    Check(diagnostic.find("R7202") != std::string::npos, "a bad Span weight uses R7202");
    Check(!Load("<Text><View/></Text>", text, diagnostic), "a View child is rejected");
    Check(diagnostic.find("R7204") != std::string::npos, "a non-Span child uses R7204");

    std::string source = "<Text>";
    for (int index = 0; index < 48; ++index) {
        source += "<Span Value=\"A\"/>";
    }
    source += "</Text>";
    Check(Load(source, text, diagnostic), "48 spans are valid");
    Check(text.runs.size() == 48, "all 48 spans are kept");
    source = "<Text>";
    for (int index = 0; index < 49; ++index) {
        source += "<Span Value=\"A\"/>";
    }
    source += "</Text>";
    Check(!Load(source, text, diagnostic), "49 spans are rejected");
    Check(diagnostic.find("R7205") != std::string::npos, "too many spans uses R7205");
}

void TestMeasureAndInk() {
    std::vector<RoboU8> bytes;
    const RoboFontPackView pack = CookedPack(bytes);
    Check(RoboFontPackValidate(bytes.data(), bytes.size()) == ROBO_TRUE, "the cooked pack is valid");

    RoboTextNode text;
    std::string diagnostic;
    Check(Load("<Text Value=\"A\" Family=\"Test\" Size=\"8\" Color=\"#FF0000\"/>", text, diagnostic), "the glyph sample reads");
    RoboTextMeasure(text, pack);
    Check(text.measuredWidth == 4, "one glyph measures its advance");
    Check(text.measuredHeight == 8, "one line measures the font height");

    RoboTextFrame frame;
    RoboTextPaint(text, pack, frame);
    RoboU32 color = 0;
    Check(RoboTextPixel(frame, 0, 0, color) && color == 0xFFFF0000u, "the ink pixel is the glyph coverage in the text color");
    Check(RoboTextPixel(frame, 1, 0, color) && color == 0, "a pixel beside the ink stays clear");
    Check(!RoboTextPixel(frame, frame.width, 0, color), "a pixel outside the frame is rejected");

    Check(Load("<Text Value=\"AA\" Family=\"Test\" Size=\"8\"/>", text, diagnostic), "two glyphs read");
    RoboTextMeasure(text, pack);
    Check(text.measuredWidth == 8, "two glyphs add their advances");
    Check(text.measuredHeight == 8, "two glyphs on one line stay one line tall");

    Check(Load("<Text Value=\"A A\" Family=\"Test\" Size=\"8\" Width=\"4\" Wrap=\"word\"/>", text, diagnostic), "word wrap reads");
    RoboTextMeasure(text, pack);
    Check(text.measuredWidth == 4, "word wrap keeps the line inside the width");
    Check(text.measuredHeight == 16, "word wrap adds a line");

    Check(Load("<Text Value=\"A A\" Family=\"Test\" Size=\"8\" Width=\"4\" Wrap=\"balance\"/>", text, diagnostic), "balance wrap reads");
    RoboTextMeasure(text, pack);
    Check(text.measuredWidth == 4 && text.measuredHeight == 16, "balance wraps like words");

    Check(Load("<Text Value=\"A A\" Family=\"Test\" Size=\"8\" Width=\"4\" Wrap=\"pretty\"/>", text, diagnostic), "pretty wrap reads");
    RoboTextMeasure(text, pack);
    Check(text.measuredWidth == 4 && text.measuredHeight == 16, "pretty wraps like words");

    Check(Load("<Text Value=\"AA\" Family=\"Test\" Size=\"8\" Width=\"4\" Wrap=\"char\"/>", text, diagnostic), "char wrap reads");
    RoboTextMeasure(text, pack);
    Check(text.measuredWidth == 4 && text.measuredHeight == 16, "char wrap breaks inside the word");

    Check(Load("<Text Value=\"AA\" Family=\"Test\" Size=\"8\" Width=\"4\" Wrap=\"none\"/>", text, diagnostic), "none wrap reads");
    RoboTextMeasure(text, pack);
    Check(text.measuredWidth == 8 && text.measuredHeight == 8, "none wrap stays on one line");

    Check(Load("<Text Value=\"A\" Family=\"Missing\" Size=\"8\"/>", text, diagnostic), "a missing face still reads");
    RoboTextMeasure(text, pack);
    Check(text.measuredWidth == 0 && text.measuredHeight == 0, "a missing face measures nothing");

    Check(Load("<Text Family=\"Test\" Size=\"8\" Width=\"12\" Align=\"center\">"
               "<Span Color=\"#FF0000\">A</Span>"
               "<Span Color=\"#00FF00\">A</Span>"
               "</Text>",
               text, diagnostic),
          "two colored spans read");
    RoboTextMeasure(text, pack);
    Check(text.measuredWidth == 8, "the spans measure both advances");
    RoboTextPaint(text, pack, frame);
    Check(RoboTextPixel(frame, 2, 0, color) && color == 0xFFFF0000u, "the first span paints its own color");
    Check(RoboTextPixel(frame, 6, 0, color) && color == 0xFF00FF00u, "the second span paints its own color");

    Check(Load("<Text Value=\"A\" Family=\"Test\" Size=\"8\" Color=\"#0000FF\" Width=\"12\" Align=\"end\"/>", text, diagnostic),
          "end align reads");
    RoboTextPaint(text, pack, frame);
    Check(RoboTextPixel(frame, 8, 0, color) && color == 0xFF0000FFu, "end align places the ink at the trailing edge");
    Check(RoboTextPixel(frame, 0, 0, color) && color == 0, "end align leaves the leading edge clear");
}

}  // namespace

int main() {
    TestSchema();
    TestSpan();
    TestMeasureAndInk();
    if (gFailed != 0) {
        std::cerr << gFailed << " failed, " << gPassed << " passed\n";
        return 1;
    }
    std::cout << gPassed << " passed\n";
    return 0;
}
