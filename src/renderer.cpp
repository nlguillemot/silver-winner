#include "renderer.h"

#include "dxutil.h"
#include "apputil.h"

#include "scene.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"

#include <d3dcompiler.h>

#include <vector>

static const D3D_FEATURE_LEVEL kMinFeatureLevel = D3D_FEATURE_LEVEL_12_1;
static const int kSwapChainBufferCount = 3;
static const DXGI_FORMAT kSwapChainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
static const DXGI_FORMAT kSwapChainRTVFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
static const UINT kSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

struct ReloadableShader
{
    std::string Path;
    std::string EntryPoint;
    std::string Target;

    ComPtr<ID3DBlob> Blob;
    ComPtr<ID3D11DeviceChild> ShaderComPtr;

    Shader* pShader;
};

struct Renderer
{
    bool IsInit;

    ComPtr<ID3D11Device> pDevice;
    ComPtr<ID3D11DeviceContext> pDeviceContext;
    ComPtr<IDXGISwapChain3> pSwapChain;
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
    ComPtr<IDXGISwapChain3> pSwapChain;
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

    g_Renderer.pDevice.Swap(pDevice);
    g_Renderer.pDeviceContext.Swap(pDeviceContext);
    g_Renderer.pSwapChain.Swap(pSwapChain);
    g_Renderer.hFrameLatencyWaitableObject = hFrameLatencyWaitableObject;
    g_Renderer.IsInit = true;

    ImGui_ImplDX11_Init(pNativeWindowHandle, g_Renderer.pDevice.Get(), g_Renderer.pDeviceContext.Get());
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

void RendererPaint()
{
    ID3D11Device* dev = g_Renderer.pDevice.Get();
    ID3D11DeviceContext* dc = g_Renderer.pDeviceContext.Get();
    IDXGISwapChain* sc = g_Renderer.pSwapChain.Get();
    HANDLE hFrameLatencyWaitableObject = g_Renderer.hFrameLatencyWaitableObject;
    D3D11_RENDER_TARGET_VIEW_DESC* pBackBufferRTVDesc = &g_Renderer.BackBufferRTVDesc;

    // Wait until the previous frame is presented before drawing the next frame
    CHECKWIN32(WaitForSingleObject(hFrameLatencyWaitableObject, INFINITE) == WAIT_OBJECT_0);

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

Shader* RendererAddShader(const char* file, const char* entry, const char* target)
{
    ID3D11Device* dev = g_Renderer.pDevice.Get();

    std::string path = std::string("shaders/") + file;
    std::wstring wpath = WideFromMultiByte(path);

    UINT flags = 0;
#if _DEBUG
    flags |= D3DCOMPILE_DEBUG;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ComPtr<ID3DBlob> pCode;
    ComPtr<ID3DBlob> pErrorMsgs;
    HRESULT hr = D3DCompileFromFile(wpath.c_str(), NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, target, flags, 0, &pCode, &pErrorMsgs);
    if (FAILED(hr))
    {
        std::string hrs = MultiByteFromHR(hr);
        SimpleMessageBox_FatalError(
            "Error (%s):\n%s%s%s",
            path.c_str(),
            hrs.c_str(),
            pErrorMsgs ? "\n" : "",
            pErrorMsgs ? pErrorMsgs->GetBufferPointer() : "");
        
        return NULL;
    }

    if (pErrorMsgs)
    {
        printf("Warning (%s): %s\n", path.c_str(), (char*)pErrorMsgs->GetBufferPointer());
    }

    ComPtr<ID3D11DeviceChild> shaderComPtr;
    ID3D11VertexShader* VS = 0;
    ID3D11PixelShader* PS = 0;
    ID3D11GeometryShader* GS = 0;
    ID3D11HullShader* HS = 0;
    ID3D11DomainShader* DS = 0;
    ID3D11ComputeShader* CS = 0;

    std::string target2 = std::string(target).substr(0, 2);
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
        SimpleMessageBox_FatalError("Unhandled shader target: %s\n", target);
    }

    Shader* sh = (Shader*)calloc(1, sizeof(Shader));
    (ID3DBlob*&)sh->Blob = pCode.Get();
    (ID3D11VertexShader*&)sh->VS = VS;
    (ID3D11PixelShader*&)sh->PS = PS;
    (ID3D11GeometryShader*&)sh->GS = GS;
    (ID3D11HullShader*&)sh->HS = HS;
    (ID3D11DomainShader*&)sh->DS = DS;
    (ID3D11ComputeShader*&)sh->CS = CS;

    ReloadableShader rs;
    rs.Path = path;
    rs.EntryPoint = entry;
    rs.Target = target;
    rs.Blob = pCode;
    rs.ShaderComPtr = shaderComPtr;
    rs.pShader = sh;

    g_Renderer.Shaders.push_back(sh);
    g_Renderer.ShaderReloaders.push_back(std::move(rs));

    return sh;
}