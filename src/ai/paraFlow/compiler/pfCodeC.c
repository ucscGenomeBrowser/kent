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

static int printFunctionPrototype(struct pfCompile *pfc, FILE *f,
	struct pfVar *funcVar)
/* Print out function prototype.  Returns number of outputs of function. */
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
return outCount;
}

static void printExpression(struct pfCompile *pfc, FILE *f, 
	struct pfParse *exp);
/* Generate code for a expression */

static boolean printCallStart(struct pfCompile *pfc, 
	struct pfParse *call, FILE *f)
/* Print the func(in1, in2 ...  part of a function call.
 * Don't print the output or the last , or the closing ) */
{
struct pfParse *inTuple = call->children->next;
struct pfParse *in;
boolean needComma = FALSE;
fprintf(f, "%s(", call->name);
for (in = inTuple->children; in != NULL; in = in->next)
    {
    if (needComma)
	fprintf(f, ", ");
    needComma = TRUE;
    printExpression(pfc, f, in);
    }
return needComma;
}

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
    fprintf(f, "{");
    for (out = outTuple->children; out != NULL; out = out->next)
        {
	needComma = TRUE;
	printType(pfc, f, out->base);
	fprintf(f, " %s; ", out->fieldName);
	}
    }

needComma = printCallStart(pfc, call, f);
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
	fprintf(f, "&%s", out->fieldName);
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

static void printAssignment(struct pfCompile *pfc, FILE *f,
	struct pfParse *left, struct pfParse *right);
/* Handle assignment where left side is not a tuple. */

static void printLvalAddress(struct pfCompile *pfc, FILE *f,
	struct pfParse *lval)
/* Print &lval */
{
switch (lval->type)
    {
    case pptVarUse:
    case pptVarInit:
	{
	struct pfVar *var = lval->var;
	if (var->isSingleOut || !var->isOut)
	    fprintf(f, "&");
        fprintf(f, "%s", lval->name);
	break;
	}
    default:
        internalErrAt(lval->tok);
	break;
    }
}

static void printCallIntoTuple(struct pfCompile *pfc, FILE *f,
	struct pfParse *tuple, struct pfParse *call)
/* Deal with things like (x, y) = unitCircle(10) */
{
struct pfParse *out;
boolean needComma;
needComma = printCallStart(pfc, call, f);
for (out = tuple->children; out != NULL; out = out->next)
    {
    if (needComma)
	fprintf(f, ", ");
    needComma = TRUE;
    printLvalAddress(pfc, f, out);
    }
fprintf(f, ");\n");
}

static void printCastCallToTuple(struct pfCompile *pfc, FILE *f, 
	struct pfParse *tuple, struct pfParse *castCall)
/* Deal with things like (int x, float y) = unitCircle(10),
 * that is a call into a tuple with the tuple not being the
 * same type as the call. */
{
struct pfParse *call = castCall->children;
struct pfParse *castList = call->next;
struct pfParse *cast;
struct pfParse *out;
int outIx;
boolean needComma;

fprintf(f, "{");
outIx = 1;
for (cast = castList; cast != NULL; cast = cast->next)
    {
    if (cast->type != pptPlaceholder)
        {
	printType(pfc, f, cast->ty->base);
	fprintf(f, " out%d; ", outIx);
	}
    ++outIx;
    }
needComma = printCallStart(pfc, call, f);
outIx = 1;
for (out=tuple->children, cast = castList; cast != NULL; 
     out = out->next, cast = cast->next)
    {
    if (needComma)
	fprintf(f, ", ");
    needComma = TRUE;
    if (cast->type == pptPlaceholder)
        {
	printLvalAddress(pfc, f, out);
	}
    else
        {
	fprintf(f, "&out%d", outIx);
	}
    ++outIx;
    }
fprintf(f, ");");
outIx = 1;
for (out=tuple->children, cast = castList; cast != NULL; 
     out = out->next, cast = cast->next)
     {
     if (cast->type != pptPlaceholder)
         {
	 switch(cast->type)
	     {
	     case pptCastByteToBit:
	     case pptCastShortToBit:
	     case pptCastIntToBit:
	     case pptCastLongToBit:
	     case pptCastFloatToBit:
	     case pptCastDoubleToBit:
	     case pptCastStringToBit:
		 if (out->var->isOut && !out->var->isSingleOut)
		     fprintf(f, "*");
	         fprintf(f, " %s = (out%d != 0);", out->name, outIx);
		 break;
	     default:
		 if (out->var->isOut && !out->var->isSingleOut)
		     fprintf(f, "*");
	         fprintf(f, " %s = out%d;", out->name, outIx);
		 break;
	     }
	 }
    ++outIx;
     }
fprintf(f, "}\n");
}


static void printTupleAssignment(struct pfCompile *pfc, FILE *f,
	struct pfParse *left, struct pfParse *right)
/* Handle assignment where left side is a tuple. */
{
switch (right->type)
    {
    case pptTuple:
        {
	for (left = left->children, right = right->children;
	     left != NULL; left = left->next, right = right->next)
	    {
	    printAssignment(pfc, f, left, right);
	    }
	break;
	}
    case pptCall:
        {
	printCallIntoTuple(pfc, f, left, right);
	break;
	}
    case pptCastCallToTuple:
        {
	printCastCallToTuple(pfc, f, left, right);
	break;
	}
    default:
        internalErrAt(left->tok);
	break;
    }
}

static void printAssignment(struct pfCompile *pfc, FILE *f,
	struct pfParse *left, struct pfParse *right)
/* Handle assignment where left side is not a tuple. */
{
fprintf(f, "/* %s = %s; */\n", left->name, right->name);
}


static void printAssignStatement(struct pfCompile *pfc, FILE *f, 
	struct pfParse *statement)
/* Print assignment statement. */
{
struct pfParse *left = statement->children;
struct pfParse *right = left->next;

if (left->ty->isTuple)
    {
    printTupleAssignment(pfc, f, left, right);
    }
else
    {
    printAssignment(pfc, f, left, right);
    }
}

static void printScope(int level,
	struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	boolean isModule, boolean functionOk);

static void printStatement(int level, struct pfCompile *pfc, FILE *f, 
	struct pfParse *statement)
/* Generate code for a statement. */
{

/* Do indentation if we are going to actually do anything. */
switch (statement->type)
    {
    case pptClass:
    case pptNop:
        return; /* All done already */
    case pptVarInit:
	{
	struct pfParse *init = statement->children->next->next;
	if (init == NULL)
	    return;
	break;
	}
    }
// fprintf(f, "/* ugly %s */", pfParseTypeAsString(statement->type));
spaceOut(f, level);

/* Process statement proper. */
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
	int outCount = printFunctionPrototype(pfc, f, funcVar);
	struct pfType *out = NULL;
	fprintf(f, "\n");
	if (outCount == 1)
	     {
	     out = funcVar->ty->children->next->children;
	     fprintf(f, "{\n");
	     printType(pfc, f, out->base);
	     fprintf(f, " %s = 0;\n", out->fieldName);
	     }
	printStatement(level+1, pfc, f, body);
	if (outCount == 1)
	     {
	     fprintf(f, "return %s;\n", out->fieldName);
	     fprintf(f, "}\n");
	     }
	fprintf(f, "\n");
	break;
	}
    case pptCall:
    	{
	printCallStatement(pfc, f, statement);
	fprintf(f, "\n");
	break;
	}
    case pptAssignment:
    	{
	printAssignStatement(pfc, f, statement);
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

