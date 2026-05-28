#ifndef PROTON_H
#define PROTON_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compile SSA assembly text to target assembly,
 * writing the result to outf.
 *
 * Returns 0 on success, -1 on error.
 */
int proton_compile_ssa(const char *ssa, FILE *outf);

#ifdef __cplusplus
}
#endif

#endif
