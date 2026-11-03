#include "ui_folder_picker.h"
#include <string.h>

#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>

/* Older MinGW header sets hide the newer browse-dialog flags behind _WIN32_IE.
   The values are Shell API constants, so defining them here is harmless. */
#ifndef BIF_NEWDIALOGSTYLE
#define BIF_NEWDIALOGSTYLE 0x0040
#endif
#ifndef BIF_NONEWFOLDERBUTTON
#define BIF_NONEWFOLDERBUTTON 0x0200
#endif

/* Windows' own folder-browse dialog. It lives in shell32/ole32 (system DLLs,
   no new third-party library); the caller links those two import libraries.
   The wide API plus an explicit UTF-8 conversion keeps paths with umlauts or
   other non-ASCII characters usable on a German Windows install. */
int ui_pick_music_folder(char *out, size_t cap) {
    if (!out || cap < 2) return -1;
    out[0] = '\0';
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    int need_uninit = (hr == S_OK || hr == S_FALSE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return -1;

    wchar_t title[] = L"Pick a folder with match music (mp3 / ogg / wav)";
    BROWSEINFOW info;
    memset(&info, 0, sizeof(info));
    info.hwndOwner = GetForegroundWindow(); /* the game window owns the dialog */
    info.lpszTitle = title;
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;

    int result = 0;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&info);
    if (pidl) {
        wchar_t path[1024];
        if (SHGetPathFromIDListW(pidl, path)) {
            int n = WideCharToMultiByte(CP_UTF8, 0, path, -1, out, (int)cap, NULL, NULL);
            if (n > 0) result = 1;
            else out[0] = '\0';
        }
        CoTaskMemFree(pidl);
    }
    if (need_uninit) CoUninitialize();
    return result;
}

#else /* non-Windows / browser: the in-game folder list is the fallback */

int ui_pick_music_folder(char *out, size_t cap) {
    if (out && cap) out[0] = '\0';
    return -1;
}

#endif
