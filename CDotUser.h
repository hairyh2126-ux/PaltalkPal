#pragma once
#include <windows.h>
#include <atlbase.h>
#include <uiautomation.h>
#include <string>

class CDotUser
{
public:
    CDotUser(IUIAutomation* pAutomation, HWND hMainWindow, const std::wstring& roomTitle);
    ~CDotUser() = default;

    HRESULT DotAndUnDotMicUser(const std::string& micUser);

private:
    // Core helpers
    HRESULT FindWindowByTitle(const std::wstring& title, IUIAutomationElement** outElement);
    void SimulateRightClick(int x, int y);
    void SimulateLeftClick(int x, int y);
    void SendEnterTwice();
    void RestoreAndBringToFront(HWND hWnd);

    // Utility helpers
    bool GetBoundingCenter(IUIAutomationElement* el, POINT& out);

private:
    IUIAutomation* m_pAutomation;
    HWND m_hMain;
    std::wstring m_roomTitle;
};
