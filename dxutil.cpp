#include "dxutil.h"

#include <comdef.h>
#include <set>
#include <tuple>
#include <memory>
#include <mutex>

std::set<std::tuple<std::string, std::string, int>> g_IgnoredAsserts;
std::mutex g_IgnoredAssertsMutex;

std::wstring WideFromMultiByte(const char* s)
{
    int bufSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
    CHECKWIN32(bufSize != 0);

    std::wstring ws;
    if (bufSize > 0)
    {
        std::unique_ptr<WCHAR[]> wbuf = std::make_unique<WCHAR[]>(bufSize);
        CHECKWIN32(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, wbuf.get(), bufSize));
        ws = wbuf.get();
    }
    return ws;
}

std::wstring WideFromMultiByte(const std::string& s)
{
    return WideFromMultiByte(s.c_str());
}

bool detail_CheckHR(HRESULT hr, const char* file, const char* function, int line)
{
    if (SUCCEEDED(hr))
    {
        return true;
    }

    std::lock_guard<std::mutex> lock(g_IgnoredAssertsMutex);

    if (g_IgnoredAsserts.find(std::make_tuple(file, function, line)) != g_IgnoredAsserts.end())
    {
        return false;
    }

    std::wstring wfile = WideFromMultiByte(file);
    std::wstring wfunction = WideFromMultiByte(function);
    _com_error err(hr);

    std::wstring msg = std::wstring() +
        L"File: " + wfile + L"\n" +
        L"Function: " + wfunction + L"\n" +
        L"Line: " + std::to_wstring(line) + L"\n" +
        L"ErrorMessage: " + err.ErrorMessage() + L"\n";

    int result = MessageBoxW(NULL, msg.c_str(), L"Error", MB_ABORTRETRYIGNORE);
    if (result == IDABORT)
    {
        ExitProcess(-1);
    }
    else if (result == IDRETRY)
    {
        DebugBreak();
    }
    else if (result == IDIGNORE)
    {
        g_IgnoredAsserts.insert(std::make_tuple(file, function, line));
    }

    return false;
}

bool detail_CheckWin32(BOOL okay, const char* file, const char* function, int line)
{
    if (okay)
    {
        return true;
    }

    return detail_CheckHR(HRESULT_FROM_WIN32(GetLastError()), file, function, line);
}