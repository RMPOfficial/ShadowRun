#include <windows.h>
#include "ShadowRun\ShadowRun.h"

extern "C" {
#pragma function(memset)
    void* __cdecl memset(void* dest, int c, size_t count) {
        char* bytes = (char*)dest;
        while (count--) {
            *bytes++ = (char)c;
        }
        return dest;
    }
#pragma function(memcpy)
    void* __cdecl memcpy(void* dest, const void* src, size_t count) {
        char* d = (char*)dest;
        const char* s = (const char*)src;
        while (count--) {
            *d++ = *s++;
        }
        return dest;
    }
}


extern bool isRunning;

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define ID_EDIT   101
#define ID_BUTTON 102

HWND hEdit;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hFont = CreateFontW(
            -18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
        );

        hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            18, 23, 480, 36, hwnd, (HMENU)ID_EDIT, NULL, NULL);
        SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hButton = CreateWindowExW(0, L"BUTTON", L"Run",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            378, 72, 120, 37, hwnd, (HMENU)ID_BUTTON, NULL, NULL);
        SendMessageW(hButton, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == ID_BUTTON) {

            constexpr DWORD maxCmdLength = MAX_PATH;
            wchar_t cmdLine[maxCmdLength];

            if (GetWindowTextW(hEdit, cmdLine, maxCmdLength) > 0) {
                STARTUPINFOW si = { sizeof(si) };
                PROCESS_INFORMATION pi = { 0 };

                ShadowRun(cmdLine);
            }
        }
        break;
    }
    case WM_DESTROY:
		ShadowStop();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void Entry() 
{
    HINSTANCE hInstance = GetModuleHandleW(NULL);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"NoCrtRunClass";
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);

    if (!RegisterClassExW(&wc)) {
        ExitProcess(1);
    }

    RECT rc = { 0, 0, 516, 128 };
    AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);

    HWND hwnd = CreateWindowExW(0, L"NoCrtRunClass", L"Run",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        ExitProcess(1);
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ExitProcess((UINT)msg.wParam);
}
