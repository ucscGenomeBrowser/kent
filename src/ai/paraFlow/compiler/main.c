/* Command line main driver module for paraFlow compiler. */
/* Copyright 2005 Jim Kent.  All rights reserved. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "hash.h"
#include "dystring.h"
#include "localmem.h"
#include "ra.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "pfBindVars.h"
#include "checkPoly.h"
#include "checkPara.h"
#include "constFold.h"
#include "cCoder.h"
#include "asmCoder.h"
#include "pfPreamble.h"
#include "defFile.h"
#include "parseInto.h"
#include "tokInto.h"


int endPhase = 12;

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
"    -asm  - Generate assembly rather than C code\n"
);
}

static struct optionSpec options[] = {
   {"endPhase", OPTION_INT},
   {"asm", OPTION_BOOLEAN},
   {NULL, 0},
};

static struct hash *createReservedWords()
/* Create reserved words table. */
{
struct hash *hash = hashNew(7);
hashAddInt(hash, "break", pftBreak);
hashAddInt(hash, "case", pftCase);
hashAddInt(hash, "catch", pftCatch);
hashAddInt(hash, "const", pftConst);
hashAddInt(hash, "continue", pftContinue);
hashAddInt(hash, "class", pftClass);
hashAddInt(hash, "else", pftElse);
hashAddInt(hash, "extends", pftExtends);
hashAddInt(hash, "if", pftIf);
hashAddInt(hash, "import", pftImport);
hashAddInt(hash, "in", pftIn);
hashAddInt(hash, "include", pftInclude);
hashAddInt(hash, "interface", pftInterface);
hashAddInt(hash, "into", pftInto);
hashAddInt(hash, "flow", pftFlow);
hashAddInt(hash, "for", pftFor);
hashAddInt(hash, "global", pftGlobal);
hashAddInt(hash, "local", pftLocal);
hashAddInt(hash, "new", pftNew);
hashAddInt(hash, "nil", pftNil);
hashAddInt(hash, "of", pftOf);
hashAddInt(hash, "para", pftPara);
hashAddInt(hash, "morph", pftPolymorphic);
hashAddInt(hash, "readable", pftReadable);
hashAddInt(hash, "return", pftReturn);
hashAddInt(hash, "static", pftStatic);
hashAddInt(hash, "to", pftTo);
hashAddInt(hash, "til", pftTil);
hashAddInt(hash, "try", pftTry);
hashAddInt(hash, "while", pftWhile);
hashAddInt(hash, "writable", pftWritable);
hashAddInt(hash, "_operator_", pftOperator);
return hash;
}

char *pfAccessTypeAsString(enum pfAccessType pa)
/* Return string representation of access qualifier. */
{
switch (pa)
    {
    case paUsual:
	return "usual";
    case paGlobal:
	return "global";
    case paLocal:
	return "local";
    case paReadable:
	return "readable";
    case paStatic:
	return "static";
    case paWritable:
	return "writable";
    default:
        internalErr();
	return NULL;
    }
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

struct pfBaseType *addGlobalType(struct pfScope *scope, char *name, 
	boolean isCollection, struct pfBaseType *parentType, int size, 
	boolean needsCleanup)
/* Add type to scope, and make it globally accessible. */
{
struct pfBaseType *base = pfScopeAddType(scope, name, isCollection, parentType,
	size, needsCleanup);
base->access = paGlobal;
return base;
}

static void addBuiltInTypes(struct pfCompile *pfc)
/* Add built in types . */
{
struct pfScope *scope = pfc->scope;

/* Declare some basic types.  Types with names in parenthesis
 * are never declared by user directly */
pfc->moduleType = addGlobalType(scope, "<module>", FALSE, NULL, 0, FALSE);
pfc->moduleFullType = pfTypeNew(pfc->moduleType);
pfc->varType = addGlobalType(scope, "var", FALSE, NULL, sizeof(_pf_Var), TRUE);
pfc->nilType = addGlobalType(scope, "nil", FALSE, NULL, sizeof(_pf_String), FALSE);
pfc->keyValType = addGlobalType(scope, "<keyVal>", FALSE, pfc->varType, 0, FALSE);
pfc->streamType = addGlobalType(scope, "<stream>", FALSE, pfc->varType, 0, FALSE);
pfc->numType = addGlobalType(scope, "<number>", FALSE, pfc->varType, 0, FALSE);
pfc->collectionType = addGlobalType(scope, "<collection>", TRUE, pfc->varType, 0, FALSE);
pfc->tupleType = addGlobalType(scope, "<tuple>", FALSE, pfc->varType, 0, FALSE);
pfc->indexRangeType = addGlobalType(scope, "<indexRange>", TRUE, pfc->collectionType, 0, FALSE);
pfc->classType = addGlobalType(scope, "<class>", FALSE, pfc->varType, 0, FALSE);
pfc->interfaceType = addGlobalType(scope, "<interface>", FALSE, pfc->varType, 0, FALSE);
pfc->functionType = addGlobalType(scope, "<function>", FALSE, pfc->varType, 0, FALSE);
pfc->toPtType = addGlobalType(scope, "<toPt>", FALSE, pfc->varType, 0, FALSE);
pfc->flowPtType = addGlobalType(scope, "<flowPt>", FALSE, pfc->varType, 0, FALSE);
pfc->toType = addGlobalType(scope, "to", FALSE, pfc->functionType, 0, FALSE);
pfc->flowType = addGlobalType(scope, "flow", FALSE, pfc->functionType, 0, FALSE);
pfc->operatorType = addGlobalType(scope, "_operator_", FALSE, pfc->functionType, 0, FALSE);

pfc->bitType = addGlobalType(scope, "bit", FALSE, pfc->numType, sizeof(_pf_Bit), FALSE);
pfc->bitFullType = pfTypeNew(pfc->bitType);
pfc->byteType = addGlobalType(scope, "byte", FALSE, pfc->numType, sizeof(_pf_Byte), FALSE);
pfc->byteFullType = pfTypeNew(pfc->byteType);
pfc->shortType = addGlobalType(scope, "short", FALSE, pfc->numType, sizeof(_pf_Short), FALSE);
pfc->shortFullType = pfTypeNew(pfc->shortType);
pfc->intType = addGlobalType(scope, "int", FALSE, pfc->numType, sizeof(_pf_Int), FALSE);
pfc->intFullType = pfTypeNew(pfc->intType);
pfc->longType = addGlobalType(scope, "long", FALSE, pfc->numType, sizeof(_pf_Long), FALSE);
pfc->longFullType = pfTypeNew(pfc->longType);
pfc->floatType = addGlobalType(scope, "float", FALSE, pfc->numType, sizeof(_pf_Float), FALSE);
pfc->floatFullType = pfTypeNew(pfc->floatType);
pfc->doubleType = addGlobalType(scope, "double", FALSE, pfc->numType, sizeof(_pf_Double), FALSE);
pfc->doubleFullType = pfTypeNew(pfc->doubleType);
pfc->charType = addGlobalType(scope, "char", FALSE, pfc->numType, sizeof(_pf_Char), FALSE);
pfc->charFullType = pfTypeNew(pfc->charType);

pfc->stringType = addGlobalType(scope, "string", TRUE, pfc->streamType, sizeof(_pf_String), TRUE);
pfc->stringType->keyedBy = pfc->longType;
pfc->dyStringType = addGlobalType(scope, "dyString", TRUE, pfc->stringType, sizeof(_pf_String), TRUE);
pfc->dyStringType->keyedBy = pfc->longType;
pfc->arrayType = addGlobalType(scope, "array", TRUE, pfc->collectionType, sizeof(_pf_Array), TRUE);
pfc->arrayType->keyedBy = pfc->longType;
pfc->dirType = addGlobalType(scope, "dir", TRUE, pfc->collectionType, sizeof(_pf_Dir), TRUE);
pfc->dirType->keyedBy = pfc->stringType;
//pfc->listType = addGlobalType(scope, "list", TRUE, pfc->collectionType, sizeof(_pf_List), TRUE);
//pfc->listType->keyedBy = pfc->longType;
//pfc->treeType = addGlobalType(scope, "tree", TRUE, pfc->collectionType, sizeof(_pf_Tree), TRUE);
//pfc->treeType->keyedBy = pfc->doubleType;
}

boolean pfBaseTypeIsPassedByValue(struct pfCompile *pfc, 
	struct pfBaseType *base)
/* Return TRUE if this type is passed by value.  (Strings
 * since they are read-only are considered to be passed by
 * value. */
{
return  (base == pfc->stringType || base->parent == pfc->numType);
}

struct pfCompile *pfCompileNew()
/* Make new compiler object. */
{
struct pfCompile *pfc;
AllocVar(pfc);
pfc->moduleHash = hashNew(0);
pfc->reservedWords = createReservedWords();
pfc->scope = pfScopeNew(pfc, NULL, 8, NULL);
addBuiltInTypes(pfc);
pfc->tkz = pfTokenizerNew(pfc->reservedWords);
return pfc;
}

static void dumpParseTree(struct pfCompile *pfc, 
	struct pfParse *program, FILE *f)
/* Dump out parse tree, including string module, which for 
 * technical reasons is not linked onto main parse tree. */
{
struct pfModule *stringModule = hashMustFindVal(pfc->moduleHash, "<string>");
pfParseDump(program, 0, f);
pfParseDump(stringModule->pp, 1, f);
}

char *mangledModuleName(char *modName)
/* Return mangled version of module name that hopefully someday
 * will be unique across directories, but for now is just the last
 * bit. */
{
char *name = strrchr(modName, '/');
if (name == NULL)
    name = modName;
else
    name += 1;
return name;
}

static char *mustFindSetting(struct hash *hash, char *name, char *fileName)
/* Find named setting in hash or squawk and die. */
{
char *val = hashFindVal(hash, name);
if (val == NULL)
   errAbort("Can't find required setting %s in %s\n", name, fileName);
return val;
}

static char *expandTilde(char *fileName)
/* If file name starts with ~, then replace that with
 * HOME dir. */
{
if (fileName[0] == '~' && fileName[1] == '/')
    {
    char *home = getenv("HOME");
    int homeLen, oldLen;
    char *expanded;
    if (home == NULL)
        errAbort("Can't find home");
    homeLen = strlen(home);
    oldLen = strlen(fileName);
    expanded = needMem(homeLen + oldLen);
    memcpy(expanded, home, homeLen);
    memcpy(expanded+homeLen, fileName+1, oldLen);
    fileName = expanded;
    }
return fileName;
}

static struct slName *parsePath(char *path)
/* Convert : separated path into a name list. */
{
struct slName *list = NULL;
char *dupe = cloneString(path);
char *s, *e;
for (s = dupe; s != NULL && s[0] != 0; s = e)
    {
    e = strchr(s, ':');
    if (e != NULL)
       *e++ = 0;
    slNameAddTail(&list, expandTilde(s));
    }
freeMem(dupe);
return list;
}

static void getPathsFromFile(struct pfCompile *pfc, char *fileName)
/* Read in several fields of pfc from file. */
{
struct hash *hash = raReadSingle(fileName);
char *libPath;
pfc->cfgHash = hash;
pfc->cIncludeDir = mustFindSetting(hash, "cIncludeDir", fileName);
pfc->runtimeLib = mustFindSetting(hash, "runtimeLib", fileName);
pfc->jkwebLib = mustFindSetting(hash, "jkwebLib", fileName);
libPath = mustFindSetting(hash, "paraLibPath", fileName);
pfc->paraLibPath = parsePath(libPath);
}

static void getPaths(struct pfCompile *pfc)
/* Figure out where paraFlowPaths is and read it. */
{
char *fileName = getenv("PARAFLOWCFG");
struct dyString *dy = dyStringNew(0);

if (fileName == NULL)
    {
    char *home = getenv("HOME");
    if (home == NULL)
	errAbort("Can't find HOME environment variable.");
    dyStringClear(dy);
    dyStringPrintf(dy, "%s/paraFlow/paraFlow.cfg", home);
    fileName = cloneString(dy->string);
    }
if (!fileExists(fileName))
    {
    errAbort(
       "Can't find %s file.\nPlease see ParaFlow installation instructions."
       , fileName);
    }
getPathsFromFile(pfc, fileName);
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
char *foldedFile = "out.folded";
char *cFile = "out.c";
FILE *parseF = mustOpen(parseFile, "w");
FILE *typeF = mustOpen(typeFile, "w");
FILE *scopeF = mustOpen(scopeFile, "w");
FILE *boundF = mustOpen(boundFile, "w");
FILE *foldedF = mustOpen(foldedFile, "w");

if (endPhase < 0)
    return;
verbose(2, "Phase 0 - initialization\n");
pfc = pfCompileNew();
getPaths(pfc);
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
dumpParseTree(pfc, program, parseF);
carefulClose(&parseF);

if (endPhase < 3)
    return;
verbose(2, "Phase 3 - binding names\n");
pfBindVars(pfc, program);
dumpParseTree(pfc, program, boundF);
carefulClose(&boundF);

if (endPhase < 4)
    return;
verbose(2, "Phase 4 - type checking\n");
pfTypeCheck(pfc, &program);
dumpParseTree(pfc, program, typeF);
carefulClose(&typeF);

if (endPhase < 5)
    return;
verbose(2, "Phase 5 - polymorphic, para, and flow checks\n");
checkPolymorphic(pfc, pfc->scopeRefList);
checkParaFlow(pfc, program);
printScopeInfo(scopeF, 0, program);
carefulClose(&scopeF);

if (endPhase < 6)
    return;
verbose(2, "Phase 6 - constant folding\n");
pfConstFold(pfc, program);
dumpParseTree(pfc, program, foldedF);

if (optionExists("asm"))
    {
    struct dyString *gccFiles;

    if (endPhase < 7)
	return;
    verbose(2, "Phase 7 - nothing\n");

    if (endPhase < 8)
	return;
    verbose(2, "Phase 8 - Code generation\n");

    gccFiles = asmCoder(pfc, program, baseDir, baseName);

    if (endPhase < 9)
        return;
    verbose(2, "Phase 9 - Assembling pentium code\n");
        {
	char *libName = hashMustFindVal(pfc->cfgHash,"runAsmLib");
	struct dyString *dy = dyStringNew(0);
	int err;
	dyStringPrintf(dy, "gcc -o %s%s ", baseDir, baseName);
	dyStringAppend(dy, gccFiles->string);
	dyStringPrintf(dy, "%s ", libName);
	dyStringPrintf(dy, " %s ", pfc->runtimeLib);
	dyStringPrintf(dy, "%s ", pfc->jkwebLib);
	verbose(2, "%s\n", dy->string);
	err = system(dy->string);
	if (err != 0)
	    errAbort("Couldn't assemble: %s", dy->string);
	dyStringFree(&dy);
	}
    dyStringFree(&gccFiles);
    }
else
    {
    verbose(2, "Phase 7 - nothing\n");
    if (endPhase < 8)
	return;
    verbose(2, "Phase 8 - C code generation\n");
    pfCodeC(pfc, program, baseDir, cFile);
    verbose(2, "%d modules, %d tokens, %d parseNodes\n",
	pfc->moduleHash->elCount, pfc->tkz->tokenCount, pfParseCount(program));

    if (endPhase < 9)
	return;
    verbose(2, "Phase 9 - compiling C code\n");
    /* Now run gcc on it. */
	{
	struct dyString *dy = dyStringNew(0);
	int err;
	for (module = program->children; module != NULL; module = module->next)
	    {
	    if (module->name[0] != '<' && module->type != pptModuleRef)
		{
		struct pfModule *mod = hashMustFindVal(pfc->moduleHash, module->name);
		char *cName = replaceSuffix(mod->fileName, ".pf", ".c");
		char *oName = replaceSuffix(mod->fileName, ".pf", ".o");
		dyStringClear(dy);
		dyStringAppend(dy, "gcc ");
		dyStringAppend(dy, "-O ");
		dyStringPrintf(dy, "-I %s ", pfc->cIncludeDir);
		dyStringAppend(dy, "-c ");
		dyStringAppend(dy, "-o ");
		dyStringPrintf(dy, "%s ", oName);
		dyStringPrintf(dy, "%s ", cName);
		verbose(2, "%s\n", dy->string);
		err = system(dy->string);
		if (err != 0)
		    errAbort("Couldn't compile %s.c", module->name);
		freeMem(oName);
		freeMem(cName);
		}
	    }
	dyStringClear(dy);
	dyStringAppend(dy, "gcc ");
	dyStringAppend(dy, "-O ");
	dyStringPrintf(dy, "-I %s ", pfc->cIncludeDir);
	dyStringPrintf(dy, "-o %s%s ", baseDir, baseName);
	dyStringPrintf(dy, "%s ", cFile);
	for (module = program->children; module != NULL; module = module->next)
	    {
	    if (module->name[0] != '<')
		{
		struct pfModule *mod = hashMustFindVal(pfc->moduleHash, module->name);
		char *suffix = (module->type == pptModuleRef ? ".pfh" : ".pf");
		char *oName = replaceSuffix(mod->fileName, suffix, ".o");
		dyStringPrintf(dy, "%s ", oName);
		freeMem(oName);
		}
	    }
	dyStringPrintf(dy, " %s ", pfc->runtimeLib);
	dyStringPrintf(dy, "%s ", pfc->jkwebLib);
	dyStringAppend(dy, "-lpthread -lm");
	verbose(2, "%s\n", dy->string);
	err = system(dy->string);
	if (err != 0)
	    errnoAbort("problem compiling:\n", dy->string);
	dyStringFree(&dy);
	}

    }
if (endPhase < 10)
    return;

verbose(2, "Phase 10 - execution\n");
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

