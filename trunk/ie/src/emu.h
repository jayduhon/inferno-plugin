typedef struct PluginFile PluginFile;

#define PF_MSGLEN	64

struct PluginFile {
	char *url;
	char *spath;
	char *dname;
	HANDLE	fetcher;
	IStream *stream;		// For marshalling Cemu obj to fetcher thread
	char status[PF_MSGLEN + 1];
};

class CInferno;

class Cemu : public Crefc, IObjectWithSite, IOleInPlaceActiveObject, IOleInPlaceObjectWindowless,
					IViewObjectEx, IOleObject, IOleControl, IPersistPropertyBag
{
public:
	// IUnknown
	ULONG _stdcall		AddRef() {return ++nref;};
	ULONG _stdcall		Release();
	HRESULT _stdcall	QueryInterface(REFIID iid, void **v);

	// IObjectWithSite
	HRESULT _stdcall	SetSite(IUnknown *s);
	HRESULT _stdcall	GetSite(REFIID iid, void **v);

	// IOleWindow
	HRESULT _stdcall	GetWindow(HWND *w);
	HRESULT _stdcall	ContextSensitiveHelp(BOOL enter);

	// IOleInPlaceActiveObject
	HRESULT _stdcall	TranslateAccelerator(LPMSG lpmsg);
	HRESULT _stdcall	OnFrameWindowActivate(BOOL fActivate);
	HRESULT _stdcall	OnDocWindowActivate(BOOL fActivate);
	HRESULT _stdcall	ResizeBorder(LPCRECT prcBorder, IOleInPlaceUIWindow __RPC_FAR *pUIWindow, BOOL fFrameWindow);
	HRESULT _stdcall	EnableModeless(BOOL fEnable);

	// IOleInPlaceObjectWindowless
//	HRESULT _stdcall	GetWindow(HWND *w);
//	HRESULT _stdcall	ContextSensitiveHelp(BOOL enter);
	HRESULT _stdcall	InPlaceDeactivate();
	HRESULT _stdcall	UIDeactivate();
	HRESULT _stdcall	SetObjectRects(LPCRECT posr, LPCRECT clipr);
	HRESULT _stdcall	ReactivateAndUndo();
	HRESULT _stdcall	OnWindowMessage(UINT msg, WPARAM w, LPARAM l, LRESULT *r);
	HRESULT _stdcall	GetDropTarget(IDropTarget **dt);

	// IViewObjectEx
	HRESULT _stdcall	Draw(DWORD aspect, LONG lindex, void *paspect,
							DVTARGETDEVICE *ptd, HDC targetdc, HDC drawdc,
							LPCRECTL brect, LPCRECTL wbrect,
							BOOL (_stdcall *Cont)(DWORD), DWORD Contarg);
	HRESULT _stdcall	GetColorSet(DWORD aspect, LONG lindex, void *paspect,
							DVTARGETDEVICE *ptd, HDC targetdc, LOGPALETTE **colours);
	HRESULT _stdcall	Freeze(DWORD aspect, LONG lindex, void *paspect, DWORD *freeze);
	HRESULT _stdcall	Unfreeze(DWORD freeze);
	HRESULT _stdcall	SetAdvise(DWORD aspects, DWORD advf, IAdviseSink *pAdvSink);
	HRESULT _stdcall	GetAdvise(DWORD *aspects, DWORD *pAdvf, IAdviseSink **ppAdvSink);

	// IViewObject2
	HRESULT _stdcall	GetExtent(DWORD dwDrawAspect, LONG lindex, DVTARGETDEVICE __RPC_FAR *ptd, LPSIZEL lpsizel);

	// IViewObjectEx
	HRESULT _stdcall	GetRect(DWORD dwAspect, LPRECTL pRect);
	HRESULT _stdcall	GetViewStatus(DWORD __RPC_FAR *pdwStatus);
	HRESULT _stdcall	QueryHitPoint(DWORD dwAspect, LPCRECT pRectBounds, POINT ptlLoc, LONG lCloseHint, DWORD __RPC_FAR *pHitResult);
	HRESULT _stdcall	QueryHitRect(DWORD dwAspect, LPCRECT pRectBounds, LPCRECT pRectLoc, LONG lCloseHint, DWORD __RPC_FAR *pHitResult);
	HRESULT _stdcall	GetNaturalExtent(DWORD dwAspect, LONG lindex, DVTARGETDEVICE __RPC_FAR *ptd, HDC hicTargetDev, DVEXTENTINFO __RPC_FAR *pExtentInfo, LPSIZEL pSizel);

	
	// IOleObject
	HRESULT _stdcall	SetClientSite(IOleClientSite *pClientSite);
	HRESULT _stdcall	GetClientSite(IOleClientSite **ppClientSite);
	HRESULT _stdcall	SetHostNames(LPCOLESTR app, LPCOLESTR obj);
	HRESULT _stdcall	Close(DWORD save);
	HRESULT _stdcall	SetMoniker(DWORD which, IMoniker *pmk);
	HRESULT _stdcall	GetMoniker(DWORD dwAssign, DWORD which, IMoniker **ppmk);
	HRESULT _stdcall	InitFromData(IDataObject *data, BOOL creat, DWORD dwReserved);
	HRESULT _stdcall	GetClipboardData(DWORD dwReserved, IDataObject **ppDataObject);
	HRESULT _stdcall	DoVerb(LONG iVerb, LPMSG lpmsg, IOleClientSite *pActiveSite,
							LONG lindex, HWND hwndParent, LPCRECT lprcPosRect);
	HRESULT _stdcall	EnumVerbs(IEnumOLEVERB **ppEnumOleVerb);
	HRESULT _stdcall	Update(void);
	HRESULT _stdcall	IsUpToDate(void);
	HRESULT _stdcall	GetUserClassID(CLSID *pClsid);
	HRESULT _stdcall	GetUserType(DWORD dwFormOfType, LPOLESTR *pszUserType);
	HRESULT _stdcall	SetExtent(DWORD dwDrawAspect, SIZEL *psizel);
	HRESULT _stdcall	GetExtent(DWORD dwDrawAspect, SIZEL *psizel);
	HRESULT _stdcall	Advise(IAdviseSink *pAdvSink, DWORD *pdwConnection);
	HRESULT _stdcall	Unadvise(DWORD dwConnection);
	HRESULT _stdcall	EnumAdvise(IEnumSTATDATA **ppenumAdvise);
	HRESULT _stdcall	GetMiscStatus(DWORD dwAspect, DWORD *pdwStatus);
	HRESULT _stdcall	SetColorScheme(LOGPALETTE *pLogpal);

	// IOleControl
	HRESULT _stdcall	GetControlInfo(CONTROLINFO *pCI);
	HRESULT _stdcall	OnMnemonic(MSG *pMsg);
	HRESULT _stdcall	OnAmbientPropertyChange(DISPID dispID);
	HRESULT _stdcall	FreezeEvents(BOOL bFreeze);

	// IPersistStreamInit
//	HRESULT _stdcall	GetClassID(CLSID *pClassID);
//	HRESULT _stdcall	IsDirty(void);
//	HRESULT _stdcall	Load(LPSTREAM pStm);
//	HRESULT _stdcall	Save(LPSTREAM pStm, BOOL fClearDirty);
//	HRESULT _stdcall	GetSizeMax(ULARGE_INTEGER *pCbSize);
//	HRESULT _stdcall	InitNew(void);
	// IPersistPropertyBag
	HRESULT _stdcall	GetClassID(CLSID *pClassID);
	HRESULT _stdcall	InitNew(void);
//	HRESULT _stdcall	IsDirty(void);
	HRESULT _stdcall	Load(IPropertyBag *pPropBag, IErrorLog *pErrorLog);
	HRESULT _stdcall	Save(IPropertyBag *pPropBag, BOOL fClearDirty, BOOL fSaveAllProperties);
//	HRESULT _stdcall	GetSizeMax(ULARGE_INTEGER *pCbSize);

	Cemu(int id);
	~Cemu();

	void PaintRect(HDC dc, RECT *r);

private:
	void	debug(char *msg);
	int	AddSink(IAdviseSink *s);
	void	uiactivate(IOleInPlaceSiteWindowless *sitewl, BOOL active);
	void	onsetfocus();
	void	onkillfocus();
	ULONG nref;
	IUnknown *site;
	IAdviseSink **sinks;
	int nsinks;
	SIZEL sizel;
	BOOL running;
	BOOL attached;
	BOOL activated;
	BOOL uiactivated;
	RECT posr, clipr;
	CInferno *inferno;
	// startup info
	int scrx;
	int scry;
	ULONG cdesc;
	char *init;			// inferno init shell command
	int nfiles;
	int cflag;
	PluginFile *files;	// files to fetch from network/cache
	char *status;
	ACCEL *accels;
	int id;
};
