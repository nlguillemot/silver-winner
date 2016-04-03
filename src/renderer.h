#pragma once

#include <d3d11.h>

struct Shader
{
    Shader() = delete;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    ID3DBlob* const Blob;

    ID3D11VertexShader* const VS;
    ID3D11PixelShader* const PS;
    ID3D11GeometryShader* const GS;
    ID3D11HullShader* const HS;
    ID3D11DomainShader* const DS;
    ID3D11ComputeShader* const CS;
};

void RendererInit(void* pNativeWindowHandle);
void RendererExit();
bool RendererIsInit();

void RendererResize(
    int windowWidth, int windowHeight, 
    int renderWidth, int renderHeight);

void RendererPaint();

ID3D11Device* RendererGetDevice();
ID3D11DeviceContext* RendererGetDeviceContext();

Shader* RendererAddShader(const char* file, const char* entry, const char* target);