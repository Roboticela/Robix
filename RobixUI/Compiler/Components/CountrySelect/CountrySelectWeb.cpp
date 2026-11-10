#include "CountrySelectWeb.hpp"

#include "CountrySelectData.hpp"
#include "../Select/SelectWeb.hpp"

#include <cstdio>
#include <string>
#include <unordered_set>

namespace {

bool RoboCountryWebFlag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

std::unordered_set<std::string> RoboCountryWebEnabled(const std::string& list) {
  std::unordered_set<std::string> enabled;
  std::string token;
  const auto push = [&]() {
    if (!token.empty()) {
      for (char& ch : token) {
        if (ch >= 'a' && ch <= 'z') {
          ch = static_cast<char>(ch - 'a' + 'A');
        }
      }
      enabled.insert(token);
      token.clear();
    }
  };
  for (char ch : list) {
    if (ch == ',' || ch == ';' || ch == ' ') {
      push();
    } else {
      token.push_back(ch);
    }
  }
  push();
  return enabled;
}

std::string RoboCountryJsString(const std::string& text) {
  std::string out;
  out.push_back('"');
  for (unsigned char ch : text) {
    if (ch == '\\' || ch == '"') {
      out.push_back('\\');
      out.push_back(static_cast<char>(ch));
    } else if (ch == '\n') {
      out += "\\n";
    } else if (ch == '\r') {
      continue;
    } else if (ch == '<') {
      out += "\\u003c";
    } else if (ch < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
      out += buf;
    } else {
      out.push_back(static_cast<char>(ch));
    }
  }
  out.push_back('"');
  return out;
}

}  // namespace

void RoboWebEmitCountrySelect(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                              const std::filesystem::path& assetsRoot) {
  RoboXMLNode select = node;
  select.name = "Select";
  select.children.clear();
  for (auto it = select.attrs.begin(); it != select.attrs.end();) {
    const std::string key = RoboWebLower(it->first);
    if (key == "label" || key == "enabledisos") {
      it = select.attrs.erase(it);
    } else {
      ++it;
    }
  }
  const std::string aria = RoboXMLAttr(node, "ariaLabel");
  select.attrs["ariaLabel"] = aria.empty() ? RoboXMLAttr(node, "label", "Country") : aria;
  if (RoboXMLAttr(select, "placeholder").empty()) {
    select.attrs["placeholder"] = "Select a country";
  }
  bool allowClearSet = false;
  for (const auto& entry : node.attrs) {
    if (RoboWebLower(entry.first) == "allowclear") {
      allowClearSet = true;
    }
  }
  if (!allowClearSet) {
    select.attrs["allowClear"] = "true";
  }
  if (RoboXMLAttr(select, "searchPlaceholder").empty()) {
    select.attrs["searchPlaceholder"] = "Search countries";
  }
  if (RoboXMLAttr(select, "emptyMessage").empty()) {
    select.attrs["emptyMessage"] = "No countries found";
  }
  if (RoboXMLAttr(select, "clearLabel").empty()) {
    select.attrs["clearLabel"] = "Not set";
  }
  select.attrs["searchable"] = "true";
  select.attrs["deferIcons"] = "true";
  select.attrs["icon"] = "GlobeWorldEarth";

  const bool allowClear = !allowClearSet || RoboCountryWebFlag(RoboXMLAttr(node, "allowClear"));
  if (!allowClear) {
    select.attrs["allowClear"] = "false";
  }
  const auto enabled = RoboCountryWebEnabled(RoboXMLAttr(node, "enabledIsos"));
  select.children.reserve(static_cast<std::size_t>(kRoboCountryCount));
  for (int index = 0; index < kRoboCountryCount; ++index) {
    const RoboCountryRow& country = kRoboCountries[index];
    const bool available = enabled.empty() || enabled.find(country.iso) != enabled.end();
    RoboXMLNode option;
    option.name = "Option";
    option.attrs["value"] = country.iso;
    option.attrs["label"] = country.name;
    option.attrs["icon"] = std::string("Flag") + country.iso;
    option.attrs["fit"] = "flag";
    option.attrs["meta"] = available ? country.iso : "Coming soon";
    option.attrs["keywords"] = std::string(country.dial) + " +" + country.dial;
    if (!available) {
      option.attrs["disabled"] = "true";
    }
    select.children.push_back(std::move(option));
  }
  RoboWebEmitSelect(html, select, library, assetsRoot);
}

std::string RoboWebCountrySelectScript(const std::filesystem::path& assetsRoot) {
  std::string js = "  Robo.flags = Robo.flags || {};\n";
  for (int index = 0; index < kRoboCountryCount; ++index) {
    const std::string name = std::string("Flag") + kRoboCountries[index].iso;
    const std::string svg = RoboWebSvg(assetsRoot, name);
    if (svg.empty()) {
      continue;
    }
    js += "  Robo.flags[";
    js += RoboCountryJsString(name);
    js += "] = ";
    js += RoboCountryJsString(svg);
    js += ";\n";
  }
  js += R"JS(
  function roboFillFlags(scope) {
    if (!scope || scope.getAttribute("data-flags") === "ready") return;
    var slots = scope.querySelectorAll("[data-icon]");
    var filled = 0;
    for (var i = 0; i < slots.length; i++) {
      var slot = slots[i];
      if (slot.getAttribute("data-filled") === "1") { filled += 1; continue; }
      var svg = Robo.flags[slot.getAttribute("data-icon")];
      if (!svg) continue;
      slot.innerHTML = svg;
      slot.setAttribute("data-filled", "1");
      filled += 1;
    }
    if (filled === slots.length) scope.setAttribute("data-flags", "ready");
  }
  document.addEventListener("pointerdown", function (ev) {
    var trigger = ev.target.closest && ev.target.closest(".robo-select-trigger");
    if (!trigger) return;
    roboFillFlags(document.getElementById(trigger.getAttribute("aria-controls") || ""));
  }, true);
  document.addEventListener("keydown", function (ev) {
    var trigger = ev.target.closest && ev.target.closest(".robo-select-trigger");
    var menu = ev.target.closest && ev.target.closest(".robo-select-menu");
    var id = trigger ? trigger.getAttribute("aria-controls") : (menu ? menu.id : "");
    if (id) roboFillFlags(document.getElementById(id));
  });
)JS";
  return js;
}
