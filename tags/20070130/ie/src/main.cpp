#include <windows.h>
#include "main.h"
#include "factory.h"


BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
//	if (fdwReason == DLL_PROCESS_ATTACH) {
//		_emu_tls_index = TlsAlloc();
//		_emu_tls_siteindex = TlsAlloc();
//	}
	return TRUE;
}

ULONG grefc = 0;	// global ref count for all objs/interfaces

HRESULT __stdcall
DllGetClassObject(REFCLSID clsid, REFIID iid, void** v)
{
	if (clsid != emuclsid)
		return CLASS_E_CLASSNOTAVAILABLE;

	Cfactory *f = new Cfactory;
	if (f == NULL) {
		MessageBox(NULL, "no memory!", "DEBUG", MB_OK);
		return E_OUTOFMEMORY;
	}

	HRESULT hr = f->QueryInterface(iid, v);
	f->Release();
	return hr;
}

HRESULT __stdcall
DllCanUnloadNow()
{
	if (grefc == 0)
		return S_OK;
	return S_FALSE;
}

IOleObject *
ObjFromStream(void *stream)
{
	IOleObject *obj;
	HRESULT hr;
	hr = CoGetInterfaceAndReleaseStream((IStream*)stream, IID_IOleObject, (PVOID *)&obj);
	return obj;
}

IStream *
StreamFromObj(void *obj)
{
	IStream *s;
	HRESULT hr;
	hr = CoMarshalInterThreadInterfaceInStream(IID_IOleObject, (IUnknown*)obj, &s);
	return s;
}
