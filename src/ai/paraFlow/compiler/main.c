/* Command line main driver module for paraFlow compiler. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "hash.h"
#include "dystring.h"
#include "localmem.h"
#include "paraToken.h"

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

struct pfParse
/* The para parse tree. */
    {
    struct pfParse *next;	/* Next in list */
    enum pfParseType type;	/* Node type */
    struct pfToken *tok;	/* Token associated with node. */
    struct pfParse *parent;	/* Parent statement if any. */
    struct pfParse *children;	/* subparts. */
    struct hash *symbols;	/* Symbol table (may be NULL) */
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


struct pfParse *pfParseStatement(struct pfParse *parent, 
	struct pfToken **pTokList);

static void syntaxError(struct pfToken *tok)
/* Complain about syntax error and quit. */
{
errAbort("Syntax error line %d of %s", tok->source->name, tok->startLine);
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

static void compoundToChildren(struct pfParse *pp, struct pfToken **pTokList)
/* Parse {, a list of statements, and } and hang statements on
 * pp->children. */
{
struct pfToken *tok = *pTokList;
if (tok->type != '{')
    expectingGot("{", tok);
tok = tok->next;
for (;;)
    {
    struct pfParse *statement;
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
    statement = pfParseStatement(pp, &tok);
    slAddHead(&pp->children, statement);
    }
}

struct pfParse *parseCompound(struct pfParse *parent,
	struct pfToken **pTokList)
/* Parse a compound statement (statement list surrounded
 * by brackets */
{
struct pfParse *pp = pfParseNew(pptCompound, *pTokList, parent);
compoundToChildren(pp, pTokList);
return pp;
}


struct pfParse *varUse(struct pfParse *parent, struct pfToken **pTokList)
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

struct pfParse *parseForeach(struct pfParse *parent,
	struct pfToken **pTokList)
/* Parse foreach statement */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptForeach, tok, parent);
struct pfParse *element;
struct pfParse *collection;
struct pfParse *statement;

tok = tok->next;	/* Skip over 'foreach' */
element = varUse(pp, &tok);
skipRequiredName("in", &tok);
collection = varUse(pp, &tok);
/* Make sure next token matches name, and then skip it. */
statement = pfParseStatement(pp, &tok);
slAddHead(&pp->children, statement);
slAddHead(&pp->children, collection);
slAddHead(&pp->children, element);
*pTokList = tok;
return pp;
}

struct pfParse *parseClass(struct pfParse *parent,
	struct pfToken **pTokList)
/* Parse class declaration statement. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
skipRequiredName("class", &tok);
pp = pfParseNew(pptClass, tok, parent);
tok = tok->next;
compoundToChildren(pp, &tok);
*pTokList = tok;
return pp;
}

struct pfParse *pfParseStatement(struct pfParse *parent, 
	struct pfToken **pTokList)
/* Parse a single statement. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = NULL;

if (tok->type == '{')
    pp = parseCompound(parent, pTokList);
else if (tok->type == pftName)
    {
    char *s = tok->val.s;
    if (sameString(s, "foreach"))
        pp = parseForeach(parent, pTokList);
    else if (sameString(s, "class"))
        pp = parseClass(parent, pTokList);
#ifdef SOON
    else if (sameString(s, "to"))
        pp = parseTo(parent, pTokList);
    else if (sameString(s, "for"))
        pp = parseFor(parent, pTokList);
    else if (sameString(s, "while"))
        pp = parseWhile(parent, pTokList);
    else if (sameString(s, "flow"))
        pp = parseFlow(parent, pTokList);
    else if (sameString(s, "para"))
        pp = parsePara(parent, pTokList);
    else
        pp = parseAssignOrCall(parent, pTokList);
#endif /* SOON */
    else
	syntaxError(tok);
    }
else
    {
    syntaxError(tok);
    }
return pp;
}

struct pfParse *pfParseProgram(struct pfToken *tokList)
/* Convert token list to parsed program. */
{
struct pfToken *tok;
struct pfParse *program = pfParseNew(pptProgram, tokList, NULL);

while (tokList != NULL)
    {
    struct pfParse *statement = pfParseStatement(pptProgram, &tokList);
    slAddHead(&program->children, statement);
    }
slReverse(&program->children);
return program;
}

void paraFlowOnFile(char *fileName)
/* Execute paraFlow on single file. */
{
struct pfTokenizer *pfTkz = pfTokenizerNew(fileName);
struct pfToken *tokList = NULL, *tok;
struct pfParse *program;

while ((tok = pfTokenizerNext(pfTkz)) != NULL)
    {
    if (tok->type != pftWhitespace && tok->type != pftComment)
	{
	slAddHead(&tokList, tok);
	}
    }
slReverse(&tokList);
program = pfParseProgram(tokList);
pfParseDump(program, 0, stdout);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
paraFlowOnFile(argv[1]);
#ifdef SOON
paraFlowOnFile("../testProg/easy/stringInit.pf");
#endif /* SOON */
return 0;
}

