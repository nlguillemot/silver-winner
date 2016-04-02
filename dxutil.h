#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShellScalingApi.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <string>

using Microsoft::WRL::ComPtr;

std::wstring WideFromMultiByte(const char* s);
std::wstring WideFromMultiByte(const std::string& s);

bool detail_CheckHR(HRESULT hr, const char* file, const char* function, int line);
bool detail_CheckWin32(BOOL okay, const char* file, const char* function, int line);

#define CHECKHR(hr) detail_CheckHR(hr, __FILE__, __FUNCSIG__, __LINE__)
#define CHECKWIN32(okay) detail_CheckWin32(okay, __FILE__, __FUNCSIG__, __LINE__)