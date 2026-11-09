#include "Robix3D.hpp"

#include "Compile.hpp"
#include "Web/WebFoundation.hpp"

#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <functional>
#include <sstream>
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
    if (material != nullptr && !Attr(*material, "opacity").empty()) args << ",transparent:true";
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
    if (material != nullptr && !Attr(*material, "shader").empty()) return "shaders[\"" + Js(Attr(*material, "shader")) + "\"](ctx)";
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

void EmitNode(Out& out, const RoboXMLNode& node, const std::string& parent, const std::string& path) {
    const std::string kind = Lower(node.name);
    const std::string name = Attr(node, "name");
    const std::string full = name.empty() ? path : (path.empty() ? name : path + "." + name);
    if (kind == "group" || kind == "object") {
        const std::string object = Fresh(out);
        out.js << "{const " << object << "=new THREE.Group();";
        if (!name.empty()) out.js << object << ".name=\"" << Js(name) << "\";";
        EmitTransform(out, object, node);
        if (!full.empty()) out.js << "remember(ctx,\"" << Js(full) << "\"," << object << ");";
        out.js << parent << ".add(" << object << ");\n";
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

std::string ShaderFunction(const RoboXMLNode& shader) {
    std::ostringstream uniforms;
    std::ostringstream decls;
    bool time = false;
    for (const RoboXMLNode& child : shader.children) {
        if (Lower(child.name) != "uniform") continue;
        const std::string name = Attr(child, "name");
        const std::string type = Lower(Attr(child, "type").empty() ? "float" : Attr(child, "type"));
        const std::string value = Attr(child, "default").empty() ? Attr(child, "value") : Attr(child, "default");
        if (Lower(name) == "time") time = true;
        if (type == "color") {
            decls << "uniform vec3 " << name << ";\n";
            uniforms << "\"" << Js(name) << "\":{value:new THREE.Color(\"" << Js(value.empty() ? "#ffffff" : value) << "\")},";
        } else {
            decls << "uniform float " << name << ";\n";
            uniforms << "\"" << Js(name) << "\":{value:" << (value.empty() ? "0" : value) << "},";
        }
    }
    const RoboXMLNode* glsl = nullptr;
    const RoboXMLNode* surface = nullptr;
    for (const RoboXMLNode& child : shader.children) {
        if (Lower(child.name) == "glsl") glsl = &child;
        if (Lower(child.name) == "surface") surface = &child;
    }
    std::string fragment;
    if (glsl != nullptr) {
        fragment = TextOf(*glsl, "fragment");
        if (fragment.empty()) fragment = Trim(glsl->text);
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
    std::string vertex = glsl != nullptr ? TextOf(*glsl, "vertex") : "";
    if (vertex.empty()) {
        vertex = "varying vec3 vN;varying vec3 vW;void main(){vec4 world=modelMatrix*vec4(position,1.0);vW=world.xyz;vN=normalize(mat3(modelMatrix)*normal);gl_Position=projectionMatrix*viewMatrix*world;}";
    }
    std::ostringstream fn;
    fn << "function(ctx){const material=new THREE.ShaderMaterial({uniforms:{" << uniforms.str()
       << "},vertexShader:\"" << Js(vertex) << "\",fragmentShader:\"" << Js(fragment) << "\"});";
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
        out.js << "(async()=>{const RAPIER=await import(\"https://cdn.jsdelivr.net/npm/@dimforge/rapier3d-compat@0.14.0/+esm\");await RAPIER.init();const world=new RAPIER.World({x:"
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
    std::function<void(const RoboXMLNode&)> collectPlays = [&](const RoboXMLNode& node) {
        if (Lower(node.name) == "play") out.played.insert(Attr(node, "animation").empty() ? Attr(node, "name") : Attr(node, "animation"));
        for (const RoboXMLNode& child : node.children) collectPlays(child);
    };
    collectPlays(scene);
    out.js << "const shaders={";
    for (const RoboXMLNode& child : prepared.root.children) {
        if (Lower(child.name) != "shader") continue;
        out.js << "\"" << Js(Attr(child, "name")) << "\":" << ShaderFunction(child) << ",";
    }
    for (const RoboXMLNode& child : scene.children) {
        if (Lower(child.name) == "shader") out.js << "\"" << Js(Attr(child, "name")) << "\":" << ShaderFunction(child) << ",";
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
            kind == "controls" || kind == "orbitcontrols" || kind == "postprocessing" || kind == "ui" || kind == "constraint" ||
            kind == "cellfield" || kind == "cell" || kind == "membrane") {
            continue;
        }
        EmitNode(out, child, "ctx.scene", "");
    }
    for (const RoboXMLNode& child : scene.children) {
        if (Lower(child.name) == "cellfield") EmitCellField(out, child);
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
            if (ext != ".rbxu" && ext != ".xml") {
                out.errors.push_back("UI Source must be a RobixUI .rbxu document");
            } else {
                const HostState saved = CaptureHost();
                fs::path panelRoot = saved.outRoot / "panels" / path.stem();
                std::string error;
                if (!Robix::UI::WriteWebFoundation(panelRoot, path, error, {}, true)) {
                    out.errors.push_back(error.empty() ? "could not compile " + source : error);
                }
                ApplyHost(saved);
                uiHtml += "<iframe class=\"r3d-frame\" src=\"panels/" + Html(path.stem().string()) + "/index.html\" title=\"" + Html(path.stem().string()) + "\"></iframe>";
            }
        }
        for (const RoboXMLNode& control : child.children) {
            const std::string controlKind = Lower(control.name);
            const std::string bind = Attr(control, "bind").empty() ? Attr(control, "name") : Attr(control, "bind");
            const std::string label = Attr(control, "label").empty() ? Attr(control, "text") : Attr(control, "label");
            if (controlKind == "colorpicker") {
                uiHtml += "<label class=\"r3d-field\">" + Html(label) + "<input type=\"color\" data-state=\"" + Html(bind) + "\"></label>";
            } else if (controlKind == "slider") {
                uiHtml += "<label class=\"r3d-field\">" + Html(label) + "<input type=\"range\" data-state=\"" + Html(bind) + "\" min=\"" +
                          Html(Attr(control, "min").empty() ? "0" : Attr(control, "min")) + "\" max=\"" + Html(Attr(control, "max").empty() ? "1" : Attr(control, "max")) +
                          "\" step=\"" + Html(Attr(control, "step").empty() ? "1" : Attr(control, "step")) + "\"></label>";
            } else if (controlKind == "toggle") {
                uiHtml += "<label class=\"r3d-field\">" + Html(label) + "<input type=\"checkbox\" data-state=\"" + Html(bind) + "\"></label>";
            } else if (controlKind == "note") {
                uiHtml += "<p class=\"r3d-note\">" + Html(label.empty() ? Trim(control.text) : label) + "</p>";
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

std::string CssBlock() {
    return ".r3d-host,.r3d-page{position:relative;width:100%;height:100%;min-height:240px;overflow:hidden;background:#000}"
           ".r3d-host canvas,.r3d-page canvas{display:block;width:100%;height:100%}"
           ".r3d-ui{position:absolute;left:16px;bottom:16px;display:flex;flex-direction:column;gap:8px;z-index:2;color:#fff;font:14px/1.3 system-ui,sans-serif}"
           ".r3d-field{display:flex;align-items:center;gap:8px}"
           ".r3d-frame{position:absolute;right:16px;bottom:16px;width:min(360px,40vw);height:min(480px,60vh);border:0;border-radius:16px;z-index:2;background:#111}";
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
        gErrors.push_back("View3D needs Source pointing at a .r3d document");
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
    const std::string id = "r3d-" + std::to_string(gView++);
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
    html += "<div class=\"r3d-host\" id=\"" + id + "\"";
    if (!width.empty() || !height.empty()) {
        html += " style=\"";
        if (!width.empty()) html += "width:" + Html(width) + ";";
        if (!height.empty()) html += "height:" + Html(height) + ";";
        html += "\"";
    }
    html += "><canvas></canvas>";
    if (!ui.empty()) html += "<div class=\"r3d-ui\">" + ui + "</div>";
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
    if (!WriteText(outRoot / "scene.js", module + "mount(document.querySelector(\"#r3d-root canvas\"), build);\n")) {
        error = "could not write scene.js";
        return false;
    }
    const std::string css = CssBlock() + "html,body{margin:0;height:100%;background:#000}";
    const std::string html = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>" +
                             Html(prepared.title) + "</title>" + ImportMap(prepared.three) + "<link rel=\"stylesheet\" href=\"app.css\"></head><body><div class=\"r3d-page\" id=\"r3d-root\"><canvas></canvas>" +
                             (ui.empty() ? "" : "<div class=\"r3d-ui\">" + ui + "</div>") + "</div><script type=\"module\" src=\"scene.js\"></script></body></html>\n";
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
