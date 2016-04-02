#include "scene.h"

struct Scene
{
    ID3D11Device* pDevice;
    ID3D11DeviceContext* pDeviceContext;

    ComPtr<ID3D11Texture2D> pSceneDepthTex2D;
    ComPtr<ID3D11DepthStencilView> pSceneDepthDSV;
};

Scene g_Scene;

void SceneInit(ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext)
{
    g_Scene.pDevice = pDevice;
    g_Scene.pDeviceContext = pDeviceContext;
}

void SceneResize(
    int windowWidth, int windowHeight,
    int renderWidth, int renderHeight)
{
    ID3D11Device* dev = g_Scene.pDevice;

    D3D11_TEXTURE2D_DESC sceneDepthTex2DDesc = {};
    sceneDepthTex2DDesc.Width = renderWidth;
    sceneDepthTex2DDesc.Height = renderHeight;
    sceneDepthTex2DDesc.MipLevels = 1;
    sceneDepthTex2DDesc.ArraySize = 1;
    sceneDepthTex2DDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    sceneDepthTex2DDesc.SampleDesc.Count = 1;
    sceneDepthTex2DDesc.Usage = D3D11_USAGE_DEFAULT;
    sceneDepthTex2DDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> pSceneDepthTex2D;
    CHECKHR(dev->CreateTexture2D(&sceneDepthTex2DDesc, NULL, &pSceneDepthTex2D));

    D3D11_DEPTH_STENCIL_VIEW_DESC sceneDepthDSVDesc = {};
    sceneDepthDSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
    sceneDepthDSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11DepthStencilView> pSceneDepthDSV;
    CHECKHR(dev->CreateDepthStencilView(pSceneDepthTex2D.Get(), &sceneDepthDSVDesc, &pSceneDepthDSV));

    g_Scene.pSceneDepthTex2D.Swap(pSceneDepthTex2D);
    g_Scene.pSceneDepthDSV.Swap(pSceneDepthDSV);
}

void ScenePaint(ID3D11RenderTargetView* pBackBufferRTV)
{
    ID3D11Device* dev = g_Scene.pDevice;
    ID3D11DeviceContext* dc = g_Scene.pDeviceContext;

    const float kClearColor[] = {
        std::pow(100.0f / 255.0f, 2.2f),
        std::pow(149.0f / 255.0f, 2.2f),
        std::pow(237.0f / 255.0f, 2.2f),
        1.0f
    };
    dc->ClearRenderTargetView(pBackBufferRTV, kClearColor);

    ID3D11RenderTargetView* rtvs[] = { pBackBufferRTV };
    ID3D11DepthStencilView* dsv = g_Scene.pSceneDepthDSV.Get();
    dc->OMSetRenderTargets(_countof(rtvs), rtvs, dsv);
    dc->OMSetRenderTargets(0, NULL, NULL);
}