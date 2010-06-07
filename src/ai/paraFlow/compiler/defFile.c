/* defFile - create a file containing the things defined
 * in the file we are compiling. */

#include "common.h"
// #include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "defFile.h"

static void findSpanningTokens(struct pfParse *pp, struct pfToken **pStart, struct pfToken **pEnd)
/* Recursively find tokens that span parse tree. */
{
struct pfToken *tok = pp->tok;
if (*pStart == NULL)
    *pStart = *pEnd = tok;
else
    {
    char *text = tok->text;
    if (text < (*pStart)->text)
         *pStart = tok;
    if (text > (*pEnd)->text)
         *pEnd = tok;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    findSpanningTokens(pp, pStart, pEnd);
    }
}

static void printTokenRange(FILE *f, struct pfToken *start, struct pfToken *end)
/* Print tokens between start and end. */
{
struct pfToken *tok = start;
for (;;)
    {
    mustWrite(f, tok->text, tok->textSize);
    if (tok == end)
        break;
    fputc(' ', f);
    tok = tok->next;
    }
}

static void rPrintIncludes(FILE *f, struct pfParse *pp)
/* Print into statements. */
{
if (pp->type == pptInclude)
    fprintf(f, "include '%s';\n", pp->children->name);
else if (pp->type == pptImport)
    {
    struct pfParse *modPp = pp->children;
    struct pfParse *asPp = modPp->next;
    fprintf(f, "import '%s' as %s;\n", modPp->name, asPp->name);
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rPrintIncludes(f, pp);
}

static void rPrintDefs(FILE *f, struct pfParse *parent, boolean printInit,
	boolean printLocals);
/* Print definitions. */

static void printVarDef(FILE *f, struct pfParse *pp, boolean printInit,
	boolean printLocals)
/* Print variable statement and optionally initialization. */
{
if (printLocals || pp->access == paWritable || pp->access == paGlobal)
    {
    struct pfToken *start = NULL, *end = NULL;
    struct pfParse *type = pp->children;
    struct pfParse *name = type->next;
    struct pfParse *init = name->next;

    start = end = type->tok;
    if (pp->access != paUsual)
	fprintf(f, "%s ", pfAccessTypeAsString(pp->access));
    if (pp->isConst)
	{
        fprintf(f, "const ");
	printInit = TRUE;
	}
    findSpanningTokens(type, &start, &end);
    if (end->next->type == ')')
	end = end->next;	// Hack for function pointers... */
    printTokenRange(f, start, end);
    fprintf(f, " ");
    start = end = name->tok;
    findSpanningTokens(name, &start, &end);
    if (printInit && init != NULL)
	findSpanningTokens(init, &start, &end);
    printTokenRange(f, start, end);
    fprintf(f, ";\n");
    }
}

static struct pfToken *addClosingParens(struct pfToken *start, 
	struct pfToken *end)
/* If the output type tuple is parenthesized we end up missing
 * the closing end paren.  This fixes that. */
{
int parenBalance = 0;
struct pfToken *tok = start;
for (;;)
    {
    if (tok->type == '(')
        ++parenBalance;
    else if (tok->type == ')')
        --parenBalance;
    if (tok == end)
        break;
    tok = tok->next;
    }
while (parenBalance > 0)
    {
    tok = tok->next;
    if (tok == NULL || tok->type != ')')
        internalErr();
    --parenBalance;
    }
return tok;
}

static void printFuncDef(FILE *f, struct pfParse *funcDef)
/* Print function definition - just name and parameters */
{
struct pfToken *start, *end;
struct pfParse *name = funcDef->children;
struct pfParse *input = name->next;
struct pfParse *output = input->next;
struct pfParse *body = output->next;
start = end = funcDef->tok;
if (funcDef->access == paGlobal)
    fprintf(f, "global ");
findSpanningTokens(name, &start, &end);
findSpanningTokens(input, &start, &end);
findSpanningTokens(output, &start, &end);
end = addClosingParens(start, end);
printTokenRange(f, start, end);
fprintf(f, ";\n");
}

static void printClassDef(FILE *f, struct pfParse *class)
/* Print class definition - everything but method bodies. */
{
struct pfToken *start, *end;
struct pfParse *name = class->children;
struct pfParse *body = name->next;
struct pfParse *extends = body->next;
struct pfBaseType *base = pfScopeFindType(class->scope, class->name);

start = end = class->tok;
if (base->access == paGlobal)
    fprintf(f, "global ");
findSpanningTokens(name, &start, &end);
if (extends != NULL)
    findSpanningTokens(extends, &start, &end);
printTokenRange(f, start, end);
fprintf(f, "\n{\n");
rPrintDefs(f, body, TRUE, TRUE);
fprintf(f, "}\n");
}

static void rPrintDefs(FILE *f, struct pfParse *parent, 
	boolean printInit, boolean printLocals)
/* Print definitions . */
{
struct pfParse *pp;
for (pp = parent->children; pp != NULL; pp = pp->next)
    {
    switch (pp->type)
        {
	case pptVarInit:
	    printVarDef(f, pp, printInit, printLocals);
	    break;
	case pptToDec:
	case pptFlowDec:
	    printFuncDef(f, pp);
	    break;
	case pptPolymorphic:
	    fprintf(f, "polymorphic ");
	    rPrintDefs(f, pp, printInit, printLocals);
	    break;
	case pptClass:
	case pptInterface:
	    printClassDef(f, pp);
	    break;
	case pptTuple:
	    rPrintDefs(f, pp, printInit, printLocals);
	    break;
	}
    }
}

static void appendType(struct dyString *dy, struct pfParse *p);

static void appendTypeTuple(struct dyString *dy, struct pfParse *p)
/* Append something that looks like (int int int) to dy */
{
dyStringAppendC(dy, '(');
for (p = p->children; p != NULL; p = p->next)
    {
    struct pfParse *type = p->children;
    if (p->type != pptVarInit && p->type != pptFormalParameter)
         internalErr();
    appendType(dy, type);
    if (p->next != NULL)
	dyStringAppendC(dy, ',');
    }
dyStringAppendC(dy, ')');
}

static void appendVarOf(struct dyString *dy, struct pfParse *pp)
/* Append something looking like var of to (int int) into () 
 * to dy. */
{
switch (pp->type)
    {
    case pptTypeToPt:
         dyStringAppend(dy, "var of to ");
	 break;
    case pptTypeFlowPt:
         dyStringAppend(dy, "var of flow ");
	 break;
    default:
         internalErr();
	 break;
    }
appendTypeTuple(dy, pp->children);
dyStringAppend(dy, " into ");
appendTypeTuple(dy, pp->children->next);
}

static void appendType(struct dyString *dy, struct pfParse *p)
/* Append type to dy. */
{
if (p->type == pptTypeName)
    dyStringAppend(dy, p->name);
else if (p->type == pptOf)
    {
    p = p->children;
    dyStringAppend(dy, p->name);
    while ((p = p->next) != NULL)
	{
	dyStringAppend(dy, " of ");
	appendType(dy, p);
	}
    }
else if (p->type == pptTypeToPt || p->type == pptTypeFlowPt)
    {
    appendVarOf(dy, p);
    }
else if (p->type == pptModuleDotType)
    {
    struct pfParse *left = p->children;
    struct pfParse *right = left->next;
    dyStringAppend(dy, left->name);
    dyStringAppendC(dy, '.');
    dyStringAppend(dy, right->name);
    }
else
    {
    internalErrAt(p->tok);
    }
}

void rPrintTypesUsed(FILE *f, struct pfParse *pp, 
	struct dyString *dy, struct hash *hash)
/* Make up a dummy variable for each type that is used. */
{
if (pp->type == pptVarInit)
    {
    struct pfParse *type = pp->children;
    dyStringClear(dy);
    appendType(dy, type);
    if (!hashLookup(hash, dy->string))
	{
	hashAdd(hash, dy->string, NULL);
	fprintf(f, "%s t%d;\n", dy->string, hash->elCount);
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rPrintTypesUsed(f, pp, dy, hash);
}

void printTypesUsed(FILE *f, struct pfParse *module)
/* Print a variable for each type used. */
{
struct dyString *dy = dyStringNew(0);
struct hash *hash = hashNew(0);

rPrintTypesUsed(f, module, dy, hash);

hashFree(&hash);
dyStringFree(&dy);
}

void pfMakeDefFile(struct pfCompile *pfc, struct pfParse *module, 
	char *defFile)
/* Write out definition file. */
{
FILE *f = mustOpen(defFile, "w");
fprintf(f, "// ParaFlow definition and type file  for %s module\n", module->name);
fprintf(f, "   // Modules referenced\n");
rPrintIncludes(f, module);
fprintf(f, "   // Types used\n");
fprintf(f, "{\n");
printTypesUsed(f, module);
fprintf(f, "}\n");
fprintf(f, "   // Symbols defined\n");
rPrintDefs(f, module, FALSE, FALSE);
carefulClose(&f);
}

