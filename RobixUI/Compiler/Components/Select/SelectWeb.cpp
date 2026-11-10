#include "SelectWeb.hpp"

#include "../../RoboCompilerShared.hpp"

#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool RoboWebSelectFlag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

bool RoboWebSelectOptionName(const std::string& name) {
  return name == "option" || name == "selectoption";
}

std::string RoboWebSelectVariant(const RoboXMLNode& node) {
  const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "outline"));
  if (variant == "subtle" || variant == "ghost" || variant == "underline" || variant == "pill") {
    return variant;
  }
  return "outline";
}

std::string RoboWebSelectSize(const RoboXMLNode& node) {
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size"));
  if (size == "sm" || size == "small") {
    return "sm";
  }
  if (size == "lg" || size == "large") {
    return "lg";
  }
  if (size == "md" || size == "medium") {
    return "md";
  }
  if (RoboWebSelectFlag(RoboXMLAttr(node, "compact"))) {
    return "sm";
  }
  return "md";
}

struct RoboWebSelectChoice {
  std::string value;
  std::string label;
  std::string description;
  std::string meta;
  std::string icon;
  std::string keywords;
  std::string fit;
  bool disabled = false;
};

std::vector<RoboWebSelectChoice> RoboWebSelectChoices(const RoboXMLNode& node) {
  std::vector<RoboWebSelectChoice> choices;
  if (RoboWebSelectFlag(RoboXMLAttr(node, "allowClear"))) {
    RoboWebSelectChoice clear;
    clear.label = RoboXMLAttr(node, "clearLabel", "Not set");
    clear.icon = RoboXMLAttr(node, "icon");
    choices.push_back(clear);
  }
  for (const RoboXMLNode& child : node.children) {
    if (!RoboWebSelectOptionName(RoboWebLower(child.name))) {
      continue;
    }
    RoboWebSelectChoice choice;
    choice.value = RoboXMLAttr(child, "value");
    choice.label = RoboXMLAttr(child, "label");
    if (choice.label.empty()) {
      choice.label = child.text;
    }
    choice.description = RoboXMLAttr(child, "description");
    choice.meta = RoboXMLAttr(child, "meta");
    choice.icon = RoboXMLAttr(child, "icon");
    choice.keywords = RoboXMLAttr(child, "keywords");
    choice.fit = RoboWebLower(RoboXMLAttr(child, "fit"));
    choice.disabled = RoboWebSelectFlag(RoboXMLAttr(child, "disabled"));
    choices.push_back(choice);
  }
  return choices;
}

void RoboWebSelectIcon(std::string& html, const fs::path& assetsRoot, const std::string& icon) {
  if (icon.empty()) {
    return;
  }
  html += RoboWebSvg(assetsRoot, icon);
}

}  // namespace

void RoboWebEmitSelect(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                       const fs::path& assetsRoot) {
  const std::string id = RoboXMLAttr(node, "id");
  const std::string variant = RoboWebSelectVariant(node);
  const std::string size = RoboWebSelectSize(node);
  const std::string preview = RoboWebLower(RoboXMLAttr(node, "preview"));
  const std::string value = RoboXMLAttr(node, "value");
  const std::string placeholder = RoboXMLAttr(node, "placeholder", "Select");
  const std::string label = RoboXMLAttr(node, "label");
  const std::string ariaLabel = RoboXMLAttr(node, "ariaLabel");
  const std::string fieldIcon = RoboXMLAttr(node, "icon");
  const std::string searchPlaceholder = RoboXMLAttr(node, "searchPlaceholder", "Search");
  const std::string emptyMessage = RoboXMLAttr(node, "emptyMessage", "No results found");
  const bool searchable = RoboWebSelectFlag(RoboXMLAttr(node, "searchable"));
  const bool invalid = RoboWebSelectFlag(RoboXMLAttr(node, "invalid"));
  const bool valid = !invalid && RoboWebSelectFlag(RoboXMLAttr(node, "valid"));
  const bool disabled = RoboWebSelectFlag(RoboXMLAttr(node, "disabled"));
  const bool compact = RoboWebSelectFlag(RoboXMLAttr(node, "compact"));
  const bool deferIcons = RoboWebSelectFlag(RoboXMLAttr(node, "deferIcons"));
  const bool openPreview = preview == "open";
  const std::vector<RoboWebSelectChoice> choices = RoboWebSelectChoices(node);
  const RoboWebSelectChoice* selected = nullptr;
  for (const RoboWebSelectChoice& choice : choices) {
    if (choice.value == value) {
      selected = &choice;
      break;
    }
  }
  const std::string shown = selected != nullptr ? selected->label : placeholder;
  const std::string leading = selected != nullptr && !selected->icon.empty() ? selected->icon : fieldIcon;
  const std::string listId = id.empty() ? "robo-select-list" : id + "List";
  const std::string style = RoboWebStyle(node, library);
  const std::string name = !ariaLabel.empty() ? ariaLabel : label;

  const std::string rootId = id.empty() ? listId + "Root" : id + "Root";
  html += "<div class=\"robo-select\" id=\"";
  html += RoboWebEscape(rootId);
  html += "\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\" data-size=\"";
  html += RoboWebEscape(size);
  html += "\"";
  if (compact) {
    html += " data-compact=\"true\"";
  }
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += ">";
  if (!label.empty()) {
    html += "<span class=\"robo-select-label robo-caption robo-type-caption\">";
    html += RoboWebEscape(label);
    html += "</span>";
  }
  html += "<button class=\"robo-select-trigger\" type=\"button\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\" data-size=\"";
  html += RoboWebEscape(size);
  html += "\" aria-haspopup=\"listbox\" aria-expanded=\"";
  html += openPreview ? "true" : "false";
  html += "\" aria-controls=\"";
  html += RoboWebEscape(listId);
  html += "\"";
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  if (!name.empty()) {
    html += " aria-label=\"";
    html += RoboWebEscape(name);
    html += "\"";
  }
  if (invalid) {
    html += " aria-invalid=\"true\"";
  }
  if (valid) {
    html += " data-valid=\"true\"";
  }
  if (disabled) {
    html += " disabled";
  }
  if (preview == "hover" || preview == "open") {
    html += " data-preview=\"";
    html += RoboWebEscape(preview);
    html += "\"";
  }
  if (openPreview) {
    html += " data-open=\"true\"";
  }
  html += ">";
  html += "<span class=\"robo-select-leading\"";
  if (leading.empty()) {
    html += " hidden";
  }
  if (selected != nullptr && selected->fit == "flag") {
    html += " data-fit=\"flag\"";
  }
  html += ">";
  RoboWebSelectIcon(html, assetsRoot, leading);
  html += "</span>";
  html += "<span class=\"robo-select-fallback\" hidden>";
  RoboWebSelectIcon(html, assetsRoot, fieldIcon);
  html += "</span>";
  html += "<span class=\"robo-select-value\"";
  if (selected == nullptr) {
    html += " data-placeholder=\"true\"";
  }
  html += ">";
  html += RoboWebEscape(shown);
  html += "</span>";
  html += "<span class=\"robo-select-chevron\" aria-hidden=\"true\">";
  html += RoboWebSvg(assetsRoot, "ChevronDown");
  html += "</span></button>";

  html += "<div class=\"robo-select-menu\" id=\"";
  html += RoboWebEscape(listId);
  html += "\" data-owner=\"";
  html += RoboWebEscape(rootId);
  html += "\" data-size=\"";
  html += RoboWebEscape(size);
  if (compact) {
    html += "\" data-compact=\"true";
  }
  html += "\" role=\"listbox\" hidden";
  if (!name.empty()) {
    html += " aria-label=\"";
    html += RoboWebEscape(name);
    html += "\"";
  }
  html += ">";
  if (searchable) {
    html += "<div class=\"robo-select-search\"><span class=\"robo-select-search-icon\" aria-hidden=\"true\">";
    html += RoboWebSvg(assetsRoot, "MagnifyingGlassFind");
    html += "</span><input class=\"robo-select-search-input\" type=\"search\" placeholder=\"";
    html += RoboWebEscape(searchPlaceholder);
    html += "\" autocomplete=\"off\" spellcheck=\"false\"></div>";
  }
  html += "<div class=\"robo-select-list robo-scroll\" data-keyboard=\"false\" data-scrollx=\"false\">";
  for (const RoboWebSelectChoice& choice : choices) {
    const bool on = selected != nullptr && choice.value == selected->value;
    html += "<button class=\"robo-select-option\" type=\"button\" role=\"option\" data-value=\"";
    html += RoboWebEscape(choice.value);
    html += "\" data-label=\"";
    html += RoboWebEscape(choice.label);
    html += "\" aria-selected=\"";
    html += on ? "true" : "false";
    html += "\"";
    if (!choice.description.empty()) {
      html += " data-description=\"";
      html += RoboWebEscape(choice.description);
      html += "\"";
    }
    if (!choice.meta.empty()) {
      html += " data-meta=\"";
      html += RoboWebEscape(choice.meta);
      html += "\"";
    }
    if (!choice.keywords.empty()) {
      html += " data-keywords=\"";
      html += RoboWebEscape(choice.keywords);
      html += "\"";
    }
    if (choice.disabled) {
      html += " disabled aria-disabled=\"true\"";
    }
    html += "><span class=\"robo-select-option-icon\"";
    if (choice.icon.empty()) {
      html += " hidden";
    }
    if (!choice.fit.empty()) {
      html += " data-fit=\"";
      html += RoboWebEscape(choice.fit);
      html += "\"";
    }
    if (deferIcons && choice.fit == "flag" && !choice.icon.empty()) {
      html += " data-icon=\"";
      html += RoboWebEscape(choice.icon);
      html += "\"></span>";
    } else {
      html += ">";
      RoboWebSelectIcon(html, assetsRoot, choice.icon);
      html += "</span>";
    }
    html += "<span class=\"robo-select-option-copy\"><span class=\"robo-select-option-line\"><span class=\"robo-select-option-name\">";
    html += RoboWebEscape(choice.label);
    html += "</span>";
    if (!choice.meta.empty()) {
      html += "<span class=\"robo-select-option-meta\">";
      html += RoboWebEscape(choice.meta);
      html += "</span>";
    }
    html += "</span>";
    if (!choice.description.empty()) {
      html += "<span class=\"robo-select-option-desc\">";
      html += RoboWebEscape(choice.description);
      html += "</span>";
    }
    html += "</span><span class=\"robo-select-check\" aria-hidden=\"true\">";
    html += RoboWebSvg(assetsRoot, "TaskCompleteCheckmark");
    html += "</span></button>";
  }
  html += "</div><div class=\"robo-select-empty\" hidden>";
  if (searchable) {
    html += "<span class=\"robo-select-empty-icon\" aria-hidden=\"true\">";
    html += RoboWebSvg(assetsRoot, "MagnifyingGlassFind");
    html += "</span>";
  }
  html += "<span class=\"robo-select-empty-title\">";
  html += RoboWebEscape(emptyMessage);
  html += "</span></div></div></div>";
}

void RoboWebAppendSelectCss(std::ostream& css) {
  css << ".robo-select{display:flex;flex-direction:column;gap:4px;width:100%;min-width:0;box-sizing:border-box;"
         "font-family:var(--font-sans,system-ui,sans-serif)}\n";
  css << ".robo-select[data-compact=\"true\"]{width:auto;min-width:7.5rem}\n";
  css << ".robo-select-label{margin:0;color:var(--text-secondary)}\n";
  css << ".robo-select-trigger{box-sizing:border-box;display:flex;align-items:center;width:100%;min-width:0;margin:0;"
         "height:44px;gap:12px;padding:0 14px 0 16px;border:1px solid var(--border-default);border-radius:12px;"
         "background:var(--bg-secondary);color:var(--text-primary);font:400 15px/1.35 var(--font-sans,system-ui,sans-serif);"
         "text-align:left;cursor:pointer;outline:2px solid transparent;outline-offset:1px;"
         "transition:border-color 150ms cubic-bezier(0.28,0.11,0.32,1),background-color 150ms "
         "cubic-bezier(0.28,0.11,0.32,1),outline-color 150ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << "html[data-theme=\"light\"] .robo-select-trigger[data-variant=\"outline\"],"
         "html[data-theme=\"light\"] .robo-select-trigger[data-variant=\"pill\"]{background:var(--bg-elevated)}\n";
  css << ".robo-select-trigger[data-size=\"sm\"]{height:36px;gap:8px;padding:0 10px 0 12px;border-radius:10px;font-size:13px}\n";
  css << ".robo-select-trigger[data-size=\"lg\"]{height:50px;padding:0 16px 0 20px;font-size:17px}\n";
  css << ".robo-select-leading,.robo-select-chevron,.robo-select-option-icon{display:inline-flex;"
         "flex:0 0 auto;align-items:center;justify-content:center;color:var(--text-tertiary)}\n";
  css << ".robo-select-leading[hidden],.robo-select-option-icon[hidden],.robo-select-fallback[hidden]{display:none !important}\n";
  css << ".robo-select-leading svg,.robo-select-option-icon svg{width:18px;height:18px;display:block}\n";
  css << ".robo-select-leading[data-fit=\"flag\"],.robo-select-option-icon[data-fit=\"flag\"]{width:24px;height:18px;"
         "overflow:hidden;border-radius:3px}\n";
  css << ".robo-select-leading[data-fit=\"flag\"] svg,.robo-select-option-icon[data-fit=\"flag\"] svg{width:24px;height:18px}\n";
  css << ".robo-select[data-compact=\"true\"] .robo-select-leading[data-fit=\"flag\"],"
         ".robo-select[data-compact=\"true\"] .robo-select-option-icon[data-fit=\"flag\"],"
         ".robo-select-menu[data-compact=\"true\"] .robo-select-option-icon[data-fit=\"flag\"]{width:16px;height:12px}\n";
  css << ".robo-select[data-compact=\"true\"] .robo-select-leading[data-fit=\"flag\"] svg,"
         ".robo-select[data-compact=\"true\"] .robo-select-option-icon[data-fit=\"flag\"] svg,"
         ".robo-select-menu[data-compact=\"true\"] .robo-select-option-icon[data-fit=\"flag\"] svg{width:16px;height:12px}\n";
  css << ".robo-select-chevron{transition:transform 200ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << ".robo-select-chevron svg{width:16px;height:16px;display:block}\n";
  css << ".robo-select-trigger[data-size=\"sm\"] .robo-select-chevron svg{width:14px;height:14px}\n";
  css << ".robo-select-trigger[data-size=\"lg\"] .robo-select-chevron svg{width:18px;height:18px}\n";
  css << ".robo-select-trigger[data-open=\"true\"] .robo-select-chevron,"
         ".robo-select-trigger[data-preview=\"open\"] .robo-select-chevron{transform:rotate(180deg)}\n";
  css << ".robo-select-value{min-width:0;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}\n";
  css << ".robo-select-value[data-placeholder=\"true\"]{color:var(--text-tertiary)}\n";
  css << ".robo-select-trigger[data-variant=\"outline\"]:hover:not(:disabled),"
         ".robo-select-trigger[data-variant=\"pill\"]:hover:not(:disabled),"
         ".robo-select-trigger[data-variant=\"underline\"]:hover:not(:disabled),"
         ".robo-select-trigger[data-variant=\"outline\"][data-preview=\"hover\"],"
         ".robo-select-trigger[data-variant=\"pill\"][data-preview=\"hover\"],"
         ".robo-select-trigger[data-variant=\"underline\"][data-preview=\"hover\"]{border-color:var(--border-strong)}\n";
  css << ".robo-select-trigger[data-variant=\"subtle\"]{border-color:transparent;background:var(--bg-tertiary)}\n";
  css << ".robo-select-trigger[data-variant=\"ghost\"]{border-color:transparent;background:transparent}\n";
  css << ".robo-select-trigger[data-variant=\"subtle\"]:hover:not(:disabled),"
         ".robo-select-trigger[data-variant=\"ghost\"]:hover:not(:disabled),"
         ".robo-select-trigger[data-variant=\"subtle\"][data-preview=\"hover\"],"
         ".robo-select-trigger[data-variant=\"ghost\"][data-preview=\"hover\"]{background:var(--bg-hover)}\n";
  css << ".robo-select-trigger[data-variant=\"underline\"]{border:0;border-bottom:1px solid var(--border-default);"
         "border-radius:0;background:transparent;outline:none}\n";
  css << ".robo-select-trigger[data-variant=\"pill\"]{border-radius:980px}\n";
  css << ".robo-select-trigger:focus-visible,.robo-select-trigger[data-open=\"true\"],"
         ".robo-select-trigger[data-preview=\"open\"]{border-color:var(--brand-primary);"
         "outline-color:var(--brand-primary-focus-ring)}\n";
  css << ".robo-select-trigger[data-variant=\"subtle\"][data-open=\"true\"],"
         ".robo-select-trigger[data-variant=\"ghost\"][data-open=\"true\"],"
         ".robo-select-trigger[data-variant=\"subtle\"][data-preview=\"open\"],"
         ".robo-select-trigger[data-variant=\"ghost\"][data-preview=\"open\"],"
         ".robo-select-trigger[data-variant=\"subtle\"]:focus-visible,"
         ".robo-select-trigger[data-variant=\"ghost\"]:focus-visible{background:var(--bg-secondary)}\n";
  css << "html[data-theme=\"light\"] .robo-select-trigger[data-variant=\"subtle\"][data-open=\"true\"],"
         "html[data-theme=\"light\"] .robo-select-trigger[data-variant=\"ghost\"][data-open=\"true\"],"
         "html[data-theme=\"light\"] .robo-select-trigger[data-variant=\"subtle\"][data-preview=\"open\"],"
         "html[data-theme=\"light\"] .robo-select-trigger[data-variant=\"ghost\"][data-preview=\"open\"],"
         "html[data-theme=\"light\"] .robo-select-trigger[data-variant=\"subtle\"]:focus-visible,"
         "html[data-theme=\"light\"] .robo-select-trigger[data-variant=\"ghost\"]:focus-visible{background:var(--bg-elevated)}\n";
  css << ".robo-select-trigger[data-variant=\"underline\"]:focus-visible,"
         ".robo-select-trigger[data-variant=\"underline\"][data-open=\"true\"],"
         ".robo-select-trigger[data-variant=\"underline\"][data-preview=\"open\"]{border-bottom-color:var(--brand-primary);"
         "outline:none}\n";
  css << ".robo-select-trigger[aria-invalid=\"true\"]{border-color:var(--danger);outline-color:transparent}\n";
  css << ".robo-select-trigger[aria-invalid=\"true\"]:focus-visible,.robo-select-trigger[aria-invalid=\"true\"][data-open=\"true\"]"
         "{border-color:var(--danger);outline-color:var(--danger-focus-ring)}\n";
  css << ".robo-select-trigger[data-valid=\"true\"]{border-color:var(--success);outline-color:transparent}\n";
  css << ".robo-select-trigger[data-valid=\"true\"]:focus-visible,.robo-select-trigger[data-valid=\"true\"][data-open=\"true\"]"
         "{border-color:var(--success);outline-color:var(--success-focus-ring)}\n";
  css << ".robo-select-trigger:disabled{opacity:0.55;cursor:not-allowed}\n";
  css << ".robo-select-menu{position:fixed;z-index:400;box-sizing:border-box;display:flex;flex-direction:column;"
         "overflow:hidden;margin:0;padding:10px;border:1px solid var(--border-default);border-radius:16px;"
         "background:var(--bg-secondary);color:var(--text-primary);transform-origin:top center;"
         "animation:robo-select-in 180ms cubic-bezier(0.28,0.11,0.32,1) both}\n";
  css << ".robo-select-menu[data-up=\"true\"]{transform-origin:bottom center}\n";
  css << "@keyframes robo-select-in{from{opacity:0;transform:scale(0.96)}to{opacity:1;transform:scale(1)}}\n";
  css << "html[data-theme=\"light\"] .robo-select-menu{background:var(--bg-elevated)}\n";
  css << ".robo-select-menu[hidden]{display:none}\n";
  css << ".robo-select-search{position:relative;margin:0 0 8px;flex:0 0 auto}\n";
  css << ".robo-select-search-icon{position:absolute;top:50%;left:12px;display:inline-flex;color:var(--text-tertiary);"
         "transform:translateY(-50%);pointer-events:none}\n";
  css << ".robo-select-search-icon svg{width:16px;height:16px;display:block}\n";
  css << ".robo-select-search-input{box-sizing:border-box;display:block;width:100%;height:36px;margin:0;padding:0 12px 0 36px;"
         "border:1px solid var(--border-default);border-radius:10px;background:var(--bg-hover);color:var(--text-primary);"
         "caret-color:var(--text-primary);font:400 14px/34px var(--font-sans,system-ui,sans-serif);"
         "outline:2px solid transparent;outline-offset:1px}\n";
  css << ".robo-select-search-input::placeholder{color:var(--text-tertiary)}\n";
  css << ".robo-select-search-input:focus{border-color:var(--brand-primary);outline-color:var(--brand-primary-focus-ring)}\n";
  css << ".robo-select-menu > .robo-scroll-host{position:relative;flex:1 1 auto;min-width:0;min-height:0;max-height:320px;overflow:visible}\n";
  css << ".robo-select-menu .robo-scroll-host[hidden]{display:none !important}\n";
  css << ".robo-select-list{box-sizing:border-box;min-height:0;flex:1 1 auto;max-height:320px;"
         "padding-right:10px;overflow-x:hidden;overflow-y:auto;overscroll-behavior:contain}\n";
  css << ".robo-select-option[hidden]{display:none !important}\n";
  css << ".robo-select-option{box-sizing:border-box;appearance:none;display:flex;align-items:center;width:100%;"
         "min-height:44px;gap:12px;margin:0;padding:0 10px;border:0;border-radius:10px;background:transparent;"
         "background-image:none;color:var(--text-primary);font:400 15px/1.35 var(--font-sans,system-ui,sans-serif);"
         "text-align:left;cursor:pointer;outline:none;"
         "transition:background-color 150ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << ".robo-select[data-size=\"sm\"] .robo-select-option,.robo-select-menu[data-size=\"sm\"] .robo-select-option"
         "{min-height:32px;gap:8px;padding:0 8px;font-size:13px}\n";
  css << ".robo-select-option[data-description]{min-height:52px;padding-top:8px;padding-bottom:8px}\n";
  css << ".robo-select-option[aria-selected=\"true\"]{background:transparent;color:var(--text-primary)}\n";
  css << ".robo-select-option:hover,.robo-select-option:focus-visible,.robo-select-option[data-active=\"true\"]"
         "{background:var(--bg-hover);outline:none}\n";
  css << ".robo-select-option:active:not(:disabled){background:var(--bg-active)}\n";
  css << ".robo-select-option:disabled{opacity:0.48;cursor:not-allowed}\n";
  css << ".robo-select-option:disabled:hover,.robo-select-option:disabled:focus-visible,"
         ".robo-select-option:disabled[data-active=\"true\"]{background:transparent}\n";
  css << ".robo-select-option-copy{display:flex;min-width:0;flex:1;flex-direction:column;gap:2px}\n";
  css << ".robo-select-option-line{display:flex;min-width:0;align-items:baseline;justify-content:space-between;gap:12px}\n";
  css << ".robo-select-option-name,.robo-select-option-desc{min-width:0;overflow:hidden;text-overflow:ellipsis;"
         "white-space:nowrap;font-weight:400}\n";
  css << ".robo-select-option[aria-selected=\"true\"] .robo-select-option-name{color:var(--brand-primary);font-weight:500}\n";
  css << ".robo-select-option-desc{color:var(--text-tertiary);font-size:13px;font-weight:400}\n";
  css << ".robo-select-option-meta{flex:0 0 auto;color:var(--text-tertiary);font-size:12px;font-weight:500;"
         "letter-spacing:0.02em}\n";
  css << ".robo-select-check{display:none;flex:0 0 auto;align-items:center;justify-content:center;color:var(--brand-primary)}\n";
  css << ".robo-select-option[aria-selected=\"true\"] > .robo-select-check{display:inline-flex}\n";
  css << ".robo-select-check svg{width:16px;height:16px;display:block}\n";
  css << ".robo-select[data-size=\"sm\"] .robo-select-check svg,.robo-select-menu[data-size=\"sm\"] .robo-select-check svg"
         "{width:14px;height:14px}\n";
  css << ".robo-select-empty{display:flex;flex-direction:column;align-items:center;padding:20px 12px;text-align:center}\n";
  css << ".robo-select-empty[hidden]{display:none !important}\n";
  css << ".robo-select-empty-icon{display:inline-flex;margin:0 0 12px;color:var(--text-tertiary)}\n";
  css << ".robo-select-empty-icon svg{width:20px;height:20px;display:block}\n";
  css << ".robo-select-empty-title{color:var(--text-primary);font:500 15px/1.35 var(--font-sans,system-ui,sans-serif)}\n";
}

const char* RoboWebSelectClass() { return "robo-select"; }

const char* RoboWebSelectScript() {
  return R"JS(
  function selectRoot(node) {
    if (!node || !node.closest) return null;
    var root = node.closest(".robo-select");
    if (root) return root;
    var menu = node.closest(".robo-select-menu");
    var owner = menu && menu.getAttribute("data-owner");
    return owner ? document.getElementById(owner) : null;
  }
  function selectMenu(root) {
    var trigger = root.querySelector(".robo-select-trigger");
    var listId = trigger && trigger.getAttribute("aria-controls");
    return listId ? document.getElementById(listId) : root.querySelector(".robo-select-menu");
  }
  function selectParts(root) {
    var menu = selectMenu(root);
    return {
      trigger: root.querySelector(".robo-select-trigger"),
      menu: menu,
      list: menu ? menu.querySelector(".robo-select-list") : null,
      search: menu ? menu.querySelector(".robo-select-search-input") : null,
      empty: menu ? menu.querySelector(".robo-select-empty") : null,
      value: root.querySelector(".robo-select-value"),
      leading: root.querySelector(".robo-select-leading"),
      fallback: root.querySelector(".robo-select-fallback")
    };
  }
  function selectOptions(root) {
    var menu = selectMenu(root);
    return menu ? menu.querySelectorAll(".robo-select-option") : [];
  }
  function selectVisible(root) {
    var all = selectOptions(root);
    var out = [];
    for (var i = 0; i < all.length; i++) if (!all[i].hidden) out.push(all[i]);
    return out;
  }
  function pageZoom() {
    var z = parseFloat(document.documentElement.style.zoom);
    if (!(z > 0) && Robo.zoom > 0) z = Robo.zoom;
    return z > 0 ? z : 1;
  }
  function selectPlace(root) {
    var part = selectParts(root);
    if (!part.trigger || !part.menu || part.menu.hidden) return;
    if (part.menu.classList.contains("robo-zoom-menu")) return;
    var z = pageZoom();
    var rect = part.trigger.getBoundingClientRect();
    var left = rect.left / z;
    var top = rect.top / z;
    var width = rect.width / z;
    var bottom = rect.bottom / z;
    var viewH = window.innerHeight / z;
    var gap = 8;
    var below = viewH - bottom - gap;
    var above = top - gap;
    var want = Math.min(360, part.menu.scrollHeight || 220);
    var up = below < want && above > below;
    part.menu.style.left = left + "px";
    part.menu.style.width = width + "px";
    var cap = 360;
    if (up) {
      part.menu.setAttribute("data-up", "true");
      part.menu.style.top = "auto";
      part.menu.style.bottom = (viewH - top + gap) + "px";
      cap = Math.min(360, Math.max(220, above - 8));
    } else {
      part.menu.removeAttribute("data-up");
      part.menu.style.bottom = "auto";
      part.menu.style.top = (bottom + gap) + "px";
      cap = Math.min(360, Math.max(220, below - 8));
    }
    part.menu.style.maxHeight = cap + "px";
    if (part.list) {
      var search = part.menu.querySelector(".robo-select-search");
      var used = 0;
      if (search) {
        var searchStyle = getComputedStyle(search);
        used += search.offsetHeight + (parseFloat(searchStyle.marginTop) || 0) + (parseFloat(searchStyle.marginBottom) || 0);
      }
      var menuStyle = getComputedStyle(part.menu);
      used += (parseFloat(menuStyle.paddingTop) || 0) + (parseFloat(menuStyle.paddingBottom) || 0);
      used += (parseFloat(menuStyle.borderTopWidth) || 0) + (parseFloat(menuStyle.borderBottomWidth) || 0);
      var room = cap - used;
      if (room < 96) room = 96;
      var host = part.list.parentElement && part.list.parentElement.classList.contains("robo-scroll-host")
        ? part.list.parentElement : part.list;
      host.style.maxHeight = room + "px";
      part.list.style.maxHeight = room + "px";
      part.list.style.overflowY = "auto";
    }
  }
  function selectFilter(root) {
    var part = selectParts(root);
    var query = part.search ? part.search.value.trim().toLowerCase() : "";
    var shown = 0;
    var options = selectOptions(root);
    for (var i = 0; i < options.length; i++) {
      var option = options[i];
      var hay = (option.getAttribute("data-label") || "") + " " + (option.getAttribute("data-value") || "") + " " +
        (option.getAttribute("data-description") || "") + " " + (option.getAttribute("data-meta") || "") + " " +
        (option.getAttribute("data-keywords") || "");
      var match = !query || hay.toLowerCase().indexOf(query) >= 0;
      option.hidden = !match;
      if (match) shown += 1;
    }
    if (part.list) {
      var host = part.list.parentElement && part.list.parentElement.classList.contains("robo-scroll-host")
        ? part.list.parentElement : part.list;
      host.hidden = shown === 0;
    }
    if (part.empty) part.empty.hidden = shown > 0;
    selectRefresh(root, false);
  }
  function selectRefresh(root, show) {
    var part = selectParts(root);
    if (!part.list || !part.list.roboPlaceScroll) return;
    requestAnimationFrame(function () {
      if (part.list.roboPlaceScroll) part.list.roboPlaceScroll(!!show);
    });
  }
  function selectActive(root, option) {
    var options = selectVisible(root);
    for (var i = 0; i < options.length; i++) options[i].removeAttribute("data-active");
    if (option) {
      option.setAttribute("data-active", "true");
      option.scrollIntoView({ block: "nearest" });
    }
  }
  function selectClose(root, focusTrigger) {
    var part = selectParts(root);
    if (!part.menu || part.menu.hidden) return;
    part.menu.hidden = true;
    if (part.trigger) {
      part.trigger.setAttribute("aria-expanded", "false");
      part.trigger.removeAttribute("data-open");
      if (focusTrigger) part.trigger.focus();
    }
    if (part.search) part.search.value = "";
    selectFilter(root);
    var options = selectOptions(root);
    for (var i = 0; i < options.length; i++) options[i].removeAttribute("data-active");
  }
  function selectCloseAll(except) {
    var roots = document.querySelectorAll(".robo-select");
    for (var i = 0; i < roots.length; i++) if (roots[i] !== except) selectClose(roots[i], false);
  }
  function selectOpen(root) {
    var part = selectParts(root);
    if (!part.trigger || part.trigger.disabled || !part.menu) return;
    selectCloseAll(root);
    if (!part.menu.classList.contains("robo-zoom-menu") && part.menu.parentNode !== document.body) document.body.appendChild(part.menu);
    part.menu.hidden = false;
    part.trigger.setAttribute("aria-expanded", "true");
    part.trigger.setAttribute("data-open", "true");
    selectFilter(root);
    selectPlace(root);
    selectRefresh(root, true);
    if (part.search) window.setTimeout(function () { part.search.focus(); }, 40);
  }
  function selectChoose(root, option) {
    if (!option || option.disabled) return;
    var part = selectParts(root);
    var options = selectOptions(root);
    for (var i = 0; i < options.length; i++) {
      options[i].setAttribute("aria-selected", options[i] === option ? "true" : "false");
      options[i].removeAttribute("data-active");
    }
    if (part.value) {
      part.value.textContent = option.getAttribute("data-label") || "";
      if (option.getAttribute("data-value")) part.value.removeAttribute("data-placeholder");
      else part.value.setAttribute("data-placeholder", "true");
    }
    if (part.leading) {
      var icon = option.querySelector(".robo-select-option-icon");
      var markup = icon && !icon.hidden ? icon.innerHTML : (part.fallback ? part.fallback.innerHTML : "");
      part.leading.innerHTML = markup;
      part.leading.hidden = !markup;
      var fit = icon && icon.getAttribute("data-fit");
      if (fit) part.leading.setAttribute("data-fit", fit);
      else part.leading.removeAttribute("data-fit");
    }
    if (part.trigger) {
      part.trigger.setAttribute("data-value", option.getAttribute("data-value") || "");
      part.trigger.dispatchEvent(new CustomEvent("robo-change", { bubbles: true, detail: { value: option.getAttribute("data-value") || "" } }));
    }
    selectClose(root, true);
  }
  document.addEventListener("click", function (ev) {
    var trigger = ev.target.closest && ev.target.closest(".robo-select-trigger");
    if (trigger) {
      var root = selectRoot(trigger);
      if (!root || trigger.disabled) return;
      var menu = selectMenu(root);
      if (menu && menu.hidden) selectOpen(root);
      else selectClose(root, false);
      return;
    }
    var option = ev.target.closest && ev.target.closest(".robo-select-option");
    if (option) {
      selectChoose(selectRoot(option), option);
      return;
    }
    if (ev.target.closest && ev.target.closest(".robo-select-menu")) return;
    selectCloseAll(null);
  });
  document.addEventListener("pointerdown", function (ev) {
    if (!ev.target.closest) return;
    if (ev.target.closest(".robo-select-trigger") || ev.target.closest(".robo-select-menu")) return;
    selectCloseAll(null);
  });
  function selectOnSearch(ev) {
    if (!ev.target.classList || !ev.target.classList.contains("robo-select-search-input")) return;
    var root = selectRoot(ev.target);
    if (!root) return;
    selectFilter(root);
    var visible = selectVisible(root);
    for (var i = 0; i < visible.length; i++) visible[i].removeAttribute("data-active");
    selectPlace(root);
  }
  document.addEventListener("input", selectOnSearch);
  document.addEventListener("search", selectOnSearch);
  document.addEventListener("keydown", function (ev) {
    var trigger = ev.target.closest && ev.target.closest(".robo-select-trigger");
    var menu = ev.target.closest && ev.target.closest(".robo-select-menu");
    var root = selectRoot(ev.target);
    if (ev.key === "Escape") {
      if (root) selectClose(root, true);
      return;
    }
    if (trigger && (ev.key === "ArrowDown" || ev.key === "Enter" || ev.key === " ")) {
      ev.preventDefault();
      var open = selectMenu(root);
      if (open && open.hidden) selectOpen(root);
      else if (ev.key === "ArrowDown") {
        var next = selectVisible(root);
        selectActive(root, next[0]);
      }
      return;
    }
    if (!menu || !root) return;
    var visible = selectVisible(root);
    if (!visible.length) return;
    var index = -1;
    for (var i = 0; i < visible.length; i++) if (visible[i].getAttribute("data-active") === "true") index = i;
    if (ev.key === "ArrowDown" || ev.key === "ArrowUp") {
      ev.preventDefault();
      var nextIndex = 0;
      if (index < 0) nextIndex = ev.key === "ArrowDown" ? 0 : visible.length - 1;
      else nextIndex = (index + (ev.key === "ArrowDown" ? 1 : -1) + visible.length) % visible.length;
      selectActive(root, visible[nextIndex]);
    } else if (ev.key === "Enter") {
      ev.preventDefault();
      var chosen = null;
      for (var n = 0; n < visible.length; n++) if (visible[n].getAttribute("data-active") === "true") chosen = visible[n];
      if (chosen) selectChoose(root, chosen);
    }
  });
  window.addEventListener("resize", function () {
    var roots = document.querySelectorAll(".robo-select");
    for (var i = 0; i < roots.length; i++) selectPlace(roots[i]);
  });
  window.addEventListener("scroll", function () {
    var roots = document.querySelectorAll(".robo-select");
    for (var i = 0; i < roots.length; i++) selectPlace(roots[i]);
  }, true);
  function placeOpenSelects() {
    var roots = document.querySelectorAll(".robo-select");
    for (var i = 0; i < roots.length; i++) selectPlace(roots[i]);
  }
  Robo.placeScrolls = Robo.placeScrolls || [];
  Robo.placeScrolls.push(placeOpenSelects);
)JS";
}
