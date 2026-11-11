#include "../../RobixUI/Compiler/Components/Image/RoboImage.hpp"
#include "../../RobixUI/Compiler/RoboXML.hpp"
#include "../../RobixUI/RoboShared/RoboImage.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace {

int gPassed = 0;
int gFailed = 0;

void Check(bool condition, const char* description) {
    if (condition) {
        ++gPassed;
        return;
    }
    ++gFailed;
    std::cerr << "[FAIL] " << description << "\n";
}

bool Load(const std::string& source, RoboImageRecord& image, std::string& diagnostic) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        diagnostic = "parse failed";
        return false;
    }
    return RoboImageRead(document.root, image, diagnostic);
}

template <typename T>
void Append(std::vector<RoboU8>& bytes, const T& value) {
    const auto* raw = reinterpret_cast<const RoboU8*>(&value);
    bytes.insert(bytes.end(), raw, raw + sizeof(T));
}

void PutPixel(std::vector<RoboU8>& bytes, RoboU8 red, RoboU8 green, RoboU8 blue, RoboU8 alpha) {
    bytes.push_back(red);
    bytes.push_back(green);
    bytes.push_back(blue);
    bytes.push_back(alpha);
}

std::vector<RoboU8> Fixture() {
    std::vector<RoboU8> bytes;
    RoboImageHeader header{};
    header.magic = ROBO_IMAGE_MAGIC;
    header.version = ROBO_IMAGE_VERSION;
    header.width = 2;
    header.height = 2;
    header.stride = 8;
    header.pixelFormat = ROBO_PIXEL_FORMAT_RGBA8;
    header.flags = 0;
    header.sourceDpi = 96;
    Append(bytes, header);
    PutPixel(bytes, 255, 0, 0, 255);
    PutPixel(bytes, 0, 0, 255, 255);
    PutPixel(bytes, 0, 255, 0, 255);
    PutPixel(bytes, 0, 0, 0, 0);
    return bytes;
}

void TestSchema() {
    RoboImageRecord image;
    std::string diagnostic;
    Check(Load("<Image Source=\"studio.roboimg\" Fit=\"cover\" Alt=\"Studio\" Width=\"40\" Height=\"24\" OnLoad=\"ready\" OnError=\"missing\"/>",
               image, diagnostic),
          "an Image is valid");
    Check(image.source == "studio.roboimg", "source is kept");
    Check(image.fit == "cover", "fit is kept");
    Check(image.alt == "Studio", "alt is kept");
    Check(image.width == 40 && image.hasWidth, "width is the frame");
    Check(image.height == 24 && image.hasHeight, "height is the frame");
    Check(image.onLoad == "ready", "OnLoad names the handler");
    Check(image.onError == "missing", "OnError names the handler");

    Check(Load("<Image Src=\"photo.roboimg\" ObjectFit=\"fill\" Description=\"Photo\"/>", image, diagnostic), "the aliases are valid");
    Check(image.source == "photo.roboimg", "src is source");
    Check(image.fit == "fill", "objectFit is fit");
    Check(image.alt == "Photo", "description is alt");

    Check(Load("<Image Source=\"a.roboimg\" Fit=\"contain\"/>", image, diagnostic), "contain is valid");
    Check(image.fit == "contain", "contain is kept");
    Check(Load("<Image Source=\"a.roboimg\" Fit=\"none\"/>", image, diagnostic), "none is valid");
    Check(image.fit == "none", "none is kept");
    Check(Load("<Image Source=\"a.roboimg\" Fit=\"scale-down\"/>", image, diagnostic), "scale-down is valid");
    Check(image.fit == "scale-down", "scale-down is kept");

    Check(Load("<Image/>", image, diagnostic), "an empty Image is valid");
    Check(image.source.empty() && image.alt.empty(), "source and alt default to empty");
    Check(image.fit == "contain", "fit defaults to contain");
    Check(!image.hasWidth && !image.hasHeight, "the frame defaults to the decoded size");

    Check(!Load("<Image NotAReal=\"1\"/>", image, diagnostic), "an unknown attribute is rejected");
    Check(diagnostic.find("R7301") != std::string::npos, "an unknown attribute uses R7301");
    Check(!Load("<Image Fit=\"tile\"/>", image, diagnostic), "a bad fit is rejected");
    Check(diagnostic.find("R7302") != std::string::npos, "a bad fit uses R7302");
    Check(!Load("<Image Width=\"wide\"/>", image, diagnostic), "a bad width is rejected");
    Check(diagnostic.find("R7303") != std::string::npos, "a bad width uses R7303");
    Check(!Load("<Image Width=\"0\"/>", image, diagnostic), "a zero width is rejected");
    Check(diagnostic.find("R7303") != std::string::npos, "a zero width uses R7303");
    Check(!Load("<Image Height=\"-4\"/>", image, diagnostic), "a negative height is rejected");
    Check(diagnostic.find("R7303") != std::string::npos, "a negative height uses R7303");
    Check(!Load("<View/>", image, diagnostic), "View is not an Image");
    Check(diagnostic.find("R7304") != std::string::npos, "the wrong tag uses R7304");
    Check(!Load("<Image><Text Value=\"A\"/></Image>", image, diagnostic), "a child is rejected");
    Check(diagnostic.find("R7304") != std::string::npos, "a child uses R7304");
}

void TestDecodeAndPixel() {
    const std::vector<RoboU8> bytes = Fixture();
    Check(RoboImageValidate(bytes.data(), bytes.size()) == ROBO_TRUE, "the fixture image is valid");

    RoboImageRecord image;
    std::string diagnostic;
    Check(Load("<Image Source=\"fixture.roboimg\" Alt=\"Swatch\" Fit=\"none\"/>", image, diagnostic), "the fixture reads");
    Check(RoboImageDecode(image, bytes.data(), bytes.size()), "the fixture decodes");
    Check(image.format == "robi", "the fixture format is the Roboticela bitmap");
    Check(image.fired.size() == 1 && image.fired[0] == "OnLoad", "OnLoad fires");
    Check(image.sourceWidth == 2 && image.sourceHeight == 2, "the decoded size is 2 by 2");

    RoboImageFrame frame;
    RoboImagePaint(image, frame);
    RoboU32 color = 0;
    Check(RoboImagePixel(frame, 0, 0, color) && color == 0xFFFF0000u, "the origin pixel is red");
    Check(RoboImagePixel(frame, 1, 0, color) && color == 0xFF0000FFu, "the next pixel is blue");
    Check(RoboImagePixel(frame, 0, 1, color) && color == 0xFF00FF00u, "the pixel below is green");
    Check(RoboImagePixel(frame, 1, 1, color) && color == 0, "the last pixel is clear");
    Check(!RoboImagePixel(frame, frame.width, 0, color), "a pixel outside the frame is rejected");

    Check(Load("<Image Source=\"fixture.roboimg\" Fit=\"contain\" Width=\"6\" Height=\"4\"/>", image, diagnostic), "contain reads");
    Check(RoboImageDecode(image, bytes.data(), bytes.size()), "contain decodes");
    RoboImagePaint(image, frame);
    Check(frame.width == 6 && frame.height == 4, "contain keeps the frame");
    Check(RoboImagePixel(frame, 0, 0, color) && color == 0, "contain leaves the leading edge clear");
    Check(RoboImagePixel(frame, 1, 0, color) && color == 0xFFFF0000u, "contain places the red pixel inside the frame");
    Check(RoboImagePixel(frame, 3, 0, color) && color == 0xFF0000FFu, "contain places the blue pixel beside it");

    Check(Load("<Image Source=\"fixture.roboimg\" Fit=\"cover\" Width=\"2\" Height=\"4\"/>", image, diagnostic), "cover reads");
    Check(RoboImageDecode(image, bytes.data(), bytes.size()), "cover decodes");
    RoboImagePaint(image, frame);
    Check(frame.width == 2 && frame.height == 4, "cover keeps the frame");
    Check(RoboImagePixel(frame, 0, 0, color) && color == 0xFFFF0000u, "cover crops to the red pixel");
    Check(RoboImagePixel(frame, 1, 0, color) && color == 0xFF0000FFu, "cover keeps the blue pixel beside it");

    Check(Load("<Image Source=\"fixture.roboimg\" Fit=\"fill\" Width=\"4\" Height=\"2\"/>", image, diagnostic), "fill reads");
    Check(RoboImageDecode(image, bytes.data(), bytes.size()), "fill decodes");
    RoboImagePaint(image, frame);
    Check(RoboImagePixel(frame, 0, 0, color) && color == 0xFFFF0000u, "fill keeps the origin pixel");
    Check(RoboImagePixel(frame, 2, 0, color) && color == 0xFF0000FFu, "fill stretches the blue pixel");

    Check(Load("<Image Source=\"fixture.roboimg\" Fit=\"scale-down\" Width=\"8\" Height=\"8\"/>", image, diagnostic), "scale-down reads");
    Check(RoboImageDecode(image, bytes.data(), bytes.size()), "scale-down decodes");
    RoboImagePaint(image, frame);
    Check(RoboImagePixel(frame, 0, 0, color) && color == 0, "scale-down does not enlarge into the corner");
    Check(RoboImagePixel(frame, 3, 3, color) && color == 0xFFFF0000u, "scale-down centers the original pixel");

    Check(Load("<Image Source=\"missing.roboimg\"/>", image, diagnostic), "a missing picture still reads");
    Check(!RoboImageDecode(image, bytes.data(), 4), "a short buffer does not decode");
    Check(image.fired.size() == 1 && image.fired[0] == "OnError", "OnError fires");
    Check(image.pixels.empty(), "a failed decode keeps no pixels");
    Check(!RoboImageDecode(image, nullptr, 0), "an empty buffer does not decode");
    Check(image.fired.size() == 1 && image.fired[0] == "OnError", "OnError fires again");
}

std::uint32_t Checksum(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

std::uint32_t Adler(const std::uint8_t* data, std::size_t size) {
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (std::size_t index = 0; index < size; ++index) {
        a = (a + data[index]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

void PutBe(std::vector<RoboU8>& out, std::uint32_t value) {
    out.push_back(static_cast<RoboU8>((value >> 24) & 0xFF));
    out.push_back(static_cast<RoboU8>((value >> 16) & 0xFF));
    out.push_back(static_cast<RoboU8>((value >> 8) & 0xFF));
    out.push_back(static_cast<RoboU8>(value & 0xFF));
}

void PngChunk(std::vector<RoboU8>& out, const char* type, const std::uint8_t* data, std::size_t size) {
    PutBe(out, static_cast<std::uint32_t>(size));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data, data + size);
    PutBe(out, Checksum(out.data() + start, out.size() - start));
}

std::vector<RoboU8> SwatchPng() {
    const std::uint8_t raw[] = {0, 255, 0, 0, 255, 0, 0, 255, 255, 0, 0, 255, 0, 255, 0, 0, 0, 0};
    std::vector<RoboU8> zlib;
    zlib.push_back(0x78);
    zlib.push_back(0x01);
    zlib.push_back(0x01);
    zlib.push_back(18);
    zlib.push_back(0);
    zlib.push_back(static_cast<RoboU8>(~18));
    zlib.push_back(0xFF);
    zlib.insert(zlib.end(), raw, raw + sizeof(raw));
    PutBe(zlib, Adler(raw, sizeof(raw)));
    const std::uint8_t header[] = {0, 0, 0, 2, 0, 0, 0, 2, 8, 6, 0, 0, 0};
    std::vector<RoboU8> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    PngChunk(png, "IHDR", header, sizeof(header));
    PngChunk(png, "IDAT", zlib.data(), zlib.size());
    PngChunk(png, "IEND", nullptr, 0);
    return png;
}

std::vector<RoboU8> SwatchBmp() {
    std::vector<RoboU8> bmp(70, 0);
    bmp[0] = 'B';
    bmp[1] = 'M';
    bmp[2] = 70;
    bmp[10] = 54;
    bmp[14] = 40;
    bmp[18] = 2;
    bmp[22] = 2;
    bmp[26] = 1;
    bmp[28] = 32;
    const RoboU8 pixels[] = {0, 255, 0, 255, 0, 0, 0, 0, 0, 0, 255, 255, 255, 0, 0, 255};
    for (int index = 0; index < 16; ++index) {
        bmp[54 + index] = pixels[index];
    }
    return bmp;
}

std::vector<RoboU8> SwatchWebp() {
    const RoboU8 bytes[] = {
        0x52, 0x49, 0x46, 0x46, 0x30, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50, 0x56, 0x50, 0x38, 0x4C, 0x23, 0x00, 0x00, 0x00, 0x2F,
        0x01, 0x40, 0x00, 0x10, 0x1F, 0x20, 0x10, 0x48, 0xDA, 0x1F, 0x7A, 0x8D, 0xF9, 0x17, 0x10, 0x14, 0xF9, 0x3F, 0xDA, 0xFC, 0x07,
        0xF0, 0xA5, 0x40, 0x41, 0xDA, 0x06, 0x2C, 0xDA, 0x5F, 0x44, 0xFF, 0xE3, 0x02, 0x00,
    };
    return std::vector<RoboU8>(bytes, bytes + sizeof(bytes));
}

std::vector<RoboU8> AnimatedGif() {
    const RoboU8 bytes[] = {
        0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x02, 0x00, 0x02, 0x00, 0xF0, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0xFF, 0x0B,
        0x4E, 0x45, 0x54, 0x53, 0x43, 0x41, 0x50, 0x45, 0x32, 0x2E, 0x30, 0x03, 0x01, 0x00, 0x00, 0x00, 0x21, 0xF9, 0x04, 0x08, 0x0A, 0x00,
        0x00, 0x00, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x02, 0x02, 0x84, 0x51, 0x00, 0x21, 0xF9, 0x04, 0x08, 0x0A,
        0x00, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x80, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x02, 0x02, 0x84,
        0x51, 0x00, 0x3B,
    };
    return std::vector<RoboU8>(bytes, bytes + sizeof(bytes));
}

std::vector<RoboU8> AnimatedWebp() {
    const RoboU8 bytes[] = {
        0x52, 0x49, 0x46, 0x46, 0x84, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50, 0x56, 0x50, 0x38, 0x58, 0x0A, 0x00, 0x00, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x41, 0x4E, 0x49, 0x4D, 0x06, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
        0x41, 0x4E, 0x4D, 0x46, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x64, 0x00,
        0x00, 0x02, 0x56, 0x50, 0x38, 0x4C, 0x0F, 0x00, 0x00, 0x00, 0x2F, 0x01, 0x40, 0x00, 0x00, 0x07, 0x10, 0xFD, 0x8F, 0xFE, 0x07, 0x22,
        0xA2, 0xFF, 0x01, 0x00, 0x41, 0x4E, 0x4D, 0x46, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4C, 0x0F, 0x00, 0x00, 0x00, 0x2F, 0x01, 0x40, 0x00, 0x00, 0x07, 0x10, 0xD1,
        0xFF, 0xFE, 0x07, 0x22, 0xA2, 0xFF, 0x01, 0x00,
    };
    return std::vector<RoboU8>(bytes, bytes + sizeof(bytes));
}

std::vector<RoboU8> EncodeHeic() {
    const HRESULT started = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool release = SUCCEEDED(started);
    std::vector<RoboU8> bytes;
    {
        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
        Microsoft::WRL::ComPtr<IStream> stream;
        Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
        Microsoft::WRL::ComPtr<IPropertyBag2> bag;
        Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(factory->CreateBitmap(8, 8, GUID_WICPixelFormat24bppBGR, WICBitmapCacheOnLoad, &bitmap))) {
            WICRect rect{0, 0, 8, 8};
            Microsoft::WRL::ComPtr<IWICBitmapLock> lock;
            UINT stride = 0;
            UINT bufferSize = 0;
            BYTE* buffer = nullptr;
            if (SUCCEEDED(bitmap->Lock(&rect, WICBitmapLockWrite, &lock)) && SUCCEEDED(lock->GetStride(&stride)) &&
                SUCCEEDED(lock->GetDataPointer(&bufferSize, &buffer))) {
                for (UINT row = 0; row < 8; ++row) {
                    for (UINT column = 0; column < 8; ++column) {
                        buffer[row * stride + column * 3] = 0;
                        buffer[row * stride + column * 3 + 1] = 0;
                        buffer[row * stride + column * 3 + 2] = 255;
                    }
                }
            }
            lock.Reset();
            if (SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) && SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatHeif, nullptr, &encoder)) &&
                SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) && SUCCEEDED(encoder->CreateNewFrame(&frame, &bag)) &&
                SUCCEEDED(frame->Initialize(bag.Get())) && SUCCEEDED(frame->WriteSource(bitmap.Get(), nullptr)) && SUCCEEDED(frame->Commit()) &&
                SUCCEEDED(encoder->Commit())) {
                HGLOBAL memory = nullptr;
                if (SUCCEEDED(GetHGlobalFromStream(stream.Get(), &memory)) && memory != nullptr) {
                    const void* view = GlobalLock(memory);
                    const std::size_t count = static_cast<std::size_t>(GlobalSize(memory));
                    if (view != nullptr && count > 0) {
                        const auto* start = static_cast<const RoboU8*>(view);
                        bytes.assign(start, start + count);
                        GlobalUnlock(memory);
                    }
                }
            }
        }
        frame.Reset();
        bag.Reset();
        encoder.Reset();
        stream.Reset();
        bitmap.Reset();
        factory.Reset();
    }
    if (release) {
        CoUninitialize();
    }
    return bytes;
}

std::vector<RoboU8> EncodeSwatch(const GUID& container, bool flat) {
    const HRESULT started = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool release = SUCCEEDED(started);
    std::vector<RoboU8> bytes;
    {
        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
        Microsoft::WRL::ComPtr<IStream> stream;
        Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
        Microsoft::WRL::ComPtr<IPropertyBag2> bag;
        Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
            factory.Reset();
        } else if (FAILED(factory->CreateBitmap(flat ? 16 : 2, flat ? 16 : 2, GUID_WICPixelFormat32bppRGBA, WICBitmapCacheOnLoad, &bitmap))) {
            bitmap.Reset();
        } else {
            const int edge = flat ? 16 : 2;
            WICRect rect{0, 0, edge, edge};
            Microsoft::WRL::ComPtr<IWICBitmapLock> lock;
            UINT stride = 0;
            UINT bufferSize = 0;
            BYTE* buffer = nullptr;
            if (SUCCEEDED(bitmap->Lock(&rect, WICBitmapLockWrite, &lock)) && SUCCEEDED(lock->GetStride(&stride)) &&
                SUCCEEDED(lock->GetDataPointer(&bufferSize, &buffer)) && stride >= static_cast<UINT>(edge * 4) &&
                bufferSize >= stride * static_cast<UINT>(edge)) {
                const RoboU8 pixels[] = {255, 0, 0, 255, 0, 0, 255, 255, 0, 255, 0, 255, 0, 0, 0, 0};
                for (int row = 0; row < edge; ++row) {
                    for (int column = 0; column < edge; ++column) {
                        const int source = flat ? 0 : (row * 2 + column) * 4;
                        buffer[row * stride + column * 4] = pixels[source];
                        buffer[row * stride + column * 4 + 1] = pixels[source + 1];
                        buffer[row * stride + column * 4 + 2] = pixels[source + 2];
                        buffer[row * stride + column * 4 + 3] = pixels[source + 3];
                    }
                }
            }
            lock.Reset();
            if (SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) && SUCCEEDED(factory->CreateEncoder(container, nullptr, &encoder)) &&
                SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) && SUCCEEDED(encoder->CreateNewFrame(&frame, &bag))) {
                if (container == GUID_ContainerFormatJpeg && bag) {
                    PROPBAG2 option{};
                    option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
                    VARIANT quality;
                    VariantInit(&quality);
                    quality.vt = VT_R4;
                    quality.fltVal = 1.0f;
                    bag->Write(1, &option, &quality);
                    VariantClear(&quality);
                }
                if (SUCCEEDED(frame->Initialize(bag.Get())) && SUCCEEDED(frame->WriteSource(bitmap.Get(), nullptr)) && SUCCEEDED(frame->Commit()) &&
                    SUCCEEDED(encoder->Commit())) {
                    HGLOBAL memory = nullptr;
                    if (SUCCEEDED(GetHGlobalFromStream(stream.Get(), &memory)) && memory != nullptr) {
                        const std::size_t count = static_cast<std::size_t>(GlobalSize(memory));
                        const void* view = GlobalLock(memory);
                        if (view != nullptr && count > 0) {
                            const auto* start = static_cast<const RoboU8*>(view);
                            bytes.assign(start, start + count);
                            GlobalUnlock(memory);
                        }
                    }
                }
            }
        }
        frame.Reset();
        bag.Reset();
        encoder.Reset();
        stream.Reset();
        bitmap.Reset();
        factory.Reset();
    }
    if (release) {
        CoUninitialize();
    }
    return bytes;
}

void ExpectSwatch(const char* format, const std::vector<RoboU8>& bytes, bool exact, bool keepClear) {
    RoboImageRecord image;
    std::string diagnostic;
    const std::string label = std::string(format) + " reads";
    Check(Load(std::string("<Image Source=\"swatch.") + format + "\" Fit=\"none\"/>", image, diagnostic), label.c_str());
    const std::string decoded = std::string(format) + " decodes";
    Check(!bytes.empty() && RoboImageDecode(image, bytes.data(), bytes.size()), decoded.c_str());
    const std::string named = std::string(format) + " keeps its format";
    Check(image.format == format, named.c_str());
    const std::string loaded = std::string(format) + " fires OnLoad";
    Check(image.fired.size() == 1 && image.fired[0] == "OnLoad", loaded.c_str());
    RoboImageFrame frame;
    RoboImagePaint(image, frame);
    RoboU32 color = 0;
    const bool origin = RoboImagePixel(frame, 0, 0, color);
    if (exact) {
        const std::string red = std::string(format) + " origin is red";
        Check(origin && color == 0xFFFF0000u, red.c_str());
        const std::string blue = std::string(format) + " next pixel is blue";
        Check(RoboImagePixel(frame, 1, 0, color) && color == 0xFF0000FFu, blue.c_str());
        const std::string green = std::string(format) + " pixel below is green";
        Check(RoboImagePixel(frame, 0, 1, color) && color == 0xFF00FF00u, green.c_str());
        const bool clearPixel = RoboImagePixel(frame, 1, 1, color);
        const std::string clear = std::string(format) + " last pixel is clear";
        const RoboU32 clearColor = keepClear ? 0u : (color & 0xFF000000u);
        Check(clearPixel && color == clearColor, clear.c_str());
    } else {
        const std::string red = std::string(format) + " origin stays red";
        const int redChannel = static_cast<int>((color >> 16) & 0xFF);
        const int greenChannel = static_cast<int>((color >> 8) & 0xFF);
        const int blueChannel = static_cast<int>(color & 0xFF);
        Check(origin && redChannel >= 250 && greenChannel <= 5 && blueChannel <= 5, red.c_str());
        if (std::string(format) == "jpeg") {
            const std::string opaque = std::string(format) + " has no transparency";
            Check(((color >> 24) & 255u) == 255u, opaque.c_str());
        }
    }
}

void TestFormats() {
    const std::vector<RoboU8> png = SwatchPng();
    ExpectSwatch("png", png, true, true);
    ExpectSwatch("bmp", SwatchBmp(), true, false);
    ExpectSwatch("jpeg", EncodeSwatch(GUID_ContainerFormatJpeg, true), false, false);
    ExpectSwatch("webp", SwatchWebp(), true, true);
    ExpectSwatch("gif", EncodeSwatch(GUID_ContainerFormatGif, false), true, true);
    ExpectSwatch("tiff", EncodeSwatch(GUID_ContainerFormatTiff, false), true, true);

    const std::string svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"2\" height=\"2\" viewBox=\"0 0 2 2\">"
        "<rect x=\"-1\" y=\"-1\" width=\"4\" height=\"2\" fill=\"#FF0000\"/>"
        "<rect x=\"-1\" y=\"1\" width=\"2\" height=\"2\" fill=\"#00FF00\"/>"
        "<rect x=\"1\" y=\"-1\" width=\"2\" height=\"2\" fill=\"#0000FF\"/>"
        "</svg>";
    std::vector<RoboU8> svgBytes(svg.begin(), svg.end());
    RoboImageRecord image;
    std::string diagnostic;
    Check(Load("<Image Source=\"swatch.svg\" Fit=\"none\"/>", image, diagnostic), "svg reads");
    Check(RoboImageDecode(image, svgBytes.data(), svgBytes.size()), "svg decodes");
    Check(image.format == "svg", "svg keeps its format");
    Check(image.fired.size() == 1 && image.fired[0] == "OnLoad", "svg fires OnLoad");
    RoboImageFrame frame;
    RoboImagePaint(image, frame);
    RoboU32 color = 0;
    Check(RoboImagePixel(frame, 0, 0, color) && color == 0xFFFF0000u, "svg origin is red");
    Check(RoboImagePixel(frame, 1, 0, color) && color == 0xFF0000FFu, "svg next pixel is blue");
    Check(RoboImagePixel(frame, 0, 1, color) && color == 0xFF00FF00u, "svg pixel below is green");

    const std::string sharp =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"4\" height=\"4\" viewBox=\"0 0 4 4\">"
        "<polygon points=\"0,0 4,0 0,4\" fill=\"#FF0000\"/>"
        "</svg>";
    std::vector<RoboU8> sharpBytes(sharp.begin(), sharp.end());
    Check(Load("<Image Source=\"mark.svg\" Width=\"32\" Height=\"32\" Fit=\"contain\"/>", image, diagnostic), "an enlarged svg reads");
    Check(RoboImageDecode(image, sharpBytes.data(), sharpBytes.size()), "an enlarged svg decodes");
    RoboImagePaint(image, frame);
    Check(frame.width == 32 && frame.height == 32, "svg paints at the enlarged size");
    Check(RoboImagePixel(frame, 0, 0, color) && color == 0xFFFF0000u, "the enlarged svg keeps the red corner");
    Check(RoboImagePixel(frame, 16, 0, color) && color == 0xFFFF0000u, "the enlarged svg keeps the top edge");
    Check(RoboImagePixel(frame, 0, 16, color) && color == 0xFFFF0000u, "the enlarged svg keeps the left edge");
    Check(RoboImagePixel(frame, 4, 4, color) && color == 0xFFFF0000u, "the enlarged svg stays sharp inside the triangle");
    Check(RoboImagePixel(frame, 28, 28, color) && (color & 0x00FFFFFFu) == 0, "the enlarged svg stays clear outside the triangle");

    const std::vector<RoboU8> gif = AnimatedGif();
    Check(Load("<Image Source=\"motion.gif\"/>", image, diagnostic), "an animated gif reads");
    Check(RoboImageDecode(image, gif.data(), gif.size()), "an animated gif decodes");
    Check(image.format == "gif" && image.frameCount == 2, "gif keeps both frames");
    Check(image.pixels[0] == 0xFFFF0000u, "gif frame one is red");
    Check(RoboImageFrameDelay(image, 0) == 100 && RoboImageFrameDelay(image, 1) == 100, "gif frame delay is 100 milliseconds");
    Check(RoboImageSelectFrame(image, 1) && image.pixels[0] == 0xFF0000FFu, "gif frame two is blue");
    Check(RoboImageSelectFrame(image, 0) && image.pixels[0] == 0xFFFF0000u, "gif returns to frame one");
    Check(!RoboImageSelectFrame(image, 2), "a gif frame past the end is rejected");

    const std::vector<RoboU8> webp = AnimatedWebp();
    Check(Load("<Image Source=\"motion.webp\"/>", image, diagnostic), "an animated webp reads");
    Check(RoboImageDecode(image, webp.data(), webp.size()), "an animated webp decodes");
    Check(image.format == "webp" && image.frameCount == 2, "webp keeps both frames");
    Check(image.pixels[0] == 0xFFFF0000u, "webp frame one is red");
    Check(RoboImageFrameDelay(image, 0) == 100, "webp frame delay is 100 milliseconds");
    Check(RoboImageSelectFrame(image, 1) && image.pixels[0] == 0xFF0000FFu, "webp frame two is blue");

    const std::vector<RoboU8> heic = EncodeHeic();
    Check(Load("<Image Source=\"photo.heic\"/>", image, diagnostic), "heic reads");
    Check(!heic.empty() && RoboImageDecode(image, heic.data(), heic.size()), "heic decodes");
    Check(image.format == "heic", "heic keeps its format");
    Check(image.frameCount == 1, "heic is a still picture");
    const RoboU32 heicPixel = image.pixels.empty() ? 0u : image.pixels[0];
    const int heicRed = static_cast<int>((heicPixel >> 16) & 0xFF);
    const int heicGreen = static_cast<int>((heicPixel >> 8) & 0xFF);
    const int heicBlue = static_cast<int>(heicPixel & 0xFF);
    Check(!image.pixels.empty() && heicRed >= 240 && heicGreen <= 16 && heicBlue <= 16 && ((heicPixel >> 24) & 0xFF) == 255, "heic origin stays red");

    const std::vector<RoboU8> broken = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    Check(!RoboImageDecode(image, broken.data(), broken.size()), "a broken png does not decode");
    Check(image.fired.size() == 1 && image.fired[0] == "OnError", "a broken png fires OnError");
}

}  // namespace

int main() {
    TestSchema();
    TestDecodeAndPixel();
    TestFormats();
    if (gFailed != 0) {
        std::cerr << gFailed << " failed, " << gPassed << " passed\n";
        return 1;
    }
    std::cout << gPassed << " passed\n";
    return 0;
}
