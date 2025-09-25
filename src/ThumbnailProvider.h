#pragma once

#include <windows.h>
#include <thumbcache.h>


class ThumbnailProvider : 
  public IInitializeWithFile, 
  public IThumbnailProvider
{
public:
  // IUnknown
  IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
  IFACEMETHODIMP_(ULONG) AddRef();
  IFACEMETHODIMP_(ULONG) Release();

  // IInitializeWithFile
  IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode);

  // IThumbnailProvider
  IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha);

  ThumbnailProvider();

protected:
  ~ThumbnailProvider();

private:
  // Reference count of component.
  long m_cRef;

  // Provided during initialization.
  LPWSTR m_pPathFile;
};
