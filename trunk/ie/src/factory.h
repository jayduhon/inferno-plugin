class Cfactory : public Crefc, IClassFactory
{
public:
	// IUnknown
	ULONG _stdcall		AddRef() {return ++nref;};
	ULONG _stdcall		Release();
	HRESULT _stdcall	QueryInterface(REFIID iid, void** v);

	// IClassFactory
	HRESULT _stdcall	CreateInstance(IUnknown* outer, REFIID iid, void **v);
	HRESULT _stdcall	LockServer(BOOL l);

	Cfactory() { nref = 1;};

private:
	ULONG nref;
};
