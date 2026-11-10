#include "BackgroundWeb.hpp"

namespace fs = std::filesystem;

void RoboWebEmitBackground(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                           const std::string& source, const fs::path& assetsRoot, RoboWebPackage* package, int depth) {
  RoboWebNoteFamily(package, "background");
  const std::string id = RoboXMLAttr(node, "id");
  std::string type = RoboWebLower(RoboXMLAttr(node, "type", "video"));
  if (type.empty()) {
    type = "video";
  }
  std::string src = RoboXMLAttr(node, "src");
  if (src.empty()) {
    src = RoboXMLAttr(node, "source");
  }
  if (src.empty() && type != "solid" && type != "gradient" && type != "slideshow") {
    src = "Assets/Background.mp4";
  }
  const std::string scale = RoboWebLower(RoboXMLAttr(node, "scale", "cover"));
  if (package != nullptr && !src.empty()) {
    src = RoboWebCopyAsset(*package, src, src);
  }
  html += "<div class=\"robo-background-slot";
  if (depth > 0) {
    html += " is-inline";
  }
  html += "\"";
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  html += " data-type=\"";
  html += RoboWebEscape(type);
  html += "\" data-scale=\"";
  html += RoboWebEscape(scale.empty() ? "cover" : scale);
  html += "\"";
  if (type == "slideshow") {
    if (package != nullptr) {
      package->used.insert("slideshow");
    }
    std::string duration = RoboXMLAttr(node, "duration");
    if (duration.empty()) {
      duration = RoboXMLAttr(node, "interval", RoboXMLAttr(node, "hold", "5000").c_str());
    }
    html += " data-duration=\"";
    html += RoboWebEscape(duration);
    html += "\"";
  }
  std::string style = RoboWebStyle(node, library);
  if (type == "solid") {
    const std::string color = RoboXMLAttr(node, "color", "#000000");
    RoboWebAddDecl(style, "background", RoboWebCssValue(library, color, true));
  } else if (type == "gradient") {
    const std::string from = RoboXMLAttr(node, "from", RoboXMLAttr(node, "gradientFrom", "#000000").c_str());
    const std::string to = RoboXMLAttr(node, "to", RoboXMLAttr(node, "gradientTo", "#000000").c_str());
    const bool vertical = RoboWebLower(RoboXMLAttr(node, "vertical", "true")) != "false";
    RoboWebAddDecl(style, "background",
                   std::string("linear-gradient(") + (vertical ? "to bottom, " : "to right, ") +
                       RoboWebCssValue(library, from, true) + ", " + RoboWebCssValue(library, to, true) + ")");
  }
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += ">";
  if (type == "video") {
    html += "<video class=\"robo-background\" autoplay muted loop playsinline src=\"";
    html += RoboWebEscape(src);
    html += "\"></video>";
  } else if (type == "image" || type == "svg") {
    html += "<img class=\"robo-background\" alt=\"\" src=\"";
    html += RoboWebEscape(src);
    html += "\">";
  } else if (type == "slideshow") {
    bool first = true;
    for (const RoboXMLNode& child : node.children) {
      const std::string childName = RoboWebLower(child.name);
      if (childName != "picture" && childName != "slide") {
        continue;
      }
      std::string picture = RoboXMLAttr(child, "src");
      if (picture.empty()) {
        picture = RoboXMLAttr(child, "source");
      }
      if (package != nullptr && !picture.empty()) {
        picture = RoboWebCopyAsset(*package, picture, picture);
      }
      html += "<img class=\"robo-background";
      if (first) {
        html += " is-on";
        first = false;
      }
      html += "\" alt=\"\" src=\"";
      html += RoboWebEscape(picture);
      html += "\">";
    }
  }
  bool ignored = false;
  for (const RoboXMLNode& child : node.children) {
    const std::string childName = RoboWebLower(child.name);
    if (childName == "picture" || childName == "slide") {
      continue;
    }
    RoboWebEmit(html, child, library, source, assetsRoot, false, ignored, package, depth + 1);
  }
  html += "</div>";
}

void RoboWebAppendBackgroundCss(std::ostream& css) {
  css << ".robo-background-slot{position:fixed;inset:0;z-index:0;overflow:hidden;pointer-events:none;background:var(--bg-primary)}\n";
  css << ".robo-background-slot video,.robo-background-slot img.robo-background{position:absolute;inset:0;width:100%;height:100%;object-fit:cover}\n";
  css << ".robo-background-slot[data-scale=\"fit\"] video,.robo-background-slot[data-scale=\"fit\"] img{object-fit:contain}\n";
  css << ".robo-background-slot[data-scale=\"fill\"] video,.robo-background-slot[data-scale=\"fill\"] img,"
         ".robo-background-slot[data-scale=\"stretch\"] video,.robo-background-slot[data-scale=\"stretch\"] img{object-fit:fill}\n";
  css << ".robo-background-slot[data-scale=\"tile\"] img{object-fit:none;width:auto;height:auto}\n";
  css << ".robo-background-slot[data-type=\"slideshow\"] img{opacity:0}\n";
  css << ".robo-background-slot[data-type=\"slideshow\"] img.is-on{opacity:1}\n";
  css << ".robo-background-slot.is-inline{position:relative;inset:auto;width:100%;height:180px;z-index:auto;pointer-events:auto}\n";
}

std::string RoboWebSlideshowScript() {
  return R"JS(
  (function () {
    var previous = Robo.mount;
    Robo.mount = function () {
      if (previous) previous();
  var shows = document.querySelectorAll(".robo-background-slot[data-type='slideshow']");
  for (var s = 0; s < shows.length; s++) {
    (function (slot) {
      var frames = slot.querySelectorAll("img");
      if (frames.length < 2) return;
      var hold = parseInt(slot.getAttribute("data-duration") || "5000", 10);
      var frame = 0;
      setInterval(function () {
        frames[frame].classList.remove("is-on");
        frame = (frame + 1) % frames.length;
        frames[frame].classList.add("is-on");
      }, hold > 0 ? hold : 5000);
    })(shows[s]);
  }
    };
  })();
)JS";
}

const char* RoboWebBackgroundClass() { return "robo-background"; }
