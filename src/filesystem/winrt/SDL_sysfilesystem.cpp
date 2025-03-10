/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.
  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:
  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

/* TODO, WinRT: remove the need to compile this with C++/CX (/ZW) extensions, and if possible, without C++ at all
 */

#ifdef SDL_PLATFORM_WINRT

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// System dependent filesystem routines

extern "C" {
#include "../../core/windows/SDL_windows.h"
#include "../SDL_sysfilesystem.h"
}

#include <string>
#include <unordered_map>

using namespace std;
using namespace Windows::Storage;

static const wchar_t *SDL_GetWinRTFSPathUNICODE(SDL_WinRT_Path pathType)
{
    switch (pathType) {
    case SDL_WINRT_PATH_INSTALLED_LOCATION:
    {
        static wstring path;
        if (path.empty()) {
#if defined(NTDDI_WIN10_19H1) && (NTDDI_VERSION >= NTDDI_WIN10_19H1) && (WINAPI_FAMILY == WINAPI_FAMILY_PC_APP) // Only PC supports mods
            // Windows 1903 supports mods, via the EffectiveLocation API
            if (Windows::Foundation::Metadata::ApiInformation::IsApiContractPresent("Windows.Foundation.UniversalApiContract", 8, 0)) {
                path = Windows::ApplicationModel::Package::Current->EffectiveLocation->Path->Data();
            } else {
                path = Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data();
            }
#else
            path = Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data();
#endif
        }
        return path.c_str();
    }

    case SDL_WINRT_PATH_LOCAL_FOLDER:
    {
        static wstring path;
        if (path.empty()) {
            path = ApplicationData::Current->LocalFolder->Path->Data();
        }
        return path.c_str();
    }

#if !SDL_WINAPI_FAMILY_PHONE || NTDDI_VERSION > NTDDI_WIN8
    case SDL_WINRT_PATH_ROAMING_FOLDER:
    {
        static wstring path;
        if (path.empty()) {
            path = ApplicationData::Current->RoamingFolder->Path->Data();
        }
        return path.c_str();
    }

    case SDL_WINRT_PATH_TEMP_FOLDER:
    {
        static wstring path;
        if (path.empty()) {
            path = ApplicationData::Current->TemporaryFolder->Path->Data();
        }
        return path.c_str();
    }
#endif

    default:
        break;
    }

    SDL_Unsupported();
    return NULL;
}

extern "C" const char *SDL_GetWinRTFSPath(SDL_WinRT_Path pathType)
{
    typedef unordered_map<SDL_WinRT_Path, string> UTF8PathMap;
    static UTF8PathMap utf8Paths;

    UTF8PathMap::iterator searchResult = utf8Paths.find(pathType);
    if (searchResult != utf8Paths.end()) {
        return searchResult->second.c_str();
    }

    const wchar_t *ucs2Path = SDL_GetWinRTFSPathUNICODE(pathType);
    if (!ucs2Path) {
        return NULL;
    }

    char *utf8Path = WIN_StringToUTF8W(ucs2Path);
    utf8Paths[pathType] = utf8Path;
    SDL_free(utf8Path);
    return SDL_GetPersistentString(utf8Paths[pathType].c_str());
}

extern "C" char *SDL_SYS_GetBasePath(void)
{
    const char *srcPath = SDL_GetWinRTFSPath(SDL_WINRT_PATH_INSTALLED_LOCATION);
    size_t destPathLen;
    char *destPath = NULL;

    if (!srcPath) {
        SDL_SetError("Couldn't locate our basepath: %s", SDL_GetError());
        return NULL;
    }

    destPathLen = SDL_strlen(srcPath) + 2;
    destPath = (char *)SDL_malloc(destPathLen);
    if (!destPath) {
        return NULL;
    }

    SDL_snprintf(destPath, destPathLen, "%s\\", srcPath);
    return destPath;
}

extern "C" char *SDL_SYS_GetPrefPath(const char *org, const char *app)
{
    /* WinRT note: The 'SHGetFolderPath' API that is used in Windows 7 and
     * earlier is not available on WinRT or Windows Phone.  WinRT provides
     * a similar API, but SHGetFolderPath can't be called, at least not
     * without violating Microsoft's app-store requirements.
     */

    const WCHAR *srcPath = NULL;
    WCHAR path[MAX_PATH];
    char *result = NULL;
    WCHAR *worg = NULL;
    WCHAR *wapp = NULL;
    size_t new_wpath_len = 0;
    BOOL api_result = FALSE;

    if (!app) {
        SDL_InvalidParamError("app");
        return NULL;
    }
    if (!org) {
        org = "";
    }

    srcPath = SDL_GetWinRTFSPathUNICODE(SDL_WINRT_PATH_LOCAL_FOLDER);
    if (!srcPath) {
        SDL_SetError("Unable to find a source path");
        return NULL;
    }

    if (SDL_wcslen(srcPath) >= MAX_PATH) {
        SDL_SetError("Path too long.");
        return NULL;
    }
    SDL_wcslcpy(path, srcPath, SDL_arraysize(path));

    worg = WIN_UTF8ToStringW(org);
    if (!worg) {
        return NULL;
    }

    wapp = WIN_UTF8ToStringW(app);
    if (!wapp) {
        SDL_free(worg);
        return NULL;
    }

    new_wpath_len = SDL_wcslen(worg) + SDL_wcslen(wapp) + SDL_wcslen(path) + 3;

    if ((new_wpath_len + 1) > MAX_PATH) {
        SDL_free(worg);
        SDL_free(wapp);
        SDL_SetError("Path too long.");
        return NULL;
    }

    if (*worg) {
        SDL_wcslcat(path, L"\\", new_wpath_len + 1);
        SDL_wcslcat(path, worg, new_wpath_len + 1);
        SDL_free(worg);
    }

    api_result = CreateDirectoryW(path, NULL);
    if (api_result == FALSE) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            SDL_free(wapp);
            WIN_SetError("Couldn't create a prefpath.");
            return NULL;
        }
    }

    SDL_wcslcat(path, L"\\", new_wpath_len + 1);
    SDL_wcslcat(path, wapp, new_wpath_len + 1);
    SDL_free(wapp);

    api_result = CreateDirectoryW(path, NULL);
    if (api_result == FALSE) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            WIN_SetError("Couldn't create a prefpath.");
            return NULL;
        }
    }

    SDL_wcslcat(path, L"\\", new_wpath_len + 1);

    result = WIN_StringToUTF8W(path);

    return result;
}

char *SDL_SYS_GetUserFolder(SDL_Folder folder)
{
    wstring wpath;

    switch (folder) {
        #define CASEPATH(sym, var) case sym: wpath = Windows::Storage::UserDataPaths::GetDefault()->var->Data(); break
        CASEPATH(SDL_FOLDER_HOME, Profile);
        CASEPATH(SDL_FOLDER_DESKTOP, Desktop);
        CASEPATH(SDL_FOLDER_DOCUMENTS, Documents);
        CASEPATH(SDL_FOLDER_DOWNLOADS, Downloads);
        CASEPATH(SDL_FOLDER_MUSIC, Music);
        CASEPATH(SDL_FOLDER_PICTURES, Pictures);
        CASEPATH(SDL_FOLDER_SCREENSHOTS, Screenshots);
        CASEPATH(SDL_FOLDER_TEMPLATES, Templates);
        CASEPATH(SDL_FOLDER_VIDEOS, Videos);
        #undef CASEPATH
        #define UNSUPPPORTED_CASEPATH(sym) SDL_SetError("The %s folder is unsupported on WinRT", #sym); return NULL;
        UNSUPPPORTED_CASEPATH(SDL_FOLDER_PUBLICSHARE);
        UNSUPPPORTED_CASEPATH(SDL_FOLDER_SAVEDGAMES);
        #undef UNSUPPPORTED_CASEPATH
        default:
            SDL_SetError("Invalid SDL_Folder: %d", (int)folder);
            return NULL;
    };

    wpath += L"\\";

    return WIN_StringToUTF8W(wpath.c_str());
}

// TODO
char *SDL_SYS_GetCurrentDirectory(void)
{
    SDL_Unsupported();
    return NULL;
}

#endif // SDL_PLATFORM_WINRT
