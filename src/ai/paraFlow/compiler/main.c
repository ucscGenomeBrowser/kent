/* Command line main driver module for paraFlow compiler. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "hash.h"
#include "dystring.h"
#include "localmem.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfParse.h"
#include "pfCompile.h"

void usage()
/* Explain command line and exit. */
{
errAbort(
"This program compiles paraFlow code.  ParaFlow is a parallel programming\n"
"language designed by Jim Kent.\n"
"Usage:\n"
"    paraFlow input.pf\n");
}

static struct optionSpec options[] = {
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

void collectDots(struct dyString *dy, struct pfParse *pp)
/* Make sure that pp is really a pptDots or a pptName. 
 * Output name plus any necessary dots into dy. */
{
if (pp->type == pptNameUse)
    {
    dyStringAppend(dy, pp->name);
    return;
    }
else if (pp->type == pptDot)
    {
    for (pp = pp->children; pp != NULL; pp = pp->next)
        {
	switch (pp->type)
	    {
	    case pptNameUse:
		dyStringAppend(dy, pp->name);
		if (pp->next != NULL)
		    dyStringAppend(dy, ".");
		break;
	    case pptRoot:
	        dyStringAppend(dy, "/");
		break;
	    case pptParent:
	        dyStringAppend(dy, "../");
		break;
	    case pptSys:
	    case pptUser:
	    case pptSysOrUser:
	        break;
	    }
	}
    }
else
    expectingGot("name", pp->tok);
}

void expandInto(struct pfParse *program, 
	struct pfCompile *pfc, struct pfParse *pp)
/* Go through program walking into modules. */
{
if (pp->type == pptInto)
    {
    struct dyString *dy = dyStringNew(0);
    struct hashEl *hel;
    char *module = NULL;
    collectDots(dy, pp->children);
    dyStringAppend(dy, ".pf");

    hel = hashLookup(pfc->modules, dy->string);
    if (hel != NULL)
        module = hel->name;

    if (module == NULL)
        {
	struct pfTokenizer *tkz = pfc->tkz;
	struct pfSource *oldSource = tkz->source;
	char *oldPos = tkz->pos;
	char *oldEndPos = tkz->endPos;
	struct pfParse *mod;
	tkz->source = pfSourceNew(dy->string);
	tkz->pos = tkz->source->contents;
	tkz->endPos = tkz->pos + tkz->source->contentSize;
	mod = pfParseFile(dy->string, pfc, program);
	expandInto(program, pfc, mod);
	tkz->source = oldSource;
	tkz->pos = oldPos;
	tkz->endPos = oldEndPos;
	slAddHead(&program->children, mod);
	module = hashStoreName(pfc->modules, dy->string);
	}
    pp->name = module;
    dyStringFree(&dy);
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    expandInto(program, pfc, pp);
}

static void substituteType(struct pfParse *pp, enum pfParseType oldType,
	enum pfParseType newType)
/* Convert type of pp and any children that are of oldType to newType */
{
if (pp->type == oldType)
    pp->type = newType;
for (pp = pp->children; pp != NULL; pp = pp->next)
    substituteType(pp, oldType, newType);
}

void varDecAndAssignToInit(struct pfParse *pp)
/* Convert pptVarDec to pptVarInit. */
{
struct pfParse *p;

for (p = pp->children; p != NULL; p = p->next)
    varDecAndAssignToInit(p);
if (pp->type == pptVarDec)
    {
    pp->type = pptVarInit;
    pp->name = pp->children->next->name;
    }
else if (pp->type == pptAssignment)
    {
    struct pfParse *left = pp->children;
    if (left->type == pptVarDec)
        {
	struct pfParse *right = left->next;
	pp->type = pptVarInit;
	pp->children = left->children;
	pp->name = left->name;
	slAddTail(&pp->children, right);
	}
    }
if (pp->type == pptVarInit)
    {
    struct pfParse *type = pp->children;
    struct pfParse *name = type->next;
    substituteType(type, pptNameUse, pptTypeName);
    name->type = pptSymName;
    }
}

void evalTypes(struct pfParse *pp)
/* Go through and fill in pp->ct field on type
 * expressions. */
{
struct pfParse *p;
for (p = pp->children; p != NULL; p = p->next)
    evalTypes(p);
switch (pp->type)
    {
    case pptVarDec:
    	internalErr();
	break;
    case pptVarInit:
	{
	struct pfParse *type = pp->children;
	pp->ct = type->ct;
	break;
	}
    case pptOf:
        {
	struct pfParse *type = pp->children;
	pp->ct = type->ct;
	while (type != NULL)
	     {
	     struct pfParse *nextType = type->next;
	     if (nextType != NULL)
	          type->ct->next = nextType->ct;
	     type = nextType;
	     }
	break;
	}
    case pptTypeName:
        {
	struct pfBaseType *bt = pfScopeFindType(pp->scope, pp->name);
	if (bt != NULL)
	    {
	    struct pfCollectedType *ct;
	    AllocVar(ct);
	    ct->base = bt;
	    pp->ct = ct;
	    }
	break;
	}
    }
}

void addDeclaredVarsToScopes(struct pfParse *pp)
/* Go through and put declared variables into symbol table
 * for scope. */
{
switch (pp->type)
    {
    case pptVarDec:
    	internalErr();
	break;
    case pptVarInit:
	{
	struct pfParse *type = pp->children;
	struct pfParse *name = type->next;
	struct pfParse *initVal = name->next;
	if (hashLookup(pp->scope->vars, name->name))
	    errAt(pp->tok, "%s redefined", name->name);
	pfScopeAddVar(pp->scope, name->name, type->ct);
	break;
	}
    case pptToDec:
    case pptParaDec:
    case pptFlowDec:
	{
	struct pfParse *name = pp->children;
	struct pfParse *input = name->next;
	struct pfParse *output = input->next;
	struct pfParse *body = output->next;
	name->type = pptSymName;
	assert(input->type == pptTuple);
	assert(output->type == pptTuple);
	substituteType(input, pptTuple, pptTypeTuple);
	substituteType(output, pptTuple, pptTypeTuple);
#ifdef SOON
#endif /* SOON */
	if (hashLookup(pp->scope->parent->vars, name->name))
	    errAt(pp->tok, "%s redefined", name->name);
	pfScopeAddVar(pp->scope->parent, name->name, NULL);
        break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    addDeclaredVarsToScopes(pp);
}

static void bindVars(struct pfParse *pp)
/* Attach variable name to pfVar in appropriate scope.
 * Complain and die if not found. */
{
switch (pp->type)
    {
    case pptNameUse:
	{
	struct pfVar *var = pfScopeFindVar(pp->scope, pp->name);
	if (var == NULL)
	    errAt(pp->tok, "Use of undefined variable %s", pp->name);
	pp->var = var;
	pp->type = pptVarUse;
	pp->ct = var->ct;
        break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    bindVars(pp);
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

static void dumpScope(FILE *f, struct pfScope *scope)
/* Write out line of info about scope. */
{
if (scope->types->elCount > 0)
    {
    struct hashCookie hc = hashFirst(scope->types);
    struct pfBaseType *bt;
    fprintf(f, " types: ");
    while ((bt = hashNextVal(&hc)) != NULL)
        fprintf(f, "%s, ", bt->name);
    }
if (scope->vars->elCount > 0)
    {
    struct hashCookie hc = hashFirst(scope->vars);
    struct pfVar *var;
    fprintf(f, " vars: ");
    while ((var = hashNextVal(&hc)) != NULL)
	{
	pfCollectedTypeDump(var->ct, f);
        fprintf(f, " %s,", var->name);
	}
    }
}

static void printScopeInfo(int level, struct pfParse *pp)
/* Print out info on each new scope. */
{
switch (pp->type)
    {
    case pptProgram:
    case pptModule:
    case pptFor:
    case pptToDec:
    case pptFlowDec:
    case pptParaDec:
    case pptCompound:
	{
	char *name = pp->name;
	if (name == NULL)
	    name = "";
        spaceOut(stdout, 2*level);
	printf("scope %s %s: ", 
		pfParseTypeAsString(pp->type), name);
	dumpScope(stdout, pp->scope);
	printf("\n");
	break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    printScopeInfo(level+1, pp);
}

static void addBuiltInTypes(struct pfCompile *pfc)
/* Add built in types . */
{
struct pfScope *scope = pfc->scope;

/* Declare some basic types.  Types with names in parenthesis
 * are never declared by user directly */
pfc->varType = pfScopeAddType(scope, "var", FALSE, NULL);
pfc->streamType = pfScopeAddType(scope, "(stream)", FALSE, pfc->varType);
pfc->numType = pfScopeAddType(scope, "(number)", FALSE, pfc->varType);
pfc->collectionType = pfScopeAddType(scope, "(collection)", TRUE, pfc->varType);
pfc->tupleType = pfScopeAddType(scope, "(tuple)", TRUE, pfc->collectionType);
pfc->classType = pfScopeAddType(scope, "(class)", TRUE, pfc->collectionType);
pfc->functionType = pfScopeAddType(scope, "(function)", TRUE, pfc->varType);
pfc->toType = pfScopeAddType(scope, "(to)", TRUE, pfc->functionType);
pfc->paraType = pfScopeAddType(scope, "(para)", TRUE, pfc->functionType);
pfc->flowType = pfScopeAddType(scope, "(flow)", TRUE, pfc->functionType);

pfScopeAddType(scope, "string", FALSE, pfc->streamType);

pfScopeAddType(scope, "bit", FALSE, pfc->numType);
pfScopeAddType(scope, "byte", FALSE, pfc->numType);
pfScopeAddType(scope, "short", FALSE, pfc->numType);
pfScopeAddType(scope, "int", FALSE, pfc->numType);
pfScopeAddType(scope, "long", FALSE, pfc->numType);
pfScopeAddType(scope, "float", FALSE, pfc->numType);
pfScopeAddType(scope, "double", FALSE, pfc->numType);

pfScopeAddType(scope, "array", TRUE, pfc->collectionType);
pfScopeAddType(scope, "list", TRUE, pfc->collectionType);
pfScopeAddType(scope, "tree", TRUE, pfc->collectionType);
pfScopeAddType(scope, "dir", TRUE, pfc->collectionType);
}

struct pfCompile *pfCompileNew(char *fileName)
/* Make new compiler object. */
{
struct pfCompile *pfc;
AllocVar(pfc);
pfc->baseFile = cloneString(fileName);
pfc->modules = hashNew(0);
pfc->reservedWords = createReservedWords();
pfc->scope = pfScopeNew(NULL, 8);
addBuiltInTypes(pfc);
pfc->tkz = pfTokenizerNew(fileName, pfc->reservedWords);
return pfc;
}

void paraFlow(char *fileName)
/* parse and dump. */
{
struct pfCompile *pfc = pfCompileNew(fileName);
struct pfParse *program = pfParseNew(pptProgram, NULL, NULL, pfc->scope);
struct pfParse *pp = pfParseFile(fileName, pfc, program);

expandInto(program, pfc, pp);
slAddHead(&program->children, pp);
slReverse(&program->children);

varDecAndAssignToInit(program);
evalTypes(program);
addDeclaredVarsToScopes(program);
bindVars(program);

pfParseDump(program, 0, stdout);

printf("%d modules, %d tokens, %d parseNodes\n",
	pfc->modules->elCount, pfc->tkz->tokenCount, pfParseCount(program));
printScopeInfo(0, program);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
paraFlow(argv[1]);
return 0;
}

