#include "WindowsCompositor.hpp"

#define NOMINMAX
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace Robix::UI {
namespace {

constexpr unsigned TILE = 256;

struct RoboTile {
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* view = nullptr;
    unsigned width = 0;
    unsigned height = 0;
    bool live = false;
};

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
    unsigned vertexCapacity = 0;
    unsigned width = 0;
    unsigned height = 0;
    unsigned columns = 0;
    unsigned rows = 0;
    std::vector<RoboTile> tiles;
    bool ready = false;
};

void ReleaseTile(RoboTile& tile) {
    if (tile.view != nullptr) {
        tile.view->Release();
        tile.view = nullptr;
    }
    if (tile.texture != nullptr) {
        tile.texture->Release();
        tile.texture = nullptr;
    }
    tile.width = 0;
    tile.height = 0;
    tile.live = false;
}

void ReleaseTiles(RoboCompositor* compositor) {
    if (compositor == nullptr) {
        return;
    }
    for (RoboTile& tile : compositor->tiles) {
        ReleaseTile(tile);
    }
    compositor->tiles.clear();
    compositor->columns = 0;
    compositor->rows = 0;
    compositor->width = 0;
    compositor->height = 0;
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
    compositor->ready = linear && SUCCEEDED(device->CreateRasterizerState(&raster, &compositor->raster));
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
    ReleaseTiles(compositor);
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

bool EnsureGrid(ID3D11Device* device, RoboCompositor* compositor, unsigned width, unsigned height) {
    if (compositor->width == width && compositor->height == height && !compositor->tiles.empty()) {
        return true;
    }
    ReleaseTiles(compositor);
    compositor->width = width;
    compositor->height = height;
    compositor->columns = (width + TILE - 1) / TILE;
    compositor->rows = (height + TILE - 1) / TILE;
    compositor->tiles.resize(static_cast<std::size_t>(compositor->columns) * compositor->rows);
    for (unsigned row = 0; row < compositor->rows; ++row) {
        for (unsigned column = 0; column < compositor->columns; ++column) {
            const unsigned tileWidth = std::min(TILE, width - column * TILE);
            const unsigned tileHeight = std::min(TILE, height - row * TILE);
            RoboTile& tile = compositor->tiles[static_cast<std::size_t>(row) * compositor->columns + column];
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = tileWidth;
            desc.Height = tileHeight;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(device->CreateTexture2D(&desc, nullptr, &tile.texture))) {
                ReleaseTiles(compositor);
                return false;
            }
            D3D11_SHADER_RESOURCE_VIEW_DESC view{};
            view.Format = desc.Format;
            view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            view.Texture2D.MipLevels = 1;
            if (FAILED(device->CreateShaderResourceView(tile.texture, &view, &tile.view))) {
                ReleaseTiles(compositor);
                return false;
            }
            tile.width = tileWidth;
            tile.height = tileHeight;
        }
    }
    return true;
}

bool EnsureVertices(ID3D11Device* device, RoboCompositor* compositor, unsigned count) {
    if (compositor->vertices != nullptr && compositor->vertexCapacity >= count) {
        return true;
    }
    if (compositor->vertices != nullptr) {
        compositor->vertices->Release();
        compositor->vertices = nullptr;
    }
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = count * static_cast<UINT>(sizeof(Vertex));
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    compositor->vertexCapacity = count;
    return SUCCEEDED(device->CreateBuffer(&desc, nullptr, &compositor->vertices));
}

void WriteQuad(Vertex* vertices, float x0, float y0, float x1, float y1) {
    const Vertex quad[6] = {
        {x0, y0, 0.f, 0.f}, {x1, y0, 1.f, 0.f}, {x0, y1, 0.f, 1.f},
        {x0, y1, 0.f, 1.f}, {x1, y0, 1.f, 0.f}, {x1, y1, 1.f, 1.f},
    };
    std::memcpy(vertices, quad, sizeof(quad));
}

int RoboCompositorPublish(RoboCompositor* compositor, ID3D11DeviceContext* context, IDXGISwapChain* swap, const unsigned char* pixels,
                          unsigned width, unsigned height, unsigned pitch, bool full, const int* damageX, const int* damageY,
                          const int* damageW, const int* damageH, int damageCount, float scale, float anchorX, float anchorY) {
    if (compositor == nullptr || !compositor->ready || context == nullptr || swap == nullptr || pixels == nullptr || width == 0 ||
        height == 0) {
        return -1;
    }
    if (!(scale > 0.f)) {
        scale = 1.f;
    }
    ID3D11Device* device = nullptr;
    context->GetDevice(&device);
    if (device == nullptr) {
        return -1;
    }
    const bool grid = EnsureGrid(device, compositor, width, height);
    const unsigned vertexCount = static_cast<unsigned>(compositor->tiles.size()) * 6u;
    const bool buffer = grid && vertexCount > 0 && EnsureVertices(device, compositor, vertexCount);
    device->Release();
    if (!buffer) {
        return -1;
    }
    std::vector<unsigned char> dirty(compositor->tiles.size(), full ? 1 : 0);
    bool anyLive = false;
    for (const RoboTile& tile : compositor->tiles) {
        if (tile.live) {
            anyLive = true;
            break;
        }
    }
    if (!anyLive) {
        std::fill(dirty.begin(), dirty.end(), static_cast<unsigned char>(1));
    } else if (!full) {
        for (int index = 0; index < damageCount; ++index) {
            if (damageW == nullptr || damageH == nullptr || damageX == nullptr || damageY == nullptr || damageW[index] <= 0 ||
                damageH[index] <= 0) {
                continue;
            }
            int x0 = damageX[index];
            int y0 = damageY[index];
            int x1 = x0 + damageW[index];
            int y1 = y0 + damageH[index];
            if (x0 < 0) {
                x0 = 0;
            }
            if (y0 < 0) {
                y0 = 0;
            }
            if (x1 > static_cast<int>(width)) {
                x1 = static_cast<int>(width);
            }
            if (y1 > static_cast<int>(height)) {
                y1 = static_cast<int>(height);
            }
            if (x1 <= x0 || y1 <= y0) {
                continue;
            }
            const unsigned column0 = static_cast<unsigned>(x0) / TILE;
            const unsigned column1 = static_cast<unsigned>(x1 - 1) / TILE;
            const unsigned row0 = static_cast<unsigned>(y0) / TILE;
            const unsigned row1 = static_cast<unsigned>(y1 - 1) / TILE;
            for (unsigned row = row0; row <= row1 && row < compositor->rows; ++row) {
                for (unsigned column = column0; column <= column1 && column < compositor->columns; ++column) {
                    dirty[static_cast<std::size_t>(row) * compositor->columns + column] = 1;
                }
            }
        }
    }
    for (std::size_t index = 0; index < compositor->tiles.size(); ++index) {
        if (dirty[index] == 0) {
            continue;
        }
        RoboTile& tile = compositor->tiles[index];
        if (tile.texture == nullptr) {
            continue;
        }
        const unsigned column = static_cast<unsigned>(index % compositor->columns);
        const unsigned row = static_cast<unsigned>(index / compositor->columns);
        const unsigned char* source = pixels + static_cast<std::size_t>(row * TILE) * pitch + static_cast<std::size_t>(column * TILE) * 4u;
        context->UpdateSubresource(tile.texture, 0, nullptr, source, pitch, 0);
        tile.live = true;
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
    const unsigned char* corner = pixels;
    const float clear[4] = {corner[2] / 255.f, corner[1] / 255.f, corner[0] / 255.f, 1.f};
    context->OMSetRenderTargets(1, &view, nullptr);
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MaxDepth = 1.f;
    context->RSSetViewports(1, &viewport);
    context->RSSetState(compositor->raster);
    context->ClearRenderTargetView(view, clear);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(compositor->vertices, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        view->Release();
        target->Release();
        return -1;
    }
    auto* vertices = static_cast<Vertex*>(mapped.pData);
    const float safeScale = scale;
    for (std::size_t index = 0; index < compositor->tiles.size(); ++index) {
        const RoboTile& tile = compositor->tiles[index];
        const unsigned column = static_cast<unsigned>(index % compositor->columns);
        const unsigned row = static_cast<unsigned>(index / compositor->columns);
        const float left = static_cast<float>(column * TILE);
        const float top = static_cast<float>(row * TILE);
        const float right = left + static_cast<float>(tile.width);
        const float bottom = top + static_cast<float>(tile.height);
        const float x0 = anchorX + (left - anchorX) * safeScale;
        const float y0 = anchorY + (top - anchorY) * safeScale;
        const float x1 = anchorX + (right - anchorX) * safeScale;
        const float y1 = anchorY + (bottom - anchorY) * safeScale;
        const float nx0 = x0 / static_cast<float>(width) * 2.f - 1.f;
        const float ny0 = 1.f - y0 / static_cast<float>(height) * 2.f;
        const float nx1 = x1 / static_cast<float>(width) * 2.f - 1.f;
        const float ny1 = 1.f - y1 / static_cast<float>(height) * 2.f;
        WriteQuad(vertices + index * 6, nx0, ny0, nx1, ny1);
    }
    context->Unmap(compositor->vertices, 0);
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetInputLayout(compositor->layout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, &compositor->vertices, &stride, &offset);
    context->VSSetShader(compositor->vertexShader, nullptr, 0);
    context->PSSetShader(compositor->pixelShader, nullptr, 0);
    ID3D11SamplerState* sampler = std::fabs(safeScale - 1.f) > 0.0005f ? compositor->linear : compositor->point;
    context->PSSetSamplers(0, 1, &sampler);
    for (std::size_t index = 0; index < compositor->tiles.size(); ++index) {
        const RoboTile& tile = compositor->tiles[index];
        if (!tile.live || tile.view == nullptr) {
            continue;
        }
        context->PSSetShaderResources(0, 1, &tile.view);
        context->Draw(6, static_cast<UINT>(index * 6));
    }
    ID3D11ShaderResourceView* empty = nullptr;
    context->PSSetShaderResources(0, 1, &empty);
    context->OMSetRenderTargets(0, nullptr, nullptr);
    view->Release();
    target->Release();
    const HRESULT presented = swap->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
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
