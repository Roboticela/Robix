#include "RoboView3D.hpp"

#include "../../../../Compiler/Runtime/SceneRaster.hpp"

#include <cctype>
#include <string>

namespace {

constexpr int ROBO_VIEW3D_MAX_SIZE = 4096;

const char* const ROBO_VIEW3D_ATTRIBUTES[] = {
    "id", "source", "src", "camera", "width", "height",
};

std::string RoboView3DLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string RoboView3DFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

bool RoboView3DKnown(const std::string& key) {
    const std::string folded = RoboView3DLower(key);
    for (const char* name : ROBO_VIEW3D_ATTRIBUTES) {
        if (folded == name) {
            return true;
        }
    }
    return false;
}

bool RoboView3DInteger(const std::string& text, int& number) {
    if (text.empty()) {
        return false;
    }
    int value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10 + (ch - '0');
        if (value > ROBO_VIEW3D_MAX_SIZE) {
            return false;
        }
    }
    number = value;
    return true;
}

}  // namespace

bool RoboView3DRead(const RoboXMLNode& node, RoboView3DNode& view, std::string& diagnostic) {
    view = RoboView3DNode{};
    diagnostic.clear();
    const std::string name = RoboView3DLower(node.name);
    if (name != "view3d" && name != "scene3d") {
        diagnostic = RoboView3DFail("R7704", "'" + node.name + "' is not a View3D. Use View3D or Scene3D.");
        return false;
    }
    if (!node.children.empty()) {
        diagnostic = RoboView3DFail("R7704", "View3D hosts a scene file. Remove the child tags.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!RoboView3DKnown(entry.first)) {
            diagnostic = RoboView3DFail("R7701", "View3D attribute '" + entry.first + "' is unknown. Use source, camera, width, and height.");
            return false;
        }
    }
    view.source = RoboXMLAttr(node, "source");
    if (view.source.empty()) {
        view.source = RoboXMLAttr(node, "src");
    }
    if (view.source.empty()) {
        diagnostic = RoboView3DFail("R7705", "View3D needs a source scene. Use a .rx3d file.");
        return false;
    }
    view.camera = RoboXMLAttr(node, "camera");
    if (view.camera.empty()) {
        diagnostic = RoboView3DFail("R7705", "View3D needs a camera.");
        return false;
    }
    const std::string width = RoboXMLAttr(node, "width");
    const std::string height = RoboXMLAttr(node, "height");
    if (!RoboView3DInteger(width, view.width) || view.width < 1) {
        diagnostic = RoboView3DFail("R7703", "View3D width '" + width + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    if (!RoboView3DInteger(height, view.height) || view.height < 1) {
        diagnostic = RoboView3DFail("R7703", "View3D height '" + height + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    return true;
}

bool RoboView3DPaint(const RoboView3DNode& view, RoboView3DFrame& frame) {
    frame = RoboView3DFrame{};
    if (view.width < 1 || view.height < 1 || view.source.empty()) {
        return false;
    }
    std::vector<std::uint32_t> raster;
    if (!Robix::RoboRasterizeScene(view.source, view.camera, view.width, view.height, raster) ||
        raster.size() != static_cast<std::size_t>(view.width) * static_cast<std::size_t>(view.height)) {
        return false;
    }
    frame.width = view.width;
    frame.height = view.height;
    frame.pixels.resize(raster.size());
    for (std::size_t index = 0; index < raster.size(); ++index) {
        frame.pixels[index] = 0xFF000000u | raster[index];
    }
    return true;
}

bool RoboView3DPixel(const RoboView3DFrame& frame, int x, int y, RoboU32& color) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return false;
    }
    color = frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)];
    return true;
}
