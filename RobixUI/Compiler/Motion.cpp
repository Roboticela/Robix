#include "Motion.hpp"

#include <iostream>
#include <sstream>

namespace {

int g_next = 0;
bool g_used = false;
std::ostringstream g_css;

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

std::string Attr(const RoboXMLNode& node, const char* key) {
    for (const auto& entry : node.attrs) {
        if (Lower(entry.first) == key) return entry.second;
    }
    return {};
}

bool KnownEnter(const std::string& name) {
    static const char* names[] = {
        "fade", "fade-up", "fade-down", "fade-left", "fade-right",
        "slide-up", "slide-down", "slide-left", "slide-right",
        "zoom-in", "zoom-out", "pop", "grow", "shrink",
        "flip-x", "flip-y", "rotate-in", "spin", "pulse", "bounce",
        "shake", "swing", "float", "wobble", "blur-in", "rise", "drop",
        "expand", "heartbeat", "flash", "jello", "rubber", "tada", "ping"};
    for (const char* item : names) {
        if (name == item) return true;
    }
    return false;
}

bool KnownGesture(const std::string& name) {
    static const char* names[] = {"lift", "press", "grow", "shrink", "tilt", "glow", "pulse", "spin", "shake", "none"};
    for (const char* item : names) {
        if (name == item) return true;
    }
    return false;
}

bool Looping(const std::string& name) {
    return name == "spin" || name == "pulse" || name == "bounce" || name == "float" || name == "heartbeat" ||
           name == "flash" || name == "ping";
}

bool IsBag(const std::string& text) {
    return text.find(':') != std::string::npos;
}

struct Pose {
    bool opacity = false;
    bool x = false;
    bool y = false;
    bool z = false;
    bool scale = false;
    bool scaleX = false;
    bool scaleY = false;
    bool rotate = false;
    bool rotateX = false;
    bool rotateY = false;
    bool skewX = false;
    bool skewY = false;
    bool blur = false;
    double opacityValue = 1;
    double xValue = 0;
    double yValue = 0;
    double zValue = 0;
    double scaleValue = 1;
    double scaleXValue = 1;
    double scaleYValue = 1;
    double rotateValue = 0;
    double rotateXValue = 0;
    double rotateYValue = 0;
    double skewXValue = 0;
    double skewYValue = 0;
    double blurValue = 0;
    bool any = false;
};

bool Number(const std::string& text, double& value) {
    if (text.empty()) return false;
    std::size_t index = 0;
    int sign = 1;
    if (text[index] == '-') {
        sign = -1;
        ++index;
    }
    if (index >= text.size() || text[index] < '0' || text[index] > '9') {
        if (index >= text.size() || text[index] != '.') return false;
    }
    double whole = 0;
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
        whole = whole * 10 + (text[index] - '0');
        ++index;
    }
    double fraction = 0;
    double place = 0.1;
    if (index < text.size() && text[index] == '.') {
        ++index;
        if (index >= text.size()) return false;
        while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
            fraction += (text[index] - '0') * place;
            place *= 0.1;
            ++index;
        }
    }
    if (index != text.size()) return false;
    value = sign * (whole + fraction);
    return true;
}

std::string Num(double value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

bool ReadPose(const std::string& text, Pose& pose) {
    std::string token;
    bool ok = false;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const char ch = i == text.size() ? ' ' : text[i];
        if (ch == ' ' || ch == '\t' || ch == ',' || ch == ';') {
            if (!token.empty()) {
                const auto colon = token.find(':');
                if (colon == std::string::npos || colon == 0 || colon + 1 >= token.size()) {
                    std::cerr << "warning: motion value '" << token << "' was not recognized and was skipped.\n";
                } else {
                    const std::string key = Lower(token.substr(0, colon));
                    double value = 0;
                    if (!Number(token.substr(colon + 1), value)) {
                        std::cerr << "warning: motion value '" << token << "' was not recognized and was skipped.\n";
                    } else {
                        ok = true;
                        pose.any = true;
                        if (key == "opacity") { pose.opacity = true; pose.opacityValue = value; }
                        else if (key == "x") { pose.x = true; pose.xValue = value; }
                        else if (key == "y") { pose.y = true; pose.yValue = value; }
                        else if (key == "z") { pose.z = true; pose.zValue = value; }
                        else if (key == "scale") { pose.scale = true; pose.scaleValue = value; }
                        else if (key == "scalex") { pose.scaleX = true; pose.scaleXValue = value; }
                        else if (key == "scaley") { pose.scaleY = true; pose.scaleYValue = value; }
                        else if (key == "rotate") { pose.rotate = true; pose.rotateValue = value; }
                        else if (key == "rotatex") { pose.rotateX = true; pose.rotateXValue = value; }
                        else if (key == "rotatey") { pose.rotateY = true; pose.rotateYValue = value; }
                        else if (key == "skewx") { pose.skewX = true; pose.skewXValue = value; }
                        else if (key == "skewy") { pose.skewY = true; pose.skewYValue = value; }
                        else if (key == "blur") { pose.blur = true; pose.blurValue = value; }
                        else {
                            ok = false;
                            std::cerr << "warning: motion property '" << key << "' was not recognized and was skipped.\n";
                        }
                    }
                }
            }
            token.clear();
        } else {
            token.push_back(ch);
        }
    }
    return ok && pose.any;
}

std::string TransformOf(const Pose& pose, bool fromIdentity) {
    const auto pick = [&](bool present, double value, double identity) {
        return present ? value : (fromIdentity ? identity : value);
    };
    const bool move = pose.x || pose.y || pose.z;
    const bool turn = pose.rotate || pose.rotateX || pose.rotateY || pose.skewX || pose.skewY;
    const bool sized = pose.scale || pose.scaleX || pose.scaleY;
    if (!move && !turn && !sized) return {};
    std::string text;
    if (move) {
        text += "translate3d(" + Num(pick(pose.x, pose.xValue, 0)) + "px," + Num(pick(pose.y, pose.yValue, 0)) + "px," +
                Num(pick(pose.z, pose.zValue, 0)) + "px) ";
    }
    if (pose.rotateX || (!fromIdentity && pose.rotateX)) {
        text += "rotateX(" + Num(pick(pose.rotateX, pose.rotateXValue, 0)) + "deg) ";
    }
    if (pose.rotateY) text += "rotateY(" + Num(pick(pose.rotateY, pose.rotateYValue, 0)) + "deg) ";
    if (pose.rotate) text += "rotate(" + Num(pick(pose.rotate, pose.rotateValue, 0)) + "deg) ";
    if (pose.skewX) text += "skewX(" + Num(pick(pose.skewX, pose.skewXValue, 0)) + "deg) ";
    if (pose.skewY) text += "skewY(" + Num(pick(pose.skewY, pose.skewYValue, 0)) + "deg) ";
    if (sized) {
        if (pose.scaleX || pose.scaleY) {
            text += "scale(" + Num(pick(pose.scaleX, pose.scaleXValue, 1)) + "," + Num(pick(pose.scaleY, pose.scaleYValue, 1)) + ") ";
        } else {
            text += "scale(" + Num(pick(pose.scale, pose.scaleValue, 1)) + ") ";
        }
    }
    if (!text.empty() && text.back() == ' ') text.pop_back();
    return text;
}

std::string Decl(const Pose& pose) {
    std::string text;
    if (pose.opacity) text += "opacity:" + Num(pose.opacityValue) + ";";
    const std::string transform = TransformOf(pose, false);
    if (!transform.empty()) text += "transform:" + transform + ";";
    if (pose.blur) text += "filter:blur(" + Num(pose.blurValue) + "px);";
    return text;
}

Pose Masked(const Pose& pose, const Pose& mask) {
    Pose out = pose;
    const auto fill = [](bool maskOn, bool& on, double& value, double identity) {
        if (maskOn && !on) {
            on = true;
            value = identity;
        }
    };
    fill(mask.opacity, out.opacity, out.opacityValue, 1);
    fill(mask.x, out.x, out.xValue, 0);
    fill(mask.y, out.y, out.yValue, 0);
    fill(mask.z, out.z, out.zValue, 0);
    fill(mask.scale, out.scale, out.scaleValue, 1);
    fill(mask.scaleX, out.scaleX, out.scaleXValue, 1);
    fill(mask.scaleY, out.scaleY, out.scaleYValue, 1);
    fill(mask.rotate, out.rotate, out.rotateValue, 0);
    fill(mask.rotateX, out.rotateX, out.rotateXValue, 0);
    fill(mask.rotateY, out.rotateY, out.rotateYValue, 0);
    fill(mask.skewX, out.skewX, out.skewXValue, 0);
    fill(mask.skewY, out.skewY, out.skewYValue, 0);
    fill(mask.blur, out.blur, out.blurValue, 0);
    return out;
}

Pose Union(const Pose& left, const Pose& right) {
    Pose mask = left;
    mask.opacity = left.opacity || right.opacity;
    mask.x = left.x || right.x;
    mask.y = left.y || right.y;
    mask.z = left.z || right.z;
    mask.scale = left.scale || right.scale;
    mask.scaleX = left.scaleX || right.scaleX;
    mask.scaleY = left.scaleY || right.scaleY;
    mask.rotate = left.rotate || right.rotate;
    mask.rotateX = left.rotateX || right.rotateX;
    mask.rotateY = left.rotateY || right.rotateY;
    mask.skewX = left.skewX || right.skewX;
    mask.skewY = left.skewY || right.skewY;
    mask.blur = left.blur || right.blur;
    return mask;
}

std::string Keyframes(const std::string& name, const std::string& from, const std::string& to) {
    return "@keyframes " + name + "{from{" + from + "}to{" + to + "}}\n";
}

std::string EaseCss(const std::string& name) {
    if (name == "linear") return "linear";
    if (name == "ease") return "ease";
    if (name == "ease-in") return "ease-in";
    if (name == "ease-out") return "ease-out";
    if (name == "ease-in-out") return "ease-in-out";
    if (name == "spring") return "cubic-bezier(0.22,1.35,0.36,1)";
    if (name == "bounce") return "cubic-bezier(0.34,1.56,0.64,1)";
    if (name == "anticipate") return "cubic-bezier(0.4,-0.15,0.2,1.2)";
    if (name == "smooth") return "cubic-bezier(0.22,1,0.36,1)";
    if (name == "snappy") return "cubic-bezier(0.2,0,0,1)";
    if (name == "gentle") return "cubic-bezier(0.4,0,0.2,1)";
    return name;
}

struct Timing {
    std::string duration;
    std::string delay;
    std::string ease;
    std::string repeat;
    std::string direction;
};

void ReadTransition(const std::string& text, Timing& timing) {
    std::string token;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const char ch = i == text.size() ? ' ' : text[i];
        if (ch == ' ' || ch == '\t' || ch == ',' || ch == ';') {
            if (!token.empty()) {
                const auto colon = token.find(':');
                if (colon != std::string::npos && colon > 0 && colon + 1 < token.size()) {
                    const std::string key = Lower(token.substr(0, colon));
                    const std::string value = Lower(token.substr(colon + 1));
                    if (key == "duration") timing.duration = value;
                    else if (key == "delay") timing.delay = value;
                    else if (key == "ease" || key == "easing" || key == "type") timing.ease = value == "tween" ? "smooth" : value;
                    else if (key == "repeat") timing.repeat = value;
                    else if (key == "direction" || key == "repeattype") timing.direction = value == "loop" ? "normal" : value == "mirror" ? "alternate" : value;
                    else std::cerr << "warning: transition '" << key << "' was not recognized and was skipped.\n";
                }
            }
            token.clear();
        } else {
            token.push_back(ch);
        }
    }
}

std::string Milliseconds(std::string value) {
    if (value.empty()) return {};
    bool digits = true;
    for (char ch : value) {
        if (ch < '0' || ch > '9') digits = false;
    }
    if (digits) value += "ms";
    return value;
}

std::string Quote(const std::string& value) {
    std::string out;
    for (char ch : value) {
        if (ch == '"' || ch == '<' || ch == '>' || ch == '&') continue;
        out.push_back(ch);
    }
    return out;
}

}  // namespace

void RoboWebMotionReset() {
    g_next = 0;
    g_used = false;
    g_css.str("");
    g_css.clear();
}

void RoboWebAppendMotion(std::string& style, const RoboXMLNode& node) {
    Timing timing;
    ReadTransition(Attr(node, "transition"), timing);
    if (!Attr(node, "duration").empty()) timing.duration = Attr(node, "duration");
    if (!Attr(node, "delay").empty()) timing.delay = Attr(node, "delay");
    if (!Attr(node, "easing").empty()) timing.ease = Lower(Attr(node, "easing"));
    if (!Attr(node, "repeat").empty()) timing.repeat = Lower(Attr(node, "repeat"));
    const std::string animate = Lower(Attr(node, "animate"));
    if (timing.repeat.empty() && KnownEnter(animate) && Looping(animate)) timing.repeat = "infinite";
    std::string extra;
    if (!timing.duration.empty()) extra += "--robo-motion-duration:" + Milliseconds(timing.duration) + ";";
    if (!timing.delay.empty()) extra += "--robo-motion-delay:" + Milliseconds(timing.delay) + ";";
    if (!timing.ease.empty()) extra += "--robo-motion-ease:" + EaseCss(timing.ease) + ";";
    if (!timing.repeat.empty()) extra += "--robo-motion-repeat:" + timing.repeat + ";";
    if (!timing.direction.empty()) extra += "--robo-motion-direction:" + timing.direction + ";";
    if (extra.empty()) return;
    if (!style.empty() && style.back() != ';') style += ";";
    style += extra;
}

std::string RoboWebMotionAttrs(const RoboXMLNode& node) {
    const std::string animate = Lower(Attr(node, "animate"));
    const std::string initial = Attr(node, "initial");
    const std::string exitValue = Attr(node, "exit");
    const std::string hover = Attr(node, "whilehover");
    const std::string tap = Attr(node, "whiletap");
    const std::string focus = Attr(node, "whilefocus");
    const std::string inview = Attr(node, "whileinview");
    const std::string drag = Lower(Attr(node, "drag"));
    const std::string once = Lower(Attr(node, "once"));
    if (animate.empty() && initial.empty() && exitValue.empty() && hover.empty() && tap.empty() && focus.empty() &&
        inview.empty() && drag.empty()) {
        return {};
    }
    g_used = true;
    std::string attrs;
    const auto data = [&](const char* key, const std::string& value) {
        if (value.empty()) return;
        attrs += " data-";
        attrs += key;
        attrs += "=\"";
        attrs += Quote(value);
        attrs += "\"";
    };
    std::string play = animate;
    if (play.empty()) play = Lower(inview);
    if (KnownEnter(play)) {
        data("animate", play);
    } else if (IsBag(play) || IsBag(initial)) {
        Pose fromPose;
        Pose toPose;
        const bool hasFrom = IsBag(initial) && ReadPose(initial, fromPose);
        const bool hasTo = IsBag(play) && ReadPose(play, toPose);
        if (hasFrom || hasTo) {
            const std::string name = "robo-m-" + std::to_string(++g_next);
            const Pose mask = Union(hasFrom ? fromPose : Pose{}, hasTo ? toPose : Pose{});
            g_css << Keyframes(name, Decl(Masked(hasFrom ? fromPose : Pose{}, mask)), Decl(Masked(hasTo ? toPose : Pose{}, mask)));
            g_css << "[data-animate=\"" << name << "\"]{animation-name:" << name << "}\n";
            data("animate", name);
        }
    } else if (!play.empty()) {
        std::cerr << "warning: animation '" << play << "' was not recognized and was skipped.\n";
    }
    if (!exitValue.empty()) {
        const std::string exitName = Lower(exitValue);
        if (KnownEnter(exitName)) {
            data("exit-name", "robo-" + exitName);
            data("exit-reverse", "true");
        } else if (IsBag(exitValue)) {
            Pose pose;
            if (ReadPose(exitValue, pose)) {
                const std::string name = "robo-x-" + std::to_string(++g_next);
                g_css << Keyframes(name, Decl(Masked(Pose{}, pose)), Decl(pose));
                data("exit-name", name);
            }
        } else {
            std::cerr << "warning: animation '" << exitValue << "' was not recognized and was skipped.\n";
        }
    }
    std::string motionId;
    const auto gesture = [&](const char* key, const std::string& value, const char* pseudo) {
        if (value.empty()) return;
        const std::string name = Lower(value);
        if (KnownGesture(name) || KnownEnter(name)) {
            data(key, name);
            return;
        }
        if (!IsBag(value)) {
            std::cerr << "warning: animation '" << value << "' was not recognized and was skipped.\n";
            return;
        }
        Pose pose;
        if (!ReadPose(value, pose)) return;
        if (motionId.empty()) motionId = std::to_string(++g_next);
        g_css << "[data-motion-id=\"" << motionId << "\"]" << pseudo << "{" << Decl(pose)
              << "transition:opacity var(--robo-motion-duration,400ms) var(--robo-motion-ease,ease),"
                 "transform var(--robo-motion-duration,400ms) var(--robo-motion-ease,ease),"
                 "filter var(--robo-motion-duration,400ms) var(--robo-motion-ease,ease)}\n";
        data(key, "custom");
    };
    gesture("hover", hover, ":hover");
    gesture("tap", tap, ":active");
    gesture("focus-motion", focus, ":focus-visible");
    data("motion-id", motionId);
    if (!inview.empty()) data("while-inview", "true");
    if (once == "false" || once == "0" || once == "no") data("once", "false");
    if (drag == "true" || drag == "x" || drag == "y" || drag == "1" || drag == "yes") data("drag", drag == "1" || drag == "yes" ? "true" : drag);
    return attrs;
}

std::string RoboWebMotionCss() {
    if (!g_used) return {};
    std::ostringstream css;
    css << "[data-animate]{animation-duration:var(--robo-motion-duration,400ms);animation-delay:var(--robo-motion-delay,0ms);"
           "animation-timing-function:var(--robo-motion-ease,cubic-bezier(0.22,1,0.36,1));"
           "animation-fill-mode:both;animation-iteration-count:var(--robo-motion-repeat,1);"
           "animation-direction:var(--robo-motion-direction,normal)}\n";
    css << "[data-while-inview]{animation-play-state:paused}\n";
    css << "[data-while-inview].is-inview{animation-play-state:running}\n";
    css << "[data-hover],[data-tap],[data-focus-motion]{transition:transform var(--robo-motion-duration,400ms) var(--robo-motion-ease,ease),"
           "filter var(--robo-motion-duration,400ms) var(--robo-motion-ease,ease),opacity var(--robo-motion-duration,400ms) var(--robo-motion-ease,ease)}\n";
    css << "[data-hover=lift]:hover{transform:translateY(-4px)}\n";
    css << "[data-hover=grow]:hover,[data-focus-motion=grow]:focus-visible{transform:scale(1.06)}\n";
    css << "[data-hover=shrink]:hover,[data-tap=shrink]:active{transform:scale(0.94)}\n";
    css << "[data-hover=tilt]:hover{transform:rotate(-2deg)}\n";
    css << "[data-hover=glow]:hover,[data-focus-motion=glow]:focus-visible{filter:brightness(1.12)}\n";
    css << "[data-hover=press]:hover,[data-tap=press]:active{transform:scale(0.96)}\n";
    css << "[data-tap=grow]:active{transform:scale(1.04)}\n";
    css << "[data-hover=shake]:hover,[data-tap=shake]:active{animation:robo-shake var(--robo-motion-duration,400ms) var(--robo-motion-ease,ease)}\n";
    css << "[data-hover=pulse]:hover{animation:robo-pulse var(--robo-motion-duration,700ms) ease-in-out infinite alternate}\n";
    css << "[data-hover=spin]:hover,[data-tap=spin]:active{animation:robo-spin var(--robo-motion-duration,800ms) linear infinite}\n";
    const char* names[] = {
        "fade", "fade-up", "fade-down", "fade-left", "fade-right",
        "slide-up", "slide-down", "slide-left", "slide-right",
        "zoom-in", "zoom-out", "pop", "grow", "shrink",
        "flip-x", "flip-y", "rotate-in", "spin", "pulse", "bounce",
        "shake", "swing", "float", "wobble", "blur-in", "rise", "drop",
        "expand", "heartbeat", "flash", "jello", "rubber", "tada", "ping"};
    for (const char* name : names) {
        css << "[data-animate=" << name << "]{animation-name:robo-" << name << "}\n";
    }
    css << "@keyframes robo-fade{from{opacity:0}to{opacity:1}}\n";
    css << "@keyframes robo-fade-up{from{opacity:0;transform:translateY(16px)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-fade-down{from{opacity:0;transform:translateY(-16px)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-fade-left{from{opacity:0;transform:translateX(20px)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-fade-right{from{opacity:0;transform:translateX(-20px)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-slide-up{from{transform:translateY(100%)}to{transform:none}}\n";
    css << "@keyframes robo-slide-down{from{transform:translateY(-100%)}to{transform:none}}\n";
    css << "@keyframes robo-slide-left{from{transform:translateX(100%)}to{transform:none}}\n";
    css << "@keyframes robo-slide-right{from{transform:translateX(-100%)}to{transform:none}}\n";
    css << "@keyframes robo-zoom-in{from{opacity:0;transform:scale(0.86)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-zoom-out{from{opacity:0;transform:scale(1.14)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-pop{0%{opacity:0;transform:scale(0.6)}70%{transform:scale(1.08)}100%{opacity:1;transform:none}}\n";
    css << "@keyframes robo-grow{from{transform:scale(0.8)}to{transform:none}}\n";
    css << "@keyframes robo-shrink{from{transform:scale(1.2)}to{transform:none}}\n";
    css << "@keyframes robo-flip-x{from{opacity:0;transform:perspective(400px) rotateX(90deg)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-flip-y{from{opacity:0;transform:perspective(400px) rotateY(90deg)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-rotate-in{from{opacity:0;transform:rotate(-12deg)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-spin{to{transform:rotate(360deg)}}\n";
    css << "@keyframes robo-pulse{from{transform:scale(1)}to{transform:scale(1.06)}}\n";
    css << "@keyframes robo-bounce{0%,100%{transform:translateY(0)}50%{transform:translateY(-10px)}}\n";
    css << "@keyframes robo-shake{25%{transform:translateX(-4px)}75%{transform:translateX(4px)}}\n";
    css << "@keyframes robo-swing{20%{transform:rotate(8deg)}40%{transform:rotate(-6deg)}60%{transform:rotate(3deg)}80%{transform:rotate(-2deg)}100%{transform:none}}\n";
    css << "@keyframes robo-float{from{transform:translateY(0)}to{transform:translateY(-8px)}}\n";
    css << "@keyframes robo-wobble{20%{transform:translateX(-6px) rotate(-3deg)}40%{transform:translateX(5px) rotate(2deg)}60%{transform:translateX(-3px) rotate(-1deg)}100%{transform:none}}\n";
    css << "@keyframes robo-blur-in{from{opacity:0;filter:blur(10px)}to{opacity:1;filter:none}}\n";
    css << "@keyframes robo-rise{from{opacity:0;transform:translateY(28px) scale(0.98)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-drop{from{opacity:0;transform:translateY(-28px)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-expand{from{opacity:0;transform:scaleX(0.4)}to{opacity:1;transform:none}}\n";
    css << "@keyframes robo-heartbeat{0%,100%{transform:scale(1)}30%{transform:scale(1.12)}55%{transform:scale(0.96)}}\n";
    css << "@keyframes robo-flash{0%,100%{opacity:1}50%{opacity:0.35}}\n";
    css << "@keyframes robo-jello{30%{transform:skewX(12deg) scale(1.04)}50%{transform:skewX(-8deg)}70%{transform:skewX(4deg)}100%{transform:none}}\n";
    css << "@keyframes robo-rubber{30%{transform:scaleX(1.16) scaleY(0.86)}50%{transform:scaleX(0.9) scaleY(1.08)}100%{transform:none}}\n";
    css << "@keyframes robo-tada{10%,30%{transform:scale(0.9) rotate(-3deg)}20%,40%{transform:scale(1.08) rotate(3deg)}100%{transform:none}}\n";
    css << "@keyframes robo-ping{0%{transform:scale(1);opacity:1}80%,100%{transform:scale(1.4);opacity:0}}\n";
    css << "@media (prefers-reduced-motion:reduce){[data-animate],[data-hover],[data-tap],[data-focus-motion]{animation:none !important;transition:none !important}}\n";
    css << g_css.str();
    return css.str();
}

std::string RoboWebMotionScript() {
    if (!g_used) return {};
    return R"JS(Robo.motion = function () {
  var nodes = document.querySelectorAll("[data-while-inview]");
  if (nodes.length && "IntersectionObserver" in window) {
    var observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) {
          entry.target.classList.add("is-inview");
          if (entry.target.getAttribute("data-once") !== "false") observer.unobserve(entry.target);
        } else if (entry.target.getAttribute("data-once") === "false") {
          entry.target.classList.remove("is-inview");
        }
      });
    }, { threshold: 0.2 });
    nodes.forEach(function (node) { observer.observe(node); });
  }
  document.querySelectorAll("[data-drag]").forEach(function (el) {
    var axis = el.getAttribute("data-drag");
    var originX = 0, originY = 0, startX = 0, startY = 0, lastX = 0, lastY = 0, velocityX = 0, velocityY = 0, lastTime = 0, dragging = false;
    el.style.touchAction = "none";
    el.addEventListener("pointerdown", function (event) {
      dragging = true;
      startX = event.clientX;
      startY = event.clientY;
      lastX = originX;
      lastY = originY;
      lastTime = performance.now();
      el.setPointerCapture(event.pointerId);
    });
    el.addEventListener("pointermove", function (event) {
      if (!dragging) return;
      var now = performance.now();
      var nextX = axis === "y" ? originX : originX + (event.clientX - startX);
      var nextY = axis === "x" ? originY : originY + (event.clientY - startY);
      if (lastTime) {
        velocityX = (nextX - lastX) / (now - lastTime);
        velocityY = (nextY - lastY) / (now - lastTime);
      }
      lastX = nextX;
      lastY = nextY;
      lastTime = now;
      el.style.transform = "translate(" + nextX + "px," + nextY + "px)";
    });
    var release = function () {
      if (!dragging) return;
      dragging = false;
      originX = lastX;
      originY = lastY;
      var step = function () {
        velocityX *= 0.92;
        velocityY *= 0.92;
        if (Math.abs(velocityX) < 0.01 && Math.abs(velocityY) < 0.01) return;
        originX += velocityX * 16;
        originY += velocityY * 16;
        el.style.transform = "translate(" + originX + "px," + originY + "px)";
        requestAnimationFrame(step);
      };
      requestAnimationFrame(step);
    };
    el.addEventListener("pointerup", release);
    el.addEventListener("pointercancel", release);
  });
};
Robo.leave = function (node) {
  if (!node) return;
  var name = node.getAttribute("data-exit-name");
  if (!name) {
    node.style.display = "none";
    return;
  }
  node.style.animation = "none";
  void node.offsetWidth;
  node.style.animationName = name;
  node.style.animationDuration = "var(--robo-motion-duration,400ms)";
  node.style.animationTimingFunction = "var(--robo-motion-ease,ease)";
  node.style.animationFillMode = "forwards";
  node.style.animationDirection = node.getAttribute("data-exit-reverse") === "true" ? "reverse" : "normal";
  node.style.animationIterationCount = "1";
  node.addEventListener("animationend", function () { node.style.display = "none"; }, { once: true });
};
Robo.motion();
)JS";
}
