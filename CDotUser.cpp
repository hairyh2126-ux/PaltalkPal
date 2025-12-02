#pragma once
#include "CDotUser.h"
#include <thread>
#include <chrono>

CDotUser::CDotUser(IUIAutomation* pAutomation, HWND hMainWindow, const std::wstring& roomTitle)
    : m_pAutomation(pAutomation), m_hMain(hMainWindow), m_roomTitle(roomTitle)
{
}

// ------------------------------------------------------------
// Simple title search
// ------------------------------------------------------------
HRESULT CDotUser::FindWindowByTitle(const std::wstring& title, IUIAutomationElement** outElement)
{
    *outElement = nullptr;

    CComPtr<IUIAutomationCondition> cond;
    VARIANT v;
    v.vt = VT_BSTR;
    v.bstrVal = SysAllocString(title.c_str());

    HRESULT hr = m_pAutomation->CreatePropertyCondition(UIA_NamePropertyId, v, &cond);
    SysFreeString(v.bstrVal);

    if (FAILED(hr)) return hr;

    CComPtr<IUIAutomationElement> root;
    hr = m_pAutomation->GetRootElement(&root);
    if (FAILED(hr)) return hr;

    return root->FindFirst(TreeScope_Descendants, cond, outElement);
}

// ------------------------------------------------------------
// Mouse simulation
// ------------------------------------------------------------
void CDotUser::SimulateRightClick(int x, int y)
{
    SetCursorPos(x, y);
    mouse_event(MOUSEEVENTF_RIGHTDOWN, x, y, 0, 0);
    mouse_event(MOUSEEVENTF_RIGHTUP,   x, y, 0, 0);
}

void CDotUser::SimulateLeftClick(int x, int y)
{
    SetCursorPos(x, y);
    mouse_event(MOUSEEVENTF_LEFTDOWN, x, y, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP,   x, y, 0, 0);
}

// ------------------------------------------------------------
// Press ENTER twice
// ------------------------------------------------------------
void CDotUser::SendEnterTwice()
{
    INPUT in = { 0 };
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = VK_RETURN;

    for (int i = 0; i < 2; i++)
    {
        SendInput(1, &in, sizeof(INPUT));
        in.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(INPUT));
        in.ki.dwFlags = 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
}

// ------------------------------------------------------------
// Extract center of UIA element (BoundingRectangle)
// ------------------------------------------------------------
bool CDotUser::GetBoundingCenter(IUIAutomationElement* el, POINT& out)
{
    if (!el) return false;

    VARIANT v;
    VariantInit(&v);

    HRESULT hr = el->GetCurrentPropertyValue(UIA_BoundingRectanglePropertyId, &v);
    if (FAILED(hr) || v.vt != (VT_ARRAY | VT_R8))
        return false;

    SAFEARRAY* psa = v.parray;
    double* p = nullptr;
    SafeArrayAccessData(psa, (void**)&p);

    if (!p)
    {
        SafeArrayUnaccessData(psa);
        return false;
    }

    RECT r;
    r.left   = (LONG)p[0];
    r.top    = (LONG)p[1];
    r.right  = (LONG)p[2];
    r.bottom = (LONG)p[3];

    out.x = r.left + (r.right / 2);
    out.y = r.top  + (r.bottom / 2);

    SafeArrayUnaccessData(psa);
    VariantClear(&v);

    return true;
}

// ------------------------------------------------------------
// Bring window to foreground
// ------------------------------------------------------------
void CDotUser::RestoreAndBringToFront(HWND hWnd)
{
    if (!IsWindow(hWnd)) return;

    if (IsIconic(hWnd))
        ShowWindow(hWnd, SW_RESTORE);

    HWND fg = GetForegroundWindow();
    if (fg == hWnd) return;

    DWORD fgThread = GetWindowThreadProcessId(fg, nullptr);
    DWORD thisThread = GetCurrentThreadId();

    AttachThreadInput(thisThread, fgThread, TRUE);
    SetForegroundWindow(hWnd);
    BringWindowToTop(hWnd);
    SetFocus(hWnd);
    AttachThreadInput(thisThread, fgThread, FALSE);
}

// ------------------------------------------------------------
// Main Function: Dot / Un-Dot User
// ------------------------------------------------------------
HRESULT CDotUser::DotAndUnDotMicUser(const std::string& micUser)
{
    RestoreAndBringToFront(m_hMain);

    POINT savedCursor{};
    GetCursorPos(&savedCursor);

    // -----------------------------------
    // Find chat room
    // -----------------------------------
    CComPtr<IUIAutomationElement> room;
    HRESULT hr = FindWindowByTitle(m_roomTitle, &room);
    if (!room) return E_FAIL;

    room->SetFocus();

    // -------------------------------------------------------
    // Locate MicQueueTitleItemWidget
    // -------------------------------------------------------
    CComPtr<IUIAutomationCondition> condMicTitle;
    CComVariant vMic(L"ui::rooms::member_list::MicQueueTitleItemWidget");

    m_pAutomation->CreatePropertyCondition(UIA_ClassNamePropertyId, vMic, &condMicTitle);

    CComPtr<IUIAutomationElement> micTitle;
    room->FindFirst(TreeScope_Descendants, condMicTitle, &micTitle);
    if (!micTitle) return E_FAIL;

    // -------------------------------------------------------
    // Find BaseButton inside it
    // -------------------------------------------------------
    CComPtr<IUIAutomationCondition> condBase;
    CComVariant vBase(L"qtctrl::BaseButton");
    m_pAutomation->CreatePropertyCondition(UIA_ClassNamePropertyId, vBase, &condBase);

    CComPtr<IUIAutomationElement> baseButton;
    micTitle->FindFirst(TreeScope_Descendants, condBase, &baseButton);
    if (!baseButton) return E_FAIL;

    // -------------------------------------------
    // Click search box in BaseButton
    // -------------------------------------------
    POINT ptButton{};
    if (!GetBoundingCenter(baseButton, ptButton))
        return E_FAIL;

    baseButton->SetFocus();
    SimulateLeftClick(ptButton.x, ptButton.y);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Type the user name
    for (char c : micUser)
        SendMessageA(m_hMain, WM_CHAR, (WPARAM)c, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // -------------------------------------------
    // Find MemberItemWidget (Search result)
    // -------------------------------------------
    CComPtr<IUIAutomationCondition> condMember;
    CComVariant vMem(L"ui::rooms::member_list::MemberItemWidget");
    m_pAutomation->CreatePropertyCondition(UIA_ClassNamePropertyId, vMem, &condMember);

    CComPtr<IUIAutomationElement> member;
    room->FindFirst(TreeScope_Descendants, condMember, &member);

    if (!member)
    {
        // Reset search
        SimulateLeftClick(ptButton.x, ptButton.y);
        SetCursorPos(savedCursor.x, savedCursor.y);
        return E_NOTIMPL;
    }

    // -------------------------------------------
    // Right click the user + Enter x2
    // -------------------------------------------
    POINT ptUser{};
    if (GetBoundingCenter(member, ptUser))
    {
        room->SetFocus();
        SimulateRightClick(ptUser.x, ptUser.y);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        SendEnterTwice();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        SimulateLeftClick(ptButton.x, ptButton.y);
    }

    SetCursorPos(savedCursor.x, savedCursor.y);
    return S_OK;
}
