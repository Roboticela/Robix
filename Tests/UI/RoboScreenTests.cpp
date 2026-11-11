#include "../../RobixUI/Compiler/Components/Screen/RoboScreen.hpp"
#include "../../RobixUI/Compiler/Foundation/FoundationPlatform.hpp"
#include "../../RobixUI/Compiler/RoboXML.hpp"

#include <iostream>
#include <string>

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

bool Read(const std::string& source, RoboScreenRecord& screen, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    const RoboXMLNode* screenNode = RoboScreenFind(document.root);
    if (screenNode == nullptr) {
        diagnostic = "missing";
        return false;
    }
    return RoboScreenRead(*screenNode, screen, diagnostic);
}

void TestDefaults() {
    RoboScreenRecord screen;
    std::string diagnostic;
    const bool ok = Read("<Screen/>", screen, diagnostic);
    Check(ok, "a bare Screen is valid");
    Check(diagnostic.empty(), "a bare Screen has no diagnostic");
    Check(screen.tag == "Screen", "the tag resolves to Screen");
    Check(screen.title.empty(), "the default title is empty");
    Check(screen.path == "/", "the default path is /");
    Check(screen.theme == "dark", "the default theme is dark");
    Check(screen.place == "center", "the default place is center");
    Check(screen.state == "normal", "the default window state is normal");
    Check(screen.width == 640 && screen.height == 400, "the default size is 640 by 400");
    Check(screen.minWidth == 320 && screen.minHeight == 200, "the default minimum is 320 by 200");
    Check(screen.maxWidth == 1920 && screen.maxHeight == 1080, "the default maximum is 1920 by 1080");
    Check(screen.color == RoboFoundationPlatformColorForTheme("bg-primary", ROBO_FALSE), "dark theme uses the dark background");
}

void TestOwnAttributes() {
    RoboScreenRecord screen;
    std::string diagnostic;
    const bool ok = Read(
        "<Screen Title=\"Robot\" Width=\"800\" Height=\"500\" MinWidth=\"400\" MinHeight=\"300\" "
        "MaxWidth=\"1000\" MaxHeight=\"700\" Theme=\"light\" Background=\"#0E7AFF\" Place=\"top-left\" "
        "State=\"maximized\"/>",
        screen, diagnostic);
    Check(ok, "a fully described Screen is valid");
    Check(screen.title == "Robot", "title is kept");
    Check(screen.width == 800 && screen.height == 500, "size is kept");
    Check(screen.minWidth == 400 && screen.minHeight == 300, "minimum size is kept");
    Check(screen.maxWidth == 1000 && screen.maxHeight == 700, "maximum size is kept");
    Check(screen.theme == "light", "theme is kept");
    Check(screen.place == "top-left", "place is kept");
    Check(screen.state == "maximized", "window state is kept");
    Check(screen.color == 0xFF0E7AFFu, "background #0E7AFF paints that color");
}

void TestAliases() {
    RoboScreenRecord screen;
    std::string diagnostic;
    const bool ok = Read("<Window Name=\"Desk\" Appearance=\"light\" Bg=\"#112233\" Route=\"/home\"/>", screen, diagnostic);
    Check(ok, "Window is a Screen");
    Check(screen.tag == "Screen", "Window resolves to Screen");
    Check(screen.title == "Desk", "name is the title");
    Check(screen.theme == "light", "appearance is the theme");
    Check(screen.color == 0xFF112233u, "bg is the background");
    Check(screen.path == "/home", "route is the path");

    const bool page = Read("<Page Title=\"Page\" State=\"closed\"/>", screen, diagnostic);
    Check(page, "Page is a Screen");
    Check(screen.tag == "Screen", "Page resolves to Screen");
    Check(screen.title == "Page", "a Page keeps its title");
    Check(screen.state == "closed", "a Page keeps its window state");
}

void TestTitleWinsOverName() {
    RoboScreenRecord screen;
    std::string diagnostic;
    const bool ok = Read("<Screen Title=\"A\" Name=\"B\"/>", screen, diagnostic);
    Check(ok, "title and name together are valid");
    Check(screen.title == "A", "title wins when name is also set");
}

void TestNestedFind() {
    RoboScreenRecord screen;
    std::string diagnostic;
    const bool ok = Read("<RobixUI><Page Title=\"Nested\" Width=\"700\" Height=\"420\"/></RobixUI>", screen, diagnostic);
    Check(ok, "a nested Page is the screen");
    Check(screen.title == "Nested", "the nested title is read");
    Check(screen.width == 700 && screen.height == 420, "the nested size is read");

    RoboXMLDocument document;
    RoboXMLParse("<Text Value=\"Hi\"/>", document);
    Check(RoboScreenFind(document.root) == nullptr, "Text is not a Screen");
}

void TestRejectedAttributes() {
    RoboScreenRecord screen;
    std::string diagnostic;
    Check(!Read("<Screen NotAReal=\"1\"/>", screen, diagnostic), "an unknown attribute is rejected");
    Check(diagnostic.find("R7001") != std::string::npos, "an unknown attribute uses R7001");
    Check(diagnostic.find("NotAReal") != std::string::npos, "the diagnostic names the attribute");

    Check(!Read("<Screen Theme=\"blue\"/>", screen, diagnostic), "an unknown theme is rejected");
    Check(diagnostic.find("R7002") != std::string::npos, "a bad theme uses R7002");

    Check(!Read("<Screen Place=\"middle\"/>", screen, diagnostic), "an unknown place is rejected");
    Check(diagnostic.find("place") != std::string::npos, "a bad place names place");

    Check(!Read("<Screen State=\"hidden\"/>", screen, diagnostic), "an unknown window state is rejected");
    Check(diagnostic.find("state") != std::string::npos, "a bad state names state");

    Check(!Read("<Screen Background=\"blue\"/>", screen, diagnostic), "a named color is rejected");
    Check(diagnostic.find("R7002") != std::string::npos, "a bad color uses R7002");

    Check(!Read("<Screen Width=\"640px\"/>", screen, diagnostic), "a non-integer size is rejected");
    Check(diagnostic.find("R7003") != std::string::npos, "a bad size uses R7003");

    Check(!Read("<Screen Width=\"-1\"/>", screen, diagnostic), "a negative size is rejected");
    Check(diagnostic.find("negative") != std::string::npos, "a negative size says it is negative");

    Check(!Read("<Screen MinWidth=\"2000\"/>", screen, diagnostic), "a minimum above the maximum is rejected");
    Check(diagnostic.find("R7005") != std::string::npos, "a crossed minimum uses R7005");

    Check(!Read("<Screen MinSize=\"nope\"/>", screen, diagnostic), "a bad minSize is rejected");

    RoboXMLDocument textDocument;
    Check(RoboXMLParse("<Text/>", textDocument), "a Text document parses");
    Check(!RoboScreenRead(textDocument.root, screen, diagnostic), "Text cannot be read as a Screen");
    Check(diagnostic.find("R7004") != std::string::npos, "the wrong tag uses R7004");
}

void TestClampAndMinSize() {
    RoboScreenRecord screen;
    std::string diagnostic;
    Check(Read("<Screen Width=\"10\" Height=\"10\"/>", screen, diagnostic), "a tiny size is still a screen");
    Check(screen.width == 320 && screen.height == 200, "size clamps up to the minimum");

    Check(Read("<Screen Width=\"4000\" Height=\"4000\"/>", screen, diagnostic), "a huge size is still a screen");
    Check(screen.width == 1920 && screen.height == 1080, "size clamps down to the maximum");

    Check(Read("<Screen MinSize=\"100 150\" Width=\"50\" Height=\"50\"/>", screen, diagnostic), "minSize is a size");
    Check(screen.minWidth == 100 && screen.minHeight == 150, "minSize sets both edges");
    Check(screen.width == 100 && screen.height == 150, "size clamps to minSize");

    Check(Read("<Screen MinSize=\"100\" MinWidth=\"180\" Width=\"50\"/>", screen, diagnostic), "minWidth overrides minSize");
    Check(screen.minWidth == 180 && screen.minHeight == 100, "the explicit edge wins");
    Check(screen.width == 180, "width clamps to the explicit minimum");

    Check(Read("<Screen MaxSize=\"900, 600\" Width=\"2000\" Height=\"2000\"/>", screen, diagnostic), "maxSize is a size");
    Check(screen.maxWidth == 900 && screen.maxHeight == 600, "maxSize sets both edges");
    Check(screen.width == 900 && screen.height == 600, "size clamps to maxSize");
}

void TestTransparentAndShortColor() {
    RoboScreenRecord screen;
    std::string diagnostic;
    Check(Read("<Screen Transparent=\"true\" Background=\"#0E7AFF\"/>", screen, diagnostic), "transparent is valid");
    Check(screen.transparent, "transparent is recorded");
    Check(screen.color == 0, "a transparent screen paints nothing");

    Check(Read("<Screen Background=\"#ABC\"/>", screen, diagnostic), "a short color is valid");
    Check(screen.color == 0xFFAABBCCu, "a short color expands");

    Check(Read("<Screen Background=\"#80ABCDEF\"/>", screen, diagnostic), "a color with alpha is valid");
    Check(screen.color == 0x80ABCDEFu, "alpha is kept");

    Check(Read("<Screen Theme=\"light\"/>", screen, diagnostic), "light theme is valid");
    Check(screen.color == RoboFoundationPlatformColorForTheme("bg-primary", ROBO_TRUE), "light theme uses the light background");
}

void TestPaint() {
    RoboScreenRecord screen;
    std::string diagnostic;
    Check(Read("<Screen MinWidth=\"1\" MinHeight=\"1\" Width=\"84\" Height=\"44\" Background=\"#0E7AFF\"/>", screen, diagnostic),
          "the paint screen is valid");
    RoboScreenFrame frame;
    RoboScreenPaint(screen, frame);
    Check(frame.width == 84 && frame.height == 44, "the frame size matches the screen");
    Check(frame.pixels.size() == 84u * 44u, "the frame has one pixel per cell");
    RoboU32 origin = 0;
    RoboU32 corner = 1;
    Check(RoboScreenPixel(frame, 0, 0, origin), "the origin is inside the frame");
    Check(origin == 0xFF0E7AFFu, "the origin pixel is the screen color");
    Check(RoboScreenPixel(frame, 83, 43, corner), "the far corner is inside the frame");
    Check(corner == origin, "the far corner is the same color");
    RoboU32 outside = 5;
    Check(!RoboScreenPixel(frame, 84, 0, outside), "a pixel past the width is outside");
    Check(!RoboScreenPixel(frame, 0, 44, outside), "a pixel past the height is outside");
    Check(outside == 5, "an outside read leaves the color alone");

    Check(Read("<Screen Width=\"20\" Height=\"10\" Background=\"#010203\"><Text Value=\"Hi\"/></Screen>", screen, diagnostic),
          "children do not make the screen invalid");
    RoboScreenPaint(screen, frame);
    RoboU32 pixel = 0;
    Check(RoboScreenPixel(frame, 0, 0, pixel) && pixel == 0xFF010203u, "children are not painted in this stage");
}

void TestHostOptions() {
    RoboScreenRecord screen;
    std::string diagnostic;
    Check(Read("<Window Title=\"Roboticela\" Width=\"700\" Height=\"420\" Place=\"left\" Theme=\"light\" "
               "Background=\"#0E7AFF\" Movable=\"false\" Resizable=\"false\" State=\"minimized\" X=\"12\" Y=\"8\"/>",
               screen, diagnostic),
          "the host document is a Window");
    Check(screen.tag == "Screen", "the host window is a Screen");
    Check(screen.title == "Roboticela", "the host title comes from the document");
    Check(screen.width == 700 && screen.height == 420, "the host size comes from the document");
    Check(screen.place == "left", "the host place comes from the document");
    Check(screen.theme == "light", "the host theme comes from the document");
    Check(screen.color == 0xFF0E7AFFu, "the host clears to the screen color");
    Check(!screen.movable && !screen.resizable, "the host keeps movable and resizable");
    Check(screen.state == "minimized", "the host keeps the window state");
    Check(screen.x == 12 && screen.y == 8, "the host keeps the window origin");

    RoboScreenFrame frame;
    RoboScreenPaint(screen, frame);
    RoboU32 origin = 0;
    Check(frame.width == screen.width && frame.height == screen.height, "the host frame uses the document size");
    Check(RoboScreenPixel(frame, 0, 0, origin) && origin == screen.color, "the host frame origin is the screen color");
}

}  // namespace

int main() {
    TestDefaults();
    TestOwnAttributes();
    TestAliases();
    TestTitleWinsOverName();
    TestNestedFind();
    TestRejectedAttributes();
    TestClampAndMinSize();
    TestTransparentAndShortColor();
    TestPaint();
    TestHostOptions();
    if (gFailed != 0) {
        std::cerr << gFailed << " failed, " << gPassed << " passed\n";
        return 1;
    }
    std::cout << gPassed << " passed\n";
    return 0;
}
