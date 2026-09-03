#include <windows.h>
#include <commctrl.h>
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
#define ID_STATUS 103

HWND hEdit;
HWND hButton;
HWND hStatus;

static HFONT CreateUiFont(int height)
{
    return CreateFontW(
        height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hFont = CreateUiFont(-18);
        HFONT hSmallFont = CreateUiFont(-12);

        HWND hLabel = CreateWindowExW(0, L"STATIC", L"Command line:",
            WS_CHILD | WS_VISIBLE,
            18, 12, 480, 18, hwnd, NULL, NULL, NULL);
        SendMessageW(hLabel, WM_SETFONT, (WPARAM)hSmallFont, TRUE);

        hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            18, 34, 480, 36, hwnd, (HMENU)ID_EDIT, NULL, NULL);
        SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hEdit, EM_SETCUEBANNER, (WPARAM)TRUE, (LPARAM)L"e.g. cmd.exe /c whoami");

        hButton = CreateWindowExW(0, L"BUTTON", L"Run",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            378, 82, 120, 37, hwnd, (HMENU)ID_BUTTON, NULL, NULL);
        SendMessageW(hButton, WM_SETFONT, (WPARAM)hFont, TRUE);

        hStatus = CreateWindowExW(0, L"STATIC", L"Ready",
            WS_CHILD | WS_VISIBLE,
            18, 86, 344, 20, hwnd, (HMENU)ID_STATUS, NULL, NULL);
        SendMessageW(hStatus, WM_SETFONT, (WPARAM)hSmallFont, TRUE);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == ID_BUTTON) {
            wchar_t cmdLine[MAX_PATH];

            if (GetWindowTextW(hEdit, cmdLine, MAX_PATH) == 0) {
                SetWindowTextW(hStatus, L"Enter a command line first.");
                break;
            }

            EnableWindow(hButton, FALSE);
            SetWindowTextW(hStatus, L"Launching...");

            if (!ShadowRun(cmdLine, hwnd)) {
                EnableWindow(hButton, TRUE);
                SetWindowTextW(hStatus, L"Launch failed (see error dialog).");
            } else {
                wchar_t status[512];
                wsprintfW(status, L"Running: %s", cmdLine);
                SetWindowTextW(hStatus, status);
            }
        }
        break;
    }
    case WM_SHADOW_DONE: {
        EnableWindow(hButton, TRUE);
        switch (LOWORD(wParam)) {
        case SHADOW_RESTORED:
            SetWindowTextW(hStatus, L"Process exited - executable restored.");
            break;
        case SHADOW_RESTORE_FAILED:
            SetWindowTextW(hStatus, L"Process exited - restore failed (see error dialog).");
            break;
        default:
            SetWindowTextW(hStatus, L"Process exited - not shadowed.");
            break;
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

    HWND hwnd = CreateWindowExW(0, L"NoCrtRunClass", L"ShadowRun",
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
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    ExitProcess((UINT)msg.wParam);
}
