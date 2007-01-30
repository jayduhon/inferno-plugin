implement PluginText;

include "sys.m";
include "draw.m";
include "tk.m";

PluginText : module {
	init : fn (ctxt : ref Draw->Context, argv : list of string);
};

Command : module {
	init : fn (ctxt : ref Draw->Context, argv : list of string);
};

sys : Sys;
draw : Draw;
tk : Tk;

Context, Display, Screen, Image, Rect : import draw;
FileIO : import sys;

screen: ref Draw->Screen;

Rdreq: adt
{
	off:	int;
	nbytes:	int;
	fid:	int;
	rc:	chan of (array of byte, string);
};


badmodule(p: string)
{
	sys->fprint(stderr(), "textwin: cannot load %s: %r\n", p);
	sys->raise("fail:bad module");
}

init(nil : ref Draw->Context, argv : list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	if (draw == nil)
		badmodule(Draw->PATH);
	tk = load Tk Tk->PATH;
	if (tk == nil)
		badmodule(Tk->PATH);

	if (argv != nil)
		argv = tl argv;
	if (argv == nil) {
		sys->fprint(stderr(), "piwm usage: piwm progpath [progargs]\n");
		sys->raise("fail:usage");
	}
	prog := hd argv;
	argv = tl argv;

	(ctxt, err) := makedrawcontext();
	if (ctxt == nil) {
		sys->fprint(stderr(), "piwm: %s\n", err);
		sys->raise("fail:no draw context");
	}
	scrwidth := screen.image.r.dx();
	scrheight := screen.image.r.dy();

	spawn mouse(screen);
	spawn keyboard(screen);

	snarfIO := sys->file2chan("/chan", "snarf");
	if(snarfIO == nil) {
		sys->fprint(stderr(), "Failed to make /chan/snarf: %r\n");
		return;
	}

	iostdout := sys->file2chan("/chan", "wmstdout");
	if(iostdout == nil) {
		sys->fprint(stderr(), "Failed to make file server %r\n");
		return;
	}

	iostderr := sys->file2chan("/chan", "wmstderr");
	if(iostderr == nil) {
		sys->fprint(stderr(), "Failed to make file server: %r\n");
		return;
	}

	spawn consolex(iostdout, iostderr);
	spawn runprog(ctxt, prog, argv);

	req : Rdreq;
	snarf : array of byte;
	for(;;) alt {
	(off, data, fid, wc) := <-snarfIO.write =>
		if(wc == nil)
			break;
		if (off == 0)			# write at zero truncates
			snarf = data;
		else {
			if (off + len data > len snarf) {
				nsnarf := array[off + len data] of byte;
				nsnarf[0:] = snarf;
				snarf = nsnarf;
			}
			snarf[off:] = data;
		}
		wc <-= (len data, "");
	req =  <-snarfIO.read =>
		if(req.rc == nil)
			break;
		if (req.off >= len snarf) {
			req.rc <-= (nil, "");
			break;
		}
		e := req.off + req.nbytes;
		if (e > len snarf)
			e = len snarf;
		req.rc <-= (snarf[req.off:e], "");
	}
}

stderr() : ref Sys->FD
{
	return sys->fildes(2);
}

makedrawcontext(): (ref Draw->Context, string)
{
	display := Display.allocate(nil);
	if(display == nil)
		return (nil, sys->sprint("cannot initialise display: %r"));

	disp := display.image;
	fill: ref Draw->Image;
	fill = display.rgb(221,221, 221);
	screen = Screen.allocate(disp, fill, 1);
	disp.draw(disp.r, screen.fill, display.ones, disp.r.min);

	ctxt := ref Draw->Context;
	ctxt.screen = screen;
	ctxt.display = display;
	return (ctxt, nil);
}

keyboard(scr: ref Draw->Screen)
{
	dfd := sys->open("/dev/keyboard", sys->OREAD);
	if(dfd == nil)
		return;

	buf := array[10] of byte;
	i := 0;
	for(;;) {
		n := sys->read(dfd, buf[i:], len buf - i);
		if(n < 1)
			break;
		i += n;
		while(i >0 && (nutf := sys->utfbytes(buf, i)) > 0){
			s := string buf[0:nutf];
			for (j := 0; j < len s; j++)
				tk->keyboard(scr, int s[j]);
			buf[0:] = buf[nutf:i];
			i -= nutf;
		}
	}
}

mouse(scr: ref Draw->Screen)
{
	fd := sys->open("/dev/pointer", sys->OREAD);
	if(fd == nil) {
		sys->fprint(stderr(), "wm: cannot open pointer: %r\n");
		return;
	}

	n := 0;
	buf := array[100] of byte;
	for(;;) {
		n = sys->read(fd, buf, len buf);
		if(n <= 0)
			break;

		if(int buf[0] != 'm' || n != 37)
			continue;

		x := int(string buf[ 1:13]);
		y := int(string buf[12:25]);
		b := int(string buf[24:37]);
		tk->mouse(scr, x, y, b);
	}
}

consolex(iostdout, iostderr: ref sys->FileIO)
{
	rc: Sys->Rread;
	wc: Sys->Rwrite;
	data: array of byte;
	off, nbytes, fid: int;

	for(;;) alt {
	(off, nbytes, fid, rc) = <-iostdout.read =>
		if(rc == nil)
			break;
		rc <-= (nil, nil);
	(off, nbytes, fid, rc) = <-iostderr.read =>
		if(rc == nil)
			break;
		rc <-= (nil, nil);
	(off, data, fid, wc) = <-iostdout.write =>
		sys->print("%s", string data);
	(off, data, fid, wc) = <-iostderr.write =>
		sys->fprint(stderr(), "%s", string data);
	}
}

runprog(ctxt : ref Context, prog : string, args : list of string)
{
	prefixes : list of string;
	if (len prog < 4 || prog[len prog - 4:] != ".dis")
		prog = prog + ".dis";
	if (prog[0] == '/' || prog[0:1] == "./")
		prefixes = "" :: nil;
	else
		prefixes = "/dis/" :: "./" :: nil;

	cmd : Command;
	for (; prefixes != nil; prefixes = tl prefixes) {
		path := hd prefixes + prog;
		cmd = load Command path;
		if (cmd != nil)
			break;
	}
	if (cmd == nil) {
		sys -> fprint(stderr(), "failed to load %s\n", prog);
		return;
	}
	sys->pctl(Sys->FORKNS, nil);
	sys->bind("/dis/pi/wmlib.dis", "/dis/lib/wmlib.dis", Sys->MREPL);
	cmd->init(ctxt, prog :: args);
}