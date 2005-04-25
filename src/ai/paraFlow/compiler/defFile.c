/* defFile - create a file containing the things defined
 * in this file. */

#include "common.h"
//#include "linefile.h"
//#include "hash.h"
//#include "dystring.h"
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

static void rPrintIntos(FILE *f, struct pfParse *pp)
/* Print into statements. */
{
if (pp->type == pptInto)
    fprintf(f, "into %s;\n", pp->children->name);
for (pp = pp->children; pp != NULL; pp = pp->next)
    rPrintIntos(f, pp);
}

static void rPrintDefs(FILE *f, struct pfParse *parent, boolean printInit);
/* Print definitions. */

static void printVarDef(FILE *f, struct pfParse *pp, boolean printInit)
/* Print variable statement and optionally initialization. */
{
struct pfToken *start = NULL, *end = NULL;
struct pfParse *type = pp->children;
struct pfParse *name = type->next;
struct pfParse *init = name->next;

findSpanningTokens(type, &start, &end);
findSpanningTokens(name, &start, &end);
if (printInit && init != NULL)
    findSpanningTokens(init, &start, &end);
printTokenRange(f, start, end);
fprintf(f, ";\n");
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
findSpanningTokens(name, &start, &end);
findSpanningTokens(input, &start, &end);
findSpanningTokens(output, &start, &end);
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

start = end = class->tok;
findSpanningTokens(name, &start, &end);
if (extends != NULL)
    findSpanningTokens(extends, &start, &end);
printTokenRange(f, start, end);
fprintf(f, "\n{\n");
uglyf("Printing class body\n");
rPrintDefs(f, body, TRUE);
fprintf(f, "}\n");
}

static void rPrintDefs(FILE *f, struct pfParse *parent, boolean printInit)
/* Print definitions . */
{
struct pfParse *pp;
uglyf("rPrintDefs %s\n", pfParseTypeAsString(parent->type));
for (pp = parent->children; pp != NULL; pp = pp->next)
    {
    uglyf(" printing %s\n", pfParseTypeAsString(pp->type));
    switch (pp->type)
        {
	case pptVarInit:
	    printVarDef(f, pp, printInit);
	    break;
	case pptToDec:
	case pptParaDec:
	case pptFlowDec:
	    printFuncDef(f, pp);
	    break;
	case pptClass:
	    printClassDef(f, pp);
	    break;
	}
    }
}

void pfMakeDefFile(struct pfCompile *pfc, struct pfParse *module, 
	char *defFile)
/* Write out definition file. */
{
FILE *f = mustOpen(defFile, "w");
fprintf(f, "// Paraflow def file for %s module\n", module->name);
rPrintIntos(f, module);
rPrintDefs(f, module, FALSE);
}

