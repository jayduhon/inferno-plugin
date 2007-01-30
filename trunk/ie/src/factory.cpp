#include <windows.h>
#include <comdef.h>
#include "main.h"
#include "factory.h"
#include "emu.h"

ULONG __stdcall
Cfactory::Release()
{
	if (--nref != 0)
		return nref;
	delete this;
	return 0;
}

HRESULT __stdcall
Cfactory::QueryInterface(REFIID iid, void** v)
{
	if (iid != IID_IUnknown && iid != IID_IClassFactory) {
		*v = NULL;
		return E_NOINTERFACE;
	}
	*v = this;
	AddRef();
	return S_OK;
}

static int emuid;

HRESULT __stdcall
Cfactory::CreateInstance(IUnknown* outer, REFIID iid, void **v)
{
	if (outer != NULL)
		return CLASS_E_NOAGGREGATION;

	Cemu *emu = new Cemu(emuid++);
	if (emu == NULL)
		return E_OUTOFMEMORY;

	HRESULT hr = emu->QueryInterface(iid, v);
	emu->Release();
	return hr;
}

HRESULT __stdcall
Cfactory::LockServer(BOOL l)
{
	if (l)
		nref++;
	else
		nref--;
	return S_OK;
}

