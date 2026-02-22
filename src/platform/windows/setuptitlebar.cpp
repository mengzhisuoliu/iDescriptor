#include "win_common.h"

// TODO: remove
void setupTitleBar(HWND hwnd)
{
    if (!hwnd)
        return;

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    if (style) {
        style &= ~WS_THICKFRAME; // disable resizing
        // Keep WS_CAPTION, WS_SYSMENU, WS_MINIMIZEBOX, WS_MAXIMIZEBOX
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
    }

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // Clear title text and icon
    SetWindowTextW(hwnd, L"");
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) nullptr);
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM) nullptr);

    // Make the title bar transparent (extend frame into client area)
    MARGINS margins = {0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // Disable the DWM caption drawing so the title bar has no background
    BOOL value = TRUE;
    const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_ATTR = 20;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_ATTR, &value,
                          sizeof(value));
}