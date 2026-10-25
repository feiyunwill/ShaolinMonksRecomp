// SV_Depth draws bypass the native depth clear during diagnostics.
ID3D12PipelineState* D3D12Backend::EnsureRasterDepthClearPipeline(DXGI_FORMAT format) {
    const uint64_t key = ~0ull - 1 - uint64_t(format);  // Separate from SDK blit and depth-copy keys.
    auto& pso = blit_psos_[key];
    if (pso) return pso.Get();
    const auto source = openshao::RasterDepthClearHlsl();
    uint64_t hash;
    ID3DBlob* vs = Compile(source, "vs", "vs_5_0", hash);
    ID3DBlob* ps = Compile(source, "psmain", "ps_5_0", hash);
    if (!vs || !ps) return nullptr;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = root_.Get();
    d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    d.SampleMask = UINT_MAX;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    d.DepthStencilState.DepthEnable = TRUE;
    d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    d.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.DSVFormat = format;
    d.SampleDesc.Count = 1;
    if (FAILED(dev_->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&pso)))) {
        LOG_FIRST(1, "d3d12: raster depth clear unavailable; using native clear");
        return nullptr;
    }
    return pso.Get();
}

bool D3D12Backend::RasterDepthClear(Target& t, float depth, const std::vector<D3D12_RECT>& rectangles) {
    const DXGI_FORMAT format = t.res->GetDesc().Format == DXGI_FORMAT_R32_TYPELESS ?
        DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D24_UNORM_S8_UINT;
    ID3D12PipelineState* pso = EnsureRasterDepthClearPipeline(format);
    if (!pso) return false;
    Transition(t.res.Get(), t.state, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    D3D12_CPU_DESCRIPTOR_HANDLE view = Cpu(dsv_heap_.Get(), t.view, dsv_inc_);
    cl_->OMSetRenderTargets(0, nullptr, FALSE, &view);
    SetFullViewport(t.w, t.h);
    Alloc cb = Upload(256, 256);
    const float value[4] = {std::clamp(depth, 0.0f, 1.0f), 0, 0, 0};
    std::memcpy(cb.cpu, value, sizeof(value));
    cl_->SetPipelineState(pso);
    cl_->SetGraphicsRootConstantBufferView(0, cb.gpu);
    cl_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const auto& rectangle : rectangles) {
        if (rectangle.left >= rectangle.right || rectangle.top >= rectangle.bottom) continue;
        cl_->RSSetScissorRects(1, &rectangle);
        cl_->DrawInstanced(3, 1, 0, 0);
    }
    // The next draw must restore its own targets after this depth-only binding.
    bound_rt_ = bound_ds_ = nullptr;
    t.drawn = true;
    if (cap_) fprintf(cap_, "     depth_clear raster %08X z %.9g rects %zu\n", t.data, value[0], rectangles.size());
    return true;
}
