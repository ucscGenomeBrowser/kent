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
pfc->tupleType = pfScopeAddType(scope, "<tuple>", TRUE, pfc->collectionType, 0, FALSE);
pfc->classType = pfScopeAddType(scope, "<class>", TRUE, pfc->collectionType, 0, FALSE);
pfc->functionType = pfScopeAddType(scope, "<function>", TRUE, pfc->varType, 0, FALSE);
pfc->toType = pfScopeAddType(scope, "to", TRUE, pfc->functionType, 0, FALSE);
pfc->paraType = pfScopeAddType(scope, "para", TRUE, pfc->functionType, 0, FALSE);
pfc->flowType = pfScopeAddType(scope, "flow", TRUE, pfc->functionType, 0, FALSE);

pfc->bitType = pfScopeAddType(scope, "bit", FALSE, pfc->numType, sizeof(_pf_Bit), FALSE);
pfc->byteType = pfScopeAddType(scope, "byte", FALSE, pfc->numType, sizeof(_pf_Byte), FALSE);
pfc->shortType = pfScopeAddType(scope, "short", FALSE, pfc->numType, sizeof(_pf_Short), FALSE);
pfc->intType = pfScopeAddType(scope, "int", FALSE, pfc->numType, sizeof(_pf_Int), FALSE);
pfc->longType = pfScopeAddType(scope, "long", FALSE, pfc->numType, sizeof(_pf_Long), FALSE);
pfc->floatType = pfScopeAddType(scope, "float", FALSE, pfc->numType, sizeof(_pf_Float), FALSE);
pfc->doubleType = pfScopeAddType(scope, "double", FALSE, pfc->numType, sizeof(_pf_Double), FALSE);

pfc->stringType = pfScopeAddType(scope, "string", FALSE, pfc->streamType, sizeof(_pf_String), FALSE);

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
}

struct pfCompile *pfCompileNew(char *fileName)
/* Make new compiler object. */
{
struct pfCompile *pfc;
AllocVar(pfc);
pfc->baseFile = cloneString(fileName);
pfc->modules = hashNew(0);
pfc->reservedWords = createReservedWords();
pfc->scope = pfScopeNew(pfc, NULL, 8, FALSE);
addBuiltInTypes(pfc);
addBuiltInFunctions(pfc);
pfc->tkz = pfTokenizerNew(fileName, pfc->reservedWords);
return pfc;
}

void paraFlow(char *fileName)
/* parse and dump. */
{
struct pfCompile *pfc;
struct pfParse *program;
char *parseFile = "out.parse";
char *typeFile = "out.typed";
char *scopeFile = "out.scope";
char *codeFile = "out.c";
FILE *parseF = mustOpen(parseFile, "w");
FILE *typeF = mustOpen(typeFile, "w");
FILE *scopeF = mustOpen(scopeFile, "w");

verbose(2, "Phase 1\n");
pfc = pfCompileNew(fileName);
if (endPhase <= 1)
    return;
verbose(2, "Phase 2\n");
program = pfParseProgram(fileName, pfc);
pfParseDump(program, 0, parseF);
if (endPhase <= 2)
    return;
verbose(2, "Phase 3\n");
pfBindVars(pfc, program);
if (endPhase <= 3)
    return;
verbose(2, "Phase 4\n");
pfTypeCheck(pfc, &program);
if (endPhase <= 4)
    return;
pfCheckParaFlow(pfc, program);
verbose(2, "Phase 5\n");
if (endPhase <= 5)
    return;
pfParseDump(program, 0, typeF);
printScopeInfo(scopeF, 0, program);
pfCodeC(pfc, program, codeFile);
printf("%d modules, %d tokens, %d parseNodes\n",
	pfc->modules->elCount, pfc->tkz->tokenCount, pfParseCount(program));
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

