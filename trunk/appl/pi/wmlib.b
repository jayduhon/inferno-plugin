implement Wmlib;

include "sys.m";
	sys: Sys;
	Dir: import sys;

include "draw.m";
	draw: Draw;
	Screen, Rect, Point: import draw;

include "tk.m";
	tk: Tk;
	cmd, Toplevel: import tk;

include "string.m";
	str: String;

include "wmlib.m";

include "workdir.m";

include "readdir.m";
	readdir: Readdir;

include "filepat.m";
	filepat: Filepat;

init()
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	str = load String String->PATH;
}

title_cfg := array[] of {
	"frame .Wm_t -width 0 -height 0 -borderwidth 0",
};

#
# Create a window manager title bar called .Wm_t which is ready
# to pack at the top level
#
titlebar(scr: ref Draw->Screen,
		nil: string,
		nil: string,
		nil: int): (ref Tk->Toplevel, chan of string)
{
	t := tk->toplevel(scr, "-borderwidth 1 -relief raised");

	width := string scr.image.r.dx();
	height := string scr.image.r.dy();
	tk->cmd(t, "pack propagate . 0");
	tk->cmd(t, ". configure -bd 0 -width " + width + " -height " + height);
	tkcmds(t, title_cfg);
	return (t, chan of string);
}

#
# titlectl implements the default window behavior for programs
# using title bars
#
titlectl(t: ref Toplevel, request: string)
{
	tk->cmd(t, "cursor -default");
}

# deprecated, awaiting removal.
untaskbar()
{
}

unhide()
{
}

#
# find upper left corner for new child window
#

geom(nil: ref Toplevel): string
{
	return "-x 0 -y 0";
}

localgeom(nil: ref Toplevel): string
{
	return nil;
}

centre(t: ref Toplevel)
{
	org: Point;
	org.x = t.image.display.image.r.dx() / 2 - t.image.r.dx() / 2;
	org.y = t.image.display.image.r.dy() / 3 - t.image.r.dy() / 2;
	if (org.y < 0)
		org.y = 0;
	tk->cmd(t, ". configure -x " + string org.x + " -y " + string org.y);
}

#
# Set the name that will be displayed on the task bar
#
taskbar(nil: ref Toplevel, nil: string): string
{
	return nil;
}

snarfget(): string
{
	fd := sys->open("/chan/snarf", sys->OREAD);
	if(fd == nil)
		return "";

	buf := array[8192] of byte;
	n := sys->read(fd, buf, len buf);
	if(n <= 0)
		return "";

	return string buf[0:n];
}

snarfput(buf: string)
{
	fd := sys->open("/chan/snarf", sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "%s", buf);
}

tkquote(s: string): string
{
	r := "{";

	j := 0;
	for(i:=0; i < len s; i++) {
		if(s[i] == '{' || s[i] == '}' || s[i] == '\\') {
			r = r + s[j:i] + "\\";
			j = i;
		}
	}
	r = r + s[j:i] + "}";
	return r;
}

tkcmds(top: ref Tk->Toplevel, a: array of string)
{
	n := len a;
	for(i := 0; i < n; i++)
		tk->cmd(top, a[i]);
}

topopts := array[] of {
	"font"
#	, "bd"			# Wait for someone to ask for these
#	, "relief"		# Note: colors aren't inherited, it seems
};

opts(top: ref Tk->Toplevel) : string
{
	if (top == nil)
		return nil;
	opts := "";
	for ( i := 0; i < len topopts; i++ ) {
		cfg := tk->cmd(top, ". cget " + topopts[i]);
		if ( cfg != "" && cfg[0] != '!' )
			opts += " -" + topopts[i] + " " + tkquote(cfg);
	}
	return opts;
}

dialog_config := array[] of {
	"label .top.ico",
	"label .top.msg",
	"frame .top -relief raised -bd 1",
	"frame .bot -relief raised -bd 1",
	"pack .top.ico -side left -padx 10 -pady 10",
	"pack .top.msg -side left -expand 1 -fill both -padx 10 -pady 10",
	"pack .Wm_t .top .bot -side top -fill both",
	"focus ."
};

dialog(parent: ref Tk->Toplevel,
	ico: string,
	title:string,
	msg: string,
	dflt: int,
	labs : list of string): int
{
	where := localgeom(parent) + " " + opts(parent);

	(t, tc) := titlebar(parent.image.screen, where, title, 0);

	d := chan of string;
	tk->namechan(t, d, "d");

	tkcmds(t, dialog_config);
	tk->cmd(t, ".top.msg configure -text '" + msg);
	if (ico != nil)
		tk->cmd(t, ".top.ico configure -bitmap " + ico);

	n := len labs;
	for(i := 0; i < n; i++) {
		tk->cmd(t, "button .bot.button" +
				string(i) + " -command {send d " +
				string(i) + "} -text '" + hd labs);

		if(i == dflt) {
			tk->cmd(t, "frame .bot.default -relief sunken -bd 1");
			tk->cmd(t, "pack .bot.default -side left -expand 1 -padx 10 -pady 8");
			tk->cmd(t, "pack .bot.button" + string i +
				" -in .bot.default -side left -padx 10 -pady 8 -ipadx 8 -ipady 4");
		}
		else
			tk->cmd(t, "pack .bot.button" + string i +
				" -side left -expand 1 -padx 10 -pady 10 -ipadx 8 -ipady 4");
		labs = tl labs;
	}

	if(dflt >= 0)
		tk->cmd(t, "bind . <Key-\n> {send d " + string dflt + "}");
	tk->cmd(t, "update");

	e := tk->cmd(t, "variable lasterror");
	if(e != "") {
		sys->fprint(sys->fildes(2), "Wmlib.dialog error: %s\n", e);
		return dflt;
	}

	for(;;) alt {
	ans := <-d =>
		return int ans;
	tcs := <-tc =>
		if(tcs[0] == 'e')
			return dflt;
		titlectl(t, tcs);
	}

}

getstring_config := array[] of {
	"label .lab",
	"entry .ent -relief sunken -bd 2 -width 200",
	"pack .lab .ent -side left",
	"bind .ent <Key-\n> {send f 1}",
	"focus .ent"
};

getstring(parent: ref Tk->Toplevel, msg: string): string
{
	where := localgeom(parent) + " " + opts(parent);

	t := tk->toplevel(parent.image.screen, where + " -borderwidth 2 -relief raised");
	f := chan of string;
	tk->namechan(t, f, "f");

	tkcmds(t, getstring_config);
	tk->cmd(t, ".lab configure -text '" + msg + ":   ");
	tk->cmd(t, "update");

	e := tk->cmd(t, "variable lasterror");
	if(e != "") {
		sys->print("getstring error: %s\n", e);
		return "";
	}

	<-f;

	ans := tk->cmd(t, ".ent get");
	if(len ans > 0 && ans[0] == '!')
		return "";

	tk->cmd(t, "destroy .");

	return ans;
}

TABSXdelta : con 2;
TABSXslant : con 5;
TABSXoff : con 5;
TABSYheight : con 35;
TABSYtop : con 10;
TABSBord : con 3;

# pseudo-widget for folder tab selections
mktabs(t: ref Tk->Toplevel, dot: string, tabs: array of (string, string), dflt: int): chan of string
{
	lab, widg: string;

	tk->cmd(t, "canvas "+dot+" -height "+string TABSYheight);
	tk->cmd(t, "pack propagate "+dot+" 0");
	c := chan of string;
	tk->namechan(t, c, dot[1:]);
	xpos := 2*TABSXdelta;
	top := 10;
	ypos := TABSYheight - 3;
	back := tk->cmd(t, dot+" cget -background");
	dark := "#999999";
	light := "#ffffff";
	w := 20;
	h := 30;
	last := "";
	for(i := 0; i < len tabs; i++){
		(lab, widg) = tabs[i];
		tag := lab+"_tag";
		sel := lab+"_sel";
		desel := lab+"_desel";
		xs := xpos;
		xpos += TABSXslant + TABSXoff;
		v := tk->cmd(t, dot+" create text "+string xpos+" "+string ypos+" -text "+lab+" -anchor sw -tags "+tag);
		bbox := tk->cmd(t, dot+" bbox "+tag);
		if(bbox[0] == '!')
			break;
		(r, nil) := parserect(bbox);
		r.max.x += TABSXoff;
		x1 := " "+string xs;
		x2 := " "+string(xs + TABSXslant);
		x3 := " "+string r.max.x;
		x4 := " "+string(r.max.x + TABSXslant);
		y1 := " "+string(TABSYheight - 2);
		y2 := " "+string TABSYtop;
		tk->cmd(t, dot+" create polygon " + x1+y1 + x2+y2 + x3+y2 + x4+y1 +
			" -fill "+back+" -tags "+tag);
		tk->cmd(t, dot+" create line " + x3+y2 + x4+y1 +
			" -fill "+dark+" -width 3 -tags "+tag);
		tk->cmd(t, dot+" create line " + x1+y1 + x2+y2 + x3+y2 +
			" -fill "+light+" -width 3 -tags "+tag);

		x1 = " "+string(xs+2);
		x4 = " "+string(r.max.x + TABSXslant - 2);
		y1 = " "+string(TABSYheight);
		tk->cmd(t, dot+" create line " + x1+y1 + x4+y1 +
			" -fill "+back+" -width 5 -tags "+sel);

		tk->cmd(t, dot+" raise "+v);
		tk->cmd(t, dot+" bind "+tag+" <ButtonRelease-1> 'send "+
			dot[1:]+" "+string i);

		tk->cmd(t, dot+" lower "+tag+" "+last);
		last = tag;

		xpos = r.max.x;
		ww := int tk->cmd(t, widg+" cget -width");
		wh := int tk->cmd(t, widg+" cget -height");
		if(wh > h)
			h = wh;
		if(ww > w)
			w = ww;
	}
	xpos += 4*TABSXslant;
	if(w < xpos)
		w = xpos;

	for(i = 0; i < len tabs; i++){
		(nil, widg) = tabs[i];
		tk->cmd(t, "pack propagate "+widg+" 0");
		tk->cmd(t, widg+" configure -width "+string w+" -height "+string h);
	}

	w += 2*TABSBord;
	h += 2*TABSBord + TABSYheight;

	tk->cmd(t, dot+" create line 0 "+string TABSYheight+
		" "+string w+" "+string TABSYheight+" -width 3 -fill "+light);
	tk->cmd(t, dot+" create line 1 "+string TABSYheight+
		" 1 "+string(h-1)+" -width 3 -fill "+light);
	tk->cmd(t, dot+" create line  0 "+string(h-1)+
		" "+string w+" "+string(h-1)+" -width 3 -fill "+dark);
	tk->cmd(t, dot+" create line "+string(w-1)+" "+string TABSYheight+
		" "+string(w-1)+" "+string(h-1)+" -width 3 -fill "+dark);

	tk->cmd(t, dot+" configure -width "+string w+" -height "+string h);
	tk->cmd(t, dot+" configure -scrollregion {0 0 "+string w+" "+string h+"}");
	tabsctl(t, dot, tabs, -1, string dflt);
	return c;
}

tabsctl(t: ref Tk->Toplevel,
	dot: string,
	tabs: array of (string, string),
	id: int,
	s: string): int
{
	lab, widg: string;

	nid := int s;
	if(id == nid)
		return id;
	if(id >= 0){
		(lab, widg) = tabs[id];
		tk->cmd(t, dot+" lower "+lab+"_sel");
		pos := tk->cmd(t, dot+" coords "+lab+"_tag");
		if(len pos >= 1 && pos[0] != '!'){
			(p, nil) := parsept(pos);
			tk->cmd(t, dot+" coords "+lab+"_tag "+string(p.x+1)+
				" "+string(p.y+1));
		}
		if(id > 0){
			(prev, nil) := tabs[id-1];
			tk->cmd(t, dot+" lower "+lab+"_tag "+prev+"_tag");
		}
		tk->cmd(t, dot+" delete "+lab+"_win");
	}
	id = nid;
	(lab, widg) = tabs[id];
	pos := tk->cmd(t, dot+" coords "+lab+"_tag");
	if(len pos >= 1 && pos[0] != '!'){
		(p, nli) := parsept(pos);
		tk->cmd(t, dot+" coords "+lab+"_tag "+string(p.x-1)+" "+string(p.y-1));
	}
	tk->cmd(t, dot+" raise "+lab+"_tag");
	tk->cmd(t, dot+" raise "+lab+"_sel");
	tk->cmd(t, dot+" create window "+string TABSBord+" "+
		string(TABSYheight+TABSBord)+" -window "+widg+" -anchor nw -tags "+lab+"_win");
	tk->cmd(t, "update");
	return id;
}

parsept(s: string): (Draw->Point, string)
{
	p: Draw->Point;

	(p.x, s) = str->toint(s, 10);
	(p.y, s) = str->toint(s, 10);
	return (p, s);
}

parserect(s: string): (Draw->Rect, string)
{
	r: Draw->Rect;

	(r.min, s) = parsept(s);
	(r.max, s) = parsept(s);
	return (r, s);
}

Browser: adt {
	top: ref Tk->Toplevel;
	ncols: int;
	colwidth: int;
	w: string;
	init: fn(top: ref Tk->Toplevel, w: string, colwidth, height: int, maxcols: int): (ref Browser, chan of string);

	addcol: fn(c: self ref Browser, t: string, d: array of string);
	delete: fn(c: self ref Browser, colno: int);
	selection: fn(c: self ref Browser, cno: int): string;
	select: fn(b: self ref Browser, cno: int, e: string);
	entries: fn(b: self ref Browser, cno: int): array of string;
	resize: fn(c: self ref Browser);
};

BState: adt {
	b: ref Browser;
	bpath: string;		# path currently displayed in browser
	epath: string;		# path entered by user
	dirfetchpid: int;
	dirfetchpath: string;
};

filename_config := array[] of {
	"entry .e -bg white",
	"frame .pf",
	"entry .pf.e",
	"label .pf.t -text {Filter:}",
	"entry .pats",
	"bind .e <Key> +{send ech key}",
	"bind .e <Key-\n> {send ech enter}",
	"bind .e {<Key-\t>} {send ech expand}",
	"bind .pf.e <Key-\n> {send ech setpat}",
	"bind . <Configure> {send ech config}",
	"pack .b -side top -fill both -expand 1",
	"pack .pf.t -side left",
	"pack .pf.e -side top -fill x",
	"pack .pf -side top -fill x",
	"pack .e -side top -fill x",
	"pack propagate . 0",
};

debugging := 0;

filename(scr: ref Screen, otop: ref Toplevel,
		title: string,
		pats: list of string,
		dir: string): string
{
	patstr: string;
	if(readdir == nil) {
		readdir = load Readdir Readdir->PATH;
		filepat = load Filepat Filepat->PATH;
	}

	if (dir == nil || dir == ".") {
		wd := load Workdir Workdir->PATH;
		if ((dir = wd->init()) != nil) {
			(ok, nil) := sys->stat(dir);
			if (ok == -1)
				dir = nil;
		}
		wd = nil;
	}
	if (dir == nil)
		dir = "/";
	(pats, patstr) = makepats(pats);
	where := localgeom(otop) + " " + opts(otop);
	if (title == nil)
		title = "Open";
	(top, wch) := titlebar(scr, where+" -bd 1 -font /fonts/misc/latin1.6x13", 
			title, Wmlib->Resize|Wmlib->OK);
	(b, colch) := Browser.init(top, ".b", 150, 250, 3);
	entrych := chan of string;
	tk->namechan(top, entrych, "ech");
	tkcmds(top, filename_config);	
	cmd(top, ".e insert 0 '" + dir);
	cmd(top, ".pf.e insert 0 '" + patstr);
	s := ref BState(b, nil, dir, -1, nil);
	s.b.resize();
	dfch := chan of (string, array of ref Sys->Dir);
	if (otop == nil)
		centre(top);
loop: for (;;) {
		if (debugging) {
			sys->print("filename: before sync, bpath: '%s'; epath: '%s'\n",
				s.bpath, s.epath);
		}
		bsync(s, dfch, pats);
		if (debugging) {
			sys->print("filename: after sync, bpath: '%s'; epath: '%s'", s.bpath, s.epath);
			if (s.dirfetchpid == -1)
				sys->print("\n");
			else
				sys->print("; fetching '%s' (pid %d)\n", s.dirfetchpath, s.dirfetchpid);
		}
		cmd(top, "focus .e");
		cmd(top, "update");
		alt {
		c := <-colch =>
			double := c[0] == 'd';
			c = c[1:];
			(bpath, nbpath, elem) := (s.bpath, "", "");
			for (cno := 0; cno <= int c; cno++) {
				(elem, bpath) = nextelem(bpath);
				nbpath = pathcat(nbpath, elem);
			}
			nsel := s.b.selection(int c);
			if (nsel != nil)
				nbpath = pathcat(nbpath, deslash(nsel));
			s.epath = nbpath;
			cmd(top, ".e delete 0 end");
			cmd(top, ".e insert 0 '" + s.epath);
			if (double)
				break loop;
		c := <-entrych =>
			case c {
			"enter" =>
				break loop;
			"config" =>
				s.b.resize();
			"key" =>
				s.epath = cmd(top, ".e get");
			"expand" =>
				cmd(top, ".e delete 0 end");
				cmd(top, ".e insert 0 '" + s.bpath);
				s.epath = s.bpath;
			"setpat" =>
				patstr = cmd(top, ".pf.e get");
				if (patstr == "  debug  ")
					debugging = !debugging;
				else {
					(nil, pats) = sys->tokenize(patstr, " ");
					s.b.delete(0);
					s.bpath = nil;
				}
			}
		c := <-wch =>
			if (c == "ok")
				break loop;
			if (c == "exit") {
				s.epath = nil;
				break loop;
			}
			titlectl(top, c);
		(t, d) := <-dfch =>
			ds := array[len d] of string;
			for (i := 0; i < len d; i++) {
				n := d[i].name;
				if ((d[i].mode & Sys->CHDIR) != 0)
					n[len n] = '/';
				ds[i] = n;
			}
			s.b.addcol(t, ds);
			ds = nil;
			d = nil;
			s.bpath = s.dirfetchpath;
			s.dirfetchpid = -1;
		}
	}
	if (s.dirfetchpid != -1)
		kill(s.dirfetchpid);
	return s.epath;
}

bsync(s: ref BState, dfch: chan of (string, array of ref Sys->Dir), pats: list of string)
{
	(epath, bpath) := (s.epath, s.bpath);
	cno := 0;
	prefix, e1, e2: string = "";

	# find maximal prefix of epath and bpath.
	for (;;) {
		p1, p2: string;
		(e1, p1) = nextelem(epath);
		(e2, p2) = nextelem(bpath);
		if (e1 == nil || e1 != e2)
			break;
		prefix = pathcat(prefix, e1);
		(epath, bpath) = (p1, p2);
		cno++;
	}

	if (epath == nil) {
		if (bpath != nil) {
			s.b.delete(cno);
			s.b.select(cno - 1, nil);
			s.bpath = prefix;
		}
		return;
	}

	# if the paths have no prefix in common then we're starting
	# at a different root - don't do anything until
	# we know we have at least one full element.
	# even then, if it's not a directory, we have to ignore it.
	if (cno == 0 && islastelem(epath))
		return;

	if (e1 != nil && islastelem(epath)) {
		# find first prefix-matching entry.
		match := "";
		for ((i, ents) := (0, s.b.entries(cno - 1)); i < len ents; i++) {
			m := ents[i];
			if (len m >= len e1 && m[0:len e1] == e1) {
				match = deslash(m);
				break;
			}
		}
		if (match != nil) {
			if (match == e2 && islastelem(bpath))
				return;

			epath = pathcat(match,  epath[len e1:]);
			e1 = match;
			if (e1 == e2)
				cno++;
		} else {
			s.b.delete(cno);
			s.bpath = prefix;
			return;
		}
	}

	s.b.delete(cno);
	s.b.select(cno - 1, e1);
	np := pathcat(prefix, e1);
	if (s.dirfetchpid != -1) {
		if (np == s.dirfetchpath)
			return;
		kill(s.dirfetchpid);
		s.dirfetchpid = -1;
	}
	(ok, dir) := sys->stat(np);
	if (ok != -1 && (dir.mode & Sys->CHDIR) != 0) {
		sync := chan of int;
		spawn dirfetch(np, e1, sync, dfch, pats);
		s.dirfetchpid = <-sync;
		s.dirfetchpath = np;
	} else if (ok != -1)
		s.bpath = np;
	else
		s.bpath = prefix;
}

dirfetch(p: string, t: string, sync: chan of int,
		dfch: chan of (string, array of ref Sys->Dir),
		pats: list of string)
{
	sync <-= sys->pctl(0, nil);
	(a, e) := readdir->init(p, Readdir->NAME|Readdir->COMPACT);
	if (e != -1) {
		j := 0;
		for (i := 0; i < len a; i++) {
			pl := pats;
			if ((a[i].mode & Sys->CHDIR) == 0) {
				for (; pl != nil; pl = tl pl)
					if (filepat->match(hd pl, a[i].name))
						break;
			}
			if (pl != nil || pats == nil)
				a[j++] = a[i];
		}
		a = a[0:j];
	}
	dfch <-= (t, a);
}
	
Browser.init(top: ref Tk->Toplevel, w: string, colwidth, height: int, maxcols: int): (ref Browser, chan of string)
{
	c := ref Browser;
	c.top = top;
	c.ncols = 0;
	c.colwidth = colwidth;
	c.w = w;
	cmd(c.top, "frame " + c.w);
	cmd(c.top, "canvas " + c.w + ".c -height " + string height +
		" -width " + string (maxcols * colwidth) +
		" -xscrollcommand {" + c.w + ".s set}");
	cmd(c.top, "frame " + c.w + ".c.f -bd 0");
	cmd(c.top, c.w + ".c create window 0 0 -tags win -window " + c.w + ".c.f -anchor nw -height " + string height);
	cmd(c.top, "scrollbar "+c.w+".s -command {"+c.w+".c xview} -orient horizontal");
	cmd(c.top, "pack "+c.w+".c -side top -fill both -expand 1");
	cmd(c.top, "pack "+c.w+".s -side top -fill x");
	ch := chan of string;
	tk->namechan(c.top, ch, "colch");
	return (c, ch);
}

xview(top: ref Tk->Toplevel, w: string): (real, real)
{
	s := cmd(top, w + " xview");
	if (s != nil && s[0] != '!') {
		(n, v) := sys->tokenize(s, " ");
		if (n == 2)
			return (real hd v, real hd tl v);
	}
	return (0.0, 0.0);
}

setscrollregion(b: ref Browser)
{
	w := int cmd(b.top, b.w+".c.f cget -actwidth");
	w += int cmd(b.top, b.w+".c cget -actwidth") - b.colwidth;
	h := int cmd(b.top, b.w+".c.f cget -actheight");
	if (w > 0 && h > 0)
		cmd(b.top, b.w + ".c configure -scrollregion {0 0 " + string w + " " + string h + "}");
	(start, end) := xview(b.top, b.w+".c");
	if (end > 1.0)
		cmd(b.top, b.w+".c xview scroll left 0 units");
}

Browser.addcol(b: self ref Browser, title: string, d: array of string)
{
	ncol := string b.ncols++;
	f := b.w + ".c.f.d" + ncol;
	t := f + ".t";
	sb := f + ".s";
	lb := f + ".l";
	cmd(b.top, "frame " + f);
	cmd(b.top, "label " + t + " -text " + tkquote(title) + " -bg black -fg white" +
			" -width " + string b.colwidth);
	cmd(b.top, "scrollbar " + sb +
		" -command {" + lb + " yview}");
	cmd(b.top, "listbox " + lb + " -width "
			 + string (b.colwidth - widgetwidth(b.top, sb)) +
		" -selectmode browse" + 
		" -yscrollcommand {" + sb + " set}" +
		" -bd 2");
	cmd(b.top, "bind " + lb + " <ButtonRelease-1> +{send colch s " + ncol + "}");
	cmd(b.top, "bind " + lb + " <Double-Button-1> +{send colch d " + ncol + "}");
	cmd(b.top, "pack " + t + " -side top -fill x");
	cmd(b.top, "pack " + sb + " -side left -fill y");
	cmd(b.top, "pack " + lb + " -side left -fill y");
	cmd(b.top, "pack " + f + " -side left -fill y");
	for (i := 0; i < len d; i++)
		cmd(b.top, lb + " insert end '" + d[i]);
	setscrollregion(b);
	seecol(b, b.ncols - 1);
}

Browser.resize(b: self ref Browser)
{
	if (b.ncols == 0)
		return;
	cmd(b.top, b.w + ".c itemconfigure win -height [" + b.w + ".c cget -actheight]");
	setscrollregion(b);
}

seecol(b: ref Browser, cno: int)
{
	w := b.w + ".c.f.d" + string cno;
	min := int cmd(b.top, w + " cget -actx");
	max := min + int cmd(b.top, w + " cget -actwidth") +
			2 * int cmd(b.top, w + " cget -bd");
	min = int cmd(b.top, b.w+".c canvasx " + string min);
	max = int cmd(b.top, b.w +".c canvasx " + string max);

	# see first the right edge; then the left edge, to ensure
	# that the start of a column is visible, even if the window
	# is narrower than one column.
	cmd(b.top, b.w + ".c see " + string max + " 0");
	cmd(b.top, b.w + ".c see " + string min + " 0");
}

Browser.delete(b: self ref Browser, colno: int)
{
	while (b.ncols > colno)
		cmd(b.top, "destroy " + b.w+".c.f.d" + string --b.ncols);
	setscrollregion(b);
}

Browser.selection(b: self ref Browser, cno: int): string
{
	if (cno >= b.ncols || cno < 0)
		return nil;
	l := b.w+".c.f.d" + string cno + ".l";
	sel := cmd(b.top, l + " curselection");
	if (sel == nil)
		return nil;
	return cmd(b.top, l + " get " + sel);
}

Browser.select(b: self ref Browser, cno: int, e: string)
{
	if (cno < 0 || cno >= b.ncols)
		return;
	l := b.w+".c.f.d" + string cno + ".l";
	cmd(b.top, l + " selection clear 0 end");
	if (e == nil)
		return;
	ents := b.entries(cno);
	for (i := 0; i < len ents; i++) {
		if (deslash(ents[i]) == e) {
			cmd(b.top, l + " selection set " + string i);
			cmd(b.top, l + " see " + string i);
			return;
		}
	}
}

Browser.entries(b: self ref Browser, cno: int): array of string
{
	if (cno < 0 || cno >= b.ncols)
		return nil;
	l := b.w+".c.f.d" + string cno + ".l";
	nent := int cmd(b.top, l + " index end") + 1;
	ents := array[nent] of string;
	for (i := 0; i < len ents; i++)
		ents[i] = cmd(b.top, l + " get " + string i);
	return ents;
}

# turn each pattern of the form "*.b (Limbo files)" into "*.b".
# ignore '*' as it's a hangover from a past age.
makepats(pats: list of string): (list of string, string)
{
	np: list of string;
	s := "";
	for (; pats != nil; pats = tl pats) {
		p := hd pats;
		for (i := 0; i < len p; i++)
			if (p[i] == ' ')
				break;
		pat := p[0:i];
		if (p != "*") {
			np = p[0:i] :: np;
			s += hd np;
			if (tl pats != nil)
				s[len s] = ' ';
		}
	}
	return (np, s);
}

widgetwidth(top: ref Tk->Toplevel, w: string): int
{
	return int cmd(top, w + " cget -width") + 2 * int cmd(top, w + " cget -bd");
}

skipslash(path: string): string
{
	for (i := 0; i < len path; i++)
		if (path[i] != '/')
			return path[i:];
	return nil;
}

nextelem(path: string): (string, string)
{
	if (path == nil)
		return (nil, nil);
	if (path[0] == '/')
		return ("/", skipslash(path));
	for (i := 0; i < len path; i++)
		if (path[i] == '/')
			break;
	return (path[0:i], skipslash(path[i:]));
}

islastelem(path: string): int
{
	for (i := 0; i < len path; i++)
		if (path[i] == '/')
			return 0;
	return 1;
}

pathcat(path, elem: string): string
{
	if (path != nil && path[len path - 1] != '/')
		path[len path] = '/';
	return path + elem;
}

# remove a possible trailing slash
deslash(s: string): string
{
	if (len s > 0 && s[len s - 1] == '/')
		s = s[0:len s - 1];
	return s;
}

kill(pid: int): int
{
	fd := sys->open("/prog/"+string pid+"/ctl", Sys->OWRITE);
	if (fd == nil)
		return -1;
	if (sys->write(fd, array of byte "kill", 4) != 4)
		return -1;
	return 0;
}
