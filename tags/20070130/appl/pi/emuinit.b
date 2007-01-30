implement PluginInit;

include "sys.m";
	sys: Sys;

include "draw.m";

include "bufio.m";
	bufio: Bufio;
	Iobuf: import bufio;

include "memfs.m";
	memfs: MemFS;

include "string.m";
	strings: String;

include "sh.m";

PluginInit: module
{
	init: fn ();
};

Maxargs: con 32768;

init()
{
	sys = load Sys Sys->PATH;
	sys->pctl(Sys->FORKNS, nil);
	if(sys->bind("#//root", "/", Sys->MBEFORE) == -1) {
		sys->print("failed to bind filesystem root: %r\n");
		return;
	}
sys->print("about to attach\n");
	err := attacharchfs("/plugin.archfs");
	if(err != nil) {
		sys->print("%s\n", err);
		return;
	}
sys->print("done attach\n");
	sys->bind("#c", "/dev", Sys->MREPL);

	strings = load String String->PATH;
	if(strings == nil) {
		sys->print("failed to load string module: %r\n");
		return;
	}

	args := getargs();
	if(args == nil) {
		sys->print("internal error: bad args\n");
		return;
	}

	prog: string;
	if(args == nil) {
		sys->print("no command - starting sh\n");
		prog = "/dis/pi/text.dis";
		args = prog :: "/dis/sh.dis" :: nil;
	} else {
		prog = hd args;
	}

	#
	# Set the user id
	#
	fd := sys->open("/dev/user", sys->OWRITE);
	if(fd == nil) {
		sys->print("failed to open /dev/user: %r");
		return;
	}
	b := array of byte "internet";
	if(sys->write(fd, b, len b) < 0) {
		sys->print("failed to write /dev/user: %r\n");
		return;
	}
	fd = nil;

	bufio = load Bufio Bufio->PATH;
	if(bufio == nil) {
		sys->print("failed to load bufio: %r\n");
		return;
	}
	memfs = load MemFS MemFS->PATH;
	if(memfs == nil) {
		sys->print("failed to load memfs: %r\n");
		return;
	}

	err = memfs->init();
	if(err != nil) {
		sys->print("%s\n", err);
		return;
	}

	memfd := memfs->newfs(1024 * 1024);
	if(memfd == nil) {
		sys->print("cannot create new memfs\n");
		return;
	}

	if(sys->mount(memfd, nil, "/usr/internet", Sys->MBEFORE | Sys->MCREATE, nil) == -1) {
		sys->print("failed to mount memfs on /usr/internet: %r\n");
		return;
	}

	# fetch download files
	if(sys->chdir("/usr/internet") == -1) {
		sys->print("failed to cd /usr/internet: %r\n");
		return;
	}
	cfd := sys->open("#C/cmd/clone", sys->ORDWR);
	if(cfd == nil)
		sys->print("Failed to open #C/cmd/clone: %r\n");

	idbuf := array[32] of byte;
	n := sys->read(cfd, idbuf, len idbuf);
	dir := "#C/cmd/"+string idbuf[0:n];
	n = sys->fprint(cfd, "exec <pluginpipe");		# Here's the hack!
	input := bufio->open(dir+"/data", Sys->OREAD);

	if(input == nil)
		return;
	# finalise the namespace
	sys->bind("#s", "/chan", Sys->MREPL);
	sys->bind("#p", "/prog", Sys->MREPL);
	sys->bind("#c", "/dev", Sys->MREPL);
	sys->bind("#i", "/dev", Sys->MBEFORE);
	sys->bind("#A", "/dev", Sys->MBEFORE);
	sys->bind("#e", "/env", Sys->MREPL|Sys->MCREATE);
	mkdir("/usr/internet/tmp", 8r777);
	sys->bind("/usr/internet/tmp", "/tmp", Sys->MREPL|Sys->MCREATE);
	sys->pctl(Sys->NODEVS, nil);

	buf := array [1024] of byte;
	for(;;) {
		fname := input.gets('\n');
		if(fname == nil || fname == "\n")
			break;
		perm := input.gets('\n');
		flen := input.gets('\n');
		if(perm == nil || flen == nil)
			break;
		nbytes := int flen;
		fname = fname[:len fname - 1];
		newf := bufio->create("/usr/internet/"+fname, Sys->OWRITE, int perm);
		if(newf == nil)
			sys->print("failed to create file %s: %r\n", fname);
		while(nbytes > 0) {
			need := len buf;
			if(need > nbytes)
				need = nbytes;
			nread := input.read(buf, need);
			if(nread <= 0)
				return;
			if(newf != nil)
				newf.write(buf, nread);
			nbytes -= nread;
		}
		if(newf != nil)
			newf.close();
	}
	cfd = nil;

	prefixes: list of string;
	if(len prog < 4 || prog[len prog - 4:] != ".dis")
		prog = prog + ".dis";
	if(prog[0] == '/' || prog[0:1] == "./")
		prefixes = "" :: nil;
	else
		prefixes = "/dis/" :: "./" :: nil;

	cmd: Command;
	for(; prefixes != nil; prefixes = tl prefixes) {
		path := hd prefixes + prog;
		cmd = load Command path;
		if(cmd != nil) {
			sys->print("about to run %s\n", path);
			break;
		}
	}

	if(cmd != nil)
		cmd->init(nil, args);
	else
		sys->print("failed to load cmd %s\n", prog);
	sys->print("DONE!\n");
}

attacharchfs(fspath: string): string
{
	cmd := "/dis/archfs.dis";
	archfs := load Command cmd;
	if(archfs == nil)
		return sys->sprint("cannot load %s: %r", cmd);
	{
		archfs->init(nil, cmd :: "-m" :: "/" :: fspath :: nil);
	} exception e {
	"*" =>
		return "exception loading FS: "+ string e;
	}
	return nil;
}

mkdir(dir: string, perm: int)
{
	sys->create(dir, Sys->OREAD, perm | Sys->DMDIR);
}

getargs(): list of string
{
	buf := array[Maxargs] of byte;
	ctlfd := sys->open("#c/consctl", Sys->OWRITE);
	if(ctlfd == nil)
		return nil;
	rawon := array of byte "rawon";
	sys->write(ctlfd, rawon, len rawon);

	fd := sys->open("#c/cons", Sys->OREAD);
	if(fd == nil)
		return nil;

	# get the arg length
	ntoread := 12;
	n := 0;
	while(n < ntoread) {
		nr := sys->read(fd, buf[n:], ntoread);
		if(nr <= 0)
			break;
		n += nr;
	}
	if(n != 12) {
		sys->print("failed to read arg length: %d %r\n", n);
		return nil;
	}

	n = 0;
	ntoread = int string buf[0:12];
	if(ntoread > Maxargs) {
		sys->print("args too long\n");
		return nil;
	}
	while(n < ntoread) {
		nr := sys->read(fd, buf[n:], ntoread);
		if(nr <= 0)
			break;
		n += nr;
	}
	return strings->unquoted(string buf[0:n]);
}
