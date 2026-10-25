#include "d3d12_cache.h"
#include <dxgi1_6.h>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
using namespace openshao::graphics;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
ComPtr<ID3DBlob> Compile(const char* source, const char* profile) {
    ComPtr<ID3DBlob> code, error;
    Require(SUCCEEDED(D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, "main", profile,
                                 D3DCOMPILE_OPTIMIZATION_LEVEL1, 0, &code, &error)), "compile synthetic shader");
    return code;
}
int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("openshao-d3d12-cache-test-" + std::to_string(GetCurrentProcessId()));
    try {
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<ID3D12Device> device;
        Require(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) &&
                SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))) &&
                SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))), "create offscreen WARP device");
        auto vs = Compile("float4 main(uint i:SV_VertexID):SV_Position{return float4(0,0,0,1);}", "vs_5_0");
        auto ps = Compile("float4 main():SV_Target{return float4(1,0,0,1);}", "ps_5_0");
        D3D12_ROOT_SIGNATURE_DESC signature{};
        ComPtr<ID3DBlob> serialized;
        ComPtr<ID3D12RootSignature> rs;
        Require(SUCCEEDED(D3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, nullptr)) &&
                SUCCEEDED(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&rs))), "create synthetic root signature");
        const std::span<const uint8_t> root_bytes{static_cast<const uint8_t*>(serialized->GetBufferPointer()), serialized->GetBufferSize()};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
        d.pRootSignature = rs.Get();
        d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        d.SampleMask = UINT_MAX;
        d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        d.NumRenderTargets = 1;
        d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count = 1;
        const auto key = PipelineKey(d, 17);
        auto changed = d;
        changed.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
        Require(PipelineKey(changed, 17) != key && PipelineKey(d, 18) != key, "pipeline identity must include render state and root signature");
        std::vector<uint8_t> copy(static_cast<uint8_t*>(vs->GetBufferPointer()), static_cast<uint8_t*>(vs->GetBufferPointer()) + vs->GetBufferSize());
        changed = d;
        changed.VS.pShaderBytecode = copy.data();
        Require(PipelineKey(changed, 17) == key, "pipeline identity must depend on bytecode contents, not addresses");
        {
            D3D12Cache cache;
            cache.Open(root, device.Get(), 77);
            cache.SetRootSignature(root_bytes);
            ComPtr<ID3DBlob> loaded;
            Require(!cache.LoadShader(1, loaded), "the first shader lookup must miss");
            cache.StoreShader(1, vs.Get(), 1);
            ComPtr<ID3D12PipelineState> pso;
            Require(SUCCEEDED(cache.CreatePipeline(&d, IID_PPV_ARGS(&pso))), "create and cache a real pipeline");
            Require(cache.Statistics().pipeline_creates == 1, "report actual pipeline creation");
        }
        {
            D3D12Cache cache;
            cache.Open(root, device.Get(), 77);
            cache.SetRootSignature(root_bytes);
            ComPtr<ID3DBlob> loaded;
            Require(cache.LoadShader(1, loaded), "compiled Direct3D bytecode must survive a session restart");
            Require(loaded->GetBufferSize() == vs->GetBufferSize() &&
                    std::memcmp(loaded->GetBufferPointer(), vs->GetBufferPointer(), vs->GetBufferSize()) == 0, "restored bytecode must match the compiled shader");
            Require(cache.Statistics().shader_hits == 1 && cache.Statistics().shader_compiles == 0, "report shader reuse without recompilation");
            ComPtr<ID3D12PipelineState> pso;
            Require(SUCCEEDED(cache.CreatePipeline(&d, IID_PPV_ARGS(&pso))), "recreate a pipeline with cache or supported fallback");
            ComPtr<ID3DBlob> cached;
            if (SUCCEEDED(pso->GetCachedBlob(&cached)) && cached && cached->GetBufferSize())
                Require(cache.Statistics().pipeline_hits == 1, "supported cached pipelines must be reused");
        }
        // A valid record checksum does not mean that the driver's opaque payload is usable.
        uint64_t pipeline_version = 0;
        {
            std::ifstream file(root / "pipelines.bin", std::ios::binary);
            file.seekg(8);
            file.read(reinterpret_cast<char*>(&pipeline_version), sizeof(pipeline_version));
        }
        {
            CacheFile file(root / "pipelines.bin", pipeline_version);
            const std::vector<uint8_t> invalid{1, 2, 3, 4};
            Require(file.Put(PipelineKey(d, CacheHash(root_bytes)), invalid), "store a synthetic driver-incompatible pipeline");
        }
        {
            D3D12Cache cache;
            cache.Open(root, device.Get(), 77);
            cache.SetRootSignature(root_bytes);
            ComPtr<ID3D12PipelineState> pso;
            Require(SUCCEEDED(cache.CreatePipeline(&d, IID_PPV_ARGS(&pso))), "a driver-rejected cache must fall back to normal creation");
            Require(cache.Statistics().pipeline_rejects == 1 && cache.Statistics().pipeline_creates == 1,
                    "report rejected payloads and rebuild instead of failing rendering");
        }
        std::filesystem::remove_all(root);
        std::puts("PASS: real Direct3D shader persistence, pipeline identity and offscreen WARP pipeline reuse");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        std::filesystem::remove_all(root);
        return 1;
    }
}
