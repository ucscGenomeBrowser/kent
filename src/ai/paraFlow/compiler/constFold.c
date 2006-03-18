/* This module turns expressions involving constants into just
 * simple constants.  */

#include "common.h"
#include "dyString.h"
#include "pfParse.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "constFold.h"

static struct pfToken *fakeToken(struct pfToken *old, 
	enum pfTokType type, union pfTokVal *val)
/* Create a fake token. */
{
struct pfToken *tok = CloneVar(old);
tok->type = type;
tok->val = *val;
return tok;
}

static void ensureConst(struct pfParse *pp)
/* Make sure that pp is really a const of some sort. */
{
switch (pp->type)
    {
    case pptConstBit:
    case pptConstByte:
    case pptConstShort:
    case pptConstInt:
    case pptConstLong:
    case pptConstFloat:
    case pptConstDouble:
    case pptConstChar:
    case pptConstString:
        break;
    default:
        internalErrAt(pp->tok);
	break;
    }
}


static void foldUnaryOp(struct pfCompile *pfc, struct pfParse *pp)
/* Fold ! ~ - */
{
struct pfParse *constPp = pp->children;
struct pfToken *constTok = constPp->tok;
union pfTokVal val, oldVal = constTok->val;
enum pfTokType tokType = constTok->type;
enum pfParseType ppType = constPp->type;

switch (pp->type)
    {
    case pptNegate:
	switch (tokType)
	    {
	    case pftInt:
		val.i = -oldVal.i;
		break;
	    case pftLong:
		val.l = -oldVal.l;
		break;
	    case pftFloat:
		val.x = -oldVal.x;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
        break;
    case pptNot:
	switch (tokType)
	    {
	    case pftInt:
		val.i = !oldVal.i;
		break;
	    case pftNil:
	         val.i = 1;
		 tokType = pftInt;
		 ppType = pptConstInt;
		 break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
        break;
    case pptFlipBits:
	switch (tokType)
	    {
	    case pftInt:
		val.i = ~oldVal.i;
		break;
	    case pftLong:
		val.l = ~oldVal.l;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
        break;
    }
pp->children = NULL;
pp->type = ppType;
pp->tok = fakeToken(pp->tok, tokType, &val);
}


static void foldBinaryOp(struct pfCompile *pfc, struct pfParse *pp)
/* Fold some sort of binary operation.  */
{
struct pfParse *left = pp->children;
struct pfParse *right = left->next;
struct pfToken *leftTok = left->tok;
struct pfToken *rightTok = right->tok;
enum pfTokType tokType = leftTok->type;
enum pfParseType ppType = left->type;
enum pfTokType rightTokType = rightTok->type;
union pfTokVal val;

/* Just do some sanity checking here to make sure that
 * both token types are compatable. */
switch (tokType)
    {
    case pftString:
    case pftSubstitute:
	if (rightTokType != pftString && rightTokType != pftSubstitute)
	    internalErrAt(pp->tok);
        break;
    default:
        if (tokType != rightTokType)
	    internalErrAt(pp->tok);
	break;
    }

/* More sanity checking, that both sides really are constants */
ensureConst(left);
ensureConst(right);

switch (pp->type)
    {
    case pptPlus:
        switch (tokType)
	    {
	    case pftInt:
		val.i = leftTok->val.i + rightTok->val.i;
		break;
	    case pftLong:
		val.l = leftTok->val.l + rightTok->val.l;
		break;
	    case pftFloat:
		val.x = leftTok->val.x + rightTok->val.x;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
	break;
    case pptMinus:
        switch (tokType)
	    {
	    case pftInt:
		val.i = leftTok->val.i - rightTok->val.i;
		break;
	    case pftLong:
		val.l = leftTok->val.l - rightTok->val.l;
		break;
	    case pftFloat:
		val.x = leftTok->val.x - rightTok->val.x;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
	break;
    case pptMul:
        switch (tokType)
	    {
	    case pftInt:
		val.i = leftTok->val.i * rightTok->val.i;
		break;
	    case pftLong:
		val.l = leftTok->val.l * rightTok->val.l;
		break;
	    case pftFloat:
		val.x = leftTok->val.x * rightTok->val.x;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
	break;
    case pptDiv:
        switch (tokType)
	    {
	    case pftInt:
		val.i = leftTok->val.i / rightTok->val.i;
		break;
	    case pftLong:
		val.l = leftTok->val.l / rightTok->val.l;
		break;
	    case pftFloat:
		val.x = leftTok->val.x / rightTok->val.x;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
	break;
    case pptMod:
        switch (tokType)
	    {
	    case pftInt:
		val.i = leftTok->val.i % rightTok->val.i;
		if (val.i < 0) val.i += rightTok->val.i;
		break;
	    case pftLong:
		val.l = leftTok->val.l % rightTok->val.l;
		if (val.l < 0) val.l += rightTok->val.l;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
	break;
    case pptBitAnd:
	switch (tokType)
	    {
	    case pftInt:
		val.i = leftTok->val.i & rightTok->val.i;
		break;
	    case pftLong:
		val.l = leftTok->val.l & rightTok->val.l;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
	break;
    case pptBitOr:
	switch (tokType)
	    {
	    case pftInt:
		val.i = leftTok->val.i | rightTok->val.i;
		break;
	    case pftLong:
		val.l = leftTok->val.l | rightTok->val.l;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
	break;
    case pptBitXor:
	switch (tokType)
	    {
	    case pftInt:
		val.i = leftTok->val.i ^ rightTok->val.i;
		break;
	    case pftLong:
		val.l = leftTok->val.l ^ rightTok->val.l;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
	break;
    case pptShiftLeft:
	switch (tokType)
	    {
	    case pftInt:
		val.i = leftTok->val.i << rightTok->val.i;
		break;
	    case pftLong:
		val.l = leftTok->val.l << rightTok->val.l;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
	break;
    case pptShiftRight:
	switch (tokType)
	    {
	    case pftInt:
		val.i = leftTok->val.i >> rightTok->val.i;
		break;
	    case pftLong:
		val.l = leftTok->val.l >> rightTok->val.l;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
	break;
    case pptLogAnd:
        switch (tokType)
	    {
	    case pftInt:
	        val.i = (leftTok->val.i && rightTok->val.i);
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
	break;
    case pptLogOr:
        switch (tokType)
	    {
	    case pftInt:
	        val.i = (leftTok->val.i || rightTok->val.i);
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
	break;
    case pptGreater:
        switch (tokType)
	    {
	    case pftInt:
	        val.i = (leftTok->val.i > rightTok->val.i);
		break;
	    case pftLong:
	        val.i = (leftTok->val.l > rightTok->val.l);
		break;
	    case pftFloat:
	        val.i = (leftTok->val.x > rightTok->val.x);
		break;
	    case pftString:
	        val.i = strcmp(leftTok->val.s, rightTok->val.s) > 0;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
         break;
    case pptGreaterOrEquals:
        switch (tokType)
	    {
	    case pftInt:
	        val.i = (leftTok->val.i >= rightTok->val.i);
		break;
	    case pftLong:
	        val.i = (leftTok->val.l >= rightTok->val.l);
		break;
	    case pftFloat:
	        val.i = (leftTok->val.x >= rightTok->val.x);
		break;
	    case pftString:
	        val.i = strcmp(leftTok->val.s, rightTok->val.s) >= 0;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
         break;
    case pptLessOrEquals:
        switch (tokType)
	    {
	    case pftInt:
	        val.i = (leftTok->val.i <= rightTok->val.i);
		break;
	    case pftLong:
	        val.i = (leftTok->val.l <= rightTok->val.l);
		break;
	    case pftFloat:
	        val.i = (leftTok->val.x <= rightTok->val.x);
		break;
	    case pftString:
	        val.i = strcmp(leftTok->val.s, rightTok->val.s) <= 0;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
        break;
    case pptLess:
        switch (tokType)
	    {
	    case pftInt:
	        val.i = (leftTok->val.i < rightTok->val.i);
		break;
	    case pftLong:
	        val.i = (leftTok->val.l < rightTok->val.l);
		break;
	    case pftFloat:
	        val.i = (leftTok->val.x < rightTok->val.x);
		break;
	    case pftString:
	        val.i = strcmp(leftTok->val.s, rightTok->val.s) < 0;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
        break;
    case pptSame:
        switch (tokType)
	    {
	    case pftInt:
	        val.i = (leftTok->val.i == rightTok->val.i);
		break;
	    case pftLong:
	        val.i = (leftTok->val.l == rightTok->val.l);
		break;
	    case pftFloat:
	        val.i = (leftTok->val.x == rightTok->val.x);
		break;
	    case pftString:
	        val.i = strcmp(leftTok->val.s, rightTok->val.s) == 0;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
        break;
    case pptNotSame:
        switch (tokType)
	    {
	    case pftInt:
	        val.i = (leftTok->val.i != rightTok->val.i);
		break;
	    case pftLong:
	        val.i = (leftTok->val.l != rightTok->val.l);
		break;
	    case pftFloat:
	        val.i = (leftTok->val.x != rightTok->val.x);
		break;
	    case pftString:
	        val.i = strcmp(leftTok->val.s, rightTok->val.s) != 0;
		break;
	    default:
	        internalErrAt(pp->tok);
		break;
	    }
        break;
    default:
	internalErrAt(pp->tok);
        break;
    }

switch (pp->type)
    {
    case pptGreater:
    case pptGreaterOrEquals:
    case pptLessOrEquals:
    case pptLess:
    case pptSame:
    case pptNotSame:
	tokType = pftInt;
	ppType = pptConstBit;
	break;
    }

pp->children = NULL;
pp->type = ppType;
pp->tok = fakeToken(pp->tok, tokType, &val);
}

static void foldStringCat(struct pfCompile *pfc, struct pfParse *pp)
/* Fold together string concatenation */
{
struct pfParse *p;
struct dyString *dy = dyStringNew(0);
union pfTokVal val;
for (p = pp->children; p != NULL; p = p->next)
    dyStringAppend(dy, p->tok->val.s);
val.s = cloneString(dy->string);
dyStringFree(&dy);
pp->tok = fakeToken(pp->tok, pftString, &val);
pp->type = pptConstString;
pp->children = NULL;
}

static char *longToString(long l)
/* Return long value as string */
{
char buf[32];
safef(buf, sizeof(buf), "%ld", l);
return cloneString(buf);
}

static char *doubleToString(double d)
/* Return double value as string */
{
char buf[32];
safef(buf, sizeof(buf), "%3.2f", d);
return cloneString(buf);
}

static void foldCast(struct pfCompile *pfc, struct pfParse *pp)
/* Fold down cast of constant. */
{
struct pfBaseType *base = pp->ty->base;
struct pfParse *constPp = pp->children;
struct pfToken *constTok = constPp->tok;
enum pfParseType ppType;
enum pfTokType tokType;
union pfTokVal oldVal =constTok->val;
union pfTokVal val = oldVal;

switch (pp->type)
    {
    case pptCastBitToByte:
    case pptCastBitToChar:
    case pptCastBitToShort:
    case pptCastBitToInt:
    case pptCastByteToChar:
    case pptCastByteToShort:
    case pptCastByteToInt:
    case pptCastCharToByte:
    case pptCastCharToShort:
    case pptCastCharToInt:
    case pptCastShortToInt:
	/* Effectively int-to-int conversion */
        break;

    case pptCastByteToBit:
    case pptCastCharToBit:
    case pptCastShortToBit:
    case pptCastIntToBit:
        val.i = (oldVal.i == 0 ? 0 : 1);
	break;

    case pptCastShortToByte:
    case pptCastIntToByte:
    case pptCastShortToChar:
    case pptCastIntToChar:
        if (val.i < -128 || val.i > 127)
	    errAt(constTok, "Byte overflow %d", val.i);
	break;

    case pptCastIntToShort:
        if (val.i < -32768 || val.i > 32767)
	    errAt(constTok, "Short overflow %d", val.i);
	break;

    case pptCastBitToLong:
    case pptCastByteToLong:
    case pptCastCharToLong:
    case pptCastShortToLong:
    case pptCastIntToLong:
        /* Effectively int-to-long conversion */
	val.l = oldVal.i;
	break;

    case pptCastBitToFloat:
    case pptCastBitToDouble:
    case pptCastByteToFloat:
    case pptCastByteToDouble:
    case pptCastShortToFloat:
    case pptCastShortToDouble:
    case pptCastCharToFloat:
    case pptCastCharToDouble:
    case pptCastIntToFloat:
    case pptCastIntToDouble:
        /* Effectively int-to-double conversion */
	val.x = oldVal.i;
	break;

    case pptCastBitToString:
    case pptCastByteToString:
    case pptCastShortToString:
    case pptCastIntToString:
        /* Effectively int-to-string conversion */
	val.s = longToString(oldVal.i);
	break;

    case pptCastCharToString:
	internalErrAt(pp->tok);	/* I don't think this happens with constants. */
	break;

    case pptCastLongToBit:
        val.i = (oldVal.l == 0 ? 0 : 1);
	break;

    case pptCastLongToByte:
    case pptCastLongToChar:
        if (val.l < -128 || val.l > 127)
	    errAt(constTok, "Byte overflow %d", val.l);
	val.i = val.l;
	break;

    case pptCastLongToShort:
        if (val.l < -32768 || val.l > 32767)
	    errAt(constTok, "Short overflow %d", val.l);
	val.i = val.l;
	break;

    case pptCastLongToInt:
        if (val.l < -2147483647 || val.l > 2147483647)
	    errAt(constTok, "Int overflow %d", val.l);
	val.i = val.l;
	break;

    case pptCastLongToFloat:
    case pptCastLongToDouble:
	val.x = oldVal.l;
	break;

    case pptCastLongToString:
	val.s = longToString(oldVal.l);
	break;

    case pptCastFloatToBit:
    case pptCastDoubleToBit:
        val.i = (val.x == 0.0 ? 0 : 1);
	break;

    case pptCastFloatToByte:
    case pptCastFloatToChar:
    case pptCastDoubleToByte:
    case pptCastDoubleToChar:
        if (val.x < -128 || val.x > 127)
	    errAt(constTok, "Byte overflow %d", val.x);
	val.i = val.x;
	break;

    case pptCastFloatToShort:
    case pptCastDoubleToShort:
        if (val.x < -32768 || val.x > 32767)
	    errAt(constTok, "Short overflow %d", val.x);
	val.i = val.x;
	break;

    case pptCastFloatToInt:
    case pptCastDoubleToInt:
        if (val.x < -2147483647 || val.x > 2147483647)
	    errAt(constTok, "Int overflow %d", val.x);
	val.i = val.x;
	break;

    case pptCastFloatToLong:
    case pptCastDoubleToLong:
	val.l = val.x;
	break;

    case pptCastFloatToDouble:
    case pptCastDoubleToFloat:
        break;

    case pptCastFloatToString:
    case pptCastDoubleToString:
	val.s = doubleToString(oldVal.x);
        break;

    case pptCastStringToBit:
        val.i = (val.s[0] == 0 ? 0 : 1);
	break;
    }


if (base == pfc->bitType)
    {
    ppType = pptConstBit;
    tokType = pftInt;
    }
else if (base == pfc->byteType)
    {
    ppType = pptConstByte;
    tokType = pftInt;
    }
else if (base == pfc->shortType)
    {
    ppType = pptConstShort;
    tokType = pftInt;
    }
else if (base == pfc->intType)
    {
    ppType = pptConstInt;
    tokType = pftInt;
    }
else if (base == pfc->longType)
    {
    ppType = pptConstLong;
    tokType = pftLong;
    }
else if (base == pfc->floatType)
    {
    ppType = pptConstFloat;
    tokType = pftFloat;
    }
else if (base == pfc->doubleType)
    {
    ppType = pptConstDouble;
    tokType = pftFloat;
    }
else if (base == pfc->stringType)
    {
    ppType = pptConstString;
    tokType = pftString;
    }
else
    {
    internalErrAt(pp->tok);
    ppType = 0;
    tokType = 0;
    }
pp->tok = fakeToken(pp->tok, tokType, &val);
pp->children = NULL;
pp->type = ppType;
}

static void flipVarUseToConst(struct pfCompile *pfc, struct pfParse *pp)
/* Convert named constant to a straight constant. */
{
struct pfParse *init = pp->var->parse->children->next->next;
struct pfParse *parent = pp->parent, *next = pp->next;
memcpy(pp, init, sizeof(*pp));
pp->parent = parent;
pp->next = next;
}

static boolean rConstFold(struct pfCompile *pfc, struct pfParse *pp)
/* Returns FALSE if there's a non-constant underneath self in tree. */
{
struct pfParse *p;
boolean isConst = TRUE;
for (p = pp->children; p != NULL; p = p->next)
    if (!rConstFold(pfc, p))
        isConst = FALSE;
if (isConst)
    {
    switch (pp->type)
        {
	case pptStringCat:
	    foldStringCat(pfc, pp);
	    break;

	case pptNegate:
	case pptNot:
	case pptFlipBits:
	    foldUnaryOp(pfc, pp);
	    break;

	case pptPlus:
	case pptMinus:
	case pptMul:
	case pptDiv:
	case pptMod:
	case pptGreater:
	case pptGreaterOrEquals:
	case pptSame:
	case pptNotSame:
	case pptLess:
	case pptLessOrEquals:
	case pptLogAnd:
	case pptLogOr:
	case pptBitAnd:
	case pptBitOr:
	case pptBitXor:
	case pptShiftLeft:
	case pptShiftRight:
	    foldBinaryOp(pfc, pp);
	    break;

	case pptCastBitToByte:
	case pptCastBitToChar:
	case pptCastBitToShort:
	case pptCastBitToInt:
	case pptCastBitToLong:
	case pptCastBitToFloat:
	case pptCastBitToDouble:
	case pptCastBitToString:
	case pptCastByteToBit:
	case pptCastByteToChar:
	case pptCastByteToShort:
	case pptCastByteToInt:
	case pptCastByteToLong:
	case pptCastByteToFloat:
	case pptCastByteToDouble:
	case pptCastByteToString:
	case pptCastCharToBit:
	case pptCastCharToByte:
	case pptCastCharToShort:
	case pptCastCharToInt:
	case pptCastCharToLong:
	case pptCastCharToFloat:
	case pptCastCharToDouble:
	case pptCastCharToString:
	case pptCastShortToBit:
	case pptCastShortToByte:
	case pptCastShortToChar:
	case pptCastShortToInt:
	case pptCastShortToLong:
	case pptCastShortToFloat:
	case pptCastShortToDouble:
	case pptCastShortToString:
	case pptCastIntToBit:
	case pptCastIntToByte:
	case pptCastIntToChar:
	case pptCastIntToShort:
	case pptCastIntToLong:
	case pptCastIntToFloat:
	case pptCastIntToDouble:
	case pptCastIntToString:
	case pptCastLongToBit:
	case pptCastLongToByte:
	case pptCastLongToChar:
	case pptCastLongToShort:
	case pptCastLongToInt:
	case pptCastLongToFloat:
	case pptCastLongToDouble:
	case pptCastLongToString:
	case pptCastFloatToBit:
	case pptCastFloatToByte:
	case pptCastFloatToChar:
	case pptCastFloatToShort:
	case pptCastFloatToInt:
	case pptCastFloatToLong:
	case pptCastFloatToDouble:
	case pptCastFloatToString:
	case pptCastDoubleToBit:
	case pptCastDoubleToByte:
	case pptCastDoubleToChar:
	case pptCastDoubleToShort:
	case pptCastDoubleToInt:
	case pptCastDoubleToLong:
	case pptCastDoubleToFloat:
	case pptCastDoubleToString:
	case pptCastStringToBit:
	    foldCast(pfc, pp);
	    break;

	case pptVarUse:
	    if (pp->var->constFolded)
		flipVarUseToConst(pfc, pp);
	    else
		isConst = FALSE;
	    break;
	case pptPlaceholder:
	    isConst = FALSE;
	    break;
	}
    }
return isConst;
}

static void rGetConstants(struct pfParse *pp, struct slRef **pRefs)
/* Get a list of all constants onto pRefs. */
{
if (pp->type == pptVarInit)
    {
    struct pfVar *var = pp->var;
    if (var->ty->isConst)
	refAdd(pRefs, var);
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rGetConstants(pp, pRefs);
}

static boolean anyUnfoldedConstants(struct pfParse *pp)
/* Return TRUE if no unfolded constants. */
{
if (pp->type == pptVarUse)
    {
    struct pfVar *var = pp->var;
    if (!var->constFolded)
        return TRUE;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    if (anyUnfoldedConstants(pp))
        return TRUE;
return FALSE;
}

static void findConstVals(struct pfCompile *pfc, struct pfParse *pp)
/* Fold down all named constants. */
{
struct slRef *refList = NULL, *ref, *newList, *nextRef;
rGetConstants(pp, &refList);

while (refList != NULL)
    {
    int resolveCount = 0;
    newList = NULL;
    for (ref = refList; ref != NULL; ref = nextRef)
	{
	struct pfVar *var = ref->val;
	struct pfParse *init = var->parse->children->next->next;
	nextRef = ref->next;
	if (!anyUnfoldedConstants(init))
	    {
	    rConstFold(pfc, init);
	    var->constFolded = TRUE;
	    freeMem(ref);
	    ++resolveCount;
	    }
	else
	    {
	    slAddHead(&newList, ref);
	    }
	}
    if (resolveCount == 0)
	{
	struct pfVar *var = refList->val;
	errAt(var->parse->tok, 
	    "Circular definition of constant %s.", var->name);
	}
    refList = newList;
    }
}

void pfConstFold(struct pfCompile *pfc, struct pfParse *pp)
/* Fold constants into simple expressions. */
{
findConstVals(pfc,pp);
rConstFold(pfc, pp);
}
