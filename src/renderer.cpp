#include "renderer.h"

#include "dxutil.h"
#include "apputil.h"

#include "scene.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"

#include <d3dcompiler.h>

#include <vector>

static const D3D_FEATURE_LEVEL kMinFeatureLevel = D3D_FEATURE_LEVEL_11_0;
static const int kSwapChainBufferCount = 3;
static const DXGI_FORMAT kSwapChainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
static const DXGI_FORMAT kSwapChainRTVFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
static const UINT kSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

struct ReloadableShader
{
    std::string Path;
    std::string EntryPoint;
    std::string Target;
    uint64_t Timestamp;

    ComPtr<ID3DBlob> Blob;
    ComPtr<ID3D11DeviceChild> ShaderComPtr;

    Shader* pShader;
};

struct Renderer
{
    bool IsInit;

    ComPtr<IDXGIFactory> pDXGIFactory;
    ComPtr<IDXGIAdapter> pDXGIAdapter;

    ComPtr<ID3D11Device> pDevice;
    ComPtr<ID3D11DeviceContext> pDeviceContext;
    ComPtr<IDXGISwapChain2> pSwapChain;
    HANDLE hFrameLatencyWaitableObject;
    D3D11_RENDER_TARGET_VIEW_DESC BackBufferRTVDesc;

    std::vector<Shader*> Shaders;
    std::vector<ReloadableShader> ShaderReloaders;
};

Renderer g_Renderer;

void RendererInit(void* pNativeWindowHandle)
{
    ComPtr<IDXGIFactory> pDXGIFactory;
    CHECKHR(CreateDXGIFactory(IID_PPV_ARGS(&pDXGIFactory)));

    ComPtr<IDXGIAdapter> pDXGIAdapter;
    CHECKHR(pDXGIFactory->EnumAdapters(0, &pDXGIAdapter));

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferDesc.Format = kSwapChainFormat;
    scd.SampleDesc.Count = 1;
    scd.BufferCount = kSwapChainBufferCount;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = (HWND)pNativeWindowHandle;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags = kSwapChainFlags;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    ComPtr<ID3D11Device> pDevice;
    ComPtr<ID3D11DeviceContext> pDeviceContext;
    ComPtr<IDXGISwapChain2> pSwapChain;
    HANDLE hFrameLatencyWaitableObject;

    D3D_FEATURE_LEVEL kFeatureLevels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    D3D_FEATURE_LEVEL featureLevel;
    ComPtr<IDXGISwapChain> pTmpSwapChain;
    CHECKHR(D3D11CreateDeviceAndSwapChain(
        pDXGIAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, flags,
        kFeatureLevels, _countof(kFeatureLevels),
        D3D11_SDK_VERSION,
        &scd, &pTmpSwapChain,
        &pDevice, &featureLevel, &pDeviceContext));
    CHECKHR(pTmpSwapChain.As(&pSwapChain));

    if (featureLevel < kMinFeatureLevel)
    {
        SimpleMessageBox_FatalError(
            "Minimum D3D feature level not satisfied:\n"
            "Minimum feature level: %d.%d\n"
            "Actual feature level: %d.%d\n",
            (kMinFeatureLevel >> 12) & 0x0F, (kMinFeatureLevel >> 8) & 0x0F,
            (featureLevel >> 12) & 0x0F, (featureLevel >> 8) & 0x0F);
    }

    hFrameLatencyWaitableObject = pSwapChain->GetFrameLatencyWaitableObject();
    CHECKHR(pDXGIFactory->MakeWindowAssociation((HWND)pNativeWindowHandle, DXGI_MWA_NO_WINDOW_CHANGES));

    g_Renderer.pDXGIAdapter = pDXGIAdapter;
    g_Renderer.pDXGIFactory = pDXGIFactory;
    g_Renderer.pDevice = pDevice;
    g_Renderer.pDeviceContext = pDeviceContext;
    g_Renderer.pSwapChain = pSwapChain;
    g_Renderer.hFrameLatencyWaitableObject = hFrameLatencyWaitableObject;
    g_Renderer.IsInit = true;

    ImGui_ImplDX11_Init(pNativeWindowHandle, pDevice.Get(), pDeviceContext.Get());
    SceneInit();
}

void RendererExit()
{
    for (Shader* sh : g_Renderer.Shaders)
    {
        free(sh);
    }

    ImGui_ImplDX11_Shutdown();
    g_Renderer = Renderer();
}

bool RendererIsInit()
{
    return g_Renderer.IsInit;
}

static void RendererReloadShader(ReloadableShader* shader)
{
    ID3D11Device* dev = g_Renderer.pDevice.Get();

    std::string path = shader->Path;
    std::wstring wpath = WideFromMultiByte(path);

    uint64_t newTimestamp = 0;

    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        FILETIME lastWriteTime;
        if (GetFileTime(hFile, NULL, NULL, &lastWriteTime))
        {
            LARGE_INTEGER largeWriteTime;
            largeWriteTime.HighPart = lastWriteTime.dwHighDateTime;
            largeWriteTime.LowPart = lastWriteTime.dwLowDateTime;
            newTimestamp = largeWriteTime.QuadPart;
        }

        CloseHandle(hFile);
    }

    if (newTimestamp == 0 || (shader->Timestamp != 0 && shader->Timestamp >= newTimestamp))
    {
        return;
    }

    shader->Timestamp = newTimestamp;

    UINT flags = 0;
#if _DEBUG
    flags |= D3DCOMPILE_DEBUG;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ComPtr<ID3DBlob> pCode;
    ComPtr<ID3DBlob> pErrorMsgs;
    HRESULT hr = D3DCompileFromFile(wpath.c_str(), NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, shader->EntryPoint.c_str(), shader->Target.c_str(), flags, 0, &pCode, &pErrorMsgs);
    if (FAILED(hr))
    {
        std::string hrs = MultiByteFromHR(hr);
        fprintf(stderr,
            "Error (%s):\n%s%s%s\n",
            path.c_str(),
            hrs.c_str(),
            pErrorMsgs ? "\n" : "",
            pErrorMsgs ? (const char*)pErrorMsgs->GetBufferPointer() : "");

        return;
    }

    if (pErrorMsgs)
    {
        printf("Warning (%s): %s\n", path.c_str(), (char*)pErrorMsgs->GetBufferPointer());
    }
    else
    {
        printf("%s compiled clean\n", path.c_str());
    }

    ComPtr<ID3D11DeviceChild> shaderComPtr;
    ID3D11VertexShader* VS = 0;
    ID3D11PixelShader* PS = 0;
    ID3D11GeometryShader* GS = 0;
    ID3D11HullShader* HS = 0;
    ID3D11DomainShader* DS = 0;
    ID3D11ComputeShader* CS = 0;

    std::string target2 = std::string(shader->Target).substr(0, 2);
    if (target2 == "vs")
    {
        ComPtr<ID3D11VertexShader> vs;
        CHECKHR(dev->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), NULL, &vs));
        shaderComPtr = vs;
        VS = vs.Get();
    }
    else if (target2 == "ps")
    {
        ComPtr<ID3D11PixelShader> ps;
        CHECKHR(dev->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), NULL, &ps));
        shaderComPtr = ps;
        PS = ps.Get();
    }
    else if (target2 == "gs")
    {
        ComPtr<ID3D11GeometryShader> gs;
        CHECKHR(dev->CreateGeometryShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), NULL, &gs));
        shaderComPtr = gs;
        GS = gs.Get();
    }
    else if (target2 == "hs")
    {
        ComPtr<ID3D11HullShader> hs;
        CHECKHR(dev->CreateHullShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), NULL, &hs));
        shaderComPtr = hs;
        HS = hs.Get();
    }
    else if (target2 == "ds")
    {
        ComPtr<ID3D11DomainShader> ds;
        CHECKHR(dev->CreateDomainShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), NULL, &ds));
        shaderComPtr = ds;
        DS = ds.Get();
    }
    else if (target2 == "cs")
    {
        ComPtr<ID3D11ComputeShader> cs;
        CHECKHR(dev->CreateComputeShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), NULL, &cs));
        shaderComPtr = cs;
        CS = cs.Get();
    }
    else
    {
        SimpleMessageBox_FatalError("Unhandled shader target: %s\n", shader->Target.c_str());
    }

    (ID3DBlob*&)shader->pShader->Blob = pCode.Get();
    (ID3D11VertexShader*&)shader->pShader->VS = VS;
    (ID3D11PixelShader*&)shader->pShader->PS = PS;
    (ID3D11GeometryShader*&)shader->pShader->GS = GS;
    (ID3D11HullShader*&)shader->pShader->HS = HS;
    (ID3D11DomainShader*&)shader->pShader->DS = DS;
    (ID3D11ComputeShader*&)shader->pShader->CS = CS;

    shader->Blob = pCode;
    shader->ShaderComPtr = shaderComPtr;
}

Shader* RendererAddShader(const char* file, const char* entry, const char* target)
{
    ID3D11Device* dev = g_Renderer.pDevice.Get();

    std::string path = std::string("shaders/") + file;

    Shader* sh = (Shader*)calloc(1, sizeof(Shader));

    ReloadableShader rs;
    rs.Path = path;
    rs.EntryPoint = entry;
    rs.Target = target;
    rs.Timestamp = 0;
    rs.pShader = sh;

    RendererReloadShader(&rs);

    g_Renderer.Shaders.push_back(sh);
    g_Renderer.ShaderReloaders.push_back(std::move(rs));

    return sh;
}

void RendererResize(
    int windowWidth, int windowHeight,
    int renderWidth, int renderHeight)
{
    IDXGISwapChain* sc = g_Renderer.pSwapChain.Get();
    D3D11_RENDER_TARGET_VIEW_DESC* pBackBufferRTVDesc = &g_Renderer.BackBufferRTVDesc;

    CHECKHR(sc->ResizeBuffers(
        kSwapChainBufferCount,
        renderWidth, renderHeight,
        kSwapChainFormat, kSwapChainFlags));

    pBackBufferRTVDesc->Format = kSwapChainRTVFormat;
    pBackBufferRTVDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    SceneResize(windowWidth, windowHeight, renderWidth, renderHeight);
}

static void RendererShowSystemInfoGUI()
{
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiSetCond_Always);
    if (ImGui::Begin("Info", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        char cpuBrandString[0x40];
        memset(cpuBrandString, 0, sizeof(cpuBrandString));

        int cpuInfo[4] = { -1 };
        __cpuid(cpuInfo, 0x80000002);
        memcpy(cpuBrandString, cpuInfo, sizeof(cpuInfo));
        __cpuid(cpuInfo, 0x80000003);
        memcpy(cpuBrandString + 16, cpuInfo, sizeof(cpuInfo));
        __cpuid(cpuInfo, 0x80000004);
        memcpy(cpuBrandString + 32, cpuInfo, sizeof(cpuInfo));

        ImGui::Text("CPU: %s", cpuBrandString);

        DXGI_ADAPTER_DESC adapterDesc;
        if (SUCCEEDED(g_Renderer.pDXGIAdapter->GetDesc(&adapterDesc)))
        {
            std::string description = MultiByteFromWide(adapterDesc.Description);
            ImGui::Text("Adapter: %s", description.c_str());
            
            ImGui::Text("Total video memory: %d MB", adapterDesc.DedicatedVideoMemory / 1024 / 1024);
            
            if (adapterDesc.DedicatedSystemMemory != 0)
                ImGui::Text("Total system memory: %d MB", adapterDesc.DedicatedSystemMemory / 1024 / 1024);

            ComPtr<IDXGIAdapter3> adapter3;
            if (SUCCEEDED(g_Renderer.pDXGIAdapter.As(&adapter3)))
            {
                DXGI_QUERY_VIDEO_MEMORY_INFO vidmeminfo;
                if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &vidmeminfo)))
                    ImGui::Text("Local memory usage: %d MB", vidmeminfo.CurrentUsage / 1024 / 1024);

                if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &vidmeminfo)))
                    ImGui::Text("Non-local memory usage: %d MB", vidmeminfo.CurrentUsage / 1024 / 1024);
            }
        }

        D3D_FEATURE_LEVEL featureLevel = g_Renderer.pDevice->GetFeatureLevel();
        ImGui::Text("Feature level %d.%d", (featureLevel >> 12) & 0x0F, (featureLevel >> 8) & 0x0F);
    }
    ImGui::End();
}

void RendererPaint()
{
    ID3D11Device* dev = g_Renderer.pDevice.Get();
    ID3D11DeviceContext* dc = g_Renderer.pDeviceContext.Get();
    IDXGISwapChain* sc = g_Renderer.pSwapChain.Get();
    HANDLE hFrameLatencyWaitableObject = g_Renderer.hFrameLatencyWaitableObject;
    D3D11_RENDER_TARGET_VIEW_DESC* pBackBufferRTVDesc = &g_Renderer.BackBufferRTVDesc;

    // Wait until the previous frame is presented before drawing the next frame
    CHECKWIN32(WaitForSingleObject(hFrameLatencyWaitableObject, INFINITE) == WAIT_OBJECT_0);

    // Reload all shaders
    for (ReloadableShader& shader : g_Renderer.ShaderReloaders)
    {
        RendererReloadShader(&shader);
    }

    RendererShowSystemInfoGUI();

    // grab the current backbuffer
    ComPtr<ID3D11Texture2D> pBackBufferTex2D;
    ComPtr<ID3D11RenderTargetView> pBackBufferRTV;
    CHECKHR(sc->GetBuffer(0, IID_PPV_ARGS(&pBackBufferTex2D)));
    CHECKHR(dev->CreateRenderTargetView(pBackBufferTex2D.Get(), pBackBufferRTVDesc, &pBackBufferRTV));

    // Render Scene
    ScenePaint(pBackBufferRTV.Get());

    // Render ImGui
    ID3D11RenderTargetView* imguiRTVs[] = { pBackBufferRTV.Get() };
    dc->OMSetRenderTargets(_countof(imguiRTVs), imguiRTVs, NULL);
    ImGui::Render();
    dc->OMSetRenderTargets(0, NULL, NULL);

    CHECKHR(sc->Present(0, 0));
}

ID3D11Device* RendererGetDevice()
{
    return g_Renderer.pDevice.Get();
}

ID3D11DeviceContext* RendererGetDeviceContext()
{
    return g_Renderer.pDeviceContext.Get();
}
