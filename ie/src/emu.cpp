#include <windows.h>
#include <comdef.h>
#include <objbase.h>
#include <urlmon.h>
#include "main.h"
#include "emu.h"
#include "ieplugin.h"
#include "inferno.h"
#include "keyboard.h"
#include "drawchans.h"

extern void debug(char *);
static ULONG __stdcall fetcher(void *arg);

void
printhresult(char *p, HRESULT hr)
{
/*
	if (HRESULT_FACILITY(hr) == FACILITY_WINDOWS)
		hr = HRESULT_CODE(hr);
	LPVOID lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL, hr,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					(LPTSTR) &lpMsgBuf, 0, NULL );
	debug((char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
*/
}

void
Cemu::debug(char *msg)
{
	char buf[1024];
	if (inferno == NULL) {
//		wsprintf(buf, "[%d] %s", id, msg);
//		::debug(buf);
		return;
	} else {
		char *p;
		wsprintf(buf, "[%s]",msg);
		for (p = buf; *p; p++)
			inferno->keyboard(*p);
	}
}

Cemu::Cemu(int i)
{
	nref = 1;
	site = NULL;
	sinks = NULL;
	nsinks = 0;
	running = FALSE;
	attached = FALSE;
	activated = FALSE;
	uiactivated = FALSE;
	inferno = NULL;
	scrx = 640;
	scry = 480;
	cdesc = 0;		// auto-detect
	init = strdup("wm/wm");
	nfiles = 0;
	cflag = 0;
	files = NULL;
	status = NULL;
	accels = NULL;
//	accels = (ACCEL*)malloc(sizeof(ACCEL));
//	if (accels != NULL) {
//		accels->fVirt = FVIRTKEY;
//		accels->key = VK_SPACE;
//		accels->key = VK_BACK;
//		accels->cmd = 0;
//	}
//else debug("failed to alloc accels");
	id = i;
}

Cemu::~Cemu()
{
//	debug("~Cemu");
	if (inferno)
		delete inferno;
	if (site)
		site->Release();
	for (int i = 0; i < nsinks; i++)
		if (sinks[i])
			sinks[i]->Release();
	if (init)
		delete init;
	if (files) {
		while (nfiles--) {
			if (files[nfiles].fetcher)
				TerminateThread(files[nfiles].fetcher, 0);
			if (files[nfiles].url)
				delete files[nfiles].url;
			if (files[nfiles].dname)
				delete files[nfiles].dname;
			if (files[nfiles].spath)
				delete files[nfiles].spath;
		}
		delete files;
	}
	if (accels != NULL)
		delete accels;
}

int
Cemu::AddSink(IAdviseSink *s)
{
	int ix;
	for (ix = 0; ix < nsinks; ix++)
		if (sinks[ix] == NULL)
			break;
	if (ix >= nsinks) {
		int n = nsinks + 8;
		IAdviseSink **newsinks = (IAdviseSink **)realloc(sinks, sizeof(s) * n);
		if (newsinks == NULL)
			return -1;
		memset(newsinks+nsinks, 0, 8*sizeof(IAdviseSink*));
		sinks = newsinks;
		nsinks = n;
	}
	sinks[ix] = s;
	s->AddRef();
	return ix;
}


void
Cemu::PaintRect(HDC dc, RECT *r)
{
	if (site == NULL)
		return;
	
	IOleInPlaceSiteWindowless *sitewindowless = NULL;

	if (dc == NULL) {
		site->GetInterface(&sitewindowless);
		if (sitewindowless == NULL)
			return;

		HDC dc2;
		HRESULT hr = sitewindowless->GetDC(r, 0, &dc2);
		if (hr != S_OK) {
			printhresult("", hr);
			return;
		}
		dc = dc2;
	}

	IntersectClipRect(dc, clipr.left, clipr.top, clipr.right, clipr.bottom);

	LONG xoff = r->left - posr.left;
	LONG yoff = r->top - posr.top;
	LONG x = r->left;
	LONG y = r->top;
	LONG w = r->right - x;
	LONG h = r->bottom - y;

	if (inferno == NULL || !inferno->attached) {
		// show progress
		int n;
		SIZE sz;
		char *title = "Downloading files...";
		int tlen = strlen(title);

		SetTextColor(dc, RGB(0,0,0));
		SelectObject(dc, GetStockObject(ANSI_FIXED_FONT));
		SetBkMode(dc, TRANSPARENT);
		GetTextExtentPoint32(dc, title, tlen, &sz);
		FillRect(dc, r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
		if (nfiles) {
			TextOut(dc, x, y, title, tlen);
			y += sz.cy;
		}

		int maxdname = 0;
		for (n = 0; n < nfiles; n++) {
			int dnamel = strlen(files[n].dname);
			if (dnamel > maxdname)
				maxdname = dnamel;
		}
		char *buf = new char[maxdname + 3 + PF_MSGLEN];
		for (n = 0; n < nfiles; n++) {
			char *p = buf;
			int l = strlen(files[n].dname);
			strcpy(p, files[n].dname);
			p += l;
			while (p < (buf + maxdname + 3))
				*(p++) = '.';
			strcpy(p, files[n].status);
			l = strlen(buf);
			TextOut(dc, x, y, buf, l);
			y += sz.cy;
		}
		delete buf;
		char *statusmsg = status;
		if (statusmsg != NULL) {
			TextOut(dc, x, y, statusmsg, strlen(statusmsg));
			y += sz.cy;
		}
		statusmsg = NULL;
		if (inferno != NULL)
				statusmsg = inferno->status;
		if (statusmsg != NULL)
			TextOut(dc, x, y, statusmsg, strlen(statusmsg));

	} else {
		//	SelectPalette(dc, palette, 0);
		//	RealizePalette(dc);
		BitBlt(dc, x, y, w, h, inferno->screen, xoff, yoff, SRCCOPY);
	}
	if (sitewindowless) {
		sitewindowless->ReleaseDC(dc);
		sitewindowless->Release();
	}
}


// IUnknown
// ========

ULONG __stdcall
Cemu::Release()
{
	if (--nref != 0)
		return nref;
	delete this;
	return 0;
}

static HKEY ifkey = NULL;
static char * ifname(REFIID iid)
{
	static char buf[512];
	LPOLESTR s;

	// convert iid to string
	StringFromIID(iid, &s);
	wcstombs(buf, s, sizeof(buf));

	// lookup interface in registry
	if (ifkey == NULL) {
		RegOpenKey(HKEY_CLASSES_ROOT, "Interface", &ifkey);
		if (ifkey == NULL)
			return "internal error";
	}
	LONG n = sizeof(buf);
	RegQueryValue(ifkey, buf, buf, &n);
	return buf;
}

HRESULT _stdcall
Cemu::QueryInterface(REFIID iid, void **v)
{
	// High-level interfaces implemented by us
	if (iid == IID_IUnknown)
		*v = (IUnknown *)(IOleInPlaceObjectWindowless *)this;
	else if (iid == IID_IObjectWithSite)
		*v = (IObjectWithSite *)this;
	else if (iid == IID_IOleInPlaceActiveObject)
		*v = (IOleInPlaceActiveObject *)this;
	else if (iid == IID_IOleInPlaceObjectWindowless)
		*v = (IOleInPlaceObjectWindowless *)this;
	else if (iid == IID_IViewObjectEx)
		*v = (IViewObjectEx *)this;
	// base classes of IViewObjectEx
	else if (iid == IID_IViewObject2)
		*v = (IViewObject2 *)(IViewObjectEx *)this;
	else if (iid == IID_IViewObject)
		*v = (IViewObject *)(IViewObjectEx *)this;
	// IOleObject
	else if (iid == IID_IOleObject)
		*v = (IOleObject *)this;
	else if (iid == IID_IOleControl)
		*v = (IOleControl *)this;
	else if (iid == IID_IPersistPropertyBag)
		*v = (IPersistPropertyBag *)this;
	// base class of IPersistStreamInit
	else if (iid == IID_IPersist)
		*v = (IPersist *)(IPersistPropertyBag *)this;
	// base classes of IOleInPlaceObjectWindowless
	else if (iid == IID_IOleWindow)
		*v = (IOleWindow *)(IOleInPlaceObjectWindowless *)this;
	else if (iid == IID_IOleInPlaceObject)
		*v = (IOleInPlaceObject *)(IOleInPlaceObjectWindowless *)this;
	else {
		*v = NULL;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

// IObjectWithSite
// ===============

#include <stdio.h>
HRESULT _stdcall
Cemu::SetSite(IUnknown *s)
{
	if (s) {
		s->AddRef();
	}
	if (site) {
		site->Release();
		site = NULL;
	}
	site = s;
	return S_OK;
}

HRESULT _stdcall
Cemu::GetSite(REFIID iid, void **v)
{
	if (site)
		return site->QueryInterface(iid, v);
	*v = NULL;
	return E_FAIL;
}


// IOleInPlaceActiveObject
// =======================

HRESULT _stdcall
Cemu::TranslateAccelerator(LPMSG lpmsg)
{
	HRESULT hr = S_FALSE;
	IOleControlSite *ctlsite = NULL;
	if (site)
		site->GetInterface(&ctlsite);
	if (ctlsite) {
		DWORD keymod = 0;
		if (::GetKeyState(VK_SHIFT) < 0)
			keymod += 1;	// KEYMOD_SHIFT
		if (::GetKeyState(VK_CONTROL) < 0)
			keymod += 2;	// KEYMOD_CONTROL
		if (::GetKeyState(VK_MENU) < 0)
			keymod += 4;	// KEYMOD_ALT
		hr = ctlsite->TranslateAccelerator(lpmsg, keymod);
	}
	return (hr == S_OK) ? S_OK : S_FALSE;
}

HRESULT _stdcall
Cemu::OnFrameWindowActivate(BOOL fActivate)
{
	return S_OK;
}

HRESULT _stdcall
Cemu::OnDocWindowActivate(BOOL fActivate)
{
	if (!fActivate) {
		IOleInPlaceSiteWindowless *sitewl = NULL;
		if (site)
			site->GetInterface(&sitewl);
		if (sitewl) {
			uiactivate(sitewl, FALSE);
			sitewl->Release();
		}
	}
	return S_OK;
}

HRESULT _stdcall
Cemu::ResizeBorder(LPCRECT prcBorder, IOleInPlaceUIWindow __RPC_FAR *pUIWindow, BOOL fFrameWindow)
{
	return S_OK;
}

HRESULT _stdcall
Cemu::EnableModeless(BOOL fEnable)
{
	return S_OK;
}

// IOleInPlaceObjectWindowless
// ===========================

HRESULT _stdcall
Cemu::GetWindow(HWND *w)
{
	return E_FAIL;
}

HRESULT _stdcall
Cemu::ContextSensitiveHelp(BOOL enter)
{
	return S_OK;
}

HRESULT _stdcall
Cemu::InPlaceDeactivate()
{
	if (inferno)
		delete inferno;
	inferno = NULL;
	return S_OK;
}

HRESULT _stdcall
Cemu::UIDeactivate()
{
	IOleInPlaceSiteWindowless *sitewl = NULL;
	if (site)
		site->GetInterface(&sitewl);
	if (sitewl) {
		uiactivate(sitewl, FALSE);
		sitewl->Release();
	}
	return S_OK;
}

HRESULT _stdcall
Cemu::SetObjectRects(LPCRECT pposr, LPCRECT pclipr)
{
	posr = *pposr;
	clipr = *pclipr;
	PaintRect(NULL, (RECT *)pposr);
	return S_OK;
}

HRESULT _stdcall
Cemu::ReactivateAndUndo()
{
	return S_OK;
}

int
WindowMessage(CInferno *inferno, UINT msg, WPARAM wparam, LPARAM lparam)
{
	LONG x, y, b;

	switch(msg) {
	case WM_LBUTTONDBLCLK:
		b = (1<<8) | 1;
		goto process;
	case WM_MBUTTONDBLCLK:
		b = (1<<8) | 2;
		goto process;
	case WM_RBUTTONDBLCLK:
		b = (1<<8) | 4;
		goto process;
	case WM_MOUSEMOVE:
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		b = 0;
	process:
		x = (LONG)((SHORT)LOWORD(lparam));
		y = (LONG)((SHORT)HIWORD(lparam));
		if(wparam & MK_LBUTTON)
			b |= 1;
		if(wparam & MK_MBUTTON)
			b |= 2;
		if(wparam & MK_RBUTTON)
			if(wparam & MK_CONTROL)
				b |= 2;  //simulate middle button
			else
				b |= 4;  //right button
		inferno->mouse(x, y, b, 1);
		break;
	case WM_KEYDOWN:
		switch(wparam){
		default:
			return 0;
		case VK_HOME:
			wparam = Home;
			break;
		case VK_END:
			wparam = End;
			break;
		case VK_UP:
			wparam = Up;
			break;
		case VK_DOWN:
			wparam = Down;
			break;
		case VK_LEFT:
			wparam = Left;
			break;
		case VK_RIGHT:
			wparam = Right;
			break;
//		case VK_RETURN:
//			wparam = '\n';
//			break;
//		case VK_BACK:
//			wparam = '\b';
//			break;
		case VK_TAB:
			wparam = '\t';
			break;
		}
		/*FALLTHROUGH*/
	case WM_CHAR:
		switch(wparam) {
		case '\n':
		  	wparam = '\r';
		  	break;
		case '\r':
		  	wparam = '\n';
		  	break;
		}
		inferno->keyboard(wparam);
		break;
	case WM_SYSCHAR:
		inferno->keyboard(wparam&0xff);
		break;
	case WM_KEYUP:
		break;
	default:
		return 0;
	}
	return 1;
}

void
Cemu::uiactivate(IOleInPlaceSiteWindowless *sitewl, BOOL active)
{
	if (active == uiactivated)
		return;

	uiactivated = active;
	if (active) {
		sitewl->OnUIActivate();
		sitewl->SetFocus(TRUE);
	}

	IOleInPlaceFrame *ipframe = NULL;
	IOleInPlaceUIWindow *ipuiwin = NULL;
	RECT xr;
	OLEINPLACEFRAMEINFO frameinfo;
	frameinfo.cb = sizeof(frameinfo);
	if(sitewl->GetWindowContext(&ipframe, &ipuiwin, &xr, &xr, &frameinfo) == S_OK) {
		IOleInPlaceActiveObject *me = NULL;
		if (active)
			this->GetInterface(&me);
		if (ipframe) {
			ipframe->SetActiveObject(me, NULL);
			ipframe->Release();
		}
		if (ipuiwin) {
			ipuiwin->SetActiveObject(me, NULL);
			ipuiwin->Release();
		}
		if (me)
			me->Release();
	}

	if (!active)
		sitewl->OnUIDeactivate(FALSE);
}

void
Cemu::onsetfocus()
{
	IOleInPlaceSiteWindowless *sitewl = NULL;
	if (site)
		site->GetInterface(&sitewl);
	if (!sitewl)
		return;

	uiactivate(sitewl, TRUE);
	sitewl->Release();

	IOleControlSite *ctlsite = NULL;
	if (site)
		site->GetInterface(&ctlsite);
	if (ctlsite) {
		ctlsite->OnFocus(TRUE);
		ctlsite->Release();
	}
}

void
Cemu::onkillfocus()
{
	IOleControlSite *ctlsite = NULL;
	if (site)
		site->GetInterface(&ctlsite);
	if (ctlsite) {
		ctlsite->OnFocus(FALSE);
		ctlsite->Release();
	}
}

HRESULT _stdcall
Cemu::OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT *r)
{
	LONG x, y;
	LPARAM nlp = lparam;
	IOleInPlaceSiteWindowless *sitewindowless = NULL;
	BOOL handled = FALSE;
	LRESULT result = 0;

	if (site)
		site->GetInterface(&sitewindowless);

	switch (msg) {
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		handled = TRUE;
		break;
	case WM_PAINT:
		PaintRect(NULL, &posr);
		handled = TRUE;
		break;
	case WM_KILLFOCUS:
//		debug("WM_KILLFOCUS");
		onkillfocus();
		*r = 1;
		return S_FALSE;
	case WM_SETFOCUS:
//		debug("WM_SETFOCUS");
		onsetfocus();
		return S_FALSE;
	 case WM_KEYDOWN:
		if (wparam == VK_F8) {
			// show Inferno console output
			HWND hwnd = NULL;
			if (sitewindowless != NULL)
				sitewindowless->GetWindow(&hwnd);
			char *text = inferno->context();
			MessageBox(hwnd, text, "Inferno Console Output", MB_OK);
			delete text;
			handled = TRUE;
		}
		break;
	case WM_KEYUP:
		break;
	case WM_SYSCHAR:
		break;
	case WM_CHAR:
		break;
	case WM_RBUTTONDOWN:
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
		uiactivate(sitewindowless, TRUE);
		if (sitewindowless != NULL)
			sitewindowless->SetFocus(TRUE);
		if (sitewindowless != NULL)
			sitewindowless->SetCapture(TRUE);
		goto process;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		if (sitewindowless != NULL)
			sitewindowless->SetCapture(FALSE);
	case WM_LBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MOUSEMOVE:
process:
		x = (LONG)((SHORT)LOWORD(lparam) - posr.left);
		y = (LONG)((SHORT)HIWORD(lparam) - posr.top);
		nlp = MAKELONG(x, y);
		break;
	}

	if (!handled && inferno)
		handled = WindowMessage(inferno, msg, wparam, nlp);

	if (!handled && sitewindowless)
		handled = (sitewindowless->OnDefWindowMessage(msg, wparam, lparam, &result) == S_OK);
	if (sitewindowless)
		sitewindowless->Release();

	*r = result;
	if (handled)
		return S_OK;
	return S_FALSE;
}

HRESULT _stdcall
Cemu::GetDropTarget(IDropTarget **dt)
{
	*dt = NULL;
	return E_NOTIMPL;
}


// IViewObject
// ===========

HRESULT _stdcall
Cemu::Draw(DWORD aspect, LONG lindex, void *paspect, DVTARGETDEVICE *ptd,
	 HDC targetdc, HDC drawdc, LPCRECTL brect, LPCRECTL wbrect,
	 BOOL (_stdcall *Cont)(DWORD), DWORD Contarg)
{
	if (aspect == DVASPECT_CONTENT || aspect == DVASPECT_OPAQUE)
		// using drawdc seems to blow-up "ActiveX test container"
		PaintRect(drawdc, &posr);
	return S_OK;
}

HRESULT _stdcall
Cemu::GetColorSet(DWORD aspect, LONG lindex, void *paspect, DVTARGETDEVICE *ptd, HDC targetdc, LOGPALETTE **colours)
{
	*colours = NULL;
	return S_FALSE;
}

HRESULT _stdcall
Cemu::Freeze(DWORD aspect, LONG lindex, void *paspect, DWORD *freeze)
{
	return S_OK;
}

HRESULT _stdcall
Cemu::Unfreeze(DWORD freeze)
{
	return S_OK;
}

HRESULT _stdcall
Cemu::SetAdvise(DWORD aspects, DWORD advf, IAdviseSink *pAdvSink)
{
//	return OLE_E_ADVISENOTSUPPORTED;
// LIE!
	int id = AddSink(pAdvSink);
	if (id < 0)
		return E_OUTOFMEMORY;
	return S_OK;
}

HRESULT _stdcall
Cemu::GetAdvise(DWORD *aspects, DWORD *pAdvf, IAdviseSink **ppAdvSink)
{
	return OLE_E_ADVISENOTSUPPORTED;
}

HRESULT _stdcall
Cemu::GetExtent(DWORD dwDrawAspect, LONG lindex, DVTARGETDEVICE __RPC_FAR *ptd, LPSIZEL lpsizel)
{
	*lpsizel = sizel;
	return S_OK;
}

HRESULT _stdcall
Cemu::GetRect(DWORD dwAspect, LPRECTL pRect)
{
//	pRect->left = posr.left;
//	pRect->right = posr.right;
//	pRect->top = posr.top;
//	pRect->bottom = posr.bottom;
//	return S_OK;
	return E_NOTIMPL;
}

HRESULT _stdcall
Cemu::GetViewStatus(DWORD __RPC_FAR *pdwStatus)
{
	*pdwStatus = VIEWSTATUS_OPAQUE | VIEWSTATUS_SOLIDBKGND | VIEWSTATUS_DVASPECTOPAQUE;
	return S_OK;
}

HRESULT _stdcall
Cemu::QueryHitPoint(DWORD dwAspect, LPCRECT pRectBounds, POINT ptlLoc, LONG lCloseHint, DWORD __RPC_FAR *pHitResult)
{
	if (dwAspect != DVASPECT_CONTENT)
		return E_FAIL;
	if (ptlLoc.x >= pRectBounds->left && ptlLoc.x <= pRectBounds->right
		&& ptlLoc.y >= pRectBounds->top && ptlLoc.y <= pRectBounds->bottom)
		*pHitResult = HITRESULT_HIT;
	else
		*pHitResult = HITRESULT_OUTSIDE;
	return S_OK;
}

HRESULT _stdcall
Cemu::QueryHitRect(DWORD dwAspect, LPCRECT pRectBounds, LPCRECT pRectLoc, LONG lCloseHint, DWORD __RPC_FAR *pHitResult)
{
	return E_NOTIMPL;
}

HRESULT _stdcall
Cemu::GetNaturalExtent(DWORD dwAspect, LONG lindex, DVTARGETDEVICE __RPC_FAR *ptd, HDC hicTargetDev, DVEXTENTINFO __RPC_FAR *pExtentInfo, LPSIZEL pSizel)
{
	return E_NOTIMPL;
}


// IOleObject
// ==========

HRESULT _stdcall
Cemu::SetClientSite(IOleClientSite *pClientSite)
{
	return SetSite(pClientSite);
}

HRESULT _stdcall
Cemu::GetClientSite(IOleClientSite **ppClientSite)
{
	return GetSite(IID_IOleClientSite, (void**)ppClientSite);
}

HRESULT _stdcall
Cemu::SetHostNames(LPCOLESTR app, LPCOLESTR obj)
{
	return S_OK;
}

HRESULT _stdcall
Cemu::Close(DWORD save)
{
	return S_OK;
}

HRESULT _stdcall
Cemu::SetMoniker(DWORD which, IMoniker *pmk)
{
	return E_NOTIMPL;
}

HRESULT _stdcall
Cemu::GetMoniker(DWORD dwAssign, DWORD which, IMoniker **ppmk)
{
	return E_NOTIMPL;
}

HRESULT _stdcall
Cemu::InitFromData(IDataObject *data, BOOL creat, DWORD dwReserved)
{
	return E_NOTIMPL;
}

HRESULT _stdcall
Cemu::GetClipboardData(DWORD dwReserved, IDataObject **ppDataObject)
{
	return E_NOTIMPL;
}

HRESULT _stdcall
Cemu::DoVerb(LONG iVerb, LPMSG lpmsg, IOleClientSite *pActiveSite,
						LONG lindex, HWND hwndParent, LPCRECT lprcPosRect)
{
	if (pActiveSite)
		SetSite((IUnknown*)pActiveSite);
	if (iVerb == OLEIVERB_SHOW) {
		PaintRect(NULL, &posr);
		return S_OK;
	}
	if (iVerb == PIVERB_REDRAW) {
		if (attached && lprcPosRect != NULL) {
			RECT r;
			r.left = lprcPosRect->left + posr.left;
			r.top = lprcPosRect->top + posr.top;
			r.right = lprcPosRect->right + posr.left;
			r.bottom = lprcPosRect->bottom + posr.top;
			PaintRect(NULL, &r);
		} else
			PaintRect(NULL, &posr);
		return S_OK;
	}

	int nfetched = 0;
	int n;
	for (n = 0; n < nfiles; n++) {
		if (files[n].fetcher == NULL)
			nfetched++;
	}

	BOOL launchemu = FALSE;
	if (iVerb == PIVERB_FETCHERDONE) {
		if (nfetched == nfiles && site && inferno == NULL)
			launchemu = TRUE;
	}

	if (iVerb == OLEIVERB_INPLACEACTIVATE || iVerb == OLEIVERB_UIACTIVATE) {
		IOleInPlaceSiteWindowless *sitewl = NULL;
		if (site)
			site->GetInterface(&sitewl);
		if (sitewl) {
			if (!activated) {
				activated = TRUE;
				BOOL b = FALSE;
				sitewl->CanWindowlessActivate();
				sitewl->OnInPlaceActivateEx(&b, ACTIVATE_WINDOWLESS);
			}
			uiactivate(sitewl, TRUE);
			sitewl->Release();
			if (inferno == NULL && nfetched == nfiles) {
				launchemu = TRUE;
			}
		}
	}
	if (launchemu) {
		status = "Launching Inferno...";
		PaintRect(NULL, &posr);
		inferno = new CInferno();
		char *err = inferno->start(this, cdesc, scrx, scry, init, files, nfiles, cflag);
		if (err != NULL) {
			delete inferno;
			inferno = NULL;
			status = err;
		} else {
			attached = 1;
		}
	}
	return S_OK;
}

HRESULT _stdcall
Cemu::EnumVerbs(IEnumOLEVERB **ppEnumOleVerb)
{
	return OleRegEnumVerbs(emuclsid, ppEnumOleVerb);
}

HRESULT _stdcall
Cemu::Update(void)
{
	return S_OK;
}

HRESULT _stdcall
Cemu::IsUpToDate(void)
{
	return S_OK;
}

HRESULT _stdcall
Cemu::GetUserClassID(CLSID *pClsid)
{
	*pClsid = emuclsid;
	return S_OK;
}

HRESULT _stdcall
Cemu::GetUserType(DWORD dwFormOfType, LPOLESTR *pszUserType)
{
	*pszUserType = NULL;
	return OLE_S_USEREG;
}

HRESULT _stdcall
Cemu::SetExtent(DWORD dwDrawAspect, SIZEL *psizel)
{
	if (dwDrawAspect == DVASPECT_CONTENT) {
		HDC dc = CreateCompatibleDC(NULL);
		int xperi = GetDeviceCaps(dc, LOGPIXELSX);
		int yperi = GetDeviceCaps(dc, LOGPIXELSY);
		scrx = MulDiv(psizel->cx, xperi, 2540);
		scry = MulDiv(psizel->cy, yperi, 2540);
		// scrx & scry must be factor of 4
		scrx &= ~0x03;
		scry &= ~0x03;
		sizel.cx = MulDiv(scrx, 2540, xperi);
		sizel.cy = MulDiv(scry, 2540, yperi);
	}
	return S_OK;
}

HRESULT _stdcall
Cemu::GetExtent(DWORD dwDrawAspect, SIZEL *psizel)
{
	*psizel = sizel;
	return S_OK;
}

HRESULT _stdcall
Cemu::Advise(IAdviseSink *pAdvSink, DWORD *pdwConnection)
{
	int id = AddSink(pAdvSink);
	if (id < 0) {
		*pdwConnection = 0;
		return E_OUTOFMEMORY;
	}
	// NOTE: AddSink has already called AddRef()
	*pdwConnection = (DWORD)(id+1);
	return S_OK;
}

HRESULT _stdcall
Cemu::Unadvise(DWORD dwConnection)
{
	int id = (int)dwConnection - 1;
	if (id < 0 || id >= nsinks)
		return OLE_E_NOCONNECTION;

	// WARNING - may need to synchronize access to sinks array
	sinks[id]->Release();
	sinks[id] = NULL;
	return S_OK;
}

HRESULT _stdcall
Cemu::EnumAdvise(IEnumSTATDATA **ppenumAdvise)
{
	return E_NOTIMPL;
}

HRESULT _stdcall
Cemu::GetMiscStatus(DWORD dwAspect, DWORD *pdwStatus)
{
	*pdwStatus = OLEMISC_RECOMPOSEONRESIZE
		|OLEMISC_CANTLINKINSIDE
		|OLEMISC_INSIDEOUT
		|OLEMISC_ACTIVATEWHENVISIBLE
		|OLEMISC_ALWAYSRUN
		|OLEMISC_SETCLIENTSITEFIRST;
	return S_OK;
}

HRESULT _stdcall
Cemu::SetColorScheme(LOGPALETTE *pLogpal)
{
	return S_OK;
}


// IOleControl
// ===========

// this doesn't get called by IE
HRESULT _stdcall
Cemu::GetControlInfo(CONTROLINFO *pCI)
{
	return E_NOTIMPL;
//	if (pCI->cb != sizeof(CONTROLINFO)) {
//		return E_POINTER;
//	}
//	if (accels == NULL) {
//		pCI->cAccel = 0;
//		pCI->hAccel = NULL;
//	} else {
//		pCI->cAccel = 1;
//		pCI->hAccel = (HACCEL)accels;
//	}
//	pCI->dwFlags = CTRLINFO_EATS_RETURN | CTRLINFO_EATS_ESCAPE;
//	return S_OK;
}

HRESULT _stdcall
Cemu::OnMnemonic(MSG *pMsg)
{
//	return S_OK;
	return E_NOTIMPL;
}

HRESULT _stdcall
Cemu::OnAmbientPropertyChange(DISPID dispID)
{
	return S_OK;
}

HRESULT _stdcall
Cemu::FreezeEvents(BOOL bFreeze)
{
	return S_OK;
}


// IPersist (base class of IPersistStreamInit)
// ===========================================
HRESULT _stdcall
Cemu::GetClassID(CLSID *pClassID)
{
	*pClassID = emuclsid;
	return S_OK;
}


// IPersistPropertyBag
// ===================


HRESULT _stdcall
Cemu::InitNew(void)
{
	if (running)
		return E_UNEXPECTED;
	running = TRUE;
	return S_OK;
}

static char *
getprop(IPropertyBag *pbag, IErrorLog *plog, char *pname)
{
	int plen = strlen(pname);
	WCHAR *uni = new WCHAR[plen +1];
	if (uni == NULL)
		return NULL;
	for (int n = 0; n <= plen; n++)
		uni[n] = pname[n];

	VARIANT var;
	VariantInit(&var);
	VariantChangeType(&var, &var, 0, VT_BSTR);
	HRESULT hr = pbag->Read(uni, &var, plog);
	delete uni;
	if (hr == S_OK) {
		int nc = wcslen(var.bstrVal);
		if (nc) {
			char *buf = (char *)malloc(nc + 1);
			if (buf) {
				for (int i = 0; i <= nc; i++)
					buf[i] = (char)var.bstrVal[i];
				VariantClear(&var);
				return buf;
			}
		}
	}
	return NULL;
}

static char channames[] = "rgbkamx";

static ULONG
strtochan(char *s)
{
	char *p, *q;
	ULONG c;
	int t, n;

	c = 0;
	p=s;
	while(*p && isspace(*p))
		p++;

	while(*p && !isspace(*p)){
		if((q = strchr(channames, p[0])) == NULL) 
			return 0;
		t = q-channames;
		if(p[1] < '0' || p[1] > '9')
			return 0;
		n = p[1]-'0';
		c = (c<<8) | __DC(t, n);
		p += 2;
	}
	return c;
}

HRESULT _stdcall
Cemu::Load(IPropertyBag *pbag, IErrorLog *plog)
{
	char *prop = getprop(pbag, plog, "init");
	if (prop != NULL) {
		if (init != NULL)
			delete init;
		init = prop;
	}
	if ((prop = getprop(pbag, plog, "compile")) != NULL) {
		cflag = atoi(prop);
		delete prop;
	}
	if ((prop = getprop(pbag, plog, "chans")) != NULL) {
		cdesc = strtochan(prop);
		delete prop;
	}

	int nf;
	int blen;
	char buf[16];
	for (nf = 0;;nf++) {
		blen = wsprintf(buf, "file%d", nf+1);
		prop = getprop(pbag, plog, buf);
		if (prop == NULL)
			break;
		delete prop;
	}

	if (nf == 0)
		return S_OK;
	
	files = new PluginFile[nf];
	if (files == NULL) {
//		SetStatus("Out of memory");
		return S_OK;
	}

	int n;
	int ix = 0;
	for (n = 0; n < nf; n++) {
		files[ix].dname = NULL;
		files[ix].spath = NULL;
		files[ix].url = NULL;
		files[ix].fetcher = NULL;
		files[ix].stream = NULL;
		files[ix].status[0] = '\0';

		blen = wsprintf(buf, "file%d", n+1);
		prop = getprop(pbag, plog, buf);
		if (prop == NULL)
			break;
		char *sp = strchr(prop, ' ');
		if (sp == NULL || *(sp+1) == '\0') {
			delete prop;
			continue;
		}
		*sp = '\0';
		files[ix].dname = strdup(prop);
		files[ix].url = strdup(sp+1);
		delete prop;

		DWORD id;
		files[ix].stream = StreamFromObj(this);
		files[ix].fetcher = CreateThread(NULL, 0, fetcher, &(files[ix]), 0, &id);
		if (files[ix].fetcher == NULL)
			strncpy(files[ix].status, "Cannot spawn thread", PF_MSGLEN);
		ix++;
	}
	nfiles = ix;
	return S_OK;
}


HRESULT _stdcall
Cemu::Save(IPropertyBag *pPropBag, BOOL fClearDirty, BOOL fSaveAllProperties)
{
	return S_OK;
}

static ULONG __stdcall
fetcher(void *arg)
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	PluginFile *pif = (PluginFile *)arg;
	IOleObject *emu = ObjFromStream(pif->stream);

	strncpy(pif->status, "DOWNLOADING", PF_MSGLEN);
	emu->DoVerb(PIVERB_REDRAW, NULL, NULL, 0, NULL, NULL);

	char buf[256];
	char *msg;
	if (URLDownloadToCacheFile(NULL, pif->url, buf, sizeof(buf), 0, NULL) == S_OK) {
		pif->spath = strdup(buf);
		msg = "DONE";
	} else
		msg = "DOWNLOAD FAILED";
	strncpy(pif->status, msg, PF_MSGLEN);
	emu->DoVerb(PIVERB_REDRAW, NULL, NULL, 0, NULL, NULL);

	pif->fetcher = NULL;
	emu->DoVerb(PIVERB_FETCHERDONE, NULL, NULL, 0, NULL, NULL);
	return 0;
}