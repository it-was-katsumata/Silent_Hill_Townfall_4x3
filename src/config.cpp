// config.cpp - Townfall 4:3 fork, by it-was-katsumata (2026). GPL-3.0, see LICENSE.
#include "config.h"

#include <cwchar>

bool ModFilePath(wchar_t* out, const wchar_t* file) {
    HMODULE self;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)&ModFilePath, &self)) {
        out[0] = L'\0';
        return false;
    }
    GetModuleFileNameW(self, out, MAX_PATH);
    wchar_t* slash = wcsrchr(out, L'\\');
    if (!slash) {
        out[0] = L'\0';
        return false;
    }
    wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - out), file);
    return true;
}

int ConfigInt(const wchar_t* section, const wchar_t* key, int def) {
    wchar_t path[MAX_PATH];
    if (!ModFilePath(path, MOD_INI_NAME)) return def;
    return GetPrivateProfileIntW(section, key, def, path);
}
