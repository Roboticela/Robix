#include "TextWeb.hpp"

bool RoboWebIsTextName(const std::string& name) {
  return name == "text" || name == "heading" || name == "subhead" || name == "body" || name == "caption" ||
         name == "eyebrow" || name == "label" || name == "span" || name == "paragraph" || name == "p" ||
         name == "h1" || name == "h2" || name == "h3" || name == "h4" || name == "h5" || name == "h6";
}

std::string RoboWebTypeClass(const RoboXMLNode& node) {
  const std::string name = RoboWebLower(node.name);
  if (name == "heading" || name == "h1" || name == "h2" || name == "h3" || name == "h4" || name == "h5" ||
      name == "h6") {
    std::string level = RoboWebLower(RoboXMLAttr(node, "level"));
    if (level.empty()) {
      if (name.size() == 2 && name[0] == 'h') {
        level = name.substr(1);
      } else {
        level = "1";
      }
    }
    if (level == "display") {
      return "robo-type-display";
    }
    if (level.size() == 1 && level[0] >= '1' && level[0] <= '6') {
      return "robo-type-h" + level;
    }
    return "robo-type-h1";
  }
  if (name == "subhead") {
    return "robo-type-subhead";
  }
  if (name == "body" || name == "paragraph" || name == "p" || name == "text") {
    const std::string size = RoboWebLower(RoboXMLAttr(node, "size"));
    if (size == "sm" || size == "small") {
      return "robo-type-body";
    }
    return "robo-type-body-lg";
  }
  if (name == "caption") {
    return "robo-type-caption";
  }
  if (name == "eyebrow") {
    return "robo-type-eyebrow";
  }
  if (name == "label") {
    return "robo-type-label";
  }
  return {};
}

void RoboWebAppendTextCss(std::ostream& css) {
    css << "[data-font=\"sans\"]{font-weight:400;font-style:normal}\n";
    css << "[data-font=\"sans-medium\"]{font-weight:500}\n";
    css << "[data-font=\"sans-semibold\"]{font-weight:600}\n";
    css << "[data-font=\"sans-bold\"]{font-weight:700}\n";
    css << "[data-font=\"sans-italic\"]{font-style:italic}\n";
    css << "h1,h2,h3,h4,h5,h6,p,.robo-heading,.robo-subhead,.robo-body,.robo-text,.robo-caption,.robo-eyebrow,.robo-label,.robo-paragraph{margin:0;display:block}\n";
    css << ".robo-type-h4{font-size:22px;font-weight:600;line-height:1.22}\n";
    css << ".robo-type-h5{font-size:19px;font-weight:600;line-height:1.26}\n";
    css << ".robo-type-h6{font-size:17px;font-weight:600;line-height:1.29}\n";
    css << ".robo-type-label{font-size:17px;line-height:1.35}\n";
    css << ".robo-type.robo-type-subhead,.robo-type.robo-type-body,.robo-type.robo-type-body-lg{color:var(--robo-text,var(--text-secondary))}\n";
    css << ".robo-type.robo-type-caption,.robo-type.robo-type-eyebrow{color:var(--robo-text,var(--text-tertiary))}\n";
    css << ".robo-type.robo-type-display,.robo-type.robo-type-h1,.robo-type.robo-type-h2,.robo-type.robo-type-h3,.robo-type.robo-type-h4,.robo-type.robo-type-h5,.robo-type.robo-type-h6,.robo-type.robo-heading{color:var(--robo-heading,var(--robo-text,var(--text-primary)))}\n";
    css << ".robo-type.robo-type-label{color:var(--robo-text,var(--text-primary))}\n";
    css << ".robo-body[data-size=\"sm\"],.robo-body[data-size=\"small\"],.robo-type-body[data-size=\"sm\"],.robo-type-body[data-size=\"small\"]{font-size:17px;line-height:1.47}\n";
    css << ".robo-type-eyebrow{text-transform:uppercase}\n";
    css << ".robo-type{font-family:var(--robo-font,inherit)}\n";
    css << ".robo-type[style*=\"--robo-heading\"] :is(h1,h2,h3,h4,h5,h6,.robo-heading){color:var(--robo-heading)}\n";
    css << ".robo-type[style*=\"--robo-paragraph\"] :is(p,.robo-body,.robo-paragraph){color:var(--robo-paragraph)}\n";
    css << ".robo-type[style*=\"--robo-link\"] a{color:var(--robo-link)}\n";
    css << ".robo-type[style*=\"--robo-link\"] a:visited{color:var(--robo-visited,var(--robo-link))}\n";
    css << ".robo-type[style*=\"--robo-strong\"] strong, .robo-type[style*=\"--robo-strong\"] b{color:var(--robo-strong)}\n";
    css << ".robo-type[style*=\"--robo-muted\"] .is-muted{color:var(--robo-muted)}\n";
    css << ".robo-type[style*=\"--robo-selection\"] ::selection{background:var(--robo-selection);color:var(--robo-selection-text,inherit)}\n";
    css << ".robo-type[data-effect=glow],.robo-type[data-effect=neon]{text-shadow:0 0 12px currentColor}\n";
    css << ".robo-type[data-effect=emboss]{text-shadow:0 1px 0 rgba(255,255,255,.35),0 -1px 0 rgba(0,0,0,.6)}\n";
    css << ".robo-type[data-effect=engrave]{text-shadow:0 -1px 0 rgba(255,255,255,.28),0 1px 0 rgba(0,0,0,.7)}\n";
    css << ".robo-type[data-effect=glass]{color:transparent;background:linear-gradient(180deg,rgba(255,255,255,.9),rgba(255,255,255,.25));-webkit-background-clip:text;background-clip:text}\n";
    css << ".robo-type[data-effect=retro]{letter-spacing:.08em;text-shadow:3px 3px 0 #111}\n";
    css << ".robo-type[data-effect=3d]{text-shadow:1px 1px 0 #6b7280,2px 2px 0 #4b5563,3px 3px 0 #374151}\n";
    css << ".robo-type[data-mark=soft]{background:color-mix(in srgb,var(--brand-primary) 28%,transparent);border-radius:4px;padding:.05em .28em}\n";
    css << ".robo-type[data-mark=pill],.robo-type[data-mark=rounded]{background:var(--brand-primary);color:var(--text-on-brand);border-radius:999px;padding:.08em .5em}\n";
    css << ".robo-type[data-mark=dark]{background:var(--bg-tertiary);border-radius:6px;padding:.08em .4em}\n";
    css << ".robo-type[data-mark=block]{background:var(--bg-secondary);padding:.2em .45em}\n";
    css << ".robo-type[data-mark=underline]{background-image:linear-gradient(transparent 62%,color-mix(in srgb,var(--brand-primary) 55%,transparent) 0);background-repeat:no-repeat}\n";
    css << ".robo-type[style*=\"--robo-pad-x\"]{padding-left:var(--robo-pad-x);padding-right:var(--robo-pad-x)}\n";
    css << ".robo-type[style*=\"--robo-pad-y\"]{padding-top:var(--robo-pad-y);padding-bottom:var(--robo-pad-y)}\n";
    css << ".robo-type[data-preset=quote],.robo-type[data-preset=blockquote]{font-style:italic;border-left:3px solid var(--border-strong);padding-left:12px}\n";
    css << ".robo-type[data-preset=code]{font-family:ui-monospace,Consolas,monospace;background:var(--bg-tertiary);border:1px solid var(--border-default);border-radius:6px;padding:.1em .35em}\n";
    css << ".robo-type[data-preset=lead]{font-size:1.25em;line-height:1.45}\n";
    css << ".robo-type[data-preset=footnote],.robo-type[data-preset=fine]{font-size:12px;color:var(--robo-text,var(--text-tertiary))}\n";
    css << ".robo-type[data-preset=numeric],.robo-type[data-preset=data]{font-variant-numeric:tabular-nums}\n";
    css << ".robo-type[data-preset=link]{color:var(--text-link);text-decoration:underline;text-underline-offset:3px}\n";
    css << ".robo-type[data-density=compact]{--robo-leading:1.15}\n";
    css << ".robo-type[data-density=comfortable]{--robo-leading:1.45}\n";
    css << ".robo-type[data-density=spacious]{--robo-leading:1.7}\n";
    css << ".robo-type[data-theme=light]{--robo-text:#141418}\n";
    css << ".robo-type[data-theme=dark]{--robo-text:#f4f6fc}\n";
    css << ".robo-type[data-theme=neon]{--robo-text:#7dd3fc;text-shadow:0 0 10px rgba(125,211,252,.7)}\n";
    css << ".robo-type[data-theme=muted],.robo-type[data-theme=minimal]{--robo-text:var(--text-tertiary)}\n";
    css << ".robo-type[data-theme=high-contrast]{--robo-text:#fff;font-weight:700}\n";
    css << ".robo-type[data-dropcap=true]::first-letter{float:left;font-size:2.4em;line-height:.8;padding-right:.12em;font-weight:700}\n";
    css << ".robo-type[style*=\"--robo-hover-color\"]:hover{color:var(--robo-hover-color) !important}\n";
    css << ".robo-type[style*=\"--robo-hover-bg\"]:hover{background:var(--robo-hover-bg) !important}\n";
    css << ".robo-type[style*=\"--robo-hover-weight\"]:hover{font-weight:var(--robo-hover-weight) !important}\n";
    css << ".robo-type[style*=\"--robo-hover-tracking\"]:hover{letter-spacing:var(--robo-hover-tracking) !important}\n";
    css << ".robo-type[style*=\"--robo-hover-shadow\"]:hover{text-shadow:var(--robo-hover-shadow) !important}\n";
    css << ".robo-type[style*=\"--robo-hover-decoration\"]:hover{text-decoration:var(--robo-hover-decoration) !important}\n";
    css << ".robo-type[style*=\"--robo-hover-opacity\"]:hover{opacity:var(--robo-hover-opacity) !important}\n";
    css << ".robo-type[style*=\"--robo-hover-transform\"]:hover{transform:var(--robo-hover-transform) !important}\n";
    css << ".robo-type[style*=\"--robo-focus-color\"]:focus-visible{color:var(--robo-focus-color) !important}\n";
    css << ".robo-type[style*=\"--robo-focus-bg\"]:focus-visible{background:var(--robo-focus-bg) !important}\n";
    css << ".robo-type[style*=\"--robo-focus-outline\"]:focus-visible{outline:var(--robo-focus-outline) !important}\n";
    css << ".robo-type[style*=\"--robo-active-color\"]:active{color:var(--robo-active-color) !important}\n";
    css << ".robo-type[style*=\"--robo-active-bg\"]:active{background:var(--robo-active-bg) !important}\n";
    css << ".robo-type[style*=\"--robo-active-transform\"]:active{transform:var(--robo-active-transform) !important}\n";
    css << ".robo-type[data-disabled=true]{opacity:.5;color:var(--text-tertiary)}\n";
    css << ".robo-type[data-selected=true]{background:color-mix(in srgb,var(--brand-primary) 30%,transparent)}\n";
    css << "@keyframes robo-t-fade{from{opacity:0}to{opacity:1}}\n";
    css << "@keyframes robo-t-up{from{opacity:0;transform:translateY(8px)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-t-down{from{opacity:0;transform:translateY(-8px)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-t-left{from{opacity:0;transform:translateX(10px)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-t-right{from{opacity:0;transform:translateX(-10px)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-t-zoom{from{opacity:0;transform:scale(.96)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-t-blur{from{opacity:0;filter:blur(6px)}to{opacity:1;filter:none}}\n";
    css << "@keyframes robo-t-type{from{clip-path:inset(0 100% 0 0)}to{clip-path:inset(0 0 0 0)}}\n";
    css << "@keyframes robo-t-shift{from{background-position:0% 50%}to{background-position:200% 50%}}\n";
    css << ".robo-type[data-enter=fade]{animation:robo-t-fade var(--robo-duration,400ms) var(--robo-ease,ease) var(--robo-delay,0ms) both}\n";
    css << ".robo-type[data-enter=fade-up],.robo-type[data-enter=slide]{animation:robo-t-up var(--robo-duration,400ms) var(--robo-ease,ease) both}\n";
    css << ".robo-type[data-enter=fade-down]{animation:robo-t-down var(--robo-duration,400ms) var(--robo-ease,ease) both}\n";
    css << ".robo-type[data-enter=fade-left]{animation:robo-t-left var(--robo-duration,400ms) var(--robo-ease,ease) both}\n";
    css << ".robo-type[data-enter=fade-right]{animation:robo-t-right var(--robo-duration,400ms) var(--robo-ease,ease) both}\n";
    css << ".robo-type[data-enter=zoom],.robo-type[data-enter=zoom-in]{animation:robo-t-zoom var(--robo-duration,400ms) var(--robo-ease,ease) both}\n";
    css << ".robo-type[data-enter=blur]{animation:robo-t-blur var(--robo-duration,500ms) var(--robo-ease,ease) both}\n";
    css << ".robo-type[data-enter=typewriter],.robo-type[data-enter=characters],.robo-type[data-enter=words]{animation:robo-t-type var(--robo-duration,900ms) steps(24,end) both}\n";
    css << ".robo-type[data-motion=pulse]:hover{animation:robo-t-fade .7s ease infinite alternate}\n";
    css << ".robo-type[data-motion=shimmer],.robo-type[data-motion=gradient],.robo-type[data-motion=rainbow]{animation:robo-t-shift 3s linear infinite}\n";
    css << ".robo-type[data-motion=float]:hover{animation:robo-t-up 1.4s ease-in-out infinite alternate}\n";
    css << ".robo-type[data-motion=blink]{animation:robo-t-fade 1s steps(2,end) infinite}\n";
    css << "@media (max-width:734px){.robo-type[style*=\"--robo-font-size-sm\"]{font-size:var(--robo-font-size-sm) !important}}\n";
    css << "@media (min-width:1280px){.robo-type[style*=\"--robo-font-size-lg\"]{font-size:var(--robo-font-size-lg) !important}}\n";
}

const char* RoboWebTextTag(const std::string& name) {
  if (name == "heading" || name == "h1") return "h1";
  if (name == "h2") return "h2";
  if (name == "h3") return "h3";
  if (name == "h4") return "h4";
  if (name == "h5") return "h5";
  if (name == "h6") return "h6";
  if (name == "body" || name == "paragraph" || name == "p") return "p";
  if (name == "span" || name == "label" || name == "caption" || name == "eyebrow" || name == "subhead" ||
      name == "text") {
    return "span";
  }
  return nullptr;
}

std::string RoboWebTextClass(const std::string& name) {
  if (name == "text") return "robo-text";
  if (name == "span") return "robo-span";
  return {};
}

namespace {

std::string RoboWebTextAttr(const RoboXMLNode& node, const char* key, const char* alt = "") {
  const std::string value = RoboXMLAttr(node, key);
  if (!value.empty() || alt[0] == 0) {
    return value;
  }
  return RoboXMLAttr(node, alt);
}

std::string RoboWebTextLength(const RoboFoundationLibrary& library, const std::string& raw) {
  return RoboWebCssValue(library, raw, false);
}

std::string RoboWebTextColor(const RoboFoundationLibrary& library, const std::string& raw) {
  return RoboWebCssValue(library, raw, true);
}

std::string RoboWebTextTime(const std::string& raw) {
  if (raw.empty()) {
    return {};
  }
  return RoboWebIsNumber(raw) ? raw + "ms" : raw;
}

std::string RoboWebTextFamily(const RoboFoundationLibrary& library, const std::string& raw) {
  if (raw.empty()) {
    return {};
  }
  const std::string key = RoboWebLower(raw);
  if (key == "system" || key == "system-default") {
    return "system-ui,sans-serif";
  }
  if (key == "sans" || key == "sans-serif" || key == "geometric" || key == "humanist" || key == "rounded") {
    return "var(--font-sans,system-ui,sans-serif)";
  }
  if (key == "serif") {
    return "Georgia,\"Times New Roman\",serif";
  }
  if (key == "mono" || key == "monospace") {
    return "ui-monospace,\"Cascadia Code\",Consolas,monospace";
  }
  if (key == "display" || key == "expanded") {
    return "var(--font-sans,system-ui,sans-serif)";
  }
  if (key == "condensed") {
    return "\"Arial Narrow\",\"Segoe UI\",sans-serif";
  }
  if (key == "handwriting" || key == "cursive" || key == "decorative") {
    return "\"Segoe Script\",\"Comic Sans MS\",cursive";
  }
  if (const RoboFoundationToken* token = RoboWebToken(library, raw)) {
    if (token->kind == RoboFoundationKind::Font) {
      return "var(--font-" + token->name + ")";
    }
  }
  return raw;
}

std::string RoboWebTextWeight(const std::string& raw) {
  const std::string key = RoboWebLower(raw);
  if (key == "thin") return "100";
  if (key == "extralight" || key == "extra-light") return "200";
  if (key == "light") return "300";
  if (key == "regular" || key == "normal") return "400";
  if (key == "medium") return "500";
  if (key == "semibold" || key == "semi-bold") return "600";
  if (key == "bold") return "700";
  if (key == "extrabold" || key == "extra-bold") return "800";
  if (key == "black") return "900";
  return raw;
}

std::string RoboWebTextSize(const RoboFoundationLibrary& library, const std::string& raw) {
  const std::string key = RoboWebLower(raw);
  if (key == "xs" || key == "extra-small") return "11px";
  if (key == "sm" || key == "small") return "13px";
  if (key == "md" || key == "medium") return "15px";
  if (key == "lg" || key == "large") return "17px";
  if (key == "xl" || key == "extra-large") return "20px";
  if (key == "2xl") return "24px";
  if (key == "3xl") return "28px";
  if (key == "4xl") return "36px";
  if (key == "5xl") return "48px";
  if (key == "6xl") return "60px";
  if (key == "7xl") return "72px";
  if (key == "fluid") return "clamp(14px,2.4vw,32px)";
  return RoboWebTextLength(library, raw);
}

std::string RoboWebTextTracking(const RoboFoundationLibrary& library, const std::string& raw) {
  const std::string key = RoboWebLower(raw);
  if (key == "tighter") return "-0.05em";
  if (key == "tight") return "-0.025em";
  if (key == "normal") return "0";
  if (key == "wide") return "0.04em";
  if (key == "wider") return "0.08em";
  if (key == "widest") return "0.16em";
  return RoboWebTextLength(library, raw);
}

std::string RoboWebTextLeading(const std::string& raw) {
  const std::string key = RoboWebLower(raw);
  if (key == "compact" || key == "extra-compact") return "1.15";
  if (key == "normal") return "1.4";
  if (key == "relaxed") return "1.6";
  if (key == "loose") return "1.8";
  if (key == "extra-loose") return "2";
  return raw;
}

std::string RoboWebTextShadow(const std::string& name) {
  const std::string key = RoboWebLower(name);
  if (key == "none") return "none";
  if (key == "soft") return "0 1px 2px rgba(0,0,0,.45)";
  if (key == "hard") return "2px 2px 0 rgba(0,0,0,.8)";
  if (key == "long") return "4px 4px 0 rgba(0,0,0,.35),8px 8px 0 rgba(0,0,0,.2)";
  if (key == "glow" || key == "neon") return "0 0 12px rgba(59,158,255,.85)";
  if (key == "directional") return "3px 4px 0 rgba(0,0,0,.55)";
  if (key == "3d" || key == "extrude") return "1px 1px 0 #6b7280,2px 2px 0 #4b5563,3px 3px 0 #374151";
  return name;
}

}  // namespace

void RoboWebTextAttrs(std::string& html, const RoboXMLNode& node) {
  const auto data = [&](const char* key, std::string value) {
    if (value.empty()) {
      return;
    }
    html += " data-";
    html += key;
    html += "=\"";
    html += RoboWebEscape(value);
    html += "\"";
  };
  data("effect", RoboWebLower(RoboXMLAttr(node, "effect")));
  data("mark", RoboWebLower(RoboWebTextAttr(node, "mark", "highlight")));
  data("preset", RoboWebLower(RoboXMLAttr(node, "preset")));
  data("density", RoboWebLower(RoboXMLAttr(node, "density")));
  data("theme", RoboWebLower(RoboXMLAttr(node, "theme")));
  data("enter", RoboWebLower(RoboXMLAttr(node, "enter")));
  data("motion", RoboWebLower(RoboXMLAttr(node, "motion")));
  data("disabled", RoboWebLower(RoboXMLAttr(node, "disabled")) == "true" ? "true" : "");
  data("selected", RoboWebLower(RoboXMLAttr(node, "selected")) == "true" ? "true" : "");
  data("dropcap", RoboWebLower(RoboXMLAttr(node, "dropCap")) == "true" ? "true" : "");
  data("lines", RoboXMLAttr(node, "lines").empty() ? RoboXMLAttr(node, "maxLines") : RoboXMLAttr(node, "lines"));
}

void RoboWebAppendTextStyle(std::string& style, const RoboXMLNode& node, const RoboFoundationLibrary& library) {
  const bool textNode = RoboWebIsTextName(RoboWebLower(node.name));
  const std::string family = RoboWebTextFamily(library, RoboWebTextAttr(node, "font", "family"));
  if (!family.empty()) {
    RoboWebAddDecl(style, "--robo-font", family);
    RoboWebAddDecl(style, "font-family", family);
  }
  const std::string weight = RoboWebTextWeight(RoboWebTextAttr(node, "fontWeight", "weight"));
  if (!weight.empty()) {
    RoboWebAddDecl(style, "--robo-font-weight", weight);
    RoboWebAddDecl(style, "font-weight", weight);
  }
  const std::string sizeMin = RoboXMLAttr(node, "fontSizeMin");
  const std::string sizeMax = RoboXMLAttr(node, "fontSizeMax");
  const std::string fontSizeAttr = RoboXMLAttr(node, "fontSize");
  const std::string sizeAttr = RoboXMLAttr(node, "size");
  const std::string sizeWord = RoboWebLower(sizeAttr);
  const bool roleSize = sizeWord == "sm" || sizeWord == "small" || sizeWord == "md" || sizeWord == "medium";
  if (!sizeMin.empty() && !sizeMax.empty()) {
    const std::string fluid = "clamp(" + RoboWebTextSize(library, sizeMin) + ",2.2vw," + RoboWebTextSize(library, sizeMax) + ")";
    RoboWebAddDecl(style, "--robo-font-size", fluid);
    RoboWebAddDecl(style, "font-size", fluid);
  } else if (!fontSizeAttr.empty()) {
    const std::string size = RoboWebTextSize(library, fontSizeAttr);
    RoboWebAddDecl(style, "--robo-font-size", size);
    RoboWebAddDecl(style, "font-size", size);
  } else if (!sizeAttr.empty() && !roleSize) {
    const std::string size = RoboWebTextSize(library, sizeAttr);
    RoboWebAddDecl(style, "--robo-font-size", size);
    RoboWebAddDecl(style, "font-size", size);
  }
  RoboWebAddDecl(style, "--robo-font-size-sm", RoboWebTextSize(library, RoboXMLAttr(node, "fontSizeSm")));
  RoboWebAddDecl(style, "--robo-font-size-lg", RoboWebTextSize(library, RoboXMLAttr(node, "fontSizeLg")));
  const std::string fontStyle = RoboWebLower(RoboWebTextAttr(node, "fontStyle", "slant"));
  if (fontStyle == "italic" || fontStyle == "oblique" || fontStyle == "normal") {
    RoboWebAddDecl(style, "--robo-font-style", fontStyle);
    RoboWebAddDecl(style, "font-style", fontStyle);
  }
  const std::string tracking = RoboWebTextTracking(library, RoboWebTextAttr(node, "letterSpacing", "tracking"));
  if (!tracking.empty()) {
    RoboWebAddDecl(style, "--robo-tracking", tracking);
    RoboWebAddDecl(style, "letter-spacing", tracking);
  }
  const std::string words = RoboWebTextLength(library, RoboXMLAttr(node, "wordSpacing"));
  if (!words.empty()) {
    RoboWebAddDecl(style, "--robo-word-spacing", words);
    RoboWebAddDecl(style, "word-spacing", words);
  }
  const std::string leading = RoboWebTextLeading(RoboWebTextAttr(node, "lineHeight", "leading"));
  if (!leading.empty()) {
    RoboWebAddDecl(style, "--robo-leading", leading);
    RoboWebAddDecl(style, "line-height", leading);
  }
  const std::string color = RoboWebTextColor(library, RoboWebTextAttr(node, "color", "textColor"));
  if (!color.empty()) {
    RoboWebAddDecl(style, "--robo-text", color);
    RoboWebAddDecl(style, "color", color);
  }
  RoboWebAddDecl(style, "--robo-heading", RoboWebTextColor(library, RoboXMLAttr(node, "headingColor")));
  RoboWebAddDecl(style, "--robo-paragraph", RoboWebTextColor(library, RoboXMLAttr(node, "paragraphColor")));
  RoboWebAddDecl(style, "--robo-link", RoboWebTextColor(library, RoboWebTextAttr(node, "linkColor", "link")));
  RoboWebAddDecl(style, "--robo-strong", RoboWebTextColor(library, RoboXMLAttr(node, "strongColor")));
  RoboWebAddDecl(style, "--robo-muted", RoboWebTextColor(library, RoboXMLAttr(node, "mutedColor")));
  RoboWebAddDecl(style, "--robo-selection", RoboWebTextColor(library, RoboXMLAttr(node, "selectionColor")));
  RoboWebAddDecl(style, "--robo-selection-text", RoboWebTextColor(library, RoboXMLAttr(node, "selectionText")));
  const std::string align = RoboWebLower(textNode ? RoboWebTextAttr(node, "textAlign", "align") : RoboXMLAttr(node, "textAlign"));
  if (align == "left" || align == "start") {
    RoboWebAddDecl(style, "text-align", "start");
  } else if (align == "right" || align == "end") {
    RoboWebAddDecl(style, "text-align", "end");
  } else if (align == "center") {
    RoboWebAddDecl(style, "text-align", "center");
  } else if (align == "justify") {
    RoboWebAddDecl(style, "text-align", "justify");
  }
  const std::string vertical = RoboWebLower(RoboXMLAttr(node, "verticalAlign"));
  if (vertical == "top") {
    RoboWebAddDecl(style, "vertical-align", "top");
  } else if (vertical == "middle" || vertical == "center") {
    RoboWebAddDecl(style, "vertical-align", "middle");
  } else if (vertical == "bottom") {
    RoboWebAddDecl(style, "vertical-align", "bottom");
  }
  const std::string wrap = RoboWebLower(textNode ? RoboWebTextAttr(node, "textWrap", "wrap") : RoboXMLAttr(node, "textWrap"));
  if (wrap == "none" || wrap == "nowrap") {
    RoboWebAddDecl(style, "white-space", "nowrap");
  } else if (wrap == "char" || wrap == "character") {
    RoboWebAddDecl(style, "word-break", "break-all");
  } else if (wrap == "balance" || wrap == "pretty" || wrap == "wrap" || wrap == "word") {
    RoboWebAddDecl(style, "text-wrap", wrap == "word" ? "wrap" : wrap);
  }
  RoboWebAddDecl(style, "text-indent", RoboWebTextLength(library, RoboWebTextAttr(node, "textIndent", "hanging")));
  RoboWebAddDecl(style, "margin-bottom", RoboWebTextLength(library, RoboXMLAttr(node, "paragraphSpacing")));

  std::string decoration = RoboWebLower(RoboWebTextAttr(node, "decoration", "textDecoration"));
  if (decoration == "double" || decoration == "wavy" || decoration == "dotted" || decoration == "dashed") {
    RoboWebAddDecl(style, "text-decoration-line", decoration == "double" ? "underline" : "underline");
    RoboWebAddDecl(style, "text-decoration-style", decoration == "double" ? "double" : decoration);
  } else if (decoration == "underline" || decoration == "overline" || decoration == "line-through" ||
             decoration == "none") {
    RoboWebAddDecl(style, "text-decoration-line", decoration);
  }
  RoboWebAddDecl(style, "text-decoration-color", RoboWebTextColor(library, RoboXMLAttr(node, "decorationColor")));
  RoboWebAddDecl(style, "text-decoration-thickness", RoboWebTextLength(library, RoboXMLAttr(node, "decorationThickness")));
  RoboWebAddDecl(style, "text-underline-offset", RoboWebTextLength(library, RoboXMLAttr(node, "decorationOffset")));
  const std::string transform = RoboWebLower(RoboWebTextAttr(node, "textTransform", "transform"));
  if (transform == "uppercase" || transform == "lowercase" || transform == "capitalize" || transform == "none") {
    RoboWebAddDecl(style, "text-transform", transform);
  } else if (transform == "small-caps" || transform == "smallcaps" || RoboWebLower(RoboXMLAttr(node, "smallCaps")) == "true") {
    RoboWebAddDecl(style, "font-variant", "small-caps");
  }

  const std::string shadowX = RoboXMLAttr(node, "shadowX");
  if (textNode && !RoboXMLAttr(node, "shadows").empty()) {
    RoboWebAddDecl(style, "text-shadow", RoboXMLAttr(node, "shadows"));
  } else if (textNode && (!shadowX.empty() || !RoboXMLAttr(node, "shadowY").empty() || !RoboXMLAttr(node, "shadowBlur").empty())) {
    const std::string shadowColor = RoboWebTextColor(library, RoboXMLAttr(node, "shadowColor"));
    RoboWebAddDecl(style, "text-shadow",
                   RoboWebTextLength(library, shadowX.empty() ? "0" : shadowX) + " " +
                       RoboWebTextLength(library, RoboXMLAttr(node, "shadowY").empty() ? "2" : RoboXMLAttr(node, "shadowY")) +
                       " " +
                       RoboWebTextLength(library, RoboXMLAttr(node, "shadowBlur").empty() ? "8"
                                                                                        : RoboXMLAttr(node, "shadowBlur")) +
                       " " + (shadowColor.empty() ? "rgba(0,0,0,.45)" : shadowColor));
  } else if (textNode && (!RoboXMLAttr(node, "shadow").empty() || !RoboXMLAttr(node, "textShadow").empty())) {
    RoboWebAddDecl(style, "text-shadow",
                   RoboWebTextShadow(RoboWebTextAttr(node, "textShadow", "shadow")));
  }
  const std::string outline = RoboWebTextLength(library, RoboWebTextAttr(node, "outline", "stroke"));
  if (textNode && !outline.empty() && RoboWebLower(outline) != "none") {
    std::string thickness = outline;
    if (RoboWebLower(outline) == "thin") thickness = "1px";
    else if (RoboWebLower(outline) == "medium") thickness = "2px";
    else if (RoboWebLower(outline) == "thick") thickness = "3px";
    const std::string outlineColor = RoboWebTextColor(library, RoboXMLAttr(node, "outlineColor"));
    RoboWebAddDecl(style, "-webkit-text-stroke", thickness + " " + (outlineColor.empty() ? "currentColor" : outlineColor));
    if (RoboWebLower(RoboXMLAttr(node, "fill")) == "transparent" || RoboWebLower(RoboXMLAttr(node, "fill")) == "none") {
      RoboWebAddDecl(style, "color", "transparent");
    }
  }

  const std::string from = RoboWebTextColor(library, RoboXMLAttr(node, "gradientFrom"));
  const std::string to = RoboWebTextColor(library, RoboXMLAttr(node, "gradientTo"));
  std::string gradient = RoboXMLAttr(node, "gradient");
  if (!from.empty() || !to.empty()) {
    const std::string angle = RoboXMLAttr(node, "gradientAngle").empty() ? "90deg" : RoboXMLAttr(node, "gradientAngle");
    const std::string kind = RoboWebLower(gradient);
    const std::string body = from + "," + (to.empty() ? from : to);
    if (kind == "radial") gradient = "radial-gradient(circle," + body + ")";
    else if (kind == "conic") gradient = "conic-gradient(from " + angle + "," + body + ")";
    else gradient = "linear-gradient(" + angle + "," + body + ")";
  }
  const std::string effect = RoboWebLower(RoboXMLAttr(node, "effect"));
  if (gradient.empty() && effect == "gold") gradient = "linear-gradient(180deg,#f8e7b0,#c6a15b 45%,#fff4cc)";
  if (gradient.empty() && effect == "silver") gradient = "linear-gradient(180deg,#fff,#9ca3af 40%,#f8fafc)";
  if (gradient.empty() && effect == "chrome") gradient = "linear-gradient(180deg,#fff,#6b7280 35%,#fff 55%,#374151)";
  if (gradient.empty() && effect == "holographic") gradient = "linear-gradient(110deg,#67e8f9,#a78bfa,#f9a8d4,#67e8f9)";
  if (gradient.empty() && (effect == "rainbow")) gradient = "linear-gradient(90deg,#f43f5e,#f59e0b,#22c55e,#3b82f6,#a855f7)";
  if (textNode && !gradient.empty() && gradient.find('(') != std::string::npos) {
    RoboWebAddDecl(style, "background-image", gradient);
    RoboWebAddDecl(style, "background-size", "200% 100%");
    RoboWebAddDecl(style, "-webkit-background-clip", "text");
    RoboWebAddDecl(style, "background-clip", "text");
    RoboWebAddDecl(style, "color", "transparent");
  }
  const std::string image = RoboXMLAttr(node, "backgroundImage");
  if (textNode && !image.empty()) {
    RoboWebAddDecl(style, "background-image", "url('" + image + "')");
    RoboWebAddDecl(style, "-webkit-background-clip", "text");
    RoboWebAddDecl(style, "background-clip", "text");
    RoboWebAddDecl(style, "color", "transparent");
  }

  const std::string mark = RoboWebTextColor(library, RoboWebTextAttr(node, "markColor", "background"));
  if (textNode && !mark.empty()) {
    RoboWebAddDecl(style, "background", mark);
  }
  RoboWebAddDecl(style, "--robo-pad-x", RoboWebTextLength(library, RoboXMLAttr(node, "paddingX")));
  RoboWebAddDecl(style, "--robo-pad-y", RoboWebTextLength(library, RoboXMLAttr(node, "paddingY")));
  RoboWebAddDecl(style, "border-radius", RoboWebTextLength(library, RoboXMLAttr(node, "radius")));

  const std::string lines = RoboXMLAttr(node, "lines").empty() ? RoboXMLAttr(node, "maxLines") : RoboXMLAttr(node, "lines");
  const std::string overflow = RoboWebLower(RoboWebTextAttr(node, "textOverflow", "overflow"));
  if (textNode && RoboWebIsNumber(lines)) {
    RoboWebAddDecl(style, "display", "-webkit-box");
    RoboWebAddDecl(style, "-webkit-box-orient", "vertical");
    RoboWebAddDecl(style, "-webkit-line-clamp", lines);
    RoboWebAddDecl(style, "overflow", "hidden");
  } else if (overflow == "ellipsis") {
    RoboWebAddDecl(style, "overflow", "hidden");
    RoboWebAddDecl(style, "text-overflow", "ellipsis");
    RoboWebAddDecl(style, "white-space", "nowrap");
  } else if (overflow == "clip" || overflow == "hidden" || overflow == "visible") {
    RoboWebAddDecl(style, "overflow", overflow);
  }
  if (RoboWebLower(RoboXMLAttr(node, "fade")) == "true") {
    RoboWebAddDecl(style, "mask-image", "linear-gradient(90deg,#000 70%,transparent)");
  }
  const std::string dir = RoboWebLower(textNode ? RoboWebTextAttr(node, "direction", "dir") : RoboXMLAttr(node, "dir"));
  if (dir == "rtl" || dir == "right-to-left") RoboWebAddDecl(style, "direction", "rtl");
  else if (dir == "ltr" || dir == "left-to-right") RoboWebAddDecl(style, "direction", "ltr");
  else if (dir == "auto") RoboWebAddDecl(style, "direction", "auto");
  const std::string writing = RoboWebLower(RoboXMLAttr(node, "writingMode"));
  if (writing == "vertical" || writing == "vertical-rl") RoboWebAddDecl(style, "writing-mode", "vertical-rl");
  else if (writing == "vertical-lr") RoboWebAddDecl(style, "writing-mode", "vertical-lr");
  else if (writing == "horizontal") RoboWebAddDecl(style, "writing-mode", "horizontal-tb");
  const std::string features = RoboWebLower(RoboWebTextAttr(node, "features", "numerals"));
  if (features == "tabular" || features == "tabular-nums") RoboWebAddDecl(style, "font-variant-numeric", "tabular-nums");
  else if (features == "oldstyle" || features == "old-style") RoboWebAddDecl(style, "font-variant-numeric", "oldstyle-nums");
  else if (features == "fractions") RoboWebAddDecl(style, "font-variant-numeric", "diagonal-fractions");
  else if (!RoboXMLAttr(node, "features").empty()) RoboWebAddDecl(style, "font-feature-settings", RoboXMLAttr(node, "features"));
  if (RoboWebLower(RoboXMLAttr(node, "ligatures")) == "none") RoboWebAddDecl(style, "font-variant-ligatures", "none");
  else if (RoboWebLower(RoboXMLAttr(node, "ligatures")) == "discretionary") RoboWebAddDecl(style, "font-variant-ligatures", "discretionary-ligatures");
  if (RoboWebLower(RoboXMLAttr(node, "kerning")) == "none") RoboWebAddDecl(style, "font-kerning", "none");
  if (RoboWebLower(RoboXMLAttr(node, "fontSmooth")) == "antialiased") {
    RoboWebAddDecl(style, "-webkit-font-smoothing", "antialiased");
  }

  RoboWebAddDecl(style, "--robo-hover-color", RoboWebTextColor(library, RoboXMLAttr(node, "hoverColor")));
  RoboWebAddDecl(style, "--robo-hover-bg", RoboWebTextColor(library, RoboXMLAttr(node, "hoverBackground")));
  RoboWebAddDecl(style, "--robo-hover-weight", RoboWebTextWeight(RoboXMLAttr(node, "hoverWeight")));
  RoboWebAddDecl(style, "--robo-hover-tracking", RoboWebTextTracking(library, RoboXMLAttr(node, "hoverTracking")));
  if (!RoboXMLAttr(node, "hoverGlow").empty() || !RoboXMLAttr(node, "hoverShadow").empty()) {
    const std::string glow = RoboWebTextColor(library, RoboXMLAttr(node, "hoverGlow"));
    RoboWebAddDecl(style, "--robo-hover-shadow", glow.empty() ? RoboWebTextShadow(RoboXMLAttr(node, "hoverShadow"))
                                                             : "0 0 12px " + glow);
  }
  RoboWebAddDecl(style, "--robo-hover-opacity", RoboXMLAttr(node, "hoverOpacity"));
  if (RoboWebLower(RoboXMLAttr(node, "hoverDecoration")) == "underline") {
    RoboWebAddDecl(style, "--robo-hover-decoration", "underline");
  }
  if (!RoboXMLAttr(node, "hoverTransform").empty()) {
    RoboWebAddDecl(style, "--robo-hover-transform", RoboXMLAttr(node, "hoverTransform"));
  }
  RoboWebAddDecl(style, "--robo-focus-color", RoboWebTextColor(library, RoboXMLAttr(node, "focusColor")));
  RoboWebAddDecl(style, "--robo-focus-bg", RoboWebTextColor(library, RoboXMLAttr(node, "focusBackground")));
  if (!RoboXMLAttr(node, "focusOutline").empty()) {
    RoboWebAddDecl(style, "--robo-focus-outline", "2px solid " + RoboWebTextColor(library, RoboXMLAttr(node, "focusOutline")));
  }
  RoboWebAddDecl(style, "--robo-active-color", RoboWebTextColor(library, RoboXMLAttr(node, "activeColor")));
  RoboWebAddDecl(style, "--robo-active-bg", RoboWebTextColor(library, RoboXMLAttr(node, "activeBackground")));
  if (!RoboXMLAttr(node, "activeScale").empty()) {
    RoboWebAddDecl(style, "--robo-active-transform", "scale(" + RoboXMLAttr(node, "activeScale") + ")");
  }
  RoboWebAddDecl(style, "--robo-duration", RoboWebTextTime(RoboWebTextAttr(node, "duration", "speed")));
  RoboWebAddDecl(style, "--robo-delay", RoboWebTextTime(RoboXMLAttr(node, "delay")));
  RoboWebAddDecl(style, "--robo-ease", RoboXMLAttr(node, "easing"));
  RoboWebAddDecl(style, "--robo-iteration", RoboXMLAttr(node, "iteration"));
  if (!RoboXMLAttr(node, "duration").empty()) {
    const std::string time = RoboWebTextTime(RoboXMLAttr(node, "duration"));
    RoboWebAddDecl(style, "transition", "color " + time + " ease,background " + time + " ease,letter-spacing " + time +
                                            " ease,text-shadow " + time + " ease,opacity " + time + " ease");
  }
}
