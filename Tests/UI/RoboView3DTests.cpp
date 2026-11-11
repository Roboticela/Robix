#include "../../RobixUI/Compiler/Components/View3D/RoboView3D.hpp"
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

bool Load(const std::string& source, RoboView3DNode& view, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboView3DRead(document.root, view, diagnostic);
}

const char* kScene = "d:/NEW/Robix/Tests/UI/Fixtures/RoboView3DBody.rx3d";

void TestSchema() {
    RoboView3DNode view;
    std::string diagnostic;
    const std::string document = std::string("<View3D source=\"") + kScene + "\" camera=\"Main\" width=\"32\" height=\"24\"/>";
    Check(Load(document, view, diagnostic), "a view3d document is accepted");
    Check(view.source == kScene && view.camera == "Main" && view.width == 32 && view.height == 24, "source, camera, and size are kept");

    RoboView3DNode alias;
    const std::string scene = std::string("<Scene3D source=\"") + kScene + "\" camera=\"Main\" width=\"16\" height=\"16\"/>";
    Check(Load(scene, alias, diagnostic), "Scene3D resolves to View3D");
    Check(alias.source == view.source && alias.camera == "Main" && alias.width == 16 && alias.height == 16, "Scene3D keeps the same fields");

    Check(!Load("<View3D source=\"scene.rx3d\" camera=\"Main\" width=\"32\" height=\"24\" tilt=\"1\"/>", view, diagnostic) &&
              diagnostic.find("R7701") != std::string::npos,
          "an unknown attribute is rejected");
    Check(!Load("<View3D camera=\"Main\" width=\"32\" height=\"24\"/>", view, diagnostic) && diagnostic.find("R7705") != std::string::npos,
          "a missing source is rejected");
    Check(!Load(std::string("<View3D source=\"") + kScene + "\" width=\"32\" height=\"24\"/>", view, diagnostic) &&
              diagnostic.find("R7705") != std::string::npos,
          "a missing camera is rejected");
    Check(!Load(std::string("<View3D source=\"") + kScene + "\" camera=\"Main\" width=\"0\" height=\"24\"/>", view, diagnostic) &&
              diagnostic.find("R7703") != std::string::npos,
          "a bad size is rejected");
    Check(!Load("<Screen/>", view, diagnostic) && diagnostic.find("R7704") != std::string::npos, "the wrong tag is rejected");
    Check(!Load(std::string("<View3D source=\"") + kScene + "\" camera=\"Main\" width=\"32\" height=\"24\"><Mesh/></View3D>", view, diagnostic) &&
              diagnostic.find("R7704") != std::string::npos,
          "a child tag is rejected");
}

void TestPaint() {
    RoboView3DNode view;
    std::string diagnostic;
    const std::string document = std::string("<View3D source=\"") + kScene + "\" camera=\"Main\" width=\"32\" height=\"24\"/>";
    Check(Load(document, view, diagnostic), "the paint document is accepted");
    RoboView3DFrame frame;
    Check(RoboView3DPaint(view, frame), "one frame is painted");
    RoboU32 color = 0;
    Check(RoboView3DPixel(frame, 0, 0, color) && color == 0xFF080A10u, "a corner pixel is the clear color");
    Check(RoboView3DPixel(frame, 16, 12, color) && color == 0xFF8D0000u, "a body pixel is the shaded box");
    Check(RoboView3DPixel(frame, 31, 0, color) && color == 0xFF080A10u, "the far corner stays the clear color");
    Check(!RoboView3DPixel(frame, 32, 0, color), "a pixel outside the frame is rejected");
    Check(!RoboView3DPixel(frame, -1, 0, color), "a negative pixel is rejected");

    view.camera = "Missing";
    Check(RoboView3DPaint(view, frame), "an unknown camera still paints the first camera");
    Check(RoboView3DPixel(frame, 16, 12, color) && color == 0xFF8D0000u, "the first camera still hits the body");
    view.source = "d:/NEW/Robix/Tests/UI/Fixtures/missing.rx3d";
    view.camera = "Main";
    Check(!RoboView3DPaint(view, frame), "a missing scene does not paint");
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
