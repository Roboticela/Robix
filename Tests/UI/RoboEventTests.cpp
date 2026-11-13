#include "../../RobixUI/Compiler/Components/Event/RoboEvent.hpp"
#include "../../RobixUI/Compiler/RoboXML.hpp"

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

bool Load(const std::string& source, RoboEventDocument& document, std::string& diagnostic) {
    RoboXMLDocument parsed;
    if (!RoboXMLParse(source, parsed)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboEventRead(parsed.root, document, diagnostic);
}

bool Play(const std::string& source, RoboEventDocument& document, std::vector<RoboEventDelivery>& deliveries,
          std::string& diagnostic) {
    if (!Load(source, document, diagnostic)) {
        return false;
    }
    return RoboEventPlay(document, deliveries, diagnostic);
}

const RoboEventDelivery* FindAt(const std::vector<RoboEventDelivery>& deliveries, const char* name, int index) {
    int seen = 0;
    for (const RoboEventDelivery& delivery : deliveries) {
        if (delivery.name != name) {
            continue;
        }
        if (seen == index) {
            return &delivery;
        }
        ++seen;
    }
    return nullptr;
}

void TestSchema() {
    RoboEventDocument document;
    std::string diagnostic;
    Check(Load("<Events><View Id=\"page\" X=\"0\" Y=\"0\" Width=\"80\" Height=\"40\" OnClick=\"pageClick\"/></Events>",
               document, diagnostic),
          "an events document is accepted");
    Check(document.roots.size() == 1 && document.roots[0].id == "page" && document.roots[0].tag == "View" &&
              document.roots[0].handlers.size() == 1 && document.roots[0].handlers[0].first == "OnClick",
          "the target keeps its id and handler");

    Check(Load("<Container Id=\"alias\" X=\"1\" Y=\"2\" Width=\"3\" Height=\"4\" OnHover=\"over\"/>", document, diagnostic) &&
              document.roots[0].tag == "View",
          "Container is a View");
    Check(Load("<TextField Id=\"field\" Width=\"10\" Height=\"8\" OnChange=\"edit\"/>", document, diagnostic) &&
              document.roots[0].tag == "Input",
          "TextField is Input");
    Check(Load("<Scene3D Id=\"scene\" Width=\"10\" Height=\"8\" OnLoad=\"ready\"/>", document, diagnostic) &&
              document.roots[0].tag == "View3D",
          "Scene3D is View3D");
    Check(Load("<Window Id=\"screen\" Width=\"10\" Height=\"8\" OnMount=\"open\"/>", document, diagnostic) &&
              document.roots[0].tag == "Screen",
          "Window is Screen");

    Check(!Load("<Events Tilt=\"1\"/>", document, diagnostic) && diagnostic.find("R8301") != std::string::npos,
          "an unknown document attribute is rejected");
    Check(!Load("<Events><View Tilt=\"1\" Width=\"4\" Height=\"4\"/></Events>", document, diagnostic) &&
              diagnostic.find("R8301") != std::string::npos,
          "an unknown target attribute is rejected");
    Check(!Load("<Events><View Width=\"4\" Height=\"4\" OnClick=\"\"/></Events>", document, diagnostic) &&
              diagnostic.find("R8302") != std::string::npos,
          "an empty handler is rejected");
    Check(!Load("<Events><View Width=\"4\" Height=\"4\" OnClick=\"a\" onclick=\"b\"/></Events>", document, diagnostic) &&
              diagnostic.find("R8302") != std::string::npos,
          "a repeated handler is rejected");
    Check(!Load("<Events><View Id=\"page\" Width=\"4\" Height=\"4\"/><View Id=\"page\" Width=\"4\" Height=\"4\"/></Events>",
                document, diagnostic) &&
              diagnostic.find("R8302") != std::string::npos,
          "a repeated id is rejected");
    Check(!Load("<Events><View Width=\"-1\" Height=\"4\"/></Events>", document, diagnostic) &&
              diagnostic.find("R8302") != std::string::npos,
          "a bad size is rejected");
    Check(!Load("<Events><View X=\"left\" Y=\"0\" Width=\"4\" Height=\"4\"/></Events>", document, diagnostic) &&
              diagnostic.find("R8302") != std::string::npos,
          "a bad origin is rejected");
    Check(!Load("<Events><View Width=\"4\" Height=\"4\" Visible=\"maybe\"/></Events>", document, diagnostic) &&
              diagnostic.find("R8302") != std::string::npos,
          "a bad visible value is rejected");
    Check(!Load("<Events><Script/><Script/></Events>", document, diagnostic) && diagnostic.find("R8302") != std::string::npos,
          "the script cannot be set twice");
    Check(!Load("<Events><Script><Click/></Script></Events>", document, diagnostic) &&
              diagnostic.find("R8302") != std::string::npos,
          "a pointer step needs a point");
    Check(!Load("<Events><Script><Click X=\"1\" Y=\"2\" Tilt=\"1\"/></Script></Events>", document, diagnostic) &&
              diagnostic.find("R8301") != std::string::npos,
          "an unknown step attribute is rejected");
    Check(!Load("<Events><Script><Spin X=\"1\" Y=\"2\"/></Script></Events>", document, diagnostic) &&
              diagnostic.find("R8302") != std::string::npos,
          "an unknown script step is rejected");
    Check(!Load("<Events><Script><Swipe X=\"1\" Y=\"2\" Direction=\"spin\"/></Script></Events>", document, diagnostic) &&
              diagnostic.find("R8302") != std::string::npos,
          "a bad swipe direction is rejected");
    Check(!Load("<Events><Note Width=\"4\" Height=\"4\"/></Events>", document, diagnostic) &&
              diagnostic.find("R8304") != std::string::npos,
          "the wrong tag is rejected");
    Check(!Load("<Events><View Width=\"4\" Height=\"4\"><Script><Click X=\"1\" Y=\"1\"/></Script></View></Events>", document,
                diagnostic) &&
              diagnostic.find("R8304") != std::string::npos,
          "a script inside a target is rejected");
}

void TestClickScript() {
    RoboEventDocument document;
    std::vector<RoboEventDelivery> deliveries;
    std::string diagnostic;
    const char* source =
        "<Events>"
        "<View Id=\"page\" X=\"0\" Y=\"0\" Width=\"80\" Height=\"40\" OnClick=\"pageClick\">"
        "<View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"16\" Height=\"16\" OnClick=\"lampClick\"/>"
        "<View Id=\"hull\" X=\"28\" Y=\"4\" Width=\"16\" Height=\"16\" OnClick=\"hullClick\"/>"
        "</View>"
        "<Script><Click X=\"8\" Y=\"8\"/></Script>"
        "</Events>";
    Check(Play(source, document, deliveries, diagnostic), "a click script plays");
    const RoboEventDelivery* click = RoboEventFind(deliveries, "OnClick");
    Check(click != nullptr && click->hitId == "lamp" && click->targetId == "lamp" && click->handler == "lampClick",
          "the click hits the topmost id");
    const RoboEventDelivery* parent = FindAt(deliveries, "OnClick", 1);
    Check(parent != nullptr && parent->hitId == "lamp" && parent->targetId == "page" && parent->handler == "pageClick",
          "the click then reaches the parent");
    Check(FindAt(deliveries, "OnClick", 2) == nullptr, "the click stops at the parent");
    const RoboEventDelivery* down = RoboEventFind(deliveries, "OnMouseDown");
    const RoboEventDelivery* up = RoboEventFind(deliveries, "OnMouseUp");
    Check(down != nullptr && down->hitId == "lamp" && up != nullptr && up->hitId == "lamp",
          "the click also delivers mouse down and mouse up");

    Check(Play("<Events><View Id=\"page\" X=\"0\" Y=\"0\" Width=\"80\" Height=\"40\" OnClick=\"pageClick\">"
               "<View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"16\" Height=\"16\" OnClick=\"lampClick\"/>"
               "<View Id=\"cover\" X=\"4\" Y=\"4\" Width=\"16\" Height=\"16\" OnClick=\"coverClick\"/>"
               "</View><Script><Click X=\"8\" Y=\"8\"/></Script></Events>",
               document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnClick")->hitId == "cover",
          "a later sibling is the topmost hit");
    std::string hit;
    Check(RoboEventHit(document, 8, 8, hit) && hit == "cover", "a hit query returns the same id");
    Check(!RoboEventHit(document, 80, 8, hit) && hit.empty(), "a point on the far edge misses");
    Check(Play("<Events><View Id=\"page\" X=\"0\" Y=\"0\" Width=\"80\" Height=\"40\" OnClick=\"pageClick\">"
               "<View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"16\" Height=\"16\" OnClick=\"lampClick\"/>"
               "</View><Script><Click X=\"2\" Y=\"2\"/></Script></Events>",
               document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnClick")->hitId == "page",
          "a point on the parent misses the child");
    Check(Play("<Events><View Id=\"page\" X=\"0\" Y=\"0\" Width=\"80\" Height=\"40\" OnClick=\"pageClick\"/>"
               "<Script><Click X=\"90\" Y=\"90\"/></Script></Events>",
               document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnClick") == nullptr,
          "a point outside the tree delivers nothing");
    Check(Play("<Events><View Id=\"page\" X=\"0\" Y=\"0\" Width=\"80\" Height=\"40\">"
               "<View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"16\" Height=\"16\" Visible=\"false\" OnClick=\"lampClick\"/>"
               "</View><Script><Click X=\"8\" Y=\"8\"/></Script></Events>",
               document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnClick")->hitId == "page",
          "a hidden child is not the hit");
}

void TestPointerAndTouch() {
    RoboEventDocument document;
    std::vector<RoboEventDelivery> deliveries;
    std::string diagnostic;
    const char* box =
        "<Events><View Id=\"page\" X=\"0\" Y=\"0\" Width=\"80\" Height=\"40\" OnHover=\"pageHover\" OnContextMenu=\"pageMenu\" "
        "OnTap=\"pageTap\" OnLongPress=\"pageHold\" OnSwipe=\"pageSwipe\" OnDrag=\"pageDrag\" OnDrop=\"pageDrop\" "
        "OnPinch=\"pagePinch\" OnRotate=\"pageRotate\">"
        "<View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"16\" Height=\"16\" OnHover=\"lampHover\" OnContextMenu=\"lampMenu\" "
        "OnTap=\"lampTap\" OnLongPress=\"lampHold\" OnSwipe=\"lampSwipe\" OnDrag=\"lampDrag\" OnDrop=\"lampDrop\" "
        "OnPinch=\"lampPinch\" OnRotate=\"lampRotate\"/>"
        "<View Id=\"hull\" X=\"28\" Y=\"4\" Width=\"16\" Height=\"16\" OnDrop=\"hullDrop\"/>"
        "</View><Script>";
    const std::string end = "</Script></Events>";
    Check(Play(std::string(box) + "<Hover X=\"8\" Y=\"8\"/>" + end, document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnHover")->hitId == "lamp" &&
              FindAt(deliveries, "OnHover", 1)->targetId == "page",
          "hover reaches the hit and then the parent");
    Check(Play(std::string(box) + "<ContextMenu X=\"8\" Y=\"8\"/>" + end, document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnContextMenu")->handler == "lampMenu",
          "context menu hits the lamp");
    Check(Play(std::string(box) + "<Tap X=\"8\" Y=\"8\"/>" + end, document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnTap")->hitId == "lamp",
          "tap hits the lamp");
    Check(Play(std::string(box) + "<LongPress X=\"8\" Y=\"8\"/>" + end, document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnLongPress")->hitId == "lamp",
          "long press hits the lamp");
    Check(Play(std::string(box) + "<Swipe X=\"8\" Y=\"8\" Direction=\"left\"/>" + end, document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnSwipe")->hitId == "lamp" && RoboEventFind(deliveries, "OnSwipe")->value == "left",
          "swipe carries its direction");
    Check(Play(std::string(box) + "<Drag X=\"8\" Y=\"8\"/><Drop X=\"32\" Y=\"8\"/>" + end, document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnDrag")->hitId == "lamp" && RoboEventFind(deliveries, "OnDrop")->hitId == "hull" &&
              RoboEventFind(deliveries, "OnDrop")->value == "lamp",
          "drop names the dragged id");
    Check(Play(std::string(box) + "<Pinch X=\"8\" Y=\"8\" Scale=\"2\"/>" + end, document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnPinch")->value == "2",
          "pinch carries its scale");
    Check(Play(std::string(box) + "<Rotate X=\"8\" Y=\"8\" Angle=\"15\"/>" + end, document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnRotate")->value == "15",
          "rotate carries its angle");
    Check(Play("<Events><View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"16\" Height=\"16\" OnClick=\"lampClick\"/>"
               "<View Id=\"hull\" X=\"28\" Y=\"4\" Width=\"16\" Height=\"16\" OnMouseUp=\"hullUp\"/>"
               "<Script><MouseDown X=\"8\" Y=\"8\"/><MouseUp X=\"32\" Y=\"8\"/></Script></Events>",
               document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnMouseDown")->hitId == "lamp" &&
              RoboEventFind(deliveries, "OnMouseUp")->hitId == "hull" && RoboEventFind(deliveries, "OnClick") == nullptr,
          "a mouse up on another target is not a click");
    Check(Play("<Events><View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"16\" Height=\"16\" OnClick=\"lampClick\" "
               "OnDoubleClick=\"lampTwice\"/><Script><Click X=\"8\" Y=\"8\" Time=\"0\"/><Click X=\"8\" Y=\"8\" Time=\"200\"/>"
               "</Script></Events>",
               document, deliveries, diagnostic) &&
              FindAt(deliveries, "OnDoubleClick", 0) != nullptr && FindAt(deliveries, "OnDoubleClick", 0)->hitId == "lamp",
          "a second click inside the window is a double click");
    Check(Play("<Events><View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"16\" Height=\"16\" OnDoubleClick=\"lampTwice\"/>"
               "<Script><Click X=\"8\" Y=\"8\" Time=\"0\"/><Click X=\"8\" Y=\"8\" Time=\"301\"/></Script></Events>",
               document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnDoubleClick") == nullptr,
          "a second click past the window is not a double click");
    Check(Play("<Events><View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"16\" Height=\"16\" OnDoubleClick=\"lampTwice\"/>"
               "<Script><DoubleClick X=\"8\" Y=\"8\"/></Script></Events>",
               document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnDoubleClick")->handler == "lampTwice",
          "a double click step hits the lamp");
}

void TestKeyboardAndValue() {
    RoboEventDocument document;
    std::vector<RoboEventDelivery> deliveries;
    std::string diagnostic;
    Check(Play("<Events><View Id=\"page\" X=\"0\" Y=\"0\" Width=\"40\" Height=\"20\" OnFocus=\"pageFocus\" OnBlur=\"pageBlur\" "
               "OnKeyDown=\"pageDown\" OnKeyUp=\"pageUp\" OnSubmit=\"pageSubmit\">"
               "<Input Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"12\" Height=\"8\" OnFocus=\"lampFocus\" OnBlur=\"lampBlur\" "
               "OnKeyDown=\"lampDown\" OnKeyUp=\"lampUp\" OnSubmit=\"lampSubmit\" OnChange=\"lampChange\" "
               "OnInput=\"lampInput\" OnValueChange=\"lampValue\" OnSelectionChange=\"lampSelect\"/>"
               "</View><Script><Focus Id=\"lamp\"/><KeyDown Key=\"Enter\"/><KeyUp Key=\"Enter\"/><Submit/>"
               "<Change Id=\"lamp\" Value=\"on\"/><Input Id=\"lamp\" Value=\"o\"/><ValueChange Id=\"lamp\" Value=\"on\"/>"
               "<SelectionChange Id=\"lamp\" Value=\"0,2\"/><Blur/></Script></Events>",
               document, deliveries, diagnostic),
          "a keyboard script plays");
    Check(RoboEventFind(deliveries, "OnFocus")->hitId == "lamp" && FindAt(deliveries, "OnFocus", 1)->targetId == "page",
          "focus hits the field and then the parent");
    Check(RoboEventFind(deliveries, "OnKeyDown")->hitId == "lamp" && RoboEventFind(deliveries, "OnKeyDown")->key == "Enter",
          "key down uses the focused id");
    Check(RoboEventFind(deliveries, "OnKeyUp")->handler == "lampUp", "key up uses the focused id");
    Check(RoboEventFind(deliveries, "OnSubmit")->handler == "lampSubmit", "submit uses the focused id");
    Check(RoboEventFind(deliveries, "OnChange")->value == "on" && RoboEventFind(deliveries, "OnInput")->value == "o",
          "change and input carry the value");
    Check(RoboEventFind(deliveries, "OnValueChange")->hitId == "lamp" &&
              RoboEventFind(deliveries, "OnSelectionChange")->value == "0,2",
          "value change and selection change hit the field");
    Check(FindAt(deliveries, "OnBlur", 0)->hitId == "lamp", "blur hits the field that lost focus");
    Check(!Play("<Events><Input Id=\"lamp\" Width=\"8\" Height=\"8\" OnKeyDown=\"lampDown\"/>"
                "<Script><KeyDown Key=\"a\"/></Script></Events>",
                document, deliveries, diagnostic) &&
              diagnostic.find("R8302") != std::string::npos,
          "a key with no focus is rejected");
    Check(!Play("<Events><Input Id=\"lamp\" Width=\"8\" Height=\"8\" OnChange=\"lampChange\"/>"
                "<Script><Change Id=\"missing\" Value=\"on\"/></Script></Events>",
                document, deliveries, diagnostic) &&
              diagnostic.find("R8302") != std::string::npos,
          "a missing value target is rejected");
}

void TestLifecycleLayoutScrollAndMedia() {
    RoboEventDocument document;
    std::vector<RoboEventDelivery> deliveries;
    std::string diagnostic;
    Check(Play("<Events><View Id=\"page\" X=\"0\" Y=\"0\" Width=\"40\" Height=\"30\" OnMount=\"pageMount\" "
               "OnUpdate=\"pageUpdate\" OnLayout=\"pageLayout\" OnResize=\"pageResize\" OnVisibilityChange=\"pageShow\" "
               "OnScroll=\"pageScroll\" OnScrollStart=\"pageStart\" OnScrollEnd=\"pageEnd\">"
               "<View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"10\" Height=\"8\" OnMount=\"lampMount\" OnUnmount=\"lampUnmount\" "
               "OnUpdate=\"lampUpdate\" OnLayout=\"lampLayout\" OnResize=\"lampResize\" OnVisibilityChange=\"lampShow\"/>"
               "<Video Id=\"clip\" X=\"4\" Y=\"16\" Width=\"10\" Height=\"8\" OnLoad=\"clipLoad\" OnError=\"clipError\" "
               "OnPlay=\"clipPlay\" OnPause=\"clipPause\" OnEnd=\"clipEnd\" OnTimeUpdate=\"clipTime\"/>"
               "</View><Script><Mount/><Update/><Layout/><Resize Id=\"lamp\" Width=\"12\" Height=\"9\"/>"
               "<Resize Id=\"lamp\" Width=\"12\" Height=\"9\"/><VisibilityChange Id=\"lamp\" Visible=\"false\"/>"
               "<ScrollStart Id=\"page\"/><Scroll Id=\"page\" Offset=\"16\"/><ScrollEnd Id=\"page\"/>"
               "<Load Id=\"clip\"/><Error Id=\"clip\"/><Play Id=\"clip\"/><Pause Id=\"clip\"/><End Id=\"clip\"/>"
               "<TimeUpdate Id=\"clip\" Time=\"100\"/><Unmount Id=\"lamp\"/></Script></Events>",
               document, deliveries, diagnostic),
          "a lifecycle script plays");
    Check(RoboEventFind(deliveries, "OnMount")->hitId == "page" && FindAt(deliveries, "OnMount", 1)->hitId == "lamp",
          "mount visits the parent before the child");
    Check(RoboEventFind(deliveries, "OnUpdate")->hitId == "page" && RoboEventFind(deliveries, "OnLayout")->hitId == "page",
          "update and layout visit the page");
    Check(RoboEventFind(deliveries, "OnResize")->hitId == "lamp" && RoboEventFind(deliveries, "OnResize")->value == "12,9" &&
              FindAt(deliveries, "OnResize", 1)->targetId == "page" && FindAt(deliveries, "OnResize", 2) == nullptr,
          "resize reports the new size once and then the parent");
    Check(RoboEventFind(deliveries, "OnVisibilityChange")->value == "false", "visibility change carries the new value");
    Check(RoboEventFind(deliveries, "OnScrollStart")->hitId == "page" && RoboEventFind(deliveries, "OnScroll")->value == "16" &&
              RoboEventFind(deliveries, "OnScrollEnd")->handler == "pageEnd",
          "scroll start, scroll, and scroll end stay distinct");
    Check(RoboEventFind(deliveries, "OnLoad")->handler == "clipLoad" && RoboEventFind(deliveries, "OnError")->hitId == "clip" &&
              RoboEventFind(deliveries, "OnPlay")->hitId == "clip" && RoboEventFind(deliveries, "OnPause")->hitId == "clip" &&
              RoboEventFind(deliveries, "OnEnd")->hitId == "clip" && RoboEventFind(deliveries, "OnTimeUpdate")->value == "100",
          "media events hit the clip");
    std::string hit;
    Check(RoboEventHit(document, 6, 6, hit) && hit == "page", "the unmounted lamp is gone");
    Check(Play("<Events><View Id=\"page\" X=\"0\" Y=\"0\" Width=\"40\" Height=\"30\">"
               "<View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"10\" Height=\"8\" OnClick=\"lampClick\" Visible=\"true\"/>"
               "</View><Script><VisibilityChange Id=\"lamp\" Visible=\"false\"/><Click X=\"6\" Y=\"6\"/></Script></Events>",
               document, deliveries, diagnostic) &&
              RoboEventFind(deliveries, "OnClick")->hitId == "page",
          "a hidden target is no longer the hit");
}

void TestEveryPrimitive() {
    const char* tags[] = {"Screen", "View", "Text", "Image", "Video", "Audio", "Icon", "Svg", "Canvas", "View3D", "Input",
                          "ScrollView", "List"};
    int x = 0;
    std::string source = "<Events>";
    for (const char* tag : tags) {
        source += "<";
        source += tag;
        source += " Id=\"";
        source += tag;
        source += "\" X=\"";
        source += std::to_string(x);
        source += "\" Y=\"0\" Width=\"8\" Height=\"8\" OnClick=\"hit\"/>";
        x += 8;
    }
    source += "<Script>";
    x = 0;
    for (const char* tag : tags) {
        source += "<Click X=\"";
        source += std::to_string(x + 1);
        source += "\" Y=\"1\" Time=\"";
        source += std::to_string(1000 + x);
        source += "\"/>";
        x += 8;
    }
    source += "</Script></Events>";
    RoboEventDocument document;
    std::vector<RoboEventDelivery> deliveries;
    std::string diagnostic;
    Check(Play(source, document, deliveries, diagnostic), "every primitive accepts a click");
    int index = 0;
    for (const char* tag : tags) {
        const RoboEventDelivery* delivery = FindAt(deliveries, "OnClick", index);
        Check(delivery != nullptr && delivery->hitId == tag, tag);
        ++index;
    }
}

}  // namespace

int main() {
    TestSchema();
    TestClickScript();
    TestPointerAndTouch();
    TestKeyboardAndValue();
    TestLifecycleLayoutScrollAndMedia();
    TestEveryPrimitive();
    std::cout << gPassed << " passed, " << gFailed << " failed\n";
    return gFailed == 0 ? 0 : 1;
}
