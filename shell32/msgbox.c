#include "shellprv.h"
#pragma  hdrstop

#include "uemapp.h"

#define MAXRCSTRING 514

// Major hack follows to get this to work with the DEBUG alloc/free -- on NT
// Local and Global heap functions evaluate to the same heap.  The problem
// here is that LocalFree gets mapped to DebugLocalFree, but when we
// call FormatMessage, the buffer is not allocated through DebugLocalAlloc,
// so it dies.
//
#ifdef DEBUG
#undef LocalFree
#define LocalFree GlobalFree
#endif

// this will check to see if lpcstr is a resource id or not.  if it
// is, it will return a LPSTR containing the loaded resource.
// the caller must LocalFree this lpstr.  if pszText IS a string, it
// will return pszText
//
// returns:
//      pszText if it is already a string
//      or
//      LocalAlloced() memory to be freed with LocalFree
//      if pszRet != pszText free pszRet

LPTSTR WINAPI ResourceCStrToStr(HINSTANCE hInst, LPCTSTR pszText)
{
    TCHAR szTemp[MAXRCSTRING];
    LPTSTR pszRet = NULL;

    if (!IS_INTRESOURCE(pszText))
        return (LPTSTR)pszText;

    if (LOWORD((DWORD_PTR)pszText) && LoadString(hInst, LOWORD((DWORD_PTR)pszText), szTemp, ARRAYSIZE(szTemp)))
    {
        pszRet = LocalAlloc(LPTR, (lstrlen(szTemp) + 1) * SIZEOF(TCHAR));
        if (pszRet)
            lstrcpy(pszRet, szTemp);
    }
    return pszRet;
}
#ifdef UNICODE
LPSTR ResourceCStrToStrA(HINSTANCE hInst, LPCSTR pszText)
{
    CHAR szTemp[MAXRCSTRING];
    LPSTR pszRet = NULL;

    if (!IS_INTRESOURCE(pszText))
        return (LPSTR)pszText;

    if (LOWORD((DWORD_PTR)pszText) && LoadStringA(hInst, LOWORD((DWORD_PTR)pszText), szTemp, ARRAYSIZE(szTemp)))
    {
        pszRet = LocalAlloc(LPTR, (lstrlenA(szTemp) + 1) * SIZEOF(CHAR));
        if (pszRet)
            lstrcpyA(pszRet, szTemp);
    }
    return pszRet;
}
#else
LPWSTR ResourceCStrToStrW(HINSTANCE hInst, LPCWSTR pszText)
{
    return NULL;        // Error condition
}
#endif

LPTSTR _ConstructMessageString(HINSTANCE hInst, LPCTSTR pszMsg, va_list *ArgList)
{
    LPTSTR pszRet;
    LPTSTR pszRes = ResourceCStrToStr(hInst, pszMsg);
    if (!pszRes)
    {
        DebugMsg(DM_ERROR, TEXT("_ConstructMessageString: Failed to load string template"));
        return NULL;
    }

    if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING,
                       pszRes, 0, 0, (LPTSTR)&pszRet, 0, ArgList))
    {
        DebugMsg(DM_ERROR, TEXT("_ConstructMessageString: FormatMessage failed %d"),GetLastError());
        DebugMsg(DM_ERROR, TEXT("                         pszRes = %s"), pszRes );
        DebugMsg(DM_ERROR, !IS_INTRESOURCE(pszMsg) ? 
        TEXT("                         pszMsg = %s") : 
        TEXT("                         pszMsg = 0x%x"), pszMsg );
        pszRet = NULL;
    }

    if (pszRes != pszMsg)
        LocalFree(pszRes);

    return pszRet;      // free with LocalFree()
}

#ifdef UNICODE
LPSTR _ConstructMessageStringA(HINSTANCE hInst, LPCSTR pszMsg, va_list *ArgList)
{
    LPSTR pszRet;
    LPSTR pszRes = ResourceCStrToStrA(hInst, pszMsg);
    if (!pszRes)
    {
        DebugMsg(DM_ERROR, TEXT("_ConstructMessageString: Failed to load string template"));
        return NULL;
    }

    if (!FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING,
                       pszRes, 0, 0, (LPSTR)&pszRet, 0, ArgList))
    {
        DebugMsg(DM_ERROR, TEXT("_ConstructMessageString: FormatMessage failed %d"),GetLastError());
        DebugMsg(DM_ERROR, TEXT("                         pszRes = %S"), pszRes );

        DebugMsg(DM_ERROR, !IS_INTRESOURCE(pszMsg) ? 
        TEXT("                         pszMsg = %s") : 
        TEXT("                         pszMsg = 0x%x"), pszMsg );
        pszRet = NULL;
    }

    if (pszRes != pszMsg)
        LocalFree(pszRes);

    return pszRet;      // free with LocalFree()
}
#else
LPWSTR _ConstructMessageStringW(HINSTANCE hInst, LPCWSTR pszMsg, va_list *ArgList)
{
    return NULL;    // Error condition
}
#endif


// NOTE: ShellMessageBoxW has been re-implemented in shlwapi, so shell32 redirects that api there
// Shlwapi doesn't need an A version (because it's in shell32), so we're leaving this code here...
// 
int WINCAPI ShellMessageBoxA(HINSTANCE hInst, HWND hWnd, LPCSTR pszMsg, LPCSTR pszTitle, UINT fuStyle, ...)
{
    LPSTR pszText;
    int result;
    CHAR szBuffer[80];
    va_list ArgList;

    // BUG 95214
#ifdef DEBUG
    IUnknown* punk = NULL;
    if (SUCCEEDED(SHGetThreadRef(&punk)) && punk)
    {
        ASSERTMSG(hWnd != NULL, "shell32\\msgbox.c : ShellMessageBoxA - Caller should either be not under a browser or should have a parent hwnd");
        punk->lpVtbl->Release(punk);
    }
#endif

    if (!IS_INTRESOURCE(pszTitle))
    {
        // do nothing
    }
    else if (LoadStringA(hInst, LOWORD((DWORD_PTR)pszTitle), szBuffer, ARRAYSIZE(szBuffer)))
    {
        // Allow this to be a resource ID or NULL to specifiy the parent's title
        pszTitle = szBuffer;
    }
    else if (hWnd)
    {
        // Grab the title of the parent
        GetWindowTextA(hWnd, szBuffer, ARRAYSIZE(szBuffer));

        //
        //  we would rather not use the "Desktop" as our title,
        //  but sometimes that is the window that is used, and we dont
        //  have a better title.  callers should review to make sure that
        //  they want "Desktop" as the title to the dialog in 
        //  the case that the hwnd passed in is the desktop window.
        //
        if (!lstrcmpA(szBuffer, "Program Manager")) 
        {
            LoadStringA(HINST_THISDLL, IDS_DESKTOP, szBuffer, ARRAYSIZE(szBuffer));
            pszTitle = szBuffer;
            DebugMsg(TF_WARNING, TEXT("No caption for SHMsgBox() on desktop"));
        } 
        else
            pszTitle = szBuffer;
    }
    else
    {
        pszTitle = "";
    }

    va_start(ArgList, fuStyle);
    pszText = _ConstructMessageStringA(hInst, pszMsg, &ArgList);
    va_end(ArgList);

    if (pszText)
    {
        result = MessageBoxA(hWnd, pszText, pszTitle, fuStyle | MB_SETFOREGROUND);
        LocalFree(pszText);
    }
    else
    {
        DebugMsg(DM_ERROR, TEXT("smb: Not enough memory to put up dialog."));
        result = -1;    // memory failure
    }

    return result;
}


//
// returns:
//      pointer to formatted string, free this with SHFree() (not Free())
//

LPTSTR WINCAPI ShellConstructMessageString(HINSTANCE hInst, LPCTSTR pszMsg, ...)
{
    LPTSTR pszRet;
    va_list ArgList;

    va_start(ArgList, pszMsg);

    pszRet = _ConstructMessageString(hInst, pszMsg, &ArgList);

    va_end(ArgList);

    return pszRet;      // free with SHFree()
}

