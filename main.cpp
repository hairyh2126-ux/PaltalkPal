/*********************************/
/* (c) 2024 Hairy Soft Solutions */
/*********************************/
#include "main.h"
#include <algorithm>
using namespace std;

// Global Variables
HINSTANCE	hInst = 0;
HWND		ghMain = 0;
HWND		ghEdit = 0;
HWND		ghList = 0;
HWND		ghRichEdit = 0;
HWND		ghListNicks = 0;

// Paltalk Windows Handles
HWND ghPtMain = NULL;
HWND ghPtRoom = NULL;
HWND ghPtLv = NULL;


// Debug related globals
#ifdef DEBUG
char gszDebugMsg[MAX_PATH] = { '\0' };
wchar_t gwcDebugMsg[MAX_PATH] = { '\0' };
#endif // DEBUG

// UIAutomation related globals
CComPtr<IUIAutomationElement> emojiTextEditElement;
CComPtr<IUIAutomationElement> automationElementRoom;
CComPtr<IUIAutomation> g_pUIAutomation;

//Quick Messagebox macro 
#define msga(x) msgba(ghMain,x)
// Quick Messagebox WIDE String 
#define msgw(x) msgbw(ghMain,x)

// Hunspell Stuff
HINSTANCE hinsDll;
typedef struct Hunhandle Hunhandle;
typedef Hunhandle* (*typHunSpCreate) (char*, char*);
typedef void(*typHunSpDestroy)(Hunhandle*);
typedef int(*typHunSpSpell)(Hunhandle*, char*);
typedef int(*typHunSpSuggest)(Hunhandle*, char***, char*);
typedef void(*typHunSpFreeList)(Hunhandle*, char***, int);
typedef int(*typHunSpAdd)(Hunhandle*, char*);

typHunSpCreate pDllHunspellCreate;
typHunSpDestroy pDllHunspellDestroy;
typHunSpSpell pDllHunspellSpell;
typHunSpSuggest pDllHunspellSuggest;
typHunSpFreeList pDllHunspellFreeList;
typHunSpAdd pDllHunspellAdd;

Hunhandle* hSpell = NULL;
char szAffUS[] = "en_US.aff";
char szDicUS[] = "en_US.dic";
char szAffGB[] = "en_GB.aff";
char szDicGB[] = "en_GB.dic";
char szAff[MAX_PATH] = { 0 };
char szDic[MAX_PATH] = { 0 };

// Global for Hungspell 
BOOL gbDoSpell = TRUE;
BOOL gbPushPt = TRUE;
bool gbHunspellInited = false;
// Global for Clipboard 
BOOL gbClipActive = FALSE;
BOOL gbClip2Richedit = FALSE;
// global for Send Text to Paltalk
BOOL gbSendBold = FALSE;
// Font handles
HFONT ghFntRichEdit = NULL;

// Function prototypes
BOOL CALLBACK DlgMain(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL InitPaltalkWindows(void);
BOOL CALLBACK EnumPaltalkWindows(HWND hWnd, LPARAM lParam);
BOOL InitRichEdit(void);
BOOL OnSendButtonClick(void);
void SaveListToFile(HWND hwList);
BOOL SetFileNamePath(wchar_t* wsFileName, DWORD dwFlNmSize, BOOL bWrite);
BOOL LoadFileToList(HWND hwList);
static void TrimWhitespace(std::wstring& str);
void OnClipboardUpdate(void);
void OnClipboardMenu(void);
static bool FileExistsA(const char* path);

// UIAutomation and Dot Mic User related functions
HRESULT __stdcall InitUIAutomation(void);
HRESULT __stdcall UninitUIAutomation(void);
HRESULT __stdcall GetUIAutomationElementFromHWNDAndClassName(HWND hwnd, const wchar_t* className, IUIAutomationElement** foundElement);
HRESULT __stdcall FindWindowByTitle(const std::wstring& title, IUIAutomationElement** outElement);

// Send Text to Paltalk Out window
BOOL SendListItemTextToPaltalk(void);
BOOL AddTextToList(wchar_t* wcText, int iLL);
void CreateContextMenu(WPARAM wParam, LPARAM lparam);
void SendMessageToPaltalk(wchar_t* szMsg);
wstring ConvertToBold(const wstring& inString);
BOOL GetNicknames(void);
BOOL SendNick2Richedit(void);

// Hunspell functions
BOOL InitHunspell(void);
bool SpellCheckAndSuggest(wchar_t* wcRawWord);
BOOL LookupWebDictionary(void);
//BOOL AppendWordToPersonalDic(const wchar_t* wword);
void AppendToPersonalDic(const std::wstring& word);
std::wstring GetExeDirectory(void);
std::wstring GetPersonalDicPath(void);
std::vector<std::string> LoadPersonalDictionary(const std::wstring& file);
std::string WideToUtf8(const std::wstring& w);
std::wstring Utf8ToWide(const std::string& s);


LRESULT CALLBACK EditLoadSubClassProc(HWND hWnd, UINT msg, WPARAM wParam,
	LPARAM lParam, UINT_PTR uIdSubClass,
	DWORD_PTR dwRefData);

// Application Entry 
int APIENTRY WinMain(_In_ HINSTANCE hInstance,_In_opt_ HINSTANCE hPrevInstance,_In_ LPSTR lpCmdLine,_In_ int nShowCmd)
{
	hInst = hInstance;
	InitCommonControls();
	// Parsing command line arguments for dictionary selection
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	if(argc == 2)
	{
		std::wstring wsArg = argv[1];
		if (wsArg == L"/us" || wsArg == L"-us")
		{
			// US English
			strcpy_s(szAff, sizeof(szAff), szAffUS);
			strcpy_s(szDic, sizeof(szDic), szDicUS);
		}
		else if (wsArg == L"/gb" || wsArg == L"-gb")
		{
			// GB English
			strcpy_s(szAff, sizeof(szAff), szAffGB);
			strcpy_s(szDic, sizeof(szDic), szDicGB);
		}
		else
		{
			// Default to GB English
			strcpy_s(szAff, sizeof(szAff), szAffGB);
			strcpy_s(szDic, sizeof(szDic), szDicGB);
		}
	}
	else
	{
		// Default to GB English
		strcpy_s(szAff, sizeof(szAff), szAffGB);
		strcpy_s(szDic, sizeof(szDic), szDicGB);
	}

	// TODO: Add any initializations as needed 
	LoadLibrary(L"Msftedit.dll"); // comment if richedit is not used 
	OutputDebugStringA("[COM] Attempting to initialize COM...\n");
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(hr)) {
		char szMsg[128];
		sprintf_s(szMsg, sizeof(szMsg), "[COM] CoInitializeEx FAILED! HRESULT=0x%08X\n", hr);
		OutputDebugStringA(szMsg);
		msga("CoInitializeEx Failed!");
		return 2;
	}
	else {
		OutputDebugStringA("[COM] COM initialized successfully (STA mode).\n");
	}
	
	return DialogBox(hInst, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)DlgMain);
}

//
// Callback main message loop for the Application
//
BOOL CALLBACK DlgMain(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		ghMain = hwndDlg;
		ghRichEdit = GetDlgItem(hwndDlg, IDC_RICHEDIT2_INPUT);
		ghList = GetDlgItem(hwndDlg, IDC_LIST_HISTORY);
		ghListNicks = GetDlgItem(hwndDlg, IDC_LIST_NICKS);

		SetWindowSubclass(ghRichEdit, EditLoadSubClassProc, 0, 0);
		if (!InitRichEdit()) msga("InitRichEdit failed!");
		if (!InitHunspell())
		{
			gbDoSpell = false;
			gbHunspellInited = false;
			MessageBoxA(ghMain, "Hungspell Engin Fail, No Spelling!", "Hunspell Module", MB_OK);
		}
		else
		{
			gbHunspellInited = true;
			//MessageBoxA(ghMain, "Hungspell Engin Initialized!", "Hunspell Module", MB_OK);
		}
		// Setting up event mask for rich edit control
		SendMessage(ghRichEdit, EM_SETEVENTMASK, 0,
			ENM_UPDATE | ENM_CHANGE | ENM_KEYEVENTS);

	}
	break; // return TRUE; 
	case WM_CONTEXTMENU:
	{
		CreateContextMenu(wParam, lParam);
	}
	return TRUE;
	case WM_CLIPBOARDUPDATE:
	{
		OnClipboardUpdate();
	}
	return TRUE;
	case WM_CLOSE:
	{
		DeleteObject(ghFntRichEdit);
		UninitUIAutomation();
		EndDialog(hwndDlg, 0);
	}
	return TRUE;
	case WM_NOTIFY:
	{
		NMHDR* hdr = (NMHDR*)lParam;

		if (hdr->hwndFrom == ghRichEdit && hdr->code == EN_MSGFILTER)
		{
			MSGFILTER* mf = (MSGFILTER*)lParam;

			if (gbHunspellInited  &&  gbDoSpell && mf->msg == WM_CHAR )
			{
				wchar_t ch = (wchar_t)mf->wParam;

				if (ch == L' ' || ch == L'\t' || ch == L'\n' ||
					wcschr(L",!?;:()[]\"", ch))
				{
					RetrieveLastWord(ghRichEdit);
				}
			}
		}
	}
	break;
	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDCANCEL:
		{
			UninitUIAutomation();
			EndDialog(hwndDlg, 0);
		}
		return TRUE;
		case IDC_BUTTON_NICKS:
		{
			if(!GetNicknames())
				msga("No Paltalk Room, Get Paltalk and try again");
		}
		return TRUE;
		case IDC_BUTTON_SEND:
		{
				if (!OnSendButtonClick())
				{
					msga("Error OnSendButtonClick() BTADDLIST FAIL!");
				}						
		}
		return TRUE;
		case IDC_GETPT:
		{
			if (!InitPaltalkWindows())
			 msga("Paltalk Windows Capture Fails!");
		}
		return TRUE;
		case IDM_SAVELIST:
		{
			SaveListToFile(ghList);
		}
		return TRUE;
		case IDM_CLEARLIST:
		{
			SendMessageW(ghList, LB_RESETCONTENT, 0, 0);
		}
		return TRUE;
		case IDM_DELSELLIST:
		{
			LRESULT lrIndx = SendMessageW(ghList, LB_GETCURSEL, 0, 0);
			if (lrIndx != LB_ERR)
				SendMessageW(ghList, LB_DELETESTRING, (WPARAM)lrIndx, 0);
		}
		return TRUE;
		case IDM_CLIP2RICHE:
		{
			if (gbClip2Richedit) gbClip2Richedit = false;
			else gbClip2Richedit = true;
		}
		return TRUE;
		case IDM_PUSH:
		{
			if (gbPushPt) gbPushPt = false;
			else gbPushPt = true;
		}
		return TRUE;
		case IDM_PASTE:
		{
			SendMessageW(ghRichEdit, WM_PASTE, 0, 0);
			SetFocus(ghRichEdit);
		}
		return TRUE;
		case IDM_COPY:
		{
			SendMessageW(ghRichEdit, WM_COPY, 0, 0);
			SetFocus(ghRichEdit);
		}
		return TRUE;
		case IDM_LOOK:
		{
			if (!LookupWebDictionary()) msgba(ghMain, "Too Long Word\nWeb Lookup Failed!");
		}
		return TRUE;
		case IDM_SPELL:
		{
			if (gbDoSpell) gbDoSpell = false;
			else gbDoSpell = true;
		}
		return TRUE;
		case IDC_TOGGLE_SEND_BOLD:
		{
			if (gbSendBold) gbSendBold = false;
			else gbSendBold = true;
		}
		return TRUE;
		case IDC_LIST_HISTORY:
		{
			if (HIWORD(wParam) == LBN_DBLCLK)
				if (!SendListItemTextToPaltalk())
					msga("Error SendListItemTextToPaltalk() Fail!");
		}
		return TRUE;
		case IDC_LIST_NICKS:
		{
			if (HIWORD(wParam) == LBN_DBLCLK)
				if (!SendNick2Richedit())
					msga("Error Send Nick to Richedit Fail!");
		}
		return TRUE;


		case IDM_LOADFILE:
		{
			if (!LoadFileToList(ghList))
			 msga("Error Loading File!");
		}
		return TRUE;
		case IDM_CLIPBRD:
			OnClipboardMenu();
			return TRUE;
		}
	}

	default:
		return FALSE;
	}
	return FALSE;
}

// Initialise the Paltalk Windows handles
BOOL InitPaltalkWindows(void)
{
	char szTitle[256] = { 0 };
	char szTemp[512] = { 0 };
	// Resetting handle
	ghPtMain = 0;
	ghPtRoom = 0;
		
	ghPtRoom = FindWindowA("DlgGroupChat Window Class", 0);

	if (GetWindowTextA(ghPtRoom, szTitle, 254) < 1)
	{
		return FALSE;
	}
	else
	{
		wsprintfA(szTemp, "Paltalk Room - %s", szTitle);
		SetWindowTextA(ghMain, szTemp);
	}

	ghPtMain = FindWindowA("Qt5150QWindowIcon",szTitle);

	if (!ghPtMain) return FALSE;

	// Cleaning up previous UIAutomation elements
	UninitUIAutomation();

	// Initialise UIAutomation
	if (FAILED(InitUIAutomation())) {
		msga("Initializing UI Automation failed!");
		return FALSE;
	}
		
	SetWindowPos(ghPtMain, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	// Getting the Emoji Text Edit control UIAutomation element to send text to Paltalk
	HRESULT hr = GetUIAutomationElementFromHWNDAndClassName(ghPtMain, L"ui::controls::EmojiTextEdit", &emojiTextEditElement);
	if (FAILED(hr)) {
		OutputDebugStringA( "GetUIAutomationElementFromHWNDAndClassName failed: " );
	}

	// Finding the chat room window controls handles
	EnumChildWindows(ghPtRoom, EnumPaltalkWindows, 0);

	return TRUE;
}

/// Enumeration Callback to Find the Control Windows
BOOL CALLBACK EnumPaltalkWindows(HWND hWnd, LPARAM lParam)
{
	char szListViewClass[] = "SysHeader32";
	char szMsg[MAX_PATH] = { 0 };
	char szClassNameBuffer[MAX_PATH] = { 0 };

	GetClassNameA(hWnd, szClassNameBuffer, MAX_PATH);

	if (strcmp(szListViewClass, szClassNameBuffer) == 0)
	{
		ghPtLv = hWnd;

		sprintf_s(szMsg, "List View window handle: %p \n", ghPtLv);
		OutputDebugStringA(szMsg);

		return FALSE;
	}

	return TRUE;
}

/// Initialise the Clock Display Window
BOOL InitRichEdit(void)
{
	HDC hDC;
	int nHeight = 0;

	hDC = GetDC(ghMain);
	nHeight = -MulDiv(12, GetDeviceCaps(hDC, LOGPIXELSY), 72);
	ghFntRichEdit = CreateFont(nHeight, 0, 0, 0, FW_REGULAR, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Microsoft Sans Serif"));
	SendMessageA(ghRichEdit, WM_SETFONT, (WPARAM)ghFntRichEdit, (LPARAM)TRUE);
	SendMessageA(ghRichEdit, WM_SETTEXT, (WPARAM)0, (LPARAM)"");

	return TRUE;
}


// Context Menu for the List Box
void CreateContextMenu(WPARAM wParam, LPARAM lparam)
{
	if ((HWND)wParam == ghList)
	{
		HMENU hMenu = CreatePopupMenu();
		if (gbClipActive)
			InsertMenuW(hMenu, 0, MF_BYCOMMAND | MF_STRING | MF_ENABLED | MF_CHECKED, IDM_CLIPBRD, L"Clipboard Active");
		else
			InsertMenuW(hMenu, 0, MF_BYCOMMAND | MF_STRING | MF_ENABLED | MF_UNCHECKED, IDM_CLIPBRD, L"Clipboard Active");
		if (gbClip2Richedit)
			InsertMenuW(hMenu, 1, MF_BYCOMMAND | MF_STRING | MF_ENABLED | MF_CHECKED, IDM_CLIP2RICHE, L"Clipboard to Richedit");
		else
			InsertMenuW(hMenu, 1, MF_BYCOMMAND | MF_STRING | MF_ENABLED | MF_UNCHECKED, IDM_CLIP2RICHE, L"Clipboard to Richedit");
			InsertMenuW(hMenu, 1, MF_BYCOMMAND | MF_STRING | MF_ENABLED, IDM_DELSELLIST, L"Delate Selected");
			InsertMenuW(hMenu, 2, MF_BYCOMMAND | MF_STRING | MF_ENABLED, IDM_CLEARLIST, L"Clear the List");
			InsertMenuW(hMenu, 3, MF_BYCOMMAND | MF_STRING | MF_ENABLED, IDM_SAVELIST, L"Save List to File");
			InsertMenuW(hMenu, 4, MF_BYCOMMAND | MF_STRING | MF_ENABLED, IDM_LOADFILE, L"Load File to List");
			TrackPopupMenu(hMenu, TPM_TOPALIGN | TPM_LEFTALIGN, LOWORD(lparam), HIWORD(lparam), 0, ghMain, 0);
	}

	if ((HWND)wParam == ghRichEdit)
	{
		HMENU hMenu = CreatePopupMenu();
		if (gbPushPt)
			InsertMenuW(hMenu, 1, MF_BYCOMMAND | MF_STRING | MF_ENABLED, IDM_PUSH, L"Disable Play to PT");
		else
			InsertMenuW(hMenu, 1, MF_BYCOMMAND | MF_STRING | MF_ENABLED, IDM_PUSH, L"Enable Play to PT");
			InsertMenuW(hMenu, 2, MF_BYCOMMAND | MF_STRING | MF_ENABLED, IDM_PASTE, L"Paste to Text Box");
			InsertMenuW(hMenu, 3, MF_BYCOMMAND | MF_STRING | MF_ENABLED, IDM_COPY, L"Copy Selected Text");
			InsertMenuW(hMenu, 4, MF_BYCOMMAND | MF_STRING | MF_ENABLED, IDM_LOOK, L"Web Lookup Selected");
		if (gbDoSpell)
			InsertMenuW(hMenu, 5, MF_BYCOMMAND | MF_STRING | MF_ENABLED | MF_CHECKED, IDM_SPELL, L"Spell Checking");
		else
			InsertMenuW(hMenu, 5, MF_BYCOMMAND | MF_STRING | MF_ENABLED | MF_UNCHECKED, IDM_SPELL, L"Spell Checking");
		if (gbSendBold)
			InsertMenuW(hMenu, 6, MF_BYCOMMAND | MF_STRING | MF_ENABLED | MF_CHECKED, IDC_TOGGLE_SEND_BOLD, L"Send Bold Text");
		else
			InsertMenuW(hMenu, 6, MF_BYCOMMAND | MF_STRING | MF_ENABLED | MF_UNCHECKED, IDC_TOGGLE_SEND_BOLD, L"Send Bold Text");

			TrackPopupMenu(hMenu, TPM_TOPALIGN | TPM_LEFTALIGN, LOWORD(lparam), HIWORD(lparam), 0, ghMain, 0);

	}

}

// Helper: check for existence of an ANSI file path
static bool FileExistsA(const char* path)
{
	DWORD attr = GetFileAttributesA(path);
	return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

//Hungspell functions definitions
BOOL InitHunspell(void)
{
	hinsDll = LoadLibraryA("libhunspell.dll");
	if (!hinsDll) 
	{
		msga("Hungspell DLL not faund!");
		OutputDebugStringA("Hungspell DLL Load Failed!\n");	
		return false;
	}

	pDllHunspellCreate = (typHunSpCreate)GetProcAddress(hinsDll, "Hunspell_create");
	pDllHunspellDestroy = (typHunSpDestroy)GetProcAddress(hinsDll, "Hunspell_destroy");
	pDllHunspellSpell = (typHunSpSpell)GetProcAddress(hinsDll, "Hunspell_spell");
	pDllHunspellSuggest = (typHunSpSuggest)GetProcAddress(hinsDll, "Hunspell_suggest");
	pDllHunspellFreeList = (typHunSpFreeList)GetProcAddress(hinsDll, "Hunspell_free_list");
	pDllHunspellAdd = (typHunSpAdd)GetProcAddress(hinsDll, "Hunspell_add");

	if (pDllHunspellCreate && pDllHunspellDestroy && pDllHunspellSpell && pDllHunspellSuggest && pDllHunspellFreeList)
	{
		// checking if dictionary files are present
		if (FileExistsA(szAff) == false || FileExistsA(szDic) == false)
		{
			msga("Dictionary files not found!");
			OutputDebugStringA("Dictionary files not found!\n");
			FreeLibrary(hinsDll);
			return false;
		}
		
		hSpell = pDllHunspellCreate(szAff, szDic);
		if (!hSpell)	
		{
			msga("Hungspell Create Handle Failed!");
			OutputDebugStringA("Hungspell Create Handle Failed!\n");
			FreeLibrary(hinsDll);
			return false;
		}
	}
	else
	{
		FreeLibrary(hinsDll);
		return false;
	}

	// Loading personal dictionary words

	std::wstring pesonalDicPath = GetPersonalDicPath();	
	std::vector<std::string> wordsInDic = LoadPersonalDictionary(pesonalDicPath);
	for (const auto& word : wordsInDic)
	{
		pDllHunspellAdd(hSpell, const_cast<char*>(word.c_str()));
	}

	return true;
}

// Using HungSpellChecker DLL with wchar_t string
bool SpellCheckAndSuggest(wchar_t* wcRawWord)
{

	LONG lngCurPos = 0;
	// Getting curser position
	SendMessageW(ghRichEdit, EM_GETSEL, 0, (LPARAM)&lngCurPos);

	// Checking for URL, if we find, no need to spell it, exit with false
	if (wcsncmp(wcRawWord, L"http://", 7) == 0 || wcsncmp(wcRawWord, L"www.", 4) == 0)
	{
		SendMessageW(ghRichEdit, EM_SETSEL, (WPARAM)lngCurPos, (LPARAM)lngCurPos);
		return FALSE;
	}

	// Now we have a clean word, try spelling
	char szRawStr[512] = { 0 };
	size_t stRet;
	const wchar_t* pszRawWord = wcRawWord;

	wcsrtombs_s(&stRet, szRawStr, 512, &pszRawWord, _TRUNCATE, 0);

	int iSpellRes = pDllHunspellSpell(hSpell, szRawStr);
	if (iSpellRes != 0) // correct spelling chugging along
	{
		SendMessageA(ghRichEdit, EM_SETSEL, (WPARAM)lngCurPos, (LPARAM)lngCurPos);
		return TRUE;
	}
	// Try to get some suggestions
	char** pwsSugList = NULL;
	int iSug = pDllHunspellSuggest(hSpell, &pwsSugList, szRawStr);
	if (iSug == 0) // No suggestion beep and continue
	{
		Beep(900, 200);
		SendMessageA(ghRichEdit, EM_SETSEL, (WPARAM)lngCurPos, (LPARAM)lngCurPos);
		return FALSE;
	}
	// if we got this far we have a list of suggestions, lets make a menu
	HMENU hSpellMenu = NULL;
	UINT uiMenuID = 1;
	wchar_t wsMenuText[256] = { 0 };
	// Create the menu for suggestions
	hSpellMenu = CreatePopupMenu();
	AppendMenuW(hSpellMenu, MF_STRING, 9993, L"Ignore and Continue");
	swprintf_s(wsMenuText, 255, L"Add: %s", wcRawWord);
	AppendMenuW(hSpellMenu, MF_STRING, 9991, wsMenuText);
	swprintf_s(wsMenuText, 255, L"Look Up: %s", wcRawWord);
	AppendMenuW(hSpellMenu, MF_STRING, 9992, wsMenuText);
	// Now we list the suggestions
	wchar_t wcSugItem[256] = { 0 };

	for (int i = 0; i < iSug; i++, uiMenuID++)
	{
		char* pszItem = pwsSugList[i];
		const char* ppszItem = pszItem;

		mbstate_t state = { 0 };
		mbsrtowcs_s(&stRet, wcSugItem, &ppszItem, 255, &state);

		AppendMenuW(hSpellMenu, MF_STRING, uiMenuID, wcSugItem);
	}
	// Display the menu
	RECT rctMain = { 0 };
	GetWindowRect(ghMain, &rctMain);
	POINT pt;
	pt.x = rctMain.left = rctMain.left + ((rctMain.right - rctMain.left) / 2);
	pt.y = rctMain.top;
	// place it in middle of main window
	UINT uiSelect = TrackPopupMenu(hSpellMenu, TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, ghRichEdit, NULL);
	// See what has been selected
	if (uiSelect == 9991) // Add the word 
	{
		// Add the word to Hunspell dictionary in memory
		pDllHunspellAdd(hSpell, szRawStr);

		// Persist the added word into personal.dic as UTF-8
		AppendToPersonalDic(wcRawWord);

		SendMessageW(ghRichEdit, EM_SETSEL, (WPARAM)lngCurPos, (LPARAM)lngCurPos);
	}
	else if (uiSelect == 9992) // Look up the word on the Internet
	{
		LookupWebDictionary();
		SendMessageW(ghRichEdit, EM_SETSEL, (WPARAM)lngCurPos, (LPARAM)lngCurPos);
	}
	else if (uiSelect == 9993) // Just ignore it
	{
		SendMessageW(ghRichEdit, EM_SETSEL, (WPARAM)lngCurPos, (LPARAM)lngCurPos);
	}
	else if (uiSelect > 0) // We have a word selected
	{
		uiSelect -= 1;
		char* pszItem = pwsSugList[uiSelect];
		const char* ppszItem = pszItem;
		mbstate_t state = { 0 };
		mbsrtowcs_s(&stRet, wcSugItem, &ppszItem, 255, &state);

		bool bRet = RichEdit_FindAndReplace(ghRichEdit, wcRawWord, wcSugItem);
		if (!bRet)
				msga("Error Replacing the word in Richedit!");
	}
	else
	{
		SendMessageW(ghRichEdit, EM_SETSEL, (WPARAM)lngCurPos, (LPARAM)lngCurPos);
	}

	if (hSpellMenu)
	{
		DestroyMenu(hSpellMenu);
		hSpellMenu = NULL;
	}
	pDllHunspellFreeList(hSpell, &pwsSugList, iSug);

	//MessageBox(ghMain, szRawWord, TEXT("spell checking"), MB_OK);

	return TRUE;
}

/// Looking words up in web dictionary
BOOL LookupWebDictionary(void)
{
	wchar_t szUrl[256] = { '\0' };
	wchar_t szWord[101] = { '\0' };

	int iLen = SendMessageA(ghRichEdit, EM_GETSELTEXT, (WPARAM)0, (LPARAM)szWord);
	if (iLen < 2 || iLen>99) return FALSE;
	swprintf_s(szUrl, L"https://dictionary.com/browse/%s ", szWord);
	ShellExecuteW(NULL, L"Open", szUrl, NULL, NULL, SW_NORMAL);
	return TRUE;
}

/// Sub Class Richedit box for spelling
LRESULT CALLBACK EditLoadSubClassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubClass, DWORD_PTR dwRefData)
{
	switch (msg)
	{
		case WM_CHAR:
		{
			// you can add custom handling for character input here if needed
		}
		break;
		case WM_KEYDOWN:
		{
			if (wParam == VK_RETURN) {
				// Call the custom function when Enter is pressed
				if (!OnSendButtonClick())
				{
					msga("Error OnSendButtonClick() FAIL!");
				}

				return 0; // Prevent default behavior
			}
		}
		break;
	}

	return DefSubclassProc(hWnd, msg, wParam, lParam);
}

/// Add text to scan list box
static void ScanAddToList(HWND hwList, wchar_t* wcText)
{
	static int iMaxLine = 0;
	int iLlen = wcslen(wcText);
	if (iLlen < 3) return;

	LRESULT lRes = SendMessageW(hwList, LB_ADDSTRING, 0, (LPARAM)wcText);
	if (lRes == LB_ERR || lRes == LB_ERRSPACE)
	{
		msga("Error Adding Text to List box!");
		return;
	}

	if (iMaxLine < iLlen) iMaxLine = iLlen;
	SendMessageW(hwList, WM_VSCROLL, (WPARAM)SB_BOTTOM, 0);
	SendMessageW(hwList, LB_SETHORIZONTALEXTENT, (WPARAM)iMaxLine * 5, 0);
}

/// Send an Item's string to Paltalk
BOOL SendListItemTextToPaltalk(void)
{
	BOOL bRet = FALSE;
	wchar_t* pwcItemText = 0;
	if (!ghPtMain || !ghList) return FALSE;
	LRESULT lIndx = SendMessageW(ghList, LB_GETCURSEL, 0, 0);
	if (lIndx == LB_ERR) return FALSE;

	LRESULT lLen = SendMessageW(ghList, LB_GETTEXTLEN, (WPARAM)lIndx, 0) + 2;
	if (lLen < 3) return FALSE;

	pwcItemText = (wchar_t*)malloc(lLen * sizeof(wchar_t));
	if (pwcItemText)
	{
		lLen = SendMessageW(ghList, LB_GETTEXT, (WPARAM)lIndx, (LPARAM)pwcItemText);
		if (lLen != 0)
		{
			// new function here
			//CopyPasteToPaltalk(pwcItemText);
			SendMessageToPaltalk(pwcItemText);
			bRet = TRUE;
		}

		free(pwcItemText);
	}

	return bRet;
}

/// Send button handler
BOOL OnSendButtonClick(void)
{
	BOOL bRet = FALSE;
	wchar_t* pwcText = 0;

	int iLL = SendMessageW(ghRichEdit, WM_GETTEXTLENGTH, 0, 0) + 2;
	if (iLL < 4) return FALSE;
	pwcText = (wchar_t*)malloc(iLL * sizeof(wchar_t));
	int iNC = SendMessageW(ghRichEdit, WM_GETTEXT, (WPARAM)iLL, (LPARAM)pwcText);
	if (iNC == 0)
	{
		bRet = FALSE;
	}
	else
	{
		//if(pwcText) pwcText[iNC] = '\0';

		if (gbPushPt)
		{
			//new function comes here 
			SendMessageToPaltalk(pwcText);
		}

		if (!AddTextToList(pwcText, iLL)) msga("Error Adding Text to List box!");

		// Clear the Rich Textbox 
		SendMessageW(ghRichEdit, WM_SETTEXT, 0, (LPARAM)L"");

		bRet = TRUE;
	}

	free(pwcText);

	return bRet;
}

/// This adds text to the list
BOOL AddTextToList(wchar_t* wcText, int iLL)
{
	static int iMaxLine = 0;

	SendMessageW(ghList, LB_ADDSTRING, 0, (LPARAM)wcText);
	SendMessageW(ghList, WM_VSCROLL, (WPARAM)SB_BOTTOM, (LPARAM)0);
	if (iMaxLine < iLL) iMaxLine = iLL;
	SendMessageW(ghList, LB_SETHORIZONTALEXTENT, (WPARAM)iMaxLine * 5, 0);

	return TRUE;
}

/// Save List to File
void SaveListToFile(HWND hwList)
{
	long lCount = SendMessageW(hwList, LB_GETCOUNT, 0, 0);
	if (lCount < 1)
	{
		msga("Nothing to Save!");
		return;
	}

	wchar_t wsFileName[MAX_PATH] = { '\0' };

	if (!SetFileNamePath(wsFileName, MAX_PATH, TRUE))
		return;


	FILE* flOut;
	_wfopen_s(&flOut, wsFileName, L"w+");
	if (!flOut)
	{
		msga("Could Not Create File");
		return;
	}

	for (int i = 0; i < lCount; i++)
	{
		int	iLen = SendMessageW(hwList, LB_GETTEXTLEN, (WPARAM)i, 0);
		if (iLen < 3) continue;
		wchar_t* wcTemp = (wchar_t*)malloc(iLen * sizeof(wchar_t) + 8);
		if (wcTemp == NULL) continue;
		int iChl = SendMessageW(hwList, LB_GETTEXT, (WPARAM)i, (LPARAM)wcTemp);
		if (iChl > 1)
		{
			wcTemp[iChl] = L'\0';
			fseek(flOut, 0L, SEEK_END);
			fwprintf(flOut, L"%s\r\n", wcTemp);
		}
		free(wcTemp);
	}
	_fcloseall();

}

/// Set the file name path 
BOOL SetFileNamePath(wchar_t *wsFileName, DWORD dwFlNmSize, BOOL bWrite)
{
	OPENFILENAMEW stOfn = { 0 };

	if (wsFileName == NULL || dwFlNmSize == 0)
		return FALSE;

	stOfn.lStructSize = sizeof(stOfn);
	stOfn.hwndOwner = ghMain;
	stOfn.hInstance = hInst;
	stOfn.lpstrCustomFilter = NULL;
	stOfn.lpstrFile = wsFileName;
	stOfn.lpstrFileTitle = NULL;
	stOfn.lpstrDefExt = L"txt";
	stOfn.lpstrFilter = L"Text (*.txt)\0*.txt\0";
	stOfn.lpstrInitialDir = NULL;
	stOfn.nMaxFile = dwFlNmSize;
	stOfn.nMaxFileTitle = dwFlNmSize;
	stOfn.nFilterIndex = (DWORD)NULL;

	if (bWrite)
	{
		stOfn.lpstrTitle = L"Save as text files Only .txt";
		stOfn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
		return GetSaveFileNameW(&stOfn);
	}
	
		stOfn.lpstrTitle = L"Open text files Only .txt";
		stOfn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
	
	    return GetOpenFileNameW(&stOfn);
}

/// Loads a Text file into List box 
BOOL LoadFileToList(HWND hwList)
{
	wchar_t wsFileName[MAX_PATH] = { '\0' };

	if (!SetFileNamePath(wsFileName, MAX_PATH, FALSE))
		return FALSE;
	std::string sLine;
	ifstream fListFile(wsFileName);
	if (!fListFile.is_open()) return FALSE;

	while (getline(fListFile, sLine))
	{
		const char* szLine = sLine.c_str();
		SendMessageA(hwList, LB_ADDSTRING, 0, (LPARAM)szLine);
	}
	fListFile.close();
	return TRUE;
}

/// Helper: Trim leading and trailing whitespace
static void TrimWhitespace(std::wstring& str)
{
	if (str.empty()) return;

	size_t start = 0;
	while (start < str.size() && iswspace(str[start]))
		++start;

	size_t end = str.size();
	while (end > start && iswspace(str[end - 1]))
		--end;

	if (start != 0 || end != str.size())
		str = str.substr(start, end - start);
}

/// Clipboard Update Handler
void OnClipboardUpdate()
{
	if (!OpenClipboard(ghMain))
		return;

	// --- RAII guard for clipboard closure ---
	struct ClipboardCloser {
		~ClipboardCloser() { CloseClipboard(); }
	} clipboardGuard;

	HANDLE hClip = nullptr;
	std::wstring text;

	// --- Try Unicode first ---
	if (IsClipboardFormatAvailable(CF_UNICODETEXT))
	{
		hClip = GetClipboardData(CF_UNICODETEXT);
		if (hClip)
		{
			LPCWSTR pData = static_cast<LPCWSTR>(GlobalLock(hClip));
			if (pData)
			{
				text.assign(pData);
				GlobalUnlock(hClip);
			}
		}
	}
	// --- Fallback to ANSI ---
	else if (IsClipboardFormatAvailable(CF_TEXT))
	{
		hClip = GetClipboardData(CF_TEXT);
		if (hClip)
		{
			LPCSTR pData = static_cast<LPCSTR>(GlobalLock(hClip));
			if (pData)
			{
				int required = MultiByteToWideChar(CP_ACP, 0, pData, -1, nullptr, 0);
				if (required > 0)
				{
					text.resize(required - 1); // exclude null terminator
					MultiByteToWideChar(CP_ACP, 0, pData, -1, &text[0], required);
				}
				GlobalUnlock(hClip);
			}
		}
	}

	if (text.empty())
		return;

	// --- Trim whitespace/newlines ---
	TrimWhitespace(text);
	if (text.empty())
		return;

	// --- Duplicate detection ---
	static std::wstring lastClipboardText;
	if (text == lastClipboardText)
		return; // ignore duplicate

	lastClipboardText = text; // update last seen

	// --- Deliver text to destination ---
	if (gbClip2Richedit)
	{
		text += L" ";
		SendMessageW(ghRichEdit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
	}
	else
	{
		SendMessageW(ghList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
	}

	// --- Optionally clear clipboard ---
	EmptyClipboard();
}

/// Turning on and off Clipboard Monitoring
void OnClipboardMenu(void)
{
	if (!gbClipActive)
	{
		gbClipActive = AddClipboardFormatListener(ghMain);
	}
	else
	{
		if (RemoveClipboardFormatListener(ghMain))
			gbClipActive = FALSE;
	}
}

/// Sending Text to Paltalk 
void SendMessageToPaltalk(wchar_t* szMsg)
{
	wstring	wcOut(szMsg);
	HRESULT hr = S_OK;

	if (!emojiTextEditElement) return;
	CComPtr<IUIAutomationLegacyIAccessiblePattern> pattern;
	hr = emojiTextEditElement->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId, IID_IUIAutomationLegacyIAccessiblePattern, (void**)&pattern);
	//GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&pattern));
	if (SUCCEEDED(hr)) {
		BSTR bstrOut = NULL;

		/*int iLen = MultiByteToWideChar(CP_ACP, 0, szMsg, -1, NULL, 0);
		wcOut.resize(iLen - 1);
		MultiByteToWideChar(CP_ACP, 0, szMsg, -1, &wcOut[0], iLen); */
		if (gbSendBold) {
			wstring wstrOutBold = ConvertToBold(wcOut);
			bstrOut = SysAllocString(wstrOutBold.c_str());
		}
		else {
			bstrOut = SysAllocString(wcOut.c_str());
		}
		//emojiTextEditElement->SetFocus();
		// Send the text 
		pattern->SetValue(bstrOut);
		SendMessageA(ghPtMain, WM_KEYDOWN, (WPARAM)VK_RETURN, 0);
		SendMessageA(ghPtMain, WM_KEYUP, (WPARAM)VK_RETURN, 0);
		SysFreeString(bstrOut);
	}
	else {
		OutputDebugStringA("GetCurrentPatternAs failed: ");
	}
	SetFocus(ghRichEdit);
	Sleep(500);

}

BOOL GetNicknames(void)
{
	if (!ghPtLv) return FALSE; // No Paltalk ListView handle
	SendMessageW(ghListNicks, LB_RESETCONTENT, (WPARAM)0, (LPARAM)0);

	char szOut[MAX_PATH] = { '0' };
	char szNickname[MAX_PATH] = { '0' };

	struct InterProcessData {
		LVITEMA hdi;
		char buffer[MAX_PATH];
	};

	DWORD dwProcId;
	HANDLE hProc;
	int iI, iNicks;
	int iImg = 0;

	GetWindowThreadProcessId(ghPtLv, &dwProcId);
	hProc = OpenProcess(
		PROCESS_VM_OPERATION |
		PROCESS_VM_READ |
		PROCESS_VM_WRITE, FALSE, dwProcId);

	if (hProc == NULL)return FALSE;

	InterProcessData* pRemoteData = reinterpret_cast<InterProcessData*>(
		VirtualAllocEx(hProc, NULL, sizeof(InterProcessData), MEM_COMMIT, PAGE_READWRITE));
	if (!pRemoteData) return FALSE;

	iNicks = SendMessage(ghPtLv, LVM_GETITEMCOUNT, 0, 0);

	wsprintfA(szOut, "Number of items in list view: %d \n", iNicks);
	OutputDebugStringA(szOut);

	for (iI = 0; iI < iNicks; iI++)
	{
		InterProcessData ixData;
		ZeroMemory(&ixData, sizeof(ixData));

		ixData.hdi.mask = LVIF_TEXT | LVIF_IMAGE;
		ixData.hdi.pszText = pRemoteData->buffer;
		ixData.hdi.iItem = iI;
		ixData.hdi.iSubItem = 0;
		ixData.hdi.cchTextMax = 260;

		SIZE_T lpNumBytesWritten = 0;
		// Write the LVITEM structure to the space in the remote process
		// (without the buffer, its contents are undefined anyway)
		if (!WriteProcessMemory(hProc, pRemoteData, &ixData, sizeof(ixData.hdi), &lpNumBytesWritten)) break;
		wsprintfA(szOut, "Num bytes written to pM: %d \n", lpNumBytesWritten);
		OutputDebugStringA(szOut);
		
		// Send the get item message  LVM_GETITEMTEXTA 4141 is to read nick as char[], 4171 to get the image number
		if (!SendMessage(ghPtLv, LVM_GETITEMTEXTA, (WPARAM)iI, reinterpret_cast<LPARAM>(&pRemoteData->hdi)))	break;

		SIZE_T lpNumBytesRead = 0;
		// Read the data back to this process memory
		if (!ReadProcessMemory(hProc, pRemoteData, &ixData, sizeof(ixData), &lpNumBytesRead)) break;
		wsprintfA(szOut, "Num bytes read to ixData : %d \n", lpNumBytesRead);
		OutputDebugStringA(szOut);

		//Documentation says that pszText can be changed by the remote process
		if (ixData.hdi.pszText != pRemoteData->buffer)
		{
			lpNumBytesRead = 0;
			ReadProcessMemory(hProc, ixData.hdi.pszText, &ixData.buffer, ixData.hdi.cchTextMax * sizeof(wchar_t), &lpNumBytesRead);
		}
		wsprintfA(szOut, "Num bytes read to ixData.buffer : %d \n", lpNumBytesRead);
		OutputDebugStringA(szOut);

		wsprintfA(szNickname, "%s",ixData.buffer);
		OutputDebugStringA(szNickname);

		SendMessageA(ghListNicks, LB_ADDSTRING, (WPARAM)0, (LPARAM)szNickname);

	}

	if (!pRemoteData) VirtualFreeEx(hProc, pRemoteData, 0, MEM_RELEASE);
	CloseHandle(hProc);

	return TRUE;
}

// Adding the Nickname to the RichEdit
BOOL SendNick2Richedit(void)
{
	BOOL bRet = FALSE;
	wchar_t* pwcItemText = 0;
	wchar_t wcMsg[MAX_PATH] = { '\0' };
	if (!ghListNicks) return FALSE;
	LRESULT lIndx = SendMessageW(ghListNicks, LB_GETCURSEL, 0, 0);
	if (lIndx == LB_ERR) return FALSE;

	LRESULT lLen = SendMessageW(ghListNicks, LB_GETTEXTLEN, (WPARAM)lIndx, 0) + 2;
	if (lLen < 3) return FALSE;

	pwcItemText = (wchar_t*)malloc(lLen * sizeof(wchar_t));
	if (pwcItemText)
	{
		lLen = SendMessageW(ghListNicks, LB_GETTEXT, (WPARAM)lIndx, (LPARAM)pwcItemText);
		
		if (lLen != 0)
		{
			wsprintfW(wcMsg, L"%s ", pwcItemText);
			//int iSel = lstrlenW(wcMsg) + 2;
			SendMessageW(ghRichEdit, EM_REPLACESEL, (WPARAM)TRUE, (LPARAM)wcMsg);
			SetFocus(ghRichEdit);
			bRet = TRUE;
		}

		free(pwcItemText);
	}
		
	return bRet;
}

/// Initialize UI Automation
HRESULT __stdcall InitUIAutomation(void)
{
	OutputDebugStringA("[UIAutomation] Creating CUIAutomation instance...\n");
	HRESULT hr = CoCreateInstance(__uuidof(CUIAutomation), NULL,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&g_pUIAutomation));
	if (FAILED(hr)) {
		char szMsg[128];
		sprintf_s(szMsg, sizeof(szMsg), "[UIAutomation] CoCreateInstance FAILED! HRESULT=0x%08X\n", hr);
		OutputDebugStringA(szMsg);
		return hr;
	}

	OutputDebugStringA("[UIAutomation] CUIAutomation instance created successfully.\n");
	return S_OK;
}

/// Clean up UI Automation
HRESULT __stdcall UninitUIAutomation(void)
{
	
	if (g_pUIAutomation) {
		g_pUIAutomation.Release();
		OutputDebugStringA("[UIAutomation] CUIAutomation instance released.\n");
		return S_OK;
	}
	else {
		OutputDebugStringA("[UIAutomation] CUIAutomation instance was not initialized.\n");
	}
	// If we reach here, it means the instance was not created or already released.

	if (automationElementRoom) {
		automationElementRoom.Release();
		OutputDebugStringA("[UIAutomation] automationElementRoom released.\n");
	}
	else {
		OutputDebugStringA("[UIAutomation] automationElementRoom was not initialized.\n");
	}
	// If we reach here, it means the instance was not created or already released.

	if (emojiTextEditElement) {
		emojiTextEditElement.Release();
		OutputDebugStringA("[UIAutomation] emojiTextEditElement released.\n");
	}
	else {
		OutputDebugStringA("[UIAutomation] emojiTextEditElement was not initialized.\n");
	}
	// If we reach here, it means the instance was not created or already released.

	OutputDebugStringA("[COM] Uninitializing COM...\n");

	CoUninitialize();

	OutputDebugStringA("[COM] COM uninitialized.\n");

	return E_NOTIMPL;
}

/// Get the UIAutomation element from HWND and ClassName
HRESULT __stdcall GetUIAutomationElementFromHWNDAndClassName(HWND hwnd, const wchar_t* className, IUIAutomationElement** foundElement) {
	HRESULT hr = S_OK;

	CComPtr<IUIAutomationElement> elementRoom;
	hr = g_pUIAutomation->ElementFromHandle(hwnd, &elementRoom);
	if (FAILED(hr)) {
#ifdef DEBUG
		char szDebug[] = "ElementFromHandle failed: elementRoom\n";
		OutputDebugStringA(szDebug);
#endif // DEBUG
		return hr;
	}

	CComPtr<IUIAutomationCondition> classNameCondition;
	CComVariant classNameVariant(className);
	hr = g_pUIAutomation->CreatePropertyCondition(UIA_ClassNamePropertyId, classNameVariant, &classNameCondition);
	if (FAILED(hr)) {
#ifdef DEBUG
		char szDebug[] = "CreatePropertyCondition failed : className\n";
		OutputDebugStringA(szDebug);
#endif // DEBUG

		return hr;
	}

	hr = elementRoom->FindFirst(TreeScope_Subtree, classNameCondition, foundElement);
	if (FAILED(hr)) {
#ifdef DEBUG
		char szDebug[] = "FindFirst failed: elementRoom\n";
		OutputDebugStringA(szDebug);
#endif // DEBUG

		return hr;
	}

	return S_OK;
}

/// Find the Chat window by title
HRESULT __stdcall FindWindowByTitle(const std::wstring& title, IUIAutomationElement** outElement)
{
	HRESULT hr = S_OK;

	CComPtr<IUIAutomationElement> root = nullptr; // Desktop
	hr = g_pUIAutomation->GetRootElement(&root);
	if (FAILED(hr)) {
#ifdef DEBUG
		swprintf_s(gwcDebugMsg, MAX_PATH, L"Error getting root element");
		OutputDebugStringW(gwcDebugMsg);
#endif // DEBUG
		return hr;
	}

	CComPtr<IUIAutomationCondition> cond = nullptr;
	CComVariant NameVariant(title.c_str());
	hr = g_pUIAutomation->CreatePropertyCondition(UIA_NamePropertyId, NameVariant, &cond);

	hr = root->FindFirst(TreeScope_Children, cond, outElement);
	if (FAILED(hr)) {
#ifdef DEBUG
		sprintf_s(gszDebugMsg, MAX_PATH, "Failed to find the Room!");
		OutputDebugStringA(gszDebugMsg);
#endif // DEBUG
		return hr;
	}

	return hr;
}

/// Convert the string to bold
wstring ConvertToBold(const wstring& inString) {
	const int boldLowercaseOffset = 119737; // 'a' to bold 'a'
	const int boldUppercaseOffset = 119743; // 'A' to bold 'A'
	const int boldDigitOffset = 120734;      // '0' to bold '0'

	wstring result;

	for (wchar_t c : inString) {
		if (iswlower(c)) { // Lowercase letters
			DWORD codePoint = c + boldLowercaseOffset;
			if (codePoint <= 0xFFFF) {
				result += static_cast<wchar_t>(codePoint);
			}
			else {
				// Handle surrogate pairs for Unicode characters above U+FFFF
				wchar_t highSurrogate = (wchar_t)(0xD800 + (codePoint - 0x10000) / 0x400);
				wchar_t lowSurrogate = (wchar_t)(0xDC00 + (codePoint - 0x10000) % 0x400);
				result += highSurrogate;
				result += lowSurrogate;
			}
		}
		else if (iswupper(c)) { // Uppercase letters
			DWORD codePoint = c + boldUppercaseOffset;
			if (codePoint <= 0xFFFF) {
				result += static_cast<wchar_t>(codePoint);
			}
			else {
				wchar_t highSurrogate = (wchar_t)(0xD800 + (codePoint - 0x10000) / 0x400);
				wchar_t lowSurrogate = (wchar_t)(0xDC00 + (codePoint - 0x10000) % 0x400);
				result += highSurrogate;
				result += lowSurrogate;
			}
		}
		else if (iswdigit(c)) { // Digits
			DWORD codePoint = c + boldDigitOffset;
			if (codePoint <= 0xFFFF) {
				result += static_cast<wchar_t>(codePoint);
			}
			else {
				wchar_t highSurrogate = (wchar_t)(0xD800 + (codePoint - 0x10000) / 0x400);
				wchar_t lowSurrogate = (wchar_t)(0xDC00 + (codePoint - 0x10000) % 0x400);
				result += highSurrogate;
				result += lowSurrogate;
			}
		}
		else {
			result += c;
		}
	}

	return result;
}

// Personal Dictionary Management	
// Get the directory of the running executable
std::wstring GetExeDirectory()
{
	wchar_t path[MAX_PATH] = { 0 };
	GetModuleFileNameW(nullptr, path, MAX_PATH);
	PathRemoveFileSpecW(path);
	return std::wstring(path);
}

// Get the full path to personal.dic	
std::wstring GetPersonalDicPath()
{
	return GetExeDirectory() + L"\\personal.dic";
}

// Load personal.dic into a vector of UTF-8 strings
std::vector<std::string> LoadPersonalDictionary(const std::wstring& file)
{
	std::vector<std::string> words;
	std::string narrow(file.begin(), file.end());
	std::ifstream ifs(narrow, std::ios::in | std::ios::binary);
	if (!ifs.is_open()) return words;

	std::string line;
	while (std::getline(ifs, line))
	{
		if (!line.empty())
		{
			// Trim CR if present
			if (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
				line.erase(std::remove_if(line.begin(), line.end(), [](char c) { return c == '\r' || c == '\n'; }), line.end());

			if (!line.empty()) words.push_back(line);
		}
	}

	return words;
}

// Append a word to personal.dic if not already present (case-insensitive)	
void AppendToPersonalDic(const std::wstring& word)
{
	std::wstring personal = GetPersonalDicPath();
	//EnsureFileExists(personal);

	// Load existing words to avoid duplicates (case-insensitive ASCII compare)
	auto vecExisting = LoadPersonalDictionary(personal);
	std::string u8Word = WideToUtf8(word);

	for (auto& s : vecExisting)
	{
		// simple case-sensitive compare	
		std::string lower_s = s;
		std::string lower_w = u8Word;
		if (lower_s == lower_w) return; // already exists
	}

	// Append the word in UTF-8, one per line
	std::string narrow(personal.begin(), personal.end());
	std::ofstream ofs(narrow, std::ios::out | std::ios::app | std::ios::binary);
	if (!ofs.is_open()) return;
	ofs << u8Word << "\n";
	ofs.close();
}

// Helper: Convert between wide string and UTF-8
std::string WideToUtf8(const std::wstring& w)
{
	if (w.empty()) return std::string();
	int required = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	if (required <= 0) return std::string();
	std::string out(required, 0);
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], required, nullptr, nullptr);
	return out;
}

// Helper: Convert from UTF-8 to wide string
std::wstring Utf8ToWide(const std::string& s)
{
	if (s.empty()) return std::wstring();
	int required = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
	if (required <= 0) return std::wstring();
	std::wstring out(required, 0);
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], required);
	return out;
}


