/* pfType - ParaFlow type heirarchy and type checking. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "pfParse.h"
#include "pfCompile.h"
#include "pfType.h"


void rTypeCheck(struct pfCompile *pfc, struct pfParse **pPp);
/* Check types (adding conversions where needed) on tree,
 * which should have variables bound already. */

static void coerceOne(struct pfCompile *pfc, struct pfParse **pPp,
	struct pfType *type, boolean numToString);
/* Make sure that a single variable is of the required type. 
 * Add casts if necessary */

static int baseTypeCount = 0;

struct pfBaseType *pfBaseTypeNew(struct pfScope *scope, char *name, 
	boolean isCollection, struct pfBaseType *parent, int size,
	boolean needsCleanup)
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
base->size = size;
base->needsCleanup = needsCleanup;
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
else
    {
    if (ty->isStatic)
        fprintf(f, "static ");
    if (ty->tyty == tytyTuple)
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
    else if (ty->tyty == tytyFunction || ty->tyty == tytyVirtualFunction
       || ty->tyty == tytyOperator)
	{
	if (ty->tyty == tytyVirtualFunction)
	   fprintf(f, "polymorphic ");
	fprintf(f, "%s ", ty->base->name);
	pfTypeDump(ty->children, f);
	fprintf(f, " into ");
	pfTypeDump(ty->children->next, f);
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
}

static void typeMismatch(struct pfParse *pp, struct pfType *type)
/* Complain about type mismatch at node. */
{
if (pp->name)
    errAt(pp->tok, "Type mismatch:  %s not %s.", pp->name, type->base->name);
else
    errAt(pp->tok, "Type mismatch: expecting %s.", type->base->name);
}

static enum pfParseType pptFromTokType(struct pfToken *tok)
/* Return pptConstXXX corresponding to tok->type */
{
switch (tok->type)
    {
    case pftInt:
	return pptConstInt;
    case pftLong:
	return pptConstLong;
    case pftFloat:
	return pptConstDouble;
    case pftString:
	return pptConstString;
    case pftNil:
        return pptConstZero;
    default:
	internalErrAt(tok);
	return 0;
    }
}

static boolean isNumerical(struct pfCompile *pfc, struct pfParse *pp)
/* Return TRUE if numerical (or var) type. */
{
return pp->ty->base == pfc->varType || pp->ty->base->parent == pfc->numType;
}

static boolean isNumOrString(struct pfCompile *pfc, struct pfParse *pp)
/* Return TRUE if numerical or string type. */
{
return pp->ty->base == pfc->stringType || isNumerical(pfc, pp);
}

static void enforceNumber(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure type of pp is numberic. */
{
if (!isNumerical(pfc, pp))
    expectingGot("number", pp->tok);
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
else if (base == pfc->stringType)
    return 7;
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
if (newType != NULL)
    cast->ty = CloneVar(newType);
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
int numTypeCount = 8;
enum pfParseType castType = pptCastBitToBit;
if (oldBase == pfc->stringType && newBase != pfc->stringType)
    expectingGot("string", pp->tok);
castType += numTypeCount * baseTypeLogicalSize(pfc, oldBase);
castType += baseTypeLogicalSize(pfc, newBase);
insertCast(castType, newType, pPp);
}

static struct pfType *typeFromChildren(struct pfCompile *pfc, 
	struct pfParse *pp, struct pfBaseType *base)
/* Create a type that is just a wrapper around children's type. */
{
struct pfType *ty = pfTypeNew(base);
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    if (pp->ty == NULL)
        errAt(pp->tok, "Expecting type");
    else
	{
	struct pfType *subType = CloneVar(pp->ty);
	subType->fieldName = pp->name;
	slAddHead(&ty->children, subType);
	}
    }
slReverse(&ty->children);
return ty;
}

void pfTypeOnTuple(struct pfCompile *pfc, struct pfParse *pp)
/* Create tuple type and link in types of all children. */
{
pp->ty = typeFromChildren(pfc, pp, pfc->tupleType);
pp->ty->tyty = tytyTuple;
}

static void coerceToBaseType(struct pfCompile *pfc, struct pfBaseType *baseType,
	struct pfParse **pPp)
/* Coerce to a particular base type */
{
struct pfParse *pp = *pPp;
if (pp->ty->base != baseType)
    {
    struct pfType *type = pfTypeNew(baseType);
    coerceOne(pfc, pPp, type, FALSE);
    }
}

static void coerceToBit(struct pfCompile *pfc, struct pfParse **pPp)
/* Make sure that pPp is a bit. */
{
coerceToBaseType(pfc, pfc->bitType, pPp);
}

static void coerceToInt(struct pfCompile *pfc, struct pfParse **pPp)
/* Make sure that pPp is a bit. */
{
coerceToBaseType(pfc, pfc->intType, pPp);
}

static void coerceToLong(struct pfCompile *pfc, struct pfParse **pPp)
/* Make sure type of pp is long integer. */
{
coerceToBaseType(pfc, pfc->longType, pPp);
}

static void coerceToDouble(struct pfCompile *pfc, struct pfParse **pPp)
/* Make sure type of pp is double. */
{
coerceToBaseType(pfc, pfc->doubleType, pPp);
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
	fake->name = "result";
	fake->ty = CloneVar(retType);
	fake->tok = call->tok;
	coerceOne(pfc, &fake, fieldType, FALSE);
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
int tupSize = slCount(tuple->ty->children);
int typeSize = slCount(types->children);
struct pfParse **pos;
struct pfType *type;
if (tupSize > typeSize)
    {
    errAt(tuple->tok, "Parenthesized list too long: expecting %d got %d",
    	typeSize, tupSize);
    }
if (typeSize == 0)
    return;
if (tuple->type == pptCall)
    {
    if (tupSize != typeSize)
	{
	errAt(tuple->tok, "Expecting tuple of %d, got tuple of %d", 
	    typeSize, tupSize);
	}
    coerceCallToTupleOfTypes(pfc, pTuple, types->children);
    }
else
    {
    pos = &tuple->children;
    type = types->children;
    for (;;)
	 {
	 if ( *pos == NULL)
	     {
	     if (type->init == NULL)
	         errAt(tuple->tok, "Need additional elements in list");
	     else
	         {
		 /* Fill in default-value for missing parameter. */
		 *pos = CloneVar(type->init);
		 }
	     }
	 else
	     coerceOne(pfc, pos, type, FALSE);
	 pos = &(*pos)->next;
	 type = type->next;
	 if (type == NULL)
	     break;
	 }
    tuple->ty = CloneVar(types);
    }
}

static void coerceArrayAppend(struct pfCompile *pfc,
	struct pfParse *pp, struct pfParse *arrayTypePp)
/* Turn pptCall subtree into a pptArrayAppend subtree.
 * The structure of this subtree is just:
 *     pptArrayAppend
 *        array expression
 *        element expression
 * The input subtree is
 *     pptCall
 *        pptDot
 *           dot-element
 *        pptTuple
 *           element-list
 * Also check that we are adding something that can
 * get turned into type of array elements. */
{
struct pfType *arrayType = arrayTypePp->ty;
struct pfParse *inDot = pp->children;
struct pfParse *inTuple = inDot->next;
struct pfParse *outArray = NULL, *outElement = NULL;
struct pfType *elType = arrayType->children;
int dotCount;
outElement = inTuple->children;
if (outElement->next != NULL)
    errAt(inTuple->tok, "array append only takes a single argument");

/* Force element to be same type as elements of array,
 * inserting casts if necessary. */
coerceOne(pfc, &outElement, elType, FALSE);

/* Get rid of the 'append' field, which happens to be right
 * after the arrayType field, and count up remaining dots. */
arrayTypePp->next = NULL;
dotCount = slCount(inDot->children);

if (dotCount == 1)
    {
    /* In this case we can get rid of the pptDot, and just
     * replace it with the first child. */
    outArray = inDot->children;
    }
else
    {
    outArray = inDot;
    }

/* Now rework the base of the subtree, changing it's type, ty,
 * and children. */
pp->type = pptArrayAppend;
pp->children = outArray;
outArray->next = outElement;
}

static void coerceCallToOperator(struct pfCompile *pfc, struct pfParse *pp)
/* Check and make type casts for operators, which are
 * built-in function with type-checking not easily handled
 * by the usual methods. 
 *    At some point we'll probably want to generalize the code here.
 * At the moment the only operator is array.append(). */
{
struct pfParse *function = pp->children;
struct pfParse *input = function->next;
if (function->type == pptDot)
    {
    /* It's a method of some sort. */
    struct pfParse *typePp, *fieldPp;
    char *operatorName;

    /* Seek to penultimate field, which contains the type
     * we are applying stuff to. */
    for (typePp=function->children; typePp->next->next != NULL; 
    	typePp = typePp->next)
	;

    /* Get field. */
    fieldPp = typePp->next;
    assert(fieldPp->type == pptFieldUse);
    operatorName = fieldPp->name;

    if (typePp->ty->base == pfc->arrayType)
        {
	if (sameString(operatorName, "append"))
	    coerceArrayAppend(pfc, pp, typePp);
	}
    else
        {
	internalErr();
	}
    }
else 
    {
    internalErr();
    }
}

static void coerceCall(struct pfCompile *pfc, struct pfParse **pPp)
/* Make sure that parameters to call are right.  Then
 * set pp->type to call's return type. */
{
struct pfParse *pp = *pPp;
struct pfParse *function = pp->children;
switch(function->ty->tyty)
    {
    case tytyOperator:
        {
	coerceCallToOperator(pfc, pp);
	break;
	}
    case tytyFunction:
    case tytyVirtualFunction:
	{
	struct pfType *functionType = function->ty;
	struct pfType *inputType = functionType->children;
	struct pfType *outputType = inputType->next;
	struct pfParse *paramTuple = function->next;
	struct pfParse *firstParam = paramTuple->children;

	if (firstParam != NULL && firstParam->type == pptCall && firstParam->next == NULL && slCount(inputType->children) != 1)
	    {
	    coerceCallToTupleOfTypes(pfc, &paramTuple->children, inputType->children);
	    }
	else
	    {
	    struct pfParse **pParamTuple = &function->next;
	    coerceTuple(pfc, pParamTuple, inputType);
	    }
	if (outputType->children != NULL && outputType->children->next == NULL)
	    pp->ty = CloneVar(outputType->children);
	else
	    pp->ty = CloneVar(outputType);
	pp->name = function->name;
	break;
	}
    default:
	errAt(function->tok, "Call to non-function");
	break;
    }
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
     coerceOne(pfc, pos, elType, FALSE);
     }
pfTypeOnTuple(pfc, tuple);
tuple->type = pptUniformTuple;
}

static struct pfParse **rCoerceTupleToClass(struct pfCompile *pfc,
	struct pfParse *parent, struct pfParse **pos, 
	struct pfBaseType *base, boolean fillMissingWithZero)
/* Recursively coerce tuple - first on parent class and
 * then on self. */
{
struct pfType *field;
struct pfToken *firstTok = parent->tok;
if (base->parent != NULL)
    pos = rCoerceTupleToClass(pfc, parent, pos, base->parent, 
    			      fillMissingWithZero);
for (field = base->fields; field != NULL; field = field->next)
    {
    if (*pos == NULL)
	{
	if (field->init != NULL)
	    {
	    if (field->init->ty == NULL)	/* Handle forward use */
		rTypeCheck(pfc, &field->init);
	    *pos = CloneVar(field->init);
	    coerceOne(pfc, pos, field, FALSE);
	    }
	else if (fillMissingWithZero)
	    {
	    struct pfParse *fill = pfParseNew(pptConstZero, firstTok, 
	    		      parent, parent->scope);
	    fill->ty = CloneVar(field);
	    *pos = fill;
	    }
	else
	    errAt(firstTok, "Not enough fields in initialization");
	}
    else
	coerceOne(pfc, pos, field, FALSE);
    pos = &(*pos)->next;
    }
return pos;
}

static void coerceTupleToClass(struct pfCompile *pfc, 
	struct pfParse **pPp, struct pfBaseType *base)
/* Given a type that is a class, and a parse tree that
 * is a tuple, do any casting required inside the tuple
 * to get the members of the tuple to be of the same type
 * as the corresponding members of the class. */
{
struct pfParse *tuple = *pPp;
struct pfParse **pLeftover;
boolean fillMissingWithZero = (tuple->children == NULL);
pLeftover = rCoerceTupleToClass(pfc, tuple, &tuple->children, 
	base, fillMissingWithZero);
if (*pLeftover != NULL)
    errAt(tuple->tok, "Type mismatch");
pfTypeOnTuple(pfc, tuple);
}

static void castNumToString(struct pfCompile *pfc, struct pfParse **pPp, 
	struct pfType *stringType)
/* Make sure that pp is numeric.  Then cast it to a string. */
{
struct pfParse *pp = *pPp;
struct pfType *type = pp->ty;
if (type->base->parent != pfc->numType)
    errAt(pp->tok, "Adding string and something strange.");
numericCast(pfc, stringType, pPp);
}

static void foldLocalMethodsIntoHash(struct hash *hash, struct pfParse *pp)
/* Assuming parse tree is a class declaration, fold all methods declared in it
 * into hash.  Doesn't handle methods in parent class. */
{
switch (pp->type)
    {
    case pptToDec:
    case pptFlowDec:
	hashAdd(hash, pp->name, pp);
        break;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    foldLocalMethodsIntoHash(hash, pp);
}

static void foldMethodsIntoHash(struct hash *hash, struct pfBaseType *base)
/* Fold methods of parents and self into hash. */
{
if (base->parent != NULL && base->parent->def != NULL)
    foldMethodsIntoHash(hash, base->parent);
foldLocalMethodsIntoHash(hash, base->def);
}

static void checkLocalMethodsAreInHash(struct hash *hash, struct pfParse *pp,
    char *className, char *interfaceName, struct pfToken *tok)
/* Assuming parse tree is a interface declaration, make sure all methods 
 * declared in it are in hash.  Doesn't handle methods in parent interface. */
{
switch (pp->type)
    {
    case pptToDec:
    case pptFlowDec:
	{
	struct pfParse *methodDef = hashFindVal(hash, pp->name);
	if (methodDef == NULL)
	    errAt(tok, "class %s doesn't implement %s method", 
	    	className, pp->name);
	if (!pfTypeSame(pp->ty, methodDef->ty))
	    errAt(tok, 
	    	"method %s is not the same in class %s as in interface %s",
	    	pp->name, className, interfaceName);
        break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    checkLocalMethodsAreInHash(hash, pp, className, interfaceName, tok);
}

static void checkMethodsAreInHash(struct hash *hash, struct pfBaseType *base, 
	char *className, struct pfToken *tok)
/* Check methods of parents and self are in hash. */
{
if (base->parent != NULL && base->parent->def != NULL)
    checkMethodsAreInHash(hash, base->parent, className, tok);
checkLocalMethodsAreInHash(hash, base->def, className, base->name, tok);
}

static void checkClassHasInterfaceMethods(struct pfCompile *pfc, struct pfToken *tok,
	struct pfBaseType *classBase, struct pfBaseType *interfaceBase)
/* Make sure that class implements all methods of interface, and that they are the
 * same type. */
{
struct hash *classMethodsHash = hashNew(8);
struct pfParse *methodDef;
foldMethodsIntoHash(classMethodsHash, classBase);
checkMethodsAreInHash(classMethodsHash, interfaceBase, classBase->name, tok);
hashFree(&classMethodsHash);
}

static void coerceClassToInterface(struct pfCompile *pfc, struct pfParse **pPp,
	struct pfBaseType *classBase, struct pfBaseType *interfaceBase)
/* Make sure class and interface are compatable, and then generate cast to interface. */
{
/* Looks like we're ok, go ahead and put int cast. */
checkClassHasInterfaceMethods(pfc, (*pPp)->tok, classBase, interfaceBase);
insertCast(pptCastClassToInterface, pfTypeNew(interfaceBase), pPp);
}

static void coerceOne(struct pfCompile *pfc, struct pfParse **pPp,
	struct pfType *destType, boolean numToString)
/* Make sure that a single variable is of the required type.  
 * Add casts if necessary.  This is a gnarly function! */
{
struct pfParse *pp = *pPp;
struct pfBaseType *destBase = destType->base;
struct pfType *pt = pp->ty;
if (pt == NULL)
    internalErrAt(pp->tok);
verbose(3, "coercingOne %s (tyty=%d) to %s\n", pt->base->name, pt->tyty, 
	destBase->name);
if (pt->base != destBase)
    {
    boolean ok = FALSE;
    verbose(3, "coercing from %s (%s)  to %s\n", pt->base->name, 
    	(pt->base->isCollection ? "collection" : "single"), destBase->name);
    if (pt->base == pfc->nilType)
        {
	pp->ty = CloneVar(destType);
	ok = TRUE;
	}
    else if (destBase == pfc->bitType && pt->base == pfc->stringType)
	{
	struct pfType *tt = pfTypeNew(pfc->bitType);
	insertCast(pptCastStringToBit, tt, pPp);
	ok = TRUE;
	}
    else if (destBase == pfc->bitType && pt->base->needsCleanup)
	{
	struct pfType *tt = pfTypeNew(pfc->bitType);
	if (pt->base == pfc->varType)
	    insertCast(pptCastVarToTyped, tt, pPp);
	else
	    insertCast(pptCastObjectToBit, tt, pPp);
	ok = TRUE;
	}
    else if (numToString && destBase == pfc->stringType && pt->base->parent == pfc->numType)
        {
	castNumToString(pfc, pPp, destType);
	ok = TRUE;
	}
    else if (destBase == pfc->varType)
	{
	struct pfType *tt = pfTypeNew(pfc->varType);
	insertCast(pptCastTypedToVar, tt, pPp);
	ok = TRUE;
	}
    else if (pt->base == pfc->varType)
	{
	struct pfType *tt = CloneVar(destType);
	insertCast(pptCastVarToTyped, tt, pPp);
	ok = TRUE;
	}
    else if (destBase->isCollection && destBase != pfc->stringType)
	{
	if (pt->tyty != tytyTuple)
	    {
	    insertCast(pptTuple, NULL, pPp);  /* In this case not just a cast. */
	    pfTypeOnTuple(pfc, *pPp);
	    }
	coerceTupleToCollection(pfc, pPp, destType);
	ok = TRUE;
	}
    else if (destBase->isClass)
        {
	if (pt->base->isClass)
	    {
	    /* Check to see if we're just coercing to a parent class, which is ok. */
	    struct pfBaseType *b;
	    for (b = pt->base->parent; b != NULL; b = b->parent)
	        {
		if (b == destBase)
		    {
		    ok = TRUE;
		    break;
		    }
		}
	    }
	if (!ok)
	    {
	    if (pt->tyty != tytyTuple)
		{
		insertCast(pptTuple, NULL, pPp);  /* Also not just a cast. */
		pfTypeOnTuple(pfc, *pPp);
		}
	    if (pp->type == pptCall)
		{
		coerceCallToClass(pfc, pPp, destType);
		}
	    else
		{
		coerceTupleToClass(pfc, pPp, destBase);
		}
	    ok = TRUE;
	    }
	}
    else if (destBase->isInterface)
        {
	if (pt->base->isInterface)
	    {
	    /* Check to see if we're just coercing to a parent interface, which is ok. */
	    struct pfBaseType *b;
	    for (b = pt->base->parent; b != NULL; b = b->parent)
	        {
		if (b == destBase)
		    {
		    ok = TRUE;
		    break;
		    }
		}
	    }
	else if (pt->base->isClass)
	    {
	    coerceClassToInterface(pfc, pPp, pt->base, destBase);
	    ok = TRUE;
	    }
	}
    else if (pt->tyty == tytyTuple)
	{
	if (pt->children == NULL)
	    errAt(pp->tok, "using void value");
	else
	    errAt(pp->tok, 
		"expecting single value, got %d values", slCount(pt->children));
	}
    else
	{
	if (destBase->parent == pfc->numType && pt->base->parent == pfc->numType)
	    {
	    numericCast(pfc, destType, pPp);
	    ok = TRUE;
	    }
	}
    if (!ok)
	{
	typeMismatch(pp, destType);
	}
    }
else if (destBase == pfc->keyValType)
    {
    coerceOne(pfc, &pp->children, destType->children, FALSE);
    coerceOne(pfc, &pp->children->next, destType->children->next, FALSE);
    }
else if (destType->tyty == tytyTuple)
    {
    assert(pt->tyty == tytyTuple);
    coerceTuple(pfc, pPp, destType);
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
struct pfParse *cond = pp->children->next;
if (cond->type != pptNop)
    coerceToBit(pfc, &pp->children->next);
}

static void coerceIf(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure have a good conditional in if. */
{
coerceToBit(pfc, &pp->children);
}

static void checkElInCollection(struct pfCompile *pfc, struct pfParse *el, 
	struct pfParse *collection, char *statementName)
/* Make sure that collection is indeed a collection, and
 * that the type of el agrees with the types in the collection */
{
boolean ok = TRUE;
if (!collection->ty->base->isCollection)
    expectingGot("collection", collection->tok);
if (collection->ty->base == pfc->stringType)
    {
    if (el->ty->base != pfc->byteType)
	ok = FALSE;
    }
else if (collection->ty->base == pfc->indexRangeType)
    {
    if (el->ty->base != pfc->longType)
	ok = FALSE;
    }
else
    {
    if (!pfTypeSame(el->ty, collection->ty->children))
	ok = FALSE;
    }
if (!ok)
    errAt(collection->tok, "type mismatch between element and collection in %s", 
    		statementName);
}

static void typeElInCollection(struct pfCompile *pfc, struct pfParse **pPp)
/* Fill in type of el from type of collection.  Collection is the
 * elder (and in fact eldest) sibling of el in parse tree. 
 * Filling in the type involves changing the type of the el
 * to varInit, putting in the type and symbol children of the
 * varInit, and filling in the real type of the variable
 * associated with the element, which at the moment is filled
 * in with a dummy nilType. */
{
struct pfParse *el = *pPp;
struct pfParse *collection = el->parent->children;
struct pfParse *type = pfParseNew(pptTypeName, el->tok, el, el->scope);
struct pfParse *sym = pfParseNew(pptSymName, el->tok, el, el->scope);
struct pfVar *var = pfScopeFindVar(el->scope, el->name);
struct pfType *ty;
if (collection->ty->base == pfc->stringType)
    ty = pfTypeNew(pfc->byteType);
else if (collection->ty->base == pfc->indexRangeType)
    ty = pfTypeNew(pfc->longType);
else
    ty = collection->ty->children;
*(var->ty) = *ty;
sym->name = el->name;
type->ty = el->ty = var->ty;
el->type = pptVarInit;
el->children = type;
type->next = sym;
}

static void checkForeach(struct pfCompile *pfc, struct pfParse *pp)
/* Figure out if looping through a collection, or over
 * repeated uses of function, and type check accordingly. */
{
/* Make sure have agreement between element and collection vars */
struct pfParse *source = pp->children;
struct pfParse *el = source->next;
struct pfParse *body = el->next;
struct pfParse *cast, *castStart;

if (source->type == pptCall)
    {
    /* Coerce call to be same type as element. */
    struct pfParse **pSource = &pp->children;
    pp->type = pptForEachCall;
    coerceOne(pfc, pSource, el->ty, FALSE);

    /* Coerce element to bit, and save cast node if
     * any after body.  Handle tuples here as a special case,
     * generating cast for first element. */
    if (el->type == pptTuple)
	cast = castStart = el->children;
    else
        cast = castStart = el;
    coerceToBit(pfc, &cast);
    if (cast != castStart)
        {
	cast->children = CloneVar(castStart);
	castStart->next = cast->next;
	castStart->parent = cast->parent;
	cast->next = NULL;
	cast->parent = pp;
	body->next = cast;
	}
    }
else
    {
    checkElInCollection(pfc, el, source, "foreach");
    }
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
	boolean numOrStringOnly, boolean numOnly)
/* Make sure that left half of assigment is a valid l-value,
 * and that right half of assignment can be coerced into a
 * compatible type.  Set pp->type to l-value type. */
{
struct pfParse *lval = pp->children;
struct pfType *destType = coerceLval(pfc, lval);
if (numOrStringOnly)
    {
    boolean isNum = (destType->base->parent == pfc->numType);
    boolean isString = (destType->base == pfc->stringType);
    if (!isNum && !isString)
	expectingGot("number or string to left of assignment", lval->tok);
    if (numOnly)
	{
	if (!isNum)
	    expectingGot("numerical variable to left of assignment", lval->tok);
	}
    }
coerceOne(pfc, &lval->next, destType, FALSE);
pp->ty = CloneVar(destType);
}

static void rCheckTypeWellFormed(struct pfCompile *pfc, struct pfParse *type)
/* Make sure that if pptTypeNames have children that they are arrays. */
{
switch (type->type)
    {
    case pptTypeName:
	if (type->children != NULL)
	    {
	    if (type->ty->base != pfc->arrayType)
	        errAt(type->children->tok, "[ illegal here except for arrays");
	    coerceOne(pfc, &type->children, pfc->longFullType, FALSE);
	    }
        break;
    default:
        break;
    }
for (type = type->children; type != NULL; type = type->next)
    rCheckTypeWellFormed(pfc, type);
}

static void checkRedefinitionInParent(struct pfCompile *pfc, struct pfParse *varInit)
/* Make sure that variable is not defined as a public member of parent class. */
{
struct pfBaseType *base;
char *name = varInit->name;
struct pfParse *classDef = pfParseEnclosingClass(varInit->parent);
if (classDef != NULL)
    {
    for (base = classDef->ty->base->parent; base != NULL; base = base->parent)
	{
	struct pfType *field;
	for (field = base->fields; field != NULL; field = field->next)
	    {
	    if (sameString(name, field->fieldName))
		errAt(varInit->tok, "%s already defined in parent class %s", name, base->name);
	    }
	}
    }
}

static void coerceVarInit(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure that variable initialization can be coerced to variable
 * type. */
{
struct pfParse *type = pp->children;
struct pfParse *symbol = type->next;
struct pfParse *init = symbol->next;

if (init != NULL)
    coerceOne(pfc, &symbol->next, type->ty, FALSE);
rCheckTypeWellFormed(pfc, type);
checkRedefinitionInParent(pfc, pp);
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

if (collectionBase == pfc->arrayType || collectionBase == pfc->stringType)
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
    coerceOne(pfc, &collectionPp->next, ty, FALSE);
    }
if (collectionBase == pfc->stringType)
    pp->ty = pfTypeNew(pfc->byteType);
else
    pp->ty = CloneVar(collectionType->children);
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
	boolean floatOk, boolean stringOk, boolean numToString, boolean objOk)
/* Make sure that both sides of a math operation agree. */
{
struct pfParse **pLval = &pp->children;
struct pfParse *lval = *pLval;
struct pfParse **pRval = &lval->next;
struct pfParse *rval = *pRval;
struct pfType *lTy = lval->ty, *rTy = rval->ty;
struct pfBaseType *lBase = lTy->base, *rBase = rTy->base;

if (!pfTypeSame(lTy, rTy))
    {
    if (lBase == pfc->varType || rBase == pfc->varType)
        errAt(pp->tok, "Sorry, you can't use a var with this operation");
    if (lBase == pfc->nilType || rBase == pfc->nilType)
        {
	if (!objOk)
	    errAt(pp->tok, "No nil allowed with this operation");
	if (lBase == pfc->nilType && rBase == pfc->nilType)
	    lval->ty = rval->ty = pfTypeNew(pfc->intType);
	else if (lBase == pfc->nilType)
	    lval->ty = rval->ty;
	else
	    rval->ty = lval->ty;
	}
    else
	{
	struct pfType *ty = largerNumType(pfc, lTy, rTy);
	coerceOne(pfc, &lval, ty, numToString);
	coerceOne(pfc, &rval, ty, numToString);
	}
    }
else 
    {
    if (lBase == pfc->varType)
        errAt(pp->tok, "Sorry, you can't have var's on both side of this operation");
    }
pp->ty = lval->ty;

/* Put lval,rval (which may have changed) back as the two children
 * of pp. */
pp->children = lval;
lval->next = rval;
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

static void coerceStringCat(struct pfCompile *pfc, struct pfParse *plusOp)
/* If the type of plusOp is string, then flip it to pptStringCat, 
 * merging together any children that are also pptStringCat.  In
 * the right situation the tree:
 *   pptPlus string
 *     pptPlus string
 *       pptVarUse string
 *       pptVarUse string
 *     pptVarUse string
 * gets transformed to
 *   pptStringCat string
 *      pptVarUse string
 *      pptVarUse string
 *      pptVarUse string
 * though this will happen over two calls to this routine. */
{
if (plusOp->ty->base == pfc->stringType)
    {
    struct pfParse *left = plusOp->children;
    struct pfParse *right = left->next;
    if (left->type == pptStringCat)
	plusOp->children = slCat(left->children, right);
    plusOp->type = pptStringCat;
    }
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


static void blessClassOrInterface(struct pfCompile *pfc, struct pfParse *pp, boolean isInterface)
/* Make sure that there are only variable , class declarations and
 * function declarations in class.  Flatten out nested declarative
 * tuples.  Add definitions to class symbol table. */
{
struct pfParse *type = pp->children;
struct pfParse *compound = type->next;
struct pfParse *p, **p2p;
struct pfBaseType *base = pfScopeFindType(pp->scope, pp->name);

if (base == NULL)
    internalErrAt(pp->tok);
pp->ty = type->ty;
p2p = &compound->children;
for (p = compound->children; p != NULL; p = p->next)
    {
    switch (p->type)
        {
	case pptNop:
	    break;
	case pptClass:
	case pptVarInit:
	    addVarToClass(base, p);
	    break;
	case pptToDec:
	case pptFlowDec:
	case pptOperatorDec:
	    addFunctionToClass(base, p);
	    if (isInterface)
	        {
		struct pfParse *body = p->children->next->next->next;
		if (body != NULL)
		    errAt(body->tok, "function body inside of an interface");
		}
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
	case pptPolymorphic:
	    {
	    struct pfType *funcType = p->children->ty;
	    struct pfBaseType *b = base;
	    funcType->tyty = tytyVirtualFunction;
	    p->ty = funcType;
	    addFunctionToClass(base, p->children);
	    base->selfPolyCount += 1;
	    break;
	    }
	default:
	    errAt(p->tok, "non-declaration statement inside of class");
	}
    p2p = &p->next;
    }
slReverse(&base->fields);
slReverse(&base->methods);
}


static void typeConstant(struct pfCompile *pfc, struct pfParse *pp)
/* Create type for constant. */
{
struct pfToken *tok = pp->tok;
if (tok->type == pftString)
    pp->ty = pfTypeNew(pfc->stringType);
else if (tok->type == pftInt)
    pp->ty = pfTypeNew(pfc->intType);
else if (tok->type == pftLong)
    pp->ty = pfTypeNew(pfc->longType);
else if (tok->type == pftFloat)
    pp->ty = pfTypeNew(pfc->doubleType);
else if (tok->type == pftNil)
    pp->ty = pfTypeNew(pfc->nilType);
pp->type = pptFromTokType(pp->tok);
}

static struct pfType *findField(struct pfBaseType *base, char *name)
/* Find named field in class or return NULL */
{
struct pfType *field;
while (base != NULL)
    {
    for (field = base->methods; field != NULL; field = field->next)
	{
	if (sameString(name, field->fieldName))
	    return field;
	}
    for (field = base->fields; field != NULL; field = field->next)
	{
	if (sameString(name, field->fieldName))
	    return field;
	}
    base = base->parent;
    }
return NULL;
}

static void typeDot(struct pfCompile *pfc, struct pfParse *pp)
/* Create type for dotted set of symbols. */
{
struct pfParse *varUse = pp->children;
struct pfParse *fieldUse;
struct pfType *type = varUse->var->ty;

for (fieldUse = varUse->next; fieldUse != NULL; fieldUse = fieldUse->next)
    {
    if (type->base == pfc->stringType)
	type = pfc->stringFullType;
    else if (type->base == pfc->arrayType)
	type = pfc->arrayFullType;
    if (!type->base->isClass)
	errAt(pp->tok, "dot after non-class variable");
    struct pfType *fieldType = findField(type->base, fieldUse->name);
    if (fieldType == NULL)
	errAt(pp->tok, "No field %s in class %s", fieldUse->name, 
		type->base->name);
    fieldUse->ty = fieldType;
    type = fieldType;
    }
pp->ty = CloneVar(type);
pp->ty->next = NULL;
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

static void addDotSelf(struct pfParse **pPp, struct pfBaseType *class)
/* Add self. in front of a method or member access. */
{
struct pfParse *old = *pPp;
struct pfParse *dot = pfParseNew(pptDot, old->tok, old->parent, old->scope);
struct pfParse *newVarUse = pfParseNew(pptVarUse, old->tok, dot, old->scope);

dot->ty = CloneVar(old->ty);
dot->next = old->next;
dot->children = newVarUse;

newVarUse->ty = pfTypeNew(class);
newVarUse->name = "self";
newVarUse->var = pfScopeFindVar(old->scope, "self");
newVarUse->next = old;

old->parent = dot;
old->type = pptFieldUse;
old->next = NULL;
*pPp = dot;
}

static void rBlessFunction(struct pfScope *outputScope,
	struct hash *outputHash, struct pfCompile *pfc, struct pfParse **pPp,
	struct pfScope *classScope)
/* Avoid functions declared in functions, and mark outputs as used.  */
{
struct pfParse *pp = *pPp;
struct pfParse **pos;

switch (pp->type)
    {
    case pptToDec:
    case pptFlowDec:
        errAt(pp->tok, "sorry, can't declare functions inside of functions");
	break;
    case pptVarUse:
	if (pp->var->scope == classScope && pp->parent->type != pptDot)
	    {
	    addDotSelf(pPp, classScope->class);
	    pp = *pPp;
	    }
        break;
    case pptAssignment:
        markUsedVars(outputScope, outputHash, pp);
	break;
    }
for (pos = &pp->children; *pos != NULL; pos = &(*pos)->next)
    rBlessFunction(outputScope, outputHash, pfc, pos, classScope);
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
struct pfParse **pBody = &output->next;
struct pfParse *pp;
struct hashCookie hc;
struct hashEl *hel;

checkIsSimpleDecTuple(input);
checkIsSimpleDecTuple(output);
for (pp = output->children; pp != NULL; pp = pp->next)
    {
    hashAddInt(outputHash, pp->name, 0);
    }
if (*pBody != NULL)
    {
    struct pfScope *classScope = funcDec->scope->parent;
    if (classScope->class == NULL)
        classScope = NULL;
    rBlessFunction(funcDec->scope, outputHash, pfc, pBody, classScope);
    }
hashFree(&outputHash);
}

static void flattenAssign(struct pfCompile *pfc, struct pfParse **pPp)
/* If it's an assignment of a tuple to a tuple, turn it into
 * a bunch of individual assignments. */
{
struct pfParse *pp = *pPp;
struct pfParse *left = pp->children;
struct pfParse *right = left->next;
if (left->type == pptTuple && right->type == pptTuple)
    {
    assert(slCount(left->children) == slCount(right->children));
    left = left->children;
    right = right->children;
    if (left != NULL)
	{
	struct pfParse *aList = NULL, *a = NULL, *lNext, *rNext;
	struct pfParse *lastChild, *oldEnd = pp->next;
	while (left != NULL)
	    {
	    lNext = left->next;
	    rNext = right->next;

	    a = pfParseNew(pptAssignment, pp->tok, pp->parent, pp->scope);
	    a->children = left;
	    left->next = right;
	    right->next = NULL;
	    a->ty = CloneVar(left->ty);
	    slAddHead(&aList, a);

	    left = lNext;
	    right = rNext;
	    }
	lastChild = a;
	slReverse(&aList);
	lastChild->next = oldEnd;
	*pPp = aList;

	/* Handle case where we have to flatten children too. */
	for (;;)
	    {
	    flattenAssign(pfc, pPp);
	    pPp = &((*pPp)->next);
	    if (*pPp == oldEnd)
	        break;
	    }
	}
    }
}

static void checkPara(struct pfCompile *pfc, struct pfParse **pPp)
/* Check one of the para invocation type nodes. */
{
struct pfParse *pp = *pPp;
struct pfParse *collection = pp->children;
struct pfParse *el = collection->next;
struct pfParse *body = el->next;
struct pfBaseType *colBase = collection->ty->base;

if (colBase == pfc->stringType)
    errAt(collection->tok, "strings not allowed as para collections.");
checkElInCollection(pfc, el, collection, "para");
switch (pp->type)
    {
    case pptParaAdd:
    case pptParaMultiply:
    case pptParaMin:
    case pptParaMax:
        {
	enforceNumber(pfc, body);
	pp->ty = body->ty;
	break;
	}
    case pptParaArgMin:
    case pptParaArgMax:
        {
	enforceNumber(pfc, body);
	if (colBase == pfc->dirType)
	    pp->ty = pfTypeNew(pfc->stringType);
	else
	    pp->ty = pfTypeNew(pfc->longType);
	break;
	}
    case pptParaAnd:
    case pptParaOr:
        {
	struct pfParse **pBody = &el->next;
        coerceToBit(pfc, pBody);
	pp->ty = pfTypeNew(pfc->bitType);
	break;
	}
    case pptParaGet:
        {
	struct pfType *ty = pfTypeNew(collection->ty->base);
	ty->children = body->ty;
	pp->ty = ty;
	break;
	}
    case pptParaFilter:
	{
	struct pfParse **pBody = &el->next;
        coerceToBit(pfc, pBody);
	pp->ty = CloneVar(collection->ty);
	break;
	}
    }
}

static void checkRangeInContext(struct pfParse *pp)
/* Check that parent is foreach or para action statement. */
{
struct pfParse *parent = pp->parent;
switch (parent->type)
    {
    case pptForeach:
    case pptParaDo:
    case pptParaAdd:
    case pptParaMultiply:
    case pptParaAnd:
    case pptParaOr:
    case pptParaMin:
    case pptParaMax:
    case pptParaArgMin:
    case pptParaArgMax:
    case pptParaGet:
    case pptParaFilter:
        break;
    default:
        errAt(pp->tok, "Use of 'to' not allowed in this context.");
    }
}

void rTypeCheck(struct pfCompile *pfc, struct pfParse **pPp)
/* Check types (adding conversions where needed) on tree,
 * which should have variables bound already. */
{
struct pfParse *pp = *pPp;
struct pfParse **pos;

for (pos = &pp->children; *pos != NULL; pos = &(*pos)->next)
    rTypeCheck(pfc, pos);

if (verboseLevel() >= 3)
    pfParseDumpOne(pp, 0, stderr);
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
    case pptParaDo:
    case pptParaAdd:
    case pptParaMultiply:
    case pptParaAnd:
    case pptParaOr:
    case pptParaMin:
    case pptParaMax:
    case pptParaArgMin:
    case pptParaArgMax:
    case pptParaGet:
    case pptParaFilter:
        checkPara(pfc, pPp);
	break;
    case pptUntypedElInCollection:
        typeElInCollection(pfc, pPp);
	break;
    case pptIf:
        coerceIf(pfc, pp);
	break;
    case pptPlus:
        coerceBinaryOp(pfc, pp, TRUE, TRUE, TRUE, FALSE);
	coerceStringCat(pfc, pp);
	if (!isNumOrString(pfc, pp))
	    errAt(pp->tok, "This operation only works on numbers and strings.");
	break;
    case pptSame:
    case pptNotSame:
	coerceBinaryOp(pfc, pp, TRUE, TRUE, FALSE, TRUE);
	pp->ty = pfTypeNew(pfc->bitType);
	break;
    case pptGreater:
    case pptLess:
    case pptGreaterOrEquals:
    case pptLessOrEquals:
	coerceBinaryOp(pfc, pp, TRUE, TRUE, FALSE, FALSE);
	pp->ty = pfTypeNew(pfc->bitType);
	break;
    case pptDiv:
	coerceToDouble(pfc, &pp->children);
	coerceToDouble(pfc, &pp->children->next);
        coerceBinaryOp(pfc, pp, TRUE, FALSE, FALSE, FALSE);
        break;
    case pptMul:
    case pptMinus:
        coerceBinaryOp(pfc, pp, TRUE, FALSE, FALSE, FALSE);
	if (!isNumerical(pfc, pp))
	    errAt(pp->tok, "This operation only works on numbers");
	break;
    case pptMod:
        coerceBinaryOp(pfc, pp, FALSE, FALSE, FALSE, FALSE);
	break;
    case pptBitAnd:
    case pptBitOr:
    case pptBitXor:
    case pptShiftLeft:
    case pptShiftRight:
        coerceBinaryOp(pfc, pp, FALSE, FALSE, FALSE, FALSE);
	break;
    case pptLogAnd:
    case pptLogOr:
        coerceBinaryLogicOp(pfc, pp);
	break;
    case pptIndexRange:
        coerceToLong(pfc, &pp->children);
	coerceToLong(pfc, &pp->children->next);
	pp->ty = pfTypeNew(pfc->indexRangeType);
	checkRangeInContext(pp);
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
        coerceAssign(pfc, pp, FALSE, FALSE);
	flattenAssign(pfc, pPp);
	break;
    case pptPlusEquals:
        coerceAssign(pfc, pp, TRUE, FALSE);
	break;
    case pptMinusEquals:
    case pptMulEquals:
    case pptDivEquals:
        coerceAssign(pfc, pp, TRUE, TRUE);
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
    case pptPolymorphic:
        pfParsePutChildrenInPlaceOfSelf(pPp);
	break;
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

static void rClassBless(struct pfCompile *pfc, struct pfParse *pp)
/* Call blessClass on whole tree. */
{
struct pfParse *p;

for (p = pp->children; p != NULL; p = p->next)
    rClassBless(pfc, p);

switch (pp->type)
    {
    case pptClass:
        blessClassOrInterface(pfc, pp, FALSE);
	break;
    case pptInterface:
        blessClassOrInterface(pfc, pp, TRUE);
	break;
    }
}

void pfTypeCheck(struct pfCompile *pfc, struct pfParse **pPp)
/* Check types (adding conversions where needed) on tree,
 * which should have variables bound already. */
{
rClassBless(pfc, *pPp);
rTypeCheck(pfc, pPp);
}
