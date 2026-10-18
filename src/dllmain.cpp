// dllmain.cpp - Townfall 4:3 mod (fork of Infinest's Townfall Super Ultrawide)
//
// Fixes for playing SILENT HILL: Townfall on a display that is already 4:3
// (e.g. 2048x1536): gameplay keeps the full 16:9 horizontal view and gains
// view above and below (the game alone crops the sides instead), gameplay is
// never letterboxed, and cutscenes can optionally fill the screen.
//
// This DLL is dropped into Townfall\Binaries\Win64 as a proxy for a system
// DLL the game imports statically (winmm.dll by default, so it can coexist
// with OptiScaler/ReShade as dxgi.dll; build.bat accepts another target) and
// installs one camera hook (hooks.cpp).
//
// The hook site is located at runtime by pattern scanning, so a game update
// that moves code will not corrupt anything - the hook simply refuses to
// install and the game runs unmodified.
//
// Configuration (Townfall43.ini, next to this DLL):
//   [Camera] Enabled              0/1 - master switch
//   [Camera] KeepWidescreenWidth  0/1 - 16:9 width + extra height (Vert+)
//   [Camera] NoGameplayLetterbox  0/1 - remove letterboxing from gameplay
//   [Camera] FillCutscenes        0/1 - cutscenes fill the screen (sides cropped)
//   [Log]    Enabled              0/1 - write Townfall43.log
// The ini is generated with defaults on first launch if it does not exist.

#include <windows.h>

#include "config.h"
#include "log.h"
#include "proxy.h"

bool InstallCameraHooks(HMODULE game);

// Write a default ini with explanations if the user has none yet.
static void EnsureDefaultIni() {
    wchar_t path[MAX_PATH];
    if (!ModFilePath(path, MOD_INI_NAME) || GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
        return;

    static const char kDefaultIni[] =
        "; Townfall 4:3 mod - configuration\r\n"
        "; Generated automatically. Delete this file to reset to defaults.\r\n"
        "; Changes take effect the next time the game starts.\r\n"
        "\r\n"
        "[Camera]\r\n"
        "; 0 - the mod does nothing (no hooks installed), 1 - on\r\n"
        "Enabled=1\r\n"
        "\r\n"
        "; 0 - off, 1 - on\r\n"
        "; Keeps the full 16:9 left-to-right view on a 4:3 (or any narrower)\r\n"
        "; display and adds the extra screen height as view above and below.\r\n"
        "; 0 shows what the game does by default: 16:9 height, sides cut off.\r\n"
        "KeepWidescreenWidth=1\r\n"
        "\r\n"
        "; 0 - off, 1 - on\r\n"
        "; Removes letterboxing (black bars) from normal gameplay, e.g. from\r\n"
        "; the in-game Letterbox option, so gameplay always fills the screen.\r\n"
        "NoGameplayLetterbox=1\r\n"
        "\r\n"
        "; 0 - off, 1 - on\r\n"
        "; Cutscenes fill the whole screen instead of the game's ~2.35:1\r\n"
        "; letterbox. Keeps the cinematic vertical framing, crops the sides.\r\n"
        "FillCutscenes=0\r\n"
        "\r\n"
        "[Log]\r\n"
        "; 0 - off, 1 - on\r\n"
        "; Writes Townfall43.log next to the DLL. Only useful for\r\n"
        "; troubleshooting; keep off during normal play.\r\n"
        "Enabled=0\r\n";

    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(f, kDefaultIni, sizeof(kDefaultIni) - 1, &written, nullptr);
    CloseHandle(f);
    LogLine("default Townfall43.ini created");
}

static DWORD WINAPI InitThread(LPVOID) {
    HMODULE game = GetModuleHandleW(nullptr);
    if (!game) {
        LogLine("ERROR: game module not found");
        return 1;
    }
    if (ConfigInt(L"Camera", L"Enabled", 1) == 0) {
        LogLine("mod disabled via Townfall43.ini");
        return 0;
    }
    if (InstallCameraHooks(game))
        LogLine("OK: camera hook installed");
    else
        LogLine("ERROR: camera hook not installed (game build not recognised)");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        LogLine("%s proxy attached (pid %lu)", PROXY_TARGET_DLL, GetCurrentProcessId());
        EnsureDefaultIni();
        HANDLE h = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
