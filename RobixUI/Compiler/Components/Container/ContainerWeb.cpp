#include "ContainerWeb.hpp"

#include "../Text/TextWeb.hpp"

#include <string>

namespace {

std::string RoboWebContainerAttr(const RoboXMLNode& node, const char* key, const char* alt = "") {
  const std::string value = RoboXMLAttr(node, key);
  if (!value.empty() || alt[0] == 0) {
    return value;
  }
  return RoboXMLAttr(node, alt);
}

std::string RoboWebContainerLength(const RoboFoundationLibrary& library, const std::string& raw) {
  return RoboWebCssValue(library, raw, false);
}

std::string RoboWebContainerColor(const RoboFoundationLibrary& library, const std::string& raw) {
  return RoboWebCssValue(library, raw, true);
}

std::string RoboWebContainerTime(const std::string& raw) {
  if (raw.empty()) {
    return {};
  }
  if (RoboWebIsNumber(raw)) {
    return raw + "ms";
  }
  return raw;
}

std::string RoboWebContainerRatio(std::string raw) {
  const std::string key = RoboWebLower(raw);
  if (key == "1:1" || key == "square") {
    return "1 / 1";
  }
  if (key == "4:3") {
    return "4 / 3";
  }
  if (key == "3:2") {
    return "3 / 2";
  }
  if (key == "16:9") {
    return "16 / 9";
  }
  if (key == "21:9") {
    return "21 / 9";
  }
  if (key == "9:16") {
    return "9 / 16";
  }
  for (char& ch : raw) {
    if (ch == ':') {
      ch = '/';
    }
  }
  return raw;
}

std::string RoboWebContainerShadow(const std::string& name) {
  const std::string key = RoboWebLower(name);
  if (key == "none" || key == "flat") {
    return "none";
  }
  if (key == "subtle") {
    return "0 1px 2px rgba(0,0,0,.28)";
  }
  if (key == "soft") {
    return "0 4px 16px rgba(0,0,0,.24)";
  }
  if (key == "medium" || key == "elevated") {
    return "0 8px 24px rgba(0,0,0,.30)";
  }
  if (key == "large") {
    return "0 16px 40px rgba(0,0,0,.36)";
  }
  if (key == "xl" || key == "extra-large" || key == "extralarge") {
    return "0 24px 64px rgba(0,0,0,.42)";
  }
  if (key == "floating" || key == "float") {
    return "0 18px 40px rgba(0,0,0,.38)";
  }
  if (key == "ambient") {
    return "0 0 48px rgba(0,0,0,.40)";
  }
  if (key == "directional") {
    return "8px 14px 28px rgba(0,0,0,.34)";
  }
  if (key == "inner" || key == "inset" || key == "recessed") {
    return "inset 0 2px 10px rgba(0,0,0,.48)";
  }
  if (key == "glow") {
    return "0 0 24px rgba(59,158,255,.45)";
  }
  if (key == "neon") {
    return "0 0 10px #3B9EFF, 0 0 28px rgba(59,158,255,.55)";
  }
  if (key == "colored") {
    return "0 10px 28px var(--c-shadow, rgba(59,158,255,.35))";
  }
  return name;
}

std::string RoboWebContainerImage(const std::string& raw) {
  if (raw.empty()) {
    return {};
  }
  const std::string key = RoboWebLower(raw);
  if (key.rfind("url(", 0) == 0 || key.rfind("linear-gradient", 0) == 0 || key.rfind("radial-gradient", 0) == 0 ||
      key.rfind("conic-gradient", 0) == 0 || key.rfind("var(", 0) == 0) {
    return raw;
  }
  std::string safe;
  for (char ch : raw) {
    if (ch == '\'' || ch == '"' || ch == '(' || ch == ')' || ch == ';') {
      continue;
    }
    safe.push_back(ch);
  }
  return "url('" + safe + "')";
}

void RoboWebContainerAxis(std::string& style, const RoboFoundationLibrary& library, const char* xProp, const char* yProp,
                          const std::string& value) {
  const std::string length = RoboWebContainerLength(library, value);
  RoboWebAddDecl(style, xProp, length);
  RoboWebAddDecl(style, yProp, length);
}

}  // namespace

void RoboWebAppendContainerCss(std::ostream& css) {
  css << ".robo-overlay{position:fixed;inset:0;z-index:50}\n";
  css << ".robo-container[data-display=stack]{display:grid}\n";
  css << ".robo-container[data-display=stack]>*{grid-area:1/1}\n";
  css << ".robo-button{display:inline-flex;align-items:center;justify-content:center;gap:6px;height:28px;padding:0 12px;"
         "border:0;border-radius:6px;font:500 13px/1 -apple-system,BlinkMacSystemFont,\"Segoe UI\",sans-serif;cursor:pointer;"
         "background:var(--brand-primary);color:var(--text-on-brand,white)}\n";
  css << ".robo-button[data-variant=secondary],.robo-button[data-variant=tinted]{background:var(--brand-primary-subtle);color:var(--brand-primary)}\n";
  css << ".robo-button[data-variant=action],.robo-button[data-variant=bordered],.robo-button[data-variant=outlined]{background:var(--bg-tertiary);color:var(--text-primary);"
         "box-shadow:inset 0 0 0 1px var(--border-default)}\n";
  css << ".robo-button[data-variant=ghost],.robo-button[data-variant=plain],.robo-button[data-variant=link]{background:transparent;color:var(--brand-primary);box-shadow:none}\n";
  css << ".robo-button[data-variant=danger]{background:var(--danger,#ff3b30);color:white}\n";
  css << "input.robo-field,textarea.robo-field,input.robo-type,textarea.robo-type{appearance:none;-webkit-appearance:none;box-sizing:border-box;display:block;margin:0;"
         "border:1px solid var(--border-default);border-radius:6px;background:var(--bg-elevated);color:var(--text-primary);"
         "font:14px/1.2 Inter,-apple-system,BlinkMacSystemFont,\"Segoe UI\",sans-serif;outline:none;box-shadow:none;caret-color:transparent !important;"
         "cursor:text;user-select:text;-webkit-user-select:text}\n";
  css << ".robo-field-caret{position:fixed;z-index:36;display:none;pointer-events:none;border-radius:999px;transform:translateX(-50%);background:var(--robo-caret,var(--text-primary))}\n";
  css << "input.robo-field,input.robo-type{padding:0 10px}\n";
  css << "textarea.robo-field,textarea.robo-type{padding:10px;line-height:1.35;resize:none}\n";
  css << "input.robo-field:focus,textarea.robo-field:focus,input.robo-type:focus,textarea.robo-type:focus,"
         "input.robo-field:focus-visible,textarea.robo-field:focus-visible,input.robo-type:focus-visible,textarea.robo-type:focus-visible{"
         "outline:none;border-color:var(--border-default);box-shadow:none}\n";
  css << "input.robo-field::placeholder,textarea.robo-field::placeholder,input.robo-type::placeholder,textarea.robo-type::placeholder{color:var(--text-tertiary);opacity:1}\n";
  css << "input.robo-field:disabled,textarea.robo-field:disabled,input.robo-type:disabled,textarea.robo-type:disabled{"
         "background:var(--bg-secondary);color:var(--text-disabled);-webkit-text-fill-color:var(--text-disabled);opacity:1;cursor:default}\n";
  css << "input.robo-field::selection,textarea.robo-field::selection,input.robo-type::selection,textarea.robo-type::selection{"
         "background:color-mix(in srgb,var(--brand-primary) 45%,var(--bg-primary));color:var(--text-primary)}\n";
  css << "input.robo-field::-ms-reveal,input.robo-field::-ms-clear,input.robo-type::-ms-reveal,input.robo-type::-ms-clear{display:none}\n";
  css << ".robo-segmented,.robo-segmentedcontrol{display:inline-flex;align-items:center;height:28px;padding:2px;gap:0;border-radius:7px;background:var(--bg-tertiary)}\n";
  css << ".robo-sidebar{background:var(--bg-secondary);padding:8px}\n";
  css << ".robo-titlebar{display:flex;align-items:center;height:52px;padding:0 12px;background:var(--bg-secondary);border-bottom:1px solid var(--border-subtle)}\n";
  css << ".robo-card,.robo-panel,.robo-surface,.robo-tile,.robo-well{background:var(--bg-elevated);border:1px solid var(--border-default);border-radius:10px;padding:16px;"
         "box-shadow:0 1px 2px rgba(0,0,0,.08)}\n";
  css << ".robo-button[data-state=off],.robo-switch[data-state=off]{opacity:.72}\n";
  css << ".robo-textfield,.robo-textarea,.robo-select{display:flex;flex-direction:column;gap:6px;width:100%}\n";
  css << ".robo-textfield [contenteditable],.robo-textarea [contenteditable]{min-height:1.4em;outline:none}\n";
  css << ".robo-select [data-panel]{display:none}\n";
  css << ".robo-select[data-open=true] [data-panel]{display:block}\n";
  css << ".robo-audio{position:absolute;width:0;height:0;margin:0;padding:0;border:0;outline:none;box-shadow:none;opacity:0;overflow:hidden;pointer-events:none}\n";
  css << ".robo-list{list-style:none;margin:0;padding:0;display:flex;flex-direction:column;gap:8px;width:100%;max-width:100%;min-width:0;box-sizing:border-box}\n";
  css << ".robo-list>li{list-style:none;margin:0;padding:0;min-width:0}\n";
  css << ".robo-scroll{overflow:auto;position:relative;min-height:0;min-width:0;scrollbar-width:none}\n";
  css << ".robo-scroll::-webkit-scrollbar{width:0;height:0}\n";
  css << ".robo-scroll>.robo-bars{position:fixed;inset:auto;z-index:5}\n";
  css << ".robo-board,.robo-container,.robo-scroll{box-sizing:border-box;min-width:0;border:0;outline:none}\n";
  css << ".robo-board:focus,.robo-board:focus-visible,.robo-container:focus,.robo-container:focus-visible,"
         ".robo-scroll:focus,.robo-scroll:focus-visible{outline:none}\n";
  css << ".robo-container{position:relative;flex-shrink:0}\n";
  css << ".robo-container>.robo-container-video{position:absolute;inset:0;width:100%;height:100%;object-fit:cover;"
         "z-index:0;pointer-events:none;opacity:var(--c-media-opacity,1)}\n";
  css << ".robo-container[data-layout=layered],.robo-container[data-layout=overlay]{display:grid}\n";
  css << ".robo-container[data-layout=layered]>*,.robo-container[data-layout=overlay]>*{grid-area:1/1}\n";
  css << ".robo-container[data-layout=absolute]{position:relative}\n";
  css << ".robo-container[data-shape=pill],.robo-container[data-shape=capsule]{border-radius:999px}\n";
  css << ".robo-container[data-shape=rounded]{border-radius:12px}\n";
  css << ".robo-container[data-shape=circle],.robo-container[data-shape=ellipse]{border-radius:50%}\n";
  css << ".robo-container[data-shape=squircle]{border-radius:28%}\n";
  css << ".robo-container[data-shape=square]{border-radius:0}\n";
  css << ".robo-container[data-shape=cut],.robo-container[data-shape=chamfer]{clip-path:polygon(14px 0,calc(100% - 14px) 0,100% 14px,100% calc(100% - 14px),calc(100% - 14px) 100%,14px 100%,0 calc(100% - 14px),0 14px)}\n";
  css << ".robo-container[data-shape=notch]{clip-path:polygon(0 0,calc(50% - 10px) 0,50% 10px,calc(50% + 10px) 0,100% 0,100% 100%,0 100%)}\n";
  css << ".robo-container[data-texture=noise]::after{content:\"\";position:absolute;inset:0;pointer-events:none;"
         "opacity:.22;background-image:radial-gradient(rgba(255,255,255,.55) .6px,transparent .6px);background-size:3px 3px}\n";
  css << ".robo-container[data-texture=pattern]::after{content:\"\";position:absolute;inset:0;pointer-events:none;"
         "opacity:.16;background-image:linear-gradient(rgba(255,255,255,.4) 1px,transparent 1px),linear-gradient(90deg,rgba(255,255,255,.4) 1px,transparent 1px);background-size:16px 16px}\n";
  css << ".robo-container[data-texture=mesh]{background-image:radial-gradient(at 15% 20%,rgba(59,158,255,.35),transparent 42%),radial-gradient(at 85% 10%,rgba(168,85,247,.28),transparent 40%),radial-gradient(at 50% 90%,rgba(16,185,129,.22),transparent 46%)}\n";
  css << ".robo-container[data-accent=top]::before,.robo-container[data-accent=bottom]::before,"
         ".robo-container[data-accent=left]::before,.robo-container[data-accent=right]::before{content:\"\";position:absolute;background:var(--c-accent,var(--brand-primary));pointer-events:none}\n";
  css << ".robo-container[data-accent=top]::before{left:0;right:0;top:0;height:2px}\n";
  css << ".robo-container[data-accent=bottom]::before{left:0;right:0;bottom:0;height:2px}\n";
  css << ".robo-container[data-accent=left]::before{top:0;bottom:0;left:0;width:3px}\n";
  css << ".robo-container[data-accent=right]::before{top:0;bottom:0;right:0;width:3px}\n";
  css << ".robo-container[data-fade=true]{mask-image:linear-gradient(to bottom,transparent,black 8%,black 92%,transparent)}\n";
  css << ".robo-container[style*=\"--robo-heading\"] :is(h1,h2,h3,h4,h5,h6,.robo-heading){color:var(--robo-heading)}\n";
  css << ".robo-container[style*=\"--robo-paragraph\"] :is(p,.robo-body,.robo-paragraph){color:var(--robo-paragraph)}\n";
  css << ".robo-container[style*=\"--robo-link\"] a{color:var(--robo-link)}\n";
  css << ".robo-container[style*=\"--robo-selection\"]::selection,.robo-container[style*=\"--robo-selection\"] ::selection{background:var(--robo-selection);color:var(--robo-selection-text,inherit)}\n";
  css << ".robo-container[data-disabled=true]{opacity:.55;pointer-events:none}\n";
  css << ".robo-container[data-selected=true]{outline:2px solid var(--brand-primary);outline-offset:2px}\n";
  css << ".robo-container[data-density=compact]{--c-pad:8px}\n";
  css << ".robo-container[data-density=comfortable]{--c-pad:16px}\n";
  css << ".robo-container[data-density=spacious]{--c-pad:28px}\n";
  css << ".robo-container[data-density]{padding:var(--c-pad)}\n";
  css << ".robo-container[data-preset=minimal]{background:transparent;box-shadow:none}\n";
  css << ".robo-container[data-preset=elevated]{background:var(--bg-secondary);box-shadow:0 10px 28px rgba(0,0,0,.32);border-radius:12px}\n";
  css << ".robo-container[data-preset=glass],.robo-container[data-preset=acrylic],.robo-container[data-preset=mica],"
         ".robo-container[data-preset=liquid]{background:rgba(255,255,255,.08);backdrop-filter:blur(18px);border:1px solid rgba(255,255,255,.16)}\n";
  css << ".robo-container[data-preset=dark]{background:var(--bg-primary);color:var(--text-primary);border:1px solid var(--border-default)}\n";
  css << ".robo-container[data-preset=neumorphic],.robo-container[data-preset=soft]{background:#2a2a2e;border-radius:16px;"
         "box-shadow:7px 7px 16px #17171a,-6px -6px 16px #3d3d44}\n";
  css << ".robo-container[data-preset=brutalist]{background:var(--bg-secondary);border:3px solid var(--text-primary);border-radius:0;box-shadow:6px 6px 0 var(--text-primary)}\n";
  css << ".robo-container[data-preset=neon],.robo-container[data-preset=cyberpunk]{background:#0c1220;color:#e8f4ff;border:1px solid #3B9EFF;"
         "box-shadow:0 0 18px rgba(59,158,255,.45)}\n";
  css << ".robo-container[data-preset=organic]{background:#24312b;color:#e7f2ea;border-radius:28px 18px 32px 16px}\n";
  css << ".robo-container[data-preset=material]{background:var(--bg-secondary);border-radius:12px;box-shadow:0 3px 8px rgba(0,0,0,.28)}\n";
  css << ".robo-container[data-preset=fluent],.robo-container[data-preset=windows]{background:rgba(32,32,36,.72);"
         "backdrop-filter:blur(24px);border:1px solid rgba(255,255,255,.12);border-radius:8px}\n";
  css << ".robo-container[data-preset=macos]{background:rgba(40,40,44,.78);backdrop-filter:blur(20px);border-radius:14px;"
         "border:1px solid rgba(255,255,255,.14)}\n";
  css << ".robo-container[data-preset=classic]{background:#d4d0c8;color:#111;border:2px solid;border-color:#fff #404040 #404040 #fff}\n";
  css << ".robo-container[data-preset=modern],.robo-container[data-preset=corporate]{background:var(--bg-secondary);"
         "border:1px solid var(--border-default);border-radius:10px}\n";
  css << ".robo-container[data-preset=luxury]{background:linear-gradient(160deg,#1c1a17,#3a342c);color:#f6ecd8;border:1px solid #c6a15b}\n";
  css << ".robo-container[data-preset=editorial]{background:transparent;border-top:1px solid var(--border-strong);border-radius:0}\n";
  css << ".robo-container[data-preset=retro]{background:#1b2838;color:#c7d5e0;border:2px solid #66c0f4;box-shadow:inset 0 0 0 1px #000}\n";
  css << ".robo-container[data-preset=industrial]{background:#1e2124;border:1px solid #3a3f44;border-radius:2px}\n";
  css << ".robo-container[data-preset=flat]{background:var(--bg-tertiary);box-shadow:none;border-radius:8px}\n";
  css << ".robo-container[data-preset=skeuomorphic]{background:linear-gradient(#3a3a3e,#242428);border:1px solid #111;border-radius:10px;"
         "box-shadow:inset 0 1px 0 rgba(255,255,255,.18)}\n";
  css << ".robo-container[data-preset=monochrome]{background:#111;color:#f5f5f5;border:1px solid #f5f5f5}\n";
  css << ".robo-container[data-preset=pastel]{background:#2a2633;color:#f3e9ff;border-radius:18px}\n";
  css << ".robo-container[data-preset=gradient]{background:linear-gradient(135deg,#1d4ed8,#7c3aed 55%,#0f766e);color:#fff}\n";
  css << ".robo-container[data-chrome=window],.robo-container[data-chrome=panel],.robo-container[data-chrome=floating],"
         ".robo-container[data-chrome=card]{background:var(--bg-secondary);border:1px solid var(--border-default);border-radius:12px;overflow:hidden}\n";
  css << ".robo-container[data-chrome=window]::before{content:\"\";display:block;height:32px;background:var(--c-titlebar,var(--bg-tertiary));"
         "border-bottom:1px solid var(--border-default)}\n";
  css << ".robo-container[data-chrome=sidebar],.robo-container[data-chrome=docked]{background:var(--bg-primary);border-right:1px solid var(--border-default)}\n";
  css << ".robo-container[data-chrome=split]{background:transparent;gap:1px}\n";
  css << ".robo-container[data-theme=light]{background:#f4f6fc;color:#141418;border-color:#d2d2d7}\n";
  css << ".robo-container[data-theme=dark]{background:#141418;color:#f4f6fc}\n";
  css << ".robo-container[style*=\"--c-scroll-size\"]{scrollbar-width:thin;scrollbar-color:var(--c-scroll-thumb,var(--text-tertiary)) var(--c-scroll-track,transparent)}\n";
  css << ".robo-container[style*=\"--c-scroll-size\"]::-webkit-scrollbar{width:var(--c-scroll-size);height:var(--c-scroll-size)}\n";
  css << ".robo-container[style*=\"--c-scroll-size\"]::-webkit-scrollbar-track{background:var(--c-scroll-track,transparent)}\n";
  css << ".robo-container[style*=\"--c-scroll-size\"]::-webkit-scrollbar-thumb{background:var(--c-scroll-thumb,var(--text-tertiary));border-radius:999px}\n";
  css << ".robo-container[style*=\"--c-scroll-hover\"]::-webkit-scrollbar-thumb:hover{background:var(--c-scroll-hover)}\n";
  css << ".robo-container[style*=\"--c-hover-bg\"]:hover{background:var(--c-hover-bg) !important}\n";
  css << ".robo-container[style*=\"--c-hover-border\"]:hover{border-color:var(--c-hover-border) !important}\n";
  css << ".robo-container[style*=\"--c-hover-border-width\"]:hover{border-width:var(--c-hover-border-width) !important}\n";
  css << ".robo-container[style*=\"--c-hover-radius\"]:hover{border-radius:var(--c-hover-radius) !important}\n";
  css << ".robo-container[style*=\"--c-hover-shadow\"]:hover{box-shadow:var(--c-hover-shadow) !important}\n";
  css << ".robo-container[style*=\"--c-hover-opacity\"]:hover{opacity:var(--c-hover-opacity) !important}\n";
  css << ".robo-container[style*=\"--c-hover-filter\"]:hover{filter:var(--c-hover-filter) !important}\n";
  css << ".robo-container[style*=\"--c-hover-transform\"]:hover{transform:var(--c-hover-transform) !important}\n";
  css << ".robo-container[style*=\"--c-hover-color\"]:hover{color:var(--c-hover-color) !important}\n";
  css << ".robo-container[style*=\"--c-focus-border\"]:focus-visible{border-color:var(--c-focus-border) !important}\n";
  css << ".robo-container[style*=\"--c-focus-width\"]:focus-visible{border-width:var(--c-focus-width) !important}\n";
  css << ".robo-container[style*=\"--c-focus-outline\"]:focus-visible{outline:var(--c-focus-outline) !important}\n";
  css << ".robo-container[style*=\"--c-focus-offset\"]:focus-visible{outline-offset:var(--c-focus-offset) !important}\n";
  css << ".robo-container[style*=\"--c-focus-shadow\"]:focus-visible{box-shadow:var(--c-focus-shadow) !important}\n";
  css << ".robo-container[style*=\"--c-focus-bg\"]:focus-visible{background:var(--c-focus-bg) !important}\n";
  css << ".robo-container[style*=\"--c-focus-opacity\"]:focus-visible{opacity:var(--c-focus-opacity) !important}\n";
  css << ".robo-container[style*=\"--c-active-bg\"]:active{background:var(--c-active-bg) !important}\n";
  css << ".robo-container[style*=\"--c-active-border\"]:active{border-color:var(--c-active-border) !important}\n";
  css << ".robo-container[style*=\"--c-active-shadow\"]:active{box-shadow:var(--c-active-shadow) !important}\n";
  css << ".robo-container[style*=\"--c-active-transform\"]:active{transform:var(--c-active-transform) !important}\n";
  css << ".robo-container[style*=\"--c-active-opacity\"]:active{opacity:var(--c-active-opacity) !important}\n";
  css << "@keyframes robo-c-fade{from{opacity:0}to{opacity:1}}\n";
  css << "@keyframes robo-c-fade-up{from{opacity:0;transform:translateY(12px)}to{opacity:1;transform:none}}\n";
  css << "@keyframes robo-c-fade-down{from{opacity:0;transform:translateY(-12px)}to{opacity:1;transform:none}}\n";
  css << "@keyframes robo-c-fade-left{from{opacity:0;transform:translateX(12px)}to{opacity:1;transform:none}}\n";
  css << "@keyframes robo-c-fade-right{from{opacity:0;transform:translateX(-12px)}to{opacity:1;transform:none}}\n";
  css << "@keyframes robo-c-zoom{from{opacity:0;transform:scale(.96)}to{opacity:1;transform:none}}\n";
  css << "@keyframes robo-c-zoom-out{from{opacity:0;transform:scale(1.04)}to{opacity:1;transform:none}}\n";
  css << "@keyframes robo-c-slide{from{opacity:0;transform:translateY(20px)}to{opacity:1;transform:none}}\n";
  css << "@keyframes robo-c-flip{from{opacity:0;transform:perspective(400px) rotateX(-12deg)}to{opacity:1;transform:none}}\n";
  css << "@keyframes robo-c-rotate{from{opacity:0;transform:rotate(-6deg)}to{opacity:1;transform:none}}\n";
  css << "@keyframes robo-c-bounce{0%{opacity:0;transform:translateY(16px)}60%{opacity:1;transform:translateY(-4px)}100%{transform:none}}\n";
  css << "@keyframes robo-c-blur{from{opacity:0;filter:blur(8px)}to{opacity:1;filter:none}}\n";
  css << ".robo-container[data-enter]{animation-name:var(--c-enter);animation-duration:var(--c-duration,400ms);"
         "animation-delay:var(--c-delay,0ms);animation-timing-function:var(--c-ease,cubic-bezier(.2,.8,.2,1));"
         "animation-fill-mode:both;animation-iteration-count:var(--c-iteration,1)}\n";
  css << ".robo-container[data-motion=lift]:hover{transform:translateY(-4px)}\n";
  css << ".robo-container[data-motion=scale]:hover{transform:scale(1.02)}\n";
  css << ".robo-container[data-motion=tilt]:hover{transform:perspective(500px) rotateX(4deg) rotateY(-4deg)}\n";
  css << ".robo-container[data-motion=rotate]:hover{transform:rotate(-1.5deg)}\n";
  css << ".robo-container[data-motion=glow]:hover{box-shadow:0 0 24px rgba(59,158,255,.45)}\n";
  css << ".robo-container[data-motion=pulse]:hover{animation:robo-c-fade .8s ease infinite alternate}\n";
  css << ".robo-container[data-motion=shake]:hover{animation:robo-c-rotate .35s ease}\n";
  css << ".robo-container[data-motion=float]:hover{animation:robo-c-fade-up 1.6s ease-in-out infinite alternate}\n";
}

const char* RoboWebContainerClass() { return "robo-container"; }

bool RoboWebIsContainerBox(const std::string& name) {
  return name == "container" || name == "view" || name == "box" || name == "flex" || name == "stack" || name == "canvas";
}

void RoboWebContainerAttrs(std::string& html, const RoboXMLNode& node) {
  const auto data = [&](const char* key, const std::string& value) {
    if (value.empty()) {
      return;
    }
    html += " data-";
    html += key;
    html += "=\"";
    html += RoboWebEscape(value);
    html += "\"";
  };
  std::string preset = RoboWebLower(RoboWebContainerAttr(node, "preset", "variant"));
  if (preset == "windows11" || preset == "windows-11") {
    preset = "windows";
  }
  if (preset == "liquid-glass" || preset == "liquidglass") {
    preset = "liquid";
  }
  if (preset == "soft-ui" || preset == "softui") {
    preset = "soft";
  }
  if (preset == "minimalist") {
    preset = "minimal";
  }
  if (preset.empty() && RoboWebLower(RoboXMLAttr(node, "glass")) == "true") {
    preset = "glass";
  }
  data("preset", preset);
  std::string layout = RoboWebLower(RoboXMLAttr(node, "layout"));
  const std::string display = RoboWebLower(RoboXMLAttr(node, "display"));
  if (layout.empty() && (display == "overlay" || display == "layered" || display == "absolute")) {
    layout = display == "absolute" ? "absolute" : display;
  }
  data("layout", layout);
  data("shape", RoboWebLower(RoboXMLAttr(node, "shape")));
  std::string texture = RoboWebLower(RoboXMLAttr(node, "texture"));
  if (texture.empty()) {
    if (RoboWebLower(RoboXMLAttr(node, "noise")) == "true" || RoboWebLower(RoboXMLAttr(node, "grain")) == "true") {
      texture = "noise";
    } else if (RoboWebLower(RoboXMLAttr(node, "pattern")) == "true") {
      texture = "pattern";
    } else if (RoboWebLower(RoboXMLAttr(node, "mesh")) == "true") {
      texture = "mesh";
    }
  }
  data("texture", texture);
  data("display", RoboWebLower(RoboXMLAttr(node, "display")));
  data("accent", RoboWebLower(RoboWebContainerAttr(node, "accent", "decor")));
  data("fade", RoboWebLower(RoboXMLAttr(node, "fade")) == "true" ? "true" : "");
  data("chrome", RoboWebLower(RoboXMLAttr(node, "chrome")));
  data("density", RoboWebLower(RoboXMLAttr(node, "density")));
  data("theme", RoboWebLower(RoboXMLAttr(node, "theme")));
  data("disabled", RoboWebLower(RoboXMLAttr(node, "disabled")) == "true" ? "true" : "");
  data("selected", RoboWebLower(RoboXMLAttr(node, "selected")) == "true" ? "true" : "");
  std::string enter = RoboWebLower(RoboXMLAttr(node, "enter"));
  if (enter == "fade-in" || enter == "fadein") {
    enter = "fade";
  } else if (enter == "fade-up" || enter == "fadeup") {
    enter = "fade-up";
  } else if (enter == "fade-down" || enter == "fadedown") {
    enter = "fade-down";
  } else if (enter == "fade-left" || enter == "fadeleft") {
    enter = "fade-left";
  } else if (enter == "fade-right" || enter == "faderight") {
    enter = "fade-right";
  } else if (enter == "zoom-in" || enter == "zoomin" || enter == "zoom") {
    enter = "zoom";
  } else if (enter == "zoom-out" || enter == "zoomout") {
    enter = "zoom-out";
  } else if (enter == "slide" || enter == "slide-in" || enter == "slidein") {
    enter = "slide";
  } else if (enter == "flip" || enter == "flip-in") {
    enter = "flip";
  } else if (enter == "rotate" || enter == "rotate-in") {
    enter = "rotate";
  } else if (enter == "bounce" || enter == "bounce-in") {
    enter = "bounce";
  } else if (enter == "blur" || enter == "blur-in") {
    enter = "blur";
  }
  data("enter", enter);
  data("motion", RoboWebLower(RoboWebContainerAttr(node, "motion", "hoverMotion")));
  const std::string reel = RoboWebLower(RoboXMLAttr(node, "reel"));
  if (reel == "true" || reel == "infinite" || reel == "loop" || reel == "on") {
    data("reel", "infinite");
    data("reel-every", RoboXMLAttr(node, "reelEvery").empty() ? "4200" : RoboXMLAttr(node, "reelEvery"));
    data("reel-move", RoboXMLAttr(node, "reelMove").empty() ? "380" : RoboXMLAttr(node, "reelMove"));
    std::string ease = RoboXMLAttr(node, "reelEase");
    if (ease.empty()) ease = "0.28,0.11,0.32,1";
    if (ease.find("cubic-bezier") == std::string::npos) ease = "cubic-bezier(" + ease + ")";
    data("reel-ease", ease);
    const std::string drag = RoboWebLower(RoboXMLAttr(node, "reelDrag"));
    data("reel-drag", drag == "false" || drag == "0" || drag == "off" ? "false" : "true");
    const std::string focus = RoboWebLower(RoboXMLAttr(node, "reelFocus"));
    data("reel-focus", focus == "false" || focus == "0" || focus == "off" ? "false" : "true");
    data("reel-copies", RoboXMLAttr(node, "reelCopies").empty() ? "3" : RoboXMLAttr(node, "reelCopies"));
    data("reel-dots", RoboXMLAttr(node, "reelDots"));
  }
}

std::string RoboWebContainerVideo(const RoboXMLNode& node) {
  const std::string src = RoboXMLAttr(node, "backgroundVideo");
  if (src.empty()) {
    return {};
  }
  return "<video class=\"robo-container-video\" autoplay muted loop playsinline src=\"" + RoboWebEscape(src) +
         "\"></video>";
}

void RoboWebAppendContainerStyle(std::string& style, const RoboXMLNode& node, const RoboFoundationLibrary& library) {
  const std::string display = RoboWebLower(RoboXMLAttr(node, "display"));
  if (display == "block" || display == "inline" || display == "inline-flex" || display == "inline-block") {
    RoboWebAddDecl(style, "display", display);
  } else if (display == "horizontal") {
    RoboWebAddDecl(style, "display", "flex");
    RoboWebAddDecl(style, "flex-direction", "row");
  } else if (display == "vertical") {
    RoboWebAddDecl(style, "display", "flex");
    RoboWebAddDecl(style, "flex-direction", "column");
  } else if (display == "overlay" || display == "layered") {
    RoboWebAddDecl(style, "display", "grid");
  }

  const std::string widthMode = RoboWebLower(RoboWebContainerAttr(node, "widthMode", "widthBehavior"));
  if (widthMode == "full") {
    RoboWebAddDecl(style, "width", "100%");
  } else if (widthMode == "auto" || widthMode == "content") {
    RoboWebAddDecl(style, "width", "auto");
  } else if (widthMode == "fit" || widthMode == "fit-content") {
    RoboWebAddDecl(style, "width", "fit-content");
  } else if (widthMode == "min" || widthMode == "min-content") {
    RoboWebAddDecl(style, "width", "min-content");
  } else if (widthMode == "max" || widthMode == "max-content") {
    RoboWebAddDecl(style, "width", "max-content");
  } else if (widthMode == "viewport") {
    RoboWebAddDecl(style, "width", "100vw");
  }
  const std::string heightMode = RoboWebLower(RoboWebContainerAttr(node, "heightMode", "heightBehavior"));
  if (heightMode == "full") {
    RoboWebAddDecl(style, "height", "100%");
  } else if (heightMode == "auto" || heightMode == "content") {
    RoboWebAddDecl(style, "height", "auto");
  } else if (heightMode == "fit" || heightMode == "fit-content") {
    RoboWebAddDecl(style, "height", "fit-content");
  } else if (heightMode == "min" || heightMode == "min-content") {
    RoboWebAddDecl(style, "height", "min-content");
  } else if (heightMode == "max" || heightMode == "max-content") {
    RoboWebAddDecl(style, "height", "max-content");
  } else if (heightMode == "viewport") {
    RoboWebAddDecl(style, "height", "100vh");
  }
  const std::string sizing = RoboWebLower(RoboXMLAttr(node, "sizing"));
  if (sizing == "content" || sizing == "intrinsic") {
    RoboWebAddDecl(style, "width", "max-content");
    RoboWebAddDecl(style, "height", "max-content");
  } else if (sizing == "equal") {
    RoboWebAddDecl(style, "flex", "1 1 0");
  } else if (sizing == "fluid") {
    RoboWebAddDecl(style, "width", "100%");
    RoboWebAddDecl(style, "max-width", "100%");
  } else if (sizing == "proportional") {
    RoboWebAddDecl(style, "flex", "1 1 auto");
  }
  const std::string ratio = RoboXMLAttr(node, "aspectRatio");
  if (!ratio.empty()) {
    RoboWebAddDecl(style, "aspect-ratio", RoboWebContainerRatio(ratio));
  }

  if (RoboXMLAttr(node, "marginLeft").empty() && RoboXMLAttr(node, "marginRight").empty()) {
    RoboWebContainerAxis(style, library, "margin-left", "margin-right", RoboXMLAttr(node, "marginX"));
  }
  if (RoboXMLAttr(node, "marginTop").empty() && RoboXMLAttr(node, "marginBottom").empty()) {
    RoboWebContainerAxis(style, library, "margin-top", "margin-bottom", RoboXMLAttr(node, "marginY"));
  }
  if (RoboXMLAttr(node, "paddingLeft").empty() && RoboXMLAttr(node, "paddingRight").empty()) {
    RoboWebContainerAxis(style, library, "padding-left", "padding-right",
                         RoboWebContainerAttr(node, "paddingX", "paddingHorizontal"));
  }
  if (RoboXMLAttr(node, "paddingTop").empty() && RoboXMLAttr(node, "paddingBottom").empty()) {
    RoboWebContainerAxis(style, library, "padding-top", "padding-bottom",
                         RoboWebContainerAttr(node, "paddingY", "paddingVertical"));
  }
  if (RoboXMLAttr(node, "gap").empty()) {
    RoboWebAddDecl(style, "gap", RoboWebContainerLength(library, RoboWebContainerAttr(node, "spacing", "childGap")));
  }
  RoboWebAddDecl(style, "row-gap", RoboWebContainerLength(library, RoboXMLAttr(node, "rowGap")));
  RoboWebAddDecl(style, "column-gap",
                 RoboWebContainerLength(library, RoboWebContainerAttr(node, "columnGap", "colGap")));
  RoboWebAddDecl(style, "align-content", RoboWebAlign(RoboXMLAttr(node, "alignContent")));
  RoboWebAddDecl(style, "justify-items", RoboWebAlign(RoboXMLAttr(node, "justifyItems")));
  RoboWebAddDecl(style, "order", RoboXMLAttr(node, "order"));
  RoboWebAddDecl(style, "inset", RoboWebContainerLength(library, RoboXMLAttr(node, "inset")));
  const std::string columns = RoboXMLAttr(node, "columns");
  if (RoboWebIsNumber(columns)) {
    RoboWebAddDecl(style, "grid-template-columns", "repeat(" + columns + ",minmax(0,1fr))");
  }
  const std::string rows = RoboXMLAttr(node, "rows");
  if (RoboWebIsNumber(rows)) {
    RoboWebAddDecl(style, "grid-template-rows", "repeat(" + rows + ",minmax(0,1fr))");
  }
  RoboWebAddDecl(style, "grid-column", RoboXMLAttr(node, "gridColumn"));
  RoboWebAddDecl(style, "grid-row", RoboXMLAttr(node, "gridRow"));

  const std::string tag = RoboWebLower(node.name);
  const std::string place = tag == "screen" || tag == "window" || tag == "page" ? std::string() : RoboWebLower(RoboXMLAttr(node, "place"));
  if (place == "top" || place == "top-center" || place == "top-left" || place == "top-right" || place == "center" ||
      place == "center-left" || place == "center-right" || place == "bottom" || place == "bottom-center" ||
      place == "bottom-left" || place == "bottom-right" || place == "left" || place == "right") {
    const bool top = place.rfind("top", 0) == 0;
    const bool bottom = place.rfind("bottom", 0) == 0;
    const bool left = place.find("left") != std::string::npos || place == "left";
    const bool right = place.find("right") != std::string::npos || place == "right";
    RoboWebAddDecl(style, "align-items", top ? "flex-start" : (bottom ? "flex-end" : "center"));
    RoboWebAddDecl(style, "justify-content", left ? "flex-start" : (right ? "flex-end" : "center"));
  }
  if (RoboWebLower(RoboXMLAttr(node, "reverse")) == "true") {
    const std::string direction =
        RoboWebLower(RoboWebContainerAttr(node, "direction", "flexDirection"));
    RoboWebAddDecl(style, "flex-direction", direction == "row" ? "row-reverse" : "column-reverse");
  }

  const std::string from = RoboWebContainerColor(library, RoboXMLAttr(node, "gradientFrom"));
  const std::string to = RoboWebContainerColor(library, RoboXMLAttr(node, "gradientTo"));
  const std::string stops = RoboXMLAttr(node, "gradientStops");
  std::string gradient = RoboXMLAttr(node, "gradient");
  const std::string gradientKind = RoboWebLower(gradient);
  if (!from.empty() || !to.empty() || !stops.empty()) {
    const std::string angle = RoboXMLAttr(node, "gradientAngle").empty() ? "160deg" : RoboXMLAttr(node, "gradientAngle");
    const std::string body = !stops.empty() ? stops : (from + "," + (to.empty() ? from : to));
    if (gradientKind == "radial") {
      gradient = "radial-gradient(circle," + body + ")";
    } else if (gradientKind == "conic") {
      gradient = "conic-gradient(from " + angle + "," + body + ")";
    } else {
      gradient = "linear-gradient(" + angle + "," + body + ")";
    }
  }
  if (!gradient.empty() && gradientKind != "radial" && gradientKind != "conic" && gradientKind != "linear" &&
      gradient.find('(') == std::string::npos) {
    gradient.clear();
  }
  if (!gradient.empty() && gradient.find('(') != std::string::npos) {
    RoboWebAddDecl(style, "background", gradient);
  }
  const std::string image = RoboWebContainerImage(RoboXMLAttr(node, "backgroundImage"));
  if (!image.empty()) {
    RoboWebAddDecl(style, "background-image", image);
    RoboWebAddDecl(style, "background-size",
                   RoboXMLAttr(node, "backgroundSize").empty() ? "cover" : RoboXMLAttr(node, "backgroundSize"));
    RoboWebAddDecl(style, "background-position", RoboXMLAttr(node, "backgroundPosition").empty()
                                                     ? "center"
                                                     : RoboXMLAttr(node, "backgroundPosition"));
    RoboWebAddDecl(style, "background-repeat", RoboXMLAttr(node, "backgroundRepeat").empty()
                                                   ? "no-repeat"
                                                   : RoboXMLAttr(node, "backgroundRepeat"));
  }
  const std::string overlay = RoboWebContainerColor(library, RoboWebContainerAttr(node, "overlay", "overlayColor"));
  if (!overlay.empty()) {
    RoboWebAddDecl(style, "background-image", "linear-gradient(" + overlay + "," + overlay + ")" +
                                                  (image.empty() ? "" : "," + image));
  }
  const std::string blur = RoboWebContainerLength(library, RoboWebContainerAttr(node, "backdropBlur", "blur"));
  if (!blur.empty() && RoboWebLower(RoboXMLAttr(node, "blur")) != "true") {
    RoboWebAddDecl(style, "backdrop-filter", "blur(" + blur + ")");
  } else if (RoboWebLower(RoboXMLAttr(node, "glass")) == "true" || RoboWebLower(RoboXMLAttr(node, "frost")) == "true") {
    RoboWebAddDecl(style, "backdrop-filter", "blur(18px)");
  }

  const std::string borderStyle = RoboWebLower(RoboXMLAttr(node, "borderStyle"));
  if (!borderStyle.empty()) {
    RoboWebAddDecl(style, "border-style", borderStyle == "none" ? "none" : borderStyle);
  }
  RoboWebAddDecl(style, "border-top-width", RoboWebContainerLength(library, RoboXMLAttr(node, "borderTopWidth")));
  RoboWebAddDecl(style, "border-right-width", RoboWebContainerLength(library, RoboXMLAttr(node, "borderRightWidth")));
  RoboWebAddDecl(style, "border-bottom-width", RoboWebContainerLength(library, RoboXMLAttr(node, "borderBottomWidth")));
  RoboWebAddDecl(style, "border-left-width", RoboWebContainerLength(library, RoboXMLAttr(node, "borderLeftWidth")));
  RoboWebAddDecl(style, "border-top-color", RoboWebContainerColor(library, RoboXMLAttr(node, "borderTopColor")));
  RoboWebAddDecl(style, "border-right-color", RoboWebContainerColor(library, RoboXMLAttr(node, "borderRightColor")));
  RoboWebAddDecl(style, "border-bottom-color", RoboWebContainerColor(library, RoboXMLAttr(node, "borderBottomColor")));
  RoboWebAddDecl(style, "border-left-color", RoboWebContainerColor(library, RoboXMLAttr(node, "borderLeftColor")));
  if (borderStyle == "gradient" || !RoboXMLAttr(node, "borderGradient").empty()) {
    const std::string paint = RoboXMLAttr(node, "borderGradient").empty() ? gradient : RoboXMLAttr(node, "borderGradient");
    if (!paint.empty()) {
      RoboWebAddDecl(style, "border-style", "solid");
      RoboWebAddDecl(style, "border-image", paint + " 1");
    }
  }
  RoboWebAddDecl(style, "border-top-left-radius", RoboWebContainerLength(library, RoboXMLAttr(node, "radiusTopLeft")));
  RoboWebAddDecl(style, "border-top-right-radius", RoboWebContainerLength(library, RoboXMLAttr(node, "radiusTopRight")));
  RoboWebAddDecl(style, "border-bottom-right-radius",
                 RoboWebContainerLength(library, RoboXMLAttr(node, "radiusBottomRight")));
  RoboWebAddDecl(style, "border-bottom-left-radius",
                 RoboWebContainerLength(library, RoboXMLAttr(node, "radiusBottomLeft")));
  RoboWebAddDecl(style, "clip-path", RoboXMLAttr(node, "clipPath"));

  const std::string shadowName = RoboWebContainerAttr(node, "shadow", "depth");
  const std::string shadowX = RoboXMLAttr(node, "shadowX");
  std::string shadowCss;
  if (!RoboXMLAttr(node, "shadows").empty()) {
    shadowCss = RoboXMLAttr(node, "shadows");
  } else if (!shadowX.empty() || !RoboXMLAttr(node, "shadowY").empty() || !RoboXMLAttr(node, "shadowBlur").empty()) {
    const std::string color = RoboWebContainerColor(library, RoboXMLAttr(node, "shadowColor")).empty()
                                  ? std::string("rgba(0,0,0,.35)")
                                  : RoboWebContainerColor(library, RoboXMLAttr(node, "shadowColor"));
    const std::string inset = RoboWebLower(RoboXMLAttr(node, "shadowInset")) == "true" ? "inset " : "";
    shadowCss = inset + RoboWebContainerLength(library, shadowX.empty() ? "0" : shadowX) + " " +
                RoboWebContainerLength(library, RoboXMLAttr(node, "shadowY").empty() ? "8" : RoboXMLAttr(node, "shadowY")) +
                " " +
                RoboWebContainerLength(library, RoboXMLAttr(node, "shadowBlur").empty() ? "24"
                                                                                       : RoboXMLAttr(node, "shadowBlur")) +
                " " +
                RoboWebContainerLength(library, RoboXMLAttr(node, "shadowSpread").empty()
                                                   ? "0"
                                                   : RoboXMLAttr(node, "shadowSpread")) +
                " " + color;
  } else if (!shadowName.empty()) {
    shadowCss = RoboWebContainerShadow(shadowName);
  }
  const std::string glow = RoboWebContainerColor(library, RoboXMLAttr(node, "glow"));
  const std::string innerGlow = RoboWebContainerColor(library, RoboXMLAttr(node, "innerGlow"));
  if (!glow.empty()) {
    if (!shadowCss.empty()) {
      shadowCss += ",";
    }
    shadowCss += "0 0 24px " + glow;
  }
  if (!innerGlow.empty()) {
    if (!shadowCss.empty()) {
      shadowCss += ",";
    }
    shadowCss += "inset 0 0 18px " + innerGlow;
  }
  RoboWebAddDecl(style, "box-shadow", shadowCss);

  std::string filter;
  const auto addFilter = [&](const char* fn, const std::string& raw, bool length) {
    if (raw.empty()) {
      return;
    }
    if (!filter.empty()) {
      filter += " ";
    }
    filter += fn;
    filter += "(";
    filter += length ? RoboWebContainerLength(library, raw) : raw;
    filter += ")";
  };
  addFilter("brightness", RoboXMLAttr(node, "brightness"), false);
  addFilter("contrast", RoboXMLAttr(node, "contrast"), false);
  addFilter("saturate", RoboXMLAttr(node, "saturation"), false);
  addFilter("grayscale", RoboXMLAttr(node, "grayscale"), false);
  addFilter("sepia", RoboXMLAttr(node, "sepia"), false);
  addFilter("hue-rotate", RoboXMLAttr(node, "hue"), true);
  addFilter("invert", RoboXMLAttr(node, "invert"), false);
  addFilter("blur", RoboWebLower(RoboXMLAttr(node, "foregroundBlur")).empty() ? std::string()
                                                                             : RoboXMLAttr(node, "foregroundBlur"),
            true);
  if (!filter.empty()) {
    RoboWebAddDecl(style, "filter", filter);
  }
  RoboWebAddDecl(style, "mix-blend-mode", RoboWebLower(RoboWebContainerAttr(node, "blend", "blendMode")));
  RoboWebAddDecl(style, "--c-accent", RoboWebContainerColor(library, RoboXMLAttr(node, "accentColor")));
  RoboWebAddDecl(style, "--c-titlebar", RoboWebContainerColor(library, RoboXMLAttr(node, "titlebarColor")));

  RoboWebAddDecl(style, "--c-hover-bg",
                 RoboWebContainerColor(library, RoboWebContainerAttr(node, "hoverBackground", "hoverGradient")));
  RoboWebAddDecl(style, "--c-hover-border", RoboWebContainerColor(library, RoboXMLAttr(node, "hoverBorderColor")));
  RoboWebAddDecl(style, "--c-hover-border-width", RoboWebContainerLength(library, RoboXMLAttr(node, "hoverBorderWidth")));
  RoboWebAddDecl(style, "--c-hover-radius", RoboWebContainerLength(library, RoboXMLAttr(node, "hoverRadius")));
  if (!RoboXMLAttr(node, "hoverShadow").empty() || !RoboXMLAttr(node, "hoverGlow").empty()) {
    const std::string hoverGlow = RoboWebContainerColor(library, RoboXMLAttr(node, "hoverGlow"));
    RoboWebAddDecl(style, "--c-hover-shadow", !hoverGlow.empty() ? "0 0 24px " + hoverGlow
                                                                : RoboWebContainerShadow(RoboXMLAttr(node, "hoverShadow")));
  }
  RoboWebAddDecl(style, "--c-hover-opacity", RoboXMLAttr(node, "hoverOpacity"));
  RoboWebAddDecl(style, "--c-hover-color", RoboWebContainerColor(library, RoboXMLAttr(node, "hoverColor")));
  std::string hoverFilter;
  if (!RoboXMLAttr(node, "hoverBrightness").empty()) {
    hoverFilter += "brightness(" + RoboXMLAttr(node, "hoverBrightness") + ")";
  }
  if (!RoboXMLAttr(node, "hoverBlur").empty()) {
    if (!hoverFilter.empty()) {
      hoverFilter += " ";
    }
    hoverFilter += "blur(" + RoboWebContainerLength(library, RoboXMLAttr(node, "hoverBlur")) + ")";
  }
  RoboWebAddDecl(style, "--c-hover-filter", hoverFilter);
  std::string hoverTransform;
  if (!RoboXMLAttr(node, "hoverScale").empty()) {
    hoverTransform += "scale(" + RoboXMLAttr(node, "hoverScale") + ")";
  }
  if (!RoboXMLAttr(node, "hoverRotate").empty()) {
    if (!hoverTransform.empty()) {
      hoverTransform += " ";
    }
    hoverTransform += "rotate(" + RoboXMLAttr(node, "hoverRotate") + ")";
  }
  if (!RoboXMLAttr(node, "hoverX").empty() || !RoboXMLAttr(node, "hoverY").empty()) {
    if (!hoverTransform.empty()) {
      hoverTransform += " ";
    }
    hoverTransform += "translate(" + RoboWebContainerLength(library, RoboXMLAttr(node, "hoverX").empty()
                                                                          ? "0"
                                                                          : RoboXMLAttr(node, "hoverX")) +
                      "," +
                      RoboWebContainerLength(library, RoboXMLAttr(node, "hoverY").empty() ? "0"
                                                                                        : RoboXMLAttr(node, "hoverY")) +
                      ")";
  }
  RoboWebAddDecl(style, "--c-hover-transform", hoverTransform);

  RoboWebAddDecl(style, "--c-focus-border", RoboWebContainerColor(library, RoboXMLAttr(node, "focusBorderColor")));
  RoboWebAddDecl(style, "--c-focus-width", RoboWebContainerLength(library, RoboXMLAttr(node, "focusBorderWidth")));
  if (!RoboXMLAttr(node, "focusOutline").empty()) {
    RoboWebAddDecl(style, "--c-focus-outline",
                   "2px solid " + RoboWebContainerColor(library, RoboXMLAttr(node, "focusOutline")));
  }
  RoboWebAddDecl(style, "--c-focus-offset", RoboWebContainerLength(library, RoboXMLAttr(node, "focusOffset")));
  if (!RoboXMLAttr(node, "focusGlow").empty() || !RoboXMLAttr(node, "focusShadow").empty()) {
    const std::string focusGlow = RoboWebContainerColor(library, RoboXMLAttr(node, "focusGlow"));
    RoboWebAddDecl(style, "--c-focus-shadow", !focusGlow.empty() ? "0 0 0 3px " + focusGlow
                                                                : RoboWebContainerShadow(RoboXMLAttr(node, "focusShadow")));
  }
  RoboWebAddDecl(style, "--c-focus-bg", RoboWebContainerColor(library, RoboXMLAttr(node, "focusBackground")));
  RoboWebAddDecl(style, "--c-focus-opacity", RoboXMLAttr(node, "focusOpacity"));
  RoboWebAddDecl(style, "--c-active-bg", RoboWebContainerColor(library, RoboXMLAttr(node, "activeBackground")));
  RoboWebAddDecl(style, "--c-active-border", RoboWebContainerColor(library, RoboXMLAttr(node, "activeBorder")));
  if (!RoboXMLAttr(node, "activeShadow").empty()) {
    RoboWebAddDecl(style, "--c-active-shadow", RoboWebContainerShadow(RoboXMLAttr(node, "activeShadow")));
  }
  if (!RoboXMLAttr(node, "activeScale").empty()) {
    RoboWebAddDecl(style, "--c-active-transform", "scale(" + RoboXMLAttr(node, "activeScale") + ")");
  }
  RoboWebAddDecl(style, "--c-active-opacity", RoboXMLAttr(node, "activeOpacity"));

  const std::string enter = RoboWebLower(RoboXMLAttr(node, "enter"));
  if (!enter.empty()) {
    std::string frames = "robo-c-fade";
    if (enter.find("up") != std::string::npos) {
      frames = "robo-c-fade-up";
    } else if (enter.find("down") != std::string::npos) {
      frames = "robo-c-fade-down";
    } else if (enter.find("left") != std::string::npos) {
      frames = "robo-c-fade-left";
    } else if (enter.find("right") != std::string::npos) {
      frames = "robo-c-fade-right";
    } else if (enter.find("zoom-out") != std::string::npos || enter.find("zoomout") != std::string::npos) {
      frames = "robo-c-zoom-out";
    } else if (enter.find("zoom") != std::string::npos) {
      frames = "robo-c-zoom";
    } else if (enter.find("slide") != std::string::npos) {
      frames = "robo-c-slide";
    } else if (enter.find("flip") != std::string::npos) {
      frames = "robo-c-flip";
    } else if (enter.find("rotate") != std::string::npos) {
      frames = "robo-c-rotate";
    } else if (enter.find("bounce") != std::string::npos) {
      frames = "robo-c-bounce";
    } else if (enter.find("blur") != std::string::npos) {
      frames = "robo-c-blur";
    }
    RoboWebAddDecl(style, "--c-enter", frames);
  }
  RoboWebAddDecl(style, "--c-duration", RoboWebContainerTime(RoboWebContainerAttr(node, "duration", "speed")));
  RoboWebAddDecl(style, "--c-delay", RoboWebContainerTime(RoboXMLAttr(node, "delay")));
  RoboWebAddDecl(style, "--c-ease", RoboXMLAttr(node, "easing"));
  RoboWebAddDecl(style, "--c-iteration", RoboXMLAttr(node, "iteration"));
  if (!RoboXMLAttr(node, "duration").empty() || !RoboXMLAttr(node, "easing").empty()) {
    const std::string time = RoboWebContainerTime(RoboXMLAttr(node, "duration").empty() ? "200" : RoboXMLAttr(node, "duration"));
    const std::string ease = RoboXMLAttr(node, "easing").empty() ? "ease" : RoboXMLAttr(node, "easing");
    RoboWebAddDecl(style, "transition", "background " + time + " " + ease + ",border-color " + time + " " + ease +
                                            ",box-shadow " + time + " " + ease + ",transform " + time + " " + ease +
                                            ",opacity " + time + " " + ease + ",filter " + time + " " + ease);
  }

  const std::string bar = RoboWebContainerLength(library, RoboXMLAttr(node, "scrollbarWidth"));
  if (!bar.empty() || !RoboXMLAttr(node, "scrollbarThumb").empty() || !RoboXMLAttr(node, "scrollbarColor").empty() ||
      !RoboXMLAttr(node, "scrollbarTrack").empty() || !RoboXMLAttr(node, "scrollbarHover").empty()) {
    RoboWebAddDecl(style, "--c-scroll-size", bar.empty() ? "8px" : bar);
    const std::string thumb = RoboWebContainerColor(
        library, RoboWebContainerAttr(node, "scrollbarThumb", "scrollbarColor"));
    RoboWebAddDecl(style, "--c-scroll-thumb", thumb);
    RoboWebAddDecl(style, "--c-scroll-track", RoboWebContainerColor(library, RoboXMLAttr(node, "scrollbarTrack")));
    RoboWebAddDecl(style, "--c-scroll-hover", RoboWebContainerColor(library, RoboXMLAttr(node, "scrollbarHover")));
  }
  RoboWebAddDecl(style, "--c-media-opacity", RoboXMLAttr(node, "backgroundOpacity"));
  RoboWebAddDecl(style, "cursor", RoboXMLAttr(node, "cursor"));
  RoboWebAddDecl(style, "transform", RoboXMLAttr(node, "transform"));
  if (!RoboXMLAttr(node, "filter").empty()) {
    RoboWebAddDecl(style, "filter", RoboXMLAttr(node, "filter"));
  }
  if (!RoboXMLAttr(node, "transition").empty()) {
    RoboWebAddDecl(style, "transition", RoboXMLAttr(node, "transition"));
  }
}

std::string RoboWebReelScript() {
  return R"JS(Robo.reel = function () {
  var reduced = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  document.querySelectorAll("[data-reel]").forEach(function (track) {
    var stage = track.parentElement;
    if (!stage) return;
    var every = Number(track.getAttribute("data-reel-every")) || 4200;
    var move = Number(track.getAttribute("data-reel-move")) || 380;
    var ease = track.getAttribute("data-reel-ease") || "cubic-bezier(0.28,0.11,0.32,1)";
    var copies = Number(track.getAttribute("data-reel-copies")) || 3;
    var focus = track.getAttribute("data-reel-focus") !== "false";
    var dragOn = track.getAttribute("data-reel-drag") !== "false";
    var dots = track.getAttribute("data-reel-dots");
    var dotRoot = dots ? document.getElementById(dots) : null;
    var originals = Array.prototype.slice.call(track.children);
    var count = originals.length;
    if (count < 1 || copies < 2) return;
    for (var copy = 1; copy < copies; copy += 1) {
      originals.forEach(function (card) { track.appendChild(card.cloneNode(true)); });
    }
    var cards = Array.prototype.slice.call(track.children);
    var phys = count;
    var pos = 0;
    var moving = false;
    var hover = false;
    var timer = 0;
    var seen = true;
    track.style.willChange = "transform";
    function center(index) {
      var card = cards[index];
      if (!card) return pos;
      return stage.clientWidth / 2 - (card.offsetLeft + card.offsetWidth / 2);
    }
    function span() {
      var a = cards[count];
      var b = cards[count * 2];
      if (!a || !b) return 0;
      return b.offsetLeft - a.offsetLeft;
    }
    function apply(next, animate) {
      pos = next;
      track.style.transition = animate && !reduced ? "transform " + move + "ms " + ease : "none";
      track.style.transform = "translate3d(" + next + "px,0,0)";
    }
    function paintDots(active) {
      if (!dotRoot) return;
      Array.prototype.forEach.call(dotRoot.children, function (dot, index) {
        var on = index === active;
        dot.style.background = on ? "var(--brand-primary)" : "var(--border-default)";
        dot.style.transform = on ? "scale(1.35)" : "none";
      });
    }
    function paint() {
      var mid = stage.getBoundingClientRect().left + stage.clientWidth / 2;
      var best = 0;
      var bestDist = Infinity;
      cards.forEach(function (card, index) {
        var rect = card.getBoundingClientRect();
        var unit = card.offsetWidth + 20;
        var dist = (rect.left + rect.width / 2 - mid) / unit;
        var u = Math.min(1, Math.abs(dist));
        var fall = u * u * (3 - 2 * u);
        if (focus) {
          card.style.transform = "scale(" + (1 - 0.085 * fall) + ")";
          card.style.opacity = String(1 - 0.42 * fall);
        }
        if (Math.abs(dist) < bestDist) {
          bestDist = Math.abs(dist);
          best = index % count;
        }
      });
      paintDots(best);
    }
    function shift() {
      var width = span();
      if (!width) return;
      if (phys < count) {
        apply(pos - width, false);
        phys += count;
      } else if (phys >= count * 2) {
        apply(pos + width, false);
        phys -= count;
      }
    }
    function finish() {
      moving = false;
      shift();
      paint();
      arm();
    }
    function go(next) {
      if (moving) return;
      phys = next;
      var dest = center(next);
      if (reduced || Math.abs(dest - pos) < 1) {
        apply(dest, false);
        finish();
        return;
      }
      moving = true;
      apply(dest, true);
      var frame = function () {
        paint();
        if (moving) requestAnimationFrame(frame);
      };
      requestAnimationFrame(frame);
      var done = function (event) {
        if (event && event.target !== track) return;
        track.removeEventListener("transitionend", done);
        window.clearTimeout(fallback);
        finish();
      };
      var fallback = window.setTimeout(function () { done(); }, move + 80);
      track.addEventListener("transitionend", done);
    }
    function arm() {
      window.clearTimeout(timer);
      timer = 0;
      if (reduced || hover || !seen) return;
      timer = window.setTimeout(function () { go(phys + 1); }, every);
    }
    function layout() {
      apply(center(phys), false);
      paint();
    }
    layout();
    window.addEventListener("resize", layout);
    if ("IntersectionObserver" in window) {
      var observer = new IntersectionObserver(function (entries) {
        seen = entries.some(function (entry) { return entry.isIntersecting; });
        if (seen) arm();
        else window.clearTimeout(timer);
      }, { threshold: 0.2 });
      observer.observe(stage);
    }
    stage.addEventListener("pointerenter", function () { hover = true; window.clearTimeout(timer); });
    stage.addEventListener("pointerleave", function () { hover = false; arm(); });
    if (dragOn) {
      var drag = null;
      stage.addEventListener("pointerdown", function (event) {
        if (event.target.closest && event.target.closest("a,button")) return;
        drag = { x: event.clientX, y: event.clientY, moved: false };
        window.clearTimeout(timer);
      });
      stage.addEventListener("pointermove", function (event) {
        if (!drag || drag.moved) return;
        var dx = event.clientX - drag.x;
        var dy = event.clientY - drag.y;
        if (Math.abs(dx) < 8 && Math.abs(dy) < 8) return;
        if (Math.abs(dy) > Math.abs(dx)) { drag = null; arm(); return; }
        drag.moved = true;
      });
      var release = function (event) {
        if (!drag) return;
        var dx = event.clientX - drag.x;
        var stepped = drag.moved && Math.abs(dx) >= 48;
        drag = null;
        if (stepped && !moving) go(phys + (dx < 0 ? 1 : -1));
        else arm();
      };
      stage.addEventListener("pointerup", release);
      stage.addEventListener("pointercancel", function () { drag = null; arm(); });
    }
    arm();
  });
};
Robo.reel();
)JS";
}
