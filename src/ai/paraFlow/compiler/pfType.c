/* pfType - ParaFlow type heirarchy */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "pfParse.h"
#include "pfCompile.h"
#include "pfType.h"

struct pfBaseType *pfBaseTypeNew(struct pfScope *scope, char *name, 
	boolean isCollection, struct pfBaseType *parent)
/* Create new base type. */
{
struct pfBaseType *base;
AllocVar(base);
if (parent != NULL)
    {
    base->parent = parent;
    slAddHead(&parent->children, base);
    }
base->name = cloneString(name);
base->scope = scope;
base->isCollection = isCollection;
return base;
}

struct pfType *pfTypeNew(struct pfBaseType *base)
/* Create new high level type object */
{
struct pfType *ty;
AllocVar(ty);
ty->base = base;
return ty;
}

boolean pfTypeSame(struct pfType *a, struct pfType *b)
/* Return TRUE if a and b are same type logically */
{
struct pfType *a1, *b1;

/* First make sure all children match */
for (a1 = a->children, b1 = b->children; a1 != NULL && b1 != NULL;
	a1 = a1->next, b1 = b1->next)
    {
    if (!pfTypeSame(a1, b1))
        return FALSE;
    }
if (a1 != NULL || b1 != NULL)	/* Different number of children - can't match */
    return FALSE;
return a->base == b->base;
}

void pfTypeDump(struct pfType *ty, FILE *f)
/* Write out info on ty to file.  (No newlines written) */
{
if (ty == NULL)
    fprintf(f, "void");
else if (ty->isTuple)
    {
    fprintf(f, "(");
    for (ty = ty->children; ty != NULL; ty = ty->next)
        {
	pfTypeDump(ty, f);
	if (ty->next != NULL)
	    fprintf(f, " ");
	}
    fprintf(f, ")");
    }
else if (ty->isFunction)
    {
    fprintf(f, "%s ", ty->base->name);
    pfTypeDump(ty->children, f);
    fprintf(f, " into ");
    pfTypeDump(ty->children->next, f);
    }
else if (ty->isModule)
    {
    fprintf(f, ".module.");
    }
else
    {
    fprintf(f, "%s", ty->base->name);
    ty = ty->children;
    if (ty != NULL)
        {
	fprintf(f, " of ");
	pfTypeDump(ty, f);
	}
    }
}

static void typeMismatch(struct pfParse *pp)
/* Complain about type mismatch at node. */
{
errAt(pp->tok, "type mismatch");
}

static int baseTypeLogicalSize(struct pfCompile *pfc, struct pfBaseType *base)
/* Return logical size of type - 0 for smallest, 1 for next smallest, etc. */
{
if (base == pfc->bitType)
    return 0;
else if (base == pfc->byteType)
    return 1;
else if (base == pfc->shortType)
    return 2;
else if (base == pfc->intType)
    return 3;
else if (base == pfc->longType)
    return 4;
else if (base == pfc->floatType)
    return 5;
else if (base == pfc->doubleType)
    return 6;
else
    {
    internalErr();
    return 0;
    }
}


static void insertCast(enum pfParseType castType, struct pfType *newType,
	struct pfParse **pPp)
/* Insert a cast operation on top of *pPp */
{
struct pfParse *pp = *pPp;
struct pfParse *cast = pfParseNew(castType, pp->tok, pp->parent, pp->scope);
cast->next = pp->next;
cast->children = pp;
cast->ty = newType;
pp->parent = cast;
pp->next = NULL;
*pPp = cast;
}

static void numericCast(struct pfCompile *pfc,
	struct pfType *newType, struct pfParse **pPp)
/* Insert a cast operation to base on top of *pPp */
{
struct pfBaseType *newBase = newType->base;
struct pfParse *pp = *pPp;
struct pfBaseType *oldBase = pp->ty->base;
int numTypeCount = 7;
enum pfParseType castType = pptCastBitToBit;
castType += numTypeCount * baseTypeLogicalSize(pfc, oldBase);
castType += baseTypeLogicalSize(pfc, newBase);
insertCast(castType, newType, pPp);
}

void pfTypeOnTuple(struct pfCompile *pfc, struct pfParse *pp)
/* Create tuple type and link in types of all children. */
{
pp->ty = pfTypeNew(pfc->tupleType);
pp->ty->isTuple = TRUE;
if (pp->children != NULL)
    {
    pp->ty->children = pp->children->ty;
    pp = pp->children;
    while (pp->next != NULL)
	{
	if (pp->ty == NULL)
	    errAt(pp->tok, "void value in tuple");
	pp->ty->next = pp->next->ty;
	pp = pp->next;
	}
    }
}


static void coerceOne(struct pfCompile *pfc, struct pfParse **pPp,
	struct pfType *type);
/* Make sure that a single variable is of the required type. 
 * Add casts if necessary */

static void coerceToBit(struct pfCompile *pfc, struct pfParse **pPp)
/* Make sure that pPp is a bit. */
{
struct pfParse *pp = *pPp;
if (pp->ty == NULL && pp->type != pptConstUse)
    expectingGot("logical value", pp->tok);
if (pp->type == pptConstUse || pp->ty->base != pfc->bitType)
    {
    struct pfType *type = pfTypeNew(pfc->bitType);
    coerceOne(pfc, pPp, type);
    }
}

static void coerceTuple(struct pfCompile *pfc, struct pfParse *tuple,
	struct pfType *types)
/* Make sure that tuple is of correct type. */
{
int tupSize = slCount(tuple->children);
int typeSize = slCount(types->children);
struct pfParse **pos;
struct pfType *type;
if (tupSize != typeSize)
    {
    errAt(tuple->tok, "Expecting tuple of %d, got tuple of %d", 
    	typeSize, tupSize);
    }
if (tupSize == 0)
    return;
pos = &tuple->children;
type = types->children;
for (;;)
     {
     coerceOne(pfc, pos, type);
     pos = &(*pos)->next;
     type = type->next;
     if (type == NULL)
         break;
     }
tuple->ty = types;
}

static void coerceCall(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure that parameters to call are right.  Then
 * set pp->type to call's return type. */
{
struct pfParse *function = pp->children;
struct pfParse *paramTuple = function->next;
struct pfVar *functionVar = function->var;
struct pfType *functionType = functionVar->ty;
struct pfType *inputType = functionType->children;
struct pfType *outputType = inputType->next;
coerceTuple(pfc, paramTuple, inputType);
if (outputType->children != NULL && outputType->children->next == NULL)
    pp->ty = CloneVar(outputType->children);
else
    pp->ty = CloneVar(outputType);
}

static void coerceTupleToCollection(struct pfCompile *pfc, 
	struct pfParse **pPp, struct pfType *type)
/* Given a type that is a collection, and a parse tree that
 * is a tuple, do any casting required inside the tuple
 * to get the members of the tuple to be of the same type
 * as the collection elements.  Then put in a castTupleToCollection
 * node in the tree. */
{
struct pfParse *tuple = *pPp;
struct pfType *elType  = type->children;
struct pfParse **pos;
struct pfType *ty = CloneVar(type);
uglyf("coerceTupleToCollection of %s\n", elType->base->name);
for (pos = &tuple->children; *pos != NULL; pos = &(*pos)->next)
     coerceOne(pfc, pos, elType);
pfTypeOnTuple(pfc, tuple);
tuple->type = pptUniformTuple;
}

static void coerceOne(struct pfCompile *pfc, struct pfParse **pPp,
	struct pfType *type)
/* Make sure that a single variable is of the required type.  
 * Add casts if necessary */
{
struct pfParse *pp = *pPp;
struct pfType *pt = pp->ty;
if (pt == NULL)
    {
    internalErrAt(pp->tok);
    }
else
    {
    if (pp->type == pptConstUse)
        {
	struct pfBaseType *base = type->base;
	if (base == pfc->bitType)
	    pp->type = pptConstBit;
	else if (base == pfc->byteType)
	    pp->type = pptConstByte;
	else if (base == pfc->shortType)
	    pp->type = pptConstShort;
	else if (base == pfc->intType)
	    pp->type = pptConstInt;
	else if (base == pfc->longType)
	    pp->type = pptConstLong;
	else if (base == pfc->floatType)
	    pp->type = pptConstFloat;
	else if (base == pfc->doubleType)
	    pp->type = pptConstDouble;
	else if (base == pfc->stringType)
	    pp->type = pptConstString;
	else if (base == pfc->varType)
	    {
	    switch (pp->tok->type)
	        {
		case pftInt:
		    pp->type = pptConstInt;
		    break;
		case pftFloat:
		    pp->type = pptConstDouble;
		    break;
		case pftString:
		    pp->type = pptConstString;
		    break;
		default:
		    internalErrAt(pp->tok);
		    break;
		}
	    }
	else
	    internalErrAt(pp->tok);
	if (pp->type == pptConstString)
	    {
	    if (pp->tok->type != pftString)
	        expectingGot("string", pp->tok);
	    }
	else if (pp->type == pptConstBit)
	    {
	    /* Anything can be converted to a bit. */
	    }
	else
	    {
	    if (pp->tok->type != pftInt && pp->tok->type != pftFloat)
	        expectingGot("number", pp->tok);
	    }
	pp->ty = pfTypeNew(base);
	if (base == pfc->varType)
	    {
	    struct pfType *tt = pfTypeNew(pfc->varType);
	    insertCast(pptCastTypedToVar, tt, pPp);
	    pp = *pPp;
	    }
	}
    else if (pt->base != type->base)
	{
	boolean ok = FALSE;
	if (type->base == pfc->bitType && pt->base == pfc->stringType)
	    {
	    struct pfType *tt = pfTypeNew(pfc->varType);
	    insertCast(pptCastStringToBit, tt, pPp);
	    ok = TRUE;
	    }
	else if (type->base == pfc->varType)
	    {
	    struct pfType *tt = pfTypeNew(pfc->varType);
	    insertCast(pptCastTypedToVar, tt, pPp);
	    ok = TRUE;
	    }
	else if (pt->base == pfc->varType)
	    {
	    struct pfType *tt = CloneVar(type);
	    insertCast(pptCastVarToTyped, tt, pPp);
	    ok = TRUE;
	    }
	else if (type->base->isCollection)
	    {
	    if (pt->isTuple)
	        {
		coerceTupleToCollection(pfc, pPp, type);
		ok = TRUE;
		}
	    else
	        {
		expectingGot("collection", pp->tok);
		}
	    }
	else if (pt->isTuple)
	    {
	    if (pt->children == NULL)
		errAt(pp->tok, "using void value");
	    else
		errAt(pp->tok, 
		    "expecting single value, got %d values", slCount(pt->children));
	    }
	else
	    {
	    if (type->base->parent == pfc->numType && pt->base->parent == pfc->numType)
	        {
		numericCast(pfc, type, pPp);
		ok = TRUE;
		}
	    }
	if (!ok)
	    {
	    typeMismatch(pp);
	    }
	}
    else if (type->isTuple)
        {
	assert(pt->isTuple);
	coerceTuple(pfc, pp, type);
	}
    }
}

static void coerceWhile(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure have a good conditional in while. */
{
coerceToBit(pfc, &pp->children);
}

static void coerceFor(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure have a good conditional in for. */
{
coerceToBit(pfc, &pp->children->next);
}

static void coerceIf(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure have a good conditional in if. */
{
coerceToBit(pfc, &pp->children);
}

struct pfType *coerceLval(struct pfCompile *pfc, struct pfParse *pp)
/* Ensure that pp can be assigned.  Return it's type */
{
switch (pp->type)
    {
    case pptVarInit:
    case pptVarUse:
        return pp->ty;
    case pptTuple:
	{
	struct pfParse *p;
	for (p = pp->children; p != NULL; p = p->next)
	    coerceLval(pfc, p);
        return pp->ty;
	}
    default:
        errAt(pp->tok, "Left hand of assignment is not a variable");
	return NULL;
    }
}

static void coerceAssign(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure that left half of assigment is a valid l-value,
 * and that right half of assignment can be coerced into a
 * compatible type.  Set pp->type to l-value type. */
{
struct pfParse *lval = pp->children;
struct pfType *destType = coerceLval(pfc, lval);
coerceOne(pfc, &lval->next, destType);
pp->ty = destType;
}

static void coerceVarInit(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure that variable initialization can be coerced to variable
 * type. */
{
struct pfParse *type = pp->children;
struct pfParse *symbol = type->next;
struct pfParse *init = symbol->next;

if (init != NULL)
    coerceOne(pfc, &symbol->next, type->ty);
}

static struct pfType *largerNumType(struct pfCompile *pfc,
	struct pfType *a, struct pfType *b)
/* Return a or b, whichever can hold the larger range. */
{
if (baseTypeLogicalSize(pfc, a->base) > baseTypeLogicalSize(pfc, b->base))
    return a;
else
    return b;
}

static void coerceBinaryOp(struct pfCompile *pfc, struct pfParse *pp,
	boolean floatOk, boolean stringOk)
/* Make sure that both sides of a math operation agree. */
{
struct pfParse *lval = pp->children;
struct pfParse *rval = lval->next;

if (lval->type == pptConstUse && rval->type == pptConstUse)
    {
    struct pfBaseType *base = NULL;
    boolean lIsString = (lval->tok->type == pftString);
    boolean rIsString = (rval->tok->type == pftString);
    if (lIsString ^ rIsString)
	errAt(pp->tok, "Mixing string and other variables in expression");
    else if (lIsString || rIsString)
	base = pfc->stringType;
    else if (lval->tok->type == pftFloat || rval->tok->type == pftFloat)
	base = pfc->doubleType;
    else if (lval->tok->val.i > 0x7FFFFFFFLL || rval->tok->val.i > 0x7FFFffffLL
	|| lval->tok->val.i < -0x7FFFFFFFLL || rval->tok->val.i < -0x7FFFffffLL)
        base = pfc->longType;
    else 
        base = pfc->intType;
    pp->ty = pfTypeNew(base);
    coerceOne(pfc, &lval, pp->ty);
    coerceOne(pfc, &rval, pp->ty);
    }
else
    {
    if (lval->type == pptConstUse)
	{
	if (rval->ty != NULL)
	     coerceOne(pfc, &lval, rval->ty);

	}
    if (rval->type == pptConstUse)
	{
	if (lval->ty != NULL)
	    coerceOne(pfc, &rval, lval->ty);
	}
    if (lval->ty == NULL)
	expectingGot("number", lval->tok);
    if (rval->ty == NULL)
	expectingGot("number", rval->tok);
    if (!pfTypeSame(lval->ty, rval->ty))
	{
	struct pfType *ty = largerNumType(pfc, lval->ty, rval->ty);
	coerceOne(pfc, &lval, ty);
	coerceOne(pfc, &rval, ty);
	pp->children = lval;
	lval->next = rval;
	}
    pp->ty = lval->ty;
    }
if (!floatOk)
    {
    struct pfBaseType *base = pp->ty->base;
    if (base == pfc->floatType || base == pfc->doubleType)
	 errAt(pp->tok, "Floating point numbers not allowed here");
    }
if (!stringOk)
    {
    struct pfBaseType *base = pp->ty->base;
    if (base == pfc->stringType)
	 errAt(pp->tok, "Strings not allowed here");
    }
}

static void typeConstant(struct pfCompile *pfc, struct pfParse *pp)
/* Create type for constant. */
{
struct pfToken *tok = pp->tok;
if (tok->type == pftString)
    pp->ty = pfTypeNew(pfc->stringType);
else if (tok->type == pftInt)
    pp->ty = pfTypeNew(pfc->intType);
else if (tok->type == pftFloat)
    pp->ty = pfTypeNew(pfc->doubleType);
}

void pfTypeCheck(struct pfCompile *pfc, struct pfParse *pp)
/* Check types (adding conversions where needed) on tree,
 * which should have variables bound already. */
{
struct pfParse *p;

for (p = pp->children; p != NULL; p = p->next)
    pfTypeCheck(pfc, p);

switch (pp->type)
    {
    case pptCall:
	coerceCall(pfc, pp);
        break;
    case pptWhile:
        coerceWhile(pfc, pp);
	break;
    case pptFor:
        coerceFor(pfc, pp);
	break;
    case pptIf:
        coerceIf(pfc, pp);
	break;
    case pptPlus:
        coerceBinaryOp(pfc, pp, TRUE, TRUE);
	break;
    case pptSame:
    case pptNotSame:
    case pptGreater:
    case pptLess:
    case pptGreaterOrEquals:
    case pptLessOrEquals:
	coerceBinaryOp(pfc, pp, TRUE, TRUE);
	pp->ty = pfTypeNew(pfc->bitType);
	break;
    case pptMul:
    case pptDiv:
    case pptMod:
    case pptMinus:
        coerceBinaryOp(pfc, pp, TRUE, FALSE);
	break;
    case pptBitAnd:
    case pptBitOr:
    case pptBitXor:
    case pptShiftLeft:
    case pptShiftRight:
        coerceBinaryOp(pfc, pp, FALSE, FALSE);
	break;
    case pptAssignment:
        coerceAssign(pfc, pp);
	break;
    case pptVarInit:
        coerceVarInit(pfc, pp);
	break;
    case pptTuple:
	pfTypeOnTuple(pfc, pp);
	break;
    case pptConstUse:
        typeConstant(pfc, pp);
	break;
    }
}
