#include "renderer.h"

#include "dxutil.h"
#include "apputil.h"

#include "scene.h"

static const D3D_FEATURE_LEVEL kMinFeatureLevel = D3D_FEATURE_LEVEL_12_1;
static const int kSwapChainBufferCount = 3;
static const DXGI_FORMAT kSwapChainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
static const DXGI_FORMAT kSwapChainRTVFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
static const UINT kSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

struct Renderer
{
    ComPtr<ID3D11Device> pDevice;
    ComPtr<ID3D11DeviceContext> pDeviceContext;
    ComPtr<IDXGISwapChain3> pSwapChain;
    HANDLE hFrameLatencyWaitableObject;
    D3D11_RENDER_TARGET_VIEW_DESC BackBufferRTVDesc;

    ComPtr<ID3D11Texture2D> pSceneDepthTex2D;
    ComPtr<ID3D11DepthStencilView> pSceneDSV;
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
    flags |= D3D11_CREATE_DEVICE_DEBUG; // | D3D11_CREATE_DEVICE_DEBUGGABLE;
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

    SceneInit(pDevice.Get(), pDeviceContext.Get());

    g_Renderer.pDevice.Swap(pDevice);
    g_Renderer.pDeviceContext.Swap(pDeviceContext);
    g_Renderer.pSwapChain.Swap(pSwapChain);
    g_Renderer.hFrameLatencyWaitableObject = hFrameLatencyWaitableObject;
}

void RendererExit()
{
    g_Renderer = Renderer();
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
    
    ScenePaint(pBackBufferRTV.Get());

    CHECKHR(sc->Present(0, 0));
}