/* pfType - ParaFlow type heirarchy and type checking. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "pfType.h"


void rTypeCheck(struct pfCompile *pfc, struct pfParse **pPp);
/* Check types (adding conversions where needed) on tree,
 * which should have variables bound already. */

static void coerceOne(struct pfCompile *pfc, struct pfParse **pPp,
	struct pfType *type, boolean numToString);
/* Make sure that a single variable is of the required type. 
 * Add casts if necessary */

static void coerceTupleToClass(struct pfCompile *pfc, 	
	struct pfParse **pPp, struct pfBaseType *base);
/* Given a type that is a class, and a parse tree that
 * is a tuple, do any casting required inside the tuple
 * to get the members of the tuple to be of the same type
 * as the corresponding members of the class. */

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

boolean pfBaseIsDerivedClass(struct pfBaseType *base)
/* Return TRUE if we have a parent class. */
{
struct pfBaseType *parentBase = base->parent;
return (parentBase != NULL && parentBase->isClass);
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

char *pfTytyAsString(enum pfTyty tyty)
/* Return string representation of tyty. */
{
switch (tyty)
    {
    case tytyVariable:
        return "tytyVariable";
    case tytyTuple:
        return "tytyTuple";
    case tytyFunction:
        return "tytyFunction";
    case tytyVirtualFunction:
        return "tytyVirtualFunction";
    case tytyModule:
        return "tytyModule";
    case tytyOperator:
        return "tytyOperator";
    case tytyFunctionPointer:
        return "tytyFunctionPointer";
    default:
        return "unknown tyty";
    }
}


void dumpTypeToDyString(struct pfType *ty, struct dyString *dy)
/*  Append info on type to dy */
{
if (ty == NULL)
    dyStringPrintf(dy, "void");
else
    {
    if (ty->access != paUsual)
        dyStringPrintf(dy, "%s ", pfAccessTypeAsString(ty->access));
    if (ty->isConst)
        dyStringPrintf(dy, "const ");
    if (ty->tyty == tytyTuple)
	{
	dyStringPrintf(dy, "(");
	for (ty = ty->children; ty != NULL; ty = ty->next)
	    {
	    dumpTypeToDyString(ty, dy);
	    if (ty->next != NULL)
		dyStringPrintf(dy, " ");
	    }
	dyStringPrintf(dy, ")");
	}
    else if (ty->tyty == tytyFunction || ty->tyty == tytyVirtualFunction
       || ty->tyty == tytyOperator || ty->tyty == tytyFunctionPointer)
	{
	if (ty->tyty == tytyVirtualFunction)
	   dyStringPrintf(dy, "polymorphic ");
	dyStringPrintf(dy, "%s ", ty->base->name);
	dumpTypeToDyString(ty->children, dy);
	dyStringPrintf(dy, " into ");
	dumpTypeToDyString(ty->children->next, dy);
	}
    else
	{
	dyStringPrintf(dy, "%s", ty->base->name);
	ty = ty->children;
	if (ty != NULL)
	    {
	    dyStringPrintf(dy, " of ");
	    dumpTypeToDyString(ty, dy);
	    for (ty = ty->next; ty != NULL; ty = ty->next)
		{
		dyStringPrintf(dy, ",");
		dumpTypeToDyString(ty, dy);
		}
	    }
	}
    }
}

void pfTypeDump(struct pfType *ty, FILE *f)
/* Write out info on ty to file.  (No newlines written) */
{
struct dyString *dy = dyStringNew(0);
dumpTypeToDyString(ty, dy);
fprintf(f, "%s", dy->string);
dyStringFree(&dy);
}

static struct pfModule *findEnclosingModule(struct pfParse *pp)
/* Find module that we are in. */
{
for (;pp != NULL; pp = pp->parent)
    {
    switch (pp->type)
        {
	case pptModule:
	case pptModuleRef:
	case pptMainModule:
	    return pp->scope->module;
	    break;
	}
    }
internalErr();
return NULL;
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


static void typeMismatch(struct pfParse *pp, struct pfType *type)
/* Complain about type mismatch at node. */
{
struct dyString *dy = dyStringNew(0);
dumpTypeToDyString(type, dy);
if (pp->name)
    {
    errAt(pp->tok, "Type mismatch:  %s not %s.", pp->name, dy->string);
    }
else
    {
    errAt(pp->tok, "Type mismatch: expecting %s.", dy->string);
    }
dyStringFree(&dy);
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
    case pftSubstitute:
	return pptConstString;
    case pftNil:
        return pptConstZero;
    default:
	internalErrAt(tok);
	return 0;
    }
}

static struct pfVar *findVar(struct pfScope *scope, char *name, 
	struct pfToken *tok)
/* Find variable in scope or abort with error message. */
{
struct pfVar *var = pfScopeFindVar(scope, name);
if (var == NULL)
    errAt(tok, "%s is undefined.", name);
return var;
}

static boolean isNumerical(struct pfCompile *pfc, struct pfParse *pp)
/* Return TRUE if numerical (or var) type. */
{
return pp->ty->base == pfc->varType || pp->ty->base->parent == pfc->numType;
}

static boolean isNumOrString(struct pfCompile *pfc, struct pfParse *pp)
/* Return TRUE if numerical or string type. */
{
return pp->ty->base == pfc->dyStringType || pp->ty->base == pfc->stringType 
	|| isNumerical(pfc, pp);
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

static int baseTypeLogicalSize(struct pfCompile *pfc, struct pfBaseType *base,
	struct pfToken *tok)
/* Return logical size of type - 0 for smallest, 1 for next smallest, etc. */
{
if (base == pfc->bitType)
    return 0;
else if (base == pfc->byteType)
    return 1;
else if (base == pfc->charType)
    return 2;
else if (base == pfc->shortType)
    return 3;
else if (base == pfc->intType)
    return 4;
else if (base == pfc->longType)
    return 5;
else if (base == pfc->floatType)
    return 6;
else if (base == pfc->doubleType)
    return 7;
else if (base == pfc->stringType || base == pfc->dyStringType)	
    return 8;
else
    {
    errAt(tok, "Can't convert %s to number or string", base->name);
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
int numTypeCount = 9;
enum pfParseType castType = pptCastBitToBit;
if ((oldBase == pfc->stringType || oldBase == pfc->dyStringType)  
	&& (newBase != pfc->stringType && newBase != pfc->dyStringType))
    expectingGot("string", pp->tok);
castType += numTypeCount * baseTypeLogicalSize(pfc, oldBase, pp->tok);
castType += baseTypeLogicalSize(pfc, newBase, pp->tok);
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

static void coerceToString(struct pfCompile *pfc, struct pfParse **pPp)
/* Make sure type of pp is string. */
{
coerceToBaseType(pfc, pfc->stringType, pPp);
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
coerceTupleToClass(pfc, pPp, type->base);
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
if (tuple->type == pptCall || tuple->type == pptIndirectCall)
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
		 if (type->init->ty == NULL)  /* Handle forward reference */
		     rTypeCheck(pfc, &type->init);
		 *pos = CloneVar(type->init);
		 }
	     }
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
struct pfType *functionType = function->ty;
struct pfParse *input = function->next;
struct pfType *inputType = functionType->children;
struct pfType *outputType = inputType->next;
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
    if (sameString(function->name, "throw"))
        {
	if (slCount(input->children) != 1)
	    errAt(input->children->tok, "throw always takes one parameter");
	coerceOne(pfc, &input->children, pfc->seriousErrorFullType, FALSE);
	insertCast(pptCastTypedToVar, pfTypeNew(pfc->varType), &input->children);
	}
    else if (sameString(function->name, "throwMore"))
        {
	if (slCount(input->children) != 2)
	    errAt(input->children->tok, "throwMore always takes two parameters");
	coerceOne(pfc, &input->children, pfc->seriousErrorFullType, FALSE);
	coerceOne(pfc, &input->children->next, pfc->seriousErrorFullType, FALSE);
	insertCast(pptCastTypedToVar, pfTypeNew(pfc->varType), 
		&input->children->next);
	}
    else
	internalErr();
    pp->name = function->name;
    pp->ty = CloneVar(outputType);
    }
}

static boolean tupleKeyValsPresentOk(struct pfParse *tuple)
/* Return true if tuple has any keyVals.  Also ensures that
 * if it has keyVals, that once an element is key val, so
 * are all the remaining elements. */
{
boolean gotKeyVal = FALSE;
struct pfParse *pp;
for (pp = tuple->children; pp != NULL; pp = pp->next)
    {
    if (pp->type == pptKeyVal)
        gotKeyVal = TRUE;
    else
        {
	if (gotKeyVal)
	    errAt(pp->tok, 
	    	"Positional parameters can't follow named parameters.");
	}
     }
return gotKeyVal;
}

static void coerceNamedTuple(struct pfCompile *pfc, 
	struct pfParse **pTuple, struct pfType *types)
/* Here we are either a function parameter list, or we are
 * a class being initialized.  It's possible that we'll be
 * specified positionally, or by name.  Deal with it. */
{
struct pfParse *tuple = *pTuple, *pp;
boolean gotKeyVal = tupleKeyValsPresentOk(tuple);
if (!gotKeyVal)
    {
    coerceTuple(pfc, pTuple, types);
    return;
    }
else
    {
    struct hash *fieldHash = hashNew(8);
    struct hash *hash = hashNew(8);
    struct pfType *type;
    struct pfParse *orderedList = NULL;

    /* Create a hash of all fields (or for functions parameters.
     * This is just to make sure that all named parameters actually
     * refer to a real field. */
    for (type = types->children; type != NULL; type = type->next)
        hashAdd(fieldHash, type->fieldName, NULL);

    /* Create a hash keyed by the field name. */
    for (pp = tuple->children, type = types->children;
         pp != NULL && type != NULL;
	 pp = pp->next, type = type->next)
	 {
	 if (pp->type == pptKeyVal)
	     {
	     struct pfParse *key = pp->children;
	     struct pfParse *val = key->next;
	     if (!hashLookup(fieldHash, key->name))
	         errAt(key->tok, "%s seems misspelled or misplaced", key->name);
	     if (hashLookup(hash, key->name) != NULL)
	         errAt(key->tok, "%s is a duplicate", key->name);
	     hashAdd(hash, key->name, val);
	     }
	 else
	     {
	     hashAdd(hash, type->fieldName, pp);
	     }
	 }

    for (type = types->children; type != NULL; type = type->next)
        {
	pp = hashFindVal(hash, type->fieldName);
	if (pp == NULL)
	    {
	    if (type->init == NULL)
		 {
	         errAt(tuple->tok, "missing %s:value", type->fieldName);
		 }
	    else
	         {
		 if (type->init->ty == NULL) /* Handle forward reference */
		     rTypeCheck(pfc, &type->init);
		 pp = CloneVar(type->init);
		 }
	    }
	coerceOne(pfc, &pp, type, FALSE);
	slAddHead(&orderedList, pp);
	}
    slReverse(&orderedList);
    tuple->children = orderedList;
    tuple->ty = CloneVar(types);
    hashFree(&hash);
    hashFree(&fieldHash);
    }
}

static void checkOneParamWritable(struct pfCompile *pfc, struct pfParse *pp)
/* Check that pp contains only local variables. */
{
if (pfBaseTypeIsPassedByValue(pfc, pp->ty->base))
    {
    if (pp->type != pptVarUse)
        errAt(pp->tok, "local inputs must be simple variables");
    if (!pp->var->scope->isLocal)
        errAt(pp->tok, "local input %s is not a local variable", pp->var->name);
    }
}

static void checkWritableParams(struct pfCompile *pfc, struct pfParse *paramTuple,
	struct pfType *inputType)
{
struct pfType *type;
struct pfParse *param;

for (type = inputType->children, param = paramTuple->children;
     type != NULL; type = type->next, param = param->next)
     {
     if (type->access == paWritable)
	 checkOneParamWritable(pfc, param);
     }
}

static void coerceCall(struct pfCompile *pfc, struct pfParse **pPp)
/* Make sure that parameters to call are right.  Then
 * set pp->type to call's return type. */
{
struct pfParse *pp = *pPp;
struct pfParse *function = pp->children;
struct pfType *functionType = function->ty;
switch(functionType->tyty)
    {
    case tytyOperator:
        {
	coerceCallToOperator(pfc, pp);
	break;
	}
    case tytyFunction:
    case tytyVirtualFunction:
    case tytyFunctionPointer:
	{
	struct pfType *inputType = functionType->children;
	struct pfType *outputType = inputType->next;
	struct pfParse *paramTuple = function->next;
	struct pfParse *firstParam = paramTuple->children;

	if (firstParam != NULL && 
	    (firstParam->type == pptCall || firstParam->type == pptIndirectCall)
	    && firstParam->next == NULL && slCount(inputType->children) != 1)
	    {
	    /* Here we attempt to handle the case of a function
	     * with multiple outputs being fed into a function
	     * with multiple inputs. */
	    coerceCallToTupleOfTypes(pfc, &paramTuple->children, 
	    	inputType->children);
	    }
	else
	    {
	    struct pfParse **pParamTuple = &function->next;
	    coerceNamedTuple(pfc, pParamTuple, inputType);
	    checkWritableParams(pfc, *pParamTuple, inputType);
	    }
	if (outputType->children != NULL && outputType->children->next == NULL)
	    pp->ty = CloneVar(outputType->children);
	else
	    pp->ty = CloneVar(outputType);
	pp->name = function->name;
	if (functionType->tyty == tytyFunctionPointer)
	    pp->type = pptIndirectCall;
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
struct pfType *elType = type->children;
struct pfParse **pos;
if (type->base == pfc->dirType)
     {
     struct pfParse *pp;
     /* Convert keys in key-val pairs from pptKeyType to
      * pptConstString. */
     for (pp = tuple->children; pp != NULL; pp = pp->next)
	 {
	 struct pfParse *key, *val;
	 if (pp->type != pptKeyVal)
	    errAt(pp->tok, "Expecting key:val here.");
	 key = pp->children;
	 key->type = pptConstString;
	 key->ty = pfTypeNew(pfc->stringType);
	 coerceOne(pfc, &key->next, elType, FALSE);
	 val = key->next;
	 pp->ty = typeFromChildren(pfc, pp, pfc->keyValType);
	 }
     }
else
     {
     for (pos = &tuple->children; *pos != NULL; pos = &(*pos)->next)
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
	    if (field->init->ty == NULL)	/* Handle forward reference */
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

static struct pfType *rClassFieldTypes(struct pfBaseType *base)
/* Return list of field types. */
{
struct pfType *field, *el, *list = NULL;
if (base->parent)
    list = rClassFieldTypes(base->parent);
for (field = base->fields; field != NULL; field = field->next)
    {
    el = CloneVar(field);
    slAddHead(&list, el);
    }
return list;
}

static void coerceNamedTupleToClass(struct pfCompile *pfc, 
	struct pfParse **pTuple, struct pfBaseType *base)
/* Here there is a named parameter in the initialization set.
 * Our strategy will be to create a flattened pfType, and
 * then call coerceNamedTuple with it. */
{
struct pfParse *tuple = *pTuple;
struct pfType *type = CloneVar(tuple->ty);
struct pfType *typeList = rClassFieldTypes(base);
slReverse(&typeList);
type->children = typeList;
coerceNamedTuple(pfc, pTuple, type);
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
struct pfParse *initMethod = base->initMethod;
if (initMethod)
    {
    /* convert parse tree from
     *     pptTuple 
     *         (input-to-init)
     * to
     *     pptClassAllocFromInit <classType>
     *        pptTuple   
     *          (default-values-and-zeroes)
     *        pptTuple
     *          (input-to-init)
     */
    struct pfType *initType = initMethod->ty;
    struct pfType *initInputType = initType->children;
    struct pfParse *allocInit= pfParseNew(pptClassAllocFromInit, tuple->tok,
    	tuple->parent, tuple->scope);
    struct pfParse *defaultValTuple = pfParseNew(pptTuple, tuple->tok,
    	tuple->parent, tuple->scope);

    /* Coerce the new empty tuple into something that will contain
     * default values for all fields. */
    rCoerceTupleToClass(pfc, defaultValTuple, &defaultValTuple->children, 
	    base, TRUE);
    coerceNamedTuple(pfc, &tuple, initInputType);

    /* Set up types on everything. */
    allocInit->ty = pfTypeNew(base);
    pfTypeOnTuple(pfc, tuple);
    pfTypeOnTuple(pfc, defaultValTuple);

    /* Arrange the parse tree. */
    allocInit->next = tuple->next;
    allocInit->children = defaultValTuple;
    defaultValTuple->next = tuple;
    tuple->next = NULL;
    tuple->parent = allocInit;
    *pPp = allocInit;
    }
else
    {
    boolean gotKeyVal = tupleKeyValsPresentOk(tuple);
    struct pfParse *classAlloc = pfParseNew(pptClassAllocFromTuple,
    	tuple->tok, tuple->parent, tuple->scope);
    classAlloc->next = tuple->next;
    classAlloc->ty = pfTypeNew(base);
    if (gotKeyVal)
	{
        coerceNamedTupleToClass(pfc, &tuple, base);
	}
    else
	{
	pLeftover = rCoerceTupleToClass(pfc, tuple, &tuple->children, 
		base, fillMissingWithZero);
	if (*pLeftover != NULL)
	    errAt(tuple->tok, "Type mismatch");
	pfTypeOnTuple(pfc, tuple);
	}
    classAlloc->children = tuple;
    tuple->next = NULL;
    tuple->parent = classAlloc;
    *pPp = classAlloc;
    }
}

static boolean fruitfulToInitClassFromSingleton(struct pfCompile *pfc,
	struct pfBaseType *classBase)
/* Look and see whether it will be ok to promote a singleton
 * to a tuple. In general this is a handy shortcut for initializing
 * classes of a single field, so we can do:
 *     type var = (val)
 * instead of
 *     type var = (val,)
 * However if the first field of the class or the init routine
 * is itself a class, this could lead to an infinite loop. */
{
struct pfParse *initMethod = classBase->initMethod;
struct pfType *fields = NULL;
if (initMethod)
    {
    fields = initMethod->ty->children->children;
    }
else
    {
    struct pfBaseType *base;
    for (base = classBase; base != NULL; base = base->parent)
        {
	fields = base->fields;
	if (fields != NULL)
	    break;
	}
    }
if (fields != NULL)
    {
    struct pfBaseType *base = fields->base;
    if (!base->isClass)
	return TRUE;
    }
return FALSE;
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

static boolean compatibleInputOutput(struct pfType *a, struct pfType *b)
/* Make sure that the input and output (first two children) of a and b
 * are compatable. */
{
return pfTypeSame(a->children, b->children) && 
       pfTypeSame(a->children->next, b->children->next);
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
	/* Nil just gets converted to any type.  */
	pp->ty = CloneVar(destType);
	ok = TRUE;
	}
    else if (destBase == pfc->stringType && pt->base == pfc->dyStringType)
        {
	insertCast(pptStringDupe, pfTypeNew(destBase), pPp);
	ok = TRUE;
	}
    else if (destBase == pfc->dyStringType && pt->base == pfc->stringType)
        {
	insertCast(pptStringDupe, pfTypeNew(destBase), pPp);
	ok = TRUE;
	}
    else if (destBase == pfc->charType 
    	&& (pt->base == pfc->stringType || pt->base == pfc->dyStringType)
    	&& pp->type == pptConstString)
        {
	/* We catch character initializations with string constants here. */
	struct pfToken *tok = pp->tok;
	if (tok->type == pftString || tok->type == pftSubstitute)
	    {
	    if (strlen(tok->val.s) != 1)
	        errAt(tok, "You can only initialze chars with single character strings.");
	    pp->type = pptConstChar;
	    pp->ty = pfTypeNew(pfc->charType);
	    }
	else
	    {
	    internalErr();
	    }
	ok = TRUE;
	}
    else if ((destBase == pfc->stringType || destBase == pfc->dyStringType)
    	&& pt->base == pfc->charType)
        {
	insertCast(pptCastCharToString, CloneVar(destType), pPp);
	ok = TRUE;
	}
    else if (destBase == pfc->bitType && (pt->base == pfc->stringType || pt->base == pfc->dyStringType))
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
    else if (numToString && (destBase == pfc->stringType || destBase == pfc->dyStringType)
    	&& pt->base->parent == pfc->numType)
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
    else if (destBase == pfc->flowPtType && pt->base == pfc->flowType)
        {
	if (compatibleInputOutput(destType, pt))
	    {
	    insertCast(pptCastFunctionToPointer, CloneVar(destType), pPp);
	    ok = TRUE;
	    }
	}
    else if (destBase == pfc->toPtType && 
    	(pt->base == pfc->flowType || pt->base == pfc->toType))
        {
	if (compatibleInputOutput(destType, pt))
	    {
	    insertCast(pptCastFunctionToPointer, CloneVar(destType), pPp);
	    ok = TRUE;
	    }
	}
    else if (destBase->isCollection && 
    	(destBase != pfc->stringType && destBase != pfc->dyStringType))
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
	verbose(3, "%s is a class\n", destBase->name);
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
	    /* Try in many cases to turn a single value into a tuple.
	     * This is a convenience for user, but have to be careful
	     * not to enter an infinite loop if the first field of
	     * class is also a class... */
	    if (pt->tyty != tytyTuple)
		{
		if (fruitfulToInitClassFromSingleton(pfc, destBase))
		    {
		    insertCast(pptTuple, NULL, pPp);  
		    pfTypeOnTuple(pfc, *pPp);
		    }
		else
		    {
		    errAt(pp->tok, 
		    	"Can't  initialize %s from singleton, try (singleton,)"
			, destType->base);
		    typeMismatch(pp, destType);
		    }
		}
	    if (pp->type == pptCall || pp->type == pptIndirectCall)
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

static void coerceCase(struct pfCompile *pfc, struct pfParse *casePp)
/* Check that case statement is kosher */
{
struct pfParse *expPp = casePp->children;
struct pfType *expType = expPp->ty;
struct pfParse *listPp = expPp->next;
struct pfParse *itemPp;

for (itemPp = listPp->children; itemPp != NULL; itemPp = itemPp->next)
    {
    if (itemPp->type == pptCaseItem)
        {
	struct pfParse *tuple = itemPp->children;
	struct pfParse **pPp;
	for (pPp = &tuple->children; *pPp != NULL; pPp = &((*pPp)->next))
	    {
	    coerceOne(pfc, pPp, expType, FALSE);
	    }
	}
    else if (itemPp->type == pptCaseElse)
        ;	/* No action, just checking */
    else
        internalErrAt(itemPp->tok);
    }
}

static void turnSelfToVarOfType(struct pfParse *pp, struct pfType *ty)
/* Turn parse node into pptVarInit type with two children (type and symbol).
 * The variable is actually expected to be in the symbol table already
 * but it doesn't yet have a type.  We make up a type and update
 * the symbol table with it as well as hanging the type on ourself. */
{
struct pfVar *var = findVar(pp->scope, pp->name, pp->tok);
struct pfParse *typePp = pfParseNew(pptTypeName, pp->tok, pp, pp->scope);
struct pfParse *symPp = pfParseNew(pptSymName, pp->tok,pp, pp->scope);
*(var->ty) = *ty;
symPp->name = pp->name;
typePp->ty = pp->ty = var->ty;
pp->type = pptVarInit;
pp->children = typePp;
typePp->next = symPp;
}

static void typeElInCollection(struct pfCompile *pfc, struct pfParse **pPp)
/* Fill in type of el from type of collection.  Possibly fill in key type
 * as well.  The parse tree we are concerned with looks either like:
 *      pptForeach/pptPara
 *         pptVarUse <collection>
 *         pptUntypedElInCollection 
 * or
 *      pptForeach/pptPara
 *         pptVarUse <collection>
 *         pptKeyVal
 *            pptKeyName
 *            pptUntypedElInCollection
 * In either case *pPp points to the pptUntypedElInCollection.
 * We want to convert the pptUntypeElInCollection into a
 *        pptVarInit
 *           pptTypeName
 *           pptSymName
 * If the pptKeyVal is present we want to do the same thing to it.
 * In addition to flipping around the parse tree, we also need to
 * set up the types rigt on the pptVarInits. */
{
struct pfParse *el = *pPp, *keyVal = NULL, *collection, *foreach;
struct pfBaseType *keyBaseType = NULL;

/* Figure out where the parts of the input tree we need are. */
foreach = el->parent;
if (foreach->type == pptKeyVal)
    {
    keyVal = foreach;
    foreach = foreach->parent;
    }
collection = foreach->children;
if (collection->type == pptIndexRange)
    {
    keyBaseType = pfc->longType;
    if (keyVal != NULL)
        {
	errAt(keyVal->tok, "key:val not allowed in collections specified by a range of numbers. The key and val are the same in this case!");
	}
    }
else if (collection->ty->base->keyedBy != NULL)
    keyBaseType = collection->ty->base->keyedBy;
else if (collection->type != pptCall && collection->type != pptIndirectCall)
    errAt(collection->tok, "%s isn't a collection, it's a %s", collection->name, collection->ty->base->name);

/* Covert pptUntypedElement into a pptVarInit that is typed to
 * match the collection. */
    {
    struct pfType *ty;
    if (collection->type == pptCall || collection->type == pptIndirectCall)
	{
	ty = collection->ty;
	if (ty->children != NULL)
	    errAt(collection->tok, 
	    	"functions here can only return a single value");
	if (keyVal != NULL)
	    errAt(keyVal->tok, "key:values aren't allowed when collection is a call.");
	}
    else
	{
	if (collection->ty->base == pfc->stringType || collection->ty->base == pfc->dyStringType)
	    ty = pfTypeNew(pfc->charType);
	else if (collection->ty->base == pfc->indexRangeType)
	    ty = pfTypeNew(pfc->longType);
	else
	    ty = collection->ty->children;
	}
    turnSelfToVarOfType(el, ty);
    }

/* If we're doing key:value, then turn the pptKeyName into a pptVarInit
 * that is typed to match the collection key. */
if (keyVal != NULL)
    {
    struct pfParse *key = keyVal->children;
    struct pfType *ty = pfTypeNew(keyBaseType);
    assert(key->type == pptKeyName);
    assert(key->next == el);
    turnSelfToVarOfType(key, ty);
    keyVal->ty = typeFromChildren(pfc, keyVal, pfc->keyValType);
    }
}


static void foreachIntoForeachCall(struct pfCompile *pfc, struct pfParse *pp)
/* Rework parse tree from:
 *     pptForeach
 *        <function call>
 *        pptVarInit
 *           pptTypeName <function-type>
 *           pptSymName <element name>
 *        <loop body>
 * Into
 *     pptFor
 *        pptVarInit
 *           pptTypeName <function-type>
 *           pptSymName <element name>
 *           <function call>
 *        <pptCastXxxToBit>
 *           pptVarUse <element var>
 *        <pptAssign>
 *           pptVarUse <element var>
 *           <function call>
 *        <loop body>
 */
{
struct pfParse *call = pp->children;
struct pfParse *elInit = call->next;
struct pfParse *body = elInit->next;
struct pfVar *elVar = elInit->var;

if (elInit->type != pptVarInit)
    errAt(elInit->tok, "element must be just a name");
/* Create new parse nodes. */
struct pfParse *firstVarUse = pfParseNew(pptVarUse, elInit->tok, pp, pp->scope);
struct pfParse *assignment = pfParseNew(pptAssign, elInit->tok, pp, pp->scope);
struct pfParse *secondVarUse = pfParseNew(pptVarUse, elInit->tok, pp, pp->scope);
firstVarUse->var = secondVarUse->var = elVar;
firstVarUse->ty = secondVarUse->ty = elVar->ty;
coerceToBit(pfc, &firstVarUse);		/* This inserts cast. */

/* Turn foreach into for, and rearrange rearrange parse tree incorperating
 * new nodes. */
pp->type = pptFor;
pp->children = elInit;
call->next = NULL;
elInit->children->next->next = call;
elInit->next = firstVarUse;
firstVarUse->next = assignment;
assignment->children = secondVarUse;
secondVarUse->next = call;
assignment->next = body;
}

static void checkForeach(struct pfCompile *pfc, struct pfParse *pp)
/* Figure out if looping through a collection, or over
 * repeated uses of function, and type check accordingly. */
{
/* Make sure have agreement between element and collection vars */
struct pfParse *source = pp->children;

if (source->type == pptCall || source->type == pptIndirectCall)
    {
    foreachIntoForeachCall(pfc, pp);
    }
else
    {
    struct pfParse *el = source->next;
    }
}

static void checkLvalWritable(struct pfParse *pp)
/* Make sure that lVal is writable. */
{
switch (pp->type)
    {
    case pptVarUse:
        {
	struct pfVar *var = pp->var;
	struct pfModule *varModule = var->scope->module;
	struct pfType *ty = var->ty;
	enum pfAccessType access = ty->access;
	if (ty->isConst)
	    errAt(pp->tok, "Writing to a const");
	if (varModule)
	    {
	    if (access != paWritable)
	        {
		struct pfModule *myModule = findEnclosingModule(pp);
		if (myModule != varModule)
		    errAt(pp->tok, "%s is not writable in this module", var->name);
		}
	    }
	break;
	}
    case pptIndex:
        {
	struct pfParse *collection = pp->children;
	checkLvalWritable(collection);
	break;
	}
    case pptFieldUse:
        {
	struct pfParse *classPp = pp->parent->children;
	struct pfBaseType *classBase = classPp->ty->base;
	struct pfType *fieldType = findField(classBase, pp->name);
	if (fieldType->access == paUsual || fieldType->access == paGlobal)
	    {
	    struct pfModule *myModule = findEnclosingModule(pp);
	    struct pfModule *classModule = findEnclosingModule(classBase->def);
	    if (myModule != classModule)
		errAt(pp->tok, "You can't write to field %s of class %s in this module",
		    fieldType->fieldName, classBase->name);
	    }
	break;
	}
    case pptDot:
        {
	struct pfParse *left = pp->children;
	struct pfParse *right = left->next;

	if (left->type == pptModuleUse)
	    checkLvalWritable(right);
	else
	    {
	    checkLvalWritable(right);
	    checkLvalWritable(left);
	    }
	break;
	}
    }
}

struct pfType *coerceLval(struct pfCompile *pfc, struct pfParse *pp)
/* Ensure that pp can be assigned.  Return it's type */
{
checkLvalWritable(pp);
switch (pp->type)
    {
    case pptVarInit:
    case pptVarUse:
    case pptDot:
        return pp->ty;
    case pptIndex:
	{
	struct pfParse *collection = pp->children;
	if (collection->ty->base == pfc->stringType)
	     errAt(collection->tok, 
	    "Can't assign to a character in a string, use a dyString instead");
        return pp->ty;
	}
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
coerceOne(pfc, &lval->next, destType, FALSE);
pp->ty = CloneVar(destType);
}

static void coercePlusEquals(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure that left hand of assignment is a valid l-value of
 * num or dyString type.  Coerce right hand side to same type. Set
 * pp->type to l-value type*/
{
struct pfParse *lval = pp->children;
struct pfType *destType = coerceLval(pfc, lval);
struct pfBaseType *destBase = destType->base;
if (destBase == pfc->dyStringType)
    {
    coerceOne(pfc, &lval->next, pfTypeNew(pfc->dyStringType), TRUE);
    }
else if (destBase->parent == pfc->numType)
    {
    coerceOne(pfc, &lval->next, destType, FALSE);
    }
else
    {
    expectingGot("numerical or dyString variable to left of assignment", lval->tok);
    }
pp->ty = CloneVar(destType);
}


static boolean  rCheckDimsAndModules(struct pfCompile *pfc, struct pfParse *type)
/* Do a double check on all pptTypeName nodes. Make sure that any
 * child nodes are array dimensions. Check that if type is defined
 * in another module that it is global. Returns TRUE if there is
 * a dimensioned array. */
{
boolean gotArray = FALSE;
switch (type->type)
    {
    case pptTypeName:
	{
	struct pfBaseType *base = type->ty->base;
	struct pfModule *typeModule = base->scope->module;
	if (typeModule != NULL)
	    {
	    enum pfAccessType access = base->access;
	    if (access != paGlobal)
	        {
		struct pfModule *myModule = findEnclosingModule(type);
		if (myModule != typeModule)
		    errAt(type->tok, 
		    "class %s in %s is not global, so it can't be used in %s",
		    base->name, typeModule->name, myModule->name);
		}
	    }
	if (type->children != NULL)
	    {
	    if (base != pfc->arrayType)
	        errAt(type->children->tok, "[ illegal here except for arrays");
	    coerceOne(pfc, &type->children, pfc->longFullType, FALSE);
	    gotArray = TRUE;
	    }
        break;
	}
    default:
        break;
    }
for (type = type->children; type != NULL; type = type->next)
    gotArray |= rCheckDimsAndModules(pfc, type);
return gotArray;
}

static boolean checkTypeWellFormed(struct pfCompile *pfc, struct pfParse *type)
/* Make sure that a type expression is a-ok. Returns TRUE if there
 * is a dimensioned array in the type. */
{
return rCheckDimsAndModules(pfc, type);
}

static boolean isFunctionIo(struct pfParse *varInit)
/* Return TRUE if varInit is part of function input or output. */
{
/* Can safely take grandparent without NULL check because there
 * is always a module and program at the least ahead of us. */
struct pfParse *grandParent = varInit->parent->parent; 
switch (grandParent->type)
    {
    case pptToDec:
    case pptFlowDec:
    case pptOperatorDec:
        return TRUE;
    default:
        return FALSE;
    }
}

static void checkRedefinitionInParent(struct pfCompile *pfc, 
	struct pfParse *varInit)
/* Make sure that variable is not defined as a public member of parent class. */
{
if (!isFunctionIo(varInit) && pfParseEnclosingFunction(varInit) == NULL)
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
		    errAt(varInit->tok, 
		    	"%s already defined in parent class %s", name, base->name);
		}
	    }
	}
    }
}

static void checkConstExp(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure that expression is really constant.  No non-constant 
 * variables allowed. */
{
switch (pp->type)
    {
    case pptVarUse:
	{
        struct pfVar *var = pp->var;
	if (!var->ty->isConst)
	   errAt(pp->tok, "Can only initialize a constant with other constants.");
	break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    checkConstExp(pfc, pp);
}

static void coerceVarInit(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure that variable initialization can be coerced to variable
 * type. */
{
struct pfParse *type = pp->children;
struct pfParse *symbol = type->next;
struct pfParse *init = symbol->next;
boolean gotDimensionedArray;

if (init != NULL)
    coerceOne(pfc, &symbol->next, type->ty, FALSE);
gotDimensionedArray = checkTypeWellFormed(pfc, type);
if (init && gotDimensionedArray)
    errAt(init->tok, "Can't initialized dimensioned array. Please either leave out [size] after array or leave out initialization tuple.");
checkRedefinitionInParent(pfc, pp);
if (pp->isConst && init != NULL)
    checkConstExp(pfc, init);
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

if (collectionBase == pfc->arrayType || collectionBase == pfc->stringType
	|| collectionBase == pfc->dyStringType)
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
if (collectionBase == pfc->stringType || collectionBase == pfc->dyStringType)
    pp->ty = pfTypeNew(pfc->charType);
else
    pp->ty = CloneVar(collectionType->children);
}

static struct pfType *largerNumType(struct pfCompile *pfc,
	struct pfType *a, struct pfType *b, struct pfToken *tok)
/* Return a or b, whichever can hold the larger range. */
{
if (baseTypeLogicalSize(pfc, a->base, tok) > 
	baseTypeLogicalSize(pfc, b->base, tok))
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
	struct pfType *ty = largerNumType(pfc, lTy, rTy, pp->tok);
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
    if (base == pfc->stringType || base == pfc->dyStringType)
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
if (plusOp->ty->base == pfc->stringType || plusOp->ty->base == pfc->dyStringType)
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

static void modifierNotAllowed(struct pfParse *pp)
/* Complain about inappropriate variable modifier. */
{
errAt(pp->tok, "Variable modifier %s not allowed here", 
     pfAccessTypeAsString(pp->access));
}

static void constNotAllowed(struct pfParse *pp)
/* Complain about inappropriate const. */
{
errAt(pp->tok, "const not allowed here");
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
switch (varPp->access)
    {
    case paStatic:
    case paReadable:
    case paGlobal:
	modifierNotAllowed(varPp);
	break;
    }
}

static void addFunctionToClass(struct pfBaseType *class, struct pfParse *funcPp)
/* Add variable to class. */
{
struct pfType *type = CloneVar(funcPp->ty);
type->fieldName = funcPp->name;
slAddHead(&class->methods, type);
}

static boolean rFindParentInit(struct pfParse *pp)
/* Look for pptCall
 *             pptDot
 *                pptVarUse parent
 *                pptFieldUse init
 */
{
switch (pp->type)
     {
     case pptCall:
	 {
	 struct pfParse *dot = pp->children;
	 if (dot->type == pptDot)
	     {
	     struct pfParse *var = dot->children;
	     if (var->type == pptVarUse)
	         {
		 struct pfParse *field = var->next;
		 if (field->type == pptFieldUse)
		     {
		     if (sameString(var->name, "parent") 
		        && sameString(field->name, "init"))
			 {
			 return TRUE;
			 }
		     }
		 }
	     }
         break;
	 }
     default:
         break;
     }
for (pp = pp->children; pp != NULL; pp = pp->next)
    if (rFindParentInit(pp))
        return TRUE;
return FALSE;
}

static boolean funcHasBody(struct pfParse *func)
/* Return true if function parse node includes body. */
{
struct pfParse *name = func->children;
struct pfParse *input = name->next;
struct pfParse *output = input->next;
struct pfParse *body = output->next;
return (body != NULL);
}

static void insureCallsParentInit(struct pfParse *initMethod,
	struct pfBaseType *parent)
/* Make sure that class calls it's parent class's init method
 * if parent has one. */
{
struct pfParse *parentInit = parent->initMethod;
if (parentInit && funcHasBody(initMethod))
    {
    if (!rFindParentInit(initMethod))
         errAt(initMethod->tok, "Need a call to parent init here.");
    }
}

static void insureNoParentInit(struct pfBaseType *base, struct pfBaseType *parent)
/* Make sure that there are not inits in any of parents. */
{
while (parent != NULL && parent->isClass)
    {
    if (parent->initMethod)
        errAt(base->def->tok, 
		"Parent class %s has an init method, so %s must as well"
		, parent->name, base->name);
    parent = parent->parent;
    }
}


static void blessClassOrInterface(struct pfCompile *pfc, struct pfParse *pp, boolean isInterface)
/* Make sure that there are only variable , class declarations and
 * function declarations in class.  Flatten out nested declarative
 * tuples.  Add definitions to class symbol table.   */
{
struct pfParse *type = pp->children;
struct pfParse *compound = type->next;
struct pfParse *p, **p2p;
struct pfBaseType *base = pfScopeFindType(pp->scope, pp->name);

if (base == NULL)
    internalErrAt(pp->tok);
if (pfBaseIsDerivedClass(base)) 
    {
    if (base->initMethod != NULL)
	insureCallsParentInit(base->initMethod, base->parent);
    else
        insureNoParentInit(base, base->parent);
    }
pp->ty = type->ty;
p2p = &compound->children;
for (p = compound->children; p != NULL; p = p->next)
    {
    switch (p->type)
        {
	case pptNop:
	    break;
	case pptClass:
	    break;
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
	    break;
	}
    p2p = &p->next;
    }
slReverse(&base->fields);
slReverse(&base->methods);
if (base->access == paGlobal)
    {
    struct pfType *field;
    for (field = base->fields; field != NULL; field = field->next)
        {
	if (field->base->access != paGlobal)
	    errAt(pp->tok, "%s is a global type, but it's component %s isn't",
	    	base->name, field->fieldName);
	}
    }
}



static void typeConstant(struct pfCompile *pfc, struct pfParse *pp)
/* Create type for constant. */
{
struct pfToken *tok = pp->tok;
if (tok->type == pftString || tok->type == pftSubstitute)
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

static boolean hasBaseEl(struct pfType *type, struct pfBaseType *base)
/* Return TRUE if type or any of it's children has same base type as
 * pfc->elTypeFullType. */
{
if (type->base == base)
    return TRUE;
for (type = type->children; type != NULL; type = type->next)
    if (hasBaseEl(type, base))
        return TRUE;
return FALSE;
}

static struct pfType *pfTypeClone(struct pfType *orig)
/* Return clone of original type */
{
struct pfType *type = CloneVar(orig);
if (type->children != NULL)
    type->children = pfTypeClone(type->children);
if (type->next != NULL)
    type->next = pfTypeClone(type->next);
return type;
}

static void subBase(struct pfType *type, 
	struct pfBaseType *oldBase, struct pfBaseType *newBase)
/* Substitute newBase for oldBase throught type sub-tree. */
{
if (type->base == oldBase)
    type->base = newBase;
if (type->children != NULL)
    subBase(type->children, oldBase, newBase);
if (type->next != NULL)
    subBase(type->next, oldBase, newBase);
}


static void swapOutModuleDot(struct pfCompile *pfc, struct pfParse *dotPp)
/* Swap out module.something in current scope for just plain
 * something in module's scope.  The parse tree look like so on input:
 *     pptDot
 *        pptModuleUse
 *        <otherStuff>
 *            <childrenOfOther>
 * where *pPp points to pptModuleUse.
 * We transform this to be just
 *     <otherStuff>
 *         <childrenOfOther> */
{
struct pfParse *modPp = dotPp->children;
struct pfParse *otherPp = modPp->next;
struct pfParse *dotNext = dotPp->next;

/* Do some sanity checking. */
if (modPp->type != pptModuleUse)
    internalErrAt(dotPp->tok);
if (otherPp == NULL)
    internalErrAt(dotPp->tok);
if (otherPp->next != NULL)
    internalErrAt(dotPp->tok);

otherPp->parent = dotPp->parent;
*dotPp = *otherPp;
dotPp->next = dotNext;
}

static void typeDot(struct pfCompile *pfc, struct pfParse *pp)
/* Create type for dotted set of symbols. */
{
struct pfParse *classUse = pp->children;
if (classUse->type == pptModuleUse)
    {
    swapOutModuleDot(pfc, pp);
    }
else
    {
    struct pfParse *fieldUse = classUse->next;
    struct pfType *type = classUse->ty;
    struct pfBaseType *genericBase = pfc->elTypeFullType->base;
    struct pfType *fieldType;
    struct pfType *elType = type->children; /* For collections */
    if (type->base == pfc->stringType)
	{
	type = pfc->strFullType;
	}
    else if (type->base == pfc->dyStringType)
	{
	type = pfc->dyStrFullType;
	}
    else if (type->base == pfc->arrayType)
	{
	type = pfc->arrayFullType;
	}
    else if (type->base == pfc->dirType)
	{
	type = pfc->dirFullType;
	}
    if (!type->base->isClass && !type->base->isInterface)
	errAt(pp->tok, "dot after non-class variable");
    fieldType = findField(type->base, fieldUse->name);
    if (fieldType == NULL)
	errAt(pp->tok, "No field %s in class %s", fieldUse->name, 
		type->base->name);
    if (fieldType->access == paLocal)
        {
	struct pfModule *fieldModule, *myModule;
	fieldModule = type->base->scope->module;
	myModule = findEnclosingModule(pp);
	if (fieldModule != myModule)
	    errAt(fieldUse->tok, "%s.%s can't be accessed in module %s",
	    	type->base->name, fieldUse->name, myModule->name);
	}
    if (elType != NULL && hasBaseEl(fieldType, genericBase))
	{
	fieldType = pfTypeClone(fieldType);
	subBase(fieldType, genericBase, elType->base);
	}
    fieldUse->ty = fieldType;
    type = fieldType;
    pp->ty = CloneVar(type);
    pp->ty->next = NULL;
    }
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

static struct pfParse *makeVarUse(char *name, struct pfScope *scope,
	struct pfToken *tok, struct pfParse *parent)
{
struct pfParse *pp = pfParseNew(pptVarUse,  tok, parent, scope);
struct pfVar *var = findVar(scope, name, tok);
pp->var = var;
pp->ty = var->ty;
pp->name = var->name;
return pp;
}

static struct pfParse *makeSelfVarUse(struct pfBaseType *class, 
	struct pfScope *scope, struct pfParse *parent, struct pfToken *tok)
/* Create a parse node for class self variable inside a method. */
{
return makeVarUse("self", scope, tok, parent);
}

static void addDotSelf(struct pfParse **pPp, struct pfBaseType *class)
/* Add self. in front of a method or member access, positioning the
 * dot in place of self in tree, and hanging children on tree. */
{
struct pfParse *old = *pPp;
struct pfParse *dot = pfParseNew(pptDot, old->tok, old->parent, old->scope);
struct pfParse *selfPp = makeSelfVarUse(class, old->scope, dot, old->tok);

dot->ty = CloneVar(old->ty);
dot->next = old->next;
dot->children = selfPp;
selfPp->next = old;

old->parent = dot;
old->type = pptFieldUse;
old->next = NULL;
*pPp = dot;
}


static void addSelfBefore(struct pfParse **pPp, struct pfBaseType *class)
/* We are in a situation like so:
 *      pptDot
 *        pptVarUse memberName
 *        pptFieldUse memberFieldName
 * We want to transform it to
 *      pptDot
 *        pptDot
 *           pptVarUse self
 *           pptFieldUse memberName
 *        pptFieldUse memberFieldName
 * Where pPp is  &pptDot->children
 */
{
/* Get pointers to all the nodes involved. */
struct pfParse *member = *pPp;
struct pfParse *memberField = member->next;
struct pfParse *outerDot = member->parent;
struct pfParse *innerDot = pfParseNew(pptDot, member->tok, outerDot,
	member->scope);
struct pfParse *selfPp = makeSelfVarUse(class, member->scope, innerDot, 
	member->tok);

/* Flip type of member from var to field. */
member->type = pptFieldUse;

/* Fill in type field for inner dot - conveniently it's the 
 * same as the type of the old member. */
innerDot->ty = CloneVar(member->ty);

/* Rearrange tree as diagrammed above. */
outerDot->children = innerDot;
innerDot->next = memberField;
innerDot->children = selfPp;
selfPp->next = member;
member->next = NULL;
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
	if (pp->var->scope->class != NULL)
	    {
	    if (pp->parent->type == pptDot)
		{
		if (!sameString(pp->name, "self"))
		    addSelfBefore(pPp, classScope->class);
		}
	    else
		{
		addDotSelf(pPp, classScope->class);
		}
	    pp = *pPp;
	    }
	return;
        break;
    case pptAssign:
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
    switch (pp->access)
        {
	case paGlobal:
	case paLocal:
	case paStatic:
	    modifierNotAllowed(pp);
	    break;
	}
    if (pp->isConst)
	constNotAllowed(pp);
    pp->ty->fieldName = pp->name;
    }
}

static void checkToIoModifiers(struct pfParse *tuple)
/* Check for inappropriate modifiers to flow inputs (on top of those
 * done by checkIsSimpleDecTuple). */
{
struct pfParse *pp;
for (pp = tuple->children; pp != NULL; pp = pp->next)
    {
    switch (pp->access)
        {
	case paReadable:
	case paWritable:
	    modifierNotAllowed(pp);
	    break;
	}
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
struct pfParse *name = funcDec->children;
struct pfParse *input = name->next;
struct pfParse *output = input->next;
struct pfParse **pBody = &output->next;
struct pfParse *pp;
struct hashCookie hc;
struct hashEl *hel;

checkIsSimpleDecTuple(input);
if (funcDec->type == pptToDec)
    checkToIoModifiers(input);
checkIsSimpleDecTuple(output);
checkToIoModifiers(output);
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

	    a = pfParseNew(pptAssign, pp->tok, pp->parent, pp->scope);
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

static struct pfType *makeCompatableCollection(struct pfCompile *pfc, 
	struct pfParse *coll, struct pfType *elType)
/* Return a copy of collection type in most situations.  If collection
 * is a range or a function call though return an array instead. */
{
struct pfBaseType *base;
struct pfType *ty;
switch (coll->type)
    {
    case pptIndexRange:
    case pptCall:
    case pptIndirectCall:
        base = pfc->arrayType;
	break;
    default:
        base = coll->ty->base;
	break;
    }
ty = pfTypeNew(base);
ty->children = elType;
return ty;
}

static void checkPara(struct pfCompile *pfc, struct pfParse **pPp)
/* Check one of the para invocation type nodes. */
{
struct pfParse *pp = *pPp;
struct pfParse *collection = pp->children;
struct pfParse *el = collection->next;
struct pfParse *body = el->next;
struct pfBaseType *colBase = collection->ty->base;

if (colBase == pfc->stringType || colBase == pfc->dyStringType)
    errAt(collection->tok, "strings not allowed as para collections.");
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
	pp->ty = makeCompatableCollection(pfc, collection, body->ty);
	break;
	}
    case pptParaFilter:
	{
	struct pfParse **pBody = &el->next;
	enum pfParseType collType = collection->type;
	struct pfType *elTy;
        coerceToBit(pfc, pBody);
	if (collType == pptIndexRange)
	    elTy = pfTypeNew(pfc->intType);
	else
	    elTy = collection->ty->children;
	pp->ty = makeCompatableCollection(pfc, collection, elTy);
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
	break;
    }
}

void coerceSubstitute(struct pfCompile *pfc, struct pfParse **pPp)
/* Deal with strings with embedded variables, as in
*  "Hello $name!".  The parse tree on the way in looks like:
*      pptConstString 
*   On the way out it looks like
*      pptSubstitute
*         pptConstString
*         pptTuple
*            pptVarUse(s)  
*/
{
/* Get all of the parse nodes except for the varUses. */
struct pfParse *con = *pPp;
struct pfToken *tok = con->tok;
struct pfScope *scope = con->scope;
struct pfParse *sub = pfParseNew(pptSubstitute, tok, con->parent, scope);
struct pfParse *tuple = pfParseNew(pptTuple, tok, sub, scope);
struct pfParse *varList = NULL, *varPp;
char *s;

/* Put the parse tree together except, again, for the varUses. */
sub->ty = con->ty;
*pPp = sub;
sub->next = con->next;
con->parent = sub;
sub->children = con;
con->next = tuple;

/* Now loop through the actual string, looking for $'s and generating
 * varUses where you find them. */
for (s = strchr(tok->val.s, '$'); s != NULL; s = strchr(s, '$') )
    {
    char *e, c;
    char *varName;
    s += 1;
    e = s;
    c = *e;
    if (c == '(')
        {
	s += 1;
	e = strchr(e, ')');
	if (e == NULL)
	    errAt(tok, "Missing closing parenthesis after $(");
	}
    else if (c == '$')
        {
	s += 1;
	continue;	/* A $$ is just converted to $. */
	}
    else
        {
	if (!isalpha(c) && c != '_')
	    errAt(tok, "Must have a variable name after $");
	while (isalnum(c) || c == '_')
	    c = *(++e);
	}
    varName = cloneStringZ(s, e-s);
    varPp = makeVarUse(varName, scope, tok, tuple);
    if (varPp->var->ty->base != pfc->stringType)
	coerceOne(pfc, &varPp, pfTypeNew(pfc->stringType), TRUE);
    slAddHead(&varList, varPp);

    freez(&varName);
    }
slReverse(&varList);

/* Finally attatch vars to parse tree. */
tuple->children = varList;
pfTypeOnTuple(pfc, tuple);
}

static void typeOnNew(struct pfCompile *pfc, struct pfParse **pPp)
/* Make sure all is fine with a new operation.  Input
 * parse tree is:
 *    pptNew
 *       <type node>
 *       pptTuple
 *          <input-parameters>
 * We make sure that the input tuple agrees with the
 * init() function for the type, or just with the class
 * fields if there is no init.  */
{
struct pfParse *pp = *pPp;
struct pfParse *typePp = pp->children;
struct pfParse *inputPp = typePp->next;
struct pfBaseType *base = typePp->ty->base;
if (!base->isClass)
    errAt(pp->tok, "New operator only works on classes.");
coerceTupleToClass(pfc, &inputPp, base);
typePp->next = inputPp;
pp->ty = typePp->ty;
}

static void checkCatch(struct pfCompile *pfc, struct pfParse *pp)
/* Make sure that the catch exception parameter is a descendent of
 * runtimeError. */
{
struct pfParse *varInit = pp->children;
struct pfType *type = varInit->ty;
struct pfBaseType *base = type->base;
boolean ok = FALSE;
if (varInit->next != NULL)
    errAt(varInit->next->tok, "too many catch parameters");
if (base->isClass)
    {
    while (base != NULL)
        {
	if (base == pfc->seriousErrorType)
	    {
	    ok = TRUE;
	    break;
	    }
	base = base->parent;
	}
    }
if (!ok)
    errAt(varInit->tok, "Can only catch exceptions.");
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
    case pptIndirectCall:
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
#ifdef TOOHARD	/* Ripples through constant folder are nasty */
	coerceToDouble(pfc, &pp->children);
	coerceToDouble(pfc, &pp->children->next);
#endif /* TOOHARD */
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
    case pptAssign:
        coerceAssign(pfc, pp, FALSE);
	flattenAssign(pfc, pPp);
	break;
    case pptPlusEquals:
	coercePlusEquals(pfc, pp);
	break;
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
         pp->ty = pp->children->next->ty; /* Get type from val of key/val */
	 break;
    case pptConstUse:
        typeConstant(pfc, pp);
	if (pp->tok->type == pftSubstitute)
	   coerceSubstitute(pfc, pPp);
	break;
    case pptConstZero:
	if (pp->ty == NULL)
	    pp->ty = pfTypeNew(pfc->intType);
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
    case pptNew:
        typeOnNew(pfc, pPp);
	break;
    case pptCaseList:
    case pptCaseItem:
        break;	/* We check these all in the higher level pptCase */
    case pptCase:
	coerceCase(pfc, pp);
        break;
    case pptCatch:
        checkCatch(pfc, pp);
	break;
    default:
        break;
    }
}

static void rFillInClassPrefix(struct pfParse *pp, struct dyString *dy)
/* Recurse mostly just to reverse order, so parent appears first... */
{
if (pp->parent)
    rFillInClassPrefix(pp->parent, dy);
switch (pp->type)
    {
    case pptModule:
    case pptMainModule:
    case pptModuleRef:
	if (pp->name[0] != '<')
	    {
	    dyStringAppendC(dy, '_');
	    dyStringAppend(dy, mangledModuleName(pp->name));
	    }
        break;
    case pptClass:
    case pptInterface:
	dyStringAppendC(dy, '_');
        dyStringAppend(dy, pp->name);
        break;
    }
}

static void fillInClassMethodsPrefixAndCName(struct pfCompile *pfc, struct pfParse *class)
/* Fill in prefix to user for class methods. */
{
struct dyString *methodPrefix = dyStringNew(0);
struct dyString *cName = dyStringNew(0);
dyStringAppend(methodPrefix, "_pf_cm");
dyStringAppend(cName, "pf");
rFillInClassPrefix(class, methodPrefix);
rFillInClassPrefix(class, cName);
class->ty->base->methodPrefix = cloneString(methodPrefix->string);
class->ty->base->cName = cloneString(cName->string);
dyStringFree(&methodPrefix);
dyStringFree(&cName);
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
    case pptInterface:
        blessClassOrInterface(pfc, pp, FALSE);
	fillInClassMethodsPrefixAndCName(pfc, pp);
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
