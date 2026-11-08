#include "ImageResize.hpp"

#include <iostream>
#include <vector>

#ifndef _WIN32

std::vector<RoboImageVariant> RoboImageWriteSizes(const std::filesystem::path& source, const std::filesystem::path& folder,
                                                  const std::string& stem, bool resize) {
    (void)resize;
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    const std::string name = stem + source.extension().string();
    std::filesystem::copy_file(source, folder / name, std::filesystem::copy_options::overwrite_existing, error);
    if (error) return {};
    return {RoboImageVariant{"images/" + name, 0}};
}

#else

#include <wincodec.h>
#include <wrl/client.h>

#include <cmath>

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

namespace {

const int kWidths[] = {320, 480, 640, 768, 960, 1280, 1600, 1920};

bool Ok(HRESULT result) { return SUCCEEDED(result); }

ComPtr<IWICImagingFactory> Factory() {
    const HRESULT start = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    (void)start;
    ComPtr<IWICImagingFactory> factory;
    if (!Ok(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
        return {};
    }
    return factory;
}

bool HasAlpha(IWICImagingFactory* factory, IWICBitmapSource* source, UINT width, UINT height) {
    ComPtr<IWICFormatConverter> converter;
    if (!Ok(factory->CreateFormatConverter(&converter))) return false;
    if (!Ok(converter->Initialize(source, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                  WICBitmapPaletteTypeCustom))) {
        return false;
    }
    const UINT stride = width * 4;
    std::vector<BYTE> pixels(static_cast<std::size_t>(stride) * height);
    if (!Ok(converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()))) return false;
    for (std::size_t i = 3; i < pixels.size(); i += 4) {
        if (pixels[i] != 255) return true;
    }
    return false;
}

bool Save(IWICImagingFactory* factory, IWICBitmapSource* bitmap, const fs::path& destination, bool alpha) {
    ComPtr<IWICStream> stream;
    if (!Ok(factory->CreateStream(&stream))) return false;
    if (!Ok(stream->InitializeFromFilename(destination.wstring().c_str(), GENERIC_WRITE))) return false;
    ComPtr<IWICBitmapEncoder> encoder;
    const GUID format = alpha ? GUID_ContainerFormatPng : GUID_ContainerFormatJpeg;
    if (!Ok(factory->CreateEncoder(format, nullptr, &encoder))) return false;
    if (!Ok(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) return false;
    ComPtr<IPropertyBag2> bag;
    ComPtr<IWICBitmapFrameEncode> frame;
    if (!Ok(encoder->CreateNewFrame(&frame, &bag))) return false;
    if (!alpha && bag) {
        PROPBAG2 option{};
        option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT quality;
        VariantInit(&quality);
        quality.vt = VT_R4;
        quality.fltVal = 0.82f;
        bag->Write(1, &option, &quality);
        VariantClear(&quality);
    }
    if (!Ok(frame->Initialize(bag.Get()))) return false;
    UINT width = 0;
    UINT height = 0;
    bitmap->GetSize(&width, &height);
    frame->SetSize(width, height);
    WICPixelFormatGUID pixel = alpha ? GUID_WICPixelFormat32bppPBGRA : GUID_WICPixelFormat24bppBGR;
    frame->SetPixelFormat(&pixel);
    ComPtr<IWICFormatConverter> converter;
    if (!Ok(factory->CreateFormatConverter(&converter))) return false;
    if (!Ok(converter->Initialize(bitmap, pixel, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return false;
    if (!Ok(frame->WriteSource(converter.Get(), nullptr))) return false;
    if (!Ok(frame->Commit())) return false;
    return Ok(encoder->Commit());
}

bool Scale(IWICImagingFactory* factory, IWICBitmapSource* source, UINT width, UINT height, ComPtr<IWICBitmapSource>& scaled) {
    ComPtr<IWICBitmapScaler> scaler;
    if (!Ok(factory->CreateBitmapScaler(&scaler))) return false;
    if (!Ok(scaler->Initialize(source, width, height, WICBitmapInterpolationModeFant))) return false;
    scaled = scaler;
    return true;
}

}  // namespace

std::vector<RoboImageVariant> RoboImageWriteSizes(const fs::path& source, const fs::path& folder, const std::string& stem, bool resize) {
    std::error_code error;
    fs::create_directories(folder, error);
    const std::string originalName = stem + source.extension().string();
    const fs::path originalDest = folder / originalName;
    fs::copy_file(source, originalDest, fs::copy_options::overwrite_existing, error);
    if (error) return {};
    RoboImageVariant original{"images/" + originalName, 0};
    if (!resize) return {original};

    ComPtr<IWICImagingFactory> factory = Factory();
    if (!factory) {
        std::cerr << "warning: image '" << source.filename().string() << "' was copied without smaller sizes.\n";
        return {original};
    }
    ComPtr<IWICBitmapDecoder> decoder;
    if (!Ok(factory->CreateDecoderFromFilename(source.wstring().c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))) {
        std::cerr << "warning: image '" << source.filename().string() << "' could not be read for resizing and was copied as itself.\n";
        return {original};
    }
    ComPtr<IWICBitmapFrameDecode> frame;
    if (!Ok(decoder->GetFrame(0, &frame))) return {original};
    UINT width = 0;
    UINT height = 0;
    frame->GetSize(&width, &height);
    original.width = static_cast<int>(width);
    if (width < 2 || height < 2) return {original};
    const bool alpha = HasAlpha(factory.Get(), frame.Get(), width, height);
    const std::string extension = alpha ? ".png" : ".jpg";
    std::vector<RoboImageVariant> files;
    for (int target : kWidths) {
        if (static_cast<UINT>(target) >= width) continue;
        const UINT scaledWidth = static_cast<UINT>(target);
        const UINT scaledHeight = static_cast<UINT>(std::lround(static_cast<double>(height) * target / width));
        const UINT safeHeight = scaledHeight < 1 ? 1 : scaledHeight;
        ComPtr<IWICBitmapSource> scaled;
        if (!Scale(factory.Get(), frame.Get(), scaledWidth, safeHeight, scaled)) continue;
        const std::string name = stem + "-" + std::to_string(target) + extension;
        if (!Save(factory.Get(), scaled.Get(), folder / name, alpha)) continue;
        files.push_back(RoboImageVariant{"images/" + name, target});
    }
    files.push_back(original);
    return files;
}

#endif
