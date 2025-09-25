#include "ClassFactory.h"

#include "ThumbnailProvider.h"

#include <new>
#include <Shlwapi.h>

#include "Config.h"

extern long g_cRefDll;

ClassFactory::ClassFactory() : m_cRef(1)
{
  InterlockedIncrement(&g_cRefDll);
}

ClassFactory::~ClassFactory()
{
  InterlockedDecrement(&g_cRefDll);
}

//
// IUnknown
//

IFACEMETHODIMP ClassFactory::QueryInterface(REFIID riid, void **ppv)
{
  static const QITAB qit[] =
  {
    QITABENT(ClassFactory, IClassFactory),
    { 0 },
  };
  return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) ClassFactory::AddRef()
{
  return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) ClassFactory::Release()
{
  ULONG cRef = InterlockedDecrement(&m_cRef);
  if (0 == cRef)
  {
    delete this;
  }
  return cRef;
}

// 
// IClassFactory
//

IFACEMETHODIMP ClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv)
{
  HRESULT hr = CLASS_E_NOAGGREGATION;

  // pUnkOuter is used for aggregation. We do not support it in the sample.
  if (pUnkOuter == NULL)
  {
    hr = E_OUTOFMEMORY;

    // Create the COM component.
    ThumbnailProvider *pExt = new (std::nothrow) ThumbnailProvider();
    if (pExt)
    {
      // Query the specified interface.
      hr = pExt->QueryInterface(riid, ppv);
      pExt->Release();
    }
  }

  return hr;
}

IFACEMETHODIMP ClassFactory::LockServer(BOOL fLock)
{
  if (fLock)
  {
    InterlockedIncrement(&g_cRefDll);
  }
  else
  {
    InterlockedDecrement(&g_cRefDll);
  }
  return S_OK;
}
