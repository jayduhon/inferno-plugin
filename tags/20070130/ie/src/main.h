// Project globals and useful macros
// =================================

// our clsid
// =========
// Registry format:	{3A274C9A-1E70-435a-8A63-B91A93F3BDDD}
static const GUID emuclsid = {0x3a274c9a, 0x1e70, 0x435a, {0x8a, 0x63, 0xb9, 0x1a, 0x93, 0xf3, 0xbd, 0xdd}};

#define GetInterface(ppO) \
	QueryInterface(__uuidof(*ppO), (void**)ppO)

extern ULONG grefc;

class Crefc
{
public:
	Crefc()		{grefc++;};
	~Crefc()	{grefc--;};
};

// Our private DoVerb() verbs
enum {	PIVERB_REDRAW		= 100,
		PIVERB_FETCHERDONE
};

extern IOleObject* ObjFromStream(void *stream);
extern IStream* StreamFromObj(void *obj);
