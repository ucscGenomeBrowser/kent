/* Command line main driver module for paraFlow compiler. */
/* Copyright 2005 Jim Kent.  All rights reserved. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "hash.h"
#include "dystring.h"
#include "localmem.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "pfBindVars.h"
#include "pfCheck.h"
#include "pfCodeC.h"
#include "pfPreamble.h"


int endPhase = 10;

void usage()
/* Explain command line and exit. */
{
errAbort(
"This program compiles paraFlow code.  ParaFlow is a parallel programming\n"
"language designed by Jim Kent.\n"
"Usage:\n"
"    paraFlow input.pf\n"
"options:\n"
"    -endPhase=N - Break after such-and-such a phase of compiler\n"
);

}

static struct optionSpec options[] = {
   {"endPhase", OPTION_INT},
   {NULL, 0},
};

static struct hash *createReservedWords()
/* Create reserved words table. */
{
struct hash *hash = hashNew(7);
hashAddInt(hash, "class", pftClass);
hashAddInt(hash, "to", pftTo);
hashAddInt(hash, "para", pftPara);
hashAddInt(hash, "flow", pftFlow);
hashAddInt(hash, "into", pftInto);
hashAddInt(hash, "for", pftFor);
hashAddInt(hash, "foreach", pftForeach);
hashAddInt(hash, "while", pftWhile);
hashAddInt(hash, "of", pftOf);
hashAddInt(hash, "if", pftIf);
hashAddInt(hash, "else", pftElse);
hashAddInt(hash, "break", pftBreak);
hashAddInt(hash, "return", pftReturn);
hashAddInt(hash, "continue", pftContinue);
return hash;
}

static void rParseCount(int *pCount, struct pfParse *pp)
/* Recursively count. */
{
*pCount += 1;
for (pp = pp->children; pp != NULL; pp = pp->next)
   rParseCount(pCount, pp);
}

int pfParseCount(struct pfParse *pp)
/* Count up number of parses in this el and children. */
{
int parseCount = 0;
rParseCount(&parseCount, pp);
return parseCount;
}

static void printScopeInfo(FILE *f, int level, struct pfParse *pp)
/* Print out info on each new scope. */
{
switch (pp->type)
    {
    case pptProgram:
    case pptModule:
    case pptFor:
    case pptForeach:
    case pptToDec:
    case pptFlowDec:
    case pptParaDec:
    case pptCompound:
	{
	char *name = pp->name;
	if (name == NULL)
	    name = "";
        spaceOut(f, 2*level);
	fprintf(f, "scope %d %s %s: ", pp->scope->id,
		pfParseTypeAsString(pp->type), name);
	pfScopeDump(pp->scope, f);
	fprintf(f, "\n");
	break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    printScopeInfo(f, level+1, pp);
}

static void addBuiltInTypes(struct pfCompile *pfc)
/* Add built in types . */
{
struct pfScope *scope = pfc->scope;

/* Declare some basic types.  Types with names in parenthesis
 * are never declared by user directly */
pfc->varType = pfScopeAddType(scope, "var", FALSE, NULL, sizeof(_pf_Var), TRUE);
pfc->keyValType = pfScopeAddType(scope, "<keyVal>", FALSE, pfc->varType, 0, FALSE);
pfc->streamType = pfScopeAddType(scope, "<stream>", FALSE, pfc->varType, 0, FALSE);
pfc->numType = pfScopeAddType(scope, "<number>", FALSE, pfc->varType, 0, FALSE);
pfc->collectionType = pfScopeAddType(scope, "<collection>", TRUE, pfc->varType, 0, FALSE);
pfc->tupleType = pfScopeAddType(scope, "<tuple>", FALSE, pfc->varType, 0, FALSE);
pfc->classType = pfScopeAddType(scope, "<class>", FALSE, pfc->varType, 0, FALSE);
pfc->functionType = pfScopeAddType(scope, "<function>", FALSE, pfc->varType, 0, FALSE);
pfc->toType = pfScopeAddType(scope, "to", FALSE, pfc->functionType, 0, FALSE);
pfc->paraType = pfScopeAddType(scope, "para", FALSE, pfc->functionType, 0, FALSE);
pfc->flowType = pfScopeAddType(scope, "flow", FALSE, pfc->functionType, 0, FALSE);

pfc->bitType = pfScopeAddType(scope, "bit", FALSE, pfc->numType, sizeof(_pf_Bit), FALSE);
pfc->byteType = pfScopeAddType(scope, "byte", FALSE, pfc->numType, sizeof(_pf_Byte), FALSE);
pfc->shortType = pfScopeAddType(scope, "short", FALSE, pfc->numType, sizeof(_pf_Short), FALSE);
pfc->intType = pfScopeAddType(scope, "int", FALSE, pfc->numType, sizeof(_pf_Int), FALSE);
pfc->intFullType = pfTypeNew(pfc->intType);
pfc->longType = pfScopeAddType(scope, "long", FALSE, pfc->numType, sizeof(_pf_Long), FALSE);
pfc->floatType = pfScopeAddType(scope, "float", FALSE, pfc->numType, sizeof(_pf_Float), FALSE);
pfc->doubleType = pfScopeAddType(scope, "double", FALSE, pfc->numType, sizeof(_pf_Double), FALSE);

pfc->stringType = pfScopeAddType(scope, "string", TRUE, pfc->streamType, sizeof(_pf_String), TRUE);

pfc->arrayType = pfScopeAddType(scope, "array", TRUE, pfc->collectionType, sizeof(_pf_Array), TRUE);
pfc->listType = pfScopeAddType(scope, "list", TRUE, pfc->collectionType, sizeof(_pf_List), TRUE);
pfc->treeType = pfScopeAddType(scope, "tree", TRUE, pfc->collectionType, sizeof(_pf_Tree), TRUE);
pfc->treeType->keyedBy = pfc->doubleType;
pfc->dirType = pfScopeAddType(scope, "dir", TRUE, pfc->collectionType, sizeof(_pf_Dir), TRUE);
pfc->dirType->keyedBy = pfc->stringType;
}

static void addBuiltInFunctions(struct pfCompile *pfc)
/* Add built in functions */
{
#ifdef OLD
struct pfScope *scope = pfc->scope;
struct pfType *toType = pfTypeNew(pfc->toType);
struct pfType *inTuple = pfTypeNew(pfc->tupleType);
struct pfType *outTuple = pfTypeNew(pfc->tupleType);
struct pfType *varType = pfTypeNew(pfc->varType);
toType->children = inTuple;
toType->isFunction = TRUE;
inTuple->next = outTuple;
inTuple->children = varType;
pfScopeAddVar(scope, "print", toType, NULL);
pfScopeAddVar(scope, "prin", toType, NULL);
#endif /* OLD */
}

struct pfCompile *pfCompileNew()
/* Make new compiler object. */
{
struct pfCompile *pfc;
AllocVar(pfc);
pfc->modules = hashNew(0);
pfc->reservedWords = createReservedWords();
pfc->scope = pfScopeNew(pfc, NULL, 8, FALSE);
addBuiltInTypes(pfc);
addBuiltInFunctions(pfc);
pfc->tkz = pfTokenizerNew(pfc->reservedWords);
return pfc;
}

void paraFlow(char *fileName)
/* parse and dump. */
{
struct pfCompile *pfc;
struct pfParse *program;
char baseDir[256], baseName[128], baseSuffix[64];
char codeFile[PATH_LEN];
char *parseFile = "out.parse";
char *typeFile = "out.typed";
char *boundFile = "out.bound";
char *scopeFile = "out.scope";
FILE *parseF = mustOpen(parseFile, "w");
FILE *typeF = mustOpen(typeFile, "w");
FILE *scopeF = mustOpen(scopeFile, "w");
FILE *boundF = mustOpen(boundFile, "w");

verbose(2, "Phase 1 - initialization\n");
splitPath(fileName, baseDir, baseName, baseSuffix);
safef(codeFile, sizeof(codeFile), "%s%s.c", baseDir, baseName);
pfc = pfCompileNew();
if (endPhase <= 1)
    return;
verbose(2, "Phase 2 - parsing\n");
program = pfParseProgram(pfc, fetchBuiltinCode(), fileName);
pfParseDump(program, 0, parseF);
carefulClose(&parseF);
if (endPhase <= 2)
    return;
verbose(2, "Phase 3 - binding names\n");
pfBindVars(pfc, program);
pfParseDump(program, 0, boundF);
carefulClose(&boundF);
if (endPhase <= 3)
    return;
verbose(2, "Phase 4 - type checking\n");
pfTypeCheck(pfc, &program);
if (endPhase <= 4)
    return;
pfCheckParaFlow(pfc, program);
pfParseDump(program, 0, typeF);
carefulClose(&typeF);
printScopeInfo(scopeF, 0, program);
carefulClose(&scopeF);
verbose(2, "Phase 5 - C code generation\n");
if (endPhase <= 5)
    return;
pfCodeC(pfc, program, codeFile);
verbose(2, "%d modules, %d tokens, %d parseNodes\n",
	pfc->modules->elCount, pfc->tkz->tokenCount, pfParseCount(program));
if (endPhase <= 6)
    return;
verbose(2, "Phase 6 - compiling C code\n");
/* Now run gcc on it. */
    {
    struct dyString *dy = dyStringNew(0);
    int err;
    dyStringAppend(dy, "gcc ");
    dyStringAppend(dy, "-I ~/src/ai/paraFlow/compiler ");
    dyStringPrintf(dy, "-o %s ", baseName);
    dyStringPrintf(dy, "%s ", codeFile);
    dyStringAppend(dy, "~/src/ai/paraFlow/runtime/runtime.a ~/src/lib/powerpc/jkweb.a");
    err = system(dy->string);
    if (err != 0)
	errnoAbort("problem compiling %s", codeFile);
    dyStringFree(&dy);
    }

if (endPhase <= 7)
    return;

verbose(2, "Phase 7 - execution\n");
/* Now go run program itself. */
    {
    struct dyString *dy = dyStringNew(0);
    dyStringPrintf(dy, "./%s", baseName);
    int err = system(dy->string);
    if (err != 0)
	errnoAbort("problem running %s", baseName);
    dyStringFree(&dy);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
endPhase = optionInt("endPhase", endPhase);
paraFlow(argv[1]);
return 0;
}

