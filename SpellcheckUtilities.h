#pragma once
#include <windows.h>
#include <string>
#include <cwctype>
#include <richole.h>

bool SpellCheckAndSuggest(wchar_t* wcRawWord);

// Utility: Retrieve the last word before the caret in an Edit control  
inline void RetrieveLastWord(HWND hEdit)
{
    DWORD selStart = 0, selEnd = 0;
    SendMessage(hEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

    int textLen = GetWindowTextLengthW(hEdit);
    std::wstring buf(textLen + 1, L'\0');
    GetWindowTextW(hEdit, &buf[0], textLen + 1);

    if (selStart == 0) return;

    int end = selStart - 1;
    while (end >= 0 && iswspace(buf[end])) end--;

    int start = end;
    while (start >= 0 && !iswspace(buf[start]) &&
        !wcschr(L",!?;:()[]\"", buf[start]))
        start--;

    start++;

    if (start <= end) {

         std::wstring word = buf.substr(start, end - start + 1);
#ifdef DEBUG
        std::wstring msg = L"Word finished: [" + word + L"]\n";
        OutputDebugStringW(msg.c_str());
#endif // DEBUG
		// Call SpellCheckAndSuggest 
		SpellCheckAndSuggest((wchar_t*)word.c_str());
    }
}

// Utility: find and replace FIRST occurrence
inline BOOL RichEdit_FindAndReplace(HWND hRich, LPCWSTR oldWord, LPCWSTR newWord)
{
    FINDTEXTEXW ft = { 0 };
    ft.chrg.cpMin = 0;        // search from start
    ft.chrg.cpMax = -1;       // until end
    ft.lpstrText = oldWord;   // word to find

    // Ask RichEdit to find it
    LONG pos = (LONG)SendMessageW(hRich, EM_FINDTEXTEXW, FR_DOWN, (LPARAM)&ft);
    if (pos < 0)
        return FALSE; // not found

    // Select the found text
    SendMessageW(hRich, EM_SETSEL, ft.chrgText.cpMin, ft.chrgText.cpMax);

    // Replace it
    SendMessageW(hRich, EM_REPLACESEL, TRUE, (LPARAM)newWord);

    return TRUE;
}


// End of SpellcheckUtilities.h 
