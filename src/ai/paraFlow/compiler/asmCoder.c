/* asmCoder - driver for assembly code generation from parse tree. */

#include "common.h"
#include "dyString.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "isx.h"
#include "optBranch.h"
#include "gnuMac.h"
#include "ctar.h"
#include "pentCode.h"

#define asmSuffix ".s"
#define objSuffix ".o"

static void finishFuncOrOutside(struct pfCompile *pfc, struct isxList *isxList,
	FILE *isxFile, FILE *branchFile, FILE *asmFile, 
	struct pentFunctionInfo *pfi, boolean isFunc, char *cName,
	boolean isGlobal)
/* Finish up code generation for a function. */
{
isxLiveList(isxList);
isxDumpList(isxList->iList, isxFile);

verbose(2, "Phase 7a - optimizing branches\n");
optBranch(isxList->iList);
isxDumpList(isxList->iList, branchFile);

verbose(2, "Phase 8 - Pentium code generation\n");
pentFromIsx(isxList, pfi);
if (isFunc)
    pentFunctionStart(pfc, pfi, cName, isGlobal, asmFile);
pentCodeSaveAll(pfi->coder->list, asmFile);
if (isFunc)
    pentFunctionEnd(pfc, pfi, asmFile);
pentCodeFreeList(&pfi->coder->list);
}

static void codeOutsideFunctions(struct pfCompile *pfc, struct pfParse *module, 
	FILE *isxFile, FILE *branchFile, FILE *asmFile)
/* Generate code outside of functions */
{
struct isxList *isxList = isxListNew();
struct pfParse *pp;
struct hash *varHash = hashNew(0);
struct pentFunctionInfo *pfi = pentFunctionInfoNew();
for (pp = module->children; pp != NULL; pp = pp->next)
    {
    switch (pp->type)
	{
	case pptClass:
	case pptToDec:
	case pptFlowDec:
	    break;
	default:
	    isxStatement(pfc, pp, varHash, 1.0, isxList->iList);
	    break;
	}
    }
finishFuncOrOutside(pfc, isxList, isxFile, branchFile, asmFile, pfi, 
	FALSE, NULL, FALSE);
hashFree(&varHash);
}

static void codeFunction(struct pfCompile *pfc, struct pfParse *funcPp, 
	FILE *isxFile, FILE *branchFile, FILE *asmFile, struct pfParse *classPp)
/* Generate code for one function */
{
struct hash *varHash = hashNew(0);
struct isxList *isxList = isxListNew();
struct pfParse *namePp = funcPp->children;
struct pfParse *inTuple = namePp->next;
struct pfParse *outTuple = inTuple->next;
struct pfParse *body = outTuple->next;
struct ctar *ctar = ctarOnFunction(funcPp);
struct pfVar *funcVar = funcPp->var;
char *cName = funcVar->cName;
enum pfAccessType access = funcVar->ty->access;
struct pfParse *pp;
struct pentFunctionInfo *pfi = pentFunctionInfoNew();
boolean isGlobal;

fprintf(isxFile, "# Starting function %s\n", cName);
fprintf(branchFile, "# Starting function %s\n", cName);
fprintf(asmFile, "# Starting function %s\n", cName);

for (pp = body->children; body != NULL; body = body->next)
    isxStatement(pfc, pp, varHash, 1.0, isxList->iList);
pentInitFuncVars(pfc, ctar, varHash, pfi);
if (classPp != NULL)
    isGlobal = (classPp->ty->access == paGlobal && access != paLocal);
else
    isGlobal = (access == paGlobal);
finishFuncOrOutside(pfc, isxList, isxFile, branchFile, asmFile, pfi, TRUE,
    cName, isGlobal);
hashFree(&varHash);
}


static void codeFunctions(struct pfCompile *pfc, struct pfParse *parent, 
	FILE *isxFile, FILE *branchFile, FILE *asmFile, struct pfParse *classPp)
/* Generate code inside of functions */
{
struct isxList *isxList = isxListNew();
struct pfParse *pp;
for (pp = parent->children; pp != NULL; pp = pp->next)
    {
    switch (pp->type)
	{
	case pptClass:
	    codeFunctions(pfc, pp, isxFile, branchFile, asmFile, pp);
	    break;
	case pptToDec:
	case pptFlowDec:
	    codeFunction(pfc, pp, isxFile, branchFile, asmFile, classPp);
	    break;
	}
    }
}


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
codeOutsideFunctions(pfc, module, isxFile, branchFile, asmFile);
gnuMacMainEnd(asmFile);

codeFunctions(pfc, module, isxFile, branchFile, asmFile, NULL);

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

