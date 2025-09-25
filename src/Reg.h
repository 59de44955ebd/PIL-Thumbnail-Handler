#pragma once
#include <windows.h>

HRESULT RegisterInprocServer(PCWSTR pszModule, const CLSID& clsid, 
    PCWSTR pszFriendlyName, PCWSTR pszThreadModel, const GUID& appId);
HRESULT UnregisterInprocServer(const CLSID& clsid, const GUID& appId);
HRESULT RegisterShellExtThumbnailHandler(PCWSTR pszFileType, const CLSID& clsid);
HRESULT UnregisterShellExtThumbnailHandler(PCWSTR pszFileType);
