#include "SceneRaster.hpp"

#include "../../RobixUI/Compiler/RoboXML.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Robix {
namespace {

struct Vec3 {
    double x = 0;
    double y = 0;
    double z = 0;
};

struct Tri {
    Vec3 v0;
    Vec3 v1;
    Vec3 v2;
    Vec3 n;
    int red = 0;
    int green = 0;
    int blue = 0;
};

struct Camera {
    std::string name;
    Vec3 position;
    Vec3 look;
    Vec3 up{0, 1, 0};
    double fov = 60;
};

double Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

Vec3 Mul(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }

Vec3 Cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 Norm(Vec3 v) {
    const double length = std::sqrt(Dot(v, v));
    if (length < 1e-12) {
        return {0, 0, 0};
    }
    return Mul(v, 1.0 / length);
}

Vec3 Rotate(Vec3 p, double rx, double ry, double rz) {
    double x = p.x;
    double y = p.y;
    double z = p.z;
    double c = std::cos(rx);
    double s = std::sin(rx);
    double y1 = y * c - z * s;
    double z1 = y * s + z * c;
    c = std::cos(ry);
    s = std::sin(ry);
    double x2 = x * c + z1 * s;
    double z2 = -x * s + z1 * c;
    c = std::cos(rz);
    s = std::sin(rz);
    return {x2 * c - y1 * s, x2 * s + y1 * c, z2};
}

bool ParseVec(const std::string& text, Vec3& out) {
    std::string body = text;
    for (char& ch : body) {
        if (ch == ',') {
            ch = ' ';
        }
    }
    std::istringstream row(body);
    return static_cast<bool>(row >> out.x >> out.y >> out.z);
}

int HexNibble(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return 0;
}

bool ParseColor(const std::string& text, int& red, int& green, int& blue) {
    if (text.size() != 7 || text[0] != '#') {
        return false;
    }
    red = (HexNibble(text[1]) << 4) | HexNibble(text[2]);
    green = (HexNibble(text[3]) << 4) | HexNibble(text[4]);
    blue = (HexNibble(text[5]) << 4) | HexNibble(text[6]);
    return true;
}

const RoboXMLNode* Child(const RoboXMLNode& node, const char* name) {
    for (const RoboXMLNode& child : node.children) {
        if (child.name == name) {
            return &child;
        }
    }
    return nullptr;
}

const RoboXMLNode* FindNamed(const RoboXMLNode& node, const char* name) {
    if (node.name == name) {
        return &node;
    }
    for (const RoboXMLNode& child : node.children) {
        if (const RoboXMLNode* found = FindNamed(child, name)) {
            return found;
        }
    }
    return nullptr;
}

void AddFace(std::vector<Tri>& tris, Vec3 center, Vec3 rotation, Vec3 normal, Vec3 a, Vec3 b, Vec3 c, Vec3 d, int red,
             int green, int blue) {
    Tri first;
    first.v0 = Add(Rotate(a, rotation.x, rotation.y, rotation.z), center);
    first.v1 = Add(Rotate(b, rotation.x, rotation.y, rotation.z), center);
    first.v2 = Add(Rotate(c, rotation.x, rotation.y, rotation.z), center);
    first.n = Norm(Rotate(normal, rotation.x, rotation.y, rotation.z));
    first.red = red;
    first.green = green;
    first.blue = blue;
    Tri second = first;
    second.v0 = first.v0;
    second.v1 = first.v2;
    second.v2 = Add(Rotate(d, rotation.x, rotation.y, rotation.z), center);
    tris.push_back(first);
    tris.push_back(second);
}

void AddBox(std::vector<Tri>& tris, Vec3 center, Vec3 half, Vec3 rotation, int red, int green, int blue) {
    const double hx = half.x;
    const double hy = half.y;
    const double hz = half.z;
    AddFace(tris, center, rotation, {1, 0, 0}, {hx, hy, hz}, {hx, -hy, hz}, {hx, -hy, -hz}, {hx, hy, -hz}, red, green, blue);
    AddFace(tris, center, rotation, {-1, 0, 0}, {-hx, hy, -hz}, {-hx, -hy, -hz}, {-hx, -hy, hz}, {-hx, hy, hz}, red, green, blue);
    AddFace(tris, center, rotation, {0, 1, 0}, {-hx, hy, -hz}, {-hx, hy, hz}, {hx, hy, hz}, {hx, hy, -hz}, red, green, blue);
    AddFace(tris, center, rotation, {0, -1, 0}, {-hx, -hy, hz}, {-hx, -hy, -hz}, {hx, -hy, -hz}, {hx, -hy, hz}, red, green, blue);
    AddFace(tris, center, rotation, {0, 0, 1}, {-hx, hy, hz}, {-hx, -hy, hz}, {hx, -hy, hz}, {hx, hy, hz}, red, green, blue);
    AddFace(tris, center, rotation, {0, 0, -1}, {hx, hy, -hz}, {hx, -hy, -hz}, {-hx, -hy, -hz}, {-hx, hy, -hz}, red, green, blue);
}

bool Hit(const Tri& tri, Vec3 origin, Vec3 direction, double& t) {
    const Vec3 edge1 = Sub(tri.v1, tri.v0);
    const Vec3 edge2 = Sub(tri.v2, tri.v0);
    const Vec3 h = Cross(direction, edge2);
    const double a = Dot(edge1, h);
    if (std::fabs(a) < 1e-8) {
        return false;
    }
    const double f = 1.0 / a;
    const Vec3 s = Sub(origin, tri.v0);
    const double u = f * Dot(s, h);
    if (u < 0 || u > 1) {
        return false;
    }
    const Vec3 q = Cross(s, edge1);
    const double v = f * Dot(direction, q);
    if (v < 0 || u + v > 1) {
        return false;
    }
    t = f * Dot(edge2, q);
    return t > 1e-4;
}

int Shade(const Tri& tri, Vec3 light, double ambient, double intensity) {
    double ndot = Dot(tri.n, Mul(light, -1));
    if (ndot < 0) {
        ndot = 0;
    }
    double factor = ambient + intensity * ndot;
    if (factor > 1) {
        factor = 1;
    }
    if (factor < 0) {
        factor = 0;
    }
    const int red = static_cast<int>(tri.red * factor + 0.5);
    const int green = static_cast<int>(tri.green * factor + 0.5);
    const int blue = static_cast<int>(tri.blue * factor + 0.5);
    return (red << 16) | (green << 8) | blue;
}

bool LoadText(const std::string& path, std::string& text) {
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    std::error_code error;
    fs::path cursor = fs::current_path(error);
    for (int step = 0; step < 8 && !cursor.empty(); ++step) {
        roots.push_back(cursor);
        const fs::path parent = cursor.parent_path();
        if (parent == cursor) {
            break;
        }
        cursor = parent;
    }
    std::vector<fs::path> candidates;
    candidates.emplace_back(path);
    for (const fs::path& root : roots) {
        candidates.push_back(root / path);
        candidates.push_back(root / "RoboticelaOS" / path);
    }
    for (const fs::path& candidate : candidates) {
        std::ifstream input(candidate, std::ios::binary);
        if (!input) {
            continue;
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();
        text = buffer.str();
        return !text.empty();
    }
    return false;
}

}  // namespace

bool RoboRasterizeScene(const std::string& path, const std::string& cameraName, int width, int height,
                        std::vector<std::uint32_t>& pixels) {
    pixels.clear();
    if (width <= 0 || height <= 0) {
        return false;
    }
    std::string source;
    if (!LoadText(path, source)) {
        return false;
    }
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        return false;
    }
    const RoboXMLNode* scene = FindNamed(document.root, "Scene");
    if (scene == nullptr) {
        return false;
    }
    int backgroundRed = 8;
    int backgroundGreen = 10;
    int backgroundBlue = 16;
    ParseColor(RoboXMLAttr(*scene, "Background", "#080A10"), backgroundRed, backgroundGreen, backgroundBlue);
    const std::uint32_t background = static_cast<std::uint32_t>((backgroundRed << 16) | (backgroundGreen << 8) | backgroundBlue);

    std::vector<Camera> cameras;
    for (const RoboXMLNode& child : scene->children) {
        if (child.name != "Camera") {
            continue;
        }
        Camera camera;
        camera.name = RoboXMLAttr(child, "Name", "Main");
        ParseVec(RoboXMLAttr(child, "Position", "0,0,4"), camera.position);
        ParseVec(RoboXMLAttr(child, "LookAt", "0,0,0"), camera.look);
        ParseVec(RoboXMLAttr(child, "Up", "0,1,0"), camera.up);
        try {
            camera.fov = std::stod(RoboXMLAttr(child, "Fov", "60"));
        } catch (...) {
            camera.fov = 60;
        }
        cameras.push_back(camera);
    }
    if (cameras.empty()) {
        return false;
    }
    const Camera* camera = &cameras.front();
    for (const Camera& item : cameras) {
        if (item.name == cameraName) {
            camera = &item;
            break;
        }
    }

    Vec3 light{-0.2, -1, -0.5};
    double ambient = 0.2;
    double intensity = 0.8;
    if (const RoboXMLNode* lighting = Child(*scene, "Lighting")) {
        for (const RoboXMLNode& child : lighting->children) {
            if (child.name == "Ambient") {
                try {
                    ambient = std::stod(RoboXMLAttr(child, "Intensity", "0.2"));
                } catch (...) {
                    ambient = 0.2;
                }
            } else if (child.name == "Directional") {
                const std::string direction = RoboXMLAttr(child, "Direction");
                if (!direction.empty()) {
                    ParseVec(direction, light);
                }
                try {
                    intensity = std::stod(RoboXMLAttr(child, "Intensity", "0.8"));
                } catch (...) {
                    intensity = 0.8;
                }
            }
        }
    }
    light = Norm(light);

    std::vector<Tri> tris;
    for (const RoboXMLNode& mesh : scene->children) {
        if (mesh.name != "Mesh") {
            continue;
        }
        const RoboXMLNode* geometry = Child(mesh, "Geometry");
        const RoboXMLNode* material = Child(mesh, "Material");
        const RoboXMLNode* transform = Child(mesh, "Transform");
        if (geometry == nullptr || RoboXMLAttr(*geometry, "Type") != "box") {
            continue;
        }
        double boxWidth = 1;
        double boxHeight = 1;
        double boxDepth = 1;
        try {
            boxWidth = std::stod(RoboXMLAttr(*geometry, "Width", "1"));
            boxHeight = std::stod(RoboXMLAttr(*geometry, "Height", "1"));
            boxDepth = std::stod(RoboXMLAttr(*geometry, "Depth", "1"));
        } catch (...) {
            boxWidth = boxHeight = boxDepth = 1;
        }
        int red = 255;
        int green = 255;
        int blue = 255;
        if (material != nullptr) {
            ParseColor(RoboXMLAttr(*material, "Color", "#FFFFFF"), red, green, blue);
        }
        Vec3 center;
        Vec3 rotation;
        if (transform != nullptr) {
            ParseVec(RoboXMLAttr(*transform, "Position", "0,0,0"), center);
            ParseVec(RoboXMLAttr(*transform, "Rotation", "0,0,0"), rotation);
        }
        AddBox(tris, center, {boxWidth * 0.5, boxHeight * 0.5, boxDepth * 0.5}, rotation, red, green, blue);
    }
    if (tris.empty()) {
        return false;
    }

    const Vec3 forward = Norm(Sub(camera->look, camera->position));
    const Vec3 right = Norm(Cross(forward, camera->up));
    const Vec3 up = Cross(right, forward);
    const double aspect = static_cast<double>(width) / static_cast<double>(height);
    const double scale = std::tan(camera->fov * 0.5 * 3.14159265358979323846 / 180.0);
    pixels.assign(static_cast<std::size_t>(width * height), background);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double u = ((static_cast<double>(x) + 0.5) / width) * 2.0 - 1.0;
            const double v = 1.0 - ((static_cast<double>(y) + 0.5) / height) * 2.0;
            const Vec3 direction = Norm(Add(Add(forward, Mul(right, u * scale * aspect)), Mul(up, v * scale)));
            const Tri* best = nullptr;
            double bestT = 0;
            for (const Tri& tri : tris) {
                double t = 0;
                if (!Hit(tri, camera->position, direction, t)) {
                    continue;
                }
                if (best == nullptr || t < bestT) {
                    best = &tri;
                    bestT = t;
                }
            }
            if (best != nullptr) {
                pixels[static_cast<std::size_t>(y * width + x)] = static_cast<std::uint32_t>(Shade(*best, light, ambient, intensity));
            }
        }
    }
    return true;
}

}  // namespace Robix
