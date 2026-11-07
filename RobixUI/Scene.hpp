#pragma once

#include "Platform/Framebuffer.hpp"
#include "RoboXML.hpp"

#include <string>
#include <vector>

namespace Robix::UI {

struct TextNode {
    RoboXMLNode source;
    std::string tag;
    std::string id;
    std::string content;
    std::string color;
    std::string tone;
    std::string align = "start";
    std::string wrap = "word";
    std::string font;
    std::string size;
    int level = 1;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int maxWidth = 0;
    bool widthSet = false;
    bool heightSet = false;
    int laidX = 0;
    int laidY = 0;
    int laidW = 0;
    int laidH = 0;
};

struct ScrollbarNode {
    std::string id = "scrollbar";
    std::string orientation = "vertical";
    std::string arrows = "hover";
    std::string thumbColor;
    std::string trackColor;
    std::string position = "relative";
    int thickness = 6;
    bool autoHide = true;
    bool overlay = true;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool widthSet = false;
    bool heightSet = false;
    int laidX = 0;
    int laidY = 0;
    int laidW = 0;
    int laidH = 0;
};

struct BoardNode {
    std::string id;
    std::string display = "canvas";
    std::string direction = "column";
    std::string justify = "start";
    std::string align = "start";
    std::string wrap = "nowrap";
    std::string background;
    std::string fill;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int gap = 0;
    int padding = 0;
    int columns = 1;
    bool widthSet = false;
    bool heightSet = false;
    std::vector<BoardNode> children;
    std::vector<TextNode> texts;
    std::vector<ScrollbarNode> bars;
    std::vector<int> order;
    int laidX = 0;
    int laidY = 0;
    int laidW = 0;
    int laidH = 0;
};

struct ScreenNode {
    std::string id = "screen";
    std::string name;
    std::string path = "/";
    std::string theme = "dark";
    std::string place = "center";
    std::string state = "normal";
    std::string background;
    std::string board;
    std::string radius = "radius-lg";
    int x = 0;
    int y = 0;
    int width = 640;
    int height = 400;
    int minWidth = 320;
    int minHeight = 200;
    int maxWidth = 1920;
    int maxHeight = 1080;
    bool movable = true;
    bool resizable = true;
    bool transparent = false;
    bool topmost = false;
    bool fullscreen = false;
    BoardNode content;
    bool hasContent = false;
};

struct Scene {
    ScreenNode screen;
    std::string foundationLine;
    std::vector<std::string> layoutLines;
};

bool LoadScene(const std::string& source, Scene& scene, std::string& error);
void LayoutScene(Scene& scene);
void PaintScene(Scene& scene, Robo::FrameBuffer& buffer);
std::string SceneHtml(const Scene& scene);
bool WriteBmp(const std::string& path, const Robo::FrameBuffer& buffer, std::string& error);
std::string AssetsFontsRoot();

}  // namespace Robix::UI
