// Synthetic offscreen shadow map: compare the generated shaders against literal masks.
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <d3d11.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <wrl/client.h>
#include "gpu/shaders.h"
#include "shadow_diagnostic_modes.h"
using Microsoft::WRL::ComPtr;

static bool Ok(HRESULT hr, const char* operation) {
    if (SUCCEEDED(hr)) return true;
    std::fprintf(stderr, "%s failed: %08X\n", operation, unsigned(hr));
    return false;
}

static ComPtr<ID3DBlob> Compile(const std::string& source, const char* profile) {
    ComPtr<ID3DBlob> code, error;
    if (!Ok(D3DCompile(source.data(), source.size(), nullptr, nullptr, nullptr, "main", profile,
                      D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &error), "shader compilation") && error)
        std::fprintf(stderr, "%s\n", static_cast<const char*>(error->GetBufferPointer()));
    return code;
}

int main(int argc, char** argv) {
    for (uint8_t kind : {uint8_t(0),uint8_t(1),uint8_t(2)}) for (bool linear : {false,true}) for (bool shader_compare : {false,true}) {
        const auto mode = openshao::ShadowSamplingMode(kind, linear, shader_compare);
        D3D12_SAMPLER_DESC before{};
        before.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        before.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        before.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        before.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        before.BorderColor[0] = .75f;
        before.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER;
        before.MaxAnisotropy = 8; before.MinLOD = 0; before.MaxLOD = 3;
        auto after = before;
        openshao::ConfigureShadowSampler(after, mode);
        if (kind && shader_compare) {
            if (after.Filter != D3D12_FILTER_MIN_MAG_MIP_POINT || after.ComparisonFunc != D3D12_COMPARISON_FUNC_ALWAYS || after.MaxAnisotropy != 1) {
                std::puts("FAIL: shader comparison sampler must return unfiltered depths"); return 1;
            }
            after.Filter = before.Filter; after.ComparisonFunc = before.ComparisonFunc; after.MaxAnisotropy = before.MaxAnisotropy;
        }
        if (std::memcmp(&after, &before, sizeof(before))) {
            std::puts("FAIL: shader comparison must preserve addressing, border, LOD and native samplers"); return 1;
        }
    }
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    const bool hardware = argc == 2 && std::strcmp(argv[1], "--hardware") == 0;
    if (!Ok(D3D11CreateDevice(nullptr, hardware ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                             D3D11_SDK_VERSION, &device, nullptr, &context), "WARP")) return 1;
    auto vs_code = Compile(R"(
struct VOut {
    float4 pos : SV_Position;
    float4 d0 : COLOR0; float4 d1 : COLOR1; float4 b0 : COLOR2; float4 b1 : COLOR3;
    float4 tc0 : TEXCOORD0; float4 tc1 : TEXCOORD1; float4 tc2 : TEXCOORD2; float4 tc3 : TEXCOORD3;
    float fogamt : TEXCOORD4;
};
cbuffer Query : register(b2) { float4 query; };
VOut main(uint id : SV_VertexID) {
    float2 p = id == 0 ? float2(-1,-1) : id == 1 ? float2(-1,3) : float2(3,-1);
    VOut o = (VOut)0;
    o.pos = float4(p,0,1); o.d0 = 1; o.fogamt = 1;
    o.tc0 = float4((p.x+1)*0.5 + query.y, query.z, query.x, 1);
    return o;
})", "vs_5_0");
    if (!vs_code) return 1;
    ComPtr<ID3D11VertexShader> vs;
    if (!Ok(device->CreateVertexShader(vs_code->GetBufferPointer(), vs_code->GetBufferSize(), nullptr, &vs), "VS")) return 1;
    const float depths[] = {.25f,.75f,.25f,.75f, .75f,.25f,.75f,.25f};
    D3D11_TEXTURE2D_DESC td{};
    td.Width = 4; td.Height = 2; td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
    td.Format = DXGI_FORMAT_R32_FLOAT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{depths,16,0};
    ComPtr<ID3D11Texture2D> map;
    ComPtr<ID3D11ShaderResourceView> srv;
    if (!Ok(device->CreateTexture2D(&td, &initial, &map), "shadow map") ||
        !Ok(device->CreateShaderResourceView(map.Get(), nullptr, &srv), "shadow SRV")) return 1;
    td.Width = 8; td.Height = 1; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target, readback;
    ComPtr<ID3D11RenderTargetView> rtv;
    if (!Ok(device->CreateTexture2D(&td, nullptr, &target), "output") ||
        !Ok(device->CreateRenderTargetView(target.Get(), nullptr, &rtv), "RTV")) return 1;
    td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (!Ok(device->CreateTexture2D(&td, nullptr, &readback), "readback")) return 1;
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(gpu::DrawConstants); bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> constants, query;
    if (!Ok(device->CreateBuffer(&bd, nullptr, &constants), "constants")) return 1;
    bd.ByteWidth = 16;
    if (!Ok(device->CreateBuffer(&bd, nullptr, &query), "query")) return 1;
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = true;
    ComPtr<ID3D11RasterizerState> raster;
    if (!Ok(device->CreateRasterizerState(&rd, &raster), "rasterizer")) return 1;
    context->RSSetState(raster.Get());
    D3D11_VIEWPORT viewport{0,0,8,1,0,1}; context->RSSetViewports(1, &viewport);
    context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vs.Get(), nullptr, 0);
    context->VSSetConstantBuffers(2, 1, query.GetAddressOf());
    context->PSSetConstantBuffers(1, 1, constants.GetAddressOf());
    context->PSSetShaderResources(0, 1, srv.GetAddressOf());

    const std::array<float,8> point_less{1,1,0,0,1,1,0,0};
    const std::array<float,8> linear_less{1,.75f,.25f,.25f,.75f,.75f,.25f,0};
    const std::array<float,8> wrap_less{.75f,.75f,.25f,.25f,.75f,.75f,.25f,.25f};
    const std::array<float,8> vertical_less{.75f,.625f,.375f,.375f,.625f,.625f,.375f,.25f};
    const unsigned swap[] = {0,4,2,6,1,5,3,7};
    unsigned cases = 0;
    for (unsigned kind : {1u,2u}) for (bool linear : {false,true}) for (bool manual : {true,false}) for (bool programmable : {false,true}) {
        gpu::PixelKey key;
        // The high bits select the shader comparison path; low bits preserve the Xbox depth format.
        key.tex[0] = {.kind=gpu::kTex2D, .shadow=openshao::ShadowSamplingMode(uint8_t(kind),linear,manual)};
        key.ff[0][0] = key.ff[0][4] = 2; key.ff[0][1] = key.ff[0][5] = 2;
        if (programmable) {
            key.programmable = 1; key.tex_modes = 2;
            key.final_abcd = 8; key.final_efg = 0x1800;
        }
        auto ps_code = Compile(gpu::PixelHlsl(key), "ps_5_0");
        if (!ps_code) return 1;
        ComPtr<ID3D11ShaderReflection> reflection;
        if (!Ok(D3DReflect(ps_code->GetBufferPointer(), ps_code->GetBufferSize(), IID_PPV_ARGS(&reflection)), "reflection")) return 1;
        D3D11_SHADER_INPUT_BIND_DESC binding{};
        if (!Ok(reflection->GetResourceBindingDescByName("smp0", &binding), "sampler reflection")) return 1;
        if (bool(binding.uFlags & D3D_SIF_COMPARISON_SAMPLER) == manual) {
            std::puts("FAIL: shader-compared shadows must use an ordinary point sampler, native shadows a comparison sampler");
            return 1;
        }
        ComPtr<ID3D11PixelShader> ps;
        if (!Ok(device->CreatePixelShader(ps_code->GetBufferPointer(), ps_code->GetBufferSize(), nullptr, &ps), "PS")) return 1;
        context->PSSetShader(ps.Get(), nullptr, 0);
        for (unsigned func=0; func<8; ++func) for (unsigned scenario=0; scenario<6; ++scenario) {
            D3D11_SAMPLER_DESC sd{};
            sd.Filter = manual ? D3D11_FILTER_MIN_MAG_MIP_POINT : linear ? D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT : D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
            sd.AddressU = sd.AddressV = sd.AddressW = scenario == 2 ? D3D11_TEXTURE_ADDRESS_WRAP : scenario == 3 ? D3D11_TEXTURE_ADDRESS_BORDER : D3D11_TEXTURE_ADDRESS_CLAMP;
            sd.BorderColor[0] = 1; sd.MaxAnisotropy = 1;
            sd.ComparisonFunc = D3D11_COMPARISON_FUNC(swap[func]+1);
            ComPtr<ID3D11SamplerState> sampler;
            if (!Ok(device->CreateSamplerState(&sd, &sampler), "sampler")) return 1;
            context->PSSetSamplers(0, 1, sampler.GetAddressOf());
            gpu::DrawConstants dc{};
            dc.misc[1] = 7; dc.misc[2] = float(func);
            dc.texsize[0][0] = 4; dc.texsize[0][1] = 2;
            context->UpdateSubresource(constants.Get(), 0, nullptr, &dc, 0, 0);
            const float q[] = {(scenario == 5 ? .25f : .5f) * (kind == 2 ? 65535.f : 16777215.f),
                               scenario >= 2 && scenario <= 4 ? 2.f : 0.f,
                               scenario == 1 ? .375f : .25f, 0};
            context->UpdateSubresource(query.Get(), 0, nullptr, q, 0, 0);
            context->Draw(3,0);
            context->CopyResource(readback.Get(), target.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (!Ok(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped), "map output")) return 1;
            for (unsigned x=0; x<8; ++x) {
                float less = (linear ? linear_less : point_less)[x];
                if (linear && scenario == 1) less = vertical_less[x];
                if (linear && scenario == 2) less = wrap_less[x];
                if (scenario == 3 || scenario == 4) less = 0;
                float want;
                if (scenario == 5) {
                    // Only the .25 texels equal the reference; none are less than it.
                    want = func == 0 || func == 1 ? 0 : func == 2 || func == 3 ? less : func == 4 || func == 5 ? 1-less : 1;
                } else {
                    want = func == 0 || func == 2 ? 0 : func == 5 || func == 7 ? 1 : func == 1 || func == 3 ? less : 1-less;
                }
                float got = static_cast<const float*>(mapped.pData)[x];
                if (!std::isfinite(got) || std::fabs(got-want) > .005f) {
                    std::fprintf(stderr,"FAIL: kind=%u linear=%d manual=%d compare=%u scenario=%u pixel=%u got=%g want=%g\n",kind,linear,manual,func,scenario,x,got,want);
                    context->Unmap(readback.Get(),0); return 1;
                }
            }
            context->Unmap(readback.Get(),0); ++cases;
        }
    }
    std::printf("PASS: %u shadow masks on %s, fixed/programmable shaders, D16/D24 references, all comparisons, filtering and addressing\n",cases,hardware ? "hardware" : "WARP");
}
