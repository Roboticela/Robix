#include "SegmentedControlWeb.hpp"

#include "../Button/ButtonWeb.hpp"
#include "../../RoboCompilerShared.hpp"
#include "../../Motion.hpp"
#include "../../ScreenSize.hpp"

#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool RoboWebFlag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

std::string RoboWebSegmentVariant(const RoboXMLNode& node) {
  const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "subtle"));
  if (variant == "outline" || variant == "ghost" || variant == "primary" || variant == "pill") {
    return variant;
  }
  return "subtle";
}

std::string RoboWebSegmentSize(const RoboXMLNode& node) {
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "md"));
  if (size == "small" || size == "sm") {
    return "sm";
  }
  if (size == "large" || size == "lg") {
    return "lg";
  }
  return "md";
}

std::string RoboWebSegmentContent(const RoboXMLNode& node) {
  const std::string content = RoboWebLower(RoboXMLAttr(node, "content", "label"));
  if (content == "icon" || content == "both") {
    return content;
  }
  return "label";
}

std::string RoboWebSegmentMode(const RoboXMLNode& node) {
  return RoboWebLower(RoboXMLAttr(node, "mode", "inset")) == "grouped" ? "grouped" : "inset";
}

int RoboWebSegmentSide(const RoboXMLNode& node) {
  const std::string side =
      RoboWebLower(RoboXMLAttr(node, "slot", RoboXMLAttr(node, "side", RoboXMLAttr(node, "place").c_str()).c_str()));
  if (side == "trailing" || side == "right" || side == "end") {
    return 1;
  }
  if (side == "leading" || side == "left" || side == "start") {
    return 0;
  }
  return -1;
}

std::string RoboWebSegmentStatus(const RoboXMLNode& node) {
  const std::string status = RoboWebLower(RoboXMLAttr(node, "status"));
  if (status == "loading" || status == "success" || status == "error" || status == "idle") {
    return status;
  }
  if (RoboWebFlag(RoboXMLAttr(node, "loading"))) {
    return "loading";
  }
  return {};
}

std::string RoboWebSegmentSpinner() {
  return "<svg class=\"robo-segment-spinner\" viewBox=\"0 0 32 32\" fill=\"none\" aria-hidden=\"true\">"
         "<circle class=\"robo-segment-spinner-track\" cx=\"16\" cy=\"16\" r=\"14\" stroke-width=\"2\"></circle>"
         "<circle class=\"robo-segment-spinner-tick\" cx=\"16\" cy=\"16\" r=\"14\" stroke-width=\"2\" "
         "stroke-linecap=\"round\" stroke-dasharray=\"24.63 63.33\"></circle></svg>";
}

std::string RoboWebSegmentMark(const char* kind) {
  const char* path = kind != nullptr && std::string(kind) == "error" ? "M6 6L18 18M18 6L6 18" : "M5.5 12.5L10 17L18.5 7.5";
  std::string html = "<svg class=\"robo-segment-mark\" viewBox=\"0 0 24 24\" fill=\"none\" aria-hidden=\"true\"><path d=\"";
  html += path;
  html += "\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" vector-effect=\"non-scaling-stroke\"";
  if (kind == nullptr || std::string(kind) != "error") {
    html += " stroke-linejoin=\"round\"";
  }
  html += "></path></svg>";
  return html;
}

struct RoboWebSegmentOption {
  std::string value;
  std::string label;
  std::string icon;
  std::string badge;
  std::string status;
  std::string action;
  bool disabled = false;
};

std::string RoboWebSegmentIcon(const fs::path& assetsRoot, const RoboXMLNode& node) {
  std::string icon = RoboWebSvg(assetsRoot, RoboXMLAttr(node, "icon"));
  for (const RoboXMLNode& child : node.children) {
    const std::string childName = RoboWebLower(child.name);
    if (childName != "icon" && childName != "svg") {
      continue;
    }
    const std::string markup = RoboWebIconMarkup(assetsRoot, child);
    if (!markup.empty()) {
      icon = markup;
      break;
    }
  }
  if (icon.empty()) {
    return {};
  }
  return "<span class=\"robo-segment-icon\" aria-hidden=\"true\">" + icon + "</span>";
}

}  // namespace

void RoboWebEmitSegmentedControl(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                                 const fs::path& assetsRoot) {
  const std::string variant = RoboWebSegmentVariant(node);
  const std::string size = RoboWebSegmentSize(node);
  const std::string content = RoboWebSegmentContent(node);
  const std::string mode = RoboWebSegmentMode(node);
  const bool fullWidth = RoboWebFlag(RoboXMLAttr(node, "fullWidth"));
  const bool disabled = RoboWebFlag(RoboXMLAttr(node, "disabled"));
  const std::string label = RoboXMLAttr(node, "label");
  const std::string wanted = RoboXMLAttr(node, "value");
  const std::string id = RoboXMLAttr(node, "id");
  const std::string action = RoboXMLAttr(node, "action");
  const std::string successMs = RoboXMLAttr(node, "successMs", "1600");
  std::string leading = RoboWebSvg(assetsRoot, RoboXMLAttr(node, "leading"));
  std::string trailing = RoboWebSvg(assetsRoot, RoboXMLAttr(node, "trailing"));
  std::vector<RoboWebSegmentOption> options;
  for (const RoboXMLNode& child : node.children) {
    const std::string childName = RoboWebLower(child.name);
    if (childName == "icon" || childName == "svg") {
      const std::string markup = RoboWebIconMarkup(assetsRoot, child);
      if (markup.empty()) {
        continue;
      }
      if (RoboWebSegmentSide(child) == 1) {
        trailing = markup;
      } else if (RoboWebSegmentSide(child) == 0 || leading.empty()) {
        leading = markup;
      } else {
        trailing = markup;
      }
      continue;
    }
    if (childName != "segment") {
      continue;
    }
    RoboWebSegmentOption option;
    option.value = RoboXMLAttr(child, "value");
    option.label = RoboXMLAttr(child, "label");
    if (option.label.empty()) {
      option.label = RoboCompilerTrim(child.text);
    }
    option.icon = RoboWebSegmentIcon(assetsRoot, child);
    option.badge = RoboXMLAttr(child, "badge");
    option.status = RoboWebSegmentStatus(child);
    option.action = RoboXMLAttr(child, "action");
    option.disabled = RoboWebFlag(RoboXMLAttr(child, "disabled"));
    options.push_back(std::move(option));
  }
  int selected = 0;
  for (int index = 0; index < static_cast<int>(options.size()); ++index) {
    if (!wanted.empty() && options[static_cast<std::size_t>(index)].value == wanted) {
      selected = index;
      break;
    }
  }
  const int count = static_cast<int>(options.size());
  const bool iconOnly = content == "icon";
  const bool showIcon = content == "icon" || content == "both";
  const bool showLabel = content != "icon";
  const bool dividers = variant != "ghost";
  std::string style = RoboWebStyle(node, library);
  RoboWebAppendMotion(style, node);
  std::string control;
  RoboWebAppendControlStyle(control, node, library);
  const std::string labelId = id.empty() ? std::string() : id + "Label";
  html += "<div class=\"robo-segmented\" data-full=\"";
  html += fullWidth ? "true" : "false";
  html += "\"";
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += ">";
  if (!label.empty()) {
    html += "<span class=\"robo-segmented-label\"";
    if (!labelId.empty()) {
      html += " id=\"";
      html += RoboWebEscape(labelId);
      html += "\"";
    }
    html += ">";
    html += RoboWebEscape(label);
    html += "</span>";
  }
  const bool sided = !leading.empty() || !trailing.empty();
  if (sided) {
    html += "<div class=\"robo-segmented-row\">";
    if (!leading.empty()) {
      html += "<span class=\"robo-segmented-side\">" + leading + "</span>";
    }
  }
  html += "<div class=\"robo-segmented-track\" role=\"radiogroup\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\" data-size=\"";
  html += size;
  html += "\" data-content=\"";
  html += content;
  html += "\" data-mode=\"";
  html += mode;
  html += "\" data-count=\"";
  html += std::to_string(count);
  html += "\" data-index=\"";
  html += std::to_string(selected);
  html += "\" style=\"--robo-count:";
  html += std::to_string(count);
  html += ";--robo-index:";
  html += std::to_string(selected);
  html += "\"";
  if (!label.empty() && !labelId.empty()) {
    html += " aria-labelledby=\"";
    html += RoboWebEscape(labelId);
    html += "\"";
  } else if (!label.empty()) {
    html += " aria-label=\"";
    html += RoboWebEscape(label);
    html += "\"";
  } else {
    html += " aria-label=\"Options\"";
  }
  if (disabled) {
    html += " aria-disabled=\"true\"";
  }
  if (!action.empty()) {
    html += " data-action=\"";
    html += RoboWebEscape(action);
    html += "\"";
  }
  if (!successMs.empty() && successMs != "1600") {
    html += " data-success-ms=\"";
    html += RoboWebEscape(successMs);
    html += "\"";
  }
  html += RoboWebScreenAttrs(node, library, "inline-flex");
  html += RoboWebMotionAttrs(node);
  html += ">";
  if (mode != "grouped") {
    html += "<span class=\"robo-segmented-thumb\" aria-hidden=\"true\"></span>";
  }
  for (int index = 0; index < count; ++index) {
    const RoboWebSegmentOption& option = options[static_cast<std::size_t>(index)];
    const bool selectedHere = index == selected;
    const bool busy = option.status == "loading";
    const bool idle = option.disabled || disabled || busy;
    if (dividers && index > 0) {
      html += "<span class=\"robo-segment-rule\" aria-hidden=\"true\" style=\"--robo-rule:";
      html += std::to_string(index);
      html += "\"></span>";
    }
    html += "<button class=\"robo-segment robo-control\" type=\"button\" role=\"radio\" aria-checked=\"";
    html += selectedHere ? "true" : "false";
    html += "\" tabindex=\"";
    html += (idle || !selectedHere) ? "-1" : "0";
    html += "\"";
    if (idle) {
      html += " aria-disabled=\"true\"";
    }
    if (busy) {
      html += " aria-busy=\"true\"";
    }
    if (iconOnly) {
      html += " aria-label=\"";
      html += RoboWebEscape(option.label);
      html += "\"";
    }
    if (!option.action.empty()) {
      html += " data-action=\"";
      html += RoboWebEscape(option.action);
      html += "\"";
    }
    if (!option.status.empty() && option.status != "idle") {
      html += " data-status=\"";
      html += RoboWebEscape(option.status);
      html += "\"";
    }
    if (option.disabled) {
      html += " data-disabled=\"true\"";
    }
    if (idle) {
      html += " disabled";
    }
    RoboWebControlAttrs(html, node);
    if (!control.empty()) {
      html += " style=\"";
      html += RoboWebEscape(control);
      html += "\"";
    }
    html += "><span class=\"robo-segment-face\">";
    if (showIcon) {
      html += option.icon;
    }
    if (showLabel || option.icon.empty()) {
      html += "<span class=\"robo-segment-label\"><span class=\"robo-segment-copy\">";
      html += RoboWebEscape(option.label);
      html += "</span><span class=\"robo-segment-sizer\" aria-hidden=\"true\">";
      html += RoboWebEscape(option.label);
      html += "</span></span>";
    }
    if (!option.badge.empty() && !iconOnly) {
      html += "<span class=\"robo-segment-badge\">";
      html += RoboWebEscape(option.badge);
      html += "</span>";
    }
    html += "</span><span class=\"robo-segment-overlay\" data-state=\"loading\">";
    html += RoboWebSegmentSpinner();
    html += "</span><span class=\"robo-segment-overlay\" data-state=\"success\">";
    html += RoboWebSegmentMark("success");
    html += "</span><span class=\"robo-segment-overlay\" data-state=\"error\">";
    html += RoboWebSegmentMark("error");
    html += "</span></button>";
  }
  html += "</div>";
  if (sided) {
    if (!trailing.empty()) {
      html += "<span class=\"robo-segmented-side\">" + trailing + "</span>";
    }
    html += "</div>";
  }
  html += "</div>";
}

void RoboWebEmitSegmentedToolbar(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                                 const std::string& source, const fs::path& assetsRoot, bool& warnedVideo,
                                 RoboWebPackage* package, int depth) {
  const bool attached = RoboWebFlag(RoboXMLAttr(node, "attached"));
  std::string style = RoboWebStyle(node, library);
  RoboWebAppendMotion(style, node);
  html += "<div class=\"robo-segmented-toolbar\" data-attached=\"";
  html += attached ? "true" : "false";
  html += "\"";
  const std::string id = RoboXMLAttr(node, "id");
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += RoboWebScreenAttrs(node, library, "flex");
  html += RoboWebMotionAttrs(node);
  html += ">";
  for (const RoboXMLNode& child : node.children) {
    RoboWebEmit(html, child, library, source, assetsRoot, false, warnedVideo, package, depth + 1);
  }
  html += "</div>";
}

void RoboWebAppendSegmentedCss(std::ostream& css) {
  css << ".robo-segmented{display:flex;min-width:0;width:fit-content;max-width:100%;flex-direction:column;gap:8px}\n";
  css << ".robo-segmented[data-full=\"true\"]{width:100%}\n";
  css << ".robo-segmented-label{font-size:12px;line-height:1.33;color:var(--text-tertiary)}\n";
  css << ".robo-segmented-row{display:flex;min-width:0;align-items:center;gap:8px}\n";
  css << ".robo-segmented[data-full=\"true\"] .robo-segmented-row{width:100%}\n";
  css << ".robo-segmented[data-full=\"true\"] .robo-segmented-row .robo-segmented-track{flex:1;width:auto;min-width:0}\n";
  css << ".robo-segmented-side{display:inline-flex;flex:0 0 auto;color:var(--text-secondary)}\n";
  css << ".robo-segmented-side svg{width:18px;height:18px;display:block}\n";
  css << ".robo-segmented-track{position:relative;isolation:isolate;display:grid;grid-auto-flow:column;"
         "grid-auto-columns:1fr;width:fit-content;max-width:100%;overflow:hidden;box-sizing:border-box;"
         "padding:2px;border:0;border-radius:12px;background:var(--bg-tertiary);touch-action:manipulation;"
         "user-select:none}\n";
  css << ".robo-segmented[data-full=\"true\"] .robo-segmented-track{width:100%}\n";
  css << ".robo-segmented-track[data-size=\"sm\"]{height:36px;border-radius:10px}\n";
  css << ".robo-segmented-track[data-size=\"md\"]{height:44px;border-radius:12px}\n";
  css << ".robo-segmented-track[data-size=\"lg\"]{height:50px;border-radius:12px}\n";
  css << ".robo-segmented-track[data-variant=\"outline\"]{border:1px solid var(--border-default);background:transparent}\n";
  css << ".robo-segmented-track[data-variant=\"ghost\"]{background:transparent}\n";
  css << ".robo-segmented-track[data-variant=\"pill\"]{border-radius:980px}\n";
  css << ".robo-segmented-track[data-mode=\"grouped\"]{padding:0}\n";
  css << ".robo-segmented-track[aria-disabled=\"true\"]{cursor:not-allowed;opacity:0.55}\n";
  css << ".robo-segmented-thumb{position:absolute;top:2px;z-index:0;height:calc(100% - 4px);pointer-events:none;"
         "width:calc((100% - 4px) / var(--robo-count));"
         "inset-inline-start:calc(2px + ((100% - 4px) / var(--robo-count)) * var(--robo-index));"
         "background:var(--bg-elevated);border-radius:10px;"
         "transition:inset-inline-start 300ms cubic-bezier(0.28,0.11,0.32,1),width 300ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << ".robo-segmented-track[data-size=\"sm\"] .robo-segmented-thumb{border-radius:8px}\n";
  css << ".robo-segmented-track[data-variant=\"outline\"] .robo-segmented-thumb{background:var(--bg-secondary)}\n";
  css << ".robo-segmented-track[data-variant=\"ghost\"] .robo-segmented-thumb{background:var(--bg-tertiary)}\n";
  css << ".robo-segmented-track[data-variant=\"primary\"] .robo-segmented-thumb{background:var(--brand-primary)}\n";
  css << "html[data-theme=\"dark\"] .robo-segmented-track[data-variant=\"subtle\"] .robo-segmented-thumb,"
         "html[data-theme=\"dark\"] .robo-segmented-track[data-variant=\"pill\"] .robo-segmented-thumb,"
         "html[data-theme=\"dark\"] .robo-segmented-track[data-variant=\"outline\"] .robo-segmented-thumb,"
         "html[data-theme=\"dark\"] .robo-segmented-track[data-variant=\"ghost\"] .robo-segmented-thumb"
         "{background:color-mix(in srgb,var(--bg-tertiary) 72%,white 28%)}\n";
  css << ".robo-segmented-track[data-variant=\"pill\"] .robo-segmented-thumb{border-radius:980px}\n";
  css << ".robo-segment-rule{position:absolute;top:50%;z-index:1;width:1px;height:46%;margin-inline-start:-0.5px;"
         "transform:translateY(-50%);background:var(--border-default);pointer-events:none;opacity:1;"
         "transition:opacity 200ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << "html[data-theme=\"dark\"] .robo-segment-rule{background:var(--border-strong)}\n";
  css << ".robo-segmented-track[data-mode=\"inset\"] .robo-segment-rule{"
         "inset-inline-start:calc(2px + ((100% - 4px) / var(--robo-count)) * var(--robo-rule))}\n";
  css << ".robo-segmented-track[data-mode=\"grouped\"] .robo-segment-rule{"
         "inset-inline-start:calc((100% / var(--robo-count)) * var(--robo-rule))}\n";
  css << ".robo-segment[aria-checked=\"true\"]+.robo-segment-rule,"
         ".robo-segment-rule:has(+.robo-segment[aria-checked=\"true\"]),"
         ".robo-segment[aria-disabled=\"true\"]+.robo-segment-rule,"
         ".robo-segment-rule:has(+.robo-segment[aria-disabled=\"true\"]){opacity:0}\n";
  css << ".robo-segment{position:relative;z-index:2;display:inline-flex;height:100%;min-width:72px;align-items:center;"
         "justify-content:center;margin:0;padding:0 16px;border:0;border-radius:10px;background:transparent;"
         "color:var(--text-secondary);font:400 17px/1.35 var(--font-sans,system-ui,sans-serif);letter-spacing:-0.01em;"
         "cursor:pointer;outline:none;transition:color 200ms cubic-bezier(0.28,0.11,0.32,1),"
         "background-color 200ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << ".robo-segmented-track[data-size=\"sm\"] .robo-segment{padding:0 12px;border-radius:8px;font-size:15px}\n";
  css << ".robo-segmented-track[data-size=\"lg\"] .robo-segment{padding:0 20px}\n";
  css << ".robo-segmented-track[data-variant=\"pill\"] .robo-segment{border-radius:980px}\n";
  css << ".robo-segmented-track[data-mode=\"grouped\"] .robo-segment{border-radius:0}\n";
  css << ".robo-segmented-track[data-content=\"icon\"][data-size=\"sm\"] .robo-segment{min-width:36px;padding:0 10px}\n";
  css << ".robo-segmented-track[data-content=\"icon\"][data-size=\"md\"] .robo-segment{min-width:44px;padding:0 12px}\n";
  css << ".robo-segmented-track[data-content=\"icon\"][data-size=\"lg\"] .robo-segment{min-width:50px;padding:0 14px}\n";
  css << ".robo-segmented[data-full=\"true\"] .robo-segmented-track:not([data-content=\"icon\"]) .robo-segment{min-width:0}\n";
  css << ".robo-segment:hover:not(:disabled):not([aria-checked=\"true\"]){color:var(--text-primary)}\n";
  css << ".robo-segment:active:not(:disabled):not([aria-checked=\"true\"]){opacity:0.7}\n";
  css << ".robo-segment[aria-checked=\"true\"]{color:var(--text-primary);font-weight:500}\n";
  css << ".robo-segmented-track[data-variant=\"primary\"] .robo-segment[aria-checked=\"true\"]{color:var(--text-on-brand)}\n";
  css << ".robo-segment:focus-visible{outline:2px solid var(--brand-primary);outline-offset:-2px}\n";
  css << ".robo-segment:disabled{cursor:not-allowed}\n";
  css << ".robo-segmented-track[data-mode=\"grouped\"][data-variant=\"subtle\"] .robo-segment[aria-checked=\"true\"],"
         ".robo-segmented-track[data-mode=\"grouped\"][data-variant=\"pill\"] .robo-segment[aria-checked=\"true\"]"
         "{background:var(--bg-elevated)}\n";
  css << ".robo-segmented-track[data-mode=\"grouped\"][data-variant=\"outline\"] .robo-segment[aria-checked=\"true\"]"
         "{background:var(--bg-secondary)}\n";
  css << ".robo-segmented-track[data-mode=\"grouped\"][data-variant=\"ghost\"] .robo-segment[aria-checked=\"true\"]"
         "{background:var(--bg-tertiary)}\n";
  css << ".robo-segmented-track[data-mode=\"grouped\"][data-variant=\"primary\"] .robo-segment[aria-checked=\"true\"]"
         "{background:var(--brand-primary)}\n";
  css << "html[data-theme=\"dark\"] .robo-segmented-track[data-mode=\"grouped\"][data-variant=\"subtle\"] "
         ".robo-segment[aria-checked=\"true\"],"
         "html[data-theme=\"dark\"] .robo-segmented-track[data-mode=\"grouped\"][data-variant=\"pill\"] "
         ".robo-segment[aria-checked=\"true\"],"
         "html[data-theme=\"dark\"] .robo-segmented-track[data-mode=\"grouped\"][data-variant=\"outline\"] "
         ".robo-segment[aria-checked=\"true\"],"
         "html[data-theme=\"dark\"] .robo-segmented-track[data-mode=\"grouped\"][data-variant=\"ghost\"] "
         ".robo-segment[aria-checked=\"true\"]"
         "{background:color-mix(in srgb,var(--bg-tertiary) 72%,white 28%)}\n";
  css << ".robo-segment-face{display:inline-flex;min-width:0;align-items:center;justify-content:center;gap:8px}\n";
  css << ".robo-segment-label{display:inline-grid;min-width:0;max-width:100%}\n";
  css << ".robo-segment-copy,.robo-segment-sizer{grid-area:1/1;white-space:nowrap;line-height:1.35}\n";
  css << ".robo-segment-copy{overflow:hidden;text-overflow:ellipsis;padding-bottom:0.08em;margin-bottom:-0.08em}\n";
  css << ".robo-segment-sizer{font-weight:500;visibility:hidden}\n";
  css << ".robo-segmented-track[data-content=\"icon\"] .robo-segment-label{position:absolute;width:1px;height:1px;"
         "padding:0;margin:-1px;overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border:0}\n";
  css << ".robo-segment-icon{display:inline-flex;flex:0 0 auto}\n";
  css << ".robo-segment-icon svg{width:1em;height:1em;display:block}\n";
  css << ".robo-segment-badge{display:inline-flex;min-width:16px;justify-content:center;color:currentColor;opacity:0.7;"
         "font:500 12px/1 var(--font-sans,system-ui,sans-serif);font-variant-numeric:tabular-nums}\n";
  css << ".robo-segmented-track[data-variant=\"primary\"] .robo-segment[aria-checked=\"true\"] .robo-segment-badge"
         "{color:var(--text-on-brand);opacity:0.8}\n";
  css << ".robo-segment-overlay{position:absolute;inset:0;display:none;align-items:center;justify-content:center}\n";
  css << ".robo-segment[data-status=\"loading\"] .robo-segment-overlay[data-state=\"loading\"],"
         ".robo-segment[data-status=\"success\"] .robo-segment-overlay[data-state=\"success\"],"
         ".robo-segment[data-status=\"error\"] .robo-segment-overlay[data-state=\"error\"],"
         ".robo-segment:not([data-status])[data-phase=\"loading\"] .robo-segment-overlay[data-state=\"loading\"],"
         ".robo-segment:not([data-status])[data-phase=\"success\"] .robo-segment-overlay[data-state=\"success\"],"
         ".robo-segment:not([data-status])[data-phase=\"error\"] .robo-segment-overlay[data-state=\"error\"]"
         "{display:flex}\n";
  css << ".robo-segment[data-status=\"loading\"] .robo-segment-face,"
         ".robo-segment[data-status=\"success\"] .robo-segment-face,"
         ".robo-segment[data-status=\"error\"] .robo-segment-face,"
         ".robo-segment:not([data-status])[data-phase=\"loading\"] .robo-segment-face,"
         ".robo-segment:not([data-status])[data-phase=\"success\"] .robo-segment-face,"
         ".robo-segment:not([data-status])[data-phase=\"error\"] .robo-segment-face{visibility:hidden}\n";
  css << ".robo-segment-spinner{display:block;width:16px;height:16px;transform-origin:center;"
         "animation:robo-segment-spin 1s linear infinite}\n";
  css << ".robo-segmented-track[data-size=\"sm\"] .robo-segment-spinner{width:14px;height:14px}\n";
  css << ".robo-segment-mark{display:block;width:18px;height:18px}\n";
  css << ".robo-segmented-track[data-size=\"sm\"] .robo-segment-mark{width:16px;height:16px}\n";
  css << ".robo-segment-overlay[data-state=\"success\"]{color:var(--success)}\n";
  css << ".robo-segment-overlay[data-state=\"error\"]{color:var(--danger)}\n";
  css << ".robo-segmented-track[data-variant=\"primary\"] .robo-segment[aria-checked=\"true\"] "
         ".robo-segment-overlay[data-state=\"success\"],"
         ".robo-segmented-track[data-variant=\"primary\"] .robo-segment[aria-checked=\"true\"] "
         ".robo-segment-overlay[data-state=\"error\"]{color:currentColor}\n";
  css << ".robo-segment-spinner-track{stroke:color-mix(in srgb,var(--text-tertiary) 35%,transparent)}\n";
  css << ".robo-segment-spinner-tick{stroke:var(--brand-primary)}\n";
  css << ".robo-segmented-track[data-variant=\"primary\"] .robo-segment[aria-checked=\"true\"] .robo-segment-spinner-track"
         "{stroke:color-mix(in srgb,var(--text-on-brand) 35%,transparent)}\n";
  css << ".robo-segmented-track[data-variant=\"primary\"] .robo-segment[aria-checked=\"true\"] .robo-segment-spinner-tick"
         "{stroke:var(--text-on-brand)}\n";
  css << "@keyframes robo-segment-spin{to{transform:rotate(360deg)}}\n";
  css << "@media (prefers-reduced-motion:reduce){.robo-segmented-thumb,.robo-segment,.robo-segment-rule{transition:none}"
         ".robo-segment-spinner{animation:none}}\n";
  css << ".robo-segmented-toolbar{display:flex;min-width:0;max-width:100%;align-items:center;flex-wrap:wrap;gap:8px}\n";
  css << ".robo-segmented-toolbar[data-attached=\"true\"]{flex-wrap:nowrap;gap:0;overflow:hidden;border-radius:12px;"
         "border:1px solid var(--border-default)}\n";
  css << ".robo-segmented-toolbar[data-attached=\"true\"]>*+*{border-left:1px solid var(--border-default)}\n";
  css << ".robo-segmented-toolbar[data-attached=\"true\"] .robo-segmented-track{border:0;border-radius:0}\n";
  css << ".robo-segmented-toolbar[data-attached=\"true\"] .robo-segment{border-radius:0}\n";
}

std::string RoboWebSegmentedScript() {
  return R"JS(
  function segmentButtons(track) {
    return track.querySelectorAll(".robo-segment");
  }
  function segmentEnabled(track) {
    var all = segmentButtons(track);
    var list = [];
    for (var i = 0; i < all.length; i++) {
      if (!all[i].disabled && all[i].getAttribute("data-disabled") !== "true") list.push(all[i]);
    }
    return list;
  }
  function segmentChoose(track, button, run) {
    if (!track || !button || track.getAttribute("aria-disabled") === "true" || button.disabled) return;
    var all = segmentButtons(track);
    var index = 0;
    for (var i = 0; i < all.length; i++) {
      var on = all[i] === button;
      all[i].setAttribute("aria-checked", on ? "true" : "false");
      all[i].tabIndex = all[i].disabled ? -1 : (on ? 0 : -1);
      if (on) index = i;
    }
    track.setAttribute("data-index", String(index));
    track.style.setProperty("--robo-index", String(index));
    if (run) segmentRun(track, button);
  }
  function segmentRun(track, button) {
    if (button.getAttribute("data-status")) return;
    var action = button.getAttribute("data-action");
    if (action == null || action === "") action = track.getAttribute("data-action");
    if (action == null || action === "") return;
    var phase = button.getAttribute("data-phase") || "idle";
    if (phase !== "idle") return;
    var wait = parseInt(action, 10);
    if (!(wait > 0)) wait = 0;
    var hold = parseInt(track.getAttribute("data-success-ms") || "1600", 10);
    if (!(hold > 0)) hold = 1600;
    var fail = action === "error";
    button.setAttribute("data-phase", "loading");
    button.disabled = true;
    button.setAttribute("aria-busy", "true");
    button.setAttribute("aria-disabled", "true");
    window.setTimeout(function () {
      button.removeAttribute("aria-busy");
      button.setAttribute("data-phase", fail ? "error" : "success");
      window.setTimeout(function () {
        button.setAttribute("data-phase", "idle");
        var locked = button.getAttribute("data-disabled") === "true" || track.getAttribute("aria-disabled") === "true";
        button.disabled = locked;
        if (locked) button.setAttribute("aria-disabled", "true");
        else button.removeAttribute("aria-disabled");
      }, hold);
    }, wait);
  }
  document.addEventListener("click", function (ev) {
    var button = ev.target.closest ? ev.target.closest(".robo-segment") : null;
    if (!button) return;
    segmentChoose(button.closest(".robo-segmented-track"), button, true);
  });
  document.addEventListener("keydown", function (ev) {
    var button = ev.target.closest ? ev.target.closest(".robo-segment") : null;
    if (!button) return;
    var track = button.closest(".robo-segmented-track");
    if (!track || track.getAttribute("aria-disabled") === "true") return;
    var key = ev.key;
    var list = segmentEnabled(track);
    if (!list.length) return;
    var at = list.indexOf(button);
    if (key === "ArrowRight" || key === "ArrowDown" || key === "ArrowLeft" || key === "ArrowUp" || key === "Home" || key === "End") {
      ev.preventDefault();
      ev.stopPropagation();
      var next = list[0];
      if (key === "End") next = list[list.length - 1];
      else if (key === "ArrowRight" || key === "ArrowDown") next = list[(at < 0 ? 0 : (at + 1) % list.length)];
      else if (key === "ArrowLeft" || key === "ArrowUp") next = list[(at < 0 ? 0 : (at - 1 + list.length) % list.length)];
      segmentChoose(track, next, false);
      next.focus();
      return;
    }
    if (key === "Enter" || key === " ") {
      ev.preventDefault();
      ev.stopPropagation();
      var current = button.getAttribute("aria-checked") === "true" ? button : null;
      if (!current) {
        for (var i = 0; i < list.length; i++) if (list[i].getAttribute("aria-checked") === "true") current = list[i];
      }
      if (current) segmentChoose(track, current, true);
    }
  }, true);
)JS";
}

const char* RoboWebSegmentedClass() { return "robo-segmented"; }
