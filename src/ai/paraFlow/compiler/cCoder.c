/* cCoder - produce C code from type-checked parse tree. */

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

static void codeScopeVars(struct pfCompile *pfc, FILE *f, 
	struct pfScope *scope);
/* Print out variable declarations associated with scope. */

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
    return "Bit";
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
    fprintf(f, "_pf_%s", s);
}

static char *stackName = "_pf_stack";
static char *stackType = "_pf_Stack";

static void codeParamAccess(struct pfCompile *pfc, FILE *f, 
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

struct compOutType
/* Composite output type. */
    {
    struct compOutType *next;	/* Next in list. */
    char *code;		/* number:name and parenthesis code. */
    int id;		/* Unique id. */
    struct pfType *type;	/* Unpacked version of type */
    };

static int cotId = 0;

static void encodeType(struct pfType *type, struct dyString *dy)
/* Encode type recursively into dy. */
{
// dyStringPrintf(dy, "%d:", type->base->scope->id);
// dyStringAppend(dy, type->base->name);
dyStringPrintf(dy, "%d", type->base->id);
if (type->children != NULL)
    {
    dyStringAppendC(dy, '(');
    for (type = type->children; type != NULL; type = type->next)
        {
	encodeType(type, dy);
	if (type->next != NULL)
	    dyStringAppendC(dy, ',');
	}
    dyStringAppendC(dy, ')');
    }
}

struct compOutType *compTypeLookup(struct hash *hash, struct dyString *dy,
	struct pfType *type)
/* Find compOutTYpe that corresponds to type in hash */
{
dyStringClear(dy);
encodeType(type, dy);
return hashMustFindVal(hash, dy->string);
}

static int typeId(struct pfCompile *pfc, struct pfType *type)
/* Return run time type ID associated with type */
{
struct dyString *dy = dyStringNew(256);
struct compOutType *cot = compTypeLookup(pfc->runTypeHash, dy, type);
int id = cot->id;
dyStringFree(&dy);
return id;
}

static void rFillCompHash(FILE *f,
	struct hash *hash, struct dyString *dy, struct pfParse *pp)
/* Fill in hash with uniq types. */
{
struct pfParse *p;
for (p = pp->children; p != NULL; p = p->next)
    rFillCompHash(f, hash, dy, p);
if (pp->ty)
    {
    dyStringClear(dy);
    encodeType(pp->ty, dy);
    if (!hashLookup(hash, dy->string))
        {
	struct compOutType *cot;
	AllocVar(cot);
	hashAddSaveName(hash, dy->string, cot, &cot->code);
	cot->id = ++cotId;
	cot->type = pp->ty;
	fprintf(f, "  {%d, \"%s\"},\n", cot->id, cot->code);
	}
    }
}

struct hash *hashCompTypes(struct pfParse *program, 
	struct dyString *dy, FILE *f)
/* Create a hash full of compOutTypes. */
{
struct hash *hash = hashNew(0);
rFillCompHash(f, hash, dy, program);
return hash;
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
if (stack == 0)
    fprintf(f, "%s(%s);\n", pp->name, stackName);
else
    fprintf(f, "%s(%s+%d);\n", pp->name, stackName, stack);
return outCount;
}

static void codeRunTimeError(struct pfCompile *pfc, FILE *f,
	struct pfToken *tok, char *message)
/* Print code for a run time error message. */
{
char *file;
int line, col;
pfSourcePos(tok->source, tok->text, &file, &line, &col);
fprintf(f, "errAbort(\"\\nRun time error line %d col %d of %s: %s\");\n", 
	line+1, col+1, file, message);
}

static int pushArrayIndexAndBoundsCheck(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Put collection and index on stack.  Check index is in range. 
 * Return offset to index. */
{
struct pfType *outType = pp->ty;
struct pfParse *collection = pp->children;
struct pfBaseType *colBase = collection->ty->base;
struct pfParse *index = collection->next;

/* Push array and index onto expression stack */
int offset = codeExpression(pfc, f, collection, stack);
codeExpression(pfc, f, index, stack+offset);

/* Do bounds checking */
fprintf(f, "if (");
codeParamAccess(pfc, f, pfc->intType, stack+offset);
fprintf(f, "< 0 || ");
codeParamAccess(pfc, f, pfc->intType, stack+offset);
fprintf(f, " >= ");
codeParamAccess(pfc, f, pfc->arrayType, stack);
fprintf(f, "->count)\n  ");
codeRunTimeError(pfc, f, pp->tok, "array access out of bounds");
return offset;
}

static void codeArrayAccess(struct pfCompile *pfc, FILE *f,
	struct pfBaseType *base, int stack, int indexOffset)
/* Print out code to access array (on either left or right
 * side */
{
fprintf(f, "((");
printType(pfc, f, base);
fprintf(f, "*)(");
codeParamAccess(pfc, f, pfc->arrayType, stack);
fprintf(f, "->elements + %d * ",  base->size);
codeParamAccess(pfc, f, pfc->intType, stack+indexOffset);
fprintf(f, "))[0]");
}

static int codeIndexExpression(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for index expression (not on left side of
 * assignment */
{
struct pfType *outType = pp->ty;
struct pfParse *collection = pp->children;
struct pfBaseType *colBase = collection->ty->base;
if (colBase == pfc->arrayType)
    {
    int indexOffset = pushArrayIndexAndBoundsCheck(pfc, f, pp, stack);
    codeParamAccess(pfc, f, outType->base, stack);
    fprintf(f, " = ");
    codeArrayAccess(pfc, f, outType->base, stack, indexOffset);
    fprintf(f, ";\n");
    }
else
    {
    fprintf(f, "(ugly coding index for something other than array)");
    }
return 1;
}

static void codeIndexLval(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op, int expSize)
/* Generate code for index expression  on left side of expression */
{
struct pfType *outType = pp->ty;
struct pfParse *collection = pp->children;
struct pfBaseType *colBase = collection->ty->base;
if (colBase == pfc->arrayType)
    {
    int emptyStack = stack + expSize;
    int indexOffset = pushArrayIndexAndBoundsCheck(pfc, f, pp, emptyStack);
    codeArrayAccess(pfc, f, outType->base, emptyStack, indexOffset);
    fprintf(f, " %s ", op);
    codeParamAccess(pfc, f, outType->base, stack);
    fprintf(f, ";\n");
    }
else
    {
    fprintf(f, "(ugly coding index for something other than array)");
    }
}

static int lvalOffStack(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op, int expSize)
/* Take an lval off of stack. */
{
switch (pp->type)
    {
    case pptVarUse:
    case pptVarInit:
	fprintf(f, "%s %s ", pp->name , op);
	codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, ";\n");
	return 1;
    case pptTuple:
        {
	int total = 0;
	struct pfParse *p;
	for (p = pp->children; p != NULL; p = p->next)
	    total += lvalOffStack(pfc, f, p, stack+total, op, expSize-total);
	return total;
	}
    case pptIndex:
        {
	codeIndexLval(pfc, f, pp, stack, op, expSize);
	return 1;
	}
    default:
        fprintf(f, "using %s %s as an lval\n", pp->name, pfParseTypeAsString(pp->type));
	return 0;
    }
}

static int codeAssignment(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op)
/* Generate code for an assignment. */
{
struct pfParse *lval = pp->children;
struct pfParse *rval = lval->next;
int eStart = stack, eEnd;
int expSize = codeExpression(pfc, f, rval, stack);
lvalOffStack(pfc, f, lval, stack, op, expSize);
return 0;
}

static void codeTupleIntoSomething(struct pfCompile *pfc, FILE *f,
	struct pfType *type, int stack, int tupleSize)
/* Code a tuple into array of given type. */
{
struct pfBaseType *base = type->children->base;
char *name;
if (base == pfc->bitType || base == pfc->byteType || base == pfc->shortType
    || base == pfc->intType || base == pfc->longType || base == pfc->floatType
    || base == pfc->doubleType || base == pfc->stringType || base == pfc->varType)
    {
    name = base->name;
    }
else
    {
    name = "class";
    }
fprintf(f, "_pf_%s_%s_from_tuple(%s+%d, %d, %d, %d);\n", name, type->base->name,
	stackName, stack, tupleSize, typeId(pfc, type), 
	typeId(pfc, type->children));
}

static void codeTupleIntoCollection(struct pfCompile *pfc, FILE *f,
	struct pfType *type, int stack, int tupleSize)
/* Shovel tuple into array, list, dir, tree. */
{
struct pfBaseType *base = type->base;
codeParamAccess(pfc, f, base, stack);
fprintf(f, " = ");
if (base == pfc->arrayType || base == pfc->listType || base == pfc->treeType
	|| base == pfc->dirType)
    {
    codeTupleIntoSomething(pfc, f, type, stack, tupleSize);
    }
else
    {
    internalErr();
    }
fprintf(f, ";\n");
}

static void codeVarInit(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for an assignment. */
{
struct pfParse *lval = pp;
struct pfParse *rval = pp->children->next->next;
int count = codeExpression(pfc, f, rval, stack);
switch (rval->type)
    {
    case pptUniformTuple:
	codeTupleIntoCollection(pfc, f, lval->ty, stack, count);
	lvalOffStack(pfc, f, lval, stack, "=", 1);
        break;
    default:
	lvalOffStack(pfc, f, lval, stack, "=", 1);
	break;
    }
}

	
static int codeBinaryOp(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op)
/* Emit code for some sort of binary op. */
{
struct pfParse *lval = pp->children;
struct pfParse *rval = lval->next;
codeExpression(pfc, f, lval, stack);
codeExpression(pfc, f, rval, stack+1);
codeParamAccess(pfc, f, pp->ty->base, stack);
fprintf(f, " = ");
codeParamAccess(pfc, f, lval->ty->base, stack);
fprintf(f, " %s ", op);
codeParamAccess(pfc, f, rval->ty->base, stack+1);
fprintf(f, ";\n");
return 1;
}

static int codeUnaryOp(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op)
/* Emit code for some sort of unary op. */
{
struct pfParse *child = pp->children;
struct pfBaseType *base = child->ty->base;
codeExpression(pfc, f, child, stack);
codeParamAccess(pfc, f, base, stack);
fprintf(f, " = ");
fprintf(f, "%s", op);
codeParamAccess(pfc, f, base, stack);
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
    case pptUniformTuple:
    case pptKeyVal:
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
	return codeAssignment(pfc, f, pp, stack, "=");
    case pptPlusEquals:
	return codeAssignment(pfc, f, pp, stack, "+=");
    case pptMinusEquals:
	return codeAssignment(pfc, f, pp, stack, "-=");
    case pptMulEquals:
	return codeAssignment(pfc, f, pp, stack, "*=");
    case pptDivEquals:
	return codeAssignment(pfc, f, pp, stack, "/=");
    case pptVarInit:
    	codeVarInit(pfc, f, pp, stack);
	return 0;
    case pptVarUse:
	codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = %s;\n", pp->name);
	return 1;
    case pptIndex:
         return codeIndexExpression(pfc, f, pp, stack);
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
    case pptGreater:
        return codeBinaryOp(pfc, f, pp, stack, ">");
    case pptGreaterOrEquals:
        return codeBinaryOp(pfc, f, pp, stack, ">=");
    case pptSame:
        return codeBinaryOp(pfc, f, pp, stack, "==");
    case pptNotSame:
        return codeBinaryOp(pfc, f, pp, stack, "!=");
    case pptLess:
        return codeBinaryOp(pfc, f, pp, stack, "<");
    case pptLessOrEquals:
        return codeBinaryOp(pfc, f, pp, stack, "<=");
    case pptBitAnd:
        return codeBinaryOp(pfc, f, pp, stack, "&");
    case pptBitOr:
        return codeBinaryOp(pfc, f, pp, stack, "|");
    case pptNegate:
         return codeUnaryOp(pfc, f, pp, stack, "-");
    case pptFlipBits:
         return codeUnaryOp(pfc, f, pp, stack, "~");
    case pptNot:
         return codeUnaryOp(pfc, f, pp, stack, "!");
    case pptConstBit:
    case pptConstByte:
    case pptConstShort:
    case pptConstInt:
    case pptConstLong:
    case pptConstFloat:
    case pptConstDouble:
        {
	struct pfToken *tok = pp->tok;
	codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = ");
	switch (tok->type)
	    {
	    case pftString:
		assert(pp->type == pptConstBit);
	        fprintf(f, "1;\n");
		break;
	    case pftFloat:
	        fprintf(f, "%1.0f;\n", pp->tok->val.x);
		break;
	    case pftInt:
		fprintf(f, "%lld;\n", pp->tok->val.i);
		break;
	    }
	return 1;
	}
    case pptConstString:
        {
	assert(pp->tok->type == pftString);
	codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = ");
	printEscapedString(f, pp->tok->val.s);
	fprintf(f, ";\n");
	return 1;
	}
    case pptCastBitToBit:
    case pptCastBitToByte:
    case pptCastBitToShort:
    case pptCastBitToInt:
    case pptCastBitToLong:
    case pptCastBitToFloat:
    case pptCastBitToDouble:
    case pptCastByteToBit:
    case pptCastByteToByte:
    case pptCastByteToShort:
    case pptCastByteToInt:
    case pptCastByteToLong:
    case pptCastByteToFloat:
    case pptCastByteToDouble:
    case pptCastShortToBit:
    case pptCastShortToByte:
    case pptCastShortToShort:
    case pptCastShortToInt:
    case pptCastShortToLong:
    case pptCastShortToFloat:
    case pptCastShortToDouble:
    case pptCastIntToBit:
    case pptCastIntToByte:
    case pptCastIntToShort:
    case pptCastIntToInt:
    case pptCastIntToLong:
    case pptCastIntToFloat:
    case pptCastIntToDouble:
    case pptCastLongToBit:
    case pptCastLongToByte:
    case pptCastLongToShort:
    case pptCastLongToInt:
    case pptCastLongToLong:
    case pptCastLongToFloat:
    case pptCastLongToDouble:
    case pptCastFloatToBit:
    case pptCastFloatToByte:
    case pptCastFloatToShort:
    case pptCastFloatToInt:
    case pptCastFloatToLong:
    case pptCastFloatToFloat:
    case pptCastFloatToDouble:
    case pptCastDoubleToBit:
    case pptCastDoubleToByte:
    case pptCastDoubleToShort:
    case pptCastDoubleToInt:
    case pptCastDoubleToLong:
    case pptCastDoubleToFloat:
    case pptCastDoubleToDouble:
	codeExpression(pfc, f, pp->children, stack);
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = ");
        codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, ";\n");
	return 0;
    case pptCastStringToBit:
	codeExpression(pfc, f, pp->children, stack);
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = (0 != ");
        codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, ");");
	return 0;
    case pptCastTypedToVar:
	{
	codeExpression(pfc, f, pp->children, stack);
        codeParamAccess(pfc, f, pfc->varType, stack);
	fprintf(f, ".val.%s = ", typeKey(pfc, pp->children->ty->base));
	codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, ";\n");
        codeParamAccess(pfc, f, pfc->varType, stack);
	fprintf(f, ".typeId = %d;\n", typeId(pfc, pp->children->ty));
	return 0;
	}
    default:
	{
	fprintf(f, "(%s expression)\n", pfParseTypeAsString(pp->type));
	return 0;
	}
    }
}


static void codeForeach(struct pfCompile *pfc, FILE *f,
	struct pfParse *foreach)
/* Emit C code for foreach statement. */
{
struct pfParse *elPp = foreach->children;
struct pfParse *collectionPp = elPp->next;
struct pfBaseType *base = collectionPp->ty->base;
struct pfBaseType *elBase = collectionPp->ty->children->base;
struct pfParse *body = collectionPp->next;
char *elName = elPp->name;
char *collectionName = collectionPp->name;
boolean isArray = (base == pfc->arrayType);

/* Print element variable in a new scope. */
fprintf(f, "{\n");
codeScopeVars(pfc, f, foreach->scope);

/* Also print some iteration stuff. */
if (isArray)
    {
    fprintf(f, "int _pf_offset;\n");
    fprintf(f, "int _pf_elSize = %s->elSize;\n", collectionName);
    fprintf(f, "int _pf_endOffset = %s->count * _pf_elSize;\n", collectionName);
    fprintf(f, "for (_pf_offset=0; _pf_offset<_pf_endOffset; _pf_offset += _pf_elSize)\n");
    fprintf(f, "{\n");
    fprintf(f, "%s = *((", elName);
    printType(pfc, f, elBase);
    fprintf(f, "*)(%s->elements + _pf_offset));\n", collectionName);
    codeStatement(pfc, f, body);
    fprintf(f, "}\n");
    }
else
    {
    fprintf(f, "Pf_iterator _pf_ix = _pf_%s_iterator_init(%s);\n",
    	base->name, collectionName);
    fprintf(f, "while (_pf_ix.next(&_pf_ix, &%s))\n", elName);
    codeStatement(pfc, f, body);
    }
fprintf(f, "}\n");
}

static void codeFor(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Emit C code for for statement. */
{
struct pfParse *init = pp->children;
struct pfParse *end = init->next;
struct pfParse *next = end->next;
struct pfParse *body = next->next;

fprintf(f, "{\n");
codeScopeVars(pfc, f, pp->scope);
codeStatement(pfc, f, init);
fprintf(f, "for(;;)\n");
fprintf(f, "{\n");
codeExpression(pfc, f, end, 0);
fprintf(f, "if (!");
codeParamAccess(pfc, f, pfc->bitType, 0);
fprintf(f, ") break;\n");
codeStatement(pfc, f, body);
codeStatement(pfc, f, next);
fprintf(f, "}\n");
fprintf(f, "}\n");
}

static void codeWhile(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Emit C code for while statement. */
{
struct pfParse *end = pp->children;
struct pfParse *body = end->next;
fprintf(f, "for(;;)\n");
fprintf(f, "{\n");
codeExpression(pfc, f, end, 0);
fprintf(f, "if (!");
codeParamAccess(pfc, f, pfc->bitType, 0);
fprintf(f, ") break;\n");
codeStatement(pfc, f, body);
fprintf(f, "}\n");
}

static void codeIf(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Emit C code for for statement. */
{
struct pfParse *cond = pp->children;
struct pfParse *trueBody = cond->next;
struct pfParse *falseBody = trueBody->next;
codeExpression(pfc, f, cond, 0);
fprintf(f, "if (");
codeParamAccess(pfc, f, pfc->bitType, 0);
fprintf(f, ")\n");
fprintf(f, "{\n");
codeStatement(pfc, f, trueBody);
fprintf(f, "}\n");
if (falseBody != NULL)
    {
    fprintf(f, "else\n");
    fprintf(f, "{\n");
    codeStatement(pfc, f, falseBody);
    fprintf(f, "}\n");
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
    case pptPlusEquals:
    case pptMinusEquals:
    case pptMulEquals:
    case pptDivEquals:
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
    case pptForeach:
	codeForeach(pfc, f, pp);
	break;
    case pptFor:
	codeFor(pfc, f, pp);
	break;
    case pptWhile:
        codeWhile(pfc, f, pp);
	break;
    case pptIf:
        codeIf(pfc, f, pp);
	break;
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
fprintf(f, "void %s(%s *%s)\n{\n", funcVar->name, stackType, stackName);

/* Print out input parameters. */
    {
    struct pfType *in;
    int inIx = 0;
    for (in = inTuple->children; in != NULL; in = in->next)
        {
	printType(pfc, f, in->base);
	fprintf(f, " %s = ", in->fieldName);
	codeParamAccess(pfc, f, in->base, inIx);
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
	codeParamAccess(pfc, f, out->base, outIx);
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

static void codeVarsInHelList(struct pfCompile *pfc, FILE *f,
	struct hashEl *helList)
/* Print out variable declarations in helList. */
{
struct hashEl *hel;
boolean gotVar = FALSE;
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
}

static void codeScopeVars(struct pfCompile *pfc, FILE *f, struct pfScope *scope)
/* Print out variable declarations associated with scope. */
{
struct hashEl *helList = hashElListHash(scope->vars);
slSort(&helList, hashElCmp);
codeVarsInHelList(pfc, f, helList);
hashElFreeList(&helList);
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
codeVarsInHelList(pfc, f, helList);
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
    fprintf(f, "void _pf_init(%s *%s)\n{\n", stackType, stackName);
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

static void printConclusion(struct pfCompile *pfc, FILE *f)
/* Print out C code for end of program. */
{
fputs(
"\n"
"int main(int argv, char *argc[])\n"
"{\n"
"static _pf_Stack stack[4*1024];\n"
"_pf_init_types(_pf_base_info, _pf_base_info_count,\n"
"               _pf_type_info, _pf_type_info_count,\n"
"               _pf_field_info, _pf_field_info_count);\n"
"_pf_init(stack);\n"
"return 0;\n"
"}\n", f);
}

static void printBaseVar(FILE *f, struct pfBaseType *base)
/* Print out name of variable where we store base type. */
{
char *s, c;
fprintf(f, "_pf_base_s%d_", base->scope->id);
s = base->name;
while ((c = *s++) != 0)
    {
    if (isalnum(c))
        fputc(c, f);
    else
        fputc('_', f);
    }
}

static struct hash *printTypeInfo(struct pfCompile *pfc, 
	struct pfParse *program, FILE *f)
/* Print out info on types. Return hash of compOutType. */
{
struct pfScope *scope;
struct hash *compTypeHash;
struct dyString *dy = dyStringNew(0);


fprintf(f, "/* All base types */\n");

fprintf(f, "struct _pf_base_info _pf_base_info[] = {\n");
for (scope = pfc->scopeList; scope != NULL; scope = scope->next)
    {
    struct hashEl *hel, *helList = hashElListHash(scope->types);
    int scopeId = scope->id;
    slSort(&helList, hashElCmp);
    for (hel = helList; hel != NULL; hel = hel->next)
        {
	struct pfBaseType *base = hel->val;
	fprintf(f, "  {%d, ", base->id);
	fprintf(f, "\"%d:", scopeId);
	fprintf(f, "%s\", ", base->name);
	if (base->parent == NULL)
	    fprintf(f, "0, ");
	else
	    fprintf(f, "%d, ", base->parent->id);
	fprintf(f, "},\n");
	}
    hashElFreeList(&helList);
    }
fprintf(f, "};\n");
fprintf(f, "int _pf_base_info_count = %d;\n\n", pfBaseTypeCount());

fprintf(f, "/* All composed types */\n");
fprintf(f, "struct _pf_type_info _pf_type_info[] = {\n");
compTypeHash = hashCompTypes(program, dy, f);
fprintf(f, "};\n");
fprintf(f, "int _pf_type_info_count = sizeof(_pf_type_info)/sizeof(_pf_type_info[0]);\n\n");

fprintf(f, "/* All field lists. */\n");
fprintf(f, "struct _pf_field_info _pf_field_info[] = {\n");
for (scope = pfc->scopeList; scope != NULL; scope = scope->next)
    {
    struct hashEl *hel, *helList = hashElListHash(scope->types);
    int scopeId = scope->id;
    slSort(&helList, hashElCmp);
    for (hel = helList; hel != NULL; hel = hel->next)
        {
	struct pfBaseType *base = hel->val;
	if (base->isClass)
	    {
	    struct pfType *field;
	    fprintf(f, "  %d, ", base->id);
	    fprintf(f, "\"");
	    for (field = base->fields; field != NULL; field = field->next)
	        {
		struct compOutType *cot = compTypeLookup(compTypeHash, dy, field);
		fprintf(f, "%d:%s", cot->id, field->fieldName);
		if (field->next != NULL)
		    fprintf(f, ",");
		}
	    fprintf(f, "\"");
	    fprintf(f, "\n");
	    }
	}
    hashElFreeList(&helList);
    }
fprintf(f, "};\n");
fprintf(f, "int _pf_field_info_count = sizeof(_pf_field_info)/sizeof(_pf_field_info[0]);\n\n");
fprintf(f, "\n");

dyStringFree(&dy);
return compTypeHash;
}

void pfCodeC(struct pfCompile *pfc, struct pfParse *program, char *fileName)
/* Generate C code for program. */
{
FILE *f = mustOpen(fileName, "w");
struct pfParse *module;
struct pfScope *scope;
struct hash *compTypeHash;

printPreamble(pfc, f);
pfc->runTypeHash = printTypeInfo(pfc, program, f);

for (module = program->children; module != NULL; module = module->next)
    {
    fprintf(f, "/* ParaFlow module %s */\n\n", module->name);
    codeScope(pfc, f, module, TRUE, FALSE);
    }
printConclusion(pfc, f);
}

