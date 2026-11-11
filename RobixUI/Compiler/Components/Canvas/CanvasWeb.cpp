#include "CanvasWeb.hpp"

#include "RoboCanvas.hpp"

#include <cstdio>
#include <sstream>

namespace {

std::string CanvasCssColor(RoboU32 color) {
    const int red = static_cast<int>((color >> 16) & 0xFFu);
    const int green = static_cast<int>((color >> 8) & 0xFFu);
    const int blue = static_cast<int>(color & 0xFFu);
    const int alpha = static_cast<int>((color >> 24) & 0xFFu);
    char text[48];
    if (alpha >= 255) {
        std::snprintf(text, sizeof(text), "#%02X%02X%02X", red, green, blue);
    } else {
        std::snprintf(text, sizeof(text), "rgba(%d,%d,%d,%.3f)", red, green, blue, static_cast<float>(alpha) / 255.0f);
    }
    return text;
}

std::string CanvasJsString(const std::string& value) {
    std::string out = "\"";
    for (unsigned char ch : value) {
        if (ch == '\\' || ch == '"') {
            out.push_back('\\');
            out.push_back(static_cast<char>(ch));
        } else if (ch == '\n') {
            out += "\\n";
        } else if (ch == '\r') {
            out += "\\r";
        } else if (ch < 32) {
            char code[8];
            std::snprintf(code, sizeof(code), "\\u%04X", ch);
            out += code;
        } else {
            out.push_back(static_cast<char>(ch));
        }
    }
    out.push_back('"');
    return out;
}

std::string CanvasFont(const RoboCanvasCommand& command) {
    const std::string family = command.family;
    std::string weight = "400";
    if (family.find("semibold") != std::string::npos || family.find("SemiBold") != std::string::npos) {
        weight = "600";
    } else if (family.find("bold") != std::string::npos || family.find("Bold") != std::string::npos) {
        weight = "700";
    } else if (family.find("medium") != std::string::npos || family.find("Medium") != std::string::npos) {
        weight = "500";
    } else if (family.find("italic") != std::string::npos) {
        weight = "italic 400";
    }
    std::string face = "Inter, sans-serif";
    if (!family.empty() && family.find("sans") == std::string::npos && family.find("Inter") == std::string::npos &&
        family.find("inter") == std::string::npos) {
        face = family + ", sans-serif";
    }
    return weight + " " + std::to_string(command.size) + "px " + face;
}

}  // namespace

bool RoboWebEmitCanvas(std::string& html, const RoboXMLNode& node) {
    RoboCanvasNode canvas;
    std::string diagnostic;
    if (!RoboCanvasRead(node, canvas, diagnostic)) {
        return false;
    }
    html += "<canvas class=\"robo-canvas\" width=\"";
    html += std::to_string(canvas.width);
    html += "\" height=\"";
    html += std::to_string(canvas.height);
    html += "\" style=\"width:";
    html += std::to_string(canvas.width);
    html += "px;aspect-ratio:";
    html += std::to_string(canvas.width);
    html += "/";
    html += std::to_string(canvas.height);
    html += "\">";
    html += "</canvas><script>(function(c){if(!c||!c.getContext)return;var g=c.getContext(\"2d\");";
    for (const RoboCanvasCommand& command : canvas.commands) {
        const std::string color = CanvasCssColor(command.color);
        if (command.kind == RoboCanvasKind::Rect) {
            html += "g.fillStyle=\"";
            html += color;
            html += "\";g.fillRect(";
            html += std::to_string(command.x);
            html += ",";
            html += std::to_string(command.y);
            html += ",";
            html += std::to_string(command.width);
            html += ",";
            html += std::to_string(command.height);
            html += ");";
        } else if (command.kind == RoboCanvasKind::Disc) {
            html += "g.fillStyle=\"";
            html += color;
            html += "\";g.beginPath();g.arc(";
            html += std::to_string(command.x);
            html += ",";
            html += std::to_string(command.y);
            html += ",";
            html += std::to_string(command.radius);
            html += ",0,Math.PI*2);g.fill();";
        } else if (command.kind == RoboCanvasKind::Line) {
            html += "g.strokeStyle=\"";
            html += color;
            html += "\";g.lineWidth=1;g.beginPath();g.moveTo(";
            html += std::to_string(command.x);
            html += "+0.5,";
            html += std::to_string(command.y);
            html += "+0.5);g.lineTo(";
            html += std::to_string(command.x2);
            html += "+0.5,";
            html += std::to_string(command.y2);
            html += "+0.5);g.stroke();";
        } else if (command.kind == RoboCanvasKind::Text) {
            html += "g.fillStyle=\"";
            html += color;
            html += "\";g.font=";
            html += CanvasJsString(CanvasFont(command));
            html += ";g.textBaseline=\"top\";g.fillText(";
            html += CanvasJsString(command.value);
            html += ",";
            html += std::to_string(command.x);
            html += ",";
            html += std::to_string(command.y);
            html += ");";
        }
    }
    html += "})(document.currentScript.previousElementSibling);</script>";
    return true;
}

void RoboWebAppendCanvasCss(std::ostream& css) {
    css << ".robo-canvas{display:block;max-width:100%;height:auto;background:transparent}\n";
}
