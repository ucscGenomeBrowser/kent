/* Command line main driver module for paraFlow compiler. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "hash.h"
#include "dystring.h"
#include "localmem.h"
#include "paraToken.h"
#include "pfType.h"

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

enum pfParseType
/* Parse type */
    {
    pptProgram,
    pptCompound,
    pptIf,
    pptWhile,
    pptFor,
    pptForeach,
    pptClass,
    pptVarDec,
    pptVarUse,
    pptToDec,
    pptFlowDec,
    pptParaDec,
    pptCall,
    pptBreak,
    pptAssignment,
    pptPlusEquals,
    pptMinusEquals,
    pptTimesEquals,
    pptDivEquals,
    pptModEquals,
    pptIndex,
    pptPlus,
    pptMinus,
    pptTimes,
    pptDiv,
    pptMod,
    pptAnd,
    pptOr,
    pptComma,
    pptSame,
    pptNotSame,
    pptNegate,
    pptConstant,
    };


static char *pfParseTypeAsString(enum pfParseType type)
/* Return string corresponding to pfParseType */
{
switch (type)
    {
    case pptProgram:
    	return "pptProgram";
    case pptCompound:
    	return "pptCompound";
    case pptIf:
    	return "pptIf";
    case pptWhile:
    	return "pptWhile";
    case pptFor:
	return "pptFor";
    case pptForeach:
	return "pptForeach";
    case pptClass:
	return "pptClass";
    case pptVarDec:
	return "pptVarDec";
    case pptVarUse:
	return "pptVarUse";
    case pptToDec:
	return "pptToDec";
    case pptFlowDec:
	return "pptFlowDec";
    case pptParaDec:
	return "pptParaDec";
    case pptCall:
	return "pptCall";
    case pptBreak:
	return "pptBreak";
    case pptAssignment:
	return "pptAssignment";
    case pptPlusEquals:
	return "pptPlusEquals";
    case pptMinusEquals:
	return "pptMinusEquals";
    case pptTimesEquals:
	return "pptTimesEquals";
    case pptDivEquals:
	return "pptDivEquals";
    case pptModEquals:
	return "pptModEquals";
    case pptIndex:
	return "pptIndex";
    case pptPlus:
	return "pptPlus";
    case pptMinus:
	return "pptMinus";
    case pptTimes:
	return "pptTimes";
    case pptDiv:
	return "pptDiv";
    case pptMod:
	return "pptMod";
    case pptAnd:
	return "pptAnd";
    case pptOr:
	return "pptOr";
    case pptComma:
	return "pptComma";
    case pptSame:
	return "pptSame";
    case pptNotSame:
	return "pptNotSame";
    case pptNegate:
	return "pptNegate";
    case pptConstant:
	return "pptConstant";
    default:
        internalErr();
	return NULL;
    }
}

struct paraSymbol
/* A symbol table entry */
    {
    char *name;	/* Allocated in hash */
    bool defined;	/* True if defined in this scope. */
    bool written;	/* True if written to. */
    struct paraType *type;	/* Symbol type */
    };

struct pfScope
/* The symbol tables for this scope and a pointer to the
 * parent scope. */
     {
     struct pfScope *parent;	/* Parent scope if any. */
     struct hash *types;	/* Types defined in this scope. */
     struct hash *vars;		/* Variables defined in this scope (including functions) */
     };

struct pfScope *pfScopeNew(struct pfScope *parent, int size)
/* Create new scope with given parent.  Size is just a hint
 * of what to make the size of the symbol table as a power of
 * two.  Pass in 0 if you don't care. */
{
struct pfScope *scope;
int varSize = size;
int typeSize;

if (varSize <= 0)
    varSize = 5;
typeSize = varSize - 2;
if (typeSize <= 0)
    typeSize = 3;
AllocVar(scope);
scope->types = hashNew(typeSize);
scope->vars = hashNew(varSize);
scope->parent = parent;
return scope;
}

static void pfScopeAddType(struct pfScope *scope, char *name, struct pfType *type)
/* Add type to scope. */
{
hashAdd(scope->types, name, type);
}

static void pfScopeAddVar(struct pfScope *scope, char *name, struct pfType *type)
/* Add type to scope. */
{
hashAdd(scope->vars, name, type);
}

struct hashEl *pfScopeFindType(struct pfScope *scope, char *name)
/* Find type associated with name in scope and it's parents. */
{
while (scope != NULL)
    {
    struct hashEl *hel = hashLookup(scope->types, name);
    if (hel != NULL)
        return hel;
    scope = scope->parent;
    }
return NULL;
}

struct pfParse
/* The para parse tree. */
    {
    struct pfParse *next;	/* Next in list */
    enum pfParseType type;	/* Node type */
    struct pfToken *tok;	/* Token associated with node. */
    struct pfParse *parent;	/* Parent statement if any. */
    struct pfParse *children;	/* subparts. */
    struct pfVar *var;		/* Associated variable if any. */
    };

void pfParseDump(struct pfParse *pp, int level, FILE *f)
/* Write out pp (and it's children) to file */
{
struct pfParse *child;
spaceOut(f, level*3);
fprintf(f, "%s\n", pfParseTypeAsString(pp->type));
for (child = pp->children; child != NULL; child = child->next)
    pfParseDump(child, level+1, f);
}


void pfParseStatement(struct pfParse *parent, 
	struct pfToken **pTokList, struct pfScope *scope);

static void syntaxError(struct pfToken *tok)
/* Complain about syntax error and quit. */
{
errAbort("Syntax error line %d of %s", tok->startLine, tok->source->name);
}

struct pfParse *pfParseNew(enum pfParseType type,
	struct pfToken *tok, struct pfParse *parent)
/* Return new parse node.  It's up to caller to fill in
 * children later, and to make symbol table if needed. */
{
struct pfParse *pp;
AllocVar(pp);
pp->type = type;
pp->tok = tok;
pp->parent = parent;
return pp;
}


static void expectingGot(char *expecting, struct pfToken *got)
/* Complain about unexpected stuff and quit. */
{
errAbort("Expecting %s line %d of %s", expecting, got->startLine,
	got->source->name);
}

static void skipRequiredName(char *name, struct pfToken **pTokList)
/* Make sure next token matches name, and then skip it. */
{
struct pfToken *tok = *pTokList;
if (tok->type != pftName || !sameString(tok->val.s, name))
    expectingGot(name, tok);
*pTokList = tok->next;
}

static void compoundToChildren(struct pfParse *pp, struct pfToken **pTokList,
	struct pfScope *scope)
/* Parse {, a list of statements, and } and hang statements on
 * pp->children. */
{
struct pfToken *tok = *pTokList;
if (tok->type != '{')
    expectingGot("{", tok);
tok = tok->next;
for (;;)
    {
    if (tok == NULL)
	{
	tok = *pTokList;
        errAbort("End of file in compound statement.  Opening { line %d of %s",
	     tok->startLine, tok->source->name);
	}
    if (tok->type == '}')
        {
	*pTokList = tok->next;
	slReverse(&pp->children);
	return;
	}
    pfParseStatement(pp, &tok, scope);
    }
}

static void parseCompound(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse a compound statement (statement list surrounded
 * by brackets */
{
struct pfParse *pp = pfParseNew(pptCompound, *pTokList, parent);
slAddHead(&parent->children, pp);
compoundToChildren(pp, pTokList, scope);
}


struct pfParse *varUse(struct pfParse *parent, struct pfToken **pTokList, struct pfScope *scope)
/* Make sure have a name, and create a varUse type node
 * based on it. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptVarUse, tok, parent);
if (tok->type != pftName)
    errAbort("Expecting variable name line %d of %s",
    	tok->startLine, tok->source->name);
*pTokList = tok->next;
return pp;
}

static void parseForeach(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse foreach statement */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptForeach, tok, parent);
struct pfParse *element;
struct pfParse *collection;
struct pfParse *statement;

tok = tok->next;	/* Skip over 'foreach' */
element = varUse(pp, &tok, scope);
skipRequiredName("in", &tok);
collection = varUse(pp, &tok, scope);
/* Make sure next token matches name, and then skip it. */
pfParseStatement(pp, &tok, scope);
slAddHead(&pp->children, collection);
slAddHead(&pp->children, element);
*pTokList = tok;
slAddHead(&parent->children, pp);
}

static void parseClass(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse class declaration statement. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
skipRequiredName("class", &tok);
pp = pfParseNew(pptClass, tok, parent);
tok = tok->next;
compoundToChildren(pp, &tok, scope);
*pTokList = tok;
slAddHead(&parent->children, pp);
}

static void varDeclareOne(struct pfParse *parent,
	struct pfType *type, struct pfToken **pTokList, struct pfScope *scope)
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptVarDec, tok, parent);
if (tok->type != pftName)
    errAbort("Expecting variable name line %d of %s", tok->startLine, tok->source->name);
pfScopeAddVar(scope, tok->val.s, type);
slAddHead(&parent->children, pp);
*pTokList = tok->next;
}

static void parseVarDeclaration(struct pfParse *parent, char *typeName,
	struct pfType *type, struct pfToken **pTokList, struct pfScope *scope)
/* Parse variable declaration - something of form:
 *    typeName varName,varName, ... varName */
{
struct pfToken *tok = *pTokList;
tok = tok->next;	/* Skip over type. */
for (;;)
    {
    varDeclareOne(parent, type, &tok, scope);
    if (tok == NULL)
        break;
    if (tok->type != ',')
        break;
    tok = tok->next;
    }
*pTokList = tok;
}

void pfParseStatement(struct pfParse *parent, 
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse a single statement, and add results to (head) of
 * parent->childList. */
{
struct pfToken *tok = *pTokList;

if (tok->type == '{')
    parseCompound(parent, pTokList, scope);
else if (tok->type == ';')
    *pTokList = tok->next;
else if (tok->type == pftName)
    {
    char *s = tok->val.s;
    struct hashEl *typeHel = pfScopeFindType(scope, s);
    if (typeHel != NULL)
	{
	struct pfType *type = typeHel->val;
        parseVarDeclaration(parent, typeHel->name, type, pTokList, scope);
	}
    else if (sameString(s, "foreach"))
        parseForeach(parent, pTokList, scope);
    else if (sameString(s, "class"))
        parseClass(parent, pTokList, scope);
#ifdef SOON
    else if (sameString(s, "to"))
        parseTo(parent, pTokList, scope);
    else if (sameString(s, "for"))
        parseFor(parent, pTokList, scope);
    else if (sameString(s, "while"))
        parseWhile(parent, pTokList);
    else if (sameString(s, "flow", scope))
        parseFlow(parent, pTokList);
    else if (sameString(s, "para"))
        parsePara(parent, pTokList, scope);
    else
        parseAssignOrCall(parent, pTokList, scope);
#endif /* SOON */
    else
	{
	syntaxError(tok);
	}
    }
else
    {
    syntaxError(tok);
    }
}

struct pfParse *pfParseProgram(struct pfToken *tokList, struct pfScope *scope)
/* Convert token list to parsed program. */
{
struct pfToken *tok = tokList;
struct pfParse *program = pfParseNew(pptProgram, tokList, NULL);

while (tok != NULL)
    {
    pfParseStatement(program, &tok, scope);
    }
slReverse(&program->children);
return program;
}

void paraFlowOnFile(char *fileName, struct pfScope *scope)
/* Execute paraFlow on single file. */
{
struct pfTokenizer *pfTkz = pfTokenizerNew(fileName);
struct pfToken *tokList = NULL, *tok;
struct pfParse *program;

scope = pfScopeNew(scope, 12);
while ((tok = pfTokenizerNext(pfTkz)) != NULL)
    {
    if (tok->type != pftWhitespace && tok->type != pftComment)
	{
	slAddHead(&tokList, tok);
	}
    }
slReverse(&tokList);
program = pfParseProgram(tokList, scope);
pfParseDump(program, 0, stdout);
}

static void addBuiltInTypes(struct pfScope *scope)
/* Add built in types . */
{
static char *t[] = { "var" , "string" , "bit" , "byte" , "short" , "int" , "long", "float"};
int i;
for (i=0; i<ArraySize(t); ++i)
    pfScopeAddType(scope, t[i], NULL);

}

int main(int argc, char *argv[])
/* Process command line. */
{
struct pfScope *scope;
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
scope = pfScopeNew(NULL, 14);
addBuiltInTypes(scope);
paraFlowOnFile(argv[1], scope);
#ifdef SOON
paraFlowOnFile("../testProg/easy/stringInit.pf");
#endif /* SOON */
return 0;
}

