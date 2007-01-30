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

snarf := "";

badmodule(p: string)
{
	sys->fprint(stderr(), "pitext: cannot load %s: %r\n", p);
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
		sys->fprint(stderr(), "pitext usage: pitext progpath [progargs]\n");
		sys->raise("fail:usage");
	}
	prog := hd argv;
	argv = tl argv;

	(ctxt, err) := makedrawcontext();
	if (ctxt == nil) {
		sys->fprint(stderr(), "pitext: %s\n", err);
		sys->raise("fail:no draw context");
	}
	spawn wininit(ctxt, prog, argv);
	spawn mouse(screen);
	keyboard(screen);
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

shwin_cfg := array[] of {
	"menu .m",
	".m add command -text Cut -command {send edit cut}",
	".m add command -text Paste -command {send edit paste}",
	".m add command -text Snarf -command {send edit snarf}",
	".m add command -text Send -command {send edit send}",
	"frame .ft -bd 0",
	"scrollbar .ft.scroll -width 14 -bd 0 -relief ridge -command {.ft.t yview}",
	"text .ft.t -bd 1 -bg #DDDDDD -relief flat -yscrollcommand {.ft.scroll set}",
	".ft.t tag configure sel -relief flat",
	"pack .ft.scroll -side left -fill y",
	"pack .ft.t -fill both -expand 1",
	"pack .ft -fill both -expand 1",
#	"pack propagate . 0",
	"focus .ft.t",
	"bind .ft.t <Key> {send keys {%A}}",
	"bind .ft.t <Control-d> {send keys {%A}}",
	"bind .ft.t <Control-h> {send keys {%A}}",
	"bind .ft.t <Control-w> {send keys {%A}}",
	"bind .ft.t <Control-u> {send keys {%A}}",
	"bind .ft.t <Button-1> +{grab set .ft.t; send but1 pressed}",
	"bind .ft.t <Double-Button-1> +{grab set .ft.t; send but1 pressed}",
	"bind .ft.t <ButtonRelease-1> +{grab release .ft.t; send but1 released}",
	"bind .ft.t <ButtonPress-2> {send but2 %X %Y}",
	"bind .ft.t <Motion-Button-2-Button-1> {}",
	"bind .ft.t <Motion-ButtonPress-2> {}",
	"bind .ft.t <ButtonPress-3> {send but3 pressed}",
	"bind .ft.t <ButtonRelease-3> {send but3 released %x %y}",
	"bind .ft.t <Motion-Button-3> {}",
	"bind .ft.t <Motion-Button-3-Button-1> {}",
	"bind .ft.t <Double-Button-3> {}",
	"bind .ft.t <Double-ButtonRelease-3> {}",
	"bind .m <ButtonRelease> {.m tkMenuButtonUp %x %y}",
};

BS:		con 8;		# ^h backspace character
BSW:		con 23;		# ^w bacspace word
BSL:		con 21;		# ^u backspace line
EOT:		con 4;		# ^d end of file
ESC:		con 27;		# hold mode
CR:		con 13;		# ^m carriage return

HIWAT:	con 2000;	# maximum number of lines in transcript
LOWAT:	con 1500;	# amount to reduce to after high water

Rdreq: adt
{
	off:	int;
	nbytes:	int;
	fid:	int;
	rc:	chan of (array of byte, string);
};

rdreq: list of Rdreq;
menuindex := "0";
holding := 0;
rawon := 0;
rawinput := "";

wininit(ctxt : ref Draw->Context, prog : string, args : list of string)
{
	sys->pctl(Sys->FORKNS | Sys->NEWPGRP, nil);

	t := tk->toplevel(screen, nil);
	edit := chan of string;
	tk->namechan(t, edit, "edit");

	tkcmds(t, shwin_cfg);
	width := string screen.image.r.dx();
	height := string screen.image.r.dy();
	tk->cmd(t, "pack propagate . 0");
	tk->cmd(t, ". configure -width " +width + " -height " + height);
	tk->cmd(t, "update");

	ioc := chan of (int, ref FileIO, ref FileIO, string);
	spawn runcmd(ctxt, ioc, prog, args);

	(pid, file, filectl, consfile) := <-ioc;
	if(file == nil || filectl == nil) {
		sys->print("newsh: %r\n");
		return;
	}

	keys := chan of string;
	tk->namechan(t, keys, "keys");

	but1 := chan of string;
	tk->namechan(t, but1, "but1");
	but2 := chan of string;
	tk->namechan(t, but2, "but2");
	but3 := chan of string;
	tk->namechan(t, but3, "but3");
	button1 := 0;
	button3 := 0;

	rdrpc: Rdreq;

	# outpoint is place in text to insert characters printed by programs
	tk->cmd(t, ".ft.t mark set outpoint end; .ft.t mark gravity outpoint left");

	for(;;) alt {
	ecmd := <-edit =>
		editor(t, ecmd);
		sendinput(t);
		tk->cmd(t, "focus .ft.t");

	c := <-keys =>
		cut(t, 1);
		char := c[1];
		if(char == '\\')
			char = c[2];
		if(rawon) {
			rawinput[len rawinput] = char;
			rawinput = sendraw(rawinput);
			break;
		}
		update := ";.ft.t see insert;update";
		case char {
		* =>
			tk->cmd(t, ".ft.t insert insert "+c+update);
		'\n' or EOT =>
			tk->cmd(t, ".ft.t insert insert "+c+update);
			sendinput(t);
		BS =>
			tk->cmd(t, ".ft.t tkTextDelIns -c"+update);
		BSL =>
			tk->cmd(t, ".ft.t tkTextDelIns -l"+update);
		BSW =>
			tk->cmd(t, ".ft.t tkTextDelIns -w"+update);
		ESC =>
			holding ^= 1;
			color := "blue";
			if(!holding){
				color = "black";
				sendinput(t);
			}
			tk->cmd(t, ".ft.t configure -foreground "+color+update);
		}

	c := <-but1 =>
		button1 = (c == "pressed");
		button3 = 0;	# abort any pending button 3 action

	c := <-but2 =>
		if(button1){
			cut(t, 1);
			tk->cmd(t, "update");
			break;
		}
		(nil, l) := sys->tokenize(c, " ");
		x := int hd l - 50;
		y := int hd tl l - int tk->cmd(t, ".m yposition "+menuindex) - 10;
		tk->cmd(t, ".m activate "+menuindex+"; .m post "+string x+" "+string y+
			"; grab set .m; update");
		button3 = 0;	# abort any pending button 3 action

	c := <-but3 =>
		if(c == "pressed"){
			button3 = 1;
			if(button1){
				paste(t);
				tk->cmd(t, "update");
			}
			break;
		}

	rdrpc = <-filectl.read =>
		if(rdrpc.rc == nil)
			continue;
		rdrpc.rc <-= ( nil, "not allowed" );

	(nil, data, nil, wc) := <-filectl.write =>
		if(wc == nil) {
			# consctl closed - revert to cooked mode
			rawon = 0;
			continue;
		}
		(nc, cmdlst) := sys->tokenize(string data, " \n");
		if(nc == 1) {
			case hd cmdlst {
			"rawon" =>
				rawon = 1;
				rawinput = "";
				# discard previous input
				advance := string (len tk->cmd(t, ".ft.t get outpoint end") +1);
				tk->cmd(t, ".ft.t mark set outpoint outpoint+" + advance + "chars");
			"rawoff" =>
				rawon = 0;
			* =>
				wc <-= (0, "unknown consctl request");
				continue;
			}
			wc <-= (len data, nil);
			continue;
		}
		wc <-= (0, "unknown consctl request");

	rdrpc = <-file.read =>
		if(rdrpc.rc == nil) {
			(ok, nil) := sys->stat(consfile);
			if (ok < 0)
				tk->cmd(t, ".ft.t configure -foreground red; update");
			continue;
		}
		append(rdrpc);
		sendinput(t);

	(off, data, fid, wc) := <-file.write =>
		if(wc == nil) {
			(ok, nil) := sys->stat(consfile);
			if (ok < 0)
				tk->cmd(t, ".ft.t configure -foreground red; update");
			continue;
		}
		cdata := cursorcontrol(t, string data);
		ncdata := string len cdata + "chars;";
		moveins := insat(t, "outpoint");
		tk->cmd(t, ".ft.t insert outpoint '"+ cdata);
		wc <-= (len data, nil);
		data = nil;
		s := ".ft.t mark set outpoint outpoint+" + ncdata;
		s += ".ft.t see outpoint;";
		if(moveins)
			s += ".ft.t mark set insert insert+" + ncdata;
		s += "update";
		tk->cmd(t, s);
		nlines := int tk->cmd(t, ".ft.t index end");
		if(nlines > HIWAT){
			s = ".ft.t delete 1.0 "+ string (nlines-LOWAT) +".0;update";
			tk->cmd(t, s);
		}
	}
}

tkcmds(t : ref Tk->Toplevel, cmds : array of string)
{
	for (i := 0; i < len cmds; i++)
		tk->cmd(t, cmds[i]);	
}

RPCread: type (int, int, int, chan of (array of byte, string));

append(r: RPCread)
{
	t := r :: nil;
	while(rdreq != nil) {
		t = hd rdreq :: t;
		rdreq = tl rdreq;
	}
	rdreq = t;
}

insat(t: ref Tk->Toplevel, mark: string): int
{
	return tk->cmd(t, ".ft.t compare insert == "+mark) == "1";
}

insininput(t: ref Tk->Toplevel): int
{
	if(tk->cmd(t, ".ft.t compare insert >= outpoint") != "1")
		return 0;
	return tk->cmd(t, ".ft.t compare {insert linestart} == {outpoint linestart}") == "1";
}

cursorcontrol(t: ref Tk->Toplevel, s: string): string
{
	l := len s;
	for(i := 0; i < l; i++) {
		case s[i] {
		    BS =>
			pre := "";
			rem := "";
			if(i + 1 < l)
				rem = s[i+1:];
			if(i == 0) {	# erase existing character in line
				if(tk->cmd(t, ".ft.t get " +
					"{outpoint linestart} outpoint") != "")
				    tk->cmd(t, ".ft.t delete outpoint-1char");
			} else {
				if(s[i-1] != '\n')	# don't erase newlines
					i--;
				if(i)
					pre = s[:i];
			}
			s = pre + rem;
			l = len s;
			i = len pre - 1;
		    CR =>
			s[i] = '\n';
			if(i + 1 < l && s[i+1] == '\n')	# \r\n
				s = s[:i] + s[i+1:];
			else if(i > 0 && s[i-1] == '\n')	# \n\r
				s = s[:i-1] + s[i:];
			l = len s;
		}
	}
	return s;
}

editor(t: ref Tk->Toplevel, ecmd: string)
{
	s: string;

	case ecmd {
	"cut" =>
		menuindex = "0";
		cut(t, 1);
	
	"paste" =>
		menuindex = "1";
		paste(t);

	"snarf" =>
		menuindex = "2";
		if(tk->cmd(t, ".ft.t tag ranges sel") == "")
			break;
		snarf = tk->cmd(t, ".ft.t get sel.first sel.last");

	"send" =>
		menuindex = "3";
		if(tk->cmd(t, ".ft.t tag ranges sel") != "")
			snarf = tk->cmd(t, ".ft.t get sel.first sel.last");
		if(snarf != "")
			s = snarf;
		else
			return;
		if(s[len s-1] != '\n' && s[len s-1] != EOT)
			s[len s] = '\n';
		tk->cmd(t, ".ft.t see end; .ft.t insert end '"+s);
		tk->cmd(t, ".ft.t mark set insert end");
		tk->cmd(t, ".ft.t tag remove sel sel.first sel.last");
	}
	tk->cmd(t, "update");
}

cut(t: ref Tk->Toplevel, snarfit: int)
{
	if(tk->cmd(t, ".ft.t tag ranges sel") == "")
		return;
	if(snarfit)
		snarf = tk->cmd(t, ".ft.t get sel.first sel.last");
	tk->cmd(t, ".ft.t delete sel.first sel.last");
}

paste(t: ref Tk->Toplevel)
{
	if(snarf == "")
		return;
	cut(t, 0);
	tk->cmd(t, ".ft.t insert insert '"+snarf);
	tk->cmd(t, ".ft.t tag add sel insert-"+string len snarf+"chars insert");
	sendinput(t);
}

sendinput(t: ref Tk->Toplevel)
{
	if(holding)
		return;
	input := tk->cmd(t, ".ft.t get outpoint end");
	slen := len input;
	if(slen == 0 || rdreq == nil)
		return;

	r := hd rdreq;
	for(i := 0; i < slen; i++)
		if(input[i] == '\n' || input[i] == EOT)
			break;

	if(i >= slen && slen < r.nbytes)
		return;

	if(i >= r.nbytes)
		i = r.nbytes-1;
	advance := string (i+1);
	if(input[i] == EOT)
		input = input[0:i];
	else
		input = input[0:i+1];

	rdreq = tl rdreq;

	alt {
	r.rc <-= (array of byte input, "") =>
		tk->cmd(t, ".ft.t mark set outpoint outpoint+" + advance + "chars");
	* =>
		# requester has disappeared; ignore his request and try again
		sendinput(t);
	}
}

sendraw(input : string) : string
{
	i := len input;
	if(i == 0 || rdreq == nil)
		return input;

	r := hd rdreq;
	rdreq = tl rdreq;

	if(i > r.nbytes)
		i = r.nbytes;

	alt {
	r.rc <-= (array of byte input[0:i], "") =>
		input = input[i:];
	* =>
		;# requester has disappeared; ignore his request and try again
	}
	return input;
}

runcmd(ctxt: ref Context, ioc: chan of (int, ref FileIO, ref FileIO, string), prog : string, args: list of string)
{
	pid := sys->pctl(sys->NEWFD, nil);

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
	if(cmd == nil) {
		ioc <-= (0, nil, nil, nil);
		return;
	}

	tty := "cons."+string pid;

	fio := sys->file2chan("/chan", tty);
	fioctl := sys->file2chan("/chan", tty + "ctl");
	ioc <-= (pid, fio, fioctl, "/chan/"+tty);
	if(fio == nil || fioctl == nil)
		return;

	sys->bind("/chan/"+tty, "/dev/cons", sys->MREPL);
	sys->bind("/chan/"+tty+"ctl", "/dev/consctl", sys->MREPL);

	fd0 := sys->open("/dev/cons", sys->OREAD|sys->ORCLOSE);
	fd1 := sys->open("/dev/cons", sys->OWRITE);
	fd2 := sys->open("/dev/cons", sys->OWRITE);

	e := ref Sys->Exception;
	if (sys->rescue("fail:*", nil) == Sys->EXCEPTION)
		exit;
	cmd->init(nil, prog :: args);
}

