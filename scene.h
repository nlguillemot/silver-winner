#pragma once

#include "dxutil.h"

void SceneInit(
    ID3D11Device* pDevice,
    ID3D11DeviceContext* pDeviceContext);

void SceneUpdate();

void SceneResize(
    int windowWidth, int windowHeight,
    int renderWidth, int renderHeight);

void ScenePaint(ID3D11RenderTargetView* pBackBufferRTV);