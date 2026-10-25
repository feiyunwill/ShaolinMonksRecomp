// Run only during an explicit capture suite.
void D3D12Backend::CaptureGpuInfo() {
    FILE* info = _wfopen((cap_dir_ / "gpu-runtime.txt").c_str(), L"w");
    if (info) {
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(factory_.As(&factory)) && SUCCEEDED(factory->EnumAdapterByLuid(dev_->GetAdapterLuid(), IID_PPV_ARGS(&adapter))))
            adapter->GetDesc1(&desc);
        fprintf(info, "adapter=%ls\nvendor=%04X\ndevice=%04X\nrevision=%X\nguest_size=%ux%u\nrender_scale=%.9g\nmode=%s\ndevice_removed_reason=%08X\n",
            desc.Description, desc.VendorId, desc.DeviceId, desc.Revision, guest_w_, guest_h_, scale_, shadow_capture_.Name(), unsigned(dev_->GetDeviceRemovedReason()));
        for (auto format : {DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_FLOAT}) {
            D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
            const HRESULT hr = dev_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support));
            fprintf(info, "format=%u hr=%08X support1=%08X support2=%08X\n", unsigned(format), unsigned(hr), support.Support1, support.Support2);
        }
        fclose(info);
    }
    FILE* validation = _wfopen((cap_dir_ / "gpu-validation.txt").c_str(), L"w");
    if (!validation) return;
    ComPtr<ID3D12InfoQueue> queue;
    if (FAILED(dev_.As(&queue))) fprintf(validation, "D3D12 debug message queue unavailable. Other diagnostics are still valid.\n");
    else {
        const UINT64 count = queue->GetNumStoredMessages();
        fprintf(validation, "stored_messages=%llu\n", (unsigned long long)count);
        for (UINT64 i = 0; i < std::min<UINT64>(count, 4096); ++i) {
            SIZE_T size = 0;
            queue->GetMessage(i, nullptr, &size);
            std::vector<uint8_t> bytes(size);
            auto* message = reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
            if (SUCCEEDED(queue->GetMessage(i, message, &size)))
                fprintf(validation, "severity=%u id=%u %s\n", unsigned(message->Severity), unsigned(message->ID), message->pDescription);
        }
        if (count > 4096) fprintf(validation, "Message output limited to the first 4096 entries.\n");
    }
    fclose(validation);
}

bool D3D12Backend::QueueDepthRead(Target& target, const std::string& name) {
    PendingDepthRead read;
    read.name = name;
    read.width = target.w;
    read.height = target.h;
    read.floating = target.res->GetDesc().Format == DXGI_FORMAT_R32_TYPELESS;
    auto desc = target.res->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 bytes;
    dev_->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
    read.pitch = footprint.Footprint.RowPitch;
    D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_READBACK};
    auto buffer = BufferDesc(bytes);
    if (FAILED(dev_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&read.resource)))) return false;
    const auto previous = target.state;
    Transition(target.res.Get(), target.state, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION source{target.res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
    D3D12_TEXTURE_COPY_LOCATION destination{read.resource.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
    destination.PlacedFootprint = footprint;
    cl_->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    Transition(target.res.Get(), target.state, previous);
    depth_reads_.push_back(std::move(read));
    return true;
}

std::vector<float> D3D12Backend::ReadDepthValues(PendingDepthRead& read, bool save) {
    std::vector<float> result;
    void* pointer = nullptr;
    if (FAILED(read.resource->Map(0, nullptr, &pointer))) return result;
    result.reserve(size_t(read.width) * read.height);
    FILE* binary = save ? _wfopen((cap_dir_ / (read.name + ".depth.bin")).c_str(), L"wb") : nullptr;
    uint64_t zero = 0, one = 0, invalid = 0;
    float minimum = 1, maximum = 0;
    for (uint32_t y = 0; y < read.height; ++y) {
        const auto* row = static_cast<const uint8_t*>(pointer) + size_t(y) * read.pitch;
        if (binary) fwrite(row, 4, read.width, binary);
        for (uint32_t x = 0; x < read.width; ++x) {
            uint32_t bits;
            std::memcpy(&bits, row + x * 4, 4);
            float value;
            if (read.floating) std::memcpy(&value, &bits, 4);
            else value = float(bits & 0xFFFFFF) / 16777215.0f;
            result.push_back(value);
            if (!std::isfinite(value)) { ++invalid; continue; }
            zero += value == 0;
            one += value == 1;
            minimum = std::min(minimum, value); maximum = std::max(maximum, value);
        }
    }
    if (binary) fclose(binary);
    read.resource->Unmap(0, nullptr);
    if (save) {
        FILE* info = _wfopen((cap_dir_ / (read.name + ".depth.txt")).c_str(), L"w");
        if (info) {
            fprintf(info, "width=%u\nheight=%u\nencoding=%s\nrow_bytes=%u\nmin=%.9g\nmax=%.9g\nzero=%llu\none=%llu\ninvalid=%llu\n",
                read.width, read.height, read.floating ? "little-endian float32" : "little-endian uint32, low 24 bits UNORM; upper bits undefined",
                read.width * 4, minimum, maximum, (unsigned long long)zero, (unsigned long long)one, (unsigned long long)invalid);
            fclose(info);
        }
    }
    return result;
}

void D3D12Backend::DrainDepthReads() {
    if (depth_reads_.empty()) return;
    Flush();
    for (auto& read : depth_reads_) {
        if (ReadDepthValues(read, true).empty() && cap_) fprintf(cap_, "depth_read_failed %s\n", read.name.c_str());
    }
    depth_reads_.clear();
}

void D3D12Backend::ProbeShadowDepth() {
    FILE* report = _wfopen((cap_dir_ / "gpu-depth-tests.csv").c_str(), L"w");
    if (!report) return;
    if (cap_) fprintf(cap_, "gpu_depth_probe_begin\n");
    fprintf(report, "format,size,optimized_clear,clear_method,raw_errors,sampled_errors,status\n");
    if (free_dsv_.empty() || free_rtv_.empty()) {
        fprintf(report, "unavailable,0,0,none,0,0,NO_DESCRIPTORS\n"); fclose(report); return;
    }
    unsigned total = 0, failed = 0;
    for (bool floating : {false, true}) for (uint32_t size : {256u, 576u})
        for (bool optimized : {false, true}) for (int method = 0; method < 3; ++method) {
        Target target;
        target.depth = true;
        target.w = target.gw = size; target.h = target.gh = size;
        target.view = free_dsv_.back();
        target.state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        const DXGI_FORMAT depth_format = floating ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D24_UNORM_S8_UINT;
        const DXGI_FORMAT sample_format = floating ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        auto desc = Texture2DDesc(floating ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_R24G8_TYPELESS, size, size);
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        D3D12_CLEAR_VALUE clear{}; clear.Format = depth_format; clear.DepthStencil.Depth = 1;
        D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
        const auto view = Cpu(dsv_heap_.Get(), target.view, dsv_inc_);
        const char* name = floating ? "D32" : "D24";
        const char* operation = method == 0 ? "native-full" : method == 1 ? "native-rect" : "raster";
        ++total;
        if (FAILED(dev_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, target.state,
                optimized ? &clear : nullptr, IID_PPV_ARGS(&target.res)))) {
            fprintf(report, "%s,%u,%u,%s,0,0,CREATE_FAILED\n", name, size, optimized, operation); ++failed; continue;
        }
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
        dsv.Format = depth_format; dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dev_->CreateDepthStencilView(target.res.Get(), &dsv, view);
        const D3D12_RECT full{0, 0, LONG(size), LONG(size)};
        const D3D12_RECT center{LONG(size / 4), LONG(size / 4), LONG(size * 3 / 4), LONG(size * 3 / 4)};
        const auto flags = floating ? D3D12_CLEAR_FLAG_DEPTH : D3D12_CLEAR_FLAGS(D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);
        cl_->ClearDepthStencilView(view, flags, 0, 0, 0, nullptr);
        bool good = true;
        if (method == 2) good = RasterDepthClear(target, 1, {full});
        else cl_->ClearDepthStencilView(view, D3D12_CLEAR_FLAG_DEPTH, 1, 0, method == 1 ? 1 : 0, method == 1 ? &full : nullptr);
        good = good && RasterDepthClear(target, 0, {center});
        // Read the sampled representation before any raw copy can change compression state.
        auto sampled = Shrink(target.res.Get(), target.state, sample_format, size, size, {});
        std::vector<uint8_t> pixels;
        good = good && sampled && ReadBack(sampled.Get(), size, size, pixels);
        uint64_t sample_errors = 0, raw_errors = 0;
        if (good && QueueDepthRead(target, "synthetic")) {
            Flush();
            auto values = ReadDepthValues(depth_reads_.back(), false);
            depth_reads_.pop_back();
            good = values.size() == size_t(size) * size;
            if (good) for (uint32_t y = 0; y < size; ++y) for (uint32_t x = 0; x < size; ++x) {
                const bool center_pixel = x >= size / 4 && x < size * 3 / 4 && y >= size / 4 && y < size * 3 / 4;
                const size_t index = size_t(y) * size + x;
                const float expected = center_pixel ? 0.0f : 1.0f;
                raw_errors += !std::isfinite(values[index]) || std::abs(values[index] - expected) > .000001f;
                sample_errors += pixels[index * 4 + 2] != (center_pixel ? 0 : 255);
            }
        } else good = false;
        if (!good) Flush();  // A failed readback must not release an in-flight temporary target.
        const bool pass = good && !raw_errors && !sample_errors;
        failed += !pass;
        fprintf(report, "%s,%u,%u,%s,%llu,%llu,%s\n", name, size, optimized, operation,
            (unsigned long long)raw_errors, (unsigned long long)sample_errors, pass ? "PASS" : good ? "MISMATCH" : "READ_FAILED");
    }
    fprintf(report, "summary,%u,0,failed,%u,0,%s\n", total, failed, failed ? "CHECK_RESULTS" : "PASS");
    fclose(report);
    if (cap_) fprintf(cap_, "gpu_depth_probe_end total=%u failed=%u\n", total, failed);
    Begin();
}
