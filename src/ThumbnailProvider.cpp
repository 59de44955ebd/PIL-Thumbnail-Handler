#include "ThumbnailProvider.h"

#include <Shlwapi.h>
#include <stdio.h>
#include <atlstr.h>

extern HINSTANCE g_hInstDll;
extern long g_cRefDll;

HBITMAP GetPilThumbnail(const wchar_t* pilFile, UINT thumbSize, BOOL *hasAlpha);


ThumbnailProvider::ThumbnailProvider()
	: m_cRef(1), m_pPathFile(NULL)
{
	InterlockedIncrement(&g_cRefDll);
}

ThumbnailProvider::~ThumbnailProvider()
{
	if (m_pPathFile)
	{
		LocalFree(m_pPathFile);
		m_pPathFile = NULL;
	}

	InterlockedDecrement(&g_cRefDll);
}

// Query to	the	interface the component	supported.
IFACEMETHODIMP ThumbnailProvider::QueryInterface(REFIID riid, void **ppv)
{
	static const QITAB qit[] =
	{
		QITABENT(ThumbnailProvider, IThumbnailProvider),
		QITABENT(ThumbnailProvider, IInitializeWithFile),
		{ 0 },
	};
	return QISearch(this, qit, riid, ppv);
}

// Increase	the	reference count	for	an interface on	an object.
IFACEMETHODIMP_(ULONG) ThumbnailProvider::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

// Decrease	the	reference count	for	an interface on	an object.
IFACEMETHODIMP_(ULONG) ThumbnailProvider::Release()
{
	ULONG cRef = InterlockedDecrement(&m_cRef);
	if (0 == cRef)
	{
		delete this;
	}

	return cRef;
}

IFACEMETHODIMP ThumbnailProvider::Initialize(LPCWSTR pszFilePath, DWORD grfMode)
{
	HRESULT hr = E_INVALIDARG;

	if (pszFilePath)
	{
		// Initialize can be called	more than once,	so release existing	valid
		// m_pStream.
		if (m_pPathFile)
		{
			LocalFree(m_pPathFile);
			m_pPathFile = NULL;
		}

		m_pPathFile = StrDup(pszFilePath);
		hr = S_OK;
	}
	return hr;
}

///////////////////////////////////////////////////////////

// Gets	a thumbnail	image and alpha	type. The GetThumbnail is called with the
// largest desired size	of the image, in pixels. Although the parameter	is
// called cx, this is used as the maximum size of both the x and y dimensions.
// If the retrieved	thumbnail is not square, then the longer axis is limited
// by cx and the aspect	ratio of the original image	respected. On exit,
// GetThumbnail	provides a handle to the retrieved image. It also provides a
// value that indicates	the	color format of	the	image and whether it has
// valid alpha information.
IFACEMETHODIMP ThumbnailProvider::GetThumbnail(UINT thumbSize, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
{
	BOOL hasAlpha;
	*phbmp = GetPilThumbnail(m_pPathFile, thumbSize, &hasAlpha);
	*pdwAlpha = hasAlpha ? WTSAT_ARGB : WTSAT_RGB;
	return S_OK;
}
