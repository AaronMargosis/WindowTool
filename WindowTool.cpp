// WindowTool.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <Psapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <io.h>
#include <fcntl.h>
#include <Shlwapi.h> // for StrStrIW
#pragma comment(lib, "Shlwapi.lib")

#include "StringUtils.h"
#include "FileOutput.h"
#include "SysErrorMessage.h"

/* 
TODO: List windows belonging to a PID
TODO: Add -out parameter
TODO: Show integrity level of the process
TODO: Add changing of x,y,h,w
TODO: Add options to minimize, maximize, restore windows
*/

/// <summary>
/// Write command-line syntax to stderr and then exit.
/// </summary>
/// <param name="argv0">The program's argv[0] value</param>
/// <param name="szError">Caller-supplied error text</param>
static void Usage(const wchar_t* argv0, const wchar_t* szError = nullptr)
{
    std::wstring sExe = GetFileNameFromFilePath(argv0);
    if (szError)
        std::wcerr << szError << std::endl << std::endl;
    std::wcerr
		<< sExe << L": multipurpose window management tool" << std::endl
        << std::endl
		<< L"Usage:" << std::endl
		<< std::endl
		<< L"    " << sExe << L" all [-v]" << std::endl
		<< L"    " << sExe << L" find <text> [-v]" << std::endl
		<< L"    " << sExe << L" process <exeName> [-v]" << std::endl
		//<< L"    " << sExe << L" pid <PID> [-v]" << std::endl
		<< L"    " << sExe << L" hwnd <hwnd> [-v]" << std::endl
		<< L"    " << sExe << L" children <hwnd> [-v]" << std::endl
		<< L"    " << sExe << L" settitle <hwnd> <title>" << std::endl
		<< L"    " << sExe << L" show <hwnd>" << std::endl
		<< L"    " << sExe << L" showall <hwnd>" << std::endl
		<< L"    " << sExe << L" hide <hwnd>" << std::endl
		<< L"    " << sExe << L" hideall <hwnd>" << std::endl
		<< L"    " << sExe << L" top <hwnd>" << std::endl
		<< L"    " << sExe << L" notop <hwnd>" << std::endl
		<< L"    " << sExe << L" click <hwnd>" << std::endl
		<< L"    " << sExe << L" close <hwnd>" << std::endl
		<< std::endl
		<< L"all      : list information about all top-level windows in the current desktop" << std::endl
		<< L"find     : list information about windows that contain the specified <text> in their titles (case-insensitive)" << std::endl
		<< L"process  : list information about windows owned by a process with an image name containing <exeName> (case-insensitive)" << std::endl
		//<< L"pid      : list information about windows owned by a process with PID equal to <PID>" << std::endl
		<< L"hwnd     : list information about the specified hwnd" << std::endl
		<< L"children : list information about child windows of the specified hwnd" << std::endl
		<< L"settitle : change the window title of hwnd to the new <title>" << std::endl
		<< L"show     : make the specified hwnd visible" << std::endl
		<< L"showall  : make the specified hwnd and its child windows visible" << std::endl
		<< L"hide     : make the specified hwnd hidden" << std::endl
		<< L"hideall  : make the specified hwnd and its child windows hidden" << std::endl
		<< L"top      : make the specified hwnd always-on-top" << std::endl
		<< L"notop    : remove the always-on-top attribute from the specified hwnd" << std::endl
		<< L"click    : simulate a mouse click on the specified hwnd" << std::endl
		<< L"close    : close the specified hwnd" << std::endl
		<< std::endl
		<< L"The \"-v\" option reports only visible windows." << std::endl
		<< std::endl
		<< std::endl;
    exit(-1);
}


// --------------------------------------------------------------------------------------------------------------
/// <summary>
/// Convert a string representation of a HWND value to a valid HWND.
/// </summary>
/// <param name="szHwnd">Input: string representation of a HWND (pointer) value</param>
/// <param name="hwnd">Output: HWND value</param>
/// <returns>true if input value is converted and is a valid window; false otherwise</returns>
static bool GetHWND(const wchar_t* szHwnd, HWND& hwnd)
{
	void* pv = NULL;
	if (1 != swscanf_s(szHwnd, L"%p", &pv) || !IsWindow((HWND)pv))
	{
		std::wcerr << L"Invalid window handle: " << szHwnd << std::endl;
		return false;
	}
	hwnd = (HWND)pv;
	return true;
}

// --------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------


/// <summary>
/// Global variable indicating whether only visible windows should be reported.
/// </summary>
static bool gb_bVisibleOnly = false;

/// <summary>
/// Global variable for redirecting stdout to a UTF-8 encoded file
/// TODO: implement the redirection.
/// </summary>
static std::wostream* pStream = &std::wcout;

/// <summary>
/// Tab delimiter
/// </summary>
static const wchar_t* szDelim = L"\t";

/// <summary>
/// Output tab-delimited headers
/// </summary>
static void OutputHeaders()
{
	*pStream
		<< L"HWND" << szDelim
		<< L"IsVisible" << szDelim
		<< L"Window class" << szDelim
		<< L"Window text" << szDelim
		<< L"PID" << szDelim
		<< L"Image name" << szDelim
		<< L"Coords (x,y;h,w)" << szDelim
		<< L"State"
		<< std::endl;
}

/// <summary>
/// Replace tab characters in the string with "[TAB]".
/// </summary>
/// <param name="sString">Input and output</param>
static void ReplaceTabs(std::wstring& sString)
{
	sString = replaceStringAll(sString, L"\t", L"[TAB]");
}

/// <summary>
/// Forward declaration for function to output tab-delimited information about the input HWND
/// </summary>
/// <param name="hwnd">Input HWND to report information about</param>
/// <param name="szImageNameMatch">Input: if not null, report windows only when process image name includes this value (case-insensitive)</param>
static void OutputWindowInfo(HWND hwnd, const wchar_t* szImageNameMatch = nullptr);

// --------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------

/// <summary>
/// Window enumeration: report about all windows
/// </summary>
static BOOL CALLBACK EnumAllWindowProc(
	_In_  HWND hwnd,
	_In_  LPARAM //lParam
)
{
	OutputWindowInfo(hwnd);
	return TRUE;
}

/// <summary>
/// Window enumeration: report windows that contain specified window text
/// </summary>
static BOOL CALLBACK FindTextWindowProc(
	_In_  HWND hwnd,
	_In_  LPARAM lParam
)
{
	// lParam is the string to search for in window text
	const wchar_t* szSearchFor = (const wchar_t*)lParam;
	const DWORD charCount = 2048;
	std::vector<wchar_t> vBuffer(charCount);
	LPWSTR pszBuffer = vBuffer.data();
	if (GetWindowTextW(hwnd, pszBuffer, charCount) > 0)
	{
		if (NULL != StrStrIW(pszBuffer, szSearchFor))
		{
			OutputWindowInfo(hwnd);
		}
	}
	return TRUE;
}

/// <summary>
/// Window enumeration: report windows that are owned by a process with a matching image name.
/// </summary>
/// <param name="hwnd"></param>
/// <param name="lParam"></param>
/// <returns></returns>
static BOOL CALLBACK FindProcessWindowsProc(
	_In_  HWND hwnd,
	_In_  LPARAM lParam
)
{
	// lParam is the full or partial process image name to match
	const wchar_t* szSearchFor = (const wchar_t*)lParam;
	OutputWindowInfo(hwnd, szSearchFor);
	return TRUE;
}

/// <summary>
/// Window enumeration: show or hide all child windows of the input hwnd
/// </summary>
static BOOL CALLBACK EnumChildProcShowWindow(
	_In_  HWND hwnd,
	_In_  LPARAM lParam
)
{
	ShowWindowAsync(hwnd, static_cast<int>(lParam));
	return TRUE;
}


int wmain(int argc, wchar_t** argv)
{
	//TODO: add command-line parameter to redirect output to a UTF-8 file.

	// Set output mode to UTF8.
	if (_setmode(_fileno(stdout), _O_U8TEXT) == -1 || _setmode(_fileno(stderr), _O_U8TEXT) == -1)
	{
		std::wcerr << L"Unable to set stdout and/or stderr modes to UTF8." << std::endl;
	}

	HWND hwndArg = 0;
	std::wstring sArg;
	// Available commands
	enum class eCmd_t { eCmd_Undefined, eCmd_All, eCmd_FindText, eCmd_Process, eCmd_Hwnd, eCmd_Children, eCmd_Settitle, eCmd_Show, eCmd_ShowAll, eCmd_Hide, eCmd_HideAll, eCmd_Top, eCmd_NoTop, eCmd_Click, eCmd_Close };
	eCmd_t cmd = eCmd_t::eCmd_Undefined;

	// Parse command line options
	int ixArg = 1;
	while (ixArg < argc)
	{
		if (0 == wcscmp(L"all", argv[ixArg]))
		{
			cmd = eCmd_t::eCmd_All;
		}
		else if (0 == wcscmp(L"find", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);

			sArg = argv[ixArg];

			cmd = eCmd_t::eCmd_FindText;
		}
		else if (0 == wcscmp(L"process", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);

			sArg = argv[ixArg];

			cmd = eCmd_t::eCmd_Process;
		}
		//else if (0 == wcscmp(L"pid", argv[ixArg]))
		//{
		//	if (++ixArg >= argc)
		//		Usage(argv[0]);

		//	//TODO: parse argv[ixArg] as a PID
		//	//TODO: set cmd as a command to search by PID
		//}
		else if (0 == wcscmp(L"hwnd", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);

			if (!GetHWND(argv[ixArg], hwndArg))
				Usage(argv[0]);

			cmd = eCmd_t::eCmd_Hwnd;
		}
		else if (0 == wcscmp(L"children", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);

			if (!GetHWND(argv[ixArg], hwndArg))
				Usage(argv[0]);

			cmd = eCmd_t::eCmd_Children;
		}
		else if (0 == wcscmp(L"settitle", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);
			if (!GetHWND(argv[ixArg], hwndArg))
				Usage(argv[0]);
			if (++ixArg >= argc)
				Usage(argv[0]);
			sArg = argv[ixArg];

			cmd = eCmd_t::eCmd_Settitle;
		}
		else if (0 == wcscmp(L"show", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);

			if (!GetHWND(argv[ixArg], hwndArg))
				Usage(argv[0]);

			cmd = eCmd_t::eCmd_Show;
		}
		else if (0 == wcscmp(L"showall", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);

			if (!GetHWND(argv[ixArg], hwndArg))
				Usage(argv[0]);

			cmd = eCmd_t::eCmd_ShowAll;
		}
		else if (0 == wcscmp(L"hide", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);

			if (!GetHWND(argv[ixArg], hwndArg))
				Usage(argv[0]);

			cmd = eCmd_t::eCmd_Hide;
		}
		else if (0 == wcscmp(L"hideall", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);

			if (!GetHWND(argv[ixArg], hwndArg))
				Usage(argv[0]);

			cmd = eCmd_t::eCmd_HideAll;
		}
		else if (0 == wcscmp(L"top", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);

			if (!GetHWND(argv[ixArg], hwndArg))
				Usage(argv[0]);

			cmd = eCmd_t::eCmd_Top;
		}
		else if (0 == wcscmp(L"notop", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);

			if (!GetHWND(argv[ixArg], hwndArg))
				Usage(argv[0]);

			cmd = eCmd_t::eCmd_NoTop;
			}
		else if (0 == wcscmp(L"click", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);

			if (!GetHWND(argv[ixArg], hwndArg))
				Usage(argv[0]);

			cmd = eCmd_t::eCmd_Click;
		}
		else if (0 == wcscmp(L"close", argv[ixArg]))
		{
			if (++ixArg >= argc)
				Usage(argv[0]);

			if (!GetHWND(argv[ixArg], hwndArg))
				Usage(argv[0]);

			cmd = eCmd_t::eCmd_Close;
		}
		else if (0 == wcscmp(L"-v", argv[ixArg]))
		{
			gb_bVisibleOnly = true;
		}
		else
		{
			Usage(argv[0], L"Unrecognized command line option");
		}
		++ixArg;
	}

	switch (cmd)
	{
	case eCmd_t::eCmd_All:
		OutputHeaders();
		EnumWindows(EnumAllWindowProc, 0);
		break;

	case eCmd_t::eCmd_FindText:
		OutputHeaders();
		EnumWindows(FindTextWindowProc, (LPARAM)sArg.c_str());
		break;

	case eCmd_t::eCmd_Process:
		OutputHeaders();
		EnumWindows(FindProcessWindowsProc, (LPARAM)sArg.c_str());
		break;

	case eCmd_t::eCmd_Hwnd:
		OutputHeaders();
		OutputWindowInfo(hwndArg);
		break;

	case eCmd_t::eCmd_Children:
		OutputHeaders();
		EnumChildWindows(hwndArg, EnumAllWindowProc, 0);
		break;

	case eCmd_t::eCmd_Settitle:
		if (!SetWindowTextW(hwndArg, sArg.c_str()))
		{
			std::wcerr << SysErrorMessageWithCode() << std::endl;
		}
		break;

	case eCmd_t::eCmd_Show:
		if (!ShowWindowAsync(hwndArg, SW_SHOW))
		{
			std::wcerr << SysErrorMessageWithCode() << std::endl;
		}
		break;

	case eCmd_t::eCmd_ShowAll:
		if (!ShowWindowAsync(hwndArg, SW_SHOW))
		{
			std::wcerr << SysErrorMessageWithCode() << std::endl;
		}
		EnumChildWindows(hwndArg, EnumChildProcShowWindow, SW_SHOW);
		break;

	case eCmd_t::eCmd_Hide:
		if (!ShowWindowAsync(hwndArg, SW_HIDE))
		{
			std::wcerr << SysErrorMessageWithCode() << std::endl;
		}
		break;

	case eCmd_t::eCmd_HideAll:
		if (!ShowWindowAsync(hwndArg, SW_HIDE))
		{
			std::wcerr << SysErrorMessageWithCode() << std::endl;
		}
		EnumChildWindows(hwndArg, EnumChildProcShowWindow, SW_HIDE);
		break;

	case eCmd_t::eCmd_Top:
		if (!SetWindowPos(hwndArg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE))
		{
			std::wcerr << SysErrorMessageWithCode() << std::endl;
		}
		break;

	case eCmd_t::eCmd_NoTop:
		if (!SetWindowPos(hwndArg, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE))
		{
			std::wcerr << SysErrorMessageWithCode() << std::endl;
		}
		break;

	case eCmd_t::eCmd_Click:
		if (!SendMessageTimeoutW(hwndArg, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5), 0, 5000, NULL) || 
			!SendMessageTimeoutW(hwndArg, WM_LBUTTONUP, 0, MAKELPARAM(5, 5), 0, 5000, NULL))
		{
			std::wcerr << SysErrorMessageWithCode() << std::endl;
		}
		break;

	case eCmd_t::eCmd_Close:
		if (!SendMessageTimeoutW(hwndArg, WM_SYSCOMMAND, SC_CLOSE, 0, 0, 5000, NULL))
		{
			std::wcerr << SysErrorMessageWithCode() << std::endl;
		}
		break;

	default:
		Usage(argv[0]);
		break;
	}

	return 0;
}

/// <summary>
/// Output tab-delimited information about the input HWND
/// </summary>
/// <param name="hwnd">Input HWND to report information about</param>
/// <param name="szImageNameMatch">Input: if not null, report windows only when process image name includes this value (case-insensitive)</param>
static void OutputWindowInfo(HWND hwnd, const wchar_t* szImageNameMatch /*= nullptr*/)
{
	if (IsWindow(hwnd))
	{
		bool bDoOutput = true;

		BOOL bIsVisible = IsWindowVisible(hwnd);

		if (bIsVisible || !gb_bVisibleOnly)
		{
			// PID and TID
			DWORD dwPID = 0, dwTID = 0;
			dwTID = GetWindowThreadProcessId(hwnd, &dwPID);

			std::wstring sTitle, sWindowClass, sImageName, sError, sRect, sState;
			RECT rect = { 0 };
			WINDOWPLACEMENT placement = { 0 };
			placement.length = sizeof(WINDOWPLACEMENT);
			const DWORD charCount = 2048;
			std::vector<wchar_t> vBuffer(charCount);
			LPWSTR pszBuffer = vBuffer.data();

			// Inspect process here, early, to avoid doing work if bDoOutput will be set to false.
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwPID);
			if (hProcess)
			{
				if (0 != K32GetModuleFileNameExW(hProcess, nullptr, pszBuffer, charCount))
				{
					sImageName = pszBuffer;
					// Filter on specific process names
					if (szImageNameMatch)
					{
						std::wstring sExeName = GetFileNameFromFilePath(sImageName);
						if (NULL == StrStrIW(sExeName.c_str(), szImageNameMatch))
						{
							bDoOutput = false;
						}
					}
				}
				CloseHandle(hProcess);
			}
			else
			{
				DWORD dwLastError = GetLastError();
				sImageName = SysErrorMessage(dwLastError);
				// If there's a process-name match and we can't query the process, don't output
				if (szImageNameMatch)
				{
					bDoOutput = false;
				}
			}

			// If still outputting info, get the rest of the info and report it
			if (bDoOutput)
			{
				// Window title
				if (GetWindowTextW(hwnd, pszBuffer, charCount))
				{
					sTitle = pszBuffer;
					ReplaceTabs(sTitle);
				}

				if (GetClassNameW(hwnd, pszBuffer, charCount))
				{
					sWindowClass = pszBuffer;
					ReplaceTabs(sWindowClass);
				}

				if (bIsVisible)
				{
					if (GetWindowRect(hwnd, &rect))
					{
						std::wstringstream strRect;
						strRect << rect.top << L", " << rect.left << L"; " << (rect.bottom - rect.top) << L", " << (rect.right - rect.left);
						sRect = strRect.str();
					}
					else
					{
						sRect = SysErrorMessageWithCode();
					}

					if (GetWindowPlacement(hwnd, &placement))
					{
						switch (placement.showCmd)
						{
						case SW_NORMAL:
						case SW_RESTORE:
						case SW_SHOWNOACTIVATE:
							sState = L"Normal";
							break;
						case SW_SHOWMINIMIZED:
						case SW_MINIMIZE:
						case SW_SHOWMINNOACTIVE:
						case SW_FORCEMINIMIZE:
							sState = L"Minimized";
							break;
						case SW_MAXIMIZE:
							sState = L"Maximized";
							break;
						case SW_HIDE:
							sState = L"SW_HIDE";
							break;
						case SW_SHOW:
							sState = L"SW_SHOW";
							break;
						case SW_SHOWNA:
							sState = L"SW_SHOWNA";
							break;
						case SW_SHOWDEFAULT:
							sState = L"SW_SHOWDEFAULT";
							break;
						default:
							sState = L"[unrecognized]";
							break;
						}
					}
					else
					{
						sState = SysErrorMessageWithCode();
					}
				}

				*pStream
					<< hwnd << szDelim
					<< (bIsVisible ? L"visible" : L"hidden") << szDelim
					<< sWindowClass << szDelim
					<< sTitle << szDelim
					<< dwPID << szDelim
					<< sImageName << szDelim
					<< sRect << szDelim
					<< sState
					<< std::endl;
			}
		}
	}
	else
	{
		*pStream
			<< hwnd << szDelim
			<< L"[invalid]" << szDelim
			<< szDelim
			<< szDelim
			<< szDelim
			<< szDelim
			<< szDelim
			<< std::endl;
	}
}

