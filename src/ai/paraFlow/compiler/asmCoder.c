/* asmCoder - driver for assembly code generation from parse tree. */

#include "common.h"
#include "dyString.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "isx.h"
#include "isxToPentium.h"
#include "optBranch.h"
#include "gnuMac.h"

#define asmSuffix ".s"
#define objSuffix ".o"

static void asmModule(struct pfCompile *pfc, struct pfParse *program,
	struct pfParse *module, char *baseDir)
/* Generate assembly for a single module */
{
FILE *asmFile;
FILE *isxFile, *branchFile; 
struct isxList *isxList, *modVarIsx;
char path[PATH_LEN];
char *baseName = module->name;

safef(path, sizeof(path), "%s%s.isx", baseDir, baseName);
isxFile = mustOpen(path, "w");
safef(path, sizeof(path), "%s%s.branch", baseDir, baseName);
branchFile = mustOpen(path, "w");
safef(path, sizeof(path), "%s%s%s", baseDir, baseName, asmSuffix);
asmFile = mustOpen(path, "w");

modVarIsx = isxModuleVars(pfc, module);

gnuMacModulePreamble(asmFile);
gnuMacInittedModuleVars(modVarIsx->iList, asmFile);
gnuMacMainStart(asmFile);

verbose(2, "Phase 7 - generating intermediate code\n");
isxList = isxFromParse(pfc, module);
isxDumpList(isxList->iList, isxFile);

verbose(2, "Phase 7a - optimizing branches\n");
optBranch(isxList->iList);
isxDumpList(isxList->iList, branchFile);

verbose(2, "Phase 8 - Pentium code generation\n");
pentFromIsx(isxList, asmFile);

gnuMacMainEnd(asmFile);
gnuMacUninittedModuleVars(modVarIsx->iList, asmFile);
gnuMacModulePostscript(asmFile);

carefulClose(&isxFile);
carefulClose(&branchFile);
carefulClose(&asmFile);
}

struct dyString *asmCoder(struct pfCompile *pfc, struct pfParse *program, 
	char *baseDir, char *baseName)
/* asmCoder - driver for assembly code generation from parse tree. 
 * Returns list of .s and .o files in string. */
{
struct pfParse *module;
struct dyString *gccFiles = dyStringNew(0);

for (module = program->children; module != NULL; module = module->next)
    {
    if (module->type == pptMainModule || module->type == pptModule)
	{
        asmModule(pfc, program, module, baseDir);
	dyStringPrintf(gccFiles, "%s%s%s ", baseDir, module->name, 
		asmSuffix);
	}
    else if (module->type == pptModuleRef)
        {
	if (module->name[0] != '<')
	    {
	    dyStringPrintf(gccFiles, "%s%s%s ", baseDir, module->name,
	    	objSuffix);
	    }
	}
    else
        internalErr();
    }
return gccFiles;
}

