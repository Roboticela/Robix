#include "ComponentTier.hpp"

namespace {

struct Entry {
    const char* name;
    RoboUITier tier;
    const char* base;
    bool expands;
};

const Entry kEntries[] = {
    {"screen", RoboUITier::Compiler, "Screen", false},
    {"window", RoboUITier::Compiler, "Screen", false},
    {"page", RoboUITier::Compiler, "Screen", false},
    {"view", RoboUITier::Compiler, "View", false},
    {"container", RoboUITier::Compiler, "View", false},
    {"box", RoboUITier::Compiler, "View", false},
    {"text", RoboUITier::Compiler, "Text", false},
    {"svg", RoboUITier::Compiler, "Svg", false},
    {"image", RoboUITier::Compiler, "Image", false},
    {"view3d", RoboUITier::Compiler, "View3D", false},
    {"scene3d", RoboUITier::Compiler, "View3D", false},
    {"input", RoboUITier::Compiler, "Input", false},
    {"textfield", RoboUITier::Compiler, "Input", false},
    {"scrollview", RoboUITier::Compiler, "ScrollView", false},
    {"list", RoboUITier::Compiler, "List", false},
    {"span", RoboUITier::Part, "Text", false},
    {"at", RoboUITier::Part, "View", false},
};

std::string Lower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return value;
}

const Entry* FindEntry(const std::string& name) {
    const std::string key = Lower(name);
    for (const Entry& entry : kEntries) {
        if (key == entry.name) return &entry;
    }
    return nullptr;
}

}  // namespace

RoboUITier RoboCompilerComponentTier(const std::string& name) {
    const Entry* entry = FindEntry(name);
    return entry == nullptr ? RoboUITier::Unknown : entry->tier;
}

const char* RoboCompilerComponentBase(const std::string& name) {
    const Entry* entry = FindEntry(name);
    return entry == nullptr ? nullptr : entry->base;
}

bool RoboCompilerPresetExpands(const std::string& name) {
    const Entry* entry = FindEntry(name);
    return entry != nullptr && entry->expands;
}

bool RoboCompilerAllowsComponentName(const std::string& name) {
    const std::string key = Lower(name);
    if (key == "import" || key == "component" || key == "data" || key == "item" || key == "for" || key == "if" ||
        key == "else" || key == "let" || key == "slot" || key == "default") {
        return false;
    }
    const RoboUITier tier = RoboCompilerComponentTier(key);
    return tier == RoboUITier::Unknown || tier == RoboUITier::Preset;
}

const char* RoboCompilerPresetLibrary() {
    return "<Library></Library>";
}
