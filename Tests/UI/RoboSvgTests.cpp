#include "../../RobixUI/Compiler/Components/Svg/RoboSvg.hpp"
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

bool Load(const std::string& source, RoboSvgNode& svg, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboSvgRead(document.root, svg, diagnostic);
}

void TestSchema() {
    RoboSvgNode svg;
    std::string diagnostic;
    Check(Load("<Svg Paths=\"M1 1 H3 V3 H1 Z\" Fill=\"#FF0000\" Stroke=\"#0000FF\" StrokeWidth=\"2\" ViewBox=\"0 0 4 4\" Width=\"32\" Height=\"32\"/>",
               svg, diagnostic),
          "svg schema reads");
    Check(diagnostic.empty(), "svg schema has no diagnostic");
    Check(svg.paths.size() == 1 && svg.paths[0].data == "M1 1 H3 V3 H1 Z", "svg keeps the path");
    Check(svg.fill == "#FF0000" && svg.stroke == "#0000FF" && svg.strokeWidth == 2.0f, "svg keeps fill, stroke, and stroke width");
    Check(svg.viewX == 0.0f && svg.viewY == 0.0f && svg.viewWidth == 4.0f && svg.viewHeight == 4.0f, "svg keeps the view box");
    Check(svg.width == 32 && svg.height == 32, "svg keeps the element box");
    Check(Load("<Svg Width=\"16\"><Path D=\"M0 0 H1\"/></Svg>", svg, diagnostic) && svg.width == 16 && svg.height == 16 && svg.paths.size() == 1,
          "a path child supplies the path and the height follows the width");
    Check(Load("<Svg Fill=\"none\" Stroke=\"#FFFFFF\"><Path D=\"M0 0 H1\" Fill=\"#00FF00\"/></Svg>", svg, diagnostic) && svg.paths[0].hasFill &&
              svg.paths[0].fill == "#00FF00",
          "a path fill overrides the svg fill");
    Check(!Load("<Svg Paths=\"M0 0\" Radius=\"4\"/>", svg, diagnostic), "an unknown svg attribute is rejected");
    Check(diagnostic.find("R7501") != std::string::npos, "an unknown svg attribute uses R7501");
    Check(!Load("<Svg Paths=\"M0 0\" Fill=\"red\"/>", svg, diagnostic), "a named svg fill is rejected");
    Check(diagnostic.find("R7502") != std::string::npos, "a bad svg fill uses R7502");
    Check(!Load("<Svg Paths=\"M0 0\" Stroke=\"#12\"/>", svg, diagnostic), "a short svg stroke is rejected");
    Check(!Load("<Svg Paths=\"M0 0\" ViewBox=\"0 0 4\"/>", svg, diagnostic), "a short view box is rejected");
    Check(!Load("<Svg Paths=\"M0 0\" ViewBox=\"0 0 0 4\"/>", svg, diagnostic), "a zero view box is rejected");
    Check(!Load("<Svg Paths=\"M0 0\" ViewBox=\"0 0 -4 4\"/>", svg, diagnostic), "a negative view box is rejected");
    Check(!Load("<Svg Paths=\"M0 0\" Width=\"0\"/>", svg, diagnostic), "a zero svg width is rejected");
    Check(diagnostic.find("R7503") != std::string::npos, "a bad svg size uses R7503");
    Check(!Load("<Svg Paths=\"M0 0\" Height=\"tall\"/>", svg, diagnostic), "a word svg height is rejected");
    Check(!Load("<Svg Paths=\"M0 0\" StrokeWidth=\"-1\"/>", svg, diagnostic), "a negative stroke width is rejected");
    Check(!Load("<View Paths=\"M0 0\"/>", svg, diagnostic), "a view is not an svg");
    Check(diagnostic.find("R7504") != std::string::npos, "the wrong svg tag uses R7504");
    Check(!Load("<Svg><Text Value=\"no\"/></Svg>", svg, diagnostic), "a text child is rejected");
    Check(!Load("<Svg><Path/></Svg>", svg, diagnostic), "a path without commands is rejected");
    Check(diagnostic.find("R7505") != std::string::npos, "a missing path uses R7505");
    Check(!Load("<Svg/>", svg, diagnostic), "an svg without a path is rejected");
    Check(!Load("<Svg><Path D=\"M0 0\" Fill=\"nope\"/></Svg>", svg, diagnostic), "a bad path fill is rejected");
    std::string many = "<Svg>";
    for (int index = 0; index < 33; ++index) {
        many += "<Path D=\"M0 0\"/>";
    }
    many += "</Svg>";
    Check(!Load(many, svg, diagnostic), "more than 32 paths are rejected");
    Check(diagnostic.find("R7506") != std::string::npos, "too many paths use R7506");
}

void TestPaint() {
    RoboSvgNode svg;
    std::string diagnostic;
    Check(Load("<Svg Paths=\"M1 1 H3 V3 H1 Z\" Fill=\"#FF0000\" ViewBox=\"0 0 4 4\" Width=\"32\" Height=\"32\"/>", svg, diagnostic),
          "the fixture svg reads");
    RoboSvgFrame frame;
    RoboSvgPaint(svg, frame);
    RoboU32 color = 0;
    Check(frame.width == 32 && frame.height == 32, "the svg raster fills the element box");
    Check(RoboSvgPixel(frame, 16, 16, color) && color == 0xFFFF0000u, "a pixel on the path is the fill");
    Check(RoboSvgPixel(frame, 0, 0, color) && color == 0, "a pixel outside the path stays clear");
    Check(RoboSvgPixel(frame, 31, 31, color) && color == 0, "the far corner outside the path stays clear");
    Check(!RoboSvgPixel(frame, 32, 0, color), "a pixel outside the svg frame is rejected");
    Check(!RoboSvgPixel(frame, -1, 0, color), "a negative svg pixel is rejected");
    Check(Load("<Svg Fill=\"#0000FF\" ViewBox=\"0 0 4 4\" Width=\"32\" Height=\"32\"><Path D=\"M1 1 H3 V3 H1 Z\"/></Svg>", svg, diagnostic),
          "a path child paints with the svg fill");
    RoboSvgPaint(svg, frame);
    Check(RoboSvgPixel(frame, 16, 16, color) && color == 0xFF0000FFu, "the child path uses the svg fill");
    Check(RoboSvgPixel(frame, 0, 0, color) && color == 0, "the child path leaves the outside clear");
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
