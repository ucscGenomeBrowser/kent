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
uglyf("Type mismatch\n");
errAt(pp->tok, "type mismatch");
}

static void coerceOne(struct pfCompile *pfc, struct pfParse *pp,
	struct pfType *type)
/* Make sure that a single variable is of the required type.  
 * Add casts if necessary */
{
struct pfType *pt = pp->ty;
if (pt == NULL)
    {
    if (pp->type == pptConstUse)
        {
	struct pfBaseType *base = type->base;
	if (base == pfc->byteType)
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
	else
	    internalErrAt(pp->tok);
	if (pp->type == pptConstString)
	    {
	    if (pp->tok->type != pftString)
	        expectingGot("string", pp->tok);
	    }
	else
	    {
	    if (pp->tok->type != pftInt && pp->tok->type != pftFloat)
	        expectingGot("number", pp->tok);
	    }
	pp->ty = pfTypeNew(base);
	}
    else
	{
        internalErrAt(pp->tok);
	}
    }
else
    {
    if (pt->base != type->base)
	{
	if (pt->isTuple && slCount(pt->children) == 1 && pt->children->base == type->base)
	    {
	    }
	else if (type->isTuple == slCount(pt->children) == 1 && type->children->base == pt->base)
	    {
	    }
	else
	    {
	    typeMismatch(pp);
	    }
	}
    }
}

static void coerceTuple(struct pfCompile *pfc, struct pfParse *tuple,
	struct pfType *types)
/* Make sure that tuple is of correct type. */
{
int tupSize = slCount(tuple->children);
int typeSize = slCount(types->children);
struct pfParse *pp;
struct pfType *type;
if (tupSize != typeSize)
    {
    errAt(tuple->tok, "Expecting tuple of %d, got tuple of %d", 
    	typeSize, tupSize);
    }
for (pp=tuple->children, type = types->children; pp != NULL;
     pp = pp->next, type = type->next)
     {
     coerceOne(pfc, pp, type);
     }
}

static void coerceCall(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure that parameters to call are right.  Then
 * set pp->type to call's return type. */
{
uglyf("coerceCall\n");
struct pfParse *function = pp->children;
struct pfParse *paramTuple = function->next;
struct pfVar *functionVar = function->var;
struct pfType *functionType = functionVar->ty;
struct pfType *inputType = functionType->children;
struct pfType *outputType = inputType->next;
coerceTuple(pfc, paramTuple, inputType);
pp->ty = outputType;
uglyf("done coerceCall\n");
}

struct pfType *coerceLval(struct pfCompile *pfc, struct pfParse *pp)
/* Ensure that pp can be assigned.  Return it's type */
{
switch (pp->type)
    {
    case pptVarInit:
    case pptVarUse:
        return pp->ty;
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
struct pfParse *rval = lval->next;
struct pfType *destType = coerceLval(pfc, lval);
coerceOne(pfc, rval, destType);
#ifdef SOON
#endif /* SOON */
pp->ty = destType;
}


static void coerceBinaryMathOp(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure that both sides of a math operation agree. */
{
struct pfParse *lval = pp->children;
struct pfParse *rval = lval->next;

if (lval->type == pptConstUse)
    {
    if (rval->ty != NULL)
	 coerceOne(pfc, lval, rval->ty);

    }
if (rval->type == pptConstUse)
    {
    if (lval->ty != NULL)
	coerceOne(pfc, rval, lval->ty);
    }
if (lval->type == NULL)
    expectingGot("number", lval->tok);
if (rval->type == NULL)
    expectingGot("number", rval->tok);
if (!pfTypeSame(lval->ty, rval->ty))
    typeMismatch(pp);
pp->ty = lval->ty;
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
    case pptMul:
    case pptDiv:
    case pptPlus:
    case pptMinus:
        coerceBinaryMathOp(pfc, pp);
	break;
    case pptAssignment:
        coerceAssign(pfc, pp);
	break;
    }
}
