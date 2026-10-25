"""Optional D3D12 capture instrumentation, excluded from normal runtime builds."""


def adapt_backend(backend, replace_once):
    backend = '#include "shadow_diagnostic_modes.h"\n#include "shadow_capture.h"\n' + backend
    backend = replace_once(backend, '    D3D12_COMMAND_QUEUE_DESC qd{};',
        '    LOGR("d3d12: shadow comparison = %s", openshao::ShaderShadowCompareEnabled() ? "shader" : "native");\n'
        '    LOGR("d3d12: shadow depth clear = %s", openshao::RasterDepthClearEnabled() ? "raster" : "native");\n'
        '    D3D12_COMMAND_QUEUE_DESC qd{};')
    backend = replace_once(backend, '    ID3D12PipelineState* EnsureDepthCopyPipeline(DXGI_FORMAT format);',
        '    ID3D12PipelineState* EnsureDepthCopyPipeline(DXGI_FORMAT format);\n'
        '    ID3D12PipelineState* EnsureRasterDepthClearPipeline(DXGI_FORMAT format);\n'
        '    bool RasterDepthClear(Target& t, float depth, const std::vector<D3D12_RECT>& rectangles);')
    backend = replace_once(backend, '    void StartCapture();',
        '    void StartCapture();\n'
        '    struct PendingDepthRead {\n'
        '        ComPtr<ID3D12Resource> resource;\n'
        '        std::string name;\n'
        '        uint32_t width = 0, height = 0, pitch = 0;\n'
        '        bool floating = false;\n'
        '    };\n'
        '    std::vector<PendingDepthRead> depth_reads_;\n'
        '    std::unordered_set<ID3D12Resource*> depth_seen_;\n'
        '    openshao::ShadowCaptureSequence shadow_capture_;\n'
        '    std::string shadow_group_;\n'
        '    bool QueueDepthRead(Target& target, const std::string& name);\n'
        '    std::vector<float> ReadDepthValues(PendingDepthRead& read, bool save);\n'
        '    void DrainDepthReads();\n'
        '    void ProbeShadowDepth();\n'
        '    void CaptureGpuInfo();')
    backend = replace_once(backend, 'constexpr uint64_t kDepthCopyKey = ~0ull;',
        '#include "shadow_depth_clear.inl"\n#include "shadow_diagnostics.inl"\n\nconstexpr uint64_t kDepthCopyKey = ~0ull;')
    backend = replace_once(backend,
        '    if (t.depth) {\n        Transition(t.res.Get(), t.state, D3D12_RESOURCE_STATE_DEPTH_WRITE);\n        D3D12_CLEAR_FLAGS cf',
        '    if (t.depth) {\n'
        '        if (openshao::UseRasterDepthClear(shadow_capture_.Raster(openshao::RasterDepthClearEnabled()),\n'
        '                t.gw != guest_w_ || t.gh != guest_h_, c.flags & kClearZ, c.flags & kClearStencil) &&\n'
        '            RasterDepthClear(t, c.z, rs)) return;\n'
        '        Transition(t.res.Get(), t.state, D3D12_RESOURCE_STATE_DEPTH_WRITE);\n        D3D12_CLEAR_FLAGS cf')
    backend = replace_once(backend, '    for (int i = 0; i < 4; i++) stages.res[i] = BindTexture(s, i, stages);',
        '    for (int i = 0; i < 4; i++) {\n'
        '        stages.res[i] = BindTexture(s, i, stages);\n'
        '        const auto& t = s.tss[size_t(i)];\n'
        '        stages.info[i].shadow = openshao::ShadowSamplingMode(stages.info[i].shadow,\n'
        '            t[kTssMinFilter] >= 2 || t[kTssMagFilter] >= 2, shadow_capture_.Manual(openshao::ShaderShadowCompareEnabled()));\n'
        '    }')
    backend = replace_once(backend, '    if (t.depth) info.shadow = ShadowDepthKind(d.format);',
        '    if (t.depth) {\n'
        '        info.shadow = ShadowDepthKind(d.format);\n'
        '        if (cap_ && shadow_capture_.Readback() && depth_seen_.insert(t.res.Get()).second) {\n'
        '            char name[80]; snprintf(name, sizeof(name), "before_sample_%08X_%ux%u", t.data, t.w, t.h);\n'
        '            if (!QueueDepthRead(t, name)) fprintf(cap_, "depth_read_failed %s\\n", name);\n'
        '        }\n'
        '    }')
    backend = replace_once(backend, '        FinishCapture(front);',
        '        const bool suite = shadow_capture_.Active();\n'
        '        FinishCapture(front);\n'
        '        if (shadow_capture_.Next()) StartCapture();\n'
        '        else if (suite) LOGI("d3d12: shadow diagnostic suite complete");')
    backend = replace_once(backend, '    } else if (g_capture.exchange(false) && CaptureOn()) {\n        StartCapture();',
        '    } else if (g_capture.exchange(false) && CaptureOn()) {\n'
        '        const char* suite = std::getenv("OPENSHAO_CAPTURE_SUITE");\n'
        '        shadow_capture_.Start(suite && std::strcmp(suite, "1") == 0);\n'
        '        StartCapture();')
    backend = replace_once(backend, '    std::error_code ec;\n    cap_dir_',
        '    if (shadow_capture_.Active()) {\n'
        '        if (shadow_capture_.Step() == 1) shadow_group_ = std::string(stamp) + "_" + std::to_string(tm.wMilliseconds);\n'
        '        snprintf(stamp, sizeof(stamp), "%s/%s", shadow_group_.c_str(), shadow_capture_.Name());\n'
        '    }\n'
        '    depth_seen_.clear();\n'
        '    std::error_code ec;\n    cap_dir_')
    backend = replace_once(backend, '    cap_i_ = cap_rt_ = 0;',
        '    if (cap_ && shadow_capture_.Active()) fprintf(cap_, "diagnostic_mode %s raster=%u manual=%u\\n",\n'
        '        shadow_capture_.Name(), shadow_capture_.Raster(false), shadow_capture_.Manual(false));\n'
        '    cap_i_ = cap_rt_ = 0;')
    backend = replace_once(backend, '    return sd;\n}',
        '    openshao::ConfigureShadowSampler(sd, stages.info[i].shadow);\n    return sd;\n}')
    backend = replace_once(backend, '    fclose(cap_);',
        '    if (shadow_capture_.Step() == 1) ProbeShadowDepth();\n'
        '    if (shadow_capture_.Active()) CaptureGpuInfo();\n    fclose(cap_);')
    backend = replace_once(backend, '        CapSave(target, name);\n    }',
        '        CapSave(target, name);\n'
        '        if (shadow_capture_.Active()) {\n'
        '            snprintf(name, sizeof(name), "after_frame_%08X_%ux%u", target.data, target.w, target.h);\n'
        '            if (!QueueDepthRead(target, name)) fprintf(cap_, "depth_read_failed %s\\n", name);\n'
        '        }\n'
        '    }\n'
        '    DrainDepthReads();')
    return backend


def adapt_shaders(shaders, replace_once):
    shaders = '#include "shadow_diagnostic_modes.h"\n' + shaders
    shaders = replace_once(shaders, '    return R"(// the sampler compares,',
        '    return\n#ifdef _WIN32\n        openshao::ShadowComparisonHlsl() +\n#endif\n        R"(// the sampler compares,')
    shaders = replace_once(shaders, 't.shadow ? "SamplerComparisonState" : "SamplerState"',
        '(t.shadow && !(t.shadow & openshao::kShadowManual)) ? "SamplerComparisonState" : "SamplerState"')
    return shaders
