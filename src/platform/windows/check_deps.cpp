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

#include <string>
#include <windows.h>

bool CheckRegistry(HKEY hKeyRoot, LPCSTR subKey, LPCSTR displayNameToFind)
{
    HKEY hKey;
    if (RegOpenKeyExA(hKeyRoot, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    char keyName[256];
    DWORD keyNameSize = sizeof(keyName);
    DWORD index = 0;

    while (RegEnumKeyExA(hKey, index++, keyName, &keyNameSize, NULL, NULL, NULL,
                         NULL) == ERROR_SUCCESS) {
        HKEY appKey;
        if (RegOpenKeyExA(hKey, keyName, 0, KEY_READ, &appKey) ==
            ERROR_SUCCESS) {
            char displayName[256];
            DWORD displayNameSize = sizeof(displayName);
            if (RegQueryValueExA(appKey, "DisplayName", NULL, NULL,
                                 (LPBYTE)displayName,
                                 &displayNameSize) == ERROR_SUCCESS) {
                if (strcmp(displayName, displayNameToFind) == 0) {
                    RegCloseKey(appKey);
                    RegCloseKey(hKey);
                    return true;
                }
            }
            RegCloseKey(appKey);
        }
        keyNameSize = sizeof(keyName);
    }

    RegCloseKey(hKey);
    return false;
}

bool CheckRegistryKeyExists(HKEY hKeyRoot, LPCSTR subKey)
{
    HKEY hKey;
    LONG result = RegOpenKeyExA(hKeyRoot, subKey, 0, KEY_READ, &hKey);
    if (result == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

bool IsAppleMobileDeviceSupportInstalled()
{
    if (CheckRegistry(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                      "Apple Mobile Device Support")) {
        return true;
    }
    if (CheckRegistry(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\WOW6432Node\\Microsoft\\Wi"
                      "ndows\\CurrentVersion\\Uninstall",
                      "Apple Mobile Device Support")) {
        return true;
    }
    return false;
}

bool IsWinFspInstalled()
{
    if (CheckRegistry(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                      "WinFsp 2025")) {
        return true;
    }
    if (CheckRegistry(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\WOW6432Node\\Microsoft\\Wi"
                      "ndows\\CurrentVersion\\Uninstall",
                      "WinFsp 2025")) {
        return true;
    }
    return false;
}

bool is_iDescriptorInstalled()
{
    if (CheckRegistry(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                      "iDescriptor")) {
        return true;
    }
    if (CheckRegistry(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\WOW6432Node\\Microsoft\\Wi"
                      "ndows\\CurrentVersion\\Uninstall",
                      "iDescriptor")) {
        return true;
    }
    return false;
}

bool IsBonjourServiceInstalled()
{
    if (CheckRegistryKeyExists(HKEY_LOCAL_MACHINE,
                               "SOFTWARE\\Apple Inc.\\Bonjour")) {
        return true;
    }

    return false;
}