// Depth-only shadow clears must preserve both untouched pixels and stencil.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "shadow_diagnostic_modes.h"
using Microsoft::WRL::ComPtr;

static bool Ok(HRESULT hr, const char* name) {
    if (SUCCEEDED(hr)) return true;
    std::fprintf(stderr, "FAIL: %s %08X\n", name, unsigned(hr));
    return false;
}

static ComPtr<ID3DBlob> Compile(const std::string& source, const char* entry, const char* profile) {
    ComPtr<ID3DBlob> result, error;
    if (!Ok(D3DCompile(source.data(), source.size(), nullptr, nullptr, nullptr, entry, profile,
                      D3DCOMPILE_ENABLE_STRICTNESS, 0, &result, &error), "compile depth clear") && error)
        std::fprintf(stderr, "%s\n", static_cast<const char*>(error->GetBufferPointer()));
    return result;
}

int main(int argc, char** argv) {
    if (!openshao::UseRasterDepthClear(true, true, true, false)) {
        std::puts("FAIL: enabled depth-only shadow clear must use rasterization"); return 1;
    }
    for (auto policy : {std::array{false, true, true, false}, std::array{true, false, true, false},
                        std::array{true, true, false, false}, std::array{true, true, true, true}})
        if (openshao::UseRasterDepthClear(policy[0], policy[1], policy[2], policy[3])) {
            std::puts("FAIL: preserve disabled, screen-sized, non-depth and stencil clears"); return 1;
        }
    const bool hardware = argc == 2 && std::strcmp(argv[1], "--hardware") == 0;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    if (!Ok(D3D11CreateDevice(nullptr, hardware ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP,
                             nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context), "device")) return 1;
    const auto source = openshao::RasterDepthClearHlsl();
    const auto vs_code = Compile(source, "vs", "vs_5_0"), ps_code = Compile(source, "psmain", "ps_5_0");
    if (!vs_code || !ps_code) return 1;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    if (!Ok(device->CreateVertexShader(vs_code->GetBufferPointer(), vs_code->GetBufferSize(), nullptr, &vs), "VS") ||
        !Ok(device->CreatePixelShader(ps_code->GetBufferPointer(), ps_code->GetBufferSize(), nullptr, &ps), "PS")) return 1;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = 8; td.Height = 6; td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
    td.Format = DXGI_FORMAT_R24G8_TYPELESS; td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> target, readback;
    if (!Ok(device->CreateTexture2D(&td, nullptr, &target), "depth target")) return 1;
    D3D11_DEPTH_STENCIL_VIEW_DESC view{};
    view.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; view.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11DepthStencilView> dsv;
    if (!Ok(device->CreateDepthStencilView(target.Get(), &view, &dsv), "depth view")) return 1;
    td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (!Ok(device->CreateTexture2D(&td, nullptr, &readback), "readback")) return 1;
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = TRUE; depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
    ComPtr<ID3D11DepthStencilState> depth_state;
    if (!Ok(device->CreateDepthStencilState(&depth, &depth_state), "depth state")) return 1;
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID; raster.CullMode = D3D11_CULL_NONE;
    raster.DepthClipEnable = raster.ScissorEnable = TRUE;
    ComPtr<ID3D11RasterizerState> raster_state;
    if (!Ok(device->CreateRasterizerState(&raster, &raster_state), "rasterizer")) return 1;
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = 16; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> constants;
    if (!Ok(device->CreateBuffer(&bd, nullptr, &constants), "constants")) return 1;
    context->VSSetShader(vs.Get(), nullptr, 0); context->PSSetShader(ps.Get(), nullptr, 0);
    auto cb = constants.Get(); context->PSSetConstantBuffers(0, 1, &cb);
    context->OMSetDepthStencilState(depth_state.Get(), 0); context->RSSetState(raster_state.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D11_VIEWPORT viewport{0, 0, 8, 6, 0, 1}; context->RSSetViewports(1, &viewport);
    const std::array<std::vector<D3D11_RECT>, 4> cases{{{{0, 0, 8, 6}}, {{2, 1, 6, 5}},
                                                     {{3, 2, 3, 4}}, {{0, 0, 3, 6}, {2, 2, 8, 4}}}};
    unsigned checked = 0;
    for (float value : {-1.0f, 0.0f, .12345f, .5f, 1.0f, 2.0f}) for (const auto& rectangles : cases) {
        context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, .25f, 0x5A);
        float c[4]{value}; context->UpdateSubresource(constants.Get(), 0, nullptr, c, 0, 0);
        context->OMSetRenderTargets(0, nullptr, dsv.Get());
        for (const auto& rectangle : rectangles) {
            context->RSSetScissorRects(1, &rectangle); context->Draw(3, 0);
        }
        context->OMSetRenderTargets(0, nullptr, nullptr); context->CopyResource(readback.Get(), target.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (!Ok(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped), "map depth")) return 1;
        bool good = true;
        for (int y = 0; y < 6; ++y) for (int x = 0; x < 8; ++x) {
            bool touched = false;
            for (const auto& r : rectangles) touched |= x >= r.left && x < r.right && y >= r.top && y < r.bottom;
            const float expected = touched ? std::clamp(value, 0.0f, 1.0f) : .25f;
            uint32_t raw;
            std::memcpy(&raw, static_cast<const char*>(mapped.pData) + y * mapped.RowPitch + x * 4, 4);
            if (std::abs(float(raw & 0xFFFFFF) / 16777215.0f - expected) > 1.0f / 16777215.0f || (raw >> 24) != 0x5A) {
                std::fprintf(stderr, "FAIL: clear %g at %d,%d expected depth %g stencil 5A, got %08X\n", value, x, y, expected, raw);
                good = false;
            }
        }
        context->Unmap(readback.Get(), 0);
        if (!good) return 1;
        ++checked;
    }
    std::printf("PASS: %u depth clear masks on %s; clamping, scissor rectangles and stencil preserved\n", checked, hardware ? "hardware" : "WARP");
}
