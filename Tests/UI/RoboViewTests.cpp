#include "../../RobixUI/Compiler/Components/View/RoboView.hpp"
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

bool Load(const std::string& source, RoboViewNode& view, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    if (!RoboViewRead(document.root, view, diagnostic)) {
        return false;
    }
    RoboViewLayout(view);
    return true;
}

const RoboViewNode* Child(const RoboViewNode& view, int index) {
    return &view.children[static_cast<std::size_t>(index)];
}

void TestSchemaAndAlias() {
    RoboViewNode view;
    std::string diagnostic;
    Check(Load("<View Id=\"box\" Width=\"40\" Height=\"20\" Background=\"#0E7AFF\"/>", view, diagnostic), "a View is valid");
    Check(view.tag == "View", "the tag is View");
    Check(view.usedW == 40 && view.usedH == 20, "the View keeps its size");
    Check(view.color == 0xFF0E7AFFu, "the View keeps its background");

    Check(Load("<Container Id=\"alias\" Width=\"30\" Height=\"16\" Bg=\"#112233\"/>", view, diagnostic), "Container is a View");
    Check(view.tag == "View", "Container resolves to View");
    Check(view.usedW == 30 && view.usedH == 16, "Container keeps its size");
    Check(view.color == 0xFF112233u, "bg is the background");

    Check(Load("<Box Width=\"12\" Height=\"8\"/>", view, diagnostic), "Box is a View");
    Check(view.tag == "View", "Box resolves to View");

    Check(!Load("<View NotAReal=\"1\"/>", view, diagnostic), "an unknown attribute is rejected");
    Check(diagnostic.find("R7101") != std::string::npos, "an unknown attribute uses R7101");
    Check(!Load("<View Direction=\"diagonal\"/>", view, diagnostic), "a bad direction is rejected");
    Check(diagnostic.find("R7102") != std::string::npos, "a bad direction uses R7102");
    Check(!Load("<View Width=\"10px\"/>", view, diagnostic), "a bad size is rejected");
    Check(diagnostic.find("R7103") != std::string::npos, "a bad size uses R7103");
    Check(!Load("<Text/>", view, diagnostic), "Text is not a View");
    Check(diagnostic.find("R7104") != std::string::npos, "the wrong tag uses R7104");

    Check(Load("<View Width=\"10\" Height=\"10\"><Text Value=\"Hi\"/></View>", view, diagnostic), "a Text child is left for a later stage");
    Check(view.children.empty(), "the Text child is not a box");
}

void TestRowColumnAndReverse() {
    RoboViewNode view;
    std::string diagnostic;
    Check(Load(
              "<View Direction=\"row\" Width=\"100\" Height=\"20\" Align=\"start\">"
              "<View Id=\"a\" Width=\"20\" Height=\"10\" Background=\"#FF0000\"/>"
              "<View Id=\"b\" Width=\"20\" Height=\"10\" Background=\"#00FF00\"/>"
              "<View Id=\"c\" Width=\"20\" Height=\"10\" Background=\"#0000FF\"/>"
              "</View>",
              view, diagnostic),
          "a row of three boxes lays out");
    Check(Child(view, 0)->boxX == 0 && Child(view, 0)->boxY == 0, "the first box starts at the origin");
    Check(Child(view, 1)->boxX == 20 && Child(view, 1)->boxY == 0, "the second box starts after the first");
    Check(Child(view, 2)->boxX == 40 && Child(view, 2)->boxY == 0, "the third box starts after the second");

    RoboViewFrame frame;
    RoboViewPaint(view, frame);
    RoboU32 pixel = 0;
    Check(frame.width == 100 && frame.height == 20, "the row frame matches the parent");
    Check(RoboViewPixel(frame, 0, 0, pixel) && pixel == 0xFFFF0000u, "the first origin pixel is red");
    Check(RoboViewPixel(frame, 20, 0, pixel) && pixel == 0xFF00FF00u, "the second origin pixel is green");
    Check(RoboViewPixel(frame, 40, 0, pixel) && pixel == 0xFF0000FFu, "the third origin pixel is blue");

    Check(Load(
              "<View Direction=\"column\" Width=\"20\" Height=\"80\" Align=\"start\">"
              "<View Id=\"a\" Width=\"10\" Height=\"15\"/>"
              "<View Id=\"b\" Width=\"10\" Height=\"15\"/>"
              "<View Id=\"c\" Width=\"10\" Height=\"15\"/>"
              "</View>",
              view, diagnostic),
          "a column lays out");
    Check(Child(view, 0)->boxY == 0 && Child(view, 1)->boxY == 15 && Child(view, 2)->boxY == 30, "column origins step downward");

    Check(Load(
              "<View Direction=\"row-reverse\" Width=\"100\" Height=\"10\" Align=\"start\">"
              "<View Id=\"a\" Width=\"10\" Height=\"10\"/>"
              "<View Id=\"b\" Width=\"10\" Height=\"10\"/>"
              "<View Id=\"c\" Width=\"10\" Height=\"10\"/>"
              "</View>",
              view, diagnostic),
          "a reversed row lays out");
    Check(Child(view, 0)->boxX == 90, "the first reversed box sits on the right");
    Check(Child(view, 1)->boxX == 80 && Child(view, 2)->boxX == 70, "the later reversed boxes step left");

    Check(Load(
              "<View Direction=\"column-reverse\" Width=\"10\" Height=\"50\" Align=\"start\">"
              "<View Id=\"a\" Width=\"10\" Height=\"10\"/>"
              "<View Id=\"b\" Width=\"10\" Height=\"10\"/>"
              "<View Id=\"c\" Width=\"10\" Height=\"10\"/>"
              "</View>",
              view, diagnostic),
          "a reversed column lays out");
    Check(Child(view, 0)->boxY == 40, "the first reversed column box sits on the bottom");
    Check(Child(view, 1)->boxY == 30 && Child(view, 2)->boxY == 20, "the later reversed boxes step up");
}

void TestStackGridAndPosition() {
    RoboViewNode view;
    std::string diagnostic;
    Check(Load(
              "<View Display=\"stack\" Width=\"20\" Height=\"20\" Background=\"#0000FF\">"
              "<View Id=\"back\" Width=\"20\" Height=\"20\" Background=\"#FF0000\"/>"
              "<View Id=\"front\" Width=\"10\" Height=\"10\" Background=\"#00FF00\"/>"
              "</View>",
              view, diagnostic),
          "a stack lays out");
    Check(Child(view, 0)->boxX == 0 && Child(view, 1)->boxX == 0, "stack children share a cell");
    Check(Child(view, 0)->boxY == 0 && Child(view, 1)->boxY == 0, "stack children share the origin");
    RoboViewFrame frame;
    RoboViewPaint(view, frame);
    RoboU32 pixel = 0;
    Check(RoboViewPixel(frame, 0, 0, pixel) && pixel == 0xFF00FF00u, "the later stack child paints above");
    Check(RoboViewPixel(frame, 15, 15, pixel) && pixel == 0xFFFF0000u, "the earlier stack child remains outside the front box");

    Check(Load(
              "<View Display=\"grid\" Columns=\"3\" Width=\"90\" Height=\"30\" Gap=\"0\">"
              "<View Id=\"wide\" GridColumn=\"span 2\" Height=\"10\" Background=\"#FF0000\"/>"
              "<View Id=\"rest\" Height=\"10\" Background=\"#00FF00\"/>"
              "<View Id=\"next\" Height=\"10\" Background=\"#0000FF\"/>"
              "</View>",
              view, diagnostic),
          "a grid lays out");
    Check(Child(view, 0)->usedW == 60 && Child(view, 0)->boxX == 0, "a span of two columns is twice a cell");
    Check(Child(view, 1)->boxX == 60, "the next cell starts after the span");
    Check(Child(view, 2)->boxX == 0 && Child(view, 2)->boxY == 10, "the third cell wraps to the next row");

    Check(Load(
              "<View Width=\"100\" Height=\"50\" Background=\"#0000FF\">"
              "<View Id=\"flow\" Width=\"10\" Height=\"10\" Background=\"#FF0000\"/>"
              "<View Id=\"float\" Position=\"absolute\" Left=\"40\" Top=\"6\" Width=\"10\" Height=\"10\" Background=\"#00FF00\"/>"
              "</View>",
              view, diagnostic),
          "an absolute child is positioned");
    Check(Child(view, 0)->boxX == 0, "the flow child stays at the start");
    Check(Child(view, 1)->boxX == 40 && Child(view, 1)->boxY == 6, "left and top place the absolute child");

    Check(Load(
              "<View Width=\"100\" Height=\"40\">"
              "<View Position=\"absolute\" Right=\"4\" Bottom=\"3\" Width=\"10\" Height=\"8\"/>"
              "</View>",
              view, diagnostic),
          "right and bottom place an absolute child");
    Check(Child(view, 0)->boxX == 86 && Child(view, 0)->boxY == 29, "the absolute child sits inside the far edges");
}

void TestPaddingMarginAlignmentSpacing() {
    RoboViewNode view;
    std::string diagnostic;
    Check(Load(
              "<View Direction=\"row\" Width=\"80\" Height=\"40\" Padding=\"8\" Align=\"start\">"
              "<View Id=\"a\" Width=\"10\" Height=\"10\"/>"
              "</View>",
              view, diagnostic),
          "padding lays out");
    Check(Child(view, 0)->boxX == 8 && Child(view, 0)->boxY == 8, "padding insets the child on both edges");

    Check(Load(
              "<View Direction=\"row\" Width=\"80\" Height=\"40\" PaddingTop=\"2\" PaddingRight=\"3\" PaddingBottom=\"4\" PaddingLeft=\"5\" Align=\"start\">"
              "<View Width=\"10\" Height=\"10\"/>"
              "</View>",
              view, diagnostic),
          "padding edges lay out");
    Check(Child(view, 0)->boxX == 5 && Child(view, 0)->boxY == 2, "each padding edge is honored");

    Check(Load(
              "<View Direction=\"row\" Width=\"80\" Height=\"30\" Align=\"start\" Gap=\"0\">"
              "<View Width=\"10\" Height=\"10\" Margin=\"4\"/>"
              "<View Width=\"10\" Height=\"10\" MarginLeft=\"6\"/>"
              "</View>",
              view, diagnostic),
          "margin lays out");
    Check(Child(view, 0)->boxX == 4, "margin insets the first box");
    Check(Child(view, 1)->boxX == 4 + 10 + 4 + 6, "margin separates the next box");

    Check(Load(
              "<View Direction=\"row\" Width=\"50\" Height=\"40\" Align=\"center\" Gap=\"0\">"
              "<View Width=\"10\" Height=\"10\"/>"
              "</View>",
              view, diagnostic),
          "alignment lays out");
    Check(Child(view, 0)->boxY == 15, "center alignment places the cross axis");

    Check(Load(
              "<View Direction=\"row\" Width=\"30\" Height=\"10\" Justify=\"center\" Align=\"start\">"
              "<View Width=\"10\" Height=\"10\"/>"
              "</View>",
              view, diagnostic),
          "main-axis alignment lays out");
    Check(Child(view, 0)->boxX == 10, "center justification places the main axis");

    Check(Load(
              "<View Direction=\"row\" Width=\"50\" Height=\"10\" Justify=\"space-between\" Align=\"start\" Gap=\"0\">"
              "<View Width=\"10\" Height=\"10\"/>"
              "<View Width=\"10\" Height=\"10\"/>"
              "<View Width=\"10\" Height=\"10\"/>"
              "</View>",
              view, diagnostic),
          "space-between lays out");
    Check(Child(view, 0)->boxX == 0 && Child(view, 1)->boxX == 20 && Child(view, 2)->boxX == 40, "space-between splits the free space");

    Check(Load(
              "<View Direction=\"row\" Width=\"50\" Height=\"10\" Justify=\"space-evenly\" Align=\"start\" Gap=\"0\">"
              "<View Width=\"10\" Height=\"10\"/>"
              "<View Width=\"10\" Height=\"10\"/>"
              "<View Width=\"10\" Height=\"10\"/>"
              "</View>",
              view, diagnostic),
          "space-evenly lays out");
    Check(Child(view, 0)->boxX == 5 && Child(view, 1)->boxX == 20 && Child(view, 2)->boxX == 35, "space-evenly shares every gap");

    Check(Load(
              "<View Direction=\"row\" Width=\"80\" Height=\"10\" Gap=\"10\" Align=\"start\">"
              "<View Width=\"10\" Height=\"10\"/>"
              "<View Width=\"10\" Height=\"10\"/>"
              "</View>",
              view, diagnostic),
          "spacing lays out");
    Check(Child(view, 1)->boxX == 20, "gap sits between the boxes");
}

void TestSizeConstraintsOverflowFlexAspect() {
    RoboViewNode view;
    std::string diagnostic;
    Check(Load(
              "<View Direction=\"row\" Size=\"content\" Gap=\"4\" Align=\"start\">"
              "<View Width=\"10\" Height=\"8\"/>"
              "<View Width=\"12\" Height=\"6\"/>"
              "</View>",
              view, diagnostic),
          "content size lays out");
    Check(view.usedW == 26 && view.usedH == 8, "content size hugs the children");

    Check(Load(
              "<View Width=\"100\" Height=\"40\">"
              "<View Fill=\"true\" Background=\"#FF0000\"/>"
              "</View>",
              view, diagnostic),
          "fill size lays out");
    Check(Child(view, 0)->usedW == 100 && Child(view, 0)->usedH == 40, "fill takes the parent");

    Check(Load("<View Width=\"10\" Height=\"8\" MinWidth=\"30\" MinHeight=\"12\"/>", view, diagnostic), "minimum size lays out");
    Check(view.usedW == 30 && view.usedH == 12, "the minimum raises the size");
    Check(Load("<View Width=\"100\" Height=\"40\" MaxWidth=\"36\" MaxHeight=\"18\"/>", view, diagnostic), "maximum size lays out");
    Check(view.usedW == 36 && view.usedH == 18, "the maximum lowers the size");
    Check(Load("<View Width=\"100\" Height=\"80\" Constraints=\"10 12 40 22\"/>", view, diagnostic), "constraints lay out");
    Check(view.usedW == 40 && view.usedH == 22, "constraints clamp both axes");
    Check(view.minWidth == 10 && view.minHeight == 12, "constraints keep the minimum");

    Check(Load("<View Width=\"40\" AspectRatio=\"2\" Background=\"#FF0000\"/>", view, diagnostic), "aspect ratio from width lays out");
    Check(view.usedW == 40 && view.usedH == 20, "aspect ratio sets the free height");
    Check(Load("<View Height=\"18\" AspectRatio=\"16:9\"/>", view, diagnostic), "aspect ratio from height lays out");
    Check(view.usedW == 32 && view.usedH == 18, "16:9 sets the free width");

    Check(Load(
              "<View Direction=\"row\" Width=\"100\" Height=\"20\" Align=\"start\">"
              "<View Width=\"10\" Height=\"10\" Grow=\"1\" Background=\"#FF0000\"/>"
              "<View Width=\"10\" Height=\"10\" Grow=\"1\" Background=\"#00FF00\"/>"
              "</View>",
              view, diagnostic),
          "flex grow lays out");
    Check(Child(view, 0)->usedW == 50 && Child(view, 1)->usedW == 50, "grow shares the free space");
    Check(Child(view, 1)->boxX == 50, "the grown boxes stay in order");

    Check(Load(
              "<View Direction=\"row\" Width=\"30\" Height=\"10\" Align=\"start\">"
              "<View Width=\"20\" Height=\"10\" Shrink=\"1\"/>"
              "<View Width=\"20\" Height=\"10\" Shrink=\"1\"/>"
              "</View>",
              view, diagnostic),
          "flex shrink lays out");
    Check(Child(view, 0)->usedW == 15 && Child(view, 1)->usedW == 15, "shrink shares the overflow");

    Check(Load(
              "<View Direction=\"row\" Width=\"40\" Height=\"10\" Align=\"start\">"
              "<View Id=\"late\" Width=\"10\" Height=\"10\" Order=\"2\" Background=\"#FF0000\"/>"
              "<View Id=\"early\" Width=\"10\" Height=\"10\" Order=\"1\" Background=\"#00FF00\"/>"
              "</View>",
              view, diagnostic),
          "flex order lays out");
    Check(RoboViewFind(view, "early")->boxX == 0, "the lower order is placed first");
    Check(RoboViewFind(view, "late")->boxX == 10, "the higher order follows");

    Check(Load(
              "<View Id=\"root\" Width=\"80\" Height=\"30\" Background=\"#0000FF\">"
              "<View Id=\"clip\" Width=\"40\" Height=\"30\" Background=\"#FF0000\" Overflow=\"clip\">"
              "<View Id=\"spill\" Position=\"absolute\" Left=\"30\" Top=\"0\" Width=\"20\" Height=\"10\" Background=\"#00FF00\"/>"
              "</View>"
              "</View>",
              view, diagnostic),
          "clip overflow lays out");
    RoboViewFrame frame;
    RoboViewPaint(view, frame);
    RoboU32 pixel = 0;
    Check(RoboViewPixel(frame, 35, 0, pixel) && pixel == 0xFF00FF00u, "the clipped child paints inside the parent");
    Check(RoboViewPixel(frame, 45, 0, pixel) && pixel == 0xFF0000FFu, "the clipped child does not paint outside the parent");
    Check(RoboViewHit(view, 35, 0) == RoboViewFind(view, "spill"), "a hit inside the clip reaches the child");
    Check(RoboViewHit(view, 45, 0) == RoboViewFind(view, "root"), "a hit outside the clip stays on the parent");

    Check(Load(
              "<View Id=\"root\" Width=\"80\" Height=\"30\" Background=\"#0000FF\">"
              "<View Id=\"open\" Width=\"40\" Height=\"30\" Overflow=\"visible\">"
              "<View Id=\"spill\" Position=\"absolute\" Left=\"30\" Top=\"0\" Width=\"20\" Height=\"10\" Background=\"#00FF00\"/>"
              "</View>"
              "</View>",
              view, diagnostic),
          "visible overflow lays out");
    RoboViewPaint(view, frame);
    Check(RoboViewPixel(frame, 45, 0, pixel) && pixel == 0xFF00FF00u, "visible overflow paints past the parent");
}

void TestChildLimit() {
    std::string source = "<View Width=\"10\" Height=\"10\">";
    for (int index = 0; index < 64; ++index) {
        source += "<View Width=\"1\" Height=\"1\"/>";
    }
    source += "</View>";
    RoboViewNode view;
    std::string diagnostic;
    Check(Load(source, view, diagnostic), "64 children are valid");
    Check(view.children.size() == 64, "all 64 children are kept");
    source = "<View Width=\"10\" Height=\"10\">";
    for (int index = 0; index < 65; ++index) {
        source += "<View Width=\"1\" Height=\"1\"/>";
    }
    source += "</View>";
    Check(!Load(source, view, diagnostic), "65 children are rejected");
    Check(diagnostic.find("R7105") != std::string::npos, "too many children uses R7105");
}

}  // namespace

int main() {
    TestSchemaAndAlias();
    TestRowColumnAndReverse();
    TestStackGridAndPosition();
    TestPaddingMarginAlignmentSpacing();
    TestSizeConstraintsOverflowFlexAspect();
    TestChildLimit();
    if (gFailed != 0) {
        std::cerr << gFailed << " failed, " << gPassed << " passed\n";
        return 1;
    }
    std::cout << gPassed << " passed\n";
    return 0;
}
