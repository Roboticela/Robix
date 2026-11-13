#include "../../RobixUI/Compiler/Components/List/RoboList.hpp"
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

bool Load(const std::string& source, RoboListNode& list, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboListRead(document.root, list, diagnostic);
}

const RoboListChild* FindChild(const RoboListNode& list, const std::string& key) {
    for (const RoboListChild& child : list.children) {
        if (child.key == key) {
            return &child;
        }
    }
    return nullptr;
}

void TestSchema() {
    RoboListNode list;
    std::string diagnostic;
    Check(Load("<List Items=\"alpha,beta,gamma\" Key=\"name\" Template=\"text\" Numbered=\"true\" Width=\"40\" ItemHeight=\"10\" Gap=\"2\"/>",
               list, diagnostic),
          "a list document is accepted");
    Check(list.children.size() == 3 && list.templateKind == "text" && list.numbered && list.keyField == "name" && list.gap == 2,
          "items, key, and template are read");
    Check(list.children[0].key == "alpha" && list.children[1].key == "beta" && list.children[2].key == "gamma",
          "three children keep their keys");
    Check(list.children[0].kind == "text" && list.children[0].identity == 1 && list.children[2].identity == 3,
          "each child is a new text row");

    Check(Load("<List Template=\"TextField\" Items=\"one\"/>", list, diagnostic) && list.templateKind == "input",
          "TextField is the Input primitive");
    Check(Load("<List Template=\"Container\" Items=\"one\"/>", list, diagnostic) && list.templateKind == "view",
          "Container is the View primitive");
    Check(Load("<List Template=\"ScrollView\" Items=\"one\"/>", list, diagnostic) && list.templateKind == "scrollview",
          "ScrollView is an earlier primitive");
    Check(Load("<List Template=\"Video\" Items=\"one\"/>", list, diagnostic) && list.templateKind == "video",
          "Video is an earlier primitive");
    Check(Load("<List Template=\"Audio\" Items=\"one\"/>", list, diagnostic) && list.templateKind == "audio",
          "Audio is an earlier primitive");

    Check(!Load("<List Items=\"alpha\" Tilt=\"1\"/>", list, diagnostic) && diagnostic.find("R8201") != std::string::npos,
          "an unknown attribute is rejected");
    Check(!Load("<List Items=\"alpha\" Template=\"button\"/>", list, diagnostic) && diagnostic.find("R8202") != std::string::npos,
          "a template that is not an earlier primitive is rejected");
    Check(!Load("<List Items=\"alpha\" Numbered=\"maybe\"/>", list, diagnostic) && diagnostic.find("R8202") != std::string::npos,
          "a bad numbered value is rejected");
    Check(!Load("<List Items=\"alpha,,beta\"/>", list, diagnostic) && diagnostic.find("R8202") != std::string::npos,
          "an empty item is rejected");
    Check(!Load("<List Items=\"alpha,alpha\"/>", list, diagnostic) && diagnostic.find("R8202") != std::string::npos,
          "a repeated key is rejected");
    Check(!Load("<List Items=\"alpha\" Width=\"0\"/>", list, diagnostic) && diagnostic.find("R8203") != std::string::npos,
          "a bad size is rejected");
    Check(!Load("<List Items=\"alpha\" Gap=\"wide\"/>", list, diagnostic) && diagnostic.find("R8203") != std::string::npos,
          "a bad gap is rejected");
    Check(!Load("<Screen/>", list, diagnostic) && diagnostic.find("R8204") != std::string::npos, "the wrong tag is rejected");
    Check(!Load("<List Items=\"alpha\"><Item key=\"beta\"/></List>", list, diagnostic) && diagnostic.find("R8202") != std::string::npos,
          "items cannot be set twice");
    Check(!Load("<List Items=\"alpha\" Template=\"text\"><Template><Text value=\"{value}\"/></Template></List>", list, diagnostic) &&
              diagnostic.find("R8202") != std::string::npos,
          "the template cannot be set twice");
    Check(!Load("<List><Item value=\"alone\"/></List>", list, diagnostic) && diagnostic.find("R8202") != std::string::npos,
          "an item without its key is rejected");
    Check(!Load("<List><Item key=\"a\" color=\"blue\"/></List>", list, diagnostic) && diagnostic.find("R8202") != std::string::npos,
          "a bad item color is rejected");
    Check(!Load("<List><Template><Text value=\"a\"/><Text value=\"b\"/></Template><Item key=\"a\"/></List>", list, diagnostic) &&
              diagnostic.find("R8202") != std::string::npos,
          "a template with two children is rejected");
}

void TestBuild() {
    RoboXMLDocument document;
    std::string diagnostic;
    const char* source =
        "<List Key=\"id\">"
        "<Template><View background=\"{color}\"><Text value=\"{value}\"/></View></Template>"
        "<Item id=\"blue\" value=\"Blue lamp\" color=\"#0E7AFF\"/>"
        "<Item id=\"green\" value=\"Green lamp\" color=\"#2F9E44\"/>"
        "<Item id=\"hull\" value=\"Hull light\" color=\"#C4553A\"/>"
        "</List>";
    Check(RoboXMLParse(source, document), "the item document parses");
    Check(RoboListMaterialize(document.root, diagnostic), "the list builds one child per item");
    Check(document.root.children.size() == 3, "three children were built");
    Check(document.root.children[0].name == "View" && document.root.children[0].attrs["key"] == "blue" &&
              document.root.children[0].attrs["background"] == "#0E7AFF",
          "the first child keeps the blue key");
    Check(document.root.children[1].attrs["key"] == "green" && document.root.children[2].attrs["key"] == "hull",
          "the other children keep their keys");
    Check(!document.root.children.empty() && !document.root.children[0].children.empty() &&
              document.root.children[0].children[0].attrs["value"] == "Blue lamp",
          "the template text receives the item value");

    RoboListNode list;
    Check(Load("<List Items=\"blue,green,hull\" Template=\"text\" Width=\"30\" ItemHeight=\"8\"/>", list, diagnostic),
          "a comma list is accepted");
    Check(list.children.size() == 3 && list.children[0].key == "blue" && list.children[0].value == "blue" &&
              list.children[0].color == 0xFF0E7AFFu && list.children[1].y == 8 && list.contentHeight == 24,
          "comma items become three placed children");

    const char* harbor =
        "<List key=\"id\" width=\"720\" direction=\"column\" gap=\"8\">"
        "<Template><View background=\"#1C2430\"><View background=\"{color}\"/><Text value=\"{value}\"/></View></Template>"
        "<Item id=\"blue\" value=\"Blue lamp, dry dock\" color=\"#0E7AFF\"/>"
        "<Item id=\"green\" value=\"Green lamp, inner harbor\" color=\"#2F9E44\"/>"
        "<Item id=\"hull\" value=\"Hull light, east wharf\" color=\"#C4553A\"/>"
        "</List>";
    Check(RoboXMLParse(harbor, document) && RoboListMaterialize(document.root, diagnostic), "a list with direction still builds");
    Check(document.root.children.size() == 3 && document.root.children[0].children.size() == 2 &&
              document.root.children[0].children[1].attrs["value"] == "Blue lamp, dry dock" &&
              document.root.children[1].children[1].attrs["value"] == "Green lamp, inner harbor" &&
              document.root.children[2].children[1].attrs["value"] == "Hull light, east wharf",
          "the harbor rows replace the template text");
}

void TestRecycle() {
    RoboListNode list;
    std::string diagnostic;
    Check(Load("<List Items=\"alpha,beta,gamma\" Width=\"20\" ItemHeight=\"6\"/>", list, diagnostic), "the recycle list is accepted");
    const int alpha = FindChild(list, "alpha")->identity;
    const int beta = FindChild(list, "beta")->identity;
    const int gamma = FindChild(list, "gamma")->identity;
    std::vector<RoboListItem> next = {{"gamma", "gamma", 0}, {"beta", "kept", 0}, {"delta", "delta", 0}};
    Check(RoboListApply(list, next, diagnostic), "the collection can change");
    Check(list.children.size() == 3 && list.children[0].key == "gamma" && list.children[1].key == "beta" && list.children[2].key == "delta",
          "the new order is gamma, beta, delta");
    Check(FindChild(list, "gamma")->identity == gamma && !FindChild(list, "gamma")->recycled, "gamma keeps its identity");
    Check(FindChild(list, "beta")->identity == beta && FindChild(list, "beta")->value == "kept", "beta keeps its identity");
    Check(FindChild(list, "alpha") == nullptr, "alpha leaves the list");
    Check(FindChild(list, "delta")->recycled && FindChild(list, "delta")->identity == alpha, "delta reuses the freed child");
    const std::vector<RoboListChild> kept = list.children;
    std::vector<RoboListItem> repeated = {{"beta", "beta", 0}, {"beta", "beta", 0}};
    Check(!RoboListApply(list, repeated, diagnostic) && diagnostic.find("R8202") != std::string::npos, "a repeated key does not apply");
    Check(list.children.size() == kept.size() && list.children[0].key == kept[0].key && list.children[2].key == kept[2].key,
          "a rejected collection leaves the children in place");
}

void TestPaint() {
    RoboListNode list;
    std::string diagnostic;
    Check(Load("<List Width=\"16\" Height=\"20\" ItemHeight=\"12\" Gap=\"0\">"
               "<Item key=\"a\" color=\"#FF0000\"/>"
               "<Item key=\"b\" color=\"#00FF00\"/>"
               "<Item key=\"c\" color=\"#0000FF\"/>"
               "</List>",
               list, diagnostic),
          "the paint list is accepted");
    RoboListFrame frame;
    RoboListPaint(list, frame);
    RoboU32 color = 0;
    Check(frame.width == 16 && frame.height == 20, "the frame is the list box");
    Check(RoboListPixel(frame, 1, 1, color) && color == 0xFFFF0000u, "the first child is red");
    Check(RoboListPixel(frame, 1, 12, color) && color == 0xFF00FF00u, "the second child is green");
    Check(RoboListPixel(frame, 1, 19, color) && color == 0xFF00FF00u, "the second child is clipped to the box");
    Check(!RoboListPixel(frame, 1, 24, color), "the third child is outside the box");
    std::string key;
    Check(RoboListHit(list, 1, 1, key) && key == "a", "a hit on the first row returns its key");
    Check(RoboListHit(list, 1, 19, key) && key == "b", "a hit on the clipped row returns its key");
    Check(!RoboListHit(list, 1, 24, key) && key.empty(), "a hit past the box misses");

    RoboListNode gapped;
    Check(Load("<List Width=\"10\" ItemHeight=\"4\" Gap=\"3\" Items=\"a,b\"/>", gapped, diagnostic), "a gapped list is accepted");
    Check(!RoboListHit(gapped, 1, 5, key), "a hit in the gap misses");
    Check(RoboListHit(gapped, 1, 8, key) && key == "b", "a hit on the second row returns its key");
}

}  // namespace

int main() {
    TestSchema();
    TestBuild();
    TestRecycle();
    TestPaint();
    if (gFailed != 0) {
        std::cerr << gFailed << " failed, " << gPassed << " passed\n";
        return 1;
    }
    std::cout << gPassed << " passed\n";
    return 0;
}
