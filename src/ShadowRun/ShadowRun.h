#pragma once
#include <windows.h>

// Posted to the window passed to ShadowRun() when a shadowing session ends.
// wParam carries one of the ShadowResult values; lParam is unused.
#define WM_SHADOW_DONE (WM_APP + 1)

enum ShadowResult {
	SHADOW_RESTORED = 0,
	SHADOW_RESTORE_FAILED = 1,
	SHADOW_SKIPPED = 2
};

bool ShadowRun(wchar_t* cmdLine, HWND hwnd);
void ShadowStop();
