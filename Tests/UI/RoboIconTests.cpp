#include "../../RobixUI/Compiler/Components/Icon/RoboIcon.hpp"
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

bool Load(const std::string& source, RoboIconNode& icon, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboIconRead(document.root, icon, diagnostic);
}

template <typename T>
void Append(std::vector<RoboU8>& bytes, const T& value) {
    const auto* raw = reinterpret_cast<const RoboU8*>(&value);
    bytes.insert(bytes.end(), raw, raw + sizeof(T));
}

std::vector<RoboU8> Pack() {
    RoboIconPackHeader header{};
    header.magic = ROBO_ICON_PACK_MAGIC;
    header.version = ROBO_ICON_PACK_VERSION;
    header.iconCount = 1;
    header.baseSize = 4;
    header.entryOffset = static_cast<RoboU32>(sizeof(RoboIconPackHeader));
    RoboIconCookedEntry entry{};
    std::memcpy(entry.name, "mark", 4);
    entry.layerCount = 1;
    entry.layerOffset = header.entryOffset + static_cast<RoboU32>(sizeof(RoboIconCookedEntry));
    entry.baseSize = 4;
    RoboIconCookedLayer layer{};
    std::memcpy(layer.slotName, "color", 5);
    layer.defaultARGB = 0xFFFFFFFFu;
    layer.width = 4;
    layer.height = 4;
    layer.stride = 4;
    layer.pixelOffset = entry.layerOffset + static_cast<RoboU32>(sizeof(RoboIconCookedLayer));
    const RoboU8 coverage[16] = {
        0, 0, 0, 0,
        0, 255, 255, 0,
        0, 255, 255, 0,
        0, 0, 0, 0,
    };
    std::vector<RoboU8> bytes;
    Append(bytes, header);
    Append(bytes, entry);
    Append(bytes, layer);
    bytes.insert(bytes.end(), coverage, coverage + 16);
    return bytes;
}

void TestSchema() {
    RoboIconNode icon;
    std::string diagnostic;
    Check(Load("<Icon Name=\"Mark\" Size=\"8\" Color=\"#FF0000\"/>", icon, diagnostic), "icon schema reads");
    Check(diagnostic.empty(), "icon schema has no diagnostic");
    Check(icon.name == "Mark" && icon.size == 8 && icon.color == 0xFFFF0000u, "icon keeps name, size, and color");
    Check(Load("<Icon Name=\"mark\"/>", icon, diagnostic) && icon.size == 24 && icon.color == 0xFFFFFFFFu, "icon defaults size and color");
    Check(!Load("<Icon Name=\"mark\" Fit=\"cover\"/>", icon, diagnostic), "an unknown icon attribute is rejected");
    Check(diagnostic.find("R7401") != std::string::npos, "an unknown icon attribute uses R7401");
    Check(!Load("<Icon Name=\"mark\" Color=\"red\"/>", icon, diagnostic), "a named icon color is rejected");
    Check(diagnostic.find("R7402") != std::string::npos, "a bad icon color uses R7402");
    Check(!Load("<Icon Name=\"mark\" Color=\"#GG0000\"/>", icon, diagnostic), "a bad icon hex color is rejected");
    Check(!Load("<Icon Name=\"mark\" Size=\"0\"/>", icon, diagnostic), "a zero icon size is rejected");
    Check(diagnostic.find("R7403") != std::string::npos, "a bad icon size uses R7403");
    Check(!Load("<Icon Name=\"mark\" Size=\"-4\"/>", icon, diagnostic), "a negative icon size is rejected");
    Check(!Load("<Icon Name=\"mark\" Size=\"wide\"/>", icon, diagnostic), "a word icon size is rejected");
    Check(!Load("<Icon Name=\"mark\" Size=\"900\"/>", icon, diagnostic), "an icon size above 512 is rejected");
    Check(!Load("<View Name=\"mark\"/>", icon, diagnostic), "a view is not an icon");
    Check(diagnostic.find("R7404") != std::string::npos, "the wrong icon tag uses R7404");
    Check(!Load("<Icon Name=\"mark\"><Path D=\"M0 0\"/></Icon>", icon, diagnostic), "an icon child is rejected");
    Check(!Load("<Icon Size=\"8\"/>", icon, diagnostic), "an icon without a name is rejected");
    Check(diagnostic.find("R7405") != std::string::npos, "a missing icon name uses R7405");
    Check(!Load("<Icon Name=\"\"/>", icon, diagnostic), "an empty icon name is rejected");
}

void TestGlyph() {
    const std::vector<RoboU8> bytes = Pack();
    Check(RoboIconPackValidate(bytes.data(), bytes.size()), "the fixture pack validates");
    const RoboIconPackView pack = RoboIconPackMakeView(bytes.data(), bytes.size());
    RoboIconNode icon;
    std::string diagnostic;
    Check(Load("<Icon Name=\"Mark\" Size=\"8\" Color=\"#FF0000\"/>", icon, diagnostic), "the glyph icon reads");
    RoboIconBox box;
    Check(RoboIconGlyph(icon, pack, box, diagnostic), "the glyph is read from the pack");
    Check(diagnostic.empty(), "a found glyph has no diagnostic");
    Check(box.x == 2 && box.y == 2 && box.width == 4 && box.height == 4, "the glyph box is the scaled coverage");
    Check(Load("<Icon Name=\"other\" Size=\"8\"/>", icon, diagnostic), "a missing glyph name still reads");
    Check(!RoboIconGlyph(icon, pack, box, diagnostic), "a missing glyph is rejected");
    Check(diagnostic.find("R7406") != std::string::npos, "a missing glyph uses R7406");
    std::vector<RoboU8> broken(sizeof(RoboIconPackHeader), 0);
    const RoboIconPackView bad = RoboIconPackMakeView(broken.data(), broken.size());
    Check(Load("<Icon Name=\"mark\" Size=\"8\"/>", icon, diagnostic), "the broken pack still has a schema");
    Check(!RoboIconGlyph(icon, bad, box, diagnostic), "a broken pack is rejected");
    RoboIconFrame frame;
    Check(Load("<Icon Name=\"mark\" Size=\"8\" Color=\"#00FF00\"/>", icon, diagnostic), "the paint icon reads");
    RoboIconPaint(icon, pack, frame);
    RoboU32 color = 0;
    Check(frame.width == 8 && frame.height == 8, "the painted glyph fills the size box");
    Check(RoboIconPixel(frame, 2, 2, color) && color == 0xFF00FF00u, "the glyph ink uses the icon color");
    Check(RoboIconPixel(frame, 5, 5, color) && color == 0xFF00FF00u, "the far corner of the glyph box is ink");
    Check(RoboIconPixel(frame, 1, 1, color) && color == 0, "the pixel outside the glyph box stays clear");
    Check(RoboIconPixel(frame, 6, 2, color) && color == 0, "the pixel beside the glyph box stays clear");
    Check(!RoboIconPixel(frame, 8, 0, color), "a pixel outside the frame is rejected");
    Check(!RoboIconPixel(frame, -1, 0, color), "a negative pixel is rejected");
}

}  // namespace

int main() {
    TestSchema();
    TestGlyph();
    if (gFailed != 0) {
        std::cerr << gFailed << " failed, " << gPassed << " passed\n";
        return 1;
    }
    std::cout << gPassed << " passed\n";
    return 0;
}
