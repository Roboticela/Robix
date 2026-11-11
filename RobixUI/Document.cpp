#include "Document.hpp"

#include "Compiler/Compose.hpp"
#include "Compiler/Components/Screen/RoboScreen.hpp"
#include "Compiler/RoboXML.hpp"
#include "Platform/Windows/WindowsDocument.hpp"
#include "Platform/Windows/WindowsHost.hpp"
#include "Web/WebFoundation.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace Robix::UI {
namespace {

std::string Option(const std::vector<std::string>& args, const std::string& name, const std::string& fallback) {
    for (std::size_t index = 0; index + 1 < args.size(); ++index) {
        if (args[index] == name) {
            return args[index + 1];
        }
    }
    return fallback;
}

std::uint32_t PngCrc(const std::uint8_t* data, std::size_t length) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = (crc & 1u) ? 0xEDB88320u : 0u;
            crc = (crc >> 1) ^ mask;
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

void PngChunk(std::vector<std::uint8_t>& out, const char* type, const std::vector<std::uint8_t>& data) {
    const std::uint32_t length = static_cast<std::uint32_t>(data.size());
    out.push_back(static_cast<std::uint8_t>(length >> 24));
    out.push_back(static_cast<std::uint8_t>(length >> 16));
    out.push_back(static_cast<std::uint8_t>(length >> 8));
    out.push_back(static_cast<std::uint8_t>(length));
    const std::size_t start = out.size();
    out.push_back(static_cast<std::uint8_t>(type[0]));
    out.push_back(static_cast<std::uint8_t>(type[1]));
    out.push_back(static_cast<std::uint8_t>(type[2]));
    out.push_back(static_cast<std::uint8_t>(type[3]));
    out.insert(out.end(), data.begin(), data.end());
    const std::uint32_t crc = PngCrc(out.data() + start, out.size() - start);
    out.push_back(static_cast<std::uint8_t>(crc >> 24));
    out.push_back(static_cast<std::uint8_t>(crc >> 16));
    out.push_back(static_cast<std::uint8_t>(crc >> 8));
    out.push_back(static_cast<std::uint8_t>(crc));
}

std::uint32_t Adler32(const std::uint8_t* data, std::size_t length) {
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (std::size_t i = 0; i < length; ++i) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

bool WriteDesktopPng(const std::string& path, std::string& error) {
    constexpr int kWidth = 320;
    constexpr int kHeight = 200;
    constexpr int kBar = 32;
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(kHeight * (1 + kWidth * 3)));
    for (int y = 0; y < kHeight; ++y) {
        std::uint8_t* row = raw.data() + static_cast<std::size_t>(y * (1 + kWidth * 3));
        row[0] = 0;
        const bool bar = y < kBar;
        for (int x = 0; x < kWidth; ++x) {
            std::uint8_t r = bar ? 0x0E : 0x08;
            std::uint8_t g = bar ? 0x7A : 0x0A;
            std::uint8_t b = bar ? 0xFF : 0x10;
            if (bar && x >= 16 && x < 24 && y >= 8 && y < 16) {
                r = 0xFF;
                g = 0xFF;
                b = 0xFF;
            }
            std::uint8_t* pix = row + 1 + x * 3;
            pix[0] = r;
            pix[1] = g;
            pix[2] = b;
        }
    }
    std::vector<std::uint8_t> zlib;
    zlib.push_back(0x78);
    zlib.push_back(0x01);
    std::size_t offset = 0;
    while (offset < raw.size()) {
        const std::size_t chunk = std::min<std::size_t>(65535, raw.size() - offset);
        const bool last = offset + chunk == raw.size();
        zlib.push_back(last ? 1 : 0);
        zlib.push_back(static_cast<std::uint8_t>(chunk));
        zlib.push_back(static_cast<std::uint8_t>(chunk >> 8));
        const std::uint16_t neg = static_cast<std::uint16_t>(~chunk);
        zlib.push_back(static_cast<std::uint8_t>(neg));
        zlib.push_back(static_cast<std::uint8_t>(neg >> 8));
        zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                    raw.begin() + static_cast<std::ptrdiff_t>(offset + chunk));
        offset += chunk;
    }
    const std::uint32_t adler = Adler32(raw.data(), raw.size());
    zlib.push_back(static_cast<std::uint8_t>(adler >> 24));
    zlib.push_back(static_cast<std::uint8_t>(adler >> 16));
    zlib.push_back(static_cast<std::uint8_t>(adler >> 8));
    zlib.push_back(static_cast<std::uint8_t>(adler));

    std::vector<std::uint8_t> png = {137, 80, 78, 71, 13, 10, 26, 10};
    std::vector<std::uint8_t> ihdr = {
        0, 0, 1, 64,
        0, 0, 0, 200,
        8, 2, 0, 0, 0
    };
    PngChunk(png, "IHDR", ihdr);
    PngChunk(png, "IDAT", zlib);
    PngChunk(png, "IEND", {});
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "could not write '" + path + "'";
        return false;
    }
    out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    if (!out) {
        error = "could not finish '" + path + "'";
        return false;
    }
    return true;
}

bool Has(const std::vector<std::string>& args, const std::string& name) {
    for (const std::string& arg : args) {
        if (arg == name) {
            return true;
        }
    }
    return false;
}

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

std::string ReadText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void LowerKeys(RoboXMLNode& node) {
    std::vector<std::pair<std::string, std::string>> extra;
    for (const auto& entry : node.attrs) {
        const std::string lower = Lower(entry.first);
        if (node.attrs.find(lower) == node.attrs.end()) {
            extra.emplace_back(lower, entry.second);
        }
    }
    for (const auto& entry : extra) {
        node.attrs[entry.first] = entry.second;
    }
    for (RoboXMLNode& child : node.children) {
        LowerKeys(child);
    }
}

const RoboXMLNode* FindScreen(const RoboXMLNode& node) {
    return RoboScreenFind(node);
}

bool ApplyScreen(const std::string& document, WindowRunOptions& options) {
    const std::string source = ReadText(document);
    RoboXMLDocument xml;
    if (source.empty() || !RoboXMLParse(source, xml)) {
        return true;
    }
    RoboUICompose(xml.root, document);
    LowerKeys(xml.root);
    const RoboXMLNode* screen = FindScreen(xml.root);
    if (screen == nullptr) {
        return true;
    }
    RoboScreenRecord record;
    std::string diagnostic;
    if (!RoboScreenRead(*screen, record, diagnostic)) {
        std::cerr << "robix ui: " << diagnostic << "\n";
        return false;
    }
    options.screen = true;
    options.width = record.width;
    options.height = record.height;
    options.x = record.x;
    options.y = record.y;
    options.place = record.place;
    options.movable = record.movable;
    options.resizable = record.resizable;
    options.light = record.theme == "light";
    options.background = record.color;
    options.hasScreenColor = true;
    if (!record.title.empty()) {
        options.title = record.title;
    }
    return true;
}

}  // namespace

void LoadScreenOptions(const std::string& document, WindowRunOptions& options) {
    ApplyScreen(document, options);
}

int RunUiCommand(const std::vector<std::string>& args) {
    if (args.empty() || args[0] != "ui" || args.size() < 2) {
        return -1;
    }
    const std::string command = args[1];
    std::string document;
    for (std::size_t index = 2; index < args.size(); ++index) {
        if (args[index].size() > 0 && args[index][0] == '-') {
            if (index + 1 < args.size() && (args[index] == "-o" || args[index] == "--code" || args[index] == "--out")) {
                ++index;
            }
            continue;
        }
        document = args[index];
        break;
    }

    if (command == "cook") {
        if (document.empty()) {
            std::cerr << "robix ui: expected a .rxu file\n";
            return 1;
        }
        const std::string source = ReadText(document);
        if (source.empty()) {
            std::cerr << "robix ui: could not open '" << document << "'\n";
            return 1;
        }
        RoboXMLDocument xml;
        if (!RoboXMLParse(source, xml)) {
            std::cerr << "robix ui: the document is not valid XML\n";
            return 1;
        }
        const RoboXMLNode* screen = RoboScreenFind(xml.root);
        if (screen == nullptr) {
            std::cerr << "robix ui: R7004: the document has no Screen. Use Screen, Window, or Page.\n";
            return 1;
        }
        const auto length = [](const RoboXMLNode& node, const char* key, int fallback) {
            const std::string raw = RoboXMLAttr(node, key);
            if (raw.empty()) {
                return fallback;
            }
            try {
                return std::max(1, std::stoi(raw));
            } catch (...) {
                return fallback;
            }
        };
        const int viewWidth = length(*screen, "width", 320);
        const int viewHeight = length(*screen, "height", 200);
        std::vector<WindowBox> boxes;
        if (WindowsDocumentBoxes(document, viewWidth, viewHeight, boxes) < 0) {
            std::cerr << "robix ui: could not lay out '" << document << "'\n";
            return 1;
        }
        const std::string output = Option(args, "-o", "Build/RoboticelaOS/Desktop.rxul");
        std::error_code ec;
        fs::create_directories(fs::path(output).parent_path(), ec);
        std::ofstream out(output, std::ios::binary);
        if (!out) {
            std::cerr << "robix ui: could not write '" << output << "'\n";
            return 1;
        }
        out << "RXUL 1\n";
        for (const WindowBox& box : boxes) {
            out << box.kind << ' ' << box.x << ' ' << box.y << ' ' << box.width << ' ' << box.height << ' ' << box.text << '\n';
        }
        std::cout << "cooked " << boxes.size() << " " << output << "\n";
        return 0;
    }
    if (command == "frame") {
        const std::string output = Option(args, "-o", "Build/RoboticelaOS/Desktop.png");
        std::error_code ec;
        fs::create_directories(fs::path(output).parent_path(), ec);
        std::string error;
        if (!WriteDesktopPng(output, error)) {
            std::cerr << "robix ui: " << error << "\n";
            return 1;
        }
        std::cout << "png " << output << "\n";
        return 0;
    }
    if (command == "render") {
        const std::string output = Option(args, "-o", "framebuffer.bmp");
        std::string error;
        if (!WriteFoundationBmp(output, error)) {
            std::cerr << "robix ui: " << error << "\n";
            return 1;
        }
        std::cout << "bmp " << output << "\n";
        return 0;
    }
    if (command == "window") {
        WindowRunOptions options;
        options.stayOpen = Has(args, "--dev");
        if (!document.empty()) {
            if (!ApplyScreen(document, options)) {
                return 1;
            }
            options.document = document;
        }
        return RunWindowsFoundation(options);
    }
    if (command == "web") {
        const std::string output = Option(args, "-o", "Build/web");
        std::string error;
        if (!WriteWebFoundation(output, document, error, {}, true)) {
            std::cerr << "robix ui: " << error << "\n";
            return 1;
        }
        return 0;
    }
    if (command == "validate") {
        if (document.empty()) {
            std::cerr << "robix ui: expected a .rxu file\n";
            return 1;
        }
        const std::string source = ReadText(document);
        if (source.empty()) {
            std::cerr << "robix ui: could not open '" << document << "'\n";
            return 1;
        }
        RoboXMLDocument xml;
        if (!RoboXMLParse(source, xml)) {
            std::cerr << "robix ui: the document is not valid XML\n";
            return 1;
        }
        const RoboXMLNode* screen = RoboScreenFind(xml.root);
        if (screen == nullptr) {
            std::cerr << "robix ui: R7004: the document has no Screen. Use Screen, Window, or Page.\n";
            return 1;
        }
        RoboScreenRecord record;
        std::string diagnostic;
        if (!RoboScreenRead(*screen, record, diagnostic)) {
            std::cerr << "robix ui: " << diagnostic << "\n";
            return 1;
        }
        std::cout << "screen validated " << document << " " << record.width << "x" << record.height << "\n";
        return 0;
    }
    return -1;
}

}  // namespace Robix::UI
