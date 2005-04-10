/* ccoder - produce C code from type-checked parse tree. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"

static void codeStatement(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp);
/* Emit code for one statement. */

static int codeExpression(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack);
/* Emit code for one expression.  Returns how many items added
 * to stack. */

static void codeScope(
	struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	boolean isModule, boolean isClass);
/* Print types and then variables from scope. */

static void printPreamble(struct pfCompile *pfc, FILE *f)
/* Print out C code for preamble. */
{
fprintf(f, "/* This file is a translation of %s by paraFlow. */\n", 
	pfc->baseFile);
fprintf(f, "\n");
fprintf(f, "#include \"pfPreamble.h\"\n");
fprintf(f, "\n");
}

static char *typeKey(struct pfCompile *pfc, struct pfBaseType *base)
/* Return key for type if available, or NULL */
{
if (base == pfc->bitType)
    return "Char";
else if (base == pfc->byteType)
    return "Byte";
else if (base == pfc->shortType)
    return "Short";
else if (base == pfc->intType)
    return "Int";
else if (base == pfc->longType)
    return "Long";
else if (base == pfc->floatType)
    return "Float";
else if (base == pfc->doubleType)
    return "Double";
else if (base == pfc->stringType)
    return "String";
else if (base == pfc->varType)
    return "Var";
else if (base == pfc->arrayType)
    return "Array";
else if (base == pfc->listType)
    return "List";
else if (base == pfc->dirType)
    return "Dir";
else if (base == pfc->treeType)
    return "Tree";
else 
    return NULL;
}

static void printType(struct pfCompile *pfc, FILE *f, struct pfBaseType *base)
/* Print out type info for C. */
{
char *s = typeKey(pfc, base);
if (s == NULL)
    fprintf(f, "struct %s*", base->name);
else
    fprintf(f, "%s", s);
}

static char *stackName = "_pfStack_";

static void printParamAccess(struct pfCompile *pfc, FILE *f, 
	struct pfBaseType *base, int offset)
/* Print out code to access paramater of given type at offset. */
{
char *s = typeKey(pfc, base);
if (s == NULL)
    s = "Val";
fprintf(f, "%s[%d].%s", stackName, offset, s);
}

static void rPrintFields(struct pfCompile *pfc, 
	FILE *f, struct pfBaseType *base)
/* Print out fields - parents first */
{
struct pfType *type;
if (base->parent != NULL)
    rPrintFields(pfc, f, base->parent);
for (type = base->fields; type != NULL; type = type->next)
    {
    printType(pfc, f, type->base);
    fprintf(f, " %s;\n", type->fieldName);
    }
}

static boolean isInitialized(struct pfVar *var)
/* Return TRUE if variable is initialized on declaration in  parse tree. */
{
struct pfParse *pp = var->parse;
if (pp->type != pptVarInit)
    errAbort("Expecting pptVarInit got %s for var %s",
    	pfParseTypeAsString(pp->type), var->name);
return pp->children->next->next != NULL;
}

static int codeCall(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for a function call. */
{
struct pfParse *varUse = pp->children;
struct pfVar *var = varUse->var;
struct pfParse *inTuple = varUse->next;
struct pfParse *in;
struct pfType *outTuple = var->ty->children->next;
int outCount = slCount(outTuple->children);;

/* Add parameters to stack. */
codeExpression(pfc, f, inTuple, stack);
if (stack != 0)
    fprintf(f, "%s += %d;\n", stackName, stack);
fprintf(f, "%s();\n", pp->name);
if (stack != 0)
    fprintf(f, "%s -= %d;\n", stackName, stack);
return outCount;
}

static int lvalOffStack(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Take an lval off of stack. */
{
switch (pp->type)
    {
    case pptVarUse:
    case pptVarInit:
	fprintf(f, "%s = ", pp->name);
	printParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, ";\n");
	return 1;
    case pptTuple:
        {
	int total = 0;
	struct pfParse *p;
	for (p = pp->children; p != NULL; p = p->next)
	    total += lvalOffStack(pfc, f, p, stack+total);
	return total;
	}
    default:
        fprintf(f, "using %s %s as an lval\n", pp->name, pfParseTypeAsString(pp->type));
	return 0;
    }
}

static void codeAssignment(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for an assignment. */
{
struct pfParse *lval = pp->children;
struct pfParse *rval = lval->next;
codeExpression(pfc, f, rval, stack);
lvalOffStack(pfc, f, lval, stack);
}

static void codeVarInit(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for an assignment. */
{
struct pfParse *lval = pp;
struct pfParse *rval = pp->children->next->next;
codeExpression(pfc, f, rval, stack);
lvalOffStack(pfc, f, lval, stack);
}
	
	
static int codeBinaryOp(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op)
/* Emit code for some sort of binary op. */
{
struct pfParse *lval = pp->children;
struct pfParse *rval = lval->next;
codeExpression(pfc, f, lval, stack);
codeExpression(pfc, f, rval, stack+1);
printParamAccess(pfc, f, pp->ty->base, stack);
fprintf(f, " = ");
printParamAccess(pfc, f, pp->ty->base, stack);
fprintf(f, " %s ", op);
printParamAccess(pfc, f, pp->ty->base, stack+1);
fprintf(f, ";\n");
return 1;
}

static int codeExpression(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Emit code for one expression.  Returns how many items added
 * to stack. */
{
switch (pp->type)
    {
    case pptTuple:
        {
	int total = 0;
	struct pfParse *p;
	for (p = pp->children; p != NULL; p = p->next)
	    total += codeExpression(pfc, f, p, stack+total);
	return total;
	}
    case pptCall:
	return codeCall(pfc, f, pp, stack);
    case pptAssignment:
	codeAssignment(pfc, f, pp, stack);
	return 0;
    case pptVarInit:
    	codeVarInit(pfc, f, pp, stack);
	return 0;
    case pptVarUse:
	printParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = %s;\n", pp->name);
	return 1;
    case pptPlus:
        return codeBinaryOp(pfc, f, pp, stack, "+");
    case pptMinus:
        return codeBinaryOp(pfc, f, pp, stack, "-");
    case pptMul:
        return codeBinaryOp(pfc, f, pp, stack, "*");
    case pptDiv:
        return codeBinaryOp(pfc, f, pp, stack, "/");
    case pptMod:
        return codeBinaryOp(pfc, f, pp, stack, "%");
    case pptConstBit:
        {
	printParamAccess(pfc, f, pfc->bitType, stack);
	fprintf(f, " = %lld;\n", pp->tok->val.i);
	return 1;
	}
    case pptConstByte:
        {
	printParamAccess(pfc, f, pfc->byteType, stack);
	fprintf(f, " = %lld;\n", pp->tok->val.i);
	return 1;
	}
    case pptConstShort:
        {
	printParamAccess(pfc, f, pfc->shortType, stack);
	fprintf(f, " = %lld;\n", pp->tok->val.i);
	return 1;
	}
    case pptConstInt:
        {
	printParamAccess(pfc, f, pfc->intType, stack);
	fprintf(f, " = %lld;\n", pp->tok->val.i);
	return 1;
	}
    case pptConstLong:
        {
	printParamAccess(pfc, f, pfc->longType, stack);
	fprintf(f, " = %lld;\n", pp->tok->val.i);
	return 1;
	}
    case pptConstFloat:
        {
	printParamAccess(pfc, f, pfc->floatType, stack);
	fprintf(f, " = %f;\n", pp->tok->val.x);
	return 1;
	}
    case pptConstDouble:
        {
	printParamAccess(pfc, f, pfc->doubleType, stack);
	fprintf(f, " = %f;\n", pp->tok->val.x);
	return 1;
	}
    default:
	{
	fprintf(f, "(%s expression)\n", pfParseTypeAsString(pp->type));
	return 0;
	}
    }
}

static void codeStatement(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp)
/* Emit C code for one statement. */
{
switch (pp->type)
    {
    case pptCompound:
        {
	fprintf(f, "{\n");
	codeScope(pfc, f, pp, FALSE, FALSE);
	fprintf(f, "}\n");
	break;
	}
    case pptCall:
    case pptAssignment:
        codeExpression(pfc, f, pp, 0);
	break;
    case pptTuple:
        {
	struct pfParse *p;
	for (p = pp->children; p != NULL; p = p->next)
	    codeStatement(pfc, f, p);
	break;
	}
    case pptVarInit:
	{
	struct pfParse *init = pp->children->next->next;
	if (init != 0)
	    {
	    codeExpression(pfc, f, pp, 0);
	    }
        break;
	}
    default:
        fprintf(f, "[%s statement];\n", pfParseTypeAsString(pp->type));
	break;
    }
}

static void codeFunction(struct pfCompile *pfc, FILE *f, 
	struct pfParse *funcDec)
/* Emit C code for function. */
{
struct pfVar *funcVar = funcDec->var;
struct pfType *funcType = funcVar->ty;
struct pfType *inTuple = funcType->children;
struct pfType *outTuple = inTuple->next;
struct pfParse *body = funcDec->children->next->next->next;

/* Put out function prototype and opening brace. */
fprintf(f, "void %s()\n{\n", funcVar->name);

/* Print out input parameters. */
    {
    struct pfType *in;
    int inIx = 0;
    for (in = inTuple->children; in != NULL; in = in->next)
        {
	printType(pfc, f, in->base);
	fprintf(f, " %s = ", in->fieldName);
	printParamAccess(pfc, f, in->base, inIx);
	fprintf(f, ";\n");
	inIx += 1;
	}
    }

/* Print out output parameters. */
    {
    struct pfType *out;
    for (out = outTuple->children; out != NULL; out = out->next)
        {
	printType(pfc, f, out->base);
	fprintf(f, " %s;\n", out->fieldName);
	}
    }

/* Print out body (which is a compound statement) */
codeStatement(pfc, f, body);

/* Save the output. */
    {
    int outIx = 0;
    struct pfType *out;
    for (out = outTuple->children; out != NULL; out = out->next)
        {
	printType(pfc, f, out->base);
	fprintf(f, " ");
	printParamAccess(pfc, f, out->base, outIx);
	fprintf(f, " = %s;\n", out->fieldName);
	outIx += 1;
	}
    }

/* Close out function. */
fprintf(f, "}\n\n");
}

static void codeMethods(struct pfCompile *pfc, FILE *f, struct pfParse *class)
/* Print out methods in class. */
{
struct pfParse *classCompound = class->children->next;
struct pfParse *statement;
for (statement = classCompound->children; statement != NULL; 
     statement = statement->next)
    {
    switch (statement->type)
        {
	case pptToDec:
	case pptParaDec:
	case pptFlowDec:
	    fprintf(f, "[code method %s.%s]\n", class->name, statement->name);
	    break;
	}
    }
}

static void codeScope(
	struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	boolean isModule, boolean isClass)
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
    rPrintFields(pfc, f, base); 
    fprintf(f, "};\n\n");
    }
hashElFreeList(&helList);

/* Get declaration list and sort it. */
helList = hashElListHash(scope->vars);
slSort(&helList, hashElCmp);

/* Print out function prototypes. */
for (hel = helList; hel != NULL; hel = hel->next)
    {
    struct pfVar *var = hel->val;
    struct pfType *type = var->ty;
    if (var->ty->isFunction)
        {
	fprintf(f, "void %s();\n", var->name);
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
	printType(pfc, f, type->base);
	fprintf(f, " %s", var->name);
	if (!isInitialized(var))
	    fprintf(f, "=0");
	fprintf(f, ";\n");
	gotVar = TRUE;
	}
    }
if (gotVar)
    fprintf(f, "\n");

hashElFreeList(&helList);

/* Print out function declarations */
for (p = pp->children; p != NULL; p = p->next)
    {
    switch (p->type)
        {
	case pptToDec:
	case pptParaDec:
	case pptFlowDec:
	    codeFunction(pfc, f, p);
	    break;
	case pptClass:
	    codeMethods(pfc, f, p);
	    break;
	}
    }

/* Print out other statements */
if (isModule)
    fprintf(f, "void _pfInit_%s()\n{\n", pp->name);
for (p = pp->children; p != NULL; p = p->next)
    {
    switch (p->type)
        {
	case pptToDec:
	case pptParaDec:
	case pptFlowDec:
	case pptNop:
	case pptClass:
	    break;
	default:
	    codeStatement(pfc, f, p);
	    break;
	}
    }
if (isModule)
    fprintf(f, "}\n");
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
    codeScope(pfc, f, module, TRUE, FALSE);
    }
}

