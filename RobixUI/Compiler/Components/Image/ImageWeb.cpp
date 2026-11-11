#include "ImageWeb.hpp"

#include "ImageResize.hpp"
#include "../../Motion.hpp"
#include "../../ScreenSize.hpp"
#include "../Container/ContainerWeb.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path g_documentDir;
fs::path g_outRoot;
int g_copy = 0;
bool g_compile = false;
bool g_defer = false;
std::vector<std::string> g_stems;

std::string Lower(std::string text) { return RoboWebLower(std::move(text)); }

std::string Attr(const RoboXMLNode& node, const char* key, const char* alt = nullptr) {
  const std::string value = RoboXMLAttr(node, key);
  if (!value.empty() || alt == nullptr) return value;
  return RoboXMLAttr(node, alt);
}

bool Remote(const std::string& source) {
  const std::string key = Lower(source);
  return key.rfind("http://", 0) == 0 || key.rfind("https://", 0) == 0 || key.rfind("data:", 0) == 0 ||
         key.rfind("blob:", 0) == 0 || key.rfind("//", 0) == 0;
}

struct Published {
  std::string src;
  std::string srcset;
};

bool ResizeOff(const RoboXMLNode& node) {
  const std::string value = Lower(RoboXMLAttr(node, "resize"));
  return value == "false" || value == "0" || value == "no" || value == "off" || value == "none";
}

Published Publish(const RoboXMLNode& node) {
  const std::string source = Attr(node, "source", "src");
  if (source.empty() || Remote(source) || g_outRoot.empty()) return {source, {}};
  fs::path file(source);
  if (file.is_relative() && !g_documentDir.empty()) file = g_documentDir / file;
  std::error_code error;
  if (!fs::is_regular_file(file, error)) {
    std::cerr << "warning: image '" << source << "' was not found and was left as written.\n";
    return {source, {}};
  }
  std::string stem = file.stem().string();
  for (const std::string& used : g_stems) {
    if (used == stem) {
      stem = std::to_string(++g_copy) + "-" + stem;
      break;
    }
  }
  g_stems.push_back(stem);
  const fs::path folder = g_outRoot / "images";
  const bool resize = g_compile && !ResizeOff(node);
  const std::vector<RoboImageVariant> files = RoboImageWriteSizes(file, folder, stem, resize);
  if (files.empty()) {
    std::cerr << "warning: image '" << source << "' could not be copied and was left as written.\n";
    return {source, {}};
  }
  if (!resize || files.size() < 2) return {files.back().url, {}};
  std::string chosen = files.front().url;
  for (const RoboImageVariant& item : files) {
    if (item.width > 0 && item.width <= 960) chosen = item.url;
  }
  std::ostringstream set;
  for (std::size_t i = 0; i < files.size(); ++i) {
    if (i != 0) set << ", ";
    set << files[i].url;
    if (files[i].width > 0) set << " " << files[i].width << "w";
  }
  return {chosen, set.str()};
}

std::string Fit(const std::string& raw) {
  const std::string key = Lower(raw);
  if (key.empty()) return {};
  if (key == "contain" || key == "cover" || key == "fill" || key == "none" || key == "scale-down") return key;
  if (key == "scaledown" || key == "scale_down") return "scale-down";
  return raw;
}

std::string Position(const std::string& raw) {
  const std::string key = Lower(raw);
  if (key.empty()) return {};
  if (key == "center" || key == "top" || key == "bottom" || key == "left" || key == "right") return key;
  if (key == "top-left" || key == "topleft") return "top left";
  if (key == "top-right" || key == "topright") return "top right";
  if (key == "bottom-left" || key == "bottomleft") return "bottom left";
  if (key == "bottom-right" || key == "bottomright") return "bottom right";
  return raw;
}

std::string Ratio(std::string raw) {
  const std::string key = Lower(raw);
  if (key == "1:1" || key == "square") return "1 / 1";
  if (key == "4:3") return "4 / 3";
  if (key == "3:2") return "3 / 2";
  if (key == "16:9") return "16 / 9";
  if (key == "21:9") return "21 / 9";
  if (key == "9:16") return "9 / 16";
  for (char& ch : raw) {
    if (ch == ':') ch = '/';
  }
  return raw;
}

std::string Radius(const RoboFoundationLibrary& library, const std::string& raw) {
  const std::string key = Lower(raw);
  if (key.empty()) return {};
  if (key == "none" || key == "square") return "0";
  if (key == "xs") return "4px";
  if (key == "sm" || key == "small") return "8px";
  if (key == "md" || key == "medium") return "12px";
  if (key == "lg" || key == "large") return "16px";
  if (key == "xl") return "24px";
  if (key == "full" || key == "pill") return "999px";
  if (key == "circle") return "50%";
  return RoboWebCssValue(library, raw, false);
}

std::string Filter(const RoboXMLNode& node, const char* prefix) {
  std::string filter;
  const auto add = [&](const char* name, const char* function, const char* unit) {
    const std::string key = std::string(prefix) + name;
    const std::string value = RoboXMLAttr(node, key.c_str());
    if (value.empty()) return;
    if (!filter.empty()) filter += " ";
    filter += function;
    filter += "(";
    filter += value;
    if (value.find(unit) == std::string::npos && unit[0] != '\0') filter += unit;
    filter += ")";
  };
  add("Brightness", "brightness", "");
  add("Contrast", "contrast", "");
  add("Saturation", "saturate", "");
  add("Grayscale", "grayscale", "");
  add("Sepia", "sepia", "");
  add("Hue", "hue-rotate", "deg");
  add("Blur", "blur", "px");
  add("Invert", "invert", "");
  const std::string named = RoboXMLAttr(node, prefix[0] == '\0' ? "filter" : "hoverFilter");
  if (!named.empty()) {
    if (!filter.empty()) filter += " ";
    filter += named;
  }
  return filter;
}

void Data(std::string& html, const char* key, const std::string& value) {
  if (value.empty()) return;
  html += " data-";
  html += key;
  html += "=\"";
  html += RoboWebEscape(value);
  html += "\"";
}

std::string Route(const std::string& path) {
  const std::string key = Lower(path);
  if (key.rfind("http://", 0) == 0 || key.rfind("https://", 0) == 0 || key.rfind("mailto:", 0) == 0 ||
      key.rfind("tel:", 0) == 0 || key.rfind("#", 0) == 0) {
    return path;
  }
  if (!path.empty() && path[0] == '/') return "#" + path;
  return "#/" + path;
}

}  // namespace

void RoboWebImageReset(const fs::path& document, const fs::path& outRoot, bool compile) {
  g_documentDir = document.empty() ? fs::path() : document.parent_path();
  g_outRoot = outRoot;
  g_copy = 0;
  g_compile = compile;
  g_defer = false;
  g_stems.clear();
}

void RoboWebAppendImageStyle(std::string& style, const RoboXMLNode& node, const RoboFoundationLibrary& library) {
  const std::string fit = Fit(Attr(node, "fit", "objectFit"));
  if (!fit.empty()) RoboWebAddDecl(style, "--img-fit", fit);
  const std::string anchor = Position(RoboXMLAttr(node, "objectPosition"));
  if (!anchor.empty()) RoboWebAddDecl(style, "--img-pos", anchor);
  const std::string ratio = Attr(node, "aspectRatio", "ratio");
  if (!ratio.empty()) RoboWebAddDecl(style, "aspect-ratio", Ratio(ratio));
  const std::string shape = Lower(RoboXMLAttr(node, "shape"));
  std::string radius = Radius(library, Attr(node, "radius", "borderRadius"));
  if (radius.empty()) {
    if (shape == "circle") radius = "50%";
    else if (shape == "pill" || shape == "capsule") radius = "999px";
    else if (shape == "rounded") radius = "12px";
    else if (shape == "squircle") radius = "28%";
    else if (shape == "square") radius = "0";
  }
  if (!radius.empty()) {
    RoboWebAddDecl(style, "border-radius", radius);
    RoboWebAddDecl(style, "--img-radius", radius);
    RoboWebAddDecl(style, "overflow", "hidden");
  }
  const auto corner = [&](const char* attrName, const char* cssName, const char* variable) {
    std::string value = Radius(library, RoboXMLAttr(node, attrName));
    if (value.empty()) return;
    RoboWebAddDecl(style, cssName, value);
    RoboWebAddDecl(style, variable, value);
    RoboWebAddDecl(style, "overflow", "hidden");
  };
  corner("radiusTopLeft", "border-top-left-radius", "--img-tl");
  corner("radiusTopRight", "border-top-right-radius", "--img-tr");
  corner("radiusBottomRight", "border-bottom-right-radius", "--img-br");
  corner("radiusBottomLeft", "border-bottom-left-radius", "--img-bl");
  const std::string filter = Filter(node, "");
  if (!filter.empty()) RoboWebAddDecl(style, "--img-filter", filter);
  const std::string hoverFilter = Filter(node, "hover");
  if (!hoverFilter.empty()) RoboWebAddDecl(style, "--img-hover-filter", hoverFilter);
  const std::string hoverRadius = Radius(library, RoboXMLAttr(node, "hoverRadius"));
  if (!hoverRadius.empty()) RoboWebAddDecl(style, "--c-hover-radius", hoverRadius);
  if (Lower(RoboXMLAttr(node, "disabled")) == "true") RoboWebAddDecl(style, "opacity", "0.55");
}

void RoboWebEmitImage(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library) {
  const Published published = Publish(node);
  const std::string source = published.src;
  std::string srcset = published.srcset;
  const std::string authoredSet = Attr(node, "srcSet", "srcset");
  if (!authoredSet.empty()) srcset = authoredSet;
  const std::string alt = Attr(node, "alt", "description");
  const std::string caption = Attr(node, "caption", "label");
  const std::string path = Attr(node, "path", "href");
  std::string frame = RoboWebStyle(node, library);
  RoboWebAppendImageStyle(frame, node, library);
  html += "<figure class=\"";
  html += RoboWebImageClass();
  html += "\"";
  const std::string id = RoboXMLAttr(node, "id");
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  Data(html, "shape", Lower(RoboXMLAttr(node, "shape")));
  Data(html, "fit", Fit(Attr(node, "fit", "objectFit")));
  if (!Attr(node, "radius", "borderRadius").empty() || !RoboXMLAttr(node, "shape").empty() ||
      !RoboXMLAttr(node, "radiusTopLeft").empty() || !RoboXMLAttr(node, "radiusTopRight").empty() ||
      !RoboXMLAttr(node, "radiusBottomRight").empty() || !RoboXMLAttr(node, "radiusBottomLeft").empty()) {
    Data(html, "radius", "1");
  }
  if (!RoboXMLAttr(node, "height").empty() || !Attr(node, "aspectRatio", "ratio").empty() ||
      !RoboXMLAttr(node, "minHeight").empty()) {
    Data(html, "framed", "1");
  }
  if (!RoboXMLAttr(node, "hoverScale").empty()) Data(html, "hover-scale", "1");
  if (!RoboXMLAttr(node, "hoverOpacity").empty()) Data(html, "hover-opacity", "1");
  if (!RoboXMLAttr(node, "hoverShadow").empty()) Data(html, "hover-shadow", "1");
  if (!Filter(node, "hover").empty()) Data(html, "hover-filter", "1");
  if (Lower(RoboXMLAttr(node, "disabled")) == "true") Data(html, "disabled", "true");
  html += RoboWebScreenAttrs(node, library, "block");
  html += RoboWebMotionAttrs(node);
  if (!frame.empty()) {
    html += " style=\"";
    html += RoboWebEscape(frame);
    html += "\"";
  }
  html += ">";
  if (!path.empty()) {
    html += "<a class=\"robo-image-link\" href=\"";
    html += RoboWebEscape(Route(path));
    html += "\">";
  }
  html += "<img";
  std::string load = Lower(Attr(node, "load", "when"));
  if (load == "with" || load == "eager" || load == "immediate" || load == "now") load = "content";
  if (load == "after" || load == "defer" || load == "idle" || load == "complete" || load == "loaded") load = "ready";
  if (load.empty()) {
    const std::string loading = Lower(RoboXMLAttr(node, "loading"));
    if (loading == "lazy") load = "lazy";
    else load = "content";
  }
  const bool ready = load == "ready";
  if (ready) g_defer = true;
  if (!source.empty()) {
    html += ready ? " data-src=\"" : " src=\"";
    html += RoboWebEscape(source);
    html += "\"";
  }
  html += " alt=\"";
  html += RoboWebEscape(alt);
  html += "\"";
  const std::string title = RoboXMLAttr(node, "title");
  if (!title.empty()) {
    html += " title=\"";
    html += RoboWebEscape(title);
    html += "\"";
  }
  const std::string srcSet = srcset;
  if (!srcSet.empty()) {
    html += ready ? " data-srcset=\"" : " srcset=\"";
    html += RoboWebEscape(srcSet);
    html += "\"";
  }
  std::string sizes = RoboXMLAttr(node, "sizes");
  if (sizes.empty() && !srcSet.empty()) {
    const std::string width = RoboXMLAttr(node, "width");
    bool digits = !width.empty();
    for (char ch : width) {
      if (ch < '0' || ch > '9') digits = false;
    }
    sizes = digits ? width + "px" : "100vw";
  }
  if (!sizes.empty()) {
    html += " sizes=\"";
    html += RoboWebEscape(sizes);
    html += "\"";
  }
  std::string loading = Lower(RoboXMLAttr(node, "loading"));
  if (ready) loading = "eager";
  else if (load == "lazy") loading = "lazy";
  else if (load == "content") loading = "eager";
  else if (loading.empty()) loading = "eager";
  if (loading == "lazy" || loading == "eager" || loading == "auto") {
    html += " loading=\"";
    html += loading;
    html += "\"";
  }
  std::string decoding = Lower(RoboXMLAttr(node, "decoding"));
  if (decoding.empty()) decoding = "async";
  if (decoding == "async" || decoding == "sync" || decoding == "auto") {
    html += " decoding=\"";
    html += decoding;
    html += "\"";
  }
  const std::string priority = Lower(Attr(node, "fetchPriority", "priority"));
  if (priority == "high" || priority == "low" || priority == "auto") {
    html += " fetchpriority=\"";
    html += priority;
    html += "\"";
  }
  const std::string cross = Lower(Attr(node, "crossOrigin", "crossorigin"));
  if (cross == "anonymous" || cross == "use-credentials") {
    html += " crossorigin=\"";
    html += cross;
    html += "\"";
  }
  const std::string referrer = RoboXMLAttr(node, "referrerPolicy");
  if (!referrer.empty()) {
    html += " referrerpolicy=\"";
    html += RoboWebEscape(referrer);
    html += "\"";
  }
  const std::string drag = Lower(RoboXMLAttr(node, "draggable"));
  if (drag == "true" || drag == "false") {
    html += " draggable=\"";
    html += drag;
    html += "\"";
  }
  html += ">";
  if (!path.empty()) html += "</a>";
  if (!caption.empty()) {
    html += "<figcaption class=\"robo-image-caption\">";
    html += RoboWebEscape(caption);
    html += "</figcaption>";
  }
  html += "</figure>";
}

void RoboWebAppendImageCss(std::ostream& css) {
  css << ".robo-image{box-sizing:border-box;display:block;margin:0;max-width:100%;position:relative}\n";
  css << ".robo-image[data-radius]{overflow:hidden}\n";
  css << ".robo-image img{display:block;width:100%;height:auto;max-width:100%;object-fit:var(--img-fit,contain);"
         "object-position:var(--img-pos,center);filter:var(--img-filter,none);"
         "border-radius:var(--img-radius,inherit);"
         "border-top-left-radius:var(--img-tl,var(--img-radius,inherit));"
         "border-top-right-radius:var(--img-tr,var(--img-radius,inherit));"
         "border-bottom-right-radius:var(--img-br,var(--img-radius,inherit));"
         "border-bottom-left-radius:var(--img-bl,var(--img-radius,inherit))}\n";
  css << ".robo-image[data-framed] img{height:100%}\n";
  css << ".robo-image-link{display:block;height:100%;color:inherit;text-decoration:none;border-radius:inherit}\n";
  css << ".robo-image-caption{margin-top:8px;font-size:12px;line-height:1.33;color:var(--text-tertiary);text-align:center}\n";
  css << ".robo-image[style*=\"--c-hover-radius\"]:hover,.robo-image[style*=\"--c-hover-radius\"]:hover img{"
         "border-radius:var(--c-hover-radius);border-top-left-radius:var(--c-hover-radius);"
         "border-top-right-radius:var(--c-hover-radius);border-bottom-right-radius:var(--c-hover-radius);"
         "border-bottom-left-radius:var(--c-hover-radius)}\n";
  css << ".robo-image[style*=\"--c-hover-shadow\"]:hover{box-shadow:var(--c-hover-shadow)}\n";
  css << ".robo-image[style*=\"--c-hover-border\"]:hover{border-color:var(--c-hover-border)}\n";
  css << ".robo-image[style*=\"--c-hover-opacity\"]:hover{opacity:var(--c-hover-opacity)}\n";
  css << ".robo-image[style*=\"--c-hover-filter\"]:hover img,.robo-image[data-hover-filter]:hover img{filter:var(--c-hover-filter,var(--img-hover-filter))}\n";
  css << ".robo-image[style*=\"--c-hover-transform\"]:hover{transform:var(--c-hover-transform)}\n";
  css << ".robo-image[data-disabled=true]{pointer-events:none}\n";
  css << ".robo-image img{transition:transform 180ms ease,filter 180ms ease,opacity 180ms ease,border-radius 180ms ease}\n";
}

const char* RoboWebImageClass() { return "robo-image"; }

std::string RoboWebImageScript() {
  if (!g_defer) return {};
  return R"JS(window.addEventListener("load",function(){document.querySelectorAll("img[data-src]").forEach(function(img){var set=img.getAttribute("data-srcset");if(set)img.srcset=set;img.src=img.getAttribute("data-src");});});
)JS";
}
