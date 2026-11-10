#include "AnchorWeb.hpp"

#include "../Button/ButtonWeb.hpp"
#include "../Text/TextWeb.hpp"
#include "../../Motion.hpp"
#include "../../ScreenSize.hpp"

namespace fs = std::filesystem;

namespace {

bool Flag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

std::string Trim(std::string text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\n' || text.front() == '\r' || text.front() == '\t')) {
    text.erase(text.begin());
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\n' || text.back() == '\r' || text.back() == '\t')) {
    text.pop_back();
  }
  return text;
}

std::string Label(const RoboXMLNode& node) {
  std::string label = RoboXMLAttr(node, "content");
  if (label.empty()) label = node.text;
  return Trim(label);
}

std::string Route(const RoboXMLNode& node) {
  std::string route = RoboXMLAttr(node, "path");
  if (route.empty()) route = RoboXMLAttr(node, "href");
  if (route.empty()) route = RoboXMLAttr(node, "route");
  if (route.empty()) route = RoboXMLAttr(node, "screen");
  return route;
}

bool ExternalAddress(const std::string& route) {
  const std::string key = RoboWebLower(route);
  return key.rfind("http://", 0) == 0 || key.rfind("https://", 0) == 0 || key.rfind("//", 0) == 0;
}

std::string VariantOf(const RoboXMLNode& node) {
  const std::string value = RoboWebLower(RoboXMLAttr(node, "variant"));
  if (value == "subtle" || value == "underline" || value == "plain" || value == "standalone" || value == "arrow" ||
      value == "more")
    return value;
  return "inline";
}

std::string SizeOf(const RoboXMLNode& node) {
  const std::string value = RoboWebLower(RoboXMLAttr(node, "size"));
  if (value == "sm" || value == "small") return "sm";
  if (value == "md" || value == "medium") return "md";
  if (value == "lg" || value == "large") return "lg";
  return "inherit";
}

std::string Glyph(const char* path, int size) {
  std::string svg = "<svg viewBox=\"0 0 24 24\" width=\"";
  svg += std::to_string(size);
  svg += "\" height=\"";
  svg += std::to_string(size);
  svg += "\" fill=\"none\" aria-hidden=\"true\">";
  svg += path;
  svg += "</svg>";
  return svg;
}

const char* kCaretRight = "<path d=\"M9.5 6.5L15.5 12L9.5 17.5\" stroke=\"currentColor\" stroke-width=\"2.2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>";
const char* kCaretLeft = "<path d=\"M14.5 6.5L8.5 12L14.5 17.5\" stroke=\"currentColor\" stroke-width=\"2.2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>";
const char* kLaunch =
    "<path d=\"M7 17L17 7\" stroke=\"currentColor\" stroke-width=\"1.8\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>"
    "<path d=\"M9 7H17V15\" stroke=\"currentColor\" stroke-width=\"1.8\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>";
const char* kCaretThin = "<path d=\"M9.5 6.5L15.5 12L9.5 17.5\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>";
const char* kArrowRight =
    "<path fill=\"currentColor\" d=\"M438.6 278.6c12.5-12.5 12.5-32.8 0-45.3l-160-160c-12.5-12.5-32.8-12.5-45.3 0s-12.5 32.8 0 45.3L338.8 224 32 224c-17.7 0-32 14.3-32 32s14.3 32 32 32l306.7 0L233.4 393.4c-12.5 12.5-12.5 32.8 0 45.3s32.8 12.5 45.3 0l160-160z\"/>";

std::string ArrowMark() {
  return std::string("<svg viewBox=\"0 0 448 512\" aria-hidden=\"true\">") + kArrowRight + "</svg>";
}

int ArrowPx(const std::string& size) {
  if (size == "sm") return 10;
  if (size == "md") return 12;
  if (size == "lg") return 14;
  return 11;
}

int EndPx(const std::string& size) {
  if (size == "sm") return 11;
  if (size == "md") return 13;
  if (size == "lg") return 15;
  return 12;
}

std::string IconSlot(const RoboXMLNode& node) {
  std::string slot = RoboXMLAttr(node, "slot");
  if (slot.empty()) slot = RoboXMLAttr(node, "side");
  if (slot.empty()) slot = RoboXMLAttr(node, "place");
  return RoboWebLower(slot);
}

}  // namespace

void RoboWebEmitAnchor(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                       const fs::path& assetsRoot) {
  const std::string id = RoboXMLAttr(node, "id");
  const std::string route = Route(node);
  const bool disabled = Flag(RoboXMLAttr(node, "disabled"));
  const bool linked = !route.empty() && !disabled;
  std::string variant = VariantOf(node);
  const std::string size = SizeOf(node);
  const std::string tone = RoboWebLower(RoboXMLAttr(node, "tone")) == "danger" ? "danger" : "brand";
  std::string arrow = RoboWebLower(RoboXMLAttr(node, "arrow"));
  if (arrow != "start" && arrow != "end") arrow.clear();
  if (!arrow.empty()) variant = "arrow";
  if (variant == "arrow" && arrow.empty()) arrow = "end";
  const std::string external = RoboWebLower(RoboXMLAttr(node, "external"));
  const std::string targetAttr = RoboXMLAttr(node, "target");
  const bool launch = external != "false" && external != "0" && external != "no" && external != "off" &&
                      (external == "true" || ExternalAddress(route) || RoboWebLower(targetAttr) == "_blank");
  const bool openNew = linked && (RoboWebLower(targetAttr) == "_blank" || (external == "true" && RoboWebLower(targetAttr) != "_self"));
  std::string target = targetAttr;
  if (target.empty() && openNew) target = "_blank";
  std::string rel = RoboXMLAttr(node, "rel");
  if (rel.empty() && target == "_blank") rel = "noopener noreferrer";
  const std::string preview = RoboWebLower(RoboXMLAttr(node, "preview"));

  std::string leading = RoboWebSvg(assetsRoot, RoboXMLAttr(node, "leading"));
  std::string trailing = RoboWebSvg(assetsRoot, RoboXMLAttr(node, "trailing"));
  for (const RoboXMLNode& child : node.children) {
    const std::string childName = RoboWebLower(child.name);
    if (childName != "icon" && childName != "svg") continue;
    const std::string icon = RoboWebIconMarkup(assetsRoot, child);
    if (icon.empty()) continue;
    const std::string slot = IconSlot(child);
    if (slot == "trailing" || slot == "right" || slot == "end") trailing = icon;
    else if (slot == "leading" || slot == "left" || slot == "start" || leading.empty()) leading = icon;
  }

  std::string href = route;
  std::string dataRoute;
  if (linked && !ExternalAddress(route)) {
    if (!href.empty() && href[0] == '#') href = href.substr(1);
    if (href.empty() || href[0] != '/') href = "/" + href;
    dataRoute = href;
    href = "#" + href;
  }

  std::string style = RoboWebStyle(node, library);
  RoboWebAppendTextStyle(style, node, library);
  RoboWebAppendControlStyle(style, node, library);
  RoboWebAppendMotion(style, node);

  const bool button = !linked;
  html += disabled ? "<span" : button ? "<button" : "<a";
  html += " class=\"robo-anchor robo-control\"";
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  html += " data-variant=\"";
  html += variant;
  html += "\" data-size=\"";
  html += size;
  html += "\" data-tone=\"";
  html += tone;
  html += "\"";
  if (!arrow.empty()) {
    html += " data-arrow=\"";
    html += arrow;
    html += "\"";
  }
  if (launch) html += " data-launch=\"true\"";
  if (!preview.empty()) {
    html += " data-preview=\"";
    html += RoboWebEscape(preview);
    html += "\"";
  }
  if (disabled) {
    html += " data-disabled=\"true\" role=\"link\" aria-disabled=\"true\"";
  } else if (button) {
    const std::string type = RoboWebLower(RoboXMLAttr(node, "type"));
    html += " type=\"";
    html += (type == "submit" || type == "reset") ? type : "button";
    html += "\"";
  } else {
    html += " href=\"";
    html += RoboWebEscape(href);
    html += "\"";
    if (!dataRoute.empty()) {
      html += " data-route=\"";
      html += RoboWebEscape(dataRoute);
      html += "\"";
    }
    if (!target.empty()) {
      html += " target=\"";
      html += RoboWebEscape(target);
      html += "\"";
    }
    if (!rel.empty()) {
      html += " rel=\"";
      html += RoboWebEscape(rel);
      html += "\"";
    }
  }
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += RoboWebScreenAttrs(node, library, "inline-flex");
  html += RoboWebMotionAttrs(node);
  html += ">";
  if (!leading.empty()) html += "<span class=\"robo-anchor-icon\">" + leading + "</span>";
  if (arrow == "start") {
    const int px = ArrowPx(size);
    const char* path = launch ? kLaunch : kCaretLeft;
    html += "<span class=\"robo-anchor-mark\" aria-hidden=\"true\">";
    if (launch || disabled) {
      html += "<span class=\"robo-anchor-glyph\">" + Glyph(path, px) + "</span>";
    } else {
      html += "<span class=\"robo-anchor-glyph\">" + Glyph(path, px) + "</span>";
      html += "<span class=\"robo-anchor-glyph robo-anchor-glyph-next\">" + Glyph(path, px) + "</span>";
    }
    html += "</span>";
  }
  html += "<span class=\"robo-anchor-label\">";
  html += RoboWebEscape(Label(node));
  html += "</span>";
  if (arrow == "end") {
    const int px = ArrowPx(size);
    const char* path = launch ? kLaunch : kCaretRight;
    html += "<span class=\"robo-anchor-mark\" aria-hidden=\"true\">";
    if (launch || disabled) {
      html += "<span class=\"robo-anchor-glyph\">" + Glyph(path, px) + "</span>";
    } else {
      html += "<span class=\"robo-anchor-glyph\">" + Glyph(path, px) + "</span>";
      html += "<span class=\"robo-anchor-glyph robo-anchor-glyph-next\">" + Glyph(path, px) + "</span>";
    }
    html += "</span>";
  } else if (trailing.empty() && variant == "more") {
    html += "<span class=\"robo-anchor-slide\" aria-hidden=\"true\">";
    html += "<span class=\"robo-anchor-glyph\">" + ArrowMark() + "</span>";
    if (!disabled) html += "<span class=\"robo-anchor-glyph robo-anchor-glyph-next\">" + ArrowMark() + "</span>";
    html += "</span>";
  } else if (trailing.empty() && launch) {
    html += "<span class=\"robo-anchor-end robo-anchor-launch\" aria-hidden=\"true\">" + Glyph(kLaunch, EndPx(size)) + "</span>";
  } else if (trailing.empty() && variant == "standalone") {
    html += "<span class=\"robo-anchor-end\" aria-hidden=\"true\">" + Glyph(kCaretThin, EndPx(size)) + "</span>";
  } else if (!trailing.empty() && arrow.empty()) {
    html += "<span class=\"robo-anchor-icon\">" + trailing + "</span>";
  }
  if (openNew && !disabled) html += "<span class=\"robo-anchor-sr\"> (opens in a new window)</span>";
  html += disabled ? "</span>" : button ? "</button>" : "</a>";
}

void RoboWebAppendAnchorCss(std::ostream& css) {
  const char* ease = "cubic-bezier(0.28,0.11,0.32,1)";
  css << ".robo-anchor{box-sizing:border-box;display:inline-flex;max-width:100%;align-items:center;"
         "font-family:var(--font-sans,system-ui,sans-serif);border-radius:2px;border:0;background:transparent;"
         "padding:0;gap:.2em;font-weight:400;line-height:1.4;color:var(--text-link);cursor:pointer;"
         "text-decoration-line:underline;text-decoration-thickness:.065em;text-underline-offset:.18em;"
         "text-decoration-color:transparent;transition:color 300ms " << ease << ",text-decoration-color 300ms " << ease
      << ";outline:none}\n";
  css << ".robo-anchor[data-size=sm]{font-size:12px;line-height:1.33}\n";
  css << ".robo-anchor[data-size=md]{font-size:17px;line-height:1.47}\n";
  css << ".robo-anchor[data-size=lg]{font-size:17px;line-height:1.42}\n";
  css << "@media (min-width:735px){.robo-anchor[data-size=lg]{font-size:19px}}\n";
  css << ".robo-anchor[data-variant=subtle]:not([data-arrow]){color:var(--text-tertiary)}\n";
  css << ".robo-anchor[data-variant=plain]:not([data-arrow]){color:var(--text-primary)}\n";
  css << ".robo-anchor[data-variant=underline]:not([data-arrow]){text-decoration-color:currentColor}\n";
  css << ".robo-anchor[data-variant=standalone]:not([data-arrow]){gap:4px;font-weight:500}\n";
  css << ".robo-anchor[data-variant=arrow],.robo-anchor[data-arrow]{gap:8px;font-weight:500;text-decoration:none}\n";
  css << ".robo-anchor[data-variant=more]{gap:6px;font-weight:500;line-height:1;text-decoration:none}\n";
  css << ".robo-anchor[data-variant=more][data-size=inherit]{font-size:17px}\n";
  css << ".robo-anchor[data-tone=danger]:not([data-variant=subtle]):not([data-variant=plain]){color:var(--danger)}\n";
  css << ".robo-anchor[data-tone=danger][data-variant=arrow],.robo-anchor[data-tone=danger][data-arrow]{color:var(--danger)}\n";
  css << ".robo-anchor:hover,.robo-anchor[data-preview=hover]{color:var(--brand-primary-hover);text-decoration-color:currentColor}\n";
  css << ".robo-anchor[data-tone=danger]:hover,.robo-anchor[data-tone=danger][data-preview=hover]{color:var(--danger-hover)}\n";
  css << ".robo-anchor:active,.robo-anchor[data-preview=active]{color:var(--brand-primary-active);text-decoration-color:currentColor}\n";
  css << ".robo-anchor[data-tone=danger]:active,.robo-anchor[data-tone=danger][data-preview=active]{color:var(--danger-hover)}\n";
  css << ".robo-anchor:visited,.robo-anchor[data-preview=visited]{color:var(--brand-primary-active)}\n";
  css << ".robo-anchor[data-tone=danger]:visited,.robo-anchor[data-tone=danger][data-preview=visited]{color:var(--danger-hover)}\n";
  css << ".robo-anchor[data-variant=underline]:visited,.robo-anchor[data-variant=underline][data-preview=visited]{text-decoration-color:currentColor}\n";
  css << ".robo-anchor[data-variant=arrow]:hover,.robo-anchor[data-arrow]:hover,.robo-anchor[data-variant=arrow][data-preview=hover],"
         ".robo-anchor[data-arrow][data-preview=hover],.robo-anchor[data-variant=arrow]:active,.robo-anchor[data-arrow]:active,"
         ".robo-anchor[data-variant=arrow][data-preview=active],.robo-anchor[data-arrow][data-preview=active],"
         ".robo-anchor[data-variant=arrow]:focus-visible,.robo-anchor[data-arrow]:focus-visible{text-decoration:none}\n";
  css << ".robo-anchor[data-variant=more]:hover,.robo-anchor[data-variant=more][data-preview=hover],"
         ".robo-anchor[data-variant=more]:active,.robo-anchor[data-variant=more][data-preview=active],"
         ".robo-anchor[data-variant=more]:focus-visible,.robo-anchor[data-variant=more][data-preview=focus]"
         "{text-decoration-line:underline;text-decoration-thickness:.065em;text-underline-offset:3px;text-decoration-color:currentColor}\n";
  css << ".robo-anchor:focus-visible,.robo-anchor[data-preview=focus]{color:var(--brand-primary-hover);"
         "text-decoration-color:currentColor;outline:2px solid var(--brand-primary);outline-offset:2px}\n";
  css << ".robo-anchor[data-variant=more]:focus-visible,.robo-anchor[data-variant=more][data-preview=focus]{outline-offset:3px}\n";
  css << ".robo-anchor[data-tone=danger]:focus-visible,.robo-anchor[data-tone=danger][data-preview=focus]{color:var(--danger-hover)}\n";
  css << ".robo-anchor[data-disabled=true]{cursor:not-allowed;color:var(--text-disabled);text-decoration-color:transparent;pointer-events:none}\n";
  css << ".robo-anchor-label{min-width:0}\n";
  css << ".robo-anchor[data-arrow] .robo-anchor-label{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}\n";
  css << ".robo-anchor-icon{display:inline-flex;flex:none;align-items:center}\n";
  css << ".robo-anchor-icon svg{width:1em;height:1em;display:block}\n";
  css << ".robo-anchor-end{display:inline-flex;flex:none;transform:translateY(.05em)}\n";
  css << ".robo-anchor-launch{transform:translateY(.06em)}\n";
  css << ".robo-anchor-mark{position:relative;display:inline-flex;flex:none;align-items:center;justify-content:center;"
         "overflow:hidden;border-radius:999px;border:1px solid currentColor;background:transparent;"
         "transition:background-color 300ms " << ease << "}\n";
  css << ".robo-anchor[data-size=inherit] .robo-anchor-mark{width:1.35em;height:1.35em}\n";
  css << ".robo-anchor[data-size=sm] .robo-anchor-mark{width:18px;height:18px}\n";
  css << ".robo-anchor[data-size=md] .robo-anchor-mark{width:24px;height:24px}\n";
  css << ".robo-anchor[data-size=lg] .robo-anchor-mark{width:28px;height:28px}\n";
  css << ".robo-anchor-glyph{display:inline-flex;width:100%;height:100%;align-items:center;justify-content:center;"
         "color:currentColor;transition:transform 300ms " << ease << "}\n";
  css << ".robo-anchor-glyph svg{display:block}\n";
  css << ".robo-anchor-glyph-next{position:absolute;inset:0}\n";
  css << ".robo-anchor[data-arrow=end] .robo-anchor-glyph-next{transform:translateX(-100%)}\n";
  css << ".robo-anchor[data-arrow=start] .robo-anchor-glyph-next{transform:translateX(100%)}\n";
  css << ".robo-anchor[data-arrow=end]:hover .robo-anchor-mark,.robo-anchor[data-arrow=end]:focus-visible .robo-anchor-mark,"
         ".robo-anchor[data-arrow=end]:active .robo-anchor-mark,.robo-anchor[data-arrow=end][data-preview=hover] .robo-anchor-mark,"
         ".robo-anchor[data-arrow=end][data-preview=focus] .robo-anchor-mark,.robo-anchor[data-arrow=end][data-preview=active] .robo-anchor-mark,"
         ".robo-anchor[data-arrow=start]:hover .robo-anchor-mark,.robo-anchor[data-arrow=start]:focus-visible .robo-anchor-mark,"
         ".robo-anchor[data-arrow=start]:active .robo-anchor-mark,.robo-anchor[data-arrow=start][data-preview=hover] .robo-anchor-mark,"
         ".robo-anchor[data-arrow=start][data-preview=focus] .robo-anchor-mark,.robo-anchor[data-arrow=start][data-preview=active] .robo-anchor-mark"
         "{background:currentColor}\n";
  css << ".robo-anchor[data-arrow]:hover .robo-anchor-glyph,.robo-anchor[data-arrow]:focus-visible .robo-anchor-glyph,"
         ".robo-anchor[data-arrow]:active .robo-anchor-glyph,.robo-anchor[data-arrow][data-preview=hover] .robo-anchor-glyph,"
         ".robo-anchor[data-arrow][data-preview=focus] .robo-anchor-glyph,.robo-anchor[data-arrow][data-preview=active] .robo-anchor-glyph"
         "{color:var(--text-on-brand)}\n";
  css << ".robo-anchor[data-arrow=end]:hover .robo-anchor-glyph,.robo-anchor[data-arrow=end]:focus-visible .robo-anchor-glyph,"
         ".robo-anchor[data-arrow=end]:active .robo-anchor-glyph,.robo-anchor[data-arrow=end][data-preview=hover] .robo-anchor-glyph,"
         ".robo-anchor[data-arrow=end][data-preview=focus] .robo-anchor-glyph,.robo-anchor[data-arrow=end][data-preview=active] .robo-anchor-glyph"
         "{transform:translateX(100%)}\n";
  css << ".robo-anchor[data-arrow=end]:hover .robo-anchor-glyph-next,.robo-anchor[data-arrow=end]:focus-visible .robo-anchor-glyph-next,"
         ".robo-anchor[data-arrow=end]:active .robo-anchor-glyph-next,.robo-anchor[data-arrow=end][data-preview=hover] .robo-anchor-glyph-next,"
         ".robo-anchor[data-arrow=end][data-preview=focus] .robo-anchor-glyph-next,.robo-anchor[data-arrow=end][data-preview=active] .robo-anchor-glyph-next"
         "{transform:translateX(0)}\n";
  css << ".robo-anchor[data-arrow=start]:hover .robo-anchor-glyph,.robo-anchor[data-arrow=start]:focus-visible .robo-anchor-glyph,"
         ".robo-anchor[data-arrow=start]:active .robo-anchor-glyph,.robo-anchor[data-arrow=start][data-preview=hover] .robo-anchor-glyph,"
         ".robo-anchor[data-arrow=start][data-preview=focus] .robo-anchor-glyph,.robo-anchor[data-arrow=start][data-preview=active] .robo-anchor-glyph"
         "{transform:translateX(-100%)}\n";
  css << ".robo-anchor[data-arrow=start]:hover .robo-anchor-glyph-next,.robo-anchor[data-arrow=start]:focus-visible .robo-anchor-glyph-next,"
         ".robo-anchor[data-arrow=start]:active .robo-anchor-glyph-next,.robo-anchor[data-arrow=start][data-preview=hover] .robo-anchor-glyph-next,"
         ".robo-anchor[data-arrow=start][data-preview=focus] .robo-anchor-glyph-next,.robo-anchor[data-arrow=start][data-preview=active] .robo-anchor-glyph-next"
         "{transform:translateX(0)}\n";
  css << ".robo-anchor[data-launch=true]:not([data-variant=more]) .robo-anchor-glyph,"
         ".robo-anchor[data-launch=true]:not([data-variant=more]):hover .robo-anchor-glyph,"
         ".robo-anchor[data-launch=true]:not([data-variant=more]):focus-visible .robo-anchor-glyph,"
         ".robo-anchor[data-launch=true]:not([data-variant=more]):active .robo-anchor-glyph{transform:none}\n";
  css << ".robo-anchor-slide{position:relative;display:block;flex:none;width:.72em;height:.72em;overflow:hidden}\n";
  css << ".robo-anchor[data-variant=more] .robo-anchor-glyph{position:absolute;inset:0;width:100%;height:100%;"
         "transition:transform 400ms " << ease << "}\n";
  css << ".robo-anchor[data-variant=more] .robo-anchor-glyph svg{width:100%;height:100%;display:block}\n";
  css << ".robo-anchor[data-variant=more] .robo-anchor-glyph-next{transform:translate3d(-120%,0,0)}\n";
  css << ".robo-anchor[data-variant=more]:hover .robo-anchor-glyph:first-child,"
         ".robo-anchor[data-variant=more]:focus-visible .robo-anchor-glyph:first-child,"
         ".robo-anchor[data-variant=more]:active .robo-anchor-glyph:first-child,"
         ".robo-anchor[data-variant=more][data-preview=hover] .robo-anchor-glyph:first-child,"
         ".robo-anchor[data-variant=more][data-preview=focus] .robo-anchor-glyph:first-child,"
         ".robo-anchor[data-variant=more][data-preview=active] .robo-anchor-glyph:first-child{transform:translate3d(120%,0,0)}\n";
  css << ".robo-anchor[data-variant=more]:hover .robo-anchor-glyph-next,"
         ".robo-anchor[data-variant=more]:focus-visible .robo-anchor-glyph-next,"
         ".robo-anchor[data-variant=more]:active .robo-anchor-glyph-next,"
         ".robo-anchor[data-variant=more][data-preview=hover] .robo-anchor-glyph-next,"
         ".robo-anchor[data-variant=more][data-preview=focus] .robo-anchor-glyph-next,"
         ".robo-anchor[data-variant=more][data-preview=active] .robo-anchor-glyph-next{transform:translate3d(0,0,0)}\n";
  css << ".robo-anchor-sr{position:absolute;width:1px;height:1px;padding:0;margin:-1px;overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border:0}\n";
  css << "@media (prefers-reduced-motion:reduce){.robo-anchor,.robo-anchor-mark,.robo-anchor-glyph{transition:none}"
         ".robo-anchor[data-arrow] .robo-anchor-glyph,.robo-anchor[data-arrow] .robo-anchor-glyph-next,"
         ".robo-anchor[data-variant=more] .robo-anchor-glyph,.robo-anchor[data-variant=more] .robo-anchor-glyph-next{transform:none}}\n";
}
