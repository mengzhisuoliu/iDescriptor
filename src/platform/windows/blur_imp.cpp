/*
 * iDescriptor: A free and open-source idevice management tool.
 *
 * Copyright (C) 2025 Uncore <https://github.com/uncor3>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "../../settingsmanager.h"
#include "win_common.h"

void enableAcrylic(HWND hwnd)
{
    const HINSTANCE hModule = LoadLibraryA("user32.dll");
    if (hModule) {
        struct ACCENTPOLICY {
            int nAccentState;
            int nFlags;
            DWORD nColor;
            int nAnimationId;
        };
        struct WINCOMPATTRDATA {
            int nAttribute;
            PVOID pData;
            ULONG ulDataSize;
        };

        typedef BOOL(WINAPI *
                     pSetWindowCompositionAttribute)(HWND, WINCOMPATTRDATA *);
        const auto SetWindowCompositionAttribute =
            (pSetWindowCompositionAttribute)GetProcAddress(
                hModule, "SetWindowCompositionAttribute");

        if (SetWindowCompositionAttribute) {
            ACCENTPOLICY policy{};
            policy.nAccentState = 4; // ACCENT_ENABLE_ACRYLICBLURBEHIND
            policy.nFlags = 2;

            policy.nColor = 0xD0202020;

            WINCOMPATTRDATA data{};
            data.nAttribute = 19;
            data.pData = &policy;
            data.ulDataSize = sizeof(policy);

            SetWindowCompositionAttribute(hwnd, &data);
        }
        FreeLibrary(hModule);
    }
}

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

#ifndef DWMWA_MICA_EFFECT
#define DWMWA_MICA_EFFECT 1029
#endif
void enableMica(HWND hwnd)
{
    if (!hwnd)
        return;
    SettingsManager *sm = SettingsManager::sharedInstance();
    WIN_BACKDROP type = sm->winBackdropType();
    MARGINS margins = {-1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    DWORD build = 0;
    RTL_OSVERSIONINFOW rovi = {0};
    rovi.dwOSVersionInfoSize = sizeof(rovi);

    using RtlGetVersionPtr = LONG(WINAPI *)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        auto pRtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
            GetProcAddress(ntdll, "RtlGetVersion"));
        if (pRtlGetVersion && pRtlGetVersion(&rovi) == 0) {
            build = rovi.dwBuildNumber;
        }
    }

    if (build >= 22523) {
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type,
                              sizeof(type));
    } else if (build >= 22000) {
        // Undocumented old method
        BOOL mica = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT, &mica, sizeof(mica));
    }
}
