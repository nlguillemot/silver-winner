#include "apputil.h"

#include "dxutil.h"

#include <cstdio>
#include <cstdarg>
#include <memory>

void SimpleMessageBox_FatalError(const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);

    int nc = _vscprintf(fmt, vl);
    std::unique_ptr<char[]> chars = std::make_unique<char[]>(nc + 1);
    vsnprintf_s(chars.get(), nc + 1, nc, fmt, vl);

    std::wstring wmsg = WideFromMultiByte(chars.get());
    MessageBoxW(NULL, wmsg.c_str(), L"Fatal Error", MB_OK);
    
    va_end(vl);

#ifdef _DEBUG
    DebugBreak();
#endif

    ExitProcess(-1);
}