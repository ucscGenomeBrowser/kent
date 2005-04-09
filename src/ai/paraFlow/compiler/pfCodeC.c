/* pfCodeC - C code generator for paraFlow. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"


static void printPreamble(struct pfCompile *pfc, FILE *f)
/* Print out C code for preamble. */
{
fprintf(f, "/* This file is a translation of %s by paraFlow. */\n", 
	pfc->baseFile);
fprintf(f, "\n");
fprintf(f, "#include \"pfPreamble.h\"\n");
fprintf(f, "\n");
}

static void printType(struct pfCompile *pfc, FILE *f, struct pfBaseType *base)
/* Print out type info for C. */
{
if (base == pfc->bitType)
    fprintf(f, "Char");
else if (base == pfc->byteType)
    fprintf(f, "Byte");
else if (base == pfc->shortType)
    fprintf(f, "Short");
else if (base == pfc->intType)
    fprintf(f, "Int");
else if (base == pfc->longType)
    fprintf(f, "Long");
else if (base == pfc->floatType)
    fprintf(f, "Float");
else if (base == pfc->doubleType)
    fprintf(f, "Double");
else if (base == pfc->stringType)
    fprintf(f, "String");
else if (base == pfc->varType)
    fprintf(f, "Var");
else if (base == pfc->arrayType)
    fprintf(f, "Array");
else if (base == pfc->listType)
    fprintf(f, "List");
else if (base == pfc->dirType)
    fprintf(f, "Dir");
else if (base == pfc->treeType)
    fprintf(f, "Tree");
else
    fprintf(f, "struct %s*", base->name);
}

static void rPrintFields(int level, struct pfCompile *pfc, 
	FILE *f, struct pfBaseType *base)
/* Print out fields - parents first */
{
struct pfType *type;
if (base->parent != NULL)
    rPrintFields(level, pfc, f, base->parent);
for (type = base->fields; type != NULL; type = type->next)
    {
    spaceOut(f, level);
    printType(pfc, f, type->base);
    fprintf(f, " %s;\n", type->fieldName);
    }
}

static void printFunctionPrototype(struct pfCompile *pfc, FILE *f,
	struct pfVar *funcVar)
{
char *name = funcVar->name;
struct pfType *type = funcVar->ty;
struct pfType *input = type->children, *in;
struct pfType *output = input->next, *out;
int outCount = slCount(output->children);
boolean needComma = FALSE;
if (outCount == 1)
    {
    printType(pfc, f, output->children->base);
    fprintf(f, " ");
    }
else
    fprintf(f, "void ");
fprintf(f, "%s(", name);
for (in = input->children; in != NULL; in = in->next)
    {
    if (needComma)
        fprintf(f, ", ");
    needComma = TRUE;
    printType(pfc, f, in->base);
    fprintf(f, " %s", in->fieldName);
    }
if (outCount > 1)
    {
    for (out = output->children; out != NULL; out = out->next)
        {
	if (needComma)
	    fprintf(f, ", ");
	needComma = TRUE;
	printType(pfc, f, out->base);
	fprintf(f, "*");
	fprintf(f, " %s", out->fieldName);
	}
    }
fprintf(f, ")");
}

static void printExpression(struct pfCompile *pfc, FILE *f, 
	struct pfParse *exp);
/* Generate code for a expression */

static void printCallStatement(struct pfCompile *pfc, 
	FILE *f, struct pfParse *call)
/* Generate code for a function call that throws away it's return 
 * values. */
{
struct pfParse *funcVarPp = call->children;
struct pfParse *inTuple = funcVarPp->next;
struct pfVar *funcVar = funcVarPp->var;
struct pfType *funcType = funcVar->ty;
struct pfType *outTuple = funcType->children->next;
struct pfParse *in;
int outCount = slCount(outTuple->children);
boolean needComma;

if (outCount > 1)
    {
    struct pfType *out;
    needComma = FALSE;
    fprintf(f, "{");
    for (out = outTuple->children; out != NULL; out = out->next)
        {
	if (needComma)
	    fprintf(f, ", ");
	needComma = TRUE;
	printType(pfc, f, out->base);
	fprintf(f, " %s", out->fieldName);
	}
    fprintf(f, "; ");
    }

needComma = FALSE;
fprintf(f, "%s(", funcVar->name);
for (in = inTuple->children; in != NULL; in = in->next)
    {
    if (needComma)
	fprintf(f, ", ");
    needComma = TRUE;
    printExpression(pfc, f, in);
    }
if (outCount > 1)
    {
    struct pfType *out;
    if (needComma)
	{
	fprintf(f, ", ");
	needComma = FALSE;
	}
    for (out = outTuple->children; out != NULL; out = out->next)
        {
	if (needComma)
	    fprintf(f, ", ");
	needComma = TRUE;
	printType(pfc, f, out->base);
	fprintf(f, " &%s", out->fieldName);
	}
    fprintf(f, "); ");
    fprintf(f, "}");
    }
else
    {
    fprintf(f, "); ");
    }

}

static void printExpression(struct pfCompile *pfc, FILE *f, struct pfParse *exp)
/* Generate code for a expression */
{
fprintf(f, "expression...");
}

static void printScope(int level,
	struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	boolean isModule, boolean functionOk);

static void printStatement(int level, struct pfCompile *pfc, FILE *f, 
	struct pfParse *statement)
/* Generate code for a statement. */
{
spaceOut(f, level);
switch (statement->type)
    {
    case pptCompound:
	fprintf(f, " {\n");
	printScope(level+1, pfc, f, statement, FALSE, FALSE);
	spaceOut(f, level);
	fprintf(f, " }\n");
	break;
    case pptTuple:
        {
	struct pfParse *p = statement->children;
	if (p->type == pptVarInit)
	    {
	    int l = 0;
	    for (; p != NULL; p = p->next)
		{
		printStatement(l, pfc, f, p);
		l = level;
		}
	    }
	break;
	}
    case pptVarInit:
        {
	struct pfParse *init = statement->children->next->next;
	if (init != NULL)
	    {
	    fprintf(f, "%s = ", statement->name);
	    printExpression(pfc, f, init);
	    fprintf(f, ";\n");
	    }
	break;
	}
    case pptToDec:
    case pptParaDec:
    case pptFlowDec:
        {
	struct pfVar *funcVar = pfScopeFindVar(statement->parent->scope,
		statement->name);
	struct pfParse *body = statement->children->next->next->next;
	printFunctionPrototype(pfc, f, funcVar);
	printStatement(level+1, pfc, f, body);
	fprintf(f, "\n");
	break;
	}
    case pptCall:
    	{
	printCallStatement(pfc, f, statement);
	fprintf(f, "\n");
	break;
	}
    case pptClass:
    case pptNop:
	/* All done already */
    	break;
    default:
        {
	printExpression(pfc, f, statement);
	fprintf(f, ";\n");
	}
    }
}

static void printScope(int level,
	struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	boolean isModule, boolean functionOk)
/* Print types and then variables from scope. */
{
struct pfScope *scope = pp->scope;
struct hashEl *hel, *helList;
struct pfParse *p;
boolean gotFunc = FALSE, gotVar = FALSE;

/* Print out classes declared in this scope. */
helList = hashElListHash(scope->types);
slSort(&helList, hashElCmp);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    struct pfBaseType *base = hel->val;
    fprintf(f, "struct %s {\n", base->name);
    rPrintFields(level+1, pfc, f, base); 
    fprintf(f, "};\n\n");
    }
hashElFreeList(&helList);

/* Get declaration list and sort it. */
helList = hashElListHash(scope->vars);
slSort(&helList, hashElCmp);

/* Print out functions */
for (hel = helList; hel != NULL; hel = hel->next)
    {
    struct pfVar *var = hel->val;
    struct pfType *type = var->ty;
    if (var->ty->isFunction)
        {
	spaceOut(f, level);
	printFunctionPrototype(pfc, f, var);
	fprintf(f, ";\n");
	gotFunc = TRUE;
	}
    }
if (gotFunc)
    fprintf(f, "\n");

/* Print out variables. */
for (hel = helList; hel != NULL; hel = hel->next)
    {
    char *ugly = 0;
    struct pfVar *var = hel->val;
    struct pfType *type = var->ty;
    if (!var->ty->isFunction)
        {
	spaceOut(f, level);
	printType(pfc, f, type->base);
	fprintf(f, " %s = 0;\n", var->name);
	gotVar = TRUE;
	}
    }
if (gotVar)
    fprintf(f, "\n");

hashElFreeList(&helList);

for (p = pp->children; p != NULL; p = p->next)
    {
    printStatement(level, pfc, f, p);
    }
}

void pfCodeC(struct pfCompile *pfc, struct pfParse *program, char *fileName)
/* Generate C code for program. */
{
FILE *f = mustOpen(fileName, "w");
struct pfParse *module;

printPreamble(pfc, f);
for (module = program->children; module != NULL; module = module->next)
    {
    fprintf(f, "/* ParaFlow module %s */\n\n", module->name);
    printScope(0, pfc, f, module, TRUE, TRUE);
    }
}

