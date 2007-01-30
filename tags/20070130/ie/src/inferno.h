typedef struct SlaveInfo SlaveInfo;
typedef struct LineInfo LineInfo;

class CInferno {
public:
	CInferno();
	~CInferno();
	char*	start(Cemu *emu, ULONG cdesc, int Scrx, int Scry, char *init, PluginFile *files, int nfiles, int cflag);
	void	mouse(int x, int y, int b, int modify);
	void	keyboard(int key);
	char*	context();		// get console text (caller must free the buffer)
	void	conout(char *text, int len);
	HDC		screen;
	BOOL	attached;
	char*	status;
private:
	SlaveInfo*	slinfo;
	HANDLE		initslave;
	HANDLE		eventslave;
	HANDLE		conslave;
	HANDLE		proc;
	HANDLE		mutex;		// protects access to plugin struct (pmem)
	HANDLE		hmem;
	PVOID		pmem;
	char*		conoutbuf;
	int*		conlines;
	int			conlineix;
	int			conend;
};

#define CONLINELEN	60
#define NCONLINES	20

struct SlaveInfo {
	CInferno *owner;
	HANDLE	conin;		// written by initslave
	HANDLE	conout;		// read by conslave
	HANDLE	datin;		// written by initslave
	Plugin	*pi;		// handled by eventslave
	IStream *s;
	char *cmd;
	BOOL *attached;
	PluginFile *files;
	int nfiles;
};