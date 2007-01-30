#include <windows.h>
#include <comdef.h>
#include <objbase.h>
#include <process.h>
#include "main.h"
#include "emu.h"
#include "ieplugin.h"
#include "inferno.h"
#include "drawchans.h"

void
debug(char *msg)
{
	MessageBox(NULL, msg, "Emu", MB_OK);
}

static ULONG __stdcall eventhandler(void *arg);
static ULONG __stdcall emuinit(void *arg);
static ULONG __stdcall readcon(void *arg);

#define PIPESIZE	4096

CInferno::CInferno()
{
	screen = NULL;
	attached = FALSE;
	initslave = NULL;
	eventslave = NULL;
	conslave = NULL;
	proc = NULL;
	mutex = NULL;
	hmem = NULL;
	pmem = NULL;
	slinfo = NULL;
	conoutbuf = new char[CONLINELEN*NCONLINES];
	conlines = new int[NCONLINES];
	conlines[0] = 0;
	conlineix = 0;
	conend = 0;
	status = NULL;
}

CInferno::~CInferno()
{
//debug("~CInferno called");
	if (initslave != NULL)
		TerminateThread(initslave, 0);
	if (eventslave != NULL)
		TerminateThread(eventslave, 0);
	if (conslave != NULL)
		TerminateThread(conslave, 0);
	if (proc != NULL)
		TerminateProcess(proc, 0);
	if (screen != NULL)
		DeleteObject(screen);
	if (pmem != NULL) {
		Plugin *pi = (Plugin *)pmem;
		CloseHandle(pi->datain);
		CloseHandle(pi->conin);
		CloseHandle(pi->conout);
		CloseHandle(pi->doiop);
		CloseHandle(pi->dopop);
		CloseHandle(pi->iopdone);
		CloseHandle(pi->popdone);
		UnmapViewOfFile(pmem);
	}
	CloseHandle(hmem);
	CloseHandle(mutex);
	if (slinfo) {
		if (slinfo->cmd)
			delete slinfo->cmd;
		delete slinfo;
	}
	delete conoutbuf;
	delete conlines;
}

char *
GetReg(HKEY root, char FAR *path, char *value)
{
	char buf[256];
	HKEY hk = NULL;
	char *p = NULL;
//	char *x = strdup(path);
//	path = x;
	if (RegOpenKeyEx(root, path, 0, KEY_READ, &hk) != ERROR_SUCCESS)
		return NULL;
//	for (; path != NULL && *path; path = p) {
//		p = strchr(path, '\\');
//		if (p)
//			*p = '\0';
//		if (hk == NULL) {
//			if (RegOpenKeyEx(root, path, 0, KEY_READ, &hk) != ERROR_SUCCESS)
//				break;
//		} else {
//			HKEY newhk;
//			LONG res = RegOpenKeyEx(hk, path, 0, KEY_READ, &newhk);
//			RegCloseKey(hk);
//			hk = NULL;
//			if (res != ERROR_SUCCESS)
//				break;
//			hk = newhk;
//		}
//
//		if (p) {
//			*p = '\\';
//			p++;
//		}
//	}
//	delete x;
	if (hk == NULL)
		return NULL;

	buf[0] = '\0';
	ULONG len = sizeof(buf) - 1;
	RegQueryValueEx(hk, value, NULL, NULL, (LPBYTE)buf, &len);
	RegCloseKey(hk);
	buf[len] = '\0';
	return strdup(buf);
}

static HKEY keyroots[] = {
	HKEY_CURRENT_USER,
	HKEY_LOCAL_MACHINE,
	NULL,
};

static char *
getemupath()
{
	char buf[1024];

	char *reg = "Software\\Vita Nuova\\Inferno 4th Edition\\Plugin";
	char *path = NULL;
	HKEY *hkroot;
	for (hkroot = keyroots; *hkroot != NULL; hkroot++) {
		path = GetReg(*hkroot, reg, "rootpath");
		if (path != NULL && strlen(path))
			break;
	}
	if (path == NULL || !strlen(path))
		return NULL;
	wsprintf(buf, "%s\\piemu.exe -r\"%s\"", path, path);
	delete path;
	return strdup(buf);
}

static void
graphicscmap(PALETTEENTRY *pal)
{
	int r, g, b, cr, cg, cb, v, p;
	int num, den;
	int i, j;
	for(r=0,i=0;r!=4;r++) for(v=0;v!=4;v++,i+=16){
		for(g=0,j=v-r;g!=4;g++) for(b=0;b!=4;b++,j++){
			den=r;
			if(g>den) den=g;
			if(b>den) den=b;
			if(den==0)	/* divide check -- pick grey shades */
				cr=cg=cb=v*17;
			else{
				num=17*(4*den+v);
				cr=r*num/den;
				cg=g*num/den;
				cb=b*num/den;
			}
			p = i+(j&15);
			pal[p].peRed = cr*0x01010101;
			pal[p].peGreen = cg*0x01010101;
			pal[p].peBlue = cb*0x01010101;
			pal[p].peFlags = 0;
		}
	}
}

static void
graphicsgmap(PALETTEENTRY *pal, int d)
{
	int i, j, s, m, p;

	s = 8-d;
	m = 1;
	while(--d >= 0)
		m *= 2;
	m = 255/(m-1);
	for(i=0; i < 256; i++){
		j = (i>>s)*m;
		p = 255-i;
		pal[p].peRed = pal[p].peGreen = pal[p].peBlue = (255-j)*0x01010101;
		pal[p].peFlags = 0;
	}
}

static ULONG
autochan()
{
	HDC dc;
	int bpp;

	dc = GetDC(NULL);
	if (dc == NULL)
		return CMAP8;

	bpp = GetDeviceCaps(dc, BITSPIXEL);
	if (bpp < 15)
		return CMAP8;
	if (bpp < 24)
		return RGB15;
	if (bpp < 32)
		return RGB24;
	return XRGB32;
}

static int
getscrinfo(ULONG *cdesc, int Xsize, int Ysize, HPALETTE *ppal, BITMAPINFO **pbmi)
{
	int i, k;
	ULONG c;
	LOGPALETTE *logpal;
	PALETTEENTRY *pal;
	RGBQUAD *rgb;
	HPALETTE palette;
	BITMAPINFO *bmi;

	logpal = (LOGPALETTE*)malloc(sizeof(LOGPALETTE) + 256*sizeof(PALETTEENTRY));
	if(logpal == NULL)
		return 0;

	logpal->palVersion = 0x300;
	logpal->palNumEntries = 256;
	pal = logpal->palPalEntry;

	c = *cdesc;
	if(c == 0)
		c = autochan();
	k = 8;
	if(TYPE(c) == CGrey){
		graphicsgmap(pal, NBITS(c));
		c = GREY8;
	}
	else{
		if(c == RGB15)
			k = 16;
		else if(c == RGB24)
			k = 24;
		else if(c == XRGB32)
			k = 32;
		else
			c = CMAP8;
		graphicscmap(pal);
	}
	palette = CreatePalette(logpal);

	if(k == 8)
		bmi = (BITMAPINFO*)malloc(sizeof(BITMAPINFOHEADER) + 256*sizeof(RGBQUAD));
	else
		bmi = (BITMAPINFO*)malloc(sizeof(BITMAPINFOHEADER));
	if(bmi == NULL) {
		DeleteObject(palette);
		return 0;
	}
	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth = Xsize;
	bmi->bmiHeader.biHeight = -Ysize;	/* - => origin upper left */
	bmi->bmiHeader.biPlanes = 1;	/* always 1 */
	bmi->bmiHeader.biBitCount = k;
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biSizeImage = 0;	/* Xsize*Ysize*(k/8) */
	bmi->bmiHeader.biXPelsPerMeter = 0;
	bmi->bmiHeader.biYPelsPerMeter = 0;
	bmi->bmiHeader.biClrUsed = 0;
	bmi->bmiHeader.biClrImportant = 0;	/* number of important colors: 0 means all */

	if(k == 8){
		rgb = bmi->bmiColors;
		for(i = 0; i < 256; i++){
			rgb[i].rgbRed = pal[i].peRed;
			rgb[i].rgbGreen = pal[i].peGreen;
			rgb[i].rgbBlue = pal[i].peBlue;
		}
	}

	*ppal = palette;
	*cdesc = c;
	*pbmi = bmi;
	return Ysize * (k*Xsize+7)/8;
}

char *
CInferno::start(Cemu *emu, ULONG cdesc, int Scrx, int Scry, char *cmd, PluginFile *files, int nfiles, int cflag)
{
	int scrsize;
	HPALETTE palette = NULL;
	BITMAPINFO *bmi = NULL;
	char *error = NULL;
	Plugin *plugin = NULL;
	PVOID data;
	HBITMAP bits;
	PROCESS_INFORMATION pinfo = {0, 0, NULL, NULL};
	slinfo = (SlaveInfo *)calloc(1, sizeof(SlaveInfo));

	if (slinfo == NULL)
		return "out of memory";

	if (cmd != NULL)
		slinfo->cmd = strdup(cmd);

	// Try and get all our kernel objects first
	SECURITY_ATTRIBUTES sa;
	HANDLE dopop, popdone;
	HANDLE doiop, iopdone;
	HANDLE coninrd = NULL;
	HANDLE coninwr = NULL;
	HANDLE conoutrd = NULL;
	HANDLE conoutwr = NULL;
	HANDLE datrd = NULL;
	HANDLE datwr = NULL;

	char *emupath = getemupath();
	if (emupath == NULL) {
		error = "emu plugin is not registered";
		goto Error;
	}

	scrsize = getscrinfo(&cdesc, Scrx, Scry, &palette, &bmi);
	if (scrsize == 0) {
		error = "failed to get screen resources";
		goto Error;
	}

	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = 1;

	// all return NULL on failure
	mutex = CreateMutex(NULL, NULL, NULL);
	dopop = CreateEvent(&sa, FALSE, FALSE, NULL);
	doiop = CreateEvent(&sa, FALSE, FALSE, NULL);
	popdone = CreateEvent(&sa, FALSE, FALSE, NULL);
	iopdone = CreateEvent(&sa, FALSE, FALSE, NULL);

	if (!(mutex && dopop && doiop && popdone && iopdone)) {
		error = "cannot get system resources";
		goto Error;
	}

	if (!CreatePipe(&coninrd, &coninwr, &sa, PIPESIZE)) {
		error = "cannot create console input pipe";
		goto Error;
	}
	if (!CreatePipe(&conoutrd, &conoutwr, &sa, PIPESIZE)) {
		error = "cannot create console output pipe";
		goto Error;
	}
	if (!CreatePipe(&datrd, &datwr, &sa, PIPESIZE)) {
		error = "cannot create data pipe";
		goto Error;
	}

	// Start Inferno (suspended)
	char iname[16];
	STARTUPINFO si;

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOW;

	if(!CreateProcess(0, emupath, 0, 0, 1,
	   CREATE_SUSPENDED|CREATE_NEW_PROCESS_GROUP|CREATE_DEFAULT_ERROR_MODE,
	   0, 0, &si, &pinfo)){
		error = "cannot load Inferno";
		goto Error;
	}
	delete emupath;
	emupath = NULL;

	wsprintf(iname, "%X", pinfo.dwProcessId);
	hmem = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Plugin)+scrsize, iname);
	if (hmem == INVALID_HANDLE_VALUE) {
		error = "cannot get shared resources";
		goto Error;
	}
	pmem = MapViewOfFile(hmem, FILE_MAP_WRITE, 0, 0, 0);
	plugin = (Plugin *)pmem;

	if (!pmem) {
		UnmapViewOfFile(pmem);
		error = "cannot map shared memory";
		goto Error;
	}

	screen = CreateCompatibleDC(NULL);
	if (screen == NULL) {
		error = "screen DC nil";
		goto Error;
	}
	SelectPalette(screen, palette, 1);
	RealizePalette(screen);
	GdiFlush();
	bits = CreateDIBSection(screen, bmi, DIB_RGB_COLORS, &data, hmem, (ULONG)plugin->screen - (ULONG)plugin);

	if (bits == NULL) {
		error = "createDIBSection failed";
		goto Error;
	}
	SelectObject(screen, bits);
	GdiFlush();

	plugin->sz = sizeof(*plugin)+scrsize;
	plugin->dopop = dopop;
	plugin->doiop = doiop;
	plugin->popdone = popdone;
	plugin->iopdone = iopdone;
	plugin->conin = coninrd;
	plugin->conout = conoutwr;
	plugin->datain = datrd; 
	plugin->Xsize = Scrx;
	plugin->Ysize = Scry;
	plugin->cdesc = cdesc;
	plugin->cflag = cflag;
	plugin->closehandles[0] = coninwr;
	plugin->closehandles[1] = conoutrd;
//	CloseHandle(coninrd);
	CloseHandle(conoutwr);
//	CloseHandle(datrd);
	proc = pinfo.hProcess;

	DWORD id;
	slinfo->owner = this;
	slinfo->conin = coninwr;
	slinfo->conout = conoutrd;
	slinfo->datin = datwr;
	slinfo->pi = plugin;
	slinfo->s = StreamFromObj(emu);
	slinfo->attached = &attached;
	slinfo->files = files;
	slinfo->nfiles = nfiles;
	// StreamFromObj wiil increase ref count on emu
	emu->Release();
	initslave = CreateThread(NULL, 0, emuinit, slinfo, 0, &id);
	conslave = CreateThread(NULL, 0, readcon, slinfo, 0, &id);
	eventslave = CreateThread(NULL, 0, eventhandler, slinfo, 0, &id);
	ResumeThread(pinfo.hThread);
	return NULL;
Error:
	// Let the destructor do most of our cleanup
	delete emupath;
	CloseHandle(dopop);
	CloseHandle(doiop);
	CloseHandle(popdone);
	CloseHandle(iopdone);
	CloseHandle(coninrd);
	CloseHandle(coninwr);
	CloseHandle(conoutrd);
	CloseHandle(conoutwr);
	CloseHandle(datrd);
	CloseHandle(datwr);
	if (slinfo) {
		if (slinfo->cmd)
			delete slinfo->cmd;
		delete slinfo;
		slinfo = NULL;
	}
	if (palette)
		DeleteObject(palette);
	return error;
}

void
CInferno::mouse(int x, int y, int b, int modify)
{
	Plugin *pi = (Plugin *)pmem;
	WaitForSingleObject(mutex, INFINITE);
	pi->pop.op = Pmouse;
	pi->pop.u.m.x = x;
	pi->pop.u.m.y = y;
	pi->pop.u.m.b = b;
	pi->pop.u.m.modify= modify;
	SetEvent(pi->dopop);
	WaitForSingleObject(pi->popdone, INFINITE);
	ReleaseMutex(mutex);
}

void
CInferno::keyboard(int key)
{
	Plugin *pi = (Plugin *)pmem;
	WaitForSingleObject(mutex, INFINITE);
	pi->pop.op = Pgfxkey;
	pi->pop.u.key = key;
	SetEvent(pi->dopop);
	WaitForSingleObject(pi->popdone, INFINITE);
	ReleaseMutex(mutex);
}

char *
CInferno::context()
{
	// use of this->mutex is somewhat overloaded here
	// the mutex is intended to protect access to the Plugin
	// data under this->pmem
	WaitForSingleObject(mutex, INFINITE);

	// determine output buffer size
	int nbytes = 0;
	int l;
	for (l = 0; l < conlineix; l++)
		nbytes += conlines[l] + 2;	// CRLF terminator
	nbytes += conlines[conlineix] + 1;

	char *buf = new char[nbytes];
	if (buf != NULL) {
		int len;
		char *s = conoutbuf;
		char *d = buf;
		for (l = 0; l < conlineix; l++) {
			len = conlines[l];
			memcpy(d, s, len);
			d += len;
			s += len;
			*d++ = '\r';
			*d++ = '\n';
		}
		if ((len = conlines[conlineix]) > 0) {
			memcpy(d, s, len);
			d += len;
		}
		*d = '\0';
	}
	ReleaseMutex(mutex);
	return buf;
}

void
CInferno::conout(char *text, int len)
{
	// use of this->mutex is somewhat overloaded here
	// the mutex is intended to protect access to the Plugin
	// data under this->pmem
	WaitForSingleObject(mutex, INFINITE);

	while (len) {
		int lspace = CONLINELEN - conlines[conlineix];
		if (lspace > len)
			lspace = len;
		while (lspace) {
			char ch = *text++;
			len--;
			if (ch == '\t')
				ch = ' ';
			else if (ch == '\n')
				break;
			*(conoutbuf + conend++) = ch;
			conlines[conlineix]++;
			lspace--;
		}
		if (len || lspace) {
			// new line
			if (conlineix + 1 < NCONLINES)
				conlineix++;
			else {
				// scroll
				int firstlen = conlines[0];
				memmove(conoutbuf, conoutbuf + firstlen, conend-firstlen);
				memmove(conlines, conlines+1, sizeof(*conlines)*(NCONLINES-1));
				conend -= firstlen;
			}
			conlines[conlineix] = 0;
		}
	}
	ReleaseMutex(mutex);
}

static ULONG __stdcall
emuinit(void *arg)
{
	SlaveInfo *si = (SlaveInfo *)arg;
	Plugin *pi = si->pi;
	// HACK - send command & files to inferno shell...
	if (si->cmd) {
		char buf[PIPESIZE];
		PluginFile *files = si->files;
		DWORD w, n;

		n = strlen(si->cmd);
		wsprintf(buf, "%11d ", n);
		WriteFile(si->conin, buf, 12, &w, NULL);
		WriteFile(si->conin, si->cmd, strlen(si->cmd), &w, NULL);
//		CloseHandle(si->conin);
		delete si->cmd;
		si->cmd = NULL;

		for (int f = 0; f < si->nfiles; f++) {
			if (files[f].dname == NULL || files[f].spath == NULL)
				continue;
			DWORD nread;
			DWORD nwrite = 0;
			HANDLE src = CreateFile(files[f].spath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (src == INVALID_HANDLE_VALUE) {
				MessageBox(NULL, "failed to open file", "FILEOPEN", MB_OK);
				wsprintf(buf, "getlasterror = %x", GetLastError());
				MessageBox(NULL, buf, "ERROR", MB_OK);
				continue;
			}
			int dlen = strlen(files[f].dname);
			int perms = 0666;
			if (dlen > 4 && strcmp(".dis", files[f].dname + (dlen - 4)) == 0)
				perms = 0777;
			wsprintf(buf, "%d\n", perms);
			WriteFile(si->datin, files[f].dname, dlen, &w, NULL);
			WriteFile(si->datin, "\n", 1, &w, NULL);
			WriteFile(si->datin, buf, strlen(buf), &w, NULL);
			nwrite = GetFileSize(src, NULL);
			wsprintf(buf, "%d\n", nwrite);
			nwrite = 0;
			WriteFile(si->datin, buf, strlen(buf), &w, NULL);
			while (ReadFile(src, buf, sizeof(buf), &nread, NULL)) {
				if (nread == 0)
					break;
				if (!WriteFile(si->datin, buf, nread, &w, NULL))
					MessageBox(NULL, "emuinit write failed", "emu plugin", MB_OK);
				if (w != nread)
					MessageBox(NULL, "w != nread", "loop", MB_OK);
				nwrite += nread;
			}
			// should assert that nwritten = value returned from GetFileSize()
			CloseHandle(src);
			wsprintf(buf, "WRITE%d", nwrite);
		}
		// EOF
		WriteFile(si->datin, "\n", 1, &w, NULL);
		CloseHandle(si->datin);
		si->datin = NULL;
	} else
		MessageBox(NULL, "cmd is NULL", "EMU", MB_OK);
	return 0;
}

static ULONG __stdcall
readcon(void *arg)
{
	SlaveInfo *si = (SlaveInfo *)arg;
	char buf[PIPESIZE];
	DWORD n;

	while (ReadFile(si->conout, buf, sizeof(buf)-1, &n, NULL))
		si->owner->conout(buf, n);
	CloseHandle(si->conout);
	return 0;
}

static ULONG __stdcall
eventhandler(void *arg)
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	SlaveInfo *si = (SlaveInfo *)arg;
	IOleObject *emu = ObjFromStream(si->s);
	Plugin *pi = si->pi;
	BOOL *attached = si->attached;
	RECT r;

	for (;;) {
		DWORD n = WaitForSingleObject(pi->doiop, INFINITE);
		switch (n) {
		case WAIT_OBJECT_0:
			// event from Inferno
			switch (pi->iop.op) {
			case Iattachscr:
				*attached = TRUE;
				emu->DoVerb(PIVERB_REDRAW, NULL, NULL, 0, NULL, NULL);
				break;
			case Iflushscr:
				r.left = pi->iop.u.r.min.x;
				r.top = pi->iop.u.r.min.y;
				r.right = pi->iop.u.r.max.x;
				r.bottom = pi->iop.u.r.max.y;
				emu->DoVerb(PIVERB_REDRAW, NULL, NULL, 0, NULL, &r);	// hack
			case Isetcur:
			case Idrawcur:
				pi->iop.val = 0;	// success
				break;
			case Iquit:
				*attached = FALSE;
				si->owner->status = "Inferno terminated";
				emu->DoVerb(PIVERB_REDRAW, NULL, NULL, 0, NULL, NULL);
				return 0;
			default:
				pi->iop.val = -1;	// failure - bad opcode
			}
			SetEvent(pi->iopdone);
			break;
		default:
			return 0;
		}
	}
}
