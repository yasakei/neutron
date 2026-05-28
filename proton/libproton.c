#include "all.h"
#include "config.h"

extern Target T_amd64_sysv;
extern Target T_amd64_apple;
extern Target T_arm64;
extern Target T_arm64_apple;
extern Target T_rv64;

Target T;
char debug['Z'+1] = {0};

static FILE *g_outf;

static void
data_cb(Dat *d)
{
	emitdat(d, g_outf);
	if (d->type == DEnd) {
		fputs("/* end data */\n\n", g_outf);
		freeall();
	}
}

static void
func_cb(Fn *fn)
{
	uint n;

	T.abi0(fn);
	fillcfg(fn);
	filluse(fn);
	promote(fn);
	filluse(fn);
	ssa(fn);
	filluse(fn);
	ssacheck(fn);
	fillalias(fn);
	loadopt(fn);
	filluse(fn);
	fillalias(fn);
	coalesce(fn);
	filluse(fn);
	filldom(fn);
	ssacheck(fn);
	gvn(fn);
	fillcfg(fn);
	simplcfg(fn);
	filluse(fn);
	filldom(fn);
	gcm(fn);
	filluse(fn);
	ssacheck(fn);
	if (T.cansel) {
		ifconvert(fn);
		fillcfg(fn);
		filluse(fn);
		filldom(fn);
		ssacheck(fn);
	}
	T.abi1(fn);
	simpl(fn);
	fillcfg(fn);
	filluse(fn);
	T.isel(fn);
	fillcfg(fn);
	filllive(fn);
	fillloop(fn);
	fillcost(fn);
	spill(fn);
	rega(fn);
	fillcfg(fn);
	simpljmp(fn);
	fillcfg(fn);
	assert(fn->rpo[0] == fn->start);
	for (n=0;; n++)
		if (n == fn->nblk-1) {
			fn->rpo[n]->link = 0;
			break;
		} else
			fn->rpo[n]->link = fn->rpo[n+1];
	T.emitfn(fn, g_outf);
	fprintf(g_outf, "/* end function %s */\n\n", fn->name);
	freeall();
}

static void
dbgfile_cb(char *fn)
{
	(void)fn;
}

int
proton_compile_ssa(const char *ssa, FILE *outf)
{
	FILE *inf;

	g_outf = outf;
	T = Deftgt;

	inf = fmemopen((void *)ssa, strlen(ssa), "r");
	if (!inf)
		return -1;

	parse(inf, "<ssa>", dbgfile_cb, data_cb, func_cb);
	fclose(inf);

	T.emitfin(outf);

	return 0;
}
