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
    pptConstUse,
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
    case pptConstUse:
	return "pptConstUse";
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

struct pfVar
/* A variable at a certain scope; */
     {
     char *name;			/* Name (not allocated here) */
     struct pfScope *scope;		/* Scope we're declared in. */
     struct pfCollectedType *type;	/* Variable type. */
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

static void pfScopeAddType(struct pfScope *scope, char *name, boolean isCollection,
	struct pfBaseType *parentType)
/* Add type to scope. */
{
struct pfBaseType *type;
AllocVar(type);
type->scope = scope;
type->parentType = parentType;
type->isCollection = isCollection;
hashAddSaveName(scope->types, name, type, &type->name);
}

static struct pfVar *pfScopeAddVar(struct pfScope *scope, char *name, struct pfCollectedType *ct)
/* Add type to scope. */
{
struct pfVar *var;
AllocVar(var);
var->scope = scope;
var->type = ct;
hashAddSaveName(scope->vars, name, var, &var->name);
return var;
}

struct pfBaseType *pfScopeFindType(struct pfScope *scope, char *name)
/* Find type associated with name in scope and it's parents. */
{
while (scope != NULL)
    {
    struct pfBaseType *baseType = hashFindVal(scope->types, name);
    if (baseType != NULL)
        return baseType;
    scope = scope->parent;
    }
return NULL;
}

struct pfVar *pfScopeFindVar(struct pfScope *scope, char *name)
/* Find variable associated with name in scope and it's parents. */
{
while (scope != NULL)
    {
    struct pfVar *var = hashFindVal(scope->vars, name);
    if (var != NULL)
        return var;
    scope = scope->parent;
    }
return NULL;
}

struct pfVar *pfScopeFindOrCreateVar(struct pfScope *scope, char *name)
/* Find variable.  If it doesn't exist create it in innermost scope. */
{
struct pfVar *var = pfScopeFindVar(scope, name);
if (var == NULL)
    var = pfScopeAddVar(scope, name, NULL);
return var;
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

static void pfDumpConst(struct pfToken *tok, FILE *f)
/* Dump out constant to file */
{
switch (tok->type)
    {
    case pftString:
	{
	#define MAXLEN 32
	char buf[MAXLEN+4];
	char *s = tok->val.s;
	int len = strlen(s);
	strcpy(buf+MAXLEN, "...");
	buf[MAXLEN-1] = 0;
	if (len > MAXLEN) len = MAXLEN;
	memcpy(buf, s, len);
	fprintf(f, "\"%s\"", buf);
	break;
	#undef MAXLEN
	}
    case pftInt:
        fprintf(f, "%lld", tok->val.i);
	break;
    case pftFloat:
        fprintf(f, "%g", tok->val.x);
	break;
    default:
        fprintf(f, "unknownType");
	break;
    }
}


void pfParseDump(struct pfParse *pp, int level, FILE *f)
/* Write out pp (and it's children) to file */
{
struct pfParse *child;
spaceOut(f, level*3);
fprintf(f, "%s", pfParseTypeAsString(pp->type));
if (pp->var != NULL)
    {
    struct pfVar *var = pp->var;
    struct pfCollectedType *ct = var->type;
    if (ct == NULL)
        {
	fprintf(f, " noType");
	}
    else
	{
	while (ct != NULL)
	    {
	    if (ct->base != NULL)
		fprintf(f, " %s", ct->base->name);
	    if (ct->next != NULL)
		fprintf(f, " of");
	    ct = ct->next;
	    }
	}
    fprintf(f, " %s", var->name);
    }
switch (pp->type)
    {
    case pptConstUse:
	fprintf(f, " ");
	pfDumpConst(pp->tok, f);
	break;
    }
fprintf(f, "\n");
for (child = pp->children; child != NULL; child = child->next)
    pfParseDump(child, level+1, f);
}


void pfParseStatement(struct pfParse *parent, 
	struct pfToken **pTokList, struct pfScope *scope);

struct pfParse *pfParseExpression(struct pfParse *parent, 
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
pp->var = pfScopeFindOrCreateVar(scope, tok->val.s);
*pTokList = tok->next;
return pp;
}

struct pfParse *constUse(struct pfParse *parent, struct pfToken **pTokList, struct pfScope *scope)
/* Create constant use */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptConstUse, tok, parent);
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

static struct pfParse *varDeclareOne(struct pfParse *parent,
	struct pfCollectedType *type, struct pfToken **pTokList, struct pfScope *scope)
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptVarDec, tok, parent);
if (tok->type != pftName)
    errAbort("Expecting variable name line %d of %s", tok->startLine, tok->source->name);
pp->var = pfScopeAddVar(scope, tok->val.s, type);
slAddHead(&parent->children, pp);
*pTokList = tok->next;
return pp;
}

static struct pfCollectedType *
parseType(struct pfBaseType *baseType, struct pfToken **pTokList, struct pfScope *scope)
/* Deal with tree of list of array of baseType */
{
struct pfToken *tok = *pTokList;
struct pfCollectedType *ctList = NULL, *ct;

for (;;)
    {
    AllocVar(ct);
    slAddHead(&ctList, ct);
    ct->base = baseType;
    tok = tok->next;
    if (tok == NULL)
	break;
    if (tok->type == pftName && sameString(tok->val.s, "of"))
	{
	if (!baseType->isCollection)
	    errAbort("%s is not a collection line %d of %s",
		baseType->name, tok->startLine, tok->source->name);
	tok = tok->next;
	}
    else
        break;
    if (tok->type != pftName || ((baseType = pfScopeFindType(scope, tok->val.s)) == NULL))
        errAbort("Expecting type, got %s line %d of %s",
		tok->val.s, tok->startLine, tok->source->name);
    }
*pTokList = tok;
slReverse(&ctList);
return ctList;
}

static void parseAssign(struct pfParse *parent, char *varName,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse assignment statement. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptAssignment, tok, parent);
pp->var = pfScopeFindOrCreateVar(scope, varName);
tok = tok->next;	/* Skip over '=' */
pp->children = pfParseExpression(pp, &tok, scope);
slAddHead(&parent->children, pp);
*pTokList = tok;
}


static void parseVarDeclaration(struct pfParse *parent, 
	struct pfBaseType *baseType, struct pfToken **pTokList, struct pfScope *scope)
/* Parse variable declaration - something of form:
 *    typeName varName,varName, ... varName */
{
struct pfCollectedType *ct = parseType(baseType, pTokList, scope);
struct pfToken *tok = *pTokList;
for (;;)
    {
    char *varName = tok->val.s;
    struct pfParse *pp = varDeclareOne(parent, ct, &tok, scope);
    if (tok->type == pftEnd)
        break;
    if (tok->type == '=')
	{
	tok = tok->next;
	pp->children = pfParseExpression(pp, &tok, scope);
	}
    if (tok->type != ',')
        break;
    tok = tok->next;
    }
*pTokList = tok;
}

struct pfParse *pfParseAtom(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse atomic expression - const, variable, or parenthesized expression. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = NULL;
switch (tok->type)
    {
    case pftName:
	pp = varUse(parent, &tok, scope);
	break;
    case pftString:
    case pftInt:
    case pftFloat:
        pp = constUse(parent, &tok, scope);
	break;
    case '(':
        {
	struct pfToken *startTok = tok;
	tok = tok->next;
	pp = pfParseExpression(parent, &tok, scope);
	if (tok->type != ')')
	    errAbort("Unclosed parenthesis starting line %d of %s",
	    	tok->startLine, tok->source->name);
	tok = tok->next;
	break;
	}
    default:
        syntaxError(tok);
	break;
    }
*pTokList = tok;
return pp;
}

struct pfParse *pfParseNegation(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse unary minus. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
if (tok->type == '-')
    {
    pp = pfParseNew(pptNegate, tok, parent);
    tok = tok->next;
    pp->children = pfParseAtom(pp, &tok, scope);
    }
else
    {
    pp = pfParseAtom(parent, &tok, scope);
    }
*pTokList = tok;
return pp;
}

struct pfParse *pfParseProduct(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse expression. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNegation(parent, &tok, scope);
while (tok->type == '*' || tok->type == '/' || tok->type == '%')
    {
    struct pfParse *left = pp, *right;
    enum pfTokType tt = pptTimes;
    switch (tok->type)
        {
	case '*':
	   tt = pptTimes;
	   break;
	case '/':
	   tt = pptDiv;
	   break;
	case '%':
	   tt = pptMod;
	   break;
	}
    pp = pfParseNew(tt, tok, parent);
    left->parent = pp;
    tok = tok->next;
    right = pfParseNegation(pp, &tok, scope);
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}

struct pfParse *pfParseExpression(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse expression. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseProduct(parent, &tok, scope);
while (tok->type == '+' || tok->type == '-')
    {
    struct pfParse *left = pp, *right;
    enum pfTokType tt = (tok->type == '+' ? pptPlus : pptMinus);
    pp = pfParseNew(tt, tok, parent);
    left->parent = pp;
    tok = tok->next;
    right = pfParseProduct(pp, &tok, scope);
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}

static void parseAssignOrCall(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse assignment or call statements. */
{
struct pfToken *tok = *pTokList, *opTok;
char *varName = tok->val.s;
int op;
tok = tok->next;
op = tok->type;
if (op == '=')
    parseAssign(parent, varName, &tok, scope);
#ifdef SOON
else if (op == '(')
    parseCall(parent, varName, &tok, scope);
#endif /* SOON */
else
    syntaxError(tok);
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
    struct pfBaseType *baseType = pfScopeFindType(scope, s);
    if (sameString(s, "foreach"))
        parseForeach(parent, pTokList, scope);
    else if (sameString(s, "class"))
        parseClass(parent, pTokList, scope);
    else if (baseType != NULL)
        parseVarDeclaration(parent, baseType, pTokList, scope);
    else
        parseAssignOrCall(parent, pTokList, scope);
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
#endif /* SOON */
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

while (tok->type != pftEnd)
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
int endCount = 3;

scope = pfScopeNew(scope, 12);
while ((tok = pfTokenizerNext(pfTkz)) != NULL)
    {
    if (tok->type != pftWhitespace && tok->type != pftComment)
	{
	slAddHead(&tokList, tok);
	}
    }

/* Add enough ends to satisfy all look-aheads */
while (--endCount >= 0)
    {
    AllocVar(tok);
    tok->type = pftEnd;
    slAddHead(&tokList, tok);
    }

slReverse(&tokList);

program = pfParseProgram(tokList, scope);
pfParseDump(program, 0, stdout);
}

static void addBuiltInTypes(struct pfScope *scope)
/* Add built in types . */
{
static char *basic[] = { "var" , "string" , "bit" , "byte" , "short" , "int" , "long", "float"};
static char *collections[] = { "array", "list", "tree", "dir" };
int i;
for (i=0; i<ArraySize(basic); ++i)
    pfScopeAddType(scope, basic[i], FALSE, NULL);
for (i=0; i<ArraySize(collections); ++i)
    pfScopeAddType(scope, collections[i], TRUE, NULL);
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

