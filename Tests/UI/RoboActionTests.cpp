#include "../../RobixUI/Compiler/Components/Action/RoboAction.hpp"
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

bool Load(const std::string& source, RoboActionDocument& document, std::string& diagnostic) {
    RoboXMLDocument parsed;
    if (!RoboXMLParse(source, parsed)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboActionRead(parsed.root, document, diagnostic);
}

bool Play(const std::string& source, RoboActionDocument& document, std::vector<RoboActionTrace>& trace, std::string& diagnostic) {
    if (!Load(source, document, diagnostic)) {
        return false;
    }
    return RoboActionPlay(document, trace, diagnostic);
}

std::string State(const RoboActionDocument& document, const char* name) {
    std::string value;
    if (!RoboActionGet(document, name, value)) {
        return {};
    }
    return value;
}

const RoboActionTrace* FindKind(const std::vector<RoboActionTrace>& trace, const char* kind, int index) {
    int seen = 0;
    for (const RoboActionTrace& record : trace) {
        if (record.kind != kind) {
            continue;
        }
        if (seen == index) {
            return &record;
        }
        ++seen;
    }
    return nullptr;
}

void TestSchema() {
    RoboActionDocument document;
    std::string diagnostic;
    Check(Load("<Behavior><State Name=\"lamp\" Value=\"off\"/></Behavior>", document, diagnostic) && State(document, "lamp") == "off",
          "a state keeps its initial value");
    Check(!Load("<Behavior Tilt=\"1\"/>", document, diagnostic) && diagnostic.find("R8401") != std::string::npos,
          "an unknown document attribute is rejected");
    Check(!Load("<Behavior><State Name=\"lamp\" Tilt=\"1\"/></Behavior>", document, diagnostic) &&
              diagnostic.find("R8401") != std::string::npos,
          "an unknown state attribute is rejected");
    Check(!Load("<Behavior><State Value=\"off\"/></Behavior>", document, diagnostic) && diagnostic.find("R8402") != std::string::npos,
          "a state needs a name");
    Check(!Load("<Behavior><State Name=\"lamp\"/><State Name=\"lamp\"/></Behavior>", document, diagnostic) &&
              diagnostic.find("R8402") != std::string::npos,
          "a repeated state is rejected");
    Check(!Load("<Behavior><Action Name=\"go\"/><Action Name=\"go\"/></Behavior>", document, diagnostic) &&
              diagnostic.find("R8402") != std::string::npos,
          "a repeated action is rejected");
    Check(!Load("<Behavior><Script/><Script/></Behavior>", document, diagnostic) && diagnostic.find("R8402") != std::string::npos,
          "the script cannot be set twice");
    Check(!Load("<Behavior><SetState Name=\"lamp\" Value=\"on\"/></Behavior>", document, diagnostic) &&
              diagnostic.find("R8404") != std::string::npos,
          "an action is not a target");
    Check(!Load("<Note/>", document, diagnostic) && diagnostic.find("R8404") != std::string::npos, "the wrong tag is rejected");
    Check(!Load("<Behavior><Script><SetState Name=\"lamp\" Tilt=\"1\"/></Script></Behavior>", document, diagnostic) &&
              diagnostic.find("R8401") != std::string::npos,
          "an unknown action attribute is rejected");
    Check(!Load("<Behavior><Script><Delay Ms=\"soon\"/></Script></Behavior>", document, diagnostic) &&
              diagnostic.find("R8402") != std::string::npos,
          "a bad delay is rejected");
    Check(!Load("<Behavior><Script><ScrollTo Id=\"page\" Offset=\"-1\"/></Script></Behavior>", document, diagnostic) &&
              diagnostic.find("R8402") != std::string::npos,
          "a bad offset is rejected");
    Check(!Load("<Behavior><View Id=\"page\" Enabled=\"maybe\" Width=\"4\" Height=\"4\"/></Behavior>", document, diagnostic) &&
              diagnostic.find("R8402") != std::string::npos,
          "a bad enabled value is rejected");
    Check(!Load("<Behavior><View Id=\"page\" Width=\"4\" Height=\"4\" OnClick=\"missing\"/></Behavior>", document, diagnostic) &&
              diagnostic.find("R8402") != std::string::npos,
          "a handler must name an action");
}

void TestClickSetsState() {
    RoboActionDocument document;
    std::vector<RoboActionTrace> trace;
    std::string diagnostic;
    const char* source =
        "<Behavior>"
        "<State Name=\"lamp\" Value=\"off\"/>"
        "<View Id=\"page\" X=\"0\" Y=\"0\" Width=\"80\" Height=\"40\" OnClick=\"room\">"
        "<View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"16\" Height=\"16\" OnClick=\"light\"/>"
        "</View>"
        "<Action Name=\"light\"><SetState Name=\"lamp\" Value=\"on\"/></Action>"
        "<Action Name=\"room\"><SetState Name=\"room\" Value=\"lit\"/></Action>"
        "<State Name=\"room\" Value=\"dark\"/>"
        "<Script><Click X=\"8\" Y=\"8\"/></Script>"
        "</Behavior>";
    Check(Play(source, document, trace, diagnostic), "a click script plays");
    Check(State(document, "lamp") == "on", "the click sets the new value");
    Check(FindKind(trace, "SetState", 0) != nullptr && FindKind(trace, "SetState", 0)->name == "lamp" &&
              FindKind(trace, "SetState", 0)->value == "on",
          "the hit action runs first");
    Check(State(document, "room") == "lit" && FindKind(trace, "SetState", 1)->name == "room",
          "the parent action runs after the hit");
    Check(Play("<Behavior><State Name=\"lamp\" Value=\"off\"/>"
               "<View Id=\"page\" X=\"0\" Y=\"0\" Width=\"80\" Height=\"40\">"
               "<View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"16\" Height=\"16\" OnClick=\"light\"/>"
               "</View><Action Name=\"light\"><SetState Name=\"lamp\" Value=\"on\"/></Action>"
               "<Script><Click X=\"2\" Y=\"2\"/></Script></Behavior>",
               document, trace, diagnostic) &&
              State(document, "lamp") == "off",
          "a miss does not set the state");
}

void TestSequenceAndParallel() {
    RoboActionDocument document;
    std::vector<RoboActionTrace> trace;
    std::string diagnostic;
    Check(Play("<Behavior><State Name=\"step\" Value=\"\"/><Action Name=\"order\"><Sequence>"
               "<SetState Name=\"step\" Value=\"one\"/><SetState Name=\"step\" Value=\"two\"/>"
               "</Sequence></Action><Script><Execute Name=\"order\"/></Script></Behavior>",
               document, trace, diagnostic),
          "a sequence plays");
    Check(State(document, "step") == "two", "the sequence finishes on the second value");
    Check(FindKind(trace, "SetState", 0)->value == "one" && FindKind(trace, "SetState", 1)->value == "two",
          "the sequence runs the two actions in order");
    Check(FindKind(trace, "Sequence", 0) != nullptr && FindKind(trace, "Execute", 0)->value == "order",
          "execute names the sequence it ran");
    Check(Play("<Behavior><State Name=\"left\" Value=\"\"/><State Name=\"right\" Value=\"\"/><Script><Parallel>"
               "<SetState Name=\"left\" Value=\"port\"/><SetState Name=\"right\" Value=\"starboard\"/>"
               "</Parallel></Script></Behavior>",
               document, trace, diagnostic) &&
              State(document, "left") == "port" && State(document, "right") == "starboard",
          "parallel runs both branches");
    Check(FindKind(trace, "Parallel", 0)->value == "together" && FindKind(trace, "SetState", 0)->name == "left" &&
              FindKind(trace, "SetState", 1)->name == "right",
          "parallel keeps the source order");
    Check(Play("<Behavior><State Name=\"step\" Value=\"\"/><Script><Sequence>"
               "<SetState Name=\"step\" Value=\"one\"/><Delay Ms=\"40\"/><SetState Name=\"step\" Value=\"two\"/>"
               "</Sequence></Script></Behavior>",
               document, trace, diagnostic) &&
              document.clock == 40 && FindKind(trace, "Delay", 0)->value == "40" && State(document, "step") == "two",
          "delay waits between the two actions");
}

void TestConditionalAndVisibility() {
    RoboActionDocument document;
    std::vector<RoboActionTrace> trace;
    std::string diagnostic;
    Check(Play("<Behavior><State Name=\"lamp\" Value=\"off\"/><Script>"
               "<Conditional Name=\"lamp\" Equals=\"off\"><SetState Name=\"lamp\" Value=\"on\"/>"
               "<Else><SetState Name=\"lamp\" Value=\"off\"/></Else></Conditional></Script></Behavior>",
               document, trace, diagnostic) &&
              State(document, "lamp") == "on" && FindKind(trace, "Conditional", 0)->value == "true",
          "conditional takes the matching branch");
    Check(Play("<Behavior><State Name=\"lamp\" Value=\"on\"/><Script>"
               "<Conditional Name=\"lamp\" Equals=\"off\"><SetState Name=\"lamp\" Value=\"on\"/>"
               "<Else><SetState Name=\"lamp\" Value=\"off\"/></Else></Conditional></Script></Behavior>",
               document, trace, diagnostic) &&
              State(document, "lamp") == "off" && FindKind(trace, "Conditional", 0)->value == "false",
          "conditional takes the else branch");
    Check(Play("<Behavior><State Name=\"lamp\" Value=\"off\"/><Script><Conditional Test=\"lamp\">"
               "<SetState Name=\"lamp\" Value=\"on\"/></Conditional></Script></Behavior>",
               document, trace, diagnostic) &&
              State(document, "lamp") == "off",
          "an off test does not run the branch");
    Check(Play("<Behavior><State Name=\"lamp\" Value=\"off\"/>"
               "<View Id=\"page\" X=\"0\" Y=\"0\" Width=\"40\" Height=\"20\">"
               "<View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"10\" Height=\"8\" OnClick=\"light\"/>"
               "</View><Action Name=\"light\"><SetState Name=\"lamp\" Value=\"on\"/></Action>"
               "<Script><Hide Id=\"lamp\"/><Click X=\"6\" Y=\"6\"/></Script></Behavior>",
               document, trace, diagnostic) &&
              State(document, "lamp") == "off" && RoboActionFind(document, "lamp")->visible == false,
          "hide keeps the click off the target");
    Check(Play("<Behavior><State Name=\"lamp\" Value=\"off\"/>"
               "<View Id=\"page\" X=\"0\" Y=\"0\" Width=\"40\" Height=\"20\">"
               "<View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"10\" Height=\"8\" OnClick=\"light\"/>"
               "</View><Action Name=\"light\"><SetState Name=\"lamp\" Value=\"on\"/></Action>"
               "<Script><Hide Id=\"lamp\"/><Show Id=\"lamp\"/><Click X=\"6\" Y=\"6\"/></Script></Behavior>",
               document, trace, diagnostic) &&
              State(document, "lamp") == "on",
          "show makes the target a hit again");
    Check(Play("<Behavior><State Name=\"lamp\" Value=\"off\"/>"
               "<View Id=\"page\" X=\"0\" Y=\"0\" Width=\"40\" Height=\"20\">"
               "<View Id=\"lamp\" X=\"4\" Y=\"4\" Width=\"10\" Height=\"8\" OnClick=\"light\" Enabled=\"true\"/>"
               "</View><Action Name=\"light\"><SetState Name=\"lamp\" Value=\"on\"/></Action>"
               "<Script><Disable Id=\"lamp\"/><Click X=\"6\" Y=\"6\"/><Enable Id=\"lamp\"/><Click X=\"6\" Y=\"6\"/>"
               "</Script></Behavior>",
               document, trace, diagnostic) &&
              State(document, "lamp") == "on" && RoboActionFind(document, "lamp")->enabled,
          "disable skips the click and enable restores it");
    Check(Play("<Behavior><View Id=\"lamp\" X=\"0\" Y=\"0\" Width=\"8\" Height=\"8\"/><Script>"
               "<Toggle Id=\"lamp\"/><Toggle Id=\"lamp\" Name=\"enabled\"/></Script></Behavior>",
               document, trace, diagnostic) &&
              RoboActionFind(document, "lamp")->visible == false && RoboActionFind(document, "lamp")->enabled == false,
          "toggle flips visibility and then enabled");
}

void TestNavigateFocusScrollAndForm() {
    RoboActionDocument document;
    std::vector<RoboActionTrace> trace;
    std::string diagnostic;
    Check(Play("<Behavior>"
               "<Screen Id=\"home\" Path=\"home\" X=\"0\" Y=\"0\" Width=\"20\" Height=\"20\"/>"
               "<Screen Id=\"dock\" Path=\"dock\" X=\"0\" Y=\"0\" Width=\"20\" Height=\"20\"/>"
               "<Script><Navigate Path=\"dock\"/></Script></Behavior>",
               document, trace, diagnostic) &&
              document.path == "dock" && RoboActionFind(document, "dock")->visible && !RoboActionFind(document, "home")->visible,
          "navigate changes the screen path");
    Check(!Play("<Behavior><Screen Id=\"home\" Path=\"home\" Width=\"8\" Height=\"8\"/>"
                "<Script><Navigate Path=\"missing\"/></Script></Behavior>",
                document, trace, diagnostic) &&
              diagnostic.find("R8402") != std::string::npos,
          "an unknown path is rejected");
    Check(Play("<Behavior><State Name=\"key\" Value=\"\"/>"
               "<Input Id=\"field\" X=\"0\" Y=\"0\" Width=\"8\" Height=\"8\" OnKeyDown=\"typed\"/>"
               "<Action Name=\"typed\"><SetState Name=\"key\" Value=\"enter\"/></Action>"
               "<Script><Focus Id=\"field\"/><KeyDown Key=\"Enter\"/><Blur Id=\"field\"/></Script></Behavior>",
               document, trace, diagnostic) &&
              State(document, "key") == "enter" && document.events.focusId.empty() && FindKind(trace, "Focus", 0)->id == "field",
          "focus then a key runs the field action and blur clears it");
    Check(Play("<Behavior><ScrollView Id=\"page\" X=\"0\" Y=\"0\" Width=\"20\" Height=\"10\" Offset=\"0\"/>"
               "<Script><ScrollTo Id=\"page\" Offset=\"16\"/></Script></Behavior>",
               document, trace, diagnostic) &&
              RoboActionFind(document, "page")->scroll == 16,
          "scroll to moves the offset");
    Check(Play("<Behavior><Input Id=\"field\" Value=\"\" Required=\"true\" Width=\"8\" Height=\"8\"/>"
               "<Script><Validate Id=\"field\"/><Submit Id=\"field\"/></Script></Behavior>",
               document, trace, diagnostic) &&
              RoboActionFind(document, "field")->valid == false && !RoboActionFind(document, "field")->submitted &&
              FindKind(trace, "Submit", 0)->value == "false",
          "an empty required field does not submit");
    Check(Play("<Behavior><Input Id=\"field\" Value=\"on\" Required=\"true\" Width=\"8\" Height=\"8\"/>"
               "<Script><Submit Id=\"field\"/></Script></Behavior>",
               document, trace, diagnostic) &&
              RoboActionFind(document, "field")->submitted && FindKind(trace, "Submit", 0)->value == "on",
          "a filled field submits its value");
    Check(Play("<Behavior><State Name=\"lamp\" Value=\"off\"/>"
               "<Input Id=\"field\" Value=\"on\" Required=\"true\" Width=\"8\" Height=\"8\" Enabled=\"true\" Offset=\"0\"/>"
               "<Script><SetState Name=\"lamp\" Value=\"on\"/><Disable Id=\"field\"/><ScrollTo Id=\"field\" Offset=\"9\"/>"
               "<Submit Id=\"field\"/><Reset Name=\"lamp\"/><Reset Id=\"field\"/></Script></Behavior>",
               document, trace, diagnostic) &&
              State(document, "lamp") == "off" && RoboActionFind(document, "field")->enabled &&
              RoboActionFind(document, "field")->scroll == 0 && !RoboActionFind(document, "field")->submitted,
          "reset restores the state and the field");
}

}  // namespace

int main() {
    TestSchema();
    TestClickSetsState();
    TestSequenceAndParallel();
    TestConditionalAndVisibility();
    TestNavigateFocusScrollAndForm();
    std::cout << gPassed << " passed, " << gFailed << " failed\n";
    return gFailed == 0 ? 0 : 1;
}
