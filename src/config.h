// config.h - Townfall 4:3 fork, by it-was-katsumata (2026). GPL-3.0, see LICENSE.
#pragma once
#include <windows.h>

// Townfall43.ini / Townfall43.log live next to the proxy DLL.
#define MOD_INI_NAME L"Townfall43.ini"
#define MOD_LOG_NAME L"Townfall43.log"

// Writes "<dll dir>\<file>" into out (MAX_PATH). Returns false if the DLL
// path cannot be determined.
bool ModFilePath(wchar_t* out, const wchar_t* file);

int ConfigInt(const wchar_t* section, const wchar_t* key, int def);
