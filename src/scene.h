#pragma once

#include "dxutil.h"

void SceneInit();

void SceneUpdate();

void SceneResize(
    int windowWidth, int windowHeight,
    int renderWidth, int renderHeight);

void ScenePaint(ID3D11RenderTargetView* pBackBufferRTV);