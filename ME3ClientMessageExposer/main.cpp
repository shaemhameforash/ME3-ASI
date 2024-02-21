#include "header.h"
#include <windows.h>
#include "resource.h"
#include <Richedit.h>
#include <string.h>
#include <fstream>
#include <ctime>
#include <sys/stat.h>
#include <direct.h>
#include <Windows.h> // Added
#include <VersionHelpers.h> // Added
#include <ShellScalingAPI.h> /

std::ofstream logFile;

struct LogWindowStruct {
	HWND hWindow;
	HWND hRichEdit;
} logWindow;

HINSTANCE hDLL;

#define LOC_START 0x452B01
#define LOC_EXIT 0x452B09

void * pointer;
int stringHeader;
int var1;

struct MsgStruct
{
	wchar_t * str;
	int size;
} *message;

void LogAppendText(LPCWSTR text)
{
	// Get current date and time
	time_t now = time(0);
	struct tm timeinfo;
	localtime_s(&timeinfo, &now);
	char buffer[80];
	strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S] ", &timeinfo);

	// Convert wide character string to narrow string
	int bufferSize = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
	if (bufferSize == 0)
	{
		// Handle error
		return;
	}

	//There is CERTAINLY a better way to perform the following operations more efficiently.
	//However this works, and I have close to nill experience with C++.
	//The following was mostly generated with the help of AI.
	std::string narrowText(bufferSize, '\0');
	WideCharToMultiByte(CP_UTF8, 0, text, -1, &narrowText[0], bufferSize, nullptr, nullptr);
	narrowText.pop_back(); // Remove null terminator

	//Concatenate Date Time with Narrow Text.
	narrowText = buffer + narrowText;

	//Convert the Date Time Narrow Text to Wide String.
	std::wstring wideString(narrowText.begin(), narrowText.end());

	//Convert it back again to LPCWSTR.
	LPCWSTR wideStringConverted = wideString.c_str();

	int currentlen = GetWindowTextLength(logWindow.hRichEdit);
	SendMessage(logWindow.hRichEdit, EM_SETSEL, (WPARAM)currentlen, (LPARAM)currentlen);
	SendMessage(logWindow.hRichEdit, EM_REPLACESEL, 0, (LPARAM)wideStringConverted);
	SendMessage(logWindow.hRichEdit, EM_REPLACESEL, 0, (LPARAM)L"\n");
	SendMessage(logWindow.hRichEdit, WM_VSCROLL, SB_BOTTOM, 0);

	// Write the text to the file
	if (logFile.is_open())
	{
		logFile << narrowText << std::endl;
	}
}

bool GetLocation(int * p)
{
	int intLoc = *(int*)0x01AB5634;
	intLoc = *(int*)(intLoc + 7747 * 4);
	//printf("intLoc: %p\n", (void*)intLoc);
	*p = intLoc;
	return true;
}

__declspec(naked) void ExposeMessageFunc()
{
	__asm
	{
		mov pointer,esi
		mov stringHeader,esp
		pushad
	}

	if(GetLocation(&var1) && pointer == (void*)var1)
	{
		//printf("pointer: %p ; var1: %p\n", pointer, (void*)var1);
		message = (MsgStruct*)stringHeader;
		LogAppendText(message->str);
	}


	__asm
	{
		popad
		mov eax,[esi]
		mov edx,[eax+0x000001A0]
		mov eax, LOC_EXIT
		jmp eax
	}
}

// Function to handle window resizing
void ResizeRichEditControl(HWND hwnd)
{
	// Retrieve the handle to the rich edit control
	HWND hRichEdit = GetDlgItem(hwnd, IDC_RICHEDIT21);

	// Get the client area rectangle of the dialog box
	RECT rcClient;
	GetClientRect(hwnd, &rcClient);

	// Resize the rich edit control to fit the client area
	SetWindowPos(hRichEdit, NULL, 0, 0, rcClient.right, rcClient.bottom, SWP_NOZORDER);
}


BOOL CALLBACK LogWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		// Initialize the dialog
		return TRUE;

	case WM_MOVING:
		return TRUE;

	case WM_SIZE:
		// Handle window resizing
		ResizeRichEditControl(hwnd);
		return TRUE;

	case WM_COMMAND:
		// Handle commands
		switch (LOWORD(wParam))
		{
		case IDOK:
		case IDCANCEL:
			// Close the dialog
			EndDialog(hwnd, LOWORD(wParam));
			return TRUE;
		}
		break;
	}

	return FALSE;
}

static bool DirectoryExists(const std::string& directoryName)
{
	struct stat info;
	if (stat(directoryName.c_str(), &info) != 0)
		return false;
	else if (info.st_mode & S_IFDIR) // S_ISDIR() doesn't exist on my windows
		return true;
	else
		return false;
}

DWORD WINAPI LogWindowThread(LPVOID lpParam)
{
	// Check if the client_logs folder exists
	if (!DirectoryExists("client_logs"))
	{
		// Attempt to create the client_logs folder
		if (_mkdir("client_logs") != 0)
		{
			// Handle error creating directory
			MessageBox(NULL, L"Error creating client_logs directory!", NULL, MB_OK | MB_ICONERROR);
			return 1;
		}
	}
	// Get current date
	time_t now = time(0);
	struct tm timeinfo;
	localtime_s(&timeinfo, &now);
	char buffer[80];
	strftime(buffer, sizeof(buffer), "client_log-%d-%m-%Y.txt", &timeinfo);
	std::string logFileName(buffer);

	// Open the log file in append mode
	logFile.open("client_logs\\"+ logFileName, std::ios::app);
	if (!logFile.is_open())
	{
		// Handle error opening file
		MessageBox(NULL, L"Error opening log file!", NULL, MB_OK | MB_ICONERROR);
		return 1;
	}

	LoadLibrary(L"riched20.dll");
	MSG msg;
	logWindow.hWindow = CreateDialog(hDLL, MAKEINTRESOURCE(IDD_DIALOG1), 0, LogWindowProc);
	SetWindowLongPtr(logWindow.hWindow, GWL_STYLE, GetWindowLongPtr(logWindow.hWindow, GWL_STYLE) | WS_THICKFRAME | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
	logWindow.hRichEdit = GetDlgItem(logWindow.hWindow, IDC_RICHEDIT21);
	CHARFORMAT cf;
	cf.cbSize = sizeof(CHARFORMAT);
	cf.dwMask = CFM_FACE | CFM_SIZE;
	cf.yHeight = 200;
	wcscpy_s(cf.szFaceName, sizeof(cf.szFaceName) / sizeof(WCHAR), L"Consolas");
	SendMessage(logWindow.hRichEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
	ShowWindow(logWindow.hWindow, SW_NORMAL);
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(logWindow.hWindow, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return 1;
}

DWORD WINAPI Start(LPVOID lpParam)
{
	DWORD hold = NULL;

	VirtualProtect((void*)LOC_START, 8, PAGE_EXECUTE_READWRITE, &hold);
	*(BYTE*)(LOC_START) = 0xE9;
	*(DWORD*)(LOC_START+1) = (unsigned long)&ExposeMessageFunc - (LOC_START + 5);
	*(BYTE*)(LOC_START+5) = 0x90;
	*(BYTE*)(LOC_START+6) = 0x90;
	*(BYTE*)(LOC_START+7) = 0x90;
	VirtualProtect((void*)LOC_START, 8, hold, NULL);

	CreateThread(0, NULL, LogWindowThread, NULL, NULL, NULL);

	return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_DETACH)
	{
		// Close the log file
		if (logFile.is_open())
		{
			logFile.close();
		}
	}
	
	hDLL = hinstDLL;
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		DWORD dwThreadId, dwThrdParam = 1;
		HANDLE hThread;
		hThread = CreateThread(NULL, 0, Start, &dwThrdParam, 0, &dwThreadId);
	}
	return 1;
}


