/* cMain - Print out our main module, which is in C even if
 * we are generating the bulk of the code in assembler. */

#ifndef CMAIN_H
#define CMAIN_H

#ifndef PFSTRUCT_H
#include "pfStruct.h"
#endif

void cMain(struct pfCompile *pfc, struct pfParse *program, char *fileName);
/* cMain - Print out our main module, which is in C even if
 * we are generating the bulk of the code in assembler.  This
 * includes type info, a module table, and the main entry point. */

void cPrintPreamble(struct pfCompile *pfc, FILE *f, char *fileName, 
	boolean doExtern);
/* Print out C code for preamble for each module. */

#define pfcGlobalPrefix "pf_"
/* Global C symbols start with this in general. */

/* Names for some tables and the like */
#define cModuleRuntimeName "_pf_lmodule"
#define cModuleRuntimeList "_pf_moduleList"
#define cModuleRuntimeType "_pf_moduleRuntime"

extern char *cStackName;	/* Name of var holding run time stack */
extern char *cStackType;	/* Type of var holding run time stack */

#endif /* CMAIN_H */
