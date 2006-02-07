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
#include "checkPoly.h"
#include "checkPara.h"
#include "cCoder.h"
#include "pfPreamble.h"
#include "defFile.h"
#include "parseInto.h"
#include "tokInto.h"


int endPhase = 10;

void usage()
/* Explain command line and exit. */
{
errAbort(
"This program compiles and executes paraFlow code.  ParaFlow is \n"
"a parallel programming language designed by Jim Kent.\n"
"Usage:\n"
"    paraFlow input.pf [command line arguments to input.pf]\n"
"This will create the files 'input.c' and 'input' (which is just\n"
"input.c compiled by gcc\n"
"options:\n"
"    -verbose=2 - Display info on the progress of things.\n"
"    -endPhase=N - Break after such-and-such a phase of compiler\n"
"                  Run it with verbose=2 to see phases defined.\n"
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
hashAddInt(hash, "continue", pftContinue);
hashAddInt(hash, "return", pftReturn);
hashAddInt(hash, "extends", pftExtends);
hashAddInt(hash, "polymorphic", pftPolymorphic);
hashAddInt(hash, "static", pftStatic);
hashAddInt(hash, "nil", pftNil);
hashAddInt(hash, "include", pftInclude);
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
    case pptMainModule:
    case pptFor:
    case pptForeach:
    case pptForEachCall:
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
pfc->moduleType = pfScopeAddType(scope, "<module>", FALSE, NULL, 0, FALSE);
pfc->varType = pfScopeAddType(scope, "var", FALSE, NULL, sizeof(_pf_Var), TRUE);
pfc->nilType = pfScopeAddType(scope, "nil", FALSE, NULL, sizeof(_pf_String), FALSE);
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

struct pfCompile *pfCompileNew()
/* Make new compiler object. */
{
struct pfCompile *pfc;
AllocVar(pfc);
pfc->moduleHash = hashNew(0);
pfc->reservedWords = createReservedWords();
pfc->scope = pfScopeNew(pfc, NULL, 8, FALSE);
addBuiltInTypes(pfc);
pfc->tkz = pfTokenizerNew(pfc->reservedWords);
return pfc;
}

void paraFlow(char *fileName, int pfArgc, char **pfArgv)
/* parse and dump. */
{
struct pfCompile *pfc;
struct pfParse *program, *module;
char baseDir[256], baseName[128], baseSuffix[64];
char defFile[PATH_LEN];
char *parseFile = "out.parse";
char *typeFile = "out.typed";
char *boundFile = "out.bound";
char *scopeFile = "out.scope";
char *cFile = "out.c";
FILE *parseF = mustOpen(parseFile, "w");
FILE *typeF = mustOpen(typeFile, "w");
FILE *scopeF = mustOpen(scopeFile, "w");
FILE *boundF = mustOpen(boundFile, "w");

if (endPhase < 0)
    return;
verbose(2, "Phase 0 - initialization\n");
pfc = pfCompileNew();
splitPath(fileName, baseDir, baseName, baseSuffix);
pfc->baseDir = cloneString(baseDir);
safef(defFile, sizeof(defFile), "%s%s.pfh", baseDir, baseName);

if (endPhase < 1)
   return ;
verbose(2, "Phase 1 - tokenizing\n");
pfTokenizeInto(pfc, baseDir, baseName);

if (endPhase < 2)
    return;
verbose(2, "Phase 2 - parsing\n");
program = pfParseInto(pfc);
pfParseDump(program, 0, parseF);
carefulClose(&parseF);

if (endPhase < 3)
    return;
verbose(2, "Phase 3 - nothing\n");

if (endPhase < 4)
    return;
verbose(2, "Phase 4 - binding names\n");
pfBindVars(pfc, program);
pfParseDump(program, 0, boundF);
carefulClose(&boundF);

if (endPhase < 5)
    return;
verbose(2, "Phase 5 - type checking\n");
pfTypeCheck(pfc, &program);
pfParseDump(program, 0, typeF);
carefulClose(&typeF);

if (endPhase < 6)
    return;
verbose(2, "Phase 6 - polymorphic, para, and flow checks\n");
checkPolymorphic(pfc, pfc->scopeList);
checkParaFlow(pfc, program);
printScopeInfo(scopeF, 0, program);
carefulClose(&scopeF);

if (endPhase < 7)
    return;
verbose(2, "Phase 7 - C code generation\n");
pfCodeC(pfc, program, baseDir, cFile);
verbose(2, "%d modules, %d tokens, %d parseNodes\n",
	pfc->moduleHash->elCount, pfc->tkz->tokenCount, pfParseCount(program));

if (endPhase < 8)
    return;
verbose(2, "Phase 8 - compiling C code\n");
/* Now run gcc on it. */
    {
    struct dyString *dy = dyStringNew(0);
    int err;
    for (module = program->children; module != NULL; module = module->next)
	{
	if (module->name[0] != '<' && module->type != pptModuleRef)
	    {
	    dyStringClear(dy);
	    dyStringAppend(dy, "gcc ");
	    dyStringAppend(dy, "-O ");
	    dyStringAppend(dy, "-I ~/kent/src/ai/paraFlow/compiler ");
	    dyStringAppend(dy, "-c ");
	    dyStringAppend(dy, "-o ");
	    dyStringAppend(dy, baseDir);
	    dyStringAppend(dy, module->name);
	    dyStringAppend(dy, ".o ");
	    dyStringAppend(dy, baseDir);
	    dyStringAppend(dy, module->name);
	    dyStringAppend(dy, ".c");
	    verbose(3, "%s\n", dy->string);
	    err = system(dy->string);
	    if (err != 0)
		errAbort("Couldn't compile %s.c", module->name);
	    }
	}
    dyStringClear(dy);
    dyStringAppend(dy, "gcc ");
    dyStringAppend(dy, "-O ");
    dyStringAppend(dy, "-I ~/kent/src/ai/paraFlow/compiler ");
    dyStringPrintf(dy, "-o %s%s ", baseDir, baseName);
    dyStringPrintf(dy, "%s ", cFile);
    for (module = program->children; module != NULL; module = module->next)
        {
	if (module->name[0] != '<')
	    {
	    dyStringPrintf(dy, "%s%s", baseDir, module->name);
	    dyStringPrintf(dy, ".o ");
	    }
	}
    dyStringAppend(dy, "~/kent/src/ai/paraFlow/runtime/runtime.a ");
    dyStringPrintf(dy, "~/kent/src/lib/%s/jkweb.a", getenv("MACHTYPE"));
    verbose(3, "%s\n", dy->string);
    err = system(dy->string);
    if (err != 0)
	errnoAbort("problem compiling:\n", dy->string);
    dyStringFree(&dy);
    }

if (endPhase < 9)
    return;

verbose(2, "Phase 9 - execution\n");
/* Now go run program itself. */
    {
    struct dyString *dy = dyStringNew(0);
    int err;
    int i;
    if (baseDir[0] == 0)
	dyStringPrintf(dy, "./%s", baseName);
    else
        dyStringPrintf(dy, "%s%s", baseDir, baseName);
    for (i=0; i<pfArgc; ++i)
        {
	dyStringAppendC(dy, ' ');
	dyStringAppend(dy, pfArgv[i]);
	}
    err = system(dy->string);
    if (err != 0)
	errAbort("problem running %s", baseName);
    dyStringFree(&dy);
    }
}

int main(int argc, char *argv[], char *environ[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
endPhase = optionInt("endPhase", endPhase);
assert(pptTypeCount <= 256);
paraFlow(argv[1], argc-2, argv+2);
return 0;
}

