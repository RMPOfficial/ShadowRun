#include <windows.h>
#include "ShadowRun.h"

#pragma optimize("", off)
void optimized_zero_memory(void* ptr, size_t size) {
	size_t chunks = size / 8;
	if (chunks > 0) {
		__stosq((unsigned __int64*)ptr, 0, chunks);
	}

	size_t remainder = size % 8;
	if (remainder > 0) {
		__stosb(((unsigned char*)ptr) + (chunks * 8), 0, remainder);
	}
}
#pragma optimize("", on)

HANDLE hStopEvent = NULL;
static size_t threadCount = 0;
static HWND hNotifyWindow = NULL;

static void NotifyDone(int result)
{
	if (hNotifyWindow)
		PostMessageW(hNotifyWindow, WM_SHADOW_DONE, (WPARAM)result, 0);
}

static DWORD WINAPI ShadowThread(LPVOID lpParam)
{
	HANDLE hProcess = (HANDLE)lpParam;

	WCHAR filePath[MAX_PATH];
	DWORD filePathSize = MAX_PATH;
	QueryFullProcessImageNameW(hProcess, 0, filePath, &filePathSize);

	HANDLE hRead = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hRead == INVALID_HANDLE_VALUE)
	{
		CHAR errorMsg[128];
		wsprintfA(errorMsg, "Failed to open file for reading.\nError code: %lu\n\nTerminate process?", GetLastError());
		if (MessageBoxA(NULL, errorMsg, "Error", MB_YESNO | MB_ICONERROR) == IDYES)
			TerminateProcess(hProcess, 0);
		CloseHandle(hProcess);
		NotifyDone(SHADOW_SKIPPED);
		return 0;
	}

	LARGE_INTEGER fileSize;
	GetFileSizeEx(hRead, &fileSize);

	BYTE* fileData = (BYTE*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)fileSize.QuadPart);
	if (!fileData)
	{
		MessageBoxA(NULL, "Out of memory while reading the file.", "Error", MB_OK | MB_ICONERROR);
		CloseHandle(hRead);
		CloseHandle(hProcess);
		NotifyDone(SHADOW_SKIPPED);
		return 0;
	}

	DWORD totalRead = 0;
	while (totalRead < (DWORD)fileSize.QuadPart)
	{
		DWORD toRead = (DWORD)fileSize.QuadPart - totalRead;
		if (toRead > 65536) toRead = 65536;
		DWORD bytesRead = 0;
		if (!ReadFile(hRead, fileData + totalRead, toRead, &bytesRead, NULL) || bytesRead == 0)
			break;
		totalRead += bytesRead;
	}

	FILETIME fts[3]{}; // {creation, last access, last write}
	GetFileTime(hRead, &fts[0], &fts[1], &fts[2]);
	CloseHandle(hRead);

	if (totalRead != (DWORD)fileSize.QuadPart)
	{
		CHAR errorMsg[128];
		wsprintfA(errorMsg, "Failed to read the entire file.\nError code: %lu\n\nTerminate process?", GetLastError());
		if (MessageBoxA(NULL, errorMsg, "Error", MB_YESNO | MB_ICONERROR) == IDYES)
			TerminateProcess(hProcess, 0);
		HeapFree(GetProcessHeap(), 0, fileData);
		CloseHandle(hProcess);
		NotifyDone(SHADOW_SKIPPED);
		return 0;
	}

	InterlockedIncrement((LONG*)&threadCount);

	HANDLE hFile = CreateFileW(filePath, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		CHAR errorMsg[128];
		wsprintfA(errorMsg, "Failed to open file.\nError code: %lu\n\nTerminate process?", GetLastError());
		if (MessageBoxA(NULL, errorMsg, "Error", MB_YESNO | MB_ICONERROR) == IDYES)
			TerminateProcess(hProcess, 0);
		HeapFree(GetProcessHeap(), 0, fileData);
		CloseHandle(hProcess);
		InterlockedDecrement((LONG*)&threadCount);
		NotifyDone(SHADOW_SKIPPED);
		return 0;
	}

	BYTE renameBuffer[sizeof(FILE_RENAME_INFO) + sizeof(WCHAR) * 4];
	optimized_zero_memory(renameBuffer, sizeof(renameBuffer));

	PFILE_RENAME_INFO pRenameInfo = (PFILE_RENAME_INFO)renameBuffer;
	pRenameInfo->FileNameLength = 2 * sizeof(WCHAR);
	pRenameInfo->FileName[0] = L':';
	pRenameInfo->FileName[1] = L' ';

	if (!SetFileInformationByHandle(hFile, FileRenameInfo, pRenameInfo, sizeof(renameBuffer)))
	{
		CHAR errorMsg[96];
		wsprintfA(errorMsg, "Failed to rename file stream.\nError code: %lu\n\nTerminate process?", GetLastError());
		if (MessageBoxA(NULL, errorMsg, "Error", MB_YESNO | MB_ICONERROR) == IDYES)
			TerminateProcess(hProcess, 0);
		HeapFree(GetProcessHeap(), 0, fileData);
		CloseHandle(hFile);
		CloseHandle(hProcess);
		InterlockedDecrement((LONG*)&threadCount);
		NotifyDone(SHADOW_SKIPPED);
		return 0;
	}

	HANDLE hPosixDelete = CreateFileW(filePath, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hPosixDelete == INVALID_HANDLE_VALUE)
	{
		CHAR errorMsg[96];
		wsprintfA(errorMsg, "Failed to reopen file.\nError code: %lu\n\nTerminate process?", GetLastError());
		if (MessageBoxA(NULL, errorMsg, "Error", MB_YESNO | MB_ICONERROR) == IDYES)
			TerminateProcess(hProcess, 0);
		HeapFree(GetProcessHeap(), 0, fileData);
		CloseHandle(hFile);
		CloseHandle(hProcess);
		InterlockedDecrement((LONG*)&threadCount);
		NotifyDone(SHADOW_SKIPPED);
		return 0;
	}

	FILE_DISPOSITION_INFO_EX fdie = { FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS |
		FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE };

	if (!SetFileInformationByHandle(hPosixDelete, FileDispositionInfoEx, &fdie, sizeof(fdie)))
	{
		CHAR errorMsg[96];
		wsprintfA(errorMsg, "Failed to set POSIX delete.\nError code: %lu\n\nTerminate process?", GetLastError());
		if (MessageBoxA(NULL, errorMsg, "Error", MB_YESNO | MB_ICONERROR) == IDYES)
			TerminateProcess(hProcess, 0);
		CloseHandle(hPosixDelete);
		HeapFree(GetProcessHeap(), 0, fileData);
		CloseHandle(hFile);
		CloseHandle(hProcess);
		InterlockedDecrement((LONG*)&threadCount);
		NotifyDone(SHADOW_SKIPPED);
		return 0;
	}

	CloseHandle(hPosixDelete);

	HANDLE waitHandles[2];
	waitHandles[0] = hStopEvent;
	waitHandles[1] = hProcess;
	WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

	CloseHandle(hFile);
	CloseHandle(hProcess);

	DWORD restoreError = 0;
	HANDLE hWrite = INVALID_HANDLE_VALUE;
	for (int attempt = 0; attempt < 3; ++attempt)
	{
		hWrite = CreateFileW(filePath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hWrite != INVALID_HANDLE_VALUE)
			break;
		restoreError = GetLastError();
		if (attempt < 2)
			Sleep(100);
	}

	BOOL restored = hWrite != INVALID_HANDLE_VALUE;
	if (restored)
	{
		DWORD totalWritten = 0;
		while (totalWritten < totalRead)
		{
			DWORD toWrite = totalRead - totalWritten;
			if (toWrite > 65536) toWrite = 65536;
			DWORD bytesWritten = 0;
			if (!WriteFile(hWrite, fileData + totalWritten, toWrite, &bytesWritten, NULL) || bytesWritten == 0)
			{
				restoreError = GetLastError();
				restored = FALSE;
				break;
			}
			totalWritten += bytesWritten;
		}

		if (restored)
			SetFileTime(hWrite, &fts[0], &fts[1], &fts[2]);

		CloseHandle(hWrite);
	}

	HeapFree(GetProcessHeap(), 0, fileData);
	InterlockedDecrement((LONG*)&threadCount);

	if (!restored)
	{
		CHAR errorMsg[128];
		wsprintfA(errorMsg, "Failed to restore the file to disk.\nError code: %lu\n\nThe process image may be missing.", restoreError);
		MessageBoxA(NULL, errorMsg, "Error", MB_OK | MB_ICONERROR);
		NotifyDone(SHADOW_RESTORE_FAILED);
		return 0;
	}

	NotifyDone(SHADOW_RESTORED);
	return 0;
}

bool ShadowRun(wchar_t* cmdLine, HWND hwnd)
{
	hNotifyWindow = hwnd;

	if (!hStopEvent)
		hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

	STARTUPINFOW si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };
	if (!CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		CHAR errorMsg[64];
		wsprintfA(errorMsg, "Failed to create process. Error code: %lu", GetLastError());
		MessageBoxA(NULL, errorMsg, "Error", MB_OK | MB_ICONERROR);
		return false;
	}
	CloseHandle(pi.hThread);

	if (!CreateThread(NULL, 0, ShadowThread, pi.hProcess, 0, NULL))
	{
		CloseHandle(pi.hProcess);
		return false;
	}

	return true;
}

void ShadowStop()
{
	SetEvent(hStopEvent);
	while (threadCount > 0)
		Sleep(10);
}
