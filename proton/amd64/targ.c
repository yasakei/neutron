#include "all.h"

#ifdef _MSC_VER
Amd64Op amd64_op[NOp] = {0};

void init_amd64_op(void)
{
	unsigned cur_op;
#undef X
#define X(nm, zf, lf) \
	amd64_op[cur_op].nmem = nm; \
	amd64_op[cur_op].zflag = zf; \
	amd64_op[cur_op].lflag = lf;
#define O(op, t, x) cur_op = O##op;
	#include "../ops.h"
#undef X
#undef O
}
#else
Amd64Op amd64_op[NOp] = {
#define O(op, t, x) [O##op] =
#define X(nm, zf, lf) { nm, zf, lf, },
	#include "../ops.h"
};
#endif

static int
amd64_memargs(int op)
{
	return amd64_op[op].nmem;
}

#define AMD64_COMMON \
	.gpr0 = RAX, \
	.ngpr = NGPR, \
	.fpr0 = XMM0, \
	.nfpr = NFPR, \
	.rglob = BIT(RBP) | BIT(RSP), \
	.nrglob = 2, \
	.rsave = amd64_sysv_rsave, \
	.nrsave = {NGPS, NFPS}, \
	.retregs = amd64_sysv_retregs, \
	.argregs = amd64_sysv_argregs, \
	.memargs = amd64_memargs, \
	.abi0 = elimsb, \
	.abi1 = amd64_sysv_abi, \
	.isel = amd64_isel, \
	.emitfn = amd64_emitfn, \
	.cansel = 1, \

Target T_amd64_sysv = {
	.name = "amd64_sysv",
	.emitfin = elf_emitfin,
	.asloc = ".L",
	AMD64_COMMON
};

Target T_amd64_apple = {
	.name = "amd64_apple",
	.apple = 1,
	.emitfin = macho_emitfin,
	.asloc = "L",
	.assym = "_",
	AMD64_COMMON
};
