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
	struct pfTokenizer *tkz, struct pfParse *pp)
/* Go through program walking into modules. */
{
if (pp->type == pptInto)
    {
    struct dyString *dy = dyStringNew(0);
    struct hashEl *hel;
    char *module = NULL;
    collectDots(dy, pp->children);
    dyStringAppend(dy, ".pf");

    hel = hashLookup(tkz->modules, dy->string);
    if (hel != NULL)
        module = hel->name;

    if (module == NULL)
        {
	struct pfSource *oldSource = tkz->source;
	char *oldPos = tkz->pos;
	char *oldEndPos = tkz->endPos;
	struct pfParse *mod;
	tkz->source = pfSourceNew(dy->string);
	tkz->pos = tkz->source->contents;
	tkz->endPos = tkz->pos + tkz->source->contentSize;
	mod = pfParseFile(dy->string, tkz, program);
	expandInto(program, tkz, mod);
	tkz->source = oldSource;
	tkz->pos = oldPos;
	tkz->endPos = oldEndPos;
	slAddHead(&program->children, mod);
	module = hashStoreName(tkz->modules, dy->string);
	}
    pp->name = module;
    dyStringFree(&dy);
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    expandInto(program, tkz, pp);
}

void initVars(struct pfParse *pp)
/* Convert pptVarDec to pptVarInit. */
{
struct pfParse *p;

for (p = pp->children; p != NULL; p = p->next)
    initVars(p);
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
    case pptNameUse:
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
        break;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    addDeclaredVarsToScopes(pp);
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

void paraFlow(char *fileName)
/* parse and dump. */
{
struct hash *reservedWords = createReservedWords();
struct pfTokenizer *tkz = pfTokenizerNew(fileName, reservedWords);
struct pfParse *program = pfParseNew(pptProgram, NULL, NULL, tkz->scope);
struct pfParse *pp = pfParseFile(fileName, tkz, program);


expandInto(program, tkz, pp);
slAddHead(&program->children, pp);
slReverse(&program->children);

initVars(program);
evalTypes(program);
addDeclaredVarsToScopes(program);
// bindVars(program);

pfParseDump(program, 0, stdout);

printf("%d modules, %d tokens, %d parseNodes\n",
	tkz->modules->elCount, tkz->tokenCount, pfParseCount(program));
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

