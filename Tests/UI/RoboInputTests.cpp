#include "../../RobixUI/Compiler/Components/Input/RoboInput.hpp"
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

bool Load(const std::string& source, RoboInputNode& input, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboInputRead(document.root, input, diagnostic);
}

void TestSchema() {
    RoboInputNode input;
    std::string diagnostic;
    Check(Load("<Input Value=\"Hi\" Placeholder=\"Name\" Mask=\"none\" Multiline=\"false\" Disabled=\"false\" CursorColor=\"#FF0000\" Width=\"64\" Height=\"24\"/>",
               input, diagnostic),
          "an input document is accepted");
    Check(input.value == "Hi" && input.placeholder == "Name" && !input.multiline && !input.disabled && input.cursorColor == 0xFFFF0000u &&
              input.width == 64 && input.height == 24,
          "value, placeholder, mask, multiline, disabled, and CursorColor are kept");

    RoboInputNode field;
    Check(Load("<TextField Value=\"Hi\" Placeholder=\"Name\" Width=\"32\" Height=\"16\" CursorColor=\"#00FF00\"/>", field, diagnostic),
          "TextField resolves to Input");
    Check(field.value == "Hi" && field.placeholder == "Name" && field.cursorColor == 0xFF00FF00u, "TextField keeps the same fields");

    Check(!Load("<Input Value=\"Hi\" Width=\"64\" Height=\"24\" Tilt=\"1\"/>", input, diagnostic) && diagnostic.find("R8001") != std::string::npos,
          "an unknown attribute is rejected");
    Check(!Load("<Input Value=\"Hi\" Width=\"64\" Height=\"24\" CursorColor=\"red\"/>", input, diagnostic) && diagnostic.find("R8002") != std::string::npos,
          "a bad caret color is rejected");
    Check(!Load("<Input Value=\"Hi\" Width=\"64\" Height=\"24\" Mask=\"secret\"/>", input, diagnostic) && diagnostic.find("R8002") != std::string::npos,
          "a bad mask is rejected");
    Check(!Load("<Input Value=\"Hi\" Width=\"0\" Height=\"24\"/>", input, diagnostic) && diagnostic.find("R8003") != std::string::npos,
          "a bad size is rejected");
    Check(!Load("<Screen/>", input, diagnostic) && diagnostic.find("R8004") != std::string::npos, "the wrong tag is rejected");
}

void TestKeyboard() {
    RoboInputNode input;
    std::string diagnostic;
    Check(Load("<TextField Width=\"80\" Height=\"32\" CursorColor=\"#FF0000\"/>", input, diagnostic), "an empty field is accepted");
    Check(RoboInputType(input, "a") && RoboInputType(input, "b") && RoboInputType(input, "c"), "three characters are typed");
    Check(input.value == "abc" && input.caret == 3, "the value is the three characters");
    Check(RoboInputDelete(input, false) && input.value == "ab" && input.caret == 2, "backspace deletes the character before the caret");
    Check(RoboInputType(input, "c") && input.value == "abc", "the third character is typed again");
    RoboInputSelect(input, 1, 3);
    Check(RoboInputType(input, "Z") && input.value == "aZ" && input.caret == 2, "typing replaces the selection");
    Check(RoboInputDelete(input, true) == false && input.value == "aZ", "delete at the end changes nothing");
    Check(!RoboInputType(input, "\n") && input.value == "aZ", "a single-line field drops a newline");

    RoboInputNode lines;
    Check(Load("<Input Multiline=\"true\" Width=\"40\" Height=\"40\"/>", lines, diagnostic), "a multiline field is accepted");
    Check(RoboInputType(lines, "a\nb"), "a newline is typed into a multiline field");
    Check(lines.value == "a\nb" && lines.caret == 3, "the multiline value keeps the newline");

    RoboInputNode locked;
    Check(Load("<Input Value=\"no\" Disabled=\"true\" Width=\"40\" Height=\"20\"/>", locked, diagnostic), "a disabled field is accepted");
    Check(!RoboInputType(locked, "x") && !RoboInputDelete(locked, false) && locked.value == "no", "a disabled field refuses the keyboard");
}

void TestPaint() {
    RoboInputNode input;
    std::string diagnostic;
    Check(Load("<TextField Width=\"80\" Height=\"32\" CursorColor=\"#FF0000\"/>", input, diagnostic), "the paint field is accepted");
    Check(RoboInputType(input, "a") && RoboInputType(input, "b") && RoboInputType(input, "c"), "the paint field receives three characters");
    RoboInputFrame frame;
    RoboInputPaint(input, frame);
    RoboU32 color = 0;
    Check(RoboInputPixel(frame, 24, 4, color) && color == 0xFFFF0000u, "the caret uses CursorColor");
    Check(RoboInputPixel(frame, 4, 6, color) && color == 0xFF111111u, "the typed text is painted");
    Check(RoboInputPixel(frame, 70, 2, color) && color == ROBO_INPUT_PLATE, "the empty plate stays clear");
    RoboInputSelect(input, 0, 2);
    RoboInputPaint(input, frame);
    Check(RoboInputPixel(frame, 1, 1, color) && color == ROBO_INPUT_SELECTION, "the selection part is painted");
    Check(RoboInputPixel(frame, 16, 4, color) && color == 0xFFFF0000u, "the caret sits at the end of the selection");

    RoboInputNode secret;
    Check(Load("<Input Mask=\"password\" Width=\"40\" Height=\"20\" CursorColor=\"#0000FF\"/>", secret, diagnostic), "a password field is accepted");
    Check(RoboInputType(secret, "ab") && secret.value == "ab", "the password value stays the typed text");
    RoboInputPaint(secret, frame);
    Check(RoboInputPixel(frame, 3, 8, color) && color == ROBO_INPUT_BULLET, "the password mask paints a bullet");
    Check(!RoboInputPixel(frame, 40, 0, color), "a pixel outside the field is rejected");
}

}  // namespace

int main() {
    TestSchema();
    TestKeyboard();
    TestPaint();
    if (gFailed != 0) {
        std::cerr << gFailed << " failed, " << gPassed << " passed\n";
        return 1;
    }
    std::cout << gPassed << " passed\n";
    return 0;
}
