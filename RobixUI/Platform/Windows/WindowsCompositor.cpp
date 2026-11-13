#include "WindowsCompositor.hpp"

#define NOMINMAX
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>

#include <cmath>
#include <cstring>

namespace Robix::UI {
namespace {

struct Vertex {
    float x;
    float y;
    float u;
    float v;
};

const char* kVertexShader = R"(
struct VSIn { float2 pos : POSITION; float2 uv : TEXCOORD0; };
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VSOut main(VSIn input) {
    VSOut output;
    output.pos = float4(input.pos, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}
)";

const char* kPixelShader = R"(
Texture2D page : register(t0);
SamplerState samp : register(s0);
float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    return page.Sample(samp, uv);
}
)";

bool CompileShader(const char* source, const char* entry, const char* target, ID3DBlob** blob) {
    if (source == nullptr || blob == nullptr) {
        return false;
    }
    ID3DBlob* errors = nullptr;
    const HRESULT compiled = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry, target,
                                        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, blob, &errors);
    if (errors != nullptr) {
        errors->Release();
    }
    return SUCCEEDED(compiled) && *blob != nullptr;
}

}  // namespace

struct RoboCompositor {
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* layout = nullptr;
    ID3D11SamplerState* point = nullptr;
    ID3D11SamplerState* linear = nullptr;
    ID3D11RasterizerState* raster = nullptr;
    ID3D11Buffer* vertices = nullptr;
    ID3D11Texture2D* page = nullptr;
    ID3D11ShaderResourceView* pageView = nullptr;
    unsigned width = 0;
    unsigned height = 0;
    bool pageReady = false;
    bool ready = false;
};

void ReleasePage(RoboCompositor* compositor) {
    if (compositor == nullptr) {
        return;
    }
    if (compositor->pageView != nullptr) {
        compositor->pageView->Release();
        compositor->pageView = nullptr;
    }
    if (compositor->page != nullptr) {
        compositor->page->Release();
        compositor->page = nullptr;
    }
    compositor->width = 0;
    compositor->height = 0;
    compositor->pageReady = false;
}

RoboCompositor* RoboCompositorCreate(ID3D11Device* device) {
    if (device == nullptr) {
        return nullptr;
    }
    auto* compositor = new RoboCompositor();
    ID3DBlob* vertexBlob = nullptr;
    ID3DBlob* pixelBlob = nullptr;
    if (!CompileShader(kVertexShader, "main", "vs_4_0", &vertexBlob) || !CompileShader(kPixelShader, "main", "ps_4_0", &pixelBlob)) {
        if (vertexBlob != nullptr) {
            vertexBlob->Release();
        }
        if (pixelBlob != nullptr) {
            pixelBlob->Release();
        }
        delete compositor;
        return nullptr;
    }
    const bool shaders = SUCCEEDED(device->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), nullptr,
                                                              &compositor->vertexShader)) &&
                         SUCCEEDED(device->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(), nullptr,
                                                             &compositor->pixelShader));
    D3D11_INPUT_ELEMENT_DESC elements[2]{};
    elements[0].SemanticName = "POSITION";
    elements[0].Format = DXGI_FORMAT_R32G32_FLOAT;
    elements[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    elements[1].SemanticName = "TEXCOORD";
    elements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    elements[1].AlignedByteOffset = 8;
    elements[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    const bool layout = shaders && SUCCEEDED(device->CreateInputLayout(elements, 2, vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(),
                                                                        &compositor->layout));
    vertexBlob->Release();
    pixelBlob->Release();
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    const bool point = layout && SUCCEEDED(device->CreateSamplerState(&sampler, &compositor->point));
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    const bool linear = point && SUCCEEDED(device->CreateSamplerState(&sampler, &compositor->linear));
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.DepthClipEnable = TRUE;
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = sizeof(Vertex) * 6;
    buffer.Usage = D3D11_USAGE_DYNAMIC;
    buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    compositor->ready = linear && SUCCEEDED(device->CreateRasterizerState(&raster, &compositor->raster)) &&
                        SUCCEEDED(device->CreateBuffer(&buffer, nullptr, &compositor->vertices));
    if (!compositor->ready) {
        RoboCompositorDestroy(compositor);
        return nullptr;
    }
    return compositor;
}

void RoboCompositorDestroy(RoboCompositor* compositor) {
    if (compositor == nullptr) {
        return;
    }
    ReleasePage(compositor);
    if (compositor->vertices != nullptr) {
        compositor->vertices->Release();
    }
    if (compositor->raster != nullptr) {
        compositor->raster->Release();
    }
    if (compositor->linear != nullptr) {
        compositor->linear->Release();
    }
    if (compositor->point != nullptr) {
        compositor->point->Release();
    }
    if (compositor->layout != nullptr) {
        compositor->layout->Release();
    }
    if (compositor->pixelShader != nullptr) {
        compositor->pixelShader->Release();
    }
    if (compositor->vertexShader != nullptr) {
        compositor->vertexShader->Release();
    }
    delete compositor;
}

bool EnsurePage(ID3D11Device* device, RoboCompositor* compositor, unsigned width, unsigned height) {
    if (compositor->page != nullptr && compositor->width == width && compositor->height == height) {
        return compositor->pageView != nullptr;
    }
    ReleasePage(compositor);
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &compositor->page))) {
        return false;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = desc.Format;
    view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(compositor->page, &view, &compositor->pageView))) {
        ReleasePage(compositor);
        return false;
    }
    compositor->width = width;
    compositor->height = height;
    return true;
}

void UploadRect(ID3D11DeviceContext* context, ID3D11Texture2D* page, const unsigned char* pixels, unsigned pitch, int x, int y,
                int width, int height, unsigned limitW, unsigned limitH) {
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (x >= static_cast<int>(limitW) || y >= static_cast<int>(limitH) || width <= 0 || height <= 0) {
        return;
    }
    if (x + width > static_cast<int>(limitW)) {
        width = static_cast<int>(limitW) - x;
    }
    if (y + height > static_cast<int>(limitH)) {
        height = static_cast<int>(limitH) - y;
    }
    if (width <= 0 || height <= 0) {
        return;
    }
    D3D11_BOX box{};
    box.left = static_cast<UINT>(x);
    box.top = static_cast<UINT>(y);
    box.right = static_cast<UINT>(x + width);
    box.bottom = static_cast<UINT>(y + height);
    box.back = 1;
    const unsigned char* source = pixels + static_cast<std::size_t>(y) * pitch + static_cast<std::size_t>(x) * 4u;
    context->UpdateSubresource(page, 0, &box, source, pitch, 0);
}

int RoboCompositorPublish(RoboCompositor* compositor, ID3D11DeviceContext* context, IDXGISwapChain* swap, const unsigned char* pixels,
                          unsigned width, unsigned height, unsigned pitch, bool full, const int* damageX, const int* damageY,
                          const int* damageW, const int* damageH, int damageCount, float scale, float anchorX, float anchorY) {
    if (compositor == nullptr || !compositor->ready || context == nullptr || swap == nullptr || pixels == nullptr || width == 0 ||
        height == 0 || pitch < width * 4u) {
        return -1;
    }
    if (!(scale > 0.f)) {
        scale = 1.f;
    }
    ID3D11Device* device = nullptr;
    context->GetDevice(&device);
    if (device == nullptr || !EnsurePage(device, compositor, width, height)) {
        if (device != nullptr) {
            device->Release();
        }
        return -1;
    }
    device->Release();
    if (!compositor->pageReady && !full) {
        return 0;
    }
    if (full) {
        context->UpdateSubresource(compositor->page, 0, nullptr, pixels, pitch, 0);
        compositor->pageReady = true;
    } else if (damageX != nullptr && damageY != nullptr && damageW != nullptr && damageH != nullptr) {
        for (int index = 0; index < damageCount; ++index) {
            UploadRect(context, compositor->page, pixels, pitch, damageX[index], damageY[index], damageW[index], damageH[index], width,
                       height);
        }
    }
    if (!compositor->pageReady || compositor->pageView == nullptr) {
        return 0;
    }
    ID3D11Texture2D* target = nullptr;
    if (FAILED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&target))) || target == nullptr) {
        return -1;
    }
    ID3D11Device* targetDevice = nullptr;
    context->GetDevice(&targetDevice);
    ID3D11RenderTargetView* view = nullptr;
    if (targetDevice == nullptr || FAILED(targetDevice->CreateRenderTargetView(target, nullptr, &view))) {
        if (targetDevice != nullptr) {
            targetDevice->Release();
        }
        target->Release();
        return -1;
    }
    targetDevice->Release();
    const bool magnify = std::fabs(scale - 1.f) > 0.0005f;
    if (magnify) {
        const float clear[4] = {pixels[2] / 255.f, pixels[1] / 255.f, pixels[0] / 255.f, 1.f};
        context->ClearRenderTargetView(view, clear);
    }
    const float left = anchorX + (0.f - anchorX) * scale;
    const float top = anchorY + (0.f - anchorY) * scale;
    const float right = anchorX + (static_cast<float>(width) - anchorX) * scale;
    const float bottom = anchorY + (static_cast<float>(height) - anchorY) * scale;
    const float x0 = left / static_cast<float>(width) * 2.f - 1.f;
    const float y0 = 1.f - top / static_cast<float>(height) * 2.f;
    const float x1 = right / static_cast<float>(width) * 2.f - 1.f;
    const float y1 = 1.f - bottom / static_cast<float>(height) * 2.f;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(compositor->vertices, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        view->Release();
        target->Release();
        return -1;
    }
    const Vertex quad[6] = {
        {x0, y0, 0.f, 0.f}, {x1, y0, 1.f, 0.f}, {x0, y1, 0.f, 1.f},
        {x0, y1, 0.f, 1.f}, {x1, y0, 1.f, 0.f}, {x1, y1, 1.f, 1.f},
    };
    std::memcpy(mapped.pData, quad, sizeof(quad));
    context->Unmap(compositor->vertices, 0);
    context->OMSetRenderTargets(1, &view, nullptr);
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MaxDepth = 1.f;
    context->RSSetViewports(1, &viewport);
    context->RSSetState(compositor->raster);
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetInputLayout(compositor->layout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, &compositor->vertices, &stride, &offset);
    context->VSSetShader(compositor->vertexShader, nullptr, 0);
    context->PSSetShader(compositor->pixelShader, nullptr, 0);
    ID3D11SamplerState* sampler = magnify ? compositor->linear : compositor->point;
    context->PSSetSamplers(0, 1, &sampler);
    context->PSSetShaderResources(0, 1, &compositor->pageView);
    context->Draw(6, 0);
    ID3D11ShaderResourceView* empty = nullptr;
    context->PSSetShaderResources(0, 1, &empty);
    ID3D11RenderTargetView* unbound = nullptr;
    context->OMSetRenderTargets(1, &unbound, nullptr);
    const HRESULT presented = swap->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
    view->Release();
    target->Release();
    if (presented == DXGI_ERROR_WAS_STILL_DRAWING || presented == DXGI_STATUS_OCCLUDED) {
        return 0;
    }
    if (FAILED(presented)) {
        const HRESULT blocking = swap->Present(0, 0);
        if (blocking == DXGI_ERROR_WAS_STILL_DRAWING || blocking == DXGI_STATUS_OCCLUDED || FAILED(blocking)) {
            return 0;
        }
    }
    return 1;
}

}  // namespace Robix::UI
