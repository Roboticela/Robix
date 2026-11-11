#include "Robix3D.hpp"

#include "Compile.hpp"
#include "Web/WebFoundation.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace Robix::D3 {
namespace {

std::string gModule;
std::string gCss;
fs::path gDocument;
fs::path gOutRoot;
std::unordered_set<std::string> gStack;
int gView = 0;

struct Piece {
    std::string id;
    std::string module;
    std::string ui;
    std::string version;
};

std::vector<Piece> gPieces;
std::vector<std::string> gErrors;

struct HostState {
    std::string module;
    std::string css;
    fs::path document;
    fs::path outRoot;
    int view = 0;
    std::unordered_set<std::string> stack;
    std::vector<Piece> pieces;
    std::vector<std::string> errors;
};

HostState CaptureHost() {
    return HostState{gModule, gCss, gDocument, gOutRoot, gView, gStack, gPieces, gErrors};
}

void ApplyHost(const HostState& saved) {
    gModule = saved.module;
    gCss = saved.css;
    gDocument = saved.document;
    gOutRoot = saved.outRoot;
    gView = saved.view;
    gStack = saved.stack;
    gPieces = saved.pieces;
    gErrors = saved.errors;
}

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

std::string Trim(std::string text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
    return text;
}

std::string Attr(const RoboXMLNode& node, const char* key) { return RoboXMLAttr(node, key); }

std::string Js(const std::string& text) {
    std::string out;
    for (char ch : text) {
        if (ch == '\\' || ch == '"' || ch == '\n' || ch == '\r') out.push_back('\\');
        if (ch == '\n') out.push_back('n');
        else if (ch == '\r') out.push_back('r');
        else out.push_back(ch);
    }
    return out;
}

std::string Html(const std::string& text) {
    std::string out;
    for (char ch : text) {
        if (ch == '&') out += "&amp;";
        else if (ch == '<') out += "&lt;";
        else if (ch == '>') out += "&gt;";
        else if (ch == '"') out += "&quot;";
        else out.push_back(ch);
    }
    return out;
}

std::string Num(double value) {
    std::ostringstream out;
    out.setf(std::ios::fmtflags(0), std::ios::floatfield);
    out.precision(9);
    out << value;
    return out.str();
}

bool Flag(const std::string& value) {
    const std::string text = Lower(value);
    return text == "true" || text == "1" || text == "yes" || text == "on";
}

struct Vec3 {
    double x = 0;
    double y = 0;
    double z = 0;
    bool set = false;
};

Vec3 ParseVec(const std::string& text, double fill) {
    Vec3 value;
    value.x = value.y = value.z = fill;
    std::string token;
    double items[3] = {fill, fill, fill};
    int count = 0;
    const auto push = [&](const std::string& part) {
        if (part.empty() || count >= 3) return;
        try {
            items[count++] = std::stod(part);
        } catch (...) {
        }
    };
    for (char ch : text) {
        if (ch == ',' || std::isspace(static_cast<unsigned char>(ch))) {
            push(Trim(token));
            token.clear();
        } else {
            token.push_back(ch);
        }
    }
    push(Trim(token));
    if (count > 0) {
        value = {items[0], count > 1 ? items[1] : fill, count > 2 ? items[2] : fill, true};
    }
    return value;
}

std::vector<double> ParseList(const std::string& text) {
    std::vector<double> values;
    std::string token;
    const auto push = [&](const std::string& part) {
        if (part.empty()) return;
        try {
            values.push_back(std::stod(part));
        } catch (...) {
        }
    };
    for (char ch : text) {
        if (ch == ',' || ch == ';' || std::isspace(static_cast<unsigned char>(ch))) {
            push(Trim(token));
            token.clear();
        } else {
            token.push_back(ch);
        }
    }
    push(Trim(token));
    return values;
}

const RoboXMLNode* Child(const RoboXMLNode& node, const std::string& name) {
    for (const RoboXMLNode& child : node.children) {
        if (Lower(child.name) == name) return &child;
    }
    return nullptr;
}

std::string TextOf(const RoboXMLNode& node, const char* childName) {
    if (const RoboXMLNode* child = Child(node, childName)) {
        if (!Trim(child->text).empty()) return Trim(child->text);
        if (!Attr(*child, "value").empty()) return Attr(*child, "value");
    }
    return {};
}

struct Uses {
    bool area = false;
    bool orbit = false;
    bool map = false;
    bool fly = false;
    bool room = false;
    bool hdr = false;
    bool gltf = false;
    bool font = false;
    bool sky = false;
    bool bloom = false;
    bool csg = false;
    bool rapier = false;
    bool rounded = false;
    bool extrude = false;
    bool lathe = false;
    bool tube = false;
    bool custom = false;
    bool subdivide = false;
    bool solidify = false;
    bool deform = false;
    bool curve = false;
};

struct Out {
    std::ostringstream js;
    std::string ui;
    Uses uses;
    bool degrees = false;
    fs::path directory;
    fs::path outRoot;
    std::vector<std::string> errors;
    std::unordered_set<std::string> played;
    int next = 0;
    const RoboXMLNode* document = nullptr;
};

std::string Fresh(Out& out) {
    return "o" + std::to_string(out.next++);
}

std::string Publish(Out& out, const std::string& source, const char* folder) {
    if (source.empty()) return {};
    const std::string key = Lower(source);
    if (key.rfind("http://", 0) == 0 || key.rfind("https://", 0) == 0 || key.rfind("data:", 0) == 0) return source;
    fs::path path = fs::path(source);
    if (path.is_relative()) path = out.directory / path;
    std::error_code error;
    if (!fs::is_regular_file(path, error)) {
        out.errors.push_back("file was not found: " + source);
        return source;
    }
    const fs::path destDir = out.outRoot / folder;
    fs::create_directories(destDir, error);
    const fs::path dest = destDir / path.filename();
    fs::copy_file(path, dest, fs::copy_options::overwrite_existing, error);
    if (error) out.errors.push_back("could not copy " + source);
    return std::string(folder) + "/" + path.filename().string();
}

double Rad(const Out& out, const RoboXMLNode* transform, double value) {
    const bool degrees = out.degrees || (transform != nullptr && Flag(Attr(*transform, "degrees")));
    return degrees ? value * 3.14159265358979323846 / 180.0 : value;
}

void EmitTransform(Out& out, const std::string& object, const RoboXMLNode& node) {
    const RoboXMLNode* transform = Child(node, "transform");
    auto value = [&](const char* key) {
        if (transform != nullptr && !Attr(*transform, key).empty()) return Attr(*transform, key);
        return Attr(node, key);
    };
    const Vec3 position = ParseVec(value("position"), 0);
    const Vec3 rotation = ParseVec(value("rotation"), 0);
    const Vec3 scale = ParseVec(value("scale"), 1);
    if (position.set) out.js << object << ".position.set(" << Num(position.x) << "," << Num(position.y) << "," << Num(position.z) << ");\n";
    if (rotation.set) {
        out.js << object << ".rotation.set(" << Num(Rad(out, transform, rotation.x)) << "," << Num(Rad(out, transform, rotation.y)) << ","
               << Num(Rad(out, transform, rotation.z)) << ");\n";
    }
    if (scale.set && (value("scale").size() > 0)) {
        out.js << object << ".scale.set(" << Num(scale.x) << "," << Num(scale.y) << "," << Num(scale.z) << ");\n";
    }
}

std::string MaterialExpr(Out& out, const RoboXMLNode* material) {
    const std::string type = material == nullptr ? "standard" : Lower(Attr(*material, "type").empty() ? "standard" : Attr(*material, "type"));
    std::string ctor = "MeshStandardMaterial";
    if (type == "physical" || type == "glass" || type == "metal") ctor = "MeshPhysicalMaterial";
    else if (type == "basic") ctor = "MeshBasicMaterial";
    else if (type == "lambert") ctor = "MeshLambertMaterial";
    else if (type == "phong") ctor = "MeshPhongMaterial";
    else if (type == "toon") ctor = "MeshToonMaterial";
    else if (type == "normal") ctor = "MeshNormalMaterial";
    else if (type == "matcap") ctor = "MeshMatcapMaterial";
    std::ostringstream args;
    const std::string color = material == nullptr || Attr(*material, "color").empty() ? "#cccccc" : Attr(*material, "color");
    args << "color:\"" << Js(color) << "\"";
    auto num = [&](const char* key) {
        if (material == nullptr || Attr(*material, key).empty()) return;
        args << "," << key << ":" << Attr(*material, key);
    };
    if (type == "metal" && (material == nullptr || Attr(*material, "metalness").empty())) args << ",metalness:1,roughness:0.25";
    if (type == "glass") {
        if (material == nullptr || Attr(*material, "transmission").empty()) args << ",transmission:1,roughness:0.05,thickness:0.4";
        args << ",transparent:true";
    }
    num("roughness");
    num("metalness");
    num("opacity");
    num("transmission");
    num("thickness");
    num("clearcoat");
    num("clearcoatRoughness");
    num("ior");
    num("iridescence");
    num("sheen");
    num("anisotropy");
    num("emissiveIntensity");
    num("attenuationDistance");
    if (material != nullptr && !Attr(*material, "emissive").empty()) args << ",emissive:\"" << Js(Attr(*material, "emissive")) << "\"";
    if (material != nullptr && !Attr(*material, "attenuationColor").empty()) args << ",attenuationColor:\"" << Js(Attr(*material, "attenuationColor")) << "\"";
    if (material != nullptr && !Attr(*material, "sheenColor").empty()) args << ",sheenColor:\"" << Js(Attr(*material, "sheenColor")) << "\"";
    if (material != nullptr && Flag(Attr(*material, "wireframe"))) args << ",wireframe:true";
    if (material != nullptr && (!Attr(*material, "opacity").empty() || Flag(Attr(*material, "transparent")))) args << ",transparent:true";
    if (material != nullptr && Lower(Attr(*material, "depthWrite")) == "false") args << ",depthWrite:false";
    if (material != nullptr && Lower(Attr(*material, "blending")) == "additive") args << ",blending:THREE.AdditiveBlending";
    if (material != nullptr) {
        const std::string side = Lower(Attr(*material, "side"));
        if (side == "double") args << ",side:THREE.DoubleSide";
        else if (side == "back") args << ",side:THREE.BackSide";
    }
    auto map = [&](const char* key, bool colorMap) {
        if (material == nullptr || Attr(*material, key).empty()) return;
        const std::string url = Publish(out, Attr(*material, key), "images");
        args << "," << key << ":(()=>{const tex=new THREE.TextureLoader().load(\"" << Js(url) << "\");";
        if (colorMap) args << "tex.colorSpace=THREE.SRGBColorSpace;";
        args << "return tex;})()";
    };
    map("map", true);
    map("normalMap", false);
    map("roughnessMap", false);
    map("metalnessMap", false);
    map("aoMap", false);
    map("emissiveMap", true);
    map("matcap", true);
    if (material != nullptr && !Attr(*material, "shader").empty()) {
        std::ostringstream wrapped;
        wrapped << "(()=>{const material=shaders[\"" << Js(Attr(*material, "shader")) << "\"](ctx);";
        for (const auto& entry : material->attrs) {
            const std::string key = entry.first;
            const std::string lower = Lower(key);
            if (lower == "shader" || lower == "type" || lower == "transparent" || lower == "opacity" || lower == "side" ||
                lower == "wireframe" || lower == "blending" || lower == "depthwrite" || lower == "color") {
                continue;
            }
            wrapped << "if(material.uniforms&&material.uniforms[\"" << Js(key) << "\"]){";
            if (!entry.second.empty() && entry.second.front() == '#') {
                wrapped << "material.uniforms[\"" << Js(key) << "\"].value.set(\"" << Js(entry.second) << "\");";
            } else {
                wrapped << "material.uniforms[\"" << Js(key) << "\"].value=" << entry.second << ";";
            }
            wrapped << "}";
        }
        if (Flag(Attr(*material, "transparent")) || !Attr(*material, "opacity").empty()) wrapped << "material.transparent=true;";
        if (Lower(Attr(*material, "depthWrite")) == "false") wrapped << "material.depthWrite=false;";
        if (Lower(Attr(*material, "blending")) == "additive") wrapped << "material.blending=THREE.AdditiveBlending;";
        if (!Attr(*material, "opacity").empty()) wrapped << "material.opacity=" << Attr(*material, "opacity") << ";";
        wrapped << "return material;})()";
        return wrapped.str();
    }
    return "new THREE." + ctor + "({" + args.str() + "})";
}

std::string GeometryExpr(Out& out, const RoboXMLNode* geometry) {
    const std::string type = geometry == nullptr ? "box" : Lower(Attr(*geometry, "type").empty() ? "box" : Attr(*geometry, "type"));
    auto n = [&](const char* key, const char* fallback) {
        if (geometry == nullptr || Attr(*geometry, key).empty()) return std::string(fallback);
        return Attr(*geometry, key);
    };
    if (type == "sphere") return "new THREE.SphereGeometry(" + n("radius", "1") + "," + n("widthSegments", "32") + "," + n("heightSegments", "16") + ")";
    if (type == "cylinder" || type == "cone") {
        const std::string top = type == "cone" ? "0" : n("radiusTop", n("radius", "1").c_str());
        return "new THREE.CylinderGeometry(" + top + "," + n("radiusBottom", n("radius", "1").c_str()) + "," + n("height", "1") + "," +
               n("segments", "32") + ",1," + (Flag(geometry ? Attr(*geometry, "openEnded") : "") ? "true" : "false") + ")";
    }
    if (type == "capsule") return "new THREE.CapsuleGeometry(" + n("radius", "0.5") + "," + n("length", n("height", "1").c_str()) + "," + n("capSegments", "8") + "," + n("segments", "16") + ")";
    if (type == "plane") return "new THREE.PlaneGeometry(" + n("width", "1") + "," + n("height", "1") + ")";
    if (type == "circle") return "new THREE.CircleGeometry(" + n("radius", "1") + "," + n("segments", "32") + ")";
    if (type == "ring") return "new THREE.RingGeometry(" + n("innerRadius", "0.4") + "," + n("outerRadius", n("radius", "1").c_str()) + "," + n("segments", "32") + ")";
    if (type == "torus") return "new THREE.TorusGeometry(" + n("radius", "1") + "," + n("tube", "0.3") + "," + n("radialSegments", "16") + "," + n("tubularSegments", "64") + ")";
    if (type == "torusknot") return "new THREE.TorusKnotGeometry(" + n("radius", "1") + "," + n("tube", "0.3") + "," + n("tubularSegments", "128") + "," + n("radialSegments", "16") + "," + n("p", "2") + "," + n("q", "3") + ")";
    if (type == "icosahedron") return "new THREE.IcosahedronGeometry(" + n("radius", "1") + "," + n("detail", "0") + ")";
    if (type == "octahedron") return "new THREE.OctahedronGeometry(" + n("radius", "1") + "," + n("detail", "0") + ")";
    if (type == "dodecahedron") return "new THREE.DodecahedronGeometry(" + n("radius", "1") + "," + n("detail", "0") + ")";
    if (type == "tetrahedron") return "new THREE.TetrahedronGeometry(" + n("radius", "1") + "," + n("detail", "0") + ")";
    if (type == "rounded-box" || type == "roundedbox") {
        out.uses.rounded = true;
        return "roundedBox(" + n("width", "1") + "," + n("height", "1") + "," + n("depth", "1") + "," + n("radius", "0.08") + "," + n("segments", "8") + ")";
    }
    if (type == "extrude") {
        out.uses.extrude = true;
        std::vector<double> points = ParseList(Attr(*geometry, "points"));
        if (points.empty()) points = ParseList(TextOf(*geometry, "point"));
        std::ostringstream list;
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (i) list << ",";
            list << Num(points[i]);
        }
        return "extrude([" + list.str() + "]," + n("depth", "1") + "," + (Flag(Attr(*geometry, "bevel")) ? "true" : "false") + "," +
               n("bevelThickness", "0.05") + "," + n("bevelSize", "0.05") + "," + n("segments", "12") + ")";
    }
    if (type == "lathe") {
        out.uses.lathe = true;
        const auto points = ParseList(Attr(*geometry, "points").empty() ? TextOf(*geometry, "point") : Attr(*geometry, "points"));
        std::ostringstream list;
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (i) list << ",";
            list << Num(points[i]);
        }
        return "lathe([" + list.str() + "]," + n("segments", "24") + ")";
    }
    if (type == "tube") {
        out.uses.tube = true;
        const auto points = ParseList(Attr(*geometry, "points").empty() ? TextOf(*geometry, "point") : Attr(*geometry, "points"));
        std::ostringstream list;
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (i) list << ",";
            list << Num(points[i]);
        }
        return "tube([" + list.str() + "]," + n("radius", "0.1") + "," + n("tubularSegments", "64") + "," + n("radialSegments", "8") + ")";
    }
    if (type == "custom") {
        out.uses.custom = true;
        const auto vertices = ParseList(TextOf(*geometry, "vertices").empty() ? Attr(*geometry, "vertices") : TextOf(*geometry, "vertices"));
        const auto indices = ParseList(TextOf(*geometry, "indices").empty() ? Attr(*geometry, "indices") : TextOf(*geometry, "indices"));
        std::ostringstream verts;
        std::ostringstream inds;
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            if (i) verts << ",";
            verts << Num(vertices[i]);
        }
        for (std::size_t i = 0; i < indices.size(); ++i) {
            if (i) inds << ",";
            inds << Num(indices[i]);
        }
        return "customGeometry([" + verts.str() + "],[" + inds.str() + "])";
    }
    return "new THREE.BoxGeometry(" + n("width", "1") + "," + n("height", "1") + "," + n("depth", "1") + ")";
}

void EmitModifiers(Out& out, const RoboXMLNode& node, const std::string& object) {
    for (const RoboXMLNode& child : node.children) {
        if (Lower(child.name) != "modifier") continue;
        const std::string type = Lower(Attr(child, "type"));
        if (type == "array") {
            const Vec3 offset = ParseVec(Attr(child, "offset"), 0);
            out.js << "for (let i=1;i<" << (Attr(child, "count").empty() ? "2" : Attr(child, "count")) << ";++i){const copy=" << object
                   << ".clone();copy.position.copy(" << object << ".position).add(new THREE.Vector3(" << Num(offset.x) << "," << Num(offset.y)
                   << "," << Num(offset.z) << ").multiplyScalar(i));" << object << ".parent.add(copy);}\n";
        } else if (type == "mirror") {
            const std::string axis = Lower(Attr(child, "axis").empty() ? "x" : Attr(child, "axis"));
            const char channel = axis == "y" || axis == "z" ? axis[0] : 'x';
            out.js << "{const copy=" << object << ".clone();copy.scale." << channel << "*=-1;copy.position." << channel << "*=-1;" << object
                   << ".parent.add(copy);}\n";
        } else if (type == "subdivide") {
            out.uses.subdivide = true;
            out.js << object << ".geometry=subdivide(" << object << ".geometry," << (Attr(child, "level").empty() ? "1" : Attr(child, "level")) << ");\n";
        } else if (type == "solidify") {
            out.uses.solidify = true;
            out.js << object << ".geometry=solidify(" << object << ".geometry," << (Attr(child, "thickness").empty() ? "0.05" : Attr(child, "thickness")) << ");\n";
        } else if (type == "boolean") {
            out.uses.csg = true;
            const std::string operation = Lower(Attr(child, "operation").empty() ? "subtract" : Attr(child, "operation"));
            const char* op = operation == "add" || operation == "union" ? "ADDITION" : operation == "intersect" ? "INTERSECTION" : "SUBTRACTION";
            out.js << "{const host=" << object << ";const tool=ctx.objects.get(\"" << Js(Attr(child, "target"))
                   << "\");if(host&&tool){const bake=(mesh)=>{mesh.updateWorldMatrix(true,false);const geo=mesh.geometry.clone();geo.applyMatrix4(mesh.matrixWorld);return geo;};"
                   << "const result=new Evaluator().evaluate(new Brush(bake(host),host.material),new Brush(bake(tool),tool.material)," << op
                   << ");ctx.scene.attach(host);host.geometry=result.geometry;host.position.set(0,0,0);host.quaternion.identity();host.scale.set(1,1,1);tool.visible=false;}}\n";
        } else {
            out.errors.push_back("modifier '" + type + "' is not supported");
        }
    }
}

void EmitMotion(Out& out, const std::string& object, const RoboXMLNode& node);

void CollectLook(const RoboXMLNode& node, std::vector<const RoboXMLNode*>& wobbles, const RoboXMLNode*& color, const RoboXMLNode*& fade);

void EmitSpecValue(std::ostream& js, const std::string& raw) {
    const std::string text = Trim(raw);
    bool ident = !text.empty() && (std::isalpha(static_cast<unsigned char>(text.front())) || text.front() == '_');
    for (char ch : text) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') ident = false;
    }
    if (ident) js << "\"" << Js(text) << "\"";
    else js << (text.empty() ? "0" : text);
}

void EmitDeform(Out& out, const std::string& object, const RoboXMLNode* material) {
    if (material == nullptr || Attr(*material, "shader").empty() || out.document == nullptr) return;
    const RoboXMLNode* shader = nullptr;
    for (const RoboXMLNode& child : out.document->children) {
        if (Lower(child.name) == "shader" && Attr(child, "name") == Attr(*material, "shader")) shader = &child;
    }
    if (shader == nullptr) return;
    std::vector<const RoboXMLNode*> wobbles;
    const RoboXMLNode* color = nullptr;
    const RoboXMLNode* fade = nullptr;
    CollectLook(*shader, wobbles, color, fade);
    if (wobbles.empty()) return;
    out.uses.deform = true;
    out.js << "const wobble=[";
    bool first = true;
    for (const RoboXMLNode* wobble : wobbles) {
        if (!first) out.js << ",";
        first = false;
        out.js << "{amount:";
        EmitSpecValue(out.js, Attr(*wobble, "amount").empty() ? "0.2" : Attr(*wobble, "amount"));
        out.js << ",detail:";
        EmitSpecValue(out.js, Attr(*wobble, "detail").empty() ? "1" : Attr(*wobble, "detail"));
        out.js << ",speed:";
        EmitSpecValue(out.js, Attr(*wobble, "speed").empty() ? "0.2" : Attr(*wobble, "speed"));
        out.js << ",phase:";
        EmitSpecValue(out.js, Attr(*wobble, "phase").empty() ? "0" : Attr(*wobble, "phase"));
        out.js << ",layers:" << (Attr(*wobble, "layers").empty() ? "2" : Attr(*wobble, "layers")) << "}";
    }
    out.js << "];deform(" << object << ",material,wobble,0.01);ctx.frames.push((dt,time)=>deform(" << object << ",material,wobble,time));\n";
}

void EmitNode(Out& out, const RoboXMLNode& node, const std::string& parent, const std::string& path);

void EmitMesh(Out& out, const RoboXMLNode& node, const std::string& parent, const std::string& path, const std::string& ctor) {
    const std::string name = Attr(node, "name");
    const std::string full = name.empty() ? path : (path.empty() ? name : path + "." + name);
    const std::string object = Fresh(out);
    const RoboXMLNode* geometry = Child(node, "geometry");
    const RoboXMLNode* material = Child(node, "material");
    out.js << "{const geometry=" << GeometryExpr(out, geometry) << ";const material=" << MaterialExpr(out, material) << ";const " << object << "=" << ctor
           << ";" << object << ".castShadow=" << (Flag(Attr(node, "castShadow")) ? "true" : "false") << ";" << object << ".receiveShadow="
           << (Flag(Attr(node, "receiveShadow")) ? "true" : "false") << ";\n";
    if (!name.empty()) out.js << object << ".name=\"" << Js(name) << "\";\n";
    EmitTransform(out, object, node);
    if (!full.empty()) out.js << "remember(ctx,\"" << Js(full) << "\"," << object << ");\n";
    out.js << parent << ".add(" << object << ");\n";
    EmitDeform(out, object, material);
    EmitMotion(out, object, node);
    EmitModifiers(out, node, object);
    for (const RoboXMLNode& child : node.children) {
        const std::string kind = Lower(child.name);
        if (kind == "geometry" || kind == "material" || kind == "transform" || kind == "modifier") continue;
        if (kind == "animation" && Flag(Attr(child, "autoRotate"))) {
            const std::string speed = Attr(child, "speed").empty() ? "0.6" : Attr(child, "speed");
            out.js << "ctx.frames.push((dt)=>" << object << ".rotation.y+=" << speed << "*dt);\n";
            continue;
        }
        EmitNode(out, child, object, full);
    }
    out.js << "}\n";
}

std::string NumAttr(const RoboXMLNode& node, const char* key, const char* fallback) {
    return Attr(node, key).empty() ? std::string(fallback) : Attr(node, key);
}

void EmitSurfaceFlags(std::ostringstream& js, const RoboXMLNode& node, const std::string& material) {
    if (Flag(Attr(node, "transparent")) || !Attr(node, "opacity").empty()) js << material << ".transparent=true;";
    if (!Attr(node, "opacity").empty()) js << material << ".opacity=" << Attr(node, "opacity") << ";";
    if (Lower(Attr(node, "depthWrite")) == "false") js << material << ".depthWrite=false;";
    if (Lower(Attr(node, "blending")) == "additive") js << material << ".blending=THREE.AdditiveBlending;";
}

void EmitMotion(Out& out, const std::string& object, const RoboXMLNode& node) {
    const bool placed = !Attr(node, "viewX").empty() || !Attr(node, "viewY").empty() || !Attr(node, "pixelSize").empty();
    const bool moving = placed || !Attr(node, "spin").empty() || !Attr(node, "spinX").empty() || !Attr(node, "spinY").empty() ||
                        !Attr(node, "spinZ").empty() || !Attr(node, "float").empty() || !Attr(node, "parallax").empty() ||
                        !Attr(node, "enter").empty() || !Attr(node, "pulse").empty() || !Attr(node, "opacityPulse").empty();
    if (!moving) return;
    if (placed) {
        out.js << object << ".userData.view={nx:" << NumAttr(node, "viewX", "0") << ",ny:" << NumAttr(node, "viewY", "0") << ",z:"
               << NumAttr(node, "viewZ", "0") << ",px:" << NumAttr(node, "pixelSize", "100") << ",narrowAt:" << NumAttr(node, "narrowAt", "100000")
               << ",narrowScale:" << NumAttr(node, "narrowScale", "1") << ",narrowEdge:" << NumAttr(node, "narrowEdge", "1") << "};\n";
        out.js << "ctx.layouts.push((width,height)=>{const v=" << object << ".userData.view;const distance=Math.abs(ctx.camera.position.z-v.z);"
               << "const visible=2*Math.tan(THREE.MathUtils.degToRad(ctx.camera.fov/2))*distance;const narrow=width<=v.narrowAt;"
               << "const edge=narrow?v.narrowEdge:1;const diameter=narrow?v.px*v.narrowScale:v.px;const world=(px)=>px/Math.max(height,1)*visible;"
               << object << ".userData.base=new THREE.Vector3(world(v.nx*edge*width/2),world(v.ny*height/2),v.z);"
               << object << ".userData.radius=world(diameter/2);});\n";
    }
    out.js << "ctx.frames.push((dt,time)=>{const reduced=window.matchMedia(\"(prefers-reduced-motion: reduce)\").matches;";
    if (placed || !Attr(node, "float").empty() || !Attr(node, "parallax").empty() || !Attr(node, "enter").empty()) {
        out.js << "const base=" << object << ".userData.base;if(base){let y=base.y;const height=ctx.renderer.domElement.clientHeight||1;"
               << "const distance=Math.abs(ctx.camera.position.z-base.z);const visible=2*Math.tan(THREE.MathUtils.degToRad(ctx.camera.fov/2))*distance;"
               << "const world=(px)=>px/height*visible;";
        if (!Attr(node, "float").empty()) {
            out.js << "if(!reduced)y+=Math.sin((time*Math.PI*2)/" << NumAttr(node, "period", "8") << "+" << NumAttr(node, "phase", "0") << ")*world("
                   << NumAttr(node, "float", "0") << ");";
        }
        if (!Attr(node, "parallax").empty()) {
            out.js << "if(!reduced&&ctx.host)y+=ctx.host.getBoundingClientRect().top*" << NumAttr(node, "parallax", "0") << "/height*visible;";
        }
        out.js << object << ".position.set(base.x,y,base.z);";
        if (!Attr(node, "enter").empty()) {
            const std::string ease = Attr(node, "ease");
            double x1 = 0, y1 = 0, x2 = 1, y2 = 1;
            if (!ease.empty()) {
                const auto values = ParseList(ease);
                if (values.size() >= 4) {
                    x1 = values[0];
                    y1 = values[1];
                    x2 = values[2];
                    y2 = values[3];
                }
            }
            out.js << "let span=" << object << ".userData.radius||1;if(!reduced){const u=Math.min(1,Math.max(0,(time-" << NumAttr(node, "enterDelay", "0")
                   << ")/" << NumAttr(node, "enter", "0.6") << "));const sample=(t,a,b)=>{const m=1-t;return 3*m*m*t*a+3*m*t*t*b+t*t*t;};"
                   << "let lo=0,hi=1,mid=u;for(let i=0;i<8;i++){const x=sample(mid," << Num(x1) << "," << Num(x2) << ");if(x<u)lo=mid;else hi=mid;mid=(lo+hi)/2;}"
                   << "span*=sample(mid," << Num(y1) << "," << Num(y2) << ");}" << object << ".scale.setScalar(span);";
        } else if (placed) {
            out.js << object << ".scale.setScalar(" << object << ".userData.radius||1);";
        }
        out.js << "}";
    }
    auto spin = [&](const char* axis, const char* key) {
        if (Attr(node, key).empty()) return;
        out.js << "if(!reduced)" << object << ".rotation." << axis << "+=" << Attr(node, key) << "*dt;";
    };
    spin("y", "spin");
    spin("x", "spinX");
    spin("y", "spinY");
    spin("z", "spinZ");
    if (!Attr(node, "pulse").empty()) {
        const std::string base = NumAttr(node, "size", "1");
        out.js << "{const pulse=1+Math.sin((time+" << NumAttr(node, "pulsePhase", "0") << ")*" << NumAttr(node, "pulseSpeed", "1") << ")*" << NumAttr(node, "pulse", "0") << ";"
               << "const scale=" << base << "*(reduced?1:" << (Flag(Attr(node, "pulseInvert")) ? "(2-pulse)" : "pulse") << ");"
               << object << ".scale.set(scale,scale,1);}";
    }
    if (!Attr(node, "opacityPulse").empty()) {
        out.js << "if(" << object << ".material){" << object << ".material.opacity=(reduced?" << NumAttr(node, "opacity", "1") << ":"
               << NumAttr(node, "opacity", "1") << "+Math.sin(time*" << NumAttr(node, "opacitySpeed", "1") << "+" << NumAttr(node, "opacityPhase", "0")
               << ")*" << NumAttr(node, "opacityPulse", "0") << ");}";
    }
    out.js << "});\n";
}

void EmitNode(Out& out, const RoboXMLNode& node, const std::string& parent, const std::string& path) {
    const std::string kind = Lower(node.name);
    const std::string name = Attr(node, "name");
    const std::string full = name.empty() ? path : (path.empty() ? name : path + "." + name);
    if (kind == "field") {
        const std::string shaderName = Attr(node, "shader");
        if (shaderName.empty()) out.errors.push_back("Field needs Shader");
        out.js << "{const material=shaders[\"" << Js(shaderName) << "\"](ctx);material.transparent=true;material.depthWrite=false;"
               << "const mesh=new THREE.Mesh(new THREE.PlaneGeometry(2,2),material);mesh.frustumCulled=false;ctx.scene.add(mesh);}\n";
        return;
    }
    if (kind == "points" || kind == "sprite" || kind == "line") {
        const std::string object = Fresh(out);
        out.js << "{";
        if (kind == "points") {
            const int count = std::max(1, std::atoi(NumAttr(node, "count", "12").c_str()));
            const double inner = std::atof(NumAttr(node, "inner", "1.35").c_str());
            const double spread = std::atof(NumAttr(node, "spread", "1.5").c_str());
            out.js << "const positions=[];";
            if (Flag(Attr(node, "random"))) {
                out.js << "const count=" << count << ";const inner=" << Num(inner) << ";const spread=" << Num(spread) << ";"
                       << "for(let i=0;i<count;i++){const dir=new THREE.Vector3(Math.random()*2-1,Math.random()*2-1,Math.random()*2-1).normalize();"
                       << "const rad=inner+Math.random()*spread;positions.push(dir.x*rad,dir.y*rad,dir.z*rad);}";
            } else {
            std::uint32_t seed = static_cast<std::uint32_t>(std::atoi(NumAttr(node, "seed", "1337").c_str()));
            for (int i = 0; i < count; ++i) {
                seed = seed * 1664525u + 1013904223u;
                const double rx = (seed >> 8) / 16777216.0 * 2.0 - 1.0;
                seed = seed * 1664525u + 1013904223u;
                const double ry = (seed >> 8) / 16777216.0 * 2.0 - 1.0;
                seed = seed * 1664525u + 1013904223u;
                const double rz = (seed >> 8) / 16777216.0 * 2.0 - 1.0;
                const double length = std::sqrt(rx * rx + ry * ry + rz * rz);
                const double scale = length < 1e-6 ? 0 : (inner + ((seed >> 8) / 16777216.0) * spread) / length;
                out.js << "positions.push(" << Num(rx * scale) << "," << Num(ry * scale) << "," << Num(rz * scale) << ");";
            }
            }
            out.js << "const geometry=new THREE.BufferGeometry();geometry.setAttribute(\"position\",new THREE.Float32BufferAttribute(positions,3));"
                   << "const material=new THREE.PointsMaterial({color:\"" << Js(Attr(node, "color").empty() ? "#ffffff" : Attr(node, "color"))
                   << "\",size:" << NumAttr(node, "size", "0.02") << ",transparent:true,opacity:" << NumAttr(node, "opacity", "1")
                   << ",sizeAttenuation:true});";
        } else if (kind == "sprite") {
            out.js << "const canvas=document.createElement(\"canvas\");canvas.width=256;canvas.height=256;const paint=canvas.getContext(\"2d\");"
                   << "const glow=paint.createRadialGradient(128,128,0,128,128,128);";
            bool anyStop = false;
            for (const RoboXMLNode& stop : node.children) {
                if (Lower(stop.name) != "stop") continue;
                anyStop = true;
                out.js << "glow.addColorStop(" << NumAttr(stop, "offset", "0") << ",\"" << Js(Attr(stop, "color")) << "\");";
            }
            if (!anyStop) out.js << "glow.addColorStop(0,\"" << Js(Attr(node, "color").empty() ? "#ffffff" : Attr(node, "color")) << "\");glow.addColorStop(1,\"rgba(0,0,0,0)\");";
            out.js << "paint.fillStyle=glow;paint.fillRect(0,0,256,256);const texture=new THREE.CanvasTexture(canvas);texture.needsUpdate=true;"
                   << "const material=new THREE.SpriteMaterial({map:texture,transparent:true,opacity:" << NumAttr(node, "opacity", "1") << "});";
        } else {
            out.js << "const pts=[];const radius=" << NumAttr(node, "radius", "1") << ";const segments=" << NumAttr(node, "segments", "64")
                   << ";for(let i=0;i<segments;i++){const a=i/segments*Math.PI*2;pts.push(new THREE.Vector3(Math.cos(a)*radius,Math.sin(a)*radius,0));}"
                   << "const material=new THREE.LineBasicMaterial({color:\"" << Js(Attr(node, "color").empty() ? "#ffffff" : Attr(node, "color"))
                   << "\",transparent:true,opacity:" << NumAttr(node, "opacity", "1") << "});";
        }
        EmitSurfaceFlags(out.js, node, "material");
        const char* ctor = kind == "points" ? "new THREE.Points(geometry, material)" : kind == "sprite" ? "new THREE.Sprite(material)" : "new THREE.LineLoop(new THREE.BufferGeometry().setFromPoints(pts), material)";
        out.js << "const " << object << "=" << ctor << ";";
        if (!name.empty()) out.js << object << ".name=\"" << Js(name) << "\";";
        EmitTransform(out, object, node);
        if (!full.empty()) out.js << "remember(ctx,\"" << Js(full) << "\"," << object << ");";
        out.js << parent << ".add(" << object << ");\n";
        EmitMotion(out, object, node);
        out.js << "}\n";
        return;
    }
    if (kind == "group" || kind == "object") {
        const std::string object = Fresh(out);
        out.js << "{const " << object << "=new THREE.Group();";
        if (!name.empty()) out.js << object << ".name=\"" << Js(name) << "\";";
        EmitTransform(out, object, node);
        if (!full.empty()) out.js << "remember(ctx,\"" << Js(full) << "\"," << object << ");";
        out.js << parent << ".add(" << object << ");\n";
        if (!Attr(node, "span").empty() && !Attr(node, "cover").empty()) {
            out.js << "ctx.layouts.push((width,height)=>{const distance=Math.abs(ctx.camera.position.z-" << object << ".position.z);"
                   << "const visibleH=2*Math.tan(THREE.MathUtils.degToRad(ctx.camera.fov*0.5))*distance;"
                   << "const visibleW=visibleH*(width/Math.max(height,1));" << object << ".scale.x=(visibleW*" << Attr(node, "cover")
                   << ")/" << Attr(node, "span") << ";});\n";
        }
        EmitMotion(out, object, node);
        for (const RoboXMLNode& child : node.children) {
            if (Lower(child.name) == "transform") continue;
            EmitNode(out, child, object, full);
        }
        out.js << "}\n";
        return;
    }
    if (kind == "mesh") {
        EmitMesh(out, node, parent, path, "new THREE.Mesh(geometry, material)");
        return;
    }
    if (kind == "model") {
        out.uses.gltf = true;
        const std::string url = Publish(out, Attr(node, "source"), "models");
        out.js << "new GLTFLoader().load(\"" << Js(url) << "\",(gltf)=>{const model=gltf.scene;";
        if (!name.empty()) out.js << "model.name=\"" << Js(name) << "\";";
        EmitTransform(out, "model", node);
        if (!full.empty()) out.js << "remember(ctx,\"" << Js(full) << "\",model);";
        out.js << parent << ".add(model);model.traverse((child)=>{if(child.isMesh){child.castShadow=true;child.receiveShadow=true;}});});\n";
        return;
    }
    if (kind == "text3d") {
        out.uses.font = true;
        const std::string font = Publish(out, Attr(node, "font"), "fonts");
        if (Attr(node, "font").empty()) out.errors.push_back("Text3D needs Font pointing at a Three.js typeface json");
        out.js << "new FontLoader().load(\"" << Js(font) << "\",(font)=>{const geometry=new TextGeometry(" << "\"" << Js(Trim(node.text).empty() ? Attr(node, "text") : Trim(node.text))
               << "\",{font,size:" << (Attr(node, "size").empty() ? "0.4" : Attr(node, "size")) << ",depth:" << (Attr(node, "depth").empty() ? "0.08" : Attr(node, "depth"))
               << ",curveSegments:8});geometry.center();const material=" << MaterialExpr(out, Child(node, "material")) << ";const mesh=new THREE.Mesh(geometry,material);";
        EmitTransform(out, "mesh", node);
        if (!full.empty()) out.js << "remember(ctx,\"" << Js(full) << "\",mesh);";
        out.js << parent << ".add(mesh);});\n";
        return;
    }
    if (kind == "ground") {
        const std::string size = Attr(node, "size").empty() ? "200" : Attr(node, "size");
        const std::string material = Lower(Attr(node, "material")) == "studio-floor" && Child(node, "material") == nullptr
                                          ? "new THREE.MeshStandardMaterial({color:\"#1c1c1e\",roughness:0.45,metalness:0.05})"
                                          : MaterialExpr(out, Child(node, "material"));
        out.js << "{const mesh=new THREE.Mesh(new THREE.CircleGeometry(" << size << ",64)," << material
               << ");mesh.rotation.x=-Math.PI/2;mesh.receiveShadow=" << (Attr(node, "receiveShadow").empty() || Flag(Attr(node, "receiveShadow")) ? "true" : "false") << ";";
        EmitTransform(out, "mesh", node);
        if (!name.empty()) out.js << "remember(ctx,\"" << Js(full) << "\",mesh);";
        out.js << parent << ".add(mesh);}\n";
        return;
    }
    if (kind == "helper") {
        const std::string type = Lower(Attr(node, "type").empty() ? "grid" : Attr(node, "type"));
        if (type == "axes") out.js << parent << ".add(new THREE.AxesHelper(" << (Attr(node, "size").empty() ? "2" : Attr(node, "size")) << "));\n";
        else out.js << parent << ".add(new THREE.GridHelper(" << (Attr(node, "size").empty() ? "20" : Attr(node, "size")) << "," << (Attr(node, "divisions").empty() ? "20" : Attr(node, "divisions")) << "));\n";
        return;
    }
    if (kind == "sky") {
        out.uses.sky = true;
        out.js << "{const sky=new Sky();sky.scale.setScalar(450000);const u=sky.material.uniforms;u[\"turbidity\"].value="
               << (Attr(node, "turbidity").empty() ? "10" : Attr(node, "turbidity")) << ";u[\"rayleigh\"].value="
               << (Attr(node, "rayleigh").empty() ? "2" : Attr(node, "rayleigh")) << ";u[\"mieCoefficient\"].value="
               << (Attr(node, "mie").empty() ? "0.005" : Attr(node, "mie")) << ";u[\"mieDirectionalG\"].value=0.8;u[\"sunPosition\"].value.setFromSphericalCoords(1,"
               << (Attr(node, "elevation").empty() ? "0.3" : Attr(node, "elevation")) << "," << (Attr(node, "azimuth").empty() ? "0.2" : Attr(node, "azimuth"))
               << ");ctx.scene.add(sky);}\n";
        return;
    }
    if (kind == "curve") {
        out.uses.curve = true;
        std::vector<double> points;
        for (const RoboXMLNode& point : node.children) {
            if (Lower(point.name) != "point") continue;
            const Vec3 position = ParseVec(Attr(point, "position"), 0);
            points.push_back(position.x);
            points.push_back(position.y);
            points.push_back(position.z);
        }
        std::ostringstream list;
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (i) list << ",";
            list << Num(points[i]);
        }
        out.js << "ctx.curves.set(\"" << Js(name) << "\",curveOf([" << list.str() << "]," << (Flag(Attr(node, "closed")) ? "true" : "false") << "));\n";
    }
}

void EmitLights(Out& out, const RoboXMLNode& node) {
    for (const RoboXMLNode& child : node.children) {
        const std::string kind = Lower(child.name);
        std::string ctor;
        if (kind == "ambient" || kind == "ambientlight") ctor = "AmbientLight";
        else if (kind == "directional" || kind == "directionallight") ctor = "DirectionalLight";
        else if (kind == "point" || kind == "pointlight") ctor = "PointLight";
        else if (kind == "spot" || kind == "spotlight") ctor = "SpotLight";
        else if (kind == "hemisphere" || kind == "hemispherelight") ctor = "HemisphereLight";
        else if (kind == "arealight" || kind == "rectarea") {
            ctor = "RectAreaLight";
            out.uses.area = true;
        } else if (kind == "lighting") {
            EmitLights(out, child);
            continue;
        } else {
            continue;
        }
        const std::string color = Attr(child, "color").empty() ? "#ffffff" : Attr(child, "color");
        const std::string intensity = Attr(child, "intensity").empty() ? "1" : Attr(child, "intensity");
        out.js << "{const light=new THREE." << ctor << "(\"" << Js(color) << "\"," << intensity;
        if (ctor == "HemisphereLight") out.js << ",\"" << Js(Attr(child, "ground").empty() ? "#444444" : Attr(child, "ground")) << "\"";
        if (ctor == "RectAreaLight") out.js << "," << (Attr(child, "width").empty() ? "4" : Attr(child, "width")) << "," << (Attr(child, "height").empty() ? "4" : Attr(child, "height"));
        if (ctor == "SpotLight") out.js << "," << (Attr(child, "distance").empty() ? "0" : Attr(child, "distance")) << "," << (Attr(child, "angle").empty() ? "Math.PI/6" : Attr(child, "angle")) << "," << (Attr(child, "penumbra").empty() ? "0.2" : Attr(child, "penumbra")) << "," << (Attr(child, "decay").empty() ? "2" : Attr(child, "decay"));
        out.js << ");";
        const Vec3 position = ParseVec(Attr(child, "position"), 0);
        if (position.set) out.js << "light.position.set(" << Num(position.x) << "," << Num(position.y) << "," << Num(position.z) << ");";
        if (Flag(Attr(child, "castShadow")) || ctor == "DirectionalLight") out.js << "if(light.castShadow!==undefined)light.castShadow=true;";
        if (!Attr(child, "name").empty()) out.js << "remember(ctx,\"" << Js(Attr(child, "name")) << "\",light);";
        out.js << "ctx.scene.add(light);}\n";
    }
}

std::string ShaderTerm(const std::string& raw, const std::unordered_set<std::string>& inputs, bool color) {
    const std::string text = Trim(raw);
    if (text.empty()) return color ? "vec3(1.0)" : "0.0";
    if (inputs.count(text) != 0) return text;
    if (text.front() == '#') {
        try {
            const int rgb = std::stoi(text.substr(1), nullptr, 16);
            std::ostringstream out;
            out.setf(std::ios::fmtflags(0), std::ios::floatfield);
            out.precision(4);
            out << "vec3(" << ((rgb >> 16) & 255) / 255.0 << "," << ((rgb >> 8) & 255) / 255.0 << "," << (rgb & 255) / 255.0 << ")";
            return out.str();
        } catch (...) {
            return color ? "vec3(1.0)" : "0.0";
        }
    }
    return text;
}

void CollectLook(const RoboXMLNode& node, std::vector<const RoboXMLNode*>& wobbles, const RoboXMLNode*& color, const RoboXMLNode*& fade) {
    const std::string kind = Lower(node.name);
    if (kind == "wobble") {
        wobbles.push_back(&node);
        return;
    }
    if (kind == "color" && color == nullptr) color = &node;
    if (kind == "fade" && fade == nullptr) fade = &node;
    if (kind == "shader" || kind == "shape" || kind == "look") {
        for (const RoboXMLNode& child : node.children) CollectLook(child, wobbles, color, fade);
    }
}

std::string ShaderCall(const std::string& name) {
    const std::string key = Lower(name);
    if (key == "smooth" || key == "smoothstep") return "smoothstep";
    if (key == "abs" || key == "min" || key == "max" || key == "clamp" || key == "mix" || key == "length" || key == "distance" ||
        key == "dot" || key == "normalize" || key == "pow" || key == "floor" || key == "fract" || key == "sqrt" || key == "sin" ||
        key == "cos" || key == "tan" || key == "asin" || key == "acos" || key == "atan" || key == "sign" || key == "step" ||
        key == "reflect" || key == "refract" || key == "cross" || key == "mod" || key == "exp" || key == "log" || key == "radians" ||
        key == "degrees" || key == "vec2" || key == "vec3" || key == "vec4" || key == "mat2" || key == "mat3" || key == "mat4" ||
        key == "float" || key == "int" || key == "noise" || key == "sample" || key == "slopex" || key == "slopey" || key == "slope") {
        if (key == "noise") return "robixNoise";
        if (key == "sample") return "texture2D";
        if (key == "slopex") return "dFdx";
        if (key == "slopey") return "dFdy";
        if (key == "slope") return "fwidth";
        return key;
    }
    return {};
}

std::string ShaderWord(const std::string& name, bool vertex) {
    const std::string key = Lower(name);
    if (key == "projectionmatrix") return "projectionMatrix";
    if (key == "viewmatrix") return "viewMatrix";
    if (key == "modelmatrix") return "modelMatrix";
    if (key == "modelviewmatrix") return "modelViewMatrix";
    if (key == "normalmatrix") return "normalMatrix";
    if (key == "cameraposition") return "cameraPosition";
    if (vertex && key == "position") return "position";
    if (vertex && key == "normal") return "normal";
    if (vertex && key == "uv") return "uv";
    return name;
}

class ShaderExpr {
 public:
    ShaderExpr(const std::string& text, bool vertex) : text_(text), vertex_(vertex) {}

    std::string Run(std::string& error) {
        error_.clear();
        const std::string value = ParseChoice();
        Skip();
        if (!error_.empty()) {
            error = error_;
            return "0.0";
        }
        if (index_ != text_.size()) error = "could not read '" + text_ + "'";
        return value;
    }

 private:
    void Skip() {
        while (index_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[index_]))) ++index_;
    }

    std::string ParseChoice() {
        const std::string value = ParseCompare();
        Skip();
        if (index_ < text_.size() && text_[index_] == '?') {
            ++index_;
            const std::string yes = ParseChoice();
            Skip();
            if (index_ >= text_.size() || text_[index_] != ':') error_ = "missing :";
            else ++index_;
            const std::string no = ParseChoice();
            return "(" + value + "?" + yes + ":" + no + ")";
        }
        return value;
    }

    std::string ParseCompare() {
        const std::string value = ParseAdd();
        Skip();
        if (index_ + 1 < text_.size() && text_[index_] == '=' && text_[index_ + 1] == '=') return value + "==" + Take(2, &ShaderExpr::ParseAdd);
        if (index_ + 1 < text_.size() && text_[index_] == '!' && text_[index_ + 1] == '=') return value + "!=" + Take(2, &ShaderExpr::ParseAdd);
        if (index_ + 1 < text_.size() && text_[index_] == '<' && text_[index_ + 1] == '=') return value + "<=" + Take(2, &ShaderExpr::ParseAdd);
        if (index_ + 1 < text_.size() && text_[index_] == '>' && text_[index_ + 1] == '=') return value + ">=" + Take(2, &ShaderExpr::ParseAdd);
        if (index_ < text_.size() && text_[index_] == '<') return value + "<" + Take(1, &ShaderExpr::ParseAdd);
        if (index_ < text_.size() && text_[index_] == '>') return value + ">" + Take(1, &ShaderExpr::ParseAdd);
        return value;
    }

    std::string Take(int count, std::string (ShaderExpr::*next)()) {
        index_ += static_cast<std::size_t>(count);
        return (this->*next)();
    }

    std::string ParseAdd() {
        std::string value = ParseMul();
        for (;;) {
            Skip();
            if (index_ < text_.size() && (text_[index_] == '+' || text_[index_] == '-')) {
                const char op = text_[index_++];
                value += op;
                value += ParseMul();
            } else {
                break;
            }
        }
        return value;
    }

    std::string ParseMul() {
        std::string value = ParseUnary();
        for (;;) {
            Skip();
            if (index_ < text_.size() && (text_[index_] == '*' || text_[index_] == '/' || text_[index_] == '%')) {
                const char op = text_[index_++];
                value += op;
                value += ParseUnary();
            } else {
                break;
            }
        }
        return value;
    }

    std::string ParseUnary() {
        Skip();
        if (index_ < text_.size() && text_[index_] == '-') {
            ++index_;
            return "-" + ParseUnary();
        }
        return ParsePrimary();
    }

    std::string Swizzle(std::string value) {
        if (index_ < text_.size() && text_[index_] == '.') {
            ++index_;
            const std::size_t start = index_;
            while (index_ < text_.size() && std::isalpha(static_cast<unsigned char>(text_[index_]))) ++index_;
            std::string part = text_.substr(start, index_ - start);
            for (char& ch : part) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (part.empty()) error_ = "missing swizzle";
            value += "." + part;
        }
        return value;
    }

    std::string ParsePrimary() {
        Skip();
        if (index_ >= text_.size()) {
            error_ = "missing value";
            return "0.0";
        }
        if (text_[index_] == '(') {
            ++index_;
            const std::string value = ParseChoice();
            Skip();
            if (index_ < text_.size() && text_[index_] == ',') {
                std::string list = value;
                int count = 1;
                while (index_ < text_.size() && text_[index_] == ',') {
                    ++index_;
                    if (!list.empty()) list += ",";
                    list += ParseChoice();
                    ++count;
                    Skip();
                }
                if (count < 2 || count > 4) error_ = "a group holds 2, 3, or 4 numbers";
                if (index_ >= text_.size() || text_[index_] != ')') error_ = "missing )";
                else ++index_;
                return Swizzle("vec" + std::to_string(count) + "(" + list + ")");
            }
            if (index_ >= text_.size() || text_[index_] != ')') error_ = "missing )";
            else ++index_;
            return Swizzle("(" + value + ")");
        }
        if (std::isalpha(static_cast<unsigned char>(text_[index_])) || text_[index_] == '_') {
            const std::size_t start = index_;
            while (index_ < text_.size() && (std::isalnum(static_cast<unsigned char>(text_[index_])) || text_[index_] == '_')) ++index_;
            const std::string name = text_.substr(start, index_ - start);
            Skip();
            if (index_ < text_.size() && text_[index_] == '(') {
                ++index_;
                std::string args;
                Skip();
                if (index_ >= text_.size() || text_[index_] != ')') {
                    for (;;) {
                        if (!args.empty()) args += ",";
                        args += ParseChoice();
                        Skip();
                        if (index_ < text_.size() && text_[index_] == ',') {
                            ++index_;
                            continue;
                        }
                        break;
                    }
                }
                if (index_ >= text_.size() || text_[index_] != ')') error_ = "missing )";
                else ++index_;
                const std::string call = Lower(name) == "saturate" ? "clamp" : ShaderCall(name);
                if (call.empty()) error_ = "unknown shader function '" + name + "'";
                const std::string written = Lower(name) == "saturate" ? call + "(" + args + ",0.0,1.0)" : call + "(" + args + ")";
                return Swizzle(written);
            }
            return Swizzle(ShaderWord(name, vertex_));
        }
        const std::size_t start = index_;
        if (text_[index_] == '.') ++index_;
        while (index_ < text_.size() && (std::isdigit(static_cast<unsigned char>(text_[index_])) || text_[index_] == '.')) ++index_;
        if (start == index_) {
            error_ = "expected a number";
            return "0.0";
        }
        return Swizzle(text_.substr(start, index_ - start));
    }

    const std::string& text_;
    bool vertex_ = false;
    std::size_t index_ = 0;
    std::string error_;
};

int RankOfType(const std::string& type) {
    const std::string key = Lower(type);
    if (key == "vec2" || key == "vector2") return 2;
    if (key == "vec3" || key == "vector3" || key == "color") return 3;
    if (key == "vec4" || key == "vector4") return 4;
    return 1;
}

const char* TypeOfRank(int rank) {
    if (rank == 2) return "vec2";
    if (rank == 3) return "vec3";
    if (rank == 4) return "vec4";
    return "float";
}

std::string InputTypeOf(const RoboXMLNode& child) {
    const std::string written = Lower(Attr(child, "type"));
    const std::string source = Lower(Attr(child, "source"));
    if (written.empty()) {
        if (source == "resolution" || source == "stage" || source == "pointer") return "vec2";
        if (source == "texture" || source == "image" || source == "map") return "texture";
        const std::string value = Attr(child, "default").empty() ? Attr(child, "value") : Attr(child, "default");
        if (!Trim(value).empty() && Trim(value).front() == '#') return "color";
        const auto parts = ParseList(value);
        if (parts.size() == 2) return "vec2";
        if (parts.size() == 3) return "vec3";
        if (parts.size() == 4) return "vec4";
        return "float";
    }
    if (written == "vector2") return "vec2";
    if (written == "vector3") return "vec3";
    if (written == "vector4") return "vec4";
    if (written == "image" || written == "map") return "texture";
    return written;
}

class ShaderRank {
 public:
    ShaderRank(const std::string& text, bool vertex, const std::unordered_map<std::string, int>& names)
        : text_(text), vertex_(vertex), names_(names) {}

    int Run() {
        const int rank = ParseChoice();
        Skip();
        return rank;
    }

 private:
    void Skip() {
        while (index_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[index_]))) ++index_;
    }

    int ParseChoice() {
        const int value = ParseCompare();
        Skip();
        if (index_ < text_.size() && text_[index_] == '?') {
            ++index_;
            const int yes = ParseChoice();
            Skip();
            if (index_ < text_.size() && text_[index_] == ':') ++index_;
            const int no = ParseChoice();
            return yes == no ? yes : std::max(yes, no);
        }
        return value;
    }

    int ParseCompare() {
        const int value = ParseAdd();
        Skip();
        if (index_ + 1 < text_.size() && ((text_[index_] == '=' && text_[index_ + 1] == '=') || (text_[index_] == '!' && text_[index_ + 1] == '=') ||
                                          (text_[index_] == '<' && text_[index_ + 1] == '=') || (text_[index_] == '>' && text_[index_ + 1] == '='))) {
            index_ += 2;
            ParseAdd();
            return 1;
        }
        if (index_ < text_.size() && (text_[index_] == '<' || text_[index_] == '>')) {
            ++index_;
            ParseAdd();
            return 1;
        }
        return value;
    }

    int Combine(int left, int right) const {
        if (left == right) return left;
        if (left == 1) return right;
        if (right == 1) return left;
        return left;
    }

    int ParseAdd() {
        int value = ParseMul();
        for (;;) {
            Skip();
            if (index_ < text_.size() && (text_[index_] == '+' || text_[index_] == '-')) {
                ++index_;
                value = Combine(value, ParseMul());
            } else {
                break;
            }
        }
        return value;
    }

    int ParseMul() {
        int value = ParseUnary();
        for (;;) {
            Skip();
            if (index_ < text_.size() && (text_[index_] == '*' || text_[index_] == '/' || text_[index_] == '%')) {
                ++index_;
                value = Combine(value, ParseUnary());
            } else {
                break;
            }
        }
        return value;
    }

    int ParseUnary() {
        Skip();
        if (index_ < text_.size() && text_[index_] == '-') {
            ++index_;
            return ParseUnary();
        }
        return ParsePrimary();
    }

    int Swizzle(int rank) {
        if (index_ < text_.size() && text_[index_] == '.') {
            ++index_;
            int count = 0;
            while (index_ < text_.size() && std::isalpha(static_cast<unsigned char>(text_[index_]))) {
                ++index_;
                ++count;
            }
            if (count >= 1 && count <= 4) return count;
        }
        return rank;
    }

    int CallRank(const std::string& name, const std::vector<int>& args) const {
        const std::string key = Lower(name);
        const int first = args.empty() ? 1 : args.front();
        const int last = args.empty() ? 1 : args.back();
        if (key == "vec2") return 2;
        if (key == "vec3") return 3;
        if (key == "vec4") return 4;
        if (key == "float" || key == "int" || key == "length" || key == "distance" || key == "dot" || key == "noise") return 1;
        if (key == "sample") return 4;
        if (key == "smooth" || key == "smoothstep") return last;
        return first;
    }

    int ParsePrimary() {
        Skip();
        if (index_ >= text_.size()) return 1;
        if (text_[index_] == '(') {
            ++index_;
            const int value = ParseChoice();
            Skip();
            if (index_ < text_.size() && text_[index_] == ',') {
                int count = 1;
                while (index_ < text_.size() && text_[index_] == ',') {
                    ++index_;
                    ParseChoice();
                    ++count;
                    Skip();
                }
                if (index_ < text_.size() && text_[index_] == ')') ++index_;
                return Swizzle(std::min(std::max(count, 1), 4));
            }
            if (index_ < text_.size() && text_[index_] == ')') ++index_;
            return Swizzle(value);
        }
        if (std::isalpha(static_cast<unsigned char>(text_[index_])) || text_[index_] == '_') {
            const std::size_t start = index_;
            while (index_ < text_.size() && (std::isalnum(static_cast<unsigned char>(text_[index_])) || text_[index_] == '_')) ++index_;
            const std::string name = text_.substr(start, index_ - start);
            Skip();
            if (index_ < text_.size() && text_[index_] == '(') {
                ++index_;
                std::vector<int> args;
                Skip();
                if (index_ >= text_.size() || text_[index_] != ')') {
                    for (;;) {
                        args.push_back(ParseChoice());
                        Skip();
                        if (index_ < text_.size() && text_[index_] == ',') {
                            ++index_;
                            continue;
                        }
                        break;
                    }
                }
                if (index_ < text_.size() && text_[index_] == ')') ++index_;
                return Swizzle(CallRank(name, args));
            }
            const auto found = names_.find(name);
            const int rank = found == names_.end() ? 1 : found->second;
            return Swizzle(rank);
        }
        if (text_[index_] == '.') ++index_;
        while (index_ < text_.size() && (std::isdigit(static_cast<unsigned char>(text_[index_])) || text_[index_] == '.')) ++index_;
        return Swizzle(1);
    }

    const std::string& text_;
    bool vertex_;
    const std::unordered_map<std::string, int>& names_;
    std::size_t index_ = 0;
};

std::string ReadShaderExpr(const std::string& text, bool vertex, const std::string& shaderName, std::vector<std::string>& errors) {
    std::string error;
    const std::string value = ShaderExpr(text, vertex).Run(error);
    if (!error.empty()) errors.push_back(shaderName + " " + error);
    return value;
}

void EmitShaderStatements(const std::vector<RoboXMLNode>& children, bool fragment, const std::string& shaderName, std::ostringstream& body,
                          std::vector<std::string>& errors, bool& sawOutput, std::unordered_map<std::string, int>& ranks) {
    for (std::size_t index = 0; index < children.size(); ++index) {
        const RoboXMLNode& child = children[index];
        const std::string kind = Lower(child.name);
        if (kind == "else") {
            errors.push_back(shaderName + " Else needs an If before it");
            continue;
        }
        if (kind == "let" || kind == "set") {
            const std::string name = Attr(child, "name");
            const bool vary = Flag(Attr(child, "vary"));
            const std::string raw = Attr(child, "value");
            int rank = 1;
            if (!Attr(child, "type").empty()) rank = RankOfType(Attr(child, "type"));
            else if (!raw.empty()) rank = ShaderRank(raw, !fragment, ranks).Run();
            if (!name.empty() && kind == "let") ranks[name] = rank;
            if (name.empty()) errors.push_back(shaderName + " " + child.name + " needs Name");
            if (kind == "set" || vary) {
                if (!raw.empty()) body << name << "=" << ReadShaderExpr(raw, !fragment, shaderName, errors) << ";\n";
            } else {
                if (raw.empty()) errors.push_back(shaderName + " Let needs Value");
                body << TypeOfRank(rank) << " " << name << "=" << ReadShaderExpr(raw.empty() ? "0.0" : raw, !fragment, shaderName, errors) << ";\n";
            }
            continue;
        }
        if (kind == "repeat") {
            const std::string variable = Attr(child, "variable").empty() ? "i" : Attr(child, "variable");
            const int count = std::atoi(Attr(child, "count").empty() ? "1" : Attr(child, "count").c_str());
            if (count < 0 || count > 256) errors.push_back(shaderName + " Repeat count must be from 0 to 256");
            body << "for(int " << variable << "=0;" << variable << "<" << std::max(0, count) << ";" << variable << "++){\n";
            bool nested = false;
            EmitShaderStatements(child.children, fragment, shaderName, body, errors, nested, ranks);
            body << "}\n";
            if (nested) sawOutput = true;
            continue;
        }
        if (kind == "if") {
            if (Attr(child, "test").empty()) errors.push_back(shaderName + " If needs Test");
            body << "if(" << ReadShaderExpr(Attr(child, "test").empty() ? "false" : Attr(child, "test"), !fragment, shaderName, errors) << "){\n";
            bool nested = false;
            EmitShaderStatements(child.children, fragment, shaderName, body, errors, nested, ranks);
            body << "}";
            if (index + 1 < children.size() && Lower(children[index + 1].name) == "else") {
                body << "else{\n";
                EmitShaderStatements(children[index + 1].children, fragment, shaderName, body, errors, nested, ranks);
                body << "}";
                ++index;
            }
            body << "\n";
            if (nested) sawOutput = true;
            continue;
        }
        if (kind == "discard") {
            if (Attr(child, "test").empty()) body << "discard;\n";
            else body << "if(" << ReadShaderExpr(Attr(child, "test"), !fragment, shaderName, errors) << ") discard;\n";
            continue;
        }
        if (kind == "output") {
            sawOutput = true;
            if (fragment && !Attr(child, "value").empty()) {
                body << "gl_FragColor=" << ReadShaderExpr(Attr(child, "value"), false, shaderName, errors) << ";\n";
            } else if (fragment) {
                const std::string color = ReadShaderExpr(Attr(child, "color").empty() ? "(1.0, 1.0, 1.0)" : Attr(child, "color"), false, shaderName, errors);
                const std::string alpha = ReadShaderExpr(Attr(child, "alpha").empty() ? "1.0" : Attr(child, "alpha"), false, shaderName, errors);
                body << "gl_FragColor=vec4(" << color << "," << alpha << ");\n";
            } else {
                if (Attr(child, "position").empty()) errors.push_back(shaderName + " vertex Output needs Position");
                body << "gl_Position=" << ReadShaderExpr(Attr(child, "position").empty() ? "vec4(0.0)" : Attr(child, "position"), true, shaderName, errors) << ";\n";
            }
        }
    }
}

void CollectVary(const std::vector<RoboXMLNode>& children, std::ostringstream& vary, std::unordered_set<std::string>& seen,
                  std::unordered_map<std::string, int>& ranks, bool vertex) {
    for (const RoboXMLNode& child : children) {
        const std::string kind = Lower(child.name);
        if (kind == "let") {
            const std::string name = Attr(child, "name");
            const std::string raw = Attr(child, "value");
            int rank = 1;
            if (!Attr(child, "type").empty()) rank = RankOfType(Attr(child, "type"));
            else if (!raw.empty()) rank = ShaderRank(raw, vertex, ranks).Run();
            if (!name.empty()) ranks[name] = rank;
            if (Flag(Attr(child, "vary")) && !name.empty() && seen.insert(name).second) vary << "varying " << TypeOfRank(rank) << " " << name << ";\n";
        } else if (kind == "repeat" || kind == "if" || kind == "else") {
            CollectVary(child.children, vary, seen, ranks, vertex);
        }
    }
}

std::string CompileStage(const RoboXMLNode& block, const std::string& declarations, bool fragment, const std::string& shaderName,
                         std::vector<std::string>& errors, std::unordered_map<std::string, int> ranks) {
    std::ostringstream body;
    if (fragment) body << "precision highp float;\n";
    body << declarations;
    body << "void main(){\n";
    if (fragment) {
        body << "vec2 Pixel=gl_FragCoord.xy;\n";
        ranks["Pixel"] = 2;
    } else {
        ranks["Position"] = 3;
        ranks["Normal"] = 3;
        ranks["Uv"] = 2;
    }
    bool sawOutput = false;
    EmitShaderStatements(block.children, fragment, shaderName, body, errors, sawOutput, ranks);
    if (!sawOutput) {
        errors.push_back(shaderName + " needs Output");
        body << (fragment ? "gl_FragColor=vec4(1.0);\n" : "gl_Position=vec4(0.0);\n");
    }
    body << "}";
    return body.str();
}

std::string ShaderFunction(const RoboXMLNode& shader, Out& out) {
    std::vector<std::string>& errors = out.errors;
    std::ostringstream uniforms;
    std::ostringstream decls;
    bool time = false;
    bool live = false;
    bool trackPointer = false;
    std::ostringstream liveWrite;
    std::unordered_set<std::string> inputs;
    std::unordered_map<std::string, int> ranks;
    for (const RoboXMLNode& child : shader.children) {
        const std::string childKind = Lower(child.name);
        if (childKind != "uniform" && childKind != "input") continue;
        const std::string name = Attr(child, "name");
        const std::string type = InputTypeOf(child);
        const std::string value = Attr(child, "default").empty() ? Attr(child, "value") : Attr(child, "default");
        const std::string source = Lower(Attr(child, "source"));
        if (Lower(name) == "time" && source.empty()) time = true;
        inputs.insert(name);
        ranks[name] = RankOfType(type);
        if (type == "color") {
            decls << "uniform vec3 " << name << ";\n";
            uniforms << "\"" << Js(name) << "\":{value:new THREE.Color(\"" << Js(value.empty() ? "#ffffff" : value) << "\")},";
        } else if (type == "vec2") {
            const auto parts = ParseList(value.empty() ? "0,0" : value);
            decls << "uniform vec2 " << name << ";\n";
            uniforms << "\"" << Js(name) << "\":{value:new THREE.Vector2(" << (parts.size() > 0 ? Num(parts[0]) : "0") << ","
                     << (parts.size() > 1 ? Num(parts[1]) : "0") << ")},";
        } else if (type == "vec3") {
            const auto parts = ParseList(value.empty() ? "0,0,0" : value);
            decls << "uniform vec3 " << name << ";\n";
            uniforms << "\"" << Js(name) << "\":{value:new THREE.Vector3(" << (parts.size() > 0 ? Num(parts[0]) : "0") << ","
                     << (parts.size() > 1 ? Num(parts[1]) : "0") << "," << (parts.size() > 2 ? Num(parts[2]) : "0") << ")},";
        } else if (type == "texture" || type == "map" || type == "sampler2d") {
            const std::string file = Attr(child, "source").empty() ? value : Attr(child, "source");
            const std::string url = Publish(out, file, "images");
            decls << "uniform sampler2D " << name << ";\n";
            uniforms << "\"" << Js(name) << "\":{value:(()=>{const tex=new THREE.TextureLoader().load(\"" << Js(url) << "\");tex.colorSpace=THREE.SRGBColorSpace;return tex;})()},";
        } else {
            decls << "uniform float " << name << ";\n";
            uniforms << "\"" << Js(name) << "\":{value:" << (value.empty() ? "0" : value) << "},";
        }
        if (source == "time") {
            live = true;
            liveWrite << "material.uniforms[\"" << Js(name) << "\"].value=reduced?" << (Attr(child, "hold").empty() ? "0" : Attr(child, "hold"))
                      << ":clock.elapsed;";
        } else if (source == "enter") {
            live = true;
            liveWrite << "{const span=" << (Attr(child, "duration").empty() ? "0.7" : Attr(child, "duration"))
                      << ";const t=Math.min(1,Math.max(0,(clock?clock.enter:time)/span));material.uniforms[\"" << Js(name)
                      << "\"].value=reduced?1:(1-Math.pow(1-t,3));}";
        } else if (source == "resolution") {
            live = true;
            liveWrite << "material.uniforms[\"" << Js(name) << "\"].value.set(ctx.renderer.domElement.width,ctx.renderer.domElement.height);";
        } else if (source == "stage") {
            live = true;
            liveWrite << "{const ratio=ctx.renderer.getPixelRatio();const host=ctx.host&&ctx.host.parentElement||ctx.host;const w=Math.max(((host&&host.clientWidth)||ctx.renderer.domElement.clientWidth)*ratio,1);const h=Math.max(((host&&host.clientHeight)||ctx.renderer.domElement.clientHeight)*ratio,1);material.uniforms[\""
                      << Js(name) << "\"].value.set(w,h);}";
        } else if (source == "pointer") {
            live = true;
            trackPointer = true;
            liveWrite << "{const u=material.uniforms[\"" << Js(name) << "\"].value;if(reduced)u.set(0,0);else{const follow="
                      << (Attr(child, "smooth").empty() ? "0.05" : Attr(child, "smooth")) << ";u.x+=(clock.pointer.x-u.x)*follow;u.y+=(-clock.pointer.y-u.y)*follow;}}";
        }
    }
    const RoboXMLNode* glsl = nullptr;
    const RoboXMLNode* surface = nullptr;
    const RoboXMLNode* fragmentProgram = nullptr;
    const RoboXMLNode* vertexProgram = nullptr;
    for (const RoboXMLNode& child : shader.children) {
        const std::string childKind = Lower(child.name);
        if (childKind == "glsl") glsl = &child;
        if (childKind == "surface") surface = &child;
        if (childKind == "fragment") fragmentProgram = &child;
        if (childKind == "vertex") vertexProgram = &child;
    }
    std::vector<const RoboXMLNode*> wobbles;
    const RoboXMLNode* lookColor = nullptr;
    const RoboXMLNode* fade = nullptr;
    CollectLook(shader, wobbles, lookColor, fade);
    const bool coded = fragmentProgram != nullptr || vertexProgram != nullptr;
    const bool friendly = !coded && glsl == nullptr && (!wobbles.empty() || lookColor != nullptr || fade != nullptr);
    const std::string shaderName = Attr(shader, "name").empty() ? "Shader" : Attr(shader, "name");
    std::string fragment;
    std::string friendlyVertex;
    if (coded) {
        std::unordered_set<std::string> varied;
        std::ostringstream varyings;
        if (vertexProgram != nullptr) {
            auto varyRanks = ranks;
            varyRanks["Position"] = 3;
            varyRanks["Normal"] = 3;
            varyRanks["Uv"] = 2;
            CollectVary(vertexProgram->children, varyings, varied, varyRanks, true);
        }
        if (fragmentProgram != nullptr) {
            auto varyRanks = ranks;
            varyRanks["Pixel"] = 2;
            CollectVary(fragmentProgram->children, varyings, varied, varyRanks, false);
        }
        const std::string header = decls.str() + varyings.str();
        if (fragmentProgram != nullptr) fragment = CompileStage(*fragmentProgram, header, true, shaderName, errors, ranks);
        if (Lower(Attr(shader, "space")) == "screen") friendlyVertex = "void main(){gl_Position=vec4(position.xy,0.0,1.0);}";
        else if (vertexProgram != nullptr) friendlyVertex = CompileStage(*vertexProgram, header, false, shaderName, errors, ranks);
    } else if (friendly) {
        friendlyVertex = "varying vec3 vNormal;varying vec3 vView;varying float vNoise;\nvoid main(){vec4 mv=modelViewMatrix*vec4(position,1.0);vView=mv.xyz;vNormal=normalize(normalMatrix*normal);vNoise=0.0;gl_Position=projectionMatrix*mv;}";
        std::ostringstream fragmentBody;
        fragmentBody << decls.str() << "varying vec3 vNormal;varying vec3 vView;varying float vNoise;\nvoid main(){\n";
        fragmentBody << "float edge=pow(clamp(1.0-abs(dot(normalize(vNormal),normalize(-vView))),0.0,1.0),";
        if (lookColor != nullptr && !Attr(*lookColor, "sharpness").empty()) fragmentBody << ShaderTerm(Attr(*lookColor, "sharpness"), inputs, false);
        else fragmentBody << "2.0";
        fragmentBody << ");\n";
        if (lookColor == nullptr) fragmentBody << "vec3 color=vec3(1.0);\n";
        else if (!Attr(*lookColor, "value").empty() || !Attr(*lookColor, "color").empty()) {
            const std::string paint = Attr(*lookColor, "value").empty() ? Attr(*lookColor, "color") : Attr(*lookColor, "value");
            fragmentBody << "vec3 color=" << ShaderTerm(paint, inputs, true) << ";\n";
        } else {
            const std::string edgeColor = ShaderTerm(Attr(*lookColor, "edge").empty() ? "#ffffff" : Attr(*lookColor, "edge"), inputs, true);
            const std::string centerColor = ShaderTerm(Attr(*lookColor, "center").empty() ? "#888888" : Attr(*lookColor, "center"), inputs, true);
            const std::string by = Lower(Attr(*lookColor, "by").empty() ? "edge" : Attr(*lookColor, "by"));
            const char* mixBy = by == "noise" ? "clamp(vNoise,-1.0,1.0)*0.5+0.5" : "edge";
            fragmentBody << "vec3 color=mix(" << centerColor << "," << edgeColor << "," << mixBy << ");\n";
        }
        if (fade == nullptr) fragmentBody << "float alpha=1.0;\n";
        else {
            const std::string base = ShaderTerm(Attr(*fade, "base").empty() ? "0.0" : Attr(*fade, "base"), inputs, false);
            const std::string edgeFade = ShaderTerm(Attr(*fade, "edge").empty() ? "0.0" : Attr(*fade, "edge"), inputs, false);
            const std::string limit = ShaderTerm(Attr(*fade, "limit").empty() ? "1.0" : Attr(*fade, "limit"), inputs, false);
            fragmentBody << "float alpha=clamp(" << base << "+edge*" << edgeFade << ",0.0," << limit << ");\n";
        }
        const std::string brightness = fade != nullptr && !Attr(*fade, "brightness").empty() ? ShaderTerm(Attr(*fade, "brightness"), inputs, false) : "1.0";
        fragmentBody << "gl_FragColor=vec4(color*" << brightness << ",alpha);}";
        fragment = fragmentBody.str();
    } else if (glsl != nullptr) {
        errors.push_back(shaderName + " cannot use raw GLSL. Describe it with Let and Output.");
        fragment = "void main(){gl_FragColor=vec4(1.0);}";
    } else {
        float power = 3;
        float bias = 0;
        float scale = 4;
        float speed = 0.5;
        std::string from = "ColorA";
        std::string to = "ColorB";
        if (surface != nullptr) {
            for (const RoboXMLNode& child : surface->children) {
                const std::string kind = Lower(child.name);
                if (kind == "fresnel") {
                    if (!Attr(child, "power").empty()) power = std::stod(Attr(child, "power"));
                    if (!Attr(child, "bias").empty()) bias = std::stod(Attr(child, "bias"));
                } else if (kind == "noise") {
                    if (!Attr(child, "scale").empty()) scale = std::stod(Attr(child, "scale"));
                    if (!Attr(child, "speed").empty()) speed = std::stod(Attr(child, "speed"));
                } else if (kind == "gradient") {
                    auto colorName = [](std::string value, const char* fallback) {
                        value = Trim(value);
                        if (value.size() >= 2 && value.front() == '{' && value.back() == '}') value = Trim(value.substr(1, value.size() - 2));
                        if (value.empty()) return std::string(fallback);
                        return value;
                    };
                    from = colorName(Attr(child, "from"), "ColorA");
                    to = colorName(Attr(child, "to"), "ColorB");
                }
            }
        }
        if (!time) {
            decls << "uniform float Time;\n";
            uniforms << "\"Time\":{value:0},";
            time = true;
        }
        auto glslColor = [](std::string token) {
            token = Trim(token);
            if (token.size() == 7 && token.front() == '#') {
                try {
                    const int rgb = std::stoi(token.substr(1), nullptr, 16);
                    std::ostringstream color;
                    color.setf(std::ios::fmtflags(0), std::ios::floatfield);
                    color.precision(4);
                    color << "vec3(" << ((rgb >> 16) & 255) / 255.0 << "," << ((rgb >> 8) & 255) / 255.0 << "," << (rgb & 255) / 255.0 << ")";
                    return color.str();
                } catch (...) {
                    return std::string("vec3(1.0)");
                }
            }
            const bool named = !token.empty() && (std::isalpha(static_cast<unsigned char>(token.front())) || token.front() == '_');
            if (!named) return std::string("vec3(1.0)");
            return token;
        };
        from = glslColor(from);
        to = glslColor(to);
        fragment = decls.str() +
                   "varying vec3 vN;varying vec3 vW;\n"
                   "float h(vec3 p){return fract(sin(dot(p,vec3(127.1,311.7,74.7)))*43758.5453);}\n"
                   "float nse(vec3 p){vec3 i=floor(p);vec3 f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(mix(h(i),h(i+vec3(1,0,0)),f.x),mix(h(i+vec3(0,1,0)),h(i+vec3(1,1,0)),f.x),f.y),mix(mix(h(i+vec3(0,0,1)),h(i+vec3(1,0,1)),f.x),mix(h(i+vec3(0,1,1)),h(i+vec3(1,1,1)),f.x),f.y),f.z);}\n"
                   "void main(){vec3 viewDir=normalize(cameraPosition-vW);float fresnel=" +
                   Num(bias) + "+pow(clamp(1.0-max(dot(normalize(vN),viewDir),0.0),0.0,1.0)," + Num(power) +
                   ");float noise=nse(vW*" + Num(scale) + "+Time*" + Num(speed) + ");vec3 color=mix(" + from + "," + to +
                   ",fresnel);color+=noise*0.12;gl_FragColor=vec4(color,1.0);}";
    }
    std::string vertex = friendlyVertex;
    if (vertex.empty()) {
        vertex = "varying vec3 vN;varying vec3 vW;void main(){vec4 world=modelMatrix*vec4(position,1.0);vW=world.xyz;vN=normalize(mat3(modelMatrix)*normal);gl_Position=projectionMatrix*viewMatrix*world;}";
    }
    const char* noiseLibrary =
        "float robixHash(vec3 q){return fract(sin(dot(q,vec3(127.1,311.7,74.7)))*43758.5453);}\n"
        "float robixNoise(vec3 p){vec3 i=floor(p);vec3 f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(mix(robixHash(i),robixHash(i+vec3(1.0,0.0,0.0)),f.x),mix(robixHash(i+vec3(0.0,1.0,0.0)),robixHash(i+vec3(1.0,1.0,0.0)),f.x),f.y),mix(mix(robixHash(i+vec3(0.0,0.0,1.0)),robixHash(i+vec3(1.0,0.0,1.0)),f.x),mix(robixHash(i+vec3(0.0,1.0,1.0)),robixHash(i+vec3(1.0,1.0,1.0)),f.x),f.y),f.z);}\n";
    auto injectNoise = [&](std::string& source) {
        if (source.find("robixNoise(") == std::string::npos) return;
        const auto main = source.find("void main");
        source.insert(main == std::string::npos ? source.size() : main, noiseLibrary);
    };
    injectNoise(vertex);
    injectNoise(fragment);
    if (fragment.find("dFdx(") != std::string::npos || fragment.find("dFdy(") != std::string::npos || fragment.find("fwidth(") != std::string::npos) {
        fragment.insert(0, "#extension GL_OES_standard_derivatives : enable\n");
    }
    std::ostringstream fn;
    fn << "function(ctx){const material=new THREE.ShaderMaterial({uniforms:{" << uniforms.str()
       << "},vertexShader:\"" << Js(vertex) << "\",fragmentShader:\"" << Js(fragment) << "\"});";
    if (live) {
        const bool whenVisible = Lower(Attr(shader, "when")) == "visible";
        fn << "if(!ctx.field){ctx.field={elapsed:0,enter:0,on:" << (whenVisible ? "false" : "true") << ",pointer:new THREE.Vector2()};"
           << "const target=ctx.host&&ctx.host.parentElement||ctx.host;if(target){";
        if (trackPointer) {
            fn << "target.addEventListener(\"pointermove\",(event)=>{const rect=target.getBoundingClientRect();ctx.field.pointer.set(((event.clientX-rect.left)/Math.max(rect.width,1)-0.5)*2,((event.clientY-rect.top)/Math.max(rect.height,1)-0.5)*2);});";
        }
        if (whenVisible) {
            fn << "const observer=new IntersectionObserver((entries)=>{const hit=entries.some((entry)=>entry.isIntersecting&&entry.intersectionRatio>=0.1);if(!hit){ctx.field.elapsed=0;ctx.field.enter=0;}ctx.field.on=hit;},{threshold:[0,0.1,0.2,0.4]});observer.observe(target);";
        }
        fn << "}ctx.frames.push((dt)=>{const clock=ctx.field;if(clock.on&&!window.matchMedia(\"(prefers-reduced-motion: reduce)\").matches){clock.elapsed+=dt;clock.enter+=dt;}});}";
        fn << "ctx.frames.push((dt,time)=>{const reduced=window.matchMedia(\"(prefers-reduced-motion: reduce)\").matches;const clock=ctx.field;"
           << liveWrite.str() << "});";
    }
    if (time) fn << "ctx.frames.push((dt,time)=>{material.uniforms.Time.value=time;});";
    fn << "return material;}";
    return fn.str();
}

void EmitTracks(Out& out, const RoboXMLNode& timeline) {
    const std::string timelineName = Attr(timeline, "name");
    const bool gated = out.played.count(timelineName) != 0 || out.played.count(Lower(timelineName)) != 0;
    for (const RoboXMLNode& track : timeline.children) {
        if (Lower(track.name) != "track") continue;
        out.js << "ctx.tracks.push({name:\"" << Js(timelineName) << "\",target:\"" << Js(Attr(track, "target")) << "\",property:\""
               << Js(Lower(Attr(track, "property"))) << "\",duration:" << (Attr(timeline, "duration").empty() ? "0" : Attr(timeline, "duration"))
               << ",loop:" << (Flag(Attr(timeline, "loop")) ? "true" : "false") << ",gated:" << (gated ? "true" : "false") << ",keys:[";
        bool first = true;
        bool loop = Flag(Attr(timeline, "loop"));
        for (const RoboXMLNode& key : track.children) {
            if (Lower(key.name) == "loop") loop = Flag(Attr(key, "enabled").empty() ? "true" : Attr(key, "enabled"));
            if (Lower(key.name) != "keyframe") continue;
            if (!first) out.js << ",";
            first = false;
            const auto values = ParseList(Attr(key, "value"));
            out.js << "{t:" << (Attr(key, "time").empty() ? "0" : Attr(key, "time")) << ",e:\"" << Js(Lower(Attr(key, "easing").empty() ? "linear" : Attr(key, "easing")))
                   << "\",v:[";
            if (values.empty()) out.js << "0";
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i) out.js << ",";
                out.js << Num(values[i]);
            }
            out.js << "]}";
        }
        out.js << "]});\n";
        if (loop) out.js << "ctx.tracks[ctx.tracks.length-1].loop=true;\n";
    }
}

void EmitPhysics(Out& out, const RoboXMLNode& node, const RoboXMLNode& sceneRoot, std::vector<std::string>& bodies) {
    const std::string kind = Lower(node.name);
    if (kind == "physicsbody" || (kind == "body" && !Attr(node, "type").empty())) {
        bodies.push_back(Attr(node, "target"));
    }
    if (kind == "physics") {
        out.uses.rapier = true;
        const Vec3 gravity = ParseVec(Attr(node, "gravity").empty() ? "0,-9.81,0" : Attr(node, "gravity"), 0);
        out.js << "ctx.frames.push(()=>{});\n";
        out.js << "(async()=>{const RAPIER=await import(\"https://cdn.jsdelivr.net/npm/@dimforge/rapierx3d-compat@0.14.0/+esm\");await RAPIER.init();const world=new RAPIER.World({x:"
               << Num(gravity.x) << ",y:" << Num(gravity.y) << ",z:" << Num(gravity.z) << "});const bodies=[];\n";
        const auto emitBody = [&](const RoboXMLNode& body, const std::string& target) {
            const std::string type = Lower(Attr(body, "type").empty() ? "dynamic" : Attr(body, "type"));
            const char* desc = type == "fixed" ? "fixed" : type == "kinematic" ? "kinematicPositionBased" : "dynamic";
            std::string hx = "0.5", hy = "0.5", hz = "0.5", shape = "cuboid";
            std::string mass = "1", restitution = "0.2", friction = "0.7", gravityScale = "1";
            for (const RoboXMLNode& child : body.children) {
                const std::string childKind = Lower(child.name);
                if (childKind == "collider") {
                    shape = Lower(Attr(child, "type").empty() ? "cuboid" : Attr(child, "type"));
                    const Vec3 size = ParseVec(Attr(child, "size"), 0.5);
                    hx = Num(size.x);
                    hy = Num(size.y);
                    hz = Num(size.z);
                    if (!Attr(child, "radius").empty()) hx = Attr(child, "radius");
                } else if (childKind == "mass") mass = Attr(child, "value").empty() ? "1" : Attr(child, "value");
                else if (childKind == "restitution") restitution = Attr(child, "value").empty() ? "0.2" : Attr(child, "value");
                else if (childKind == "friction") friction = Attr(child, "value").empty() ? "0.7" : Attr(child, "value");
                else if (childKind == "gravity") gravityScale = Flag(Attr(child, "enabled").empty() ? "true" : Attr(child, "enabled")) ? "1" : "0";
            }
            out.js << "{const mesh=ctx.objects.get(\"" << Js(target) << "\");if(mesh){ctx.scene.attach(mesh);const body=world.createRigidBody(RAPIER.RigidBodyDesc."
                   << desc << "().setTranslation(mesh.position.x,mesh.position.y,mesh.position.z));body.setGravityScale(" << gravityScale << ");let desc;";
            if (shape == "ball" || shape == "sphere") out.js << "desc=RAPIER.ColliderDesc.ball(" << hx << ");";
            else if (shape == "capsule") out.js << "desc=RAPIER.ColliderDesc.capsule(" << hy << "," << hx << ");";
            else if (shape == "cylinder") out.js << "desc=RAPIER.ColliderDesc.cylinder(" << hy << "," << hx << ");";
            else out.js << "desc=RAPIER.ColliderDesc.cuboid(" << hx << "," << hy << "," << hz << ");";
            out.js << "desc.setRestitution(" << restitution << ").setFriction(" << friction
                   << ");world.createCollider(desc,body);";
            if (type == "dynamic") out.js << "body.setAdditionalMass(" << mass << ",true);";
            out.js << "bodies.push({mesh,body});}}\n";
        };
        std::function<void(const RoboXMLNode&, const std::string&, bool)> walk = [&](const RoboXMLNode& current, const std::string& path, bool root) {
            const std::string currentName = root ? std::string{} : Attr(current, "name");
            const std::string next = currentName.empty() ? path : (path.empty() ? currentName : path + "." + currentName);
            for (const RoboXMLNode& child : current.children) {
                const std::string childKind = Lower(child.name);
                const bool body = childKind == "physicsbody" || (childKind == "body" && !Attr(child, "type").empty());
                if (body) {
                    const std::string target = Attr(child, "target");
                    if (target.empty() && next.empty()) out.errors.push_back("PhysicsBody needs Target or a named parent");
                    else emitBody(child, target.empty() ? next : target);
                } else {
                    walk(child, next, false);
                }
            }
        };
        walk(sceneRoot, "", true);
        for (const RoboXMLNode& child : node.children) {
            if (Lower(child.name) == "physicsbody" && !Attr(child, "target").empty()) {
                /* already walked */
            }
        }
        out.js << "ctx.physics={step(){world.step();for(const item of bodies){const p=item.body.translation();const r=item.body.rotation();item.mesh.position.set(p.x,p.y,p.z);item.mesh.quaternion.set(r.x,r.y,r.z,r.w);}}};})();\n";
        return;
    }
}

std::string BuildScene(const Prepared& prepared, const RoboXMLNode& scene, std::string& uiHtml, std::vector<std::string>& errors) {
    Out out;
    out.degrees = prepared.degrees;
    out.directory = prepared.directory;
    out.outRoot = gOutRoot;
    out.document = &prepared.root;
    std::function<void(const RoboXMLNode&)> collectPlays = [&](const RoboXMLNode& node) {
        if (Lower(node.name) == "play") out.played.insert(Attr(node, "animation").empty() ? Attr(node, "name") : Attr(node, "animation"));
        for (const RoboXMLNode& child : node.children) collectPlays(child);
    };
    collectPlays(scene);
    out.js << "const shaders={";
    for (const RoboXMLNode& child : prepared.root.children) {
        if (Lower(child.name) != "shader") continue;
        out.js << "\"" << Js(Attr(child, "name")) << "\":" << ShaderFunction(child, out) << ",";
    }
    for (const RoboXMLNode& child : scene.children) {
        if (Lower(child.name) == "shader") out.js << "\"" << Js(Attr(child, "name")) << "\":" << ShaderFunction(child, out) << ",";
    }
    out.js << "};\n";
    if (!Attr(scene, "background").empty()) out.js << "ctx.scene.background=new THREE.Color(\"" << Js(Attr(scene, "background")) << "\");\n";
    if (const RoboXMLNode* fog = Child(scene, "fog")) {
        const std::string color = Attr(*fog, "color").empty() ? "#aaaaaa" : Attr(*fog, "color");
        if (Lower(Attr(*fog, "type")) == "linear") {
            out.js << "ctx.scene.fog=new THREE.Fog(\"" << Js(color) << "\"," << (Attr(*fog, "near").empty() ? "1" : Attr(*fog, "near")) << ","
                   << (Attr(*fog, "far").empty() ? "50" : Attr(*fog, "far")) << ");\n";
        } else {
            out.js << "ctx.scene.fog=new THREE.FogExp2(\"" << Js(color) << "\"," << (Attr(*fog, "density").empty() ? "0.02" : Attr(*fog, "density")) << ");\n";
        }
    }
    if (const RoboXMLNode* renderer = Child(scene, "renderer")) {
        if (Flag(Attr(*renderer, "shadows"))) out.js << "ctx.renderer.shadowMap.enabled=true;ctx.renderer.shadowMap.type=THREE.PCFSoftShadowMap;\n";
        const std::string tone = Lower(Attr(*renderer, "toneMapping"));
        if (tone == "aces") out.js << "ctx.renderer.toneMapping=THREE.ACESFilmicToneMapping;\n";
        else if (tone == "reinhard") out.js << "ctx.renderer.toneMapping=THREE.ReinhardToneMapping;\n";
        else if (tone == "cineon") out.js << "ctx.renderer.toneMapping=THREE.CineonToneMapping;\n";
        else if (tone == "linear" || tone == "none") out.js << "ctx.renderer.toneMapping=THREE.LinearToneMapping;\n";
        if (!Attr(*renderer, "exposure").empty()) out.js << "ctx.renderer.toneMappingExposure=" << Attr(*renderer, "exposure") << ";\n";
    }
    const RoboXMLNode* camera = Child(scene, "camera");
    std::string fov = "45", near = "0.1", far = "1000", position = "0,2,6", look = "0,0,0", type = "perspective";
    if (camera != nullptr) {
        const std::string preset = Lower(Attr(*camera, "preset"));
        if (preset == "product-showcase") {
            fov = "38";
            position = "0,2.5,9";
            look = "0,1,0";
        } else if (preset == "front") position = "0,0,6";
        else if (preset == "top") position = "0,8,0.001";
        else if (preset == "side") position = "6,1,0";
        if (!Attr(*camera, "fov").empty()) fov = Attr(*camera, "fov");
        if (!Attr(*camera, "near").empty()) near = Attr(*camera, "near");
        if (!Attr(*camera, "far").empty()) far = Attr(*camera, "far");
        if (!Attr(*camera, "position").empty()) position = Attr(*camera, "position");
        if (!Attr(*camera, "lookAt").empty()) look = Attr(*camera, "lookAt");
        if (!Attr(*camera, "type").empty()) type = Lower(Attr(*camera, "type"));
    }
    const Vec3 eye = ParseVec(position, 0);
    const Vec3 at = ParseVec(look, 0);
    if (type == "orthographic") {
        const std::string span = camera && !Attr(*camera, "size").empty() ? Attr(*camera, "size") : "6";
        out.js << "ctx.camera=new THREE.OrthographicCamera(-1,1,1,-1," << near << "," << far << ");ctx.camera.userData.span=" << span << ";\n";
    } else {
        out.js << "ctx.camera=new THREE.PerspectiveCamera(" << fov << ",1," << near << "," << far << ");\n";
    }
    out.js << "ctx.camera.position.set(" << Num(eye.x) << "," << Num(eye.y) << "," << Num(eye.z) << ");ctx.camera.lookAt(" << Num(at.x) << "," << Num(at.y) << "," << Num(at.z) << ");\n";
    if (camera != nullptr && !Attr(*camera, "sway").empty()) {
        out.js << "{const baseX=ctx.camera.rotation.x;const baseZ=ctx.camera.rotation.z;ctx.frames.push(()=>{const reduced=window.matchMedia(\"(prefers-reduced-motion: reduce)\").matches;"
               << "const phase=reduced?" << (Attr(*camera, "swayHold").empty() ? "2.2" : Attr(*camera, "swayHold")) << ":(ctx.field?ctx.field.elapsed:0)*"
               << (Attr(*camera, "swaySpeed").empty() ? "0.785" : Attr(*camera, "swaySpeed")) << ";ctx.camera.rotation.z=baseZ+(reduced?0:Math.sin(phase)*"
               << Attr(*camera, "sway") << ");ctx.camera.rotation.x=baseX+(reduced?0:Math.cos(phase)*" << (Attr(*camera, "swayX").empty() ? "0" : Attr(*camera, "swayX"))
               << ");});}\n";
    }
    if (camera != nullptr && !Attr(*camera, "drift").empty()) {
        out.js << "ctx.frames.push((dt,time)=>{const reduced=window.matchMedia(\"(prefers-reduced-motion: reduce)\").matches;const drift="
               << Attr(*camera, "drift") << "*Math.PI/180;if(reduced){ctx.camera.rotation.set(0,0,0);return;}"
               << "ctx.camera.rotation.x=Math.sin(time*" << (Attr(*camera, "driftX").empty() ? "0.11" : Attr(*camera, "driftX")) << ")*(drift*"
               << (Attr(*camera, "driftMix").empty() ? "0.35" : Attr(*camera, "driftMix")) << ");"
               << "ctx.camera.rotation.y=Math.sin(time*" << (Attr(*camera, "driftY").empty() ? "0.08" : Attr(*camera, "driftY")) << "+"
               << (Attr(*camera, "driftPhase").empty() ? "0.4" : Attr(*camera, "driftPhase")) << ")*drift;});\n";
    }
    if (camera && !Attr(*camera, "name").empty()) out.js << "remember(ctx,\"" << Js(Attr(*camera, "name")) << "\",ctx.camera);\n";
    if (const RoboXMLNode* environment = Child(scene, "environment")) {
        const std::string preset = Lower(Attr(*environment, "preset"));
        if (!Attr(*environment, "background").empty()) out.js << "ctx.scene.background=new THREE.Color(\"" << Js(Attr(*environment, "background")) << "\");\n";
        if (!Attr(*environment, "source").empty()) {
            out.uses.hdr = true;
            const std::string url = Publish(out, Attr(*environment, "source"), "images");
            out.js << "new RGBELoader().load(\"" << Js(url) << "\",(tex)=>{tex.mapping=THREE.EquirectangularReflectionMapping;ctx.scene.environment=tex;";
            if (Flag(Attr(*environment, "visible")) || Lower(Attr(*environment, "background")) == "environment") out.js << "ctx.scene.background=tex;";
            out.js << "});\n";
        } else if (preset == "studio" || preset.empty()) {
            out.uses.room = true;
            out.js << "{const pmrem=new THREE.PMREMGenerator(ctx.renderer);ctx.scene.environment=pmrem.fromScene(new RoomEnvironment(),0.04).texture;pmrem.dispose();}\n";
        } else if (preset != "color" && preset != "none") {
            out.errors.push_back("environment preset '" + preset + "' needs an HDR Source. studio and color are built in");
        }
        if (!Attr(*environment, "intensity").empty()) out.js << "ctx.scene.environmentIntensity=" << Attr(*environment, "intensity") << ";\n";
    }
    EmitLights(out, scene);
    for (const RoboXMLNode& child : scene.children) {
        const std::string kind = Lower(child.name);
        if (kind == "camera" || kind == "lighting" || kind == "ambient" || kind == "directional" || kind == "point" || kind == "spot" ||
            kind == "hemisphere" || kind == "arealight" || kind == "environment" || kind == "fog" || kind == "renderer" || kind == "shader" ||
            kind == "timeline" || kind == "sequence" || kind == "physics" || kind == "state" || kind == "binding" || kind == "interaction" ||
            kind == "controls" || kind == "orbitcontrols" || kind == "postprocessing" || kind == "ui" || kind == "constraint") {
            continue;
        }
        EmitNode(out, child, "ctx.scene", "");
    }
    for (const RoboXMLNode& child : scene.children) {
        if (Lower(child.name) == "timeline" || Lower(child.name) == "animation") EmitTracks(out, child);
    }
    for (const RoboXMLNode& child : scene.children) {
        if (Lower(child.name) != "sequence") continue;
        out.js << "ctx.sequence=[";
        bool first = true;
        for (const RoboXMLNode& step : child.children) {
            if (!first) out.js << ",";
            first = false;
            if (Lower(step.name) == "wait") out.js << "{kind:\"wait\",duration:" << (Attr(step, "duration").empty() ? "1" : Attr(step, "duration")) << "}";
            else out.js << "{kind:\"play\",name:\"" << Js(Attr(step, "animation").empty() ? Attr(step, "name") : Attr(step, "animation")) << "\"}";
        }
        out.js << "];\n";
    }
    for (const RoboXMLNode& child : scene.children) {
        if (Lower(child.name) != "constraint") continue;
        const std::string type = Lower(Attr(child, "type"));
        if (type == "lookat") {
            out.js << "ctx.frames.push(()=>{const a=ctx.objects.get(\"" << Js(Attr(child, "target")) << "\");const b=ctx.objects.get(\"" << Js(Attr(child, "at"))
                   << "\");if(a&&b)a.lookAt(b.getWorldPosition(new THREE.Vector3()));});\n";
        } else if (type == "copy-position" || type == "copy-rotation") {
            const char* prop = type == "copy-position" ? "position" : "rotation";
            out.js << "ctx.frames.push(()=>{const a=ctx.objects.get(\"" << Js(Attr(child, "target")) << "\");const b=ctx.objects.get(\"" << Js(Attr(child, "from"))
                   << "\");if(a&&b)a." << prop << ".copy(b." << prop << ");});\n";
        } else if (type == "limit-position" || type == "limit-rotation") {
            const char* prop = type == "limit-position" ? "position" : "rotation";
            const std::string axis = Lower(Attr(child, "axis").empty() ? "y" : Attr(child, "axis"));
            out.js << "ctx.frames.push(()=>{const a=ctx.objects.get(\"" << Js(Attr(child, "target")) << "\");if(a)a." << prop << "." << axis << "=Math.min("
                   << (Attr(child, "max").empty() ? "1" : Attr(child, "max")) << ",Math.max(" << (Attr(child, "min").empty() ? "0" : Attr(child, "min")) << ",a." << prop
                   << "." << axis << "));});\n";
        } else if (type == "follow") {
            out.js << "ctx.frames.push((dt,time)=>{const a=ctx.objects.get(\"" << Js(Attr(child, "target")) << "\");const curve=ctx.curves.get(\""
                   << Js(Attr(child, "curve")) << "\");if(a&&curve){const duration=" << (Attr(child, "duration").empty() ? "4" : Attr(child, "duration"))
                   << ";const u=" << (Flag(Attr(child, "loop")) ? "(time%duration)/duration" : "Math.min(1,time/duration)")
                   << ";a.position.copy(curve.getPointAt(u));}});\n";
        } else {
            out.errors.push_back("constraint '" + type + "' is not supported");
        }
    }
    std::vector<std::string> ignored;
    for (const RoboXMLNode& child : scene.children) {
        if (Lower(child.name) == "physics") EmitPhysics(out, child, scene, ignored);
    }
    for (const RoboXMLNode& child : scene.children) {
        if (Lower(child.name) != "state") continue;
        const std::string value = Attr(child, "default").empty() ? Attr(child, "value") : Attr(child, "default");
        const std::string type = Lower(Attr(child, "type"));
        out.js << "ctx.states[\"" << Js(Attr(child, "name")) << "\"]=";
        if (type == "number") out.js << (value.empty() ? "0" : value);
        else if (type == "bool" || type == "boolean") out.js << (Flag(value) ? "true" : "false");
        else out.js << "\"" << Js(value) << "\"";
        out.js << ";\n";
    }
    for (const RoboXMLNode& child : scene.children) {
        if (Lower(child.name) != "binding") continue;
        std::string value = Attr(child, "value");
        if (!value.empty() && value.front() == '{' && value.back() == '}') value = value.substr(1, value.size() - 2);
        const std::string target = Attr(child, "target");
        const std::size_t split = target.rfind(".Material.");
        std::string object = target;
        std::string property = "material.color";
        if (split != std::string::npos) {
            object = target.substr(0, split);
            property = "material." + Lower(target.substr(split + 10));
        }
        out.js << "ctx.bindings.push({target:\"" << Js(object) << "\",property:\"" << Js(property) << "\",state:\"" << Js(Trim(value)) << "\"});\n";
    }
    bool anyInteraction = false;
    for (const RoboXMLNode& child : scene.children) {
        if (Lower(child.name) == "interaction") anyInteraction = true;
    }
    if (anyInteraction) {
        out.js << "{const ray=new THREE.Raycaster();const pointer=new THREE.Vector2();const run=(event,name)=>{"
                  "const rect=ctx.renderer.domElement.getBoundingClientRect();pointer.x=((event.clientX-rect.left)/rect.width)*2-1;pointer.y=-((event.clientY-rect.top)/rect.height)*2+1;"
                  "ray.setFromCamera(pointer,ctx.camera);const hits=ray.intersectObjects(ctx.scene.children,true);const hit=hits[0]&&hits[0].object;";
        for (const RoboXMLNode& child : scene.children) {
            if (Lower(child.name) != "interaction") continue;
            for (const RoboXMLNode& on : child.children) {
                if (Lower(on.name) != "on") continue;
                const std::string event = Lower(Attr(on, "event").empty() ? "click" : Attr(on, "event"));
                out.js << "if(name==\"" << Js(event) << "\"&&hitObject(ctx,\"" << Js(Attr(child, "target")) << "\",hit)){";
                for (const RoboXMLNode& action : on.children) {
                    const std::string actionType = Lower(Attr(action, "type"));
                    if (actionType == "setstate") out.js << "ctx.states[\"" << Js(Attr(action, "name")) << "\"]=\"" << Js(Attr(action, "value")) << "\";";
                    else if (actionType == "play") out.js << "ctx.playing.add(\"" << Js(Attr(action, "name").empty() ? Attr(action, "animation") : Attr(action, "name")) << "\");";
                    else if (actionType == "visible") out.js << "{const o=ctx.objects.get(\"" << Js(Attr(action, "target")) << "\");if(o)o.visible=" << (Flag(Attr(action, "value").empty() ? "true" : Attr(action, "value")) ? "true" : "false") << ";}";
                }
                out.js << "}";
            }
        }
        out.js << "};ctx.renderer.domElement.addEventListener(\"pointerdown\",(event)=>run(event,\"click\"));ctx.renderer.domElement.addEventListener(\"pointermove\",(event)=>run(event,\"hover\"));}\n";
    }
    bool controls = false;
    for (const RoboXMLNode& child : scene.children) {
        const std::string kind = Lower(child.name);
        const RoboXMLNode* control = &child;
        std::string mode = kind;
        if (kind == "controls") {
            if (child.children.empty()) mode = "orbitcontrols";
            else control = &child.children.front(), mode = Lower(control->name);
        }
        if (mode != "orbitcontrols" && mode != "mapcontrols" && mode != "flycontrols") continue;
        controls = true;
        if (mode == "mapcontrols") out.uses.map = true;
        else if (mode == "flycontrols") out.uses.fly = true;
        else out.uses.orbit = true;
        const char* ctor = mode == "mapcontrols" ? "MapControls" : mode == "flycontrols" ? "FlyControls" : "OrbitControls";
        out.js << "ctx.controls=new " << ctor << "(ctx.camera,ctx.renderer.domElement);";
        if (mode != "flycontrols") {
            out.js << "ctx.controls.target.set(" << Num(at.x) << "," << Num(at.y) << "," << Num(at.z) << ");";
            if (!Flag(Attr(*control, "enableZoom").empty() ? "true" : Attr(*control, "enableZoom"))) out.js << "ctx.controls.enableZoom=false;";
            if (Flag(Attr(*control, "enablePan")) == false && Attr(*control, "enablePan") == "false") out.js << "ctx.controls.enablePan=false;";
            if (Flag(Attr(*control, "autoRotate"))) out.js << "ctx.controls.autoRotate=true;";
            if (!Attr(*control, "minDistance").empty()) out.js << "ctx.controls.minDistance=" << Attr(*control, "minDistance") << ";";
            if (!Attr(*control, "maxDistance").empty()) out.js << "ctx.controls.maxDistance=" << Attr(*control, "maxDistance") << ";";
            out.js << "ctx.controls.enableDamping=true;";
        }
        out.js << "\n";
    }
    (void)controls;
    if (const RoboXMLNode* post = Child(scene, "postprocessing")) {
        for (const RoboXMLNode& child : post->children) {
            if (Lower(child.name) != "bloom") continue;
            out.uses.bloom = true;
            out.js << "{const composer=new EffectComposer(ctx.renderer);composer.addPass(new RenderPass(ctx.scene,ctx.camera));composer.addPass(new UnrealBloomPass(new THREE.Vector2(512,512),"
                   << (Attr(child, "strength").empty() ? "0.4" : Attr(child, "strength")) << "," << (Attr(child, "radius").empty() ? "0.3" : Attr(child, "radius")) << ","
                   << (Attr(child, "threshold").empty() ? "0.85" : Attr(child, "threshold")) << "));ctx.composer=composer;}\n";
        }
    }
    for (const RoboXMLNode& child : scene.children) {
        if (Lower(child.name) != "ui") continue;
        if (!Attr(child, "source").empty()) {
            const std::string source = Attr(child, "source");
            fs::path path = fs::path(source);
            if (path.is_relative()) path = prepared.directory / path;
            const std::string ext = Lower(path.extension().string());
            if (ext != ".rxu" && ext != ".xml") {
                out.errors.push_back("UI Source must be a RobixUI .rxu document");
            } else {
                const HostState saved = CaptureHost();
                fs::path panelRoot = saved.outRoot / "panels" / path.stem();
                std::string error;
                if (!Robix::UI::WriteWebFoundation(panelRoot, path, error, {}, true)) {
                    out.errors.push_back(error.empty() ? "could not compile " + source : error);
                }
                ApplyHost(saved);
                uiHtml += "<iframe class=\"rx3d-frame\" src=\"panels/" + Html(path.stem().string()) + "/index.html\" title=\"" + Html(path.stem().string()) + "\"></iframe>";
            }
        }
        for (const RoboXMLNode& control : child.children) {
            const std::string controlKind = Lower(control.name);
            const std::string bind = Attr(control, "bind").empty() ? Attr(control, "name") : Attr(control, "bind");
            const std::string label = Attr(control, "label").empty() ? Attr(control, "text") : Attr(control, "label");
            if (controlKind == "colorpicker") {
                uiHtml += "<label class=\"rx3d-field\">" + Html(label) + "<input type=\"color\" data-state=\"" + Html(bind) + "\"></label>";
            } else if (controlKind == "slider") {
                uiHtml += "<label class=\"rx3d-field\">" + Html(label) + "<input type=\"range\" data-state=\"" + Html(bind) + "\" min=\"" +
                          Html(Attr(control, "min").empty() ? "0" : Attr(control, "min")) + "\" max=\"" + Html(Attr(control, "max").empty() ? "1" : Attr(control, "max")) +
                          "\" step=\"" + Html(Attr(control, "step").empty() ? "1" : Attr(control, "step")) + "\"></label>";
            } else if (controlKind == "toggle") {
                uiHtml += "<label class=\"rx3d-field\">" + Html(label) + "<input type=\"checkbox\" data-state=\"" + Html(bind) + "\"></label>";
            } else if (controlKind == "note") {
                uiHtml += "<p class=\"rx3d-note\">" + Html(label.empty() ? Trim(control.text) : label) + "</p>";
            }
        }
    }
    if (Lower(Attr(scene, "material")) == "unused") out.js << "";
    std::ostringstream file;
    file << "import * as THREE from \"three\";\n";
    file << "import { mount, remember, hitObject";
    if (out.uses.rounded) file << ", roundedBox";
    if (out.uses.extrude) file << ", extrude";
    if (out.uses.lathe) file << ", lathe";
    if (out.uses.tube) file << ", tube";
    if (out.uses.custom) file << ", customGeometry";
    if (out.uses.subdivide) file << ", subdivide";
    if (out.uses.solidify) file << ", solidify";
    if (out.uses.curve) file << ", curveOf";
    if (out.uses.deform) file << ", deform";
    file << " } from \"./robix3d.js\";\n";
    if (out.uses.orbit) file << "import { OrbitControls } from \"three/addons/controls/OrbitControls.js\";\n";
    if (out.uses.map) file << "import { MapControls } from \"three/addons/controls/MapControls.js\";\n";
    if (out.uses.fly) file << "import { FlyControls } from \"three/addons/controls/FlyControls.js\";\n";
    if (out.uses.area) file << "import { RectAreaLightUniformsLib } from \"three/addons/lights/RectAreaLightUniformsLib.js\";\n";
    if (out.uses.room) file << "import { RoomEnvironment } from \"three/addons/environments/RoomEnvironment.js\";\n";
    if (out.uses.hdr) file << "import { RGBELoader } from \"three/addons/loaders/RGBELoader.js\";\n";
    if (out.uses.gltf) file << "import { GLTFLoader } from \"three/addons/loaders/GLTFLoader.js\";\n";
    if (out.uses.font) {
        file << "import { FontLoader } from \"three/addons/loaders/FontLoader.js\";\n";
        file << "import { TextGeometry } from \"three/addons/geometries/TextGeometry.js\";\n";
    }
    if (out.uses.sky) file << "import { Sky } from \"three/addons/objects/Sky.js\";\n";
    if (out.uses.bloom) {
        file << "import { EffectComposer } from \"three/addons/postprocessing/EffectComposer.js\";\n";
        file << "import { RenderPass } from \"three/addons/postprocessing/RenderPass.js\";\n";
        file << "import { UnrealBloomPass } from \"three/addons/postprocessing/UnrealBloomPass.js\";\n";
    }
    if (out.uses.csg) file << "import { Brush, Evaluator, ADDITION, SUBTRACTION, INTERSECTION } from \"https://cdn.jsdelivr.net/npm/three-bvh-csg@0.0.16/+esm\";\n";
    file << "const build=(ctx)=>{\n";
    if (out.uses.area) file << "RectAreaLightUniformsLib.init();\n";
    file << out.js.str();
    file << "};\nexport default build;\n";
    errors.insert(errors.end(), out.errors.begin(), out.errors.end());
    return file.str();
}

std::string FindRuntime() {
    const fs::path beside = fs::path(__FILE__).parent_path() / "Runtime" / "robix3d.js";
    if (fs::is_regular_file(beside)) return beside.string();
    fs::path cursor = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        const fs::path candidate = cursor / "Robix3D" / "Runtime" / "robix3d.js";
        if (fs::is_regular_file(candidate)) return candidate.string();
        if (!cursor.has_parent_path() || cursor == cursor.root_path()) break;
        cursor = cursor.parent_path();
    }
    return {};
}

bool CopyRuntime(const fs::path& outRoot, std::string& error) {
    const std::string source = FindRuntime();
    if (source.empty()) {
        error = "Robix3D runtime was not found";
        return false;
    }
    std::ifstream in(source, std::ios::binary);
    std::ofstream dest(outRoot / "robix3d.js", std::ios::binary);
    dest << in.rdbuf();
    if (!dest) {
        error = "could not write robix3d.js";
        return false;
    }
    return true;
}

std::string ImportMap(const std::string& version) {
    std::string clean;
    for (char ch : version) {
        if ((ch >= '0' && ch <= '9') || ch == '.') clean.push_back(ch);
    }
    if (clean.empty()) clean = "0.170.0";
    return "<script type=\"importmap\">{\"imports\":{\"three\":\"https://unpkg.com/three@" + clean +
           "/build/three.module.js\",\"three/addons/\":\"https://unpkg.com/three@" + clean + "/examples/jsm/\"}}</script>";
}

bool SceneAlpha(const RoboXMLNode& scene) {
    if (Flag(Attr(scene, "transparent")) || Lower(Attr(scene, "background")) == "none") return true;
    if (const RoboXMLNode* renderer = Child(scene, "renderer")) {
        if (Flag(Attr(*renderer, "alpha")) || Flag(Attr(*renderer, "transparent"))) return true;
    }
    return false;
}

std::string CssBlock() {
    return ".rx3d-host,.rx3d-page{position:relative;width:100%;height:100%;min-height:240px;overflow:hidden;background:#000}"
           ".rx3d-host canvas,.rx3d-page canvas{display:block;width:100%;height:100%}"
           ".rx3d-ui{position:absolute;left:16px;bottom:16px;display:flex;flex-direction:column;gap:8px;z-index:2;color:#fff;font:14px/1.3 system-ui,sans-serif}"
           ".rx3d-field{display:flex;align-items:center;gap:8px}"
           ".rx3d-frame{position:absolute;right:16px;bottom:16px;width:min(360px,40vw);height:min(480px,60vh);border:0;border-radius:16px;z-index:2;background:#111}"
           ".rx3d-host[data-layer=backdrop]{position:absolute;inset:0;z-index:0;min-height:0;overflow:visible;pointer-events:none;background:transparent}"
           ".rx3d-host[data-layer=backdrop] canvas{background:transparent}"
           ".rx3d-host[data-paint=bleed]{overflow:visible;top:calc(-1 * var(--rx3d-bleed, min(48vh, 420px)));bottom:calc(-1 * var(--rx3d-bleed, min(48vh, 420px)));height:auto;z-index:1}"
           ".rx3d-host[data-paint=clip]{overflow:hidden;inset:0;top:0;bottom:0;height:auto;z-index:0}";
}

bool WriteText(const fs::path& path, const std::string& text) {
    std::ofstream dest(path, std::ios::binary);
    dest << text;
    return static_cast<bool>(dest);
}

}  // namespace

void Reset() {
    gModule.clear();
    gCss = CssBlock();
    gView = 0;
    gStack.clear();
    gPieces.clear();
    gErrors.clear();
}

void UseDocument(const fs::path& document, const fs::path& outRoot) {
    gDocument = document;
    gOutRoot = outRoot;
}

std::string ModuleSource() { return gModule; }
std::string CssSource() { return gCss; }

void Restore(const std::string& module, const std::string& css) {
    gModule = module;
    gCss = css;
}

void EmitView(std::string& html, const RoboXMLNode& node) {
    const std::string source = Attr(node, "source");
    if (source.empty()) {
        gErrors.push_back("View3D needs Source pointing at a .rx3d document");
        return;
    }
    fs::path path = fs::path(source);
    if (path.is_relative()) path = gDocument.parent_path() / path;
    std::error_code code;
    const fs::path absolute = fs::weakly_canonical(path, code);
    const std::string key = code ? path.string() : absolute.string();
    if (!gStack.insert(key).second) {
        gErrors.push_back("Robix3D import cycle at " + source);
        return;
    }
    const Prepared prepared = Prepare(path);
    gStack.erase(key);
    for (const std::string& error : prepared.errors) gErrors.push_back(error);
    if (!prepared.errors.empty()) return;
    const std::string id = "rx3d-" + std::to_string(gView++);
    std::string ui;
    std::vector<std::string> errors;
    const fs::path previousOut = gOutRoot;
    const std::string module = BuildScene(prepared, [&]() -> const RoboXMLNode& {
        for (const RoboXMLNode& child : prepared.root.children) {
            if (Lower(child.name) == "scene") return child;
        }
        return prepared.root;
    }(), ui, errors);
    gOutRoot = previousOut;
    for (const std::string& error : errors) gErrors.push_back(error);
    if (!errors.empty()) return;
    gPieces.push_back(Piece{id, module, ui, prepared.three});
    std::string width = Attr(node, "width");
    std::string height = Attr(node, "height");
    if (!width.empty() && width.find_first_not_of("0123456789.") == std::string::npos) width += "px";
    if (!height.empty() && height.find_first_not_of("0123456789.") == std::string::npos) height += "px";
    const RoboXMLNode* scene = nullptr;
    for (const RoboXMLNode& child : prepared.root.children) {
        if (Lower(child.name) == "scene") scene = &child;
    }
    const bool alpha = scene != nullptr && SceneAlpha(*scene);
    const bool backdrop = Lower(Attr(node, "layer")) == "backdrop";
    auto paintOf = [](const RoboXMLNode& item) {
        const std::string paint = Lower(Attr(item, "paint"));
        if (paint == "bleed" || paint == "clip") return paint;
        const std::string bleed = Lower(Attr(item, "bleed"));
        if (!Attr(item, "bleed").empty() && bleed != "false" && bleed != "0" && bleed != "no" && bleed != "off") return std::string("bleed");
        const std::string clip = Lower(Attr(item, "clip"));
        if (clip == "true" || clip == "1" || clip == "yes" || clip == "on") return std::string("clip");
        return std::string();
    };
    std::string paint = paintOf(node);
    if (paint.empty() && scene != nullptr) paint = paintOf(*scene);
    std::string bleed = Attr(node, "bleed");
    if ((bleed.empty() || Lower(bleed) == "true") && scene != nullptr) {
        const std::string sceneBleed = Attr(*scene, "bleed");
        if (!sceneBleed.empty() && Lower(sceneBleed) != "true") bleed = sceneBleed;
    }
    const std::string bleedKey = Lower(bleed);
    std::string distance = "min(48vh, 420px)";
    if (!bleed.empty() && bleedKey != "true" && bleedKey != "yes" && bleedKey != "on" && bleedKey != "1" && bleedKey != "false" &&
        bleedKey != "no" && bleedKey != "off" && bleedKey != "0") {
        distance = bleed.find_first_not_of("0123456789.") == std::string::npos ? bleed + "px" : bleed;
    }
    html += "<div class=\"rx3d-host\" id=\"" + id + "\"";
    std::string style;
    if (!width.empty()) style += "width:" + Html(width) + ";";
    if (!height.empty()) style += "height:" + Html(height) + ";";
    if (paint == "bleed") style += "--rx3d-bleed:" + Html(distance) + ";";
    if (!style.empty()) html += " style=\"" + style + "\"";
    if (backdrop) html += " data-layer=\"backdrop\"";
    if (paint == "bleed" || paint == "clip") html += " data-paint=\"" + paint + "\"";
    html += "><canvas";
    if (alpha) html += " data-alpha=\"true\"";
    html += "></canvas>";
    if (!ui.empty()) html += "<div class=\"rx3d-ui\">" + ui + "</div>";
    html += "</div>";
}

bool WritePage(const fs::path& outRoot, const fs::path& document, std::string& error) {
    Reset();
    UseDocument(document, outRoot);
    std::error_code code;
    const fs::path absolute = fs::weakly_canonical(document, code);
    gStack.insert(code ? document.string() : absolute.string());
    const Prepared prepared = Prepare(document);
    if (!prepared.errors.empty()) {
        error = prepared.errors.front();
        for (std::size_t i = 1; i < prepared.errors.size(); ++i) error += "\n" + prepared.errors[i];
        return false;
    }
    const RoboXMLNode* scene = nullptr;
    for (const RoboXMLNode& child : prepared.root.children) {
        if (Lower(child.name) == "scene") scene = &child;
    }
    std::string ui;
    std::vector<std::string> errors;
    const std::string module = BuildScene(prepared, *scene, ui, errors);
    if (!errors.empty()) {
        error = errors.front();
        for (std::size_t i = 1; i < errors.size(); ++i) error += "\n" + errors[i];
        return false;
    }
    if (!CopyRuntime(outRoot, error)) return false;
    if (!WriteText(outRoot / "scene.js", module + "mount(document.querySelector(\"#rx3d-root canvas\"), build);\n")) {
        error = "could not write scene.js";
        return false;
    }
    const std::string css = CssBlock() + "html,body{margin:0;height:100%;background:#000}";
    const bool alpha = SceneAlpha(*scene);
    const std::string html = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>" +
                             Html(prepared.title) + "</title>" + ImportMap(prepared.three) + "<link rel=\"stylesheet\" href=\"app.css\"></head><body><div class=\"rx3d-page\" id=\"rx3d-root\"><canvas" +
                             (alpha ? " data-alpha=\"true\"" : "") + "></canvas>" +
                             (ui.empty() ? "" : "<div class=\"rx3d-ui\">" + ui + "</div>") + "</div><script type=\"module\" src=\"scene.js\"></script></body></html>\n";
    if (!WriteText(outRoot / "index.html", html) || !WriteText(outRoot / "app.css", css)) {
        error = "could not write the Robix3D page";
        return false;
    }
    std::cout << "robix3d " << (outRoot / "index.html").string() << "\n";
    return true;
}

const std::vector<std::string>& Errors() { return gErrors; }

bool CommitViews(const fs::path& outRoot, std::string& error) {
    if (!gErrors.empty()) {
        error = gErrors.front();
        for (std::size_t index = 1; index < gErrors.size(); ++index) error += "\n" + gErrors[index];
        return false;
    }
    if (gPieces.empty()) return true;
    if (!CopyRuntime(outRoot, error)) return false;
    std::ostringstream loader;
    loader << "import { mount } from \"./robix3d.js\";\n";
    for (std::size_t index = 0; index < gPieces.size(); ++index) {
        const std::string file = "scene-" + std::to_string(index) + ".js";
        if (!WriteText(outRoot / file, gPieces[index].module)) {
            error = "could not write " + file;
            return false;
        }
        loader << "import build" << index << " from \"./" << file << "\";\n";
    }
    for (std::size_t index = 0; index < gPieces.size(); ++index) {
        loader << "mount(document.querySelector(\"#" << gPieces[index].id << " canvas\"), build" << index << ");\n";
    }
    if (!WriteText(outRoot / "robix3d.scene.js", loader.str())) {
        error = "could not write robix3d.scene.js";
        return false;
    }
    gModule = ImportMap(gPieces.front().version);
    return true;
}

}  // namespace Robix::D3
