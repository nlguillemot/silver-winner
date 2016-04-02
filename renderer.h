#pragma once

void RendererInit(void* pNativeWindowHandle);
void RendererExit();

void RendererResize(
    int windowWidth, int windowHeight, 
    int renderWidth, int renderHeight);

void RendererPaint();