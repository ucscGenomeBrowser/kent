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


static void finishIsx(struct pfCompile *pfc,
	struct isxList *isxList, FILE *isxFile, FILE *branchFile)
/* Make live list and do a little optimization on intermediate code,
 * and save for debugging. */
{
pentSubCallsForHardStuff(pfc, isxList);

isxLiveList(isxList);
isxDumpList(isxList->iList, isxFile);
fflush(isxFile);

optBranch(isxList->iList);
isxDumpList(isxList->iList, branchFile);
fflush(branchFile);

}

static void codeOutsideFunctions(struct pfCompile *pfc, struct pfParse *module, 
	struct hash *constHash, FILE *isxFile, FILE *branchFile, FILE *asmFile)
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
finishIsx(pfc, isxList, isxFile, branchFile);
pentCodeLocalConsts(isxList, constHash, asmFile);
pentFromIsx(isxList, pfi);
gnuMacMainStart(asmFile);
pentCodeSaveAll(pfi->coder->list, asmFile);
gnuMacMainEnd(asmFile);
pentCodeFreeList(&pfi->coder->list);
hashFree(&varHash);
}

static void codeFunction(struct pfCompile *pfc, struct pfParse *funcPp, 
	struct hash *constHash, FILE *isxFile, FILE *branchFile, 
	FILE *asmFile, struct pfParse *classPp)
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

for (pp = body->children; pp != NULL; pp = pp->next)
    isxStatement(pfc, pp, varHash, 1.0, isxList->iList);
isxAddReturnInfo(pfc, outTuple, varHash, isxList->iList);
pentInitFuncVars(pfc, ctar, varHash, pfi);
if (classPp != NULL)
    isGlobal = (classPp->ty->access == paGlobal && access != paLocal);
else
    isGlobal = (access == paGlobal);
finishIsx(pfc, isxList, isxFile, branchFile);
pentCodeLocalConsts(isxList, constHash, asmFile);
pentFromIsx(isxList, pfi);
pentFunctionStart(pfc, pfi, cName, isGlobal, asmFile);
pentCodeSaveAll(pfi->coder->list, asmFile);
pentFunctionEnd(pfc, pfi, asmFile);
pentCodeFreeList(&pfi->coder->list);
hashFree(&varHash);
}


static void codeFunctions(struct pfCompile *pfc, struct pfParse *parent, 
	struct hash *constHash, FILE *isxFile, FILE *branchFile, 
	FILE *asmFile, struct pfParse *classPp)
/* Generate code inside of functions */
{
struct isxList *isxList = isxListNew();
struct pfParse *pp;
for (pp = parent->children; pp != NULL; pp = pp->next)
    {
    switch (pp->type)
	{
	case pptClass:
	    codeFunctions(pfc, pp, constHash, isxFile, branchFile, asmFile, pp);
	    break;
	case pptToDec:
	case pptFlowDec:
	    codeFunction(pfc, pp, constHash, isxFile, branchFile, asmFile, 
	    	classPp);
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
struct hash *constHash = hashNew(0);

safef(path, sizeof(path), "%s%s.isx", baseDir, baseName);
isxFile = mustOpen(path, "w");
safef(path, sizeof(path), "%s%s.branch", baseDir, baseName);
branchFile = mustOpen(path, "w");
safef(path, sizeof(path), "%s%s%s", baseDir, baseName, asmSuffix);
asmFile = mustOpen(path, "w");

modVarIsx = isxModuleVars(pfc, module);
gnuMacModulePreamble(asmFile);
codeOutsideFunctions(pfc, module, constHash, isxFile, branchFile, asmFile);
codeFunctions(pfc, module, constHash, isxFile, branchFile, asmFile, NULL);
gnuMacModuleVars(modVarIsx->iList, asmFile);
gnuMacModulePostscript(asmFile);

hashFree(&constHash);
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

