#include "app.h"
#include "renderer.h"

#include "dxutil.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"

struct App
{
    HWND hWnd;
    bool bShouldClose;
};

App g_App;

IMGUI_API LRESULT ImGui_ImplDX11_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplDX11_WndProcHandler(hWnd, msg, wParam, lParam))
    {
        return 1;
    }

    switch (msg)
    {
    case WM_SIZE:
        if (RendererIsInit() && wParam != SIZE_MINIMIZED)
        {
            int windowWidth = (int)LOWORD(lParam);
            int windowHeight = (int)HIWORD(lParam);
            int renderWidth = windowWidth;
            int renderHeight = windowHeight;
            RendererResize(windowWidth, windowHeight, renderWidth, renderHeight);
        }
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_CLOSE:
        g_App.bShouldClose = true;
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void AppInit(int width, int height, const char* title)
{
    CHECKHR(SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE));

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"WindowClass";
    CHECKWIN32(RegisterClassExW(&wc));

    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    DWORD dwExStyle = 0;
    RECT wr = { 0, 0, width, height };
    CHECKWIN32(AdjustWindowRectEx(&wr, dwStyle, FALSE, dwExStyle));

    std::wstring wtitle = WideFromMultiByte(title);

    HWND hWnd = CreateWindowExW(
        dwExStyle, L"WindowClass", wtitle.c_str(), dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, GetModuleHandleW(NULL), NULL);
    CHECKWIN32(hWnd != NULL);

    RendererInit(hWnd);

    RECT cr;
    CHECKWIN32(GetClientRect(hWnd, &cr));
    RendererResize(
        cr.right - cr.left, cr.bottom - cr.top,
        cr.right - cr.left, cr.bottom - cr.top);

    ShowWindow(hWnd, SW_SHOWDEFAULT);

    g_App.hWnd = hWnd;
    g_App.bShouldClose = false;
}

static void AppExit()
{
    RendererExit();
    
    CHECKWIN32(DestroyWindow(g_App.hWnd));
}

void AppMain()
{
    AppInit(1280, 720, "silver-winner");

    for (;;)
    {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (g_App.bShouldClose)
        {
            break;
        }

        ImGui_ImplDX11_NewFrame();

        RendererPaint();
    }

    AppExit();
}

void AppGetClientCursorPos(int* x, int* y)
{
    POINT cursorPos;
    CHECKWIN32(GetCursorPos(&cursorPos));
    CHECKWIN32(ScreenToClient(g_App.hWnd, &cursorPos));
    if (x) *x = cursorPos.x;
    if (y) *y = cursorPos.y;
}