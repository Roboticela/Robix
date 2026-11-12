#include "../../RobixUI/Compiler/Components/ScrollView/RoboScrollView.hpp"
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

bool Load(const std::string& source, RoboScrollViewNode& view, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboScrollViewRead(document.root, view, diagnostic);
}

void TestSchema() {
    RoboScrollViewNode view;
    std::string diagnostic;
    Check(Load("<ScrollView Axis=\"vertical\" Offset=\"10\" Width=\"40\" Height=\"20\" ContentWidth=\"40\" ContentHeight=\"60\" ChildColor=\"#0000FF\"/>",
               view, diagnostic),
          "a scroll view document is accepted");
    Check(view.axis == "vertical" && view.offsetY == 10 && view.childOriginX == 0 && view.childOriginY == -10 && view.childColor == 0xFF0000FFu,
          "axis and offset set the child origin");
    Check(view.verticalBar && view.trackWidth == ROBO_SCROLL_BAR && view.thumbHeight > 0, "the track and the thumb are parts");

    Check(!Load("<ScrollView Width=\"40\" Height=\"20\" ContentWidth=\"40\" ContentHeight=\"60\" Tilt=\"1\"/>", view, diagnostic) &&
              diagnostic.find("R8101") != std::string::npos,
          "an unknown attribute is rejected");
    Check(!Load("<ScrollView Axis=\"diagonal\" Width=\"40\" Height=\"20\" ContentWidth=\"40\" ContentHeight=\"60\"/>", view, diagnostic) &&
              diagnostic.find("R8102") != std::string::npos,
          "a bad axis is rejected");
    Check(!Load("<ScrollView Offset=\"-4\" Width=\"40\" Height=\"20\" ContentWidth=\"40\" ContentHeight=\"60\"/>", view, diagnostic) &&
              diagnostic.find("R8102") != std::string::npos,
          "a bad offset is rejected");
    Check(!Load("<ScrollView Width=\"0\" Height=\"20\" ContentWidth=\"40\" ContentHeight=\"60\"/>", view, diagnostic) &&
              diagnostic.find("R8103") != std::string::npos,
          "a bad size is rejected");
    Check(!Load("<Screen/>", view, diagnostic) && diagnostic.find("R8104") != std::string::npos, "the wrong tag is rejected");
}

void TestScroll() {
    RoboScrollViewNode view;
    std::string diagnostic;
    Check(Load("<ScrollView Width=\"40\" Height=\"20\" ContentWidth=\"40\" ContentHeight=\"60\"/>", view, diagnostic), "a vertical view is accepted");
    Check(view.childOriginX == 0 && view.childOriginY == 0, "the child starts at the origin");
    Check(RoboScrollViewWheel(view, 0, 16) && view.offsetY == 16 && view.childOriginY == -16 && view.childOriginX == 0,
          "a wheel delta moves the child origin");
    Check(!RoboScrollViewWheel(view, 12, 0) && view.offsetX == 0 && view.childOriginX == 0, "a horizontal wheel does nothing on a vertical view");
    Check(RoboScrollViewWheel(view, 0, 100) && view.offsetY == 40 && view.childOriginY == -40, "the offset stops at the end of the content");
    Check(!RoboScrollViewWheel(view, 0, 8) && view.offsetY == 40, "a wheel at the end does not move");

    RoboScrollViewNode fresh;
    Check(Load("<ScrollView Width=\"40\" Height=\"20\" ContentWidth=\"40\" ContentHeight=\"60\"/>", fresh, diagnostic), "a drag view is accepted");
    Check(fresh.thumbX == 34 && fresh.thumbY == 0 && fresh.thumbWidth == 6 && fresh.thumbHeight == 8, "the thumb starts at the top of the track");
    Check(RoboScrollViewDrag(fresh, 35, 2, 35, 8) && fresh.offsetY == 20 && fresh.childOriginY == -20, "dragging the thumb scrolls by a known delta");
    Check(!RoboScrollViewDrag(fresh, 2, 2, 2, 10) && fresh.offsetY == 20, "a drag that misses the thumb does nothing");

    RoboScrollViewNode across;
    Check(Load("<ScrollView Axis=\"horizontal\" Width=\"20\" Height=\"16\" ContentWidth=\"80\" ContentHeight=\"16\"/>", across, diagnostic),
          "a horizontal view is accepted");
    Check(RoboScrollViewWheel(across, 10, 4) && across.offsetX == 10 && across.childOriginX == -10 && across.offsetY == 0,
          "a horizontal wheel moves the child origin on x");
    Check(across.horizontalBar && across.thumbY == 10, "the horizontal thumb sits on the bottom track");

    RoboScrollViewNode both;
    Check(Load("<ScrollView Axis=\"both\" OffsetX=\"4\" OffsetY=\"6\" Width=\"30\" Height=\"20\" ContentWidth=\"90\" ContentHeight=\"80\"/>", both, diagnostic),
          "both axes are accepted");
    Check(both.childOriginX == -4 && both.childOriginY == -6, "both offsets set both child origins");
    Check(RoboScrollViewWheel(both, 0, 2) && both.offsetY == 8 && both.offsetX == 4, "a vertical wheel on both axes changes only y");
}

void TestPaint() {
    RoboScrollViewNode view;
    std::string diagnostic;
    Check(Load("<ScrollView Width=\"40\" Height=\"20\" ContentWidth=\"40\" ContentHeight=\"60\" ChildColor=\"#FF0000\"/>", view, diagnostic),
          "the paint view is accepted");
    RoboScrollViewFrame frame;
    RoboScrollViewPaint(view, frame);
    RoboU32 color = 0;
    Check(RoboScrollViewPixel(frame, 1, 1, color) && color == ROBO_SCROLL_MARKER, "the child marker is at the origin");
    Check(RoboScrollViewPixel(frame, 8, 8, color) && color == 0xFFFF0000u, "the child fills the viewport");
    Check(RoboScrollViewPixel(frame, 35, 2, color) && color == ROBO_SCROLL_THUMB, "the thumb is painted");
    Check(RoboScrollViewPixel(frame, 35, 16, color) && color == ROBO_SCROLL_TRACK, "the track is painted past the thumb");
    Check(RoboScrollViewWheel(view, 0, 10) && view.childOriginY == -10, "the paint view scrolls by ten pixels");
    RoboScrollViewPaint(view, frame);
    Check(RoboScrollViewPixel(frame, 1, 1, color) && color == 0xFFFF0000u, "the scrolled marker is clipped out of the viewport");
    Check(RoboScrollViewPixel(frame, 8, 8, color) && color == 0xFFFF0000u, "the child still fills the clipped viewport");
    Check(!RoboScrollViewPixel(frame, 40, 0, color), "a pixel outside the viewport is rejected");
}

}  // namespace

int main() {
    TestSchema();
    TestScroll();
    TestPaint();
    if (gFailed != 0) {
        std::cerr << gFailed << " failed, " << gPassed << " passed\n";
        return 1;
    }
    std::cout << gPassed << " passed\n";
    return 0;
}
