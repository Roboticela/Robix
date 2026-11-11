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
    {"container", RoboUITier::Compiler, "Container", false},
    {"text", RoboUITier::Compiler, "Text", false},
    {"svg", RoboUITier::Compiler, "Svg", false},
    {"icon", RoboUITier::Preset, "Svg", true},
    {"image", RoboUITier::Compiler, "Image", false},
    {"background", RoboUITier::Compiler, "Background", false},
    {"button", RoboUITier::Preset, "Container", true},
    {"textfield", RoboUITier::Preset, "Text", true},
    {"textarea", RoboUITier::Preset, "Text", true},
    {"select", RoboUITier::Preset, "Container", true},
    {"switch", RoboUITier::Preset, "Container", true},
    {"segmentedcontrol", RoboUITier::Preset, "Container", true},
    {"scrollview", RoboUITier::Preset, "Container", true},
    {"scroll", RoboUITier::Preset, "Container", true},
    {"view3d", RoboUITier::Compiler, "View3D", false},
    {"scene3d", RoboUITier::Compiler, "View3D", false},

    {"span", RoboUITier::Part, "Text", false},
    {"at", RoboUITier::Part, "Container", false},
    {"option", RoboUITier::Preset, "Container", true},
    {"segment", RoboUITier::Preset, "Container", true},
    {"scrollbar", RoboUITier::Preset, "Container", true},

    {"heading", RoboUITier::Preset, "Text", true},
    {"h1", RoboUITier::Preset, "Text", true},
    {"h2", RoboUITier::Preset, "Text", true},
    {"h3", RoboUITier::Preset, "Text", true},
    {"h4", RoboUITier::Preset, "Text", true},
    {"h5", RoboUITier::Preset, "Text", true},
    {"h6", RoboUITier::Preset, "Text", true},
    {"subhead", RoboUITier::Preset, "Text", true},
    {"body", RoboUITier::Preset, "Text", true},
    {"paragraph", RoboUITier::Preset, "Text", true},
    {"p", RoboUITier::Preset, "Text", true},
    {"caption", RoboUITier::Preset, "Text", true},
    {"eyebrow", RoboUITier::Preset, "Text", true},
    {"label", RoboUITier::Preset, "Text", true},
    {"box", RoboUITier::Preset, "Container", true},
    {"flex", RoboUITier::Preset, "Container", true},
    {"stack", RoboUITier::Preset, "Container", true},
    {"canvas", RoboUITier::Preset, "Container", true},
    {"column", RoboUITier::Preset, "Container", true},
    {"row", RoboUITier::Preset, "Container", true},
    {"grid", RoboUITier::Preset, "Container", true},
    {"board", RoboUITier::Preset, "Container", true},
    {"pane", RoboUITier::Preset, "Container", true},
    {"sidebar", RoboUITier::Preset, "Container", true},
    {"paneitem", RoboUITier::Preset, "Container", true},
    {"sidebaritem", RoboUITier::Preset, "Container", true},
    {"titlebar", RoboUITier::Preset, "Container", true},
    {"navbar", RoboUITier::Preset, "Container", true},
    {"titlebaritem", RoboUITier::Preset, "Container", true},
    {"navbaritem", RoboUITier::Preset, "Container", true},
    {"anchor", RoboUITier::Preset, "Button", true},
    {"iconbutton", RoboUITier::Preset, "Button", true},
    {"passwordreveal", RoboUITier::Preset, "Button", true},
    {"confirmaction", RoboUITier::Preset, "Button", true},
    {"themetoggle", RoboUITier::Preset, "Button", true},
    {"themeicontoggle", RoboUITier::Preset, "Button", true},
    {"emailfield", RoboUITier::Preset, "TextField", true},
    {"phonefield", RoboUITier::Preset, "TextField", true},
    {"passwordfield", RoboUITier::Preset, "TextField", true},
    {"namefield", RoboUITier::Preset, "TextField", true},
    {"switchitem", RoboUITier::Preset, "Switch", true},
    {"switchgroup", RoboUITier::Preset, "Switch", true},
    {"segmentedtoolbar", RoboUITier::Preset, "SegmentedControl", true},
    {"countryselect", RoboUITier::Preset, "Select", true},
    {"picture", RoboUITier::Preset, "Image", true},
    {"slide", RoboUITier::Preset, "Image", true},
    {"brandlogo", RoboUITier::Preset, "Icon", true},
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
    return R"RXU(<Library>
  <Component Name="EmailField" Base="TextField">
    <Default Type="email" Email="true" Placeholder="Email address" Icon="EnvelopeMessageMail" AutoComplete="email" AutoCapitalize="none" AutoCorrect="off" SpellCheck="false" InputMode="email"/>
  </Component>
  <Component Name="PhoneField" Base="TextField">
    <Default Type="tel" Phone="true" Placeholder="Phone number" Icon="HandheldMobilePhone" AutoComplete="tel" AutoCapitalize="none" AutoCorrect="off" SpellCheck="false" InputMode="tel"/>
  </Component>
  <Component Name="PasswordField" Base="TextField">
    <Default Password="true" Type="password" Placeholder="Password" Icon="PadlockSecureClosed" AutoComplete="current-password" AutoCapitalize="none" AutoCorrect="off" SpellCheck="false" Reveal="text" Visible=""/>
    <PasswordReveal Variant="{Reveal}" Visible="{Visible}"/>
  </Component>
</Library>)RXU";
}
