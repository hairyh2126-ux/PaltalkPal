/*********************************/
/* (c) 2015 LaszloSoft Solutions */
/*********************************/
#pragma once

// Including SDKDDKVer.h defines the highest available Windows platform.

// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.

#define _WIN32_WINNT _WIN32_WINNT_VISTA
#include <SDKDDKVer.h>

#include <windows.h>
#include <CommCtrl.h>
#include <stdio.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#define  _RICHEDIT_VER 0x0500
#include <Richedit.h>
#include <string>
#include <sstream>
#include <fstream>

// UIAutomation
#include <UIAutomation.h>
#include <ole2.h>
#include <atlbase.h> // For CComPtr
#include <cctype> // For std::iswlower and std::iswupper
#include <iterator> // This include for char std::begin and std::end
#include <cwctype> //  This include for wchar std::iswlower and std::iswupper
#include "SpellcheckUtilities.h"

// Resource Header
#include "resource2.h"

#pragma comment(lib,"comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// TODO: Change these to the App name 
char szAppName[] = "Platalk Text Input";
wchar_t wcAppName[] = L"Platalk Text Input";

#define msgba(h,x) MessageBoxA(h,x,szAppName,MB_OK)
#define msgbw(h,x) MessageBoxW(h,x,wcAppName,MB_OK)

// Context Menu IDs
#define IDM_SAVELIST	50001
#define IDM_CLEARLIST	50002
#define IDM_DELSELLIST	50003
#define IDM_LOADFILE	50004
#define IDM_CLIPBRD		50005
#define IDM_CLIP2RICHE	50006 

// Timer IDs
#define IDT_TIMERREAD	50011
#define IDT_TIMERANNC	50012

// Pop up menu items
#define IDM_PUSH  5001
#define IDM_PASTE 5002
#define IDM_COPY  5003
#define IDM_LOOK  5004
#define IDM_SPELL 5005
#define IDC_TOGGLE_SEND_BOLD 5006	