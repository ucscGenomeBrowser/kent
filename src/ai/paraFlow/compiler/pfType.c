/* pfType - ParaFlow type heirarchy and type checking. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "pfParse.h"
#include "pfCompile.h"
#include "pfType.h"

static int baseTypeCount = 0;

int pfBaseTypeCount()
/* Return base type count. */
{
return baseTypeCount;
}

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
base->id = ++baseTypeCount;
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
	for (ty = ty->next; ty != NULL; ty = ty->next)
	    {
	    fprintf(f, ",");
	    pfTypeDump(ty, f);
	    }
	}
    }
}

static void typeMismatch(struct pfParse *pp, struct pfType *type)
/* Complain about type mismatch at node. */
{
if (pp->name)
    errAt(pp->tok, "Type mismatch:  %s not %s.", pp->name, type->base->name);
else
    errAt(pp->tok, "Type mismatch: expecting %s.", type->base->name);
}

static enum pfParseType pftFromTokType(struct pfToken *tok)
/* Return pptConstXXX corresponding to tok->type */
{
switch (tok->type)
    {
    case pftInt:
	return pptConstInt;
    case pftFloat:
	return pptConstDouble;
    case pftString:
	return pptConstString;
    default:
	internalErrAt(tok);
	return 0;
    }
}

static void enforceNumber(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure type of pp is numberic. */
{
if (pp->ty->base != pfc->varType)
    if (pp->ty->base->parent != pfc->numType)
	expectingGot("number", pp->tok);
if (pp->type == pptConstUse)
    pp->type = pftFromTokType(pp->tok);
}

static void enforceInt(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure type of pp is integer. */
{
struct pfBaseType *base = pp->ty->base;
enforceNumber(pfc, pp);
if (base == pfc->floatType || base == pfc->doubleType)
     expectingGot("integer", pp->tok);
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


static struct pfParse *insertCast(enum pfParseType castType, 
	struct pfType *newType, struct pfParse **pPp)
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
return cast;
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

static struct pfType *typeFromChildren(struct pfCompile *pfc, struct pfParse *pp,
     struct pfBaseType *base)
/* Create a type that is just a wrapper around children's type. */
{
struct pfType *ty = pfTypeNew(base);
if (pp->children != NULL)
    {
    ty->children = pp->children->ty;
    pp = pp->children;
    while (pp->next != NULL)
	{
	if (pp->ty == NULL)
	    errAt(pp->tok, "void value in tuple");
	pp->ty->next = pp->next->ty;
	pp = pp->next;
	}
    }
return ty;
}

void pfTypeOnTuple(struct pfCompile *pfc, struct pfParse *pp)
/* Create tuple type and link in types of all children. */
{
pp->ty = typeFromChildren(pfc, pp, pfc->tupleType);
pp->ty->isTuple = TRUE;
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

boolean pfTypesAllSame(struct pfType *aList, struct pfType *bList)
/* Return TRUE if all elements of aList and bList have the same
 * type, and aList and bList have same number of elements. */
{
struct pfType *a = aList, *b = bList;

for (;;)
    {
    if (a == NULL)
        return b == NULL;
    else if (b == NULL)
        return FALSE;
    else if (!pfTypeSame(a, b))
        return FALSE;
    a = a->next;
    b = b->next;
    }
}

struct pfType *wrapTupleType(struct pfCompile *pfc, struct pfType *typeList)
/* Wrap tuple around a list of types. */
{
struct pfType *ty  = pfTypeNew(pfc->tupleType);
ty->children = typeList;
return ty;
}

static void coerceCallToTupleOfTypes(struct pfCompile *pfc,
	struct pfParse **pCall, struct pfType *fieldTypes)
/* Given a function call return tuple and some types
 * that it is supposed to be decide whether
 *      1) The types match
 *      2) Types mismatch but can be fixed  by
 *         inserting a  pptCastCallToTuple
 *      3) Types mismatch in a way that can't be fixed
 * and act appropriately. */
{
struct pfParse *call = *pCall;
struct pfType *retTypes = call->ty->children;
int retCount = slCount(retTypes);
int fieldCount = slCount(fieldTypes);
int i;

if (retCount != fieldCount)
    errAt(call->tok, "%s returns %d values, but need %d values here",
    	  call->name, retCount, fieldCount);
if (!pfTypesAllSame(retTypes, fieldTypes))
    {
    struct pfType *ty = wrapTupleType(pfc, fieldTypes);
    struct pfParse *castAll = insertCast(pptCastCallToTuple, ty, pCall);
    struct pfType *retType = retTypes, *fieldType = fieldTypes;
    struct pfParse *castList = NULL, *fake;

    for (i=0; i<fieldCount; ++i)
        {
	AllocVar(fake);
	fake->type = pptPlaceholder;
	fake->name = "placeholder";
	fake->ty = retType;
	coerceOne(pfc, &fake, fieldType);
	slAddHead(&castList, fake);
	retType = retType->next;
	fieldType = fieldType->next;
	}
    slReverse(&castList);
    castAll->children->next = castList;
    }
}

static void coerceCallToClass(struct pfCompile *pfc, 
	struct pfParse **pPp, struct pfType *type)
/* Given a type that is a class, and a parse tree that
 * is a tuple, do any casting required inside the tuple
 * to get the members of the tuple to be of the same type
 * as the corresponding members of the class. */
{
coerceCallToTupleOfTypes(pfc, pPp, type->base->fields);
}


static void coerceTuple(struct pfCompile *pfc, struct pfParse **pTuple,
	struct pfType *types)
/* Make sure that tuple is of correct type. */
{
struct pfParse *tuple = *pTuple;
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
if (tuple->type == pptCall)
    {
    coerceCallToTupleOfTypes(pfc, pTuple, types->children);
    }
else
    {
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
}

static void coerceCall(struct pfCompile *pfc, struct pfParse **pPp)
/* Make sure that parameters to call are right.  Then
 * set pp->type to call's return type. */
{
struct pfParse *pp = *pPp;
struct pfParse *function = pp->children;
if (function->ty->isFunction)
    {
    struct pfParse **paramTuple = &function->next;
    struct pfType *functionType = function->ty;
    struct pfType *inputType = functionType->children;
    struct pfType *outputType = inputType->next;

    coerceTuple(pfc, paramTuple, inputType);
    if (outputType->children != NULL && outputType->children->next == NULL)
	pp->ty = CloneVar(outputType->children);
    else
	pp->ty = CloneVar(outputType);
    pp->name = function->name;
    }
else
    errAt(function->tok, "Call to non-function");
}

static void coerceTupleToCollection(struct pfCompile *pfc, 
	struct pfParse **pPp, struct pfType *type)
/* Given a type that is a collection, and a parse tree that
 * is a tuple, do any casting required inside the tuple
 * to get the members of the tuple to be of the same type
 * as the collection elements.  */
{
struct pfParse *tuple = *pPp;
struct pfType *elType;
struct pfParse **pos;
if (type->base->keyedBy)
     {
     struct pfType *key = pfTypeNew(type->base->keyedBy);
     struct pfType *val = type->children;
     elType = pfTypeNew(pfc->keyValType);
     elType->children = key;
     key->next = val;
     }
else
     {
     elType = type->children;
     }
for (pos = &tuple->children; *pos != NULL; pos = &(*pos)->next)
     {
     coerceOne(pfc, pos, elType);
     }
pfTypeOnTuple(pfc, tuple);
tuple->type = pptUniformTuple;
}

static void coerceTupleToClass(struct pfCompile *pfc, 
	struct pfParse **pPp, struct pfType *type)
/* Given a type that is a class, and a parse tree that
 * is a tuple, do any casting required inside the tuple
 * to get the members of the tuple to be of the same type
 * as the corresponding members of the class. */
{
struct pfParse *tuple = *pPp;
struct pfType *field;
struct pfParse **pos;

pos = &tuple->children;
for (field = type->base->fields; field != NULL; field = field->next)
    {
    if (*pos == NULL)
	break;
    coerceOne(pfc, pos, field);
    pos = &(*pos)->next;
    }
pfTypeOnTuple(pfc, tuple);
}

static void coerceOne(struct pfCompile *pfc, struct pfParse **pPp,
	struct pfType *type)
/* Make sure that a single variable is of the required type.  
 * Add casts if necessary */
{
struct pfParse *pp = *pPp;
struct pfType *pt = pp->ty;
struct pfBaseType *base;
if (pt == NULL)
    internalErrAt(pp->tok);
base = type->base;
// uglyf("coercingOne %s (isFunction=%d) to %s\n", pt->base->name, pt->isFunction, base->name);
if (pp->type == pptConstUse)
    {
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
	pp->type = pftFromTokType(pp->tok);
	}
    else if (base->isCollection)
	{
	coerceOne(pfc, pPp, type->children);
	insertCast(pptUniformTuple, NULL, pPp);
	pfTypeOnTuple(pfc, *pPp);
	return;
	}
    else if (base->isClass )
        {
	if (slCount(base->fields) != 1)
	    errAt(pp->tok, "Missing fields in object initialization");
	coerceOne(pfc, pPp, base->fields);
	insertCast(pptTuple, NULL, pPp);
	pfTypeOnTuple(pfc, *pPp);
	}
    else
	{
	errAt(pp->tok, "Misplaced constant");
	}
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
    if (base == pfc->varType)
	{
	struct pfType *tt = pfTypeNew(pfc->varType);
	insertCast(pptCastTypedToVar, tt, pPp);
	pp = *pPp;
	}
    pp->ty = pfTypeNew(base);
    }
else if (pt->base != base)
    {
    boolean ok = FALSE;
    // uglyf("coercing from %s (%s)  to %s\n", pt->base->name, (pt->base->isCollection ? "collection" : "single"), base->name);
    if (base == pfc->bitType && pt->base == pfc->stringType)
	{
	struct pfType *tt = pfTypeNew(pfc->bitType);
	insertCast(pptCastStringToBit, tt, pPp);
	ok = TRUE;
	}
    else if (base == pfc->varType)
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
    else if (base->isCollection)
	{
	if (!pt->isTuple)
	    {
	    insertCast(pptTuple, NULL, pPp);  /* In this case not just a cast. */
	    pfTypeOnTuple(pfc, *pPp);
	    }
	coerceTupleToCollection(pfc, pPp, type);
	ok = TRUE;
	}
    else if (base->isClass)
        {
	if (!pt->isTuple)
	    {
	    insertCast(pptTuple, NULL, pPp);  /* Also not just a cast. */
	    pfTypeOnTuple(pfc, *pPp);
	    }
	if (pp->type == pptCall)
	    coerceCallToClass(pfc, pPp, type);
	else
	    coerceTupleToClass(pfc, pPp, type);
	ok = TRUE;
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
	if (base->parent == pfc->numType && pt->base->parent == pfc->numType)
	    {
	    numericCast(pfc, type, pPp);
	    ok = TRUE;
	    }
	}
    if (!ok)
	{
	typeMismatch(pp, type);
	}
    }
else if (base == pfc->keyValType)
    {
    coerceOne(pfc, &pp->children, type->children);
    coerceOne(pfc, &pp->children->next, type->children->next);
    }
else if (type->isTuple)
    {
    assert(pt->isTuple);
    coerceTuple(pfc, pPp, type);
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

static void checkForeach(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure have agreement between element and collection vars */
{
struct pfParse *el = pp->children;
struct pfParse *collection = el->next;
if (!collection->ty->base->isCollection)
    expectingGot("collection", collection->tok);
if (!pfTypeSame(el->ty, collection->ty->children))
    errAt(pp->tok, "type mismatch between element and collection in foreach");
}

struct pfType *coerceLval(struct pfCompile *pfc, struct pfParse *pp)
/* Ensure that pp can be assigned.  Return it's type */
{
switch (pp->type)
    {
    case pptVarInit:
    case pptVarUse:
    case pptDot:
    case pptIndex:
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

static void coerceAssign(struct pfCompile *pfc, struct pfParse *pp, 
	boolean numOnly)
/* Make sure that left half of assigment is a valid l-value,
 * and that right half of assignment can be coerced into a
 * compatible type.  Set pp->type to l-value type. */
{
struct pfParse *lval = pp->children;
struct pfType *destType = coerceLval(pfc, lval);
if (numOnly)
    {
    if (destType->base->parent != pfc->numType)
        expectingGot("numerical variable to left of assignment", lval->tok);
    }
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

static void coerceIndex(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure that [] is after a collection type, and that
 * what's inside the [] agrees with the key type of the collection. */
{
struct pfParse *collectionPp = pp->children;
struct pfParse *indexPp = collectionPp->next;
struct pfType *collectionType = collectionPp->ty;
struct pfBaseType *collectionBase = collectionType->base;
struct pfBaseType *keyBase;

if (collectionBase == pfc->arrayType)
    keyBase = pfc->intType;
else
    {
    keyBase = collectionBase->keyedBy;
    if (keyBase == NULL)
	errAt(pp->tok, "trying to index a %s", collectionBase->name);
    }
if (indexPp->ty->base != keyBase)
    {
    struct pfType *ty = pfTypeNew(keyBase);
    coerceOne(pfc, &collectionPp->next, ty);
    }
pp->ty = collectionType->children;
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

static void coerceBinaryLogicOp(struct pfCompile *pfc, struct pfParse *pp)
/* Coerce both sides of binary logic operation to bit, and make
 * output bit as well. */
{
coerceToBit(pfc, &pp->children);
coerceToBit(pfc, &pp->children->next);
pp->ty = pp->children->ty;
}

static void ensureDeclarationTuple(struct pfParse *tuple)
/* Given tuple, ensure every child is type pptVarInit. */
{
struct pfParse *pp;
for (pp = tuple->children; pp != NULL; pp = pp->next)
    {
    if (pp->type != pptVarInit)
	expectingGot("variable declaration", pp->tok);
    }
}

static void addVarToClass(struct pfBaseType *class, struct pfParse *varPp)
/* Add variable to class. */
{
struct pfParse *typePp = varPp->children;
struct pfParse *namePp = typePp->next;
struct pfParse *initPp = namePp->next;
struct pfType *type = CloneVar(varPp->ty);
type->fieldName = varPp->name;
type->init = initPp;
slAddHead(&class->fields, type);
}

static void addFunctionToClass(struct pfBaseType *class, struct pfParse *funcPp)
/* Add variable to class. */
{
struct pfType *type = CloneVar(funcPp->ty);
type->fieldName = funcPp->name;
slAddHead(&class->methods, type);
}


static void blessClass(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure that there are only variable , class declarations and
 * function declarations in class.  Flatten out nested declarative
 * tuples. */
{
struct pfParse *compound = pp->children->next;
struct pfParse *p, **p2p;
struct pfBaseType *base = pfScopeFindType(pp->scope, pp->name);
struct pfType *ty;

if (base == NULL)
    internalErrAt(pp->tok);
p2p = &compound->children;
for (p = compound->children; p != NULL; p = p->next)
    {
    switch (p->type)
        {
	case pptNop:
	case pptClass:
	case pptVarInit:
	    addVarToClass(base, p);
	    break;
	case pptToDec:
	case pptParaDec:
	case pptFlowDec:
	    addFunctionToClass(base, p);
	    break;
	case pptTuple:
	    {
	    struct pfParse *child = p->children;
	    if (child == NULL || child->type != pptVarInit)
	        errAt(p->tok, "non-declaration tuple inside of class");
	    ensureDeclarationTuple(p);
	    for ( ; child != NULL; child = child->next)
	        addVarToClass(base, child);
	    pfParsePutChildrenInPlaceOfSelf(p2p);
	    break;
	    }
	default:
	    errAt(p->tok, "non-declaration statement inside of class");
	}
    p2p = &p->next;
    }
slReverse(&base->methods);
slReverse(&base->fields);
// uglyf("Got %d methods, %d fields in %s\n", slCount(base->methods), slCount(base->fields), base->name);
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

static struct pfType *findField(struct pfType *class, char *name)
/* Find named field in class or return NULL */
{
struct pfType *field;
for (field = class->base->methods; field != NULL; field = field->next)
    {
    if (sameString(name, field->fieldName))
        return field;
    }
for (field = class->base->fields; field != NULL; field = field->next)
    {
    if (sameString(name, field->fieldName))
        return field;
    }
return NULL;
}

static void typeDot(struct pfCompile *pfc, struct pfParse *pp)
/* Create type for dotted set of symbols. */
{
struct pfParse *varUse = pp->children;
struct pfParse *fieldUse;
struct pfVar *var = varUse->var;
struct pfType *type = var->ty;

if (!type->base->isClass)
    errAt(pp->tok, "dot after non-class variable");
    
for (fieldUse = varUse->next; fieldUse != NULL; fieldUse = fieldUse->next)
    {
    struct pfType *fieldType = findField(type, fieldUse->name);
    if (fieldType == NULL)
        errAt(pp->tok, "No field %s in class %s", fieldUse->name, 
		type->base->name);
    type = fieldType;
    }
pp->ty = CloneVar(type);
}

static void markUsedVars(struct pfScope *scope, 
	struct hash *hash, struct pfParse *pp)
/* Mark any pptVarUse inside scope as 1's in hash */
{
switch (pp->type)
    {
    case pptVarUse:
	{
	if (pp->var->scope == scope)
	    {
	    struct hashEl *hel = hashLookup(hash, pp->name);
	    if (hel != NULL)
		 hel->val = intToPt(1);
	    }
        break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    markUsedVars(scope, hash, pp);
}

static void rBlessFunction(struct pfScope *outputScope,
	struct hash *outputHash, struct pfCompile *pfc, struct pfParse *pp)
/* Avoid functions declared in functions, and mark outputs as used.  */
{
switch (pp->type)
    {
    case pptToDec:
    case pptParaDec:
    case pptFlowDec:
        errAt(pp->tok, "sorry, can't declare functions inside of functions");
	break;
    case pptAssignment:
        markUsedVars(outputScope, outputHash, pp);
	break;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rBlessFunction(outputScope, outputHash, pfc, pp);
}

static void checkIsSimpleDecTuple(struct pfParse *tuple)
/* Make sure tuple contains only pptVarInits. */
{
struct pfParse *pp;
for (pp = tuple->children; pp != NULL; pp = pp->next)
    {
    if (pp->type != pptVarInit)
        errAt(pp->tok, "only variable declarations allowed in input/output lists");
    pp->ty->fieldName = pp->name;
    }
}

void checkInLoop(struct pfCompile *pfc, struct pfParse *pp)
/* Check that break/continue lies inside of loop */
{
struct pfParse *p;
for (p = pp->parent; p != NULL; p = p->parent)
    {
    switch (p->type)
        {
	case pptWhile:
	case pptFor:
	case pptForeach:
	    return;	/* We're good. */
	}
    }
errAt(pp->tok, "break or continue outside of loop");
}

void checkInFunction(struct pfCompile *pfc, struct pfParse *pp)
/* Check that return lies inside of function */
{
struct pfParse *p;
for (p = pp->parent; p != NULL; p = p->parent)
    {
    switch (p->type)
        {
	case pptToDec:
	case pptParaDec:
	case pptFlowDec:
	    return;	/* We're good. */
	}
    }
errAt(pp->tok, "return outside of function");
}

static void blessFunction(struct pfCompile *pfc, struct pfParse *funcDec)
/* Make sure that function looks kosher - that all inputs are
 * covered, and that there are no functions inside functions. */
{
struct hash *outputHash = hashNew(4);
struct pfParse *input = funcDec->children->next;
struct pfParse *output = input->next;
struct pfParse *pp;
struct hashCookie hc;
struct hashEl *hel;

checkIsSimpleDecTuple(input);
checkIsSimpleDecTuple(output);
for (pp = output->children; pp != NULL; pp = pp->next)
    {
    hashAddInt(outputHash, pp->name, 0);
    }
for (pp = funcDec->children; pp != NULL; pp = pp->next)
    rBlessFunction(funcDec->scope, outputHash, pfc, pp);
hc = hashFirst(outputHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    if (hel->val == intToPt(0))
        errAt(funcDec->tok, "Output variable %s not set in %s", 
		hel->name, funcDec->name);
    }
hashFree(&outputHash);
}

void pfTypeCheck(struct pfCompile *pfc, struct pfParse **pPp)
/* Check types (adding conversions where needed) on tree,
 * which should have variables bound already. */
{
struct pfParse *pp = *pPp;
struct pfParse **pos;

for (pos = &pp->children; *pos != NULL; pos = &(*pos)->next)
    pfTypeCheck(pfc, pos);

switch (pp->type)
    {
    case pptCall:
	coerceCall(pfc, pPp);
        break;
    case pptWhile:
        coerceWhile(pfc, pp);
	break;
    case pptFor:
        coerceFor(pfc, pp);
	break;
    case pptForeach:
        checkForeach(pfc, pp);
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
    case pptMinus:
        coerceBinaryOp(pfc, pp, TRUE, FALSE);
	break;
    case pptMod:
        coerceBinaryOp(pfc, pp, FALSE, FALSE);
	break;
    case pptBitAnd:
    case pptBitOr:
    case pptBitXor:
    case pptShiftLeft:
    case pptShiftRight:
        coerceBinaryOp(pfc, pp, FALSE, FALSE);
	break;
    case pptLogAnd:
    case pptLogOr:
        coerceBinaryLogicOp(pfc, pp);
	break;
    case pptNegate:
	enforceNumber(pfc, pp->children);
	pp->ty = pp->children->ty;
	break;
    case pptNot:
        coerceToBit(pfc, &pp->children);
	pp->ty = pp->children->ty;
	break;
    case pptFlipBits:
	enforceInt(pfc, pp->children);
	pp->ty = pp->children->ty;
        break;
    case pptAssignment:
        coerceAssign(pfc, pp, FALSE);
	break;
    case pptPlusEquals:
    case pptMinusEquals:
    case pptMulEquals:
    case pptDivEquals:
        coerceAssign(pfc, pp, TRUE);
	break;
    case pptVarInit:
        coerceVarInit(pfc, pp);
	break;
    case pptIndex:
        coerceIndex(pfc, pp);
	break;
    case pptTuple:
	pfTypeOnTuple(pfc, pp);
	break;
    case pptKeyVal:
         pp->ty = typeFromChildren(pfc, pp, pfc->keyValType);
	 break;
    case pptConstUse:
        typeConstant(pfc, pp);
	break;
    case pptDot:
	typeDot(pfc,pp);
        break;
    case pptClass:
        blessClass(pfc, pp);
	break;
    case pptParaDec:
    case pptToDec:
    case pptFlowDec:
        blessFunction(pfc, pp);
	break;
    case pptBreak:
    case pptContinue:
        checkInLoop(pfc, pp);
	break;
    case pptReturn:
        checkInFunction(pfc, pp);
	break;
    }
}
