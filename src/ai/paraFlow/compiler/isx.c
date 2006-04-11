/* isx - intermediate sequenced executable -  the paraFlow
 * intermediate code.  A fairly standard "three address code"
 * that is a lower level representation than the parse tree but
 * higher level than actual machine code.  The format is
 *    opCode dest left right
 * which is interpreted as 
 *    dest = left opCode right
 * so
 *    + x 3 8		represents x = 3 + 8
 *    * t1 t2 t3	represents t1 = t2*t3
 *    = x y		represents x = y
 *    - u v             represents u = -v
 */

#include "common.h"
#include "hash.h"
#include "dlist.h"
#include "pfParse.h"
#include "pfToken.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfCompile.h"
#include "ctar.h"
#include "isxLiveVar.h"
#include "isx.h"

static struct isxAddress *isxExpression(struct pfCompile *pfc, 
	struct pfParse *pp, struct hash *varHash, 
	double weight, struct dlList *iList);
/* Generate intermediate code for expression. Return destination */

char *isxValTypeToString(enum isxValType val)
/* Convert isxValType to string. */
{
switch (val)
    {
    case ivZero:
	return "zer";
    case ivBit:
	return "bit";
    case ivByte:
	return "byt";
    case ivShort:
	return "sho";
    case ivInt:
	return "int";
    case ivLong:
	return "lng";
    case ivFloat:
	return "flt";
    case ivDouble:
	return "dbl";
    case ivObject:
	return "obj";
    case ivString:
	return "str";
    case ivJump:
	return "lab";
    default:
        internalErr();
	return NULL;
    }
}

char *isxOpTypeToString(enum isxOpType val)
/* Convert isxValType to string. */
{
switch (val)
    {
    case poInit:
        return "poInit";
    case poAssign:
        return "poAssign";
    case poPlus:
	return "poPlus";
    case poMinus:
	return "poMinus";
    case poMul:
	return "poMul";
    case poDiv:
	return "poDiv";
    case poMod:
	return "poMod";
    case poBitAnd:
	return "poBitAnd";
    case poBitOr:
	return "poBitOr";
    case poBitXor:
	return "poBitXor";
    case poShiftLeft:
	return "poShiftLeft";
    case poShiftRight:
	return "poShiftRight";
    case poNegate:
	return "poNegate";
    case poFlipBits:
	return "poFlipBits";
    case poInput:
        return "poInput";
    case poCall:
	return "poCall";
    case poOutput:
        return "poOutput";
    case poLabel:
	return "poLabel";
    case poLoopStart:
        return "poLoopStart";
    case poLoopEnd:
        return "poLoopEnd";
    case poCondEnd:
        return "poCondEnd";
    case poCondStart:
        return "poCondStart";
    case poCondCase:
        return "poCondCase";
    case poJump:
	return "poJump";
    case poBeq:
	return "poBeq";
    case poBne:
	return "poBne";
    case poBlt:
	return "poBlt";
    case poBle:
	return "poBle";
    case poBgt:
	return "poBgt";
    case poBge:
	return "poBge";
    case poBz:
	return "poBz";
    case poBnz:
	return "poBnz";
    case poFuncStart:
        return "poFuncStart";
    case poFuncEnd:
        return "poFuncEnd";
    case poReturnVal:
        return "poReturnVal";
    case poCast:
        return "poCast";
    default:
        internalErr();
	return NULL;
    }
}

char *isxRegName(struct isxReg *reg, enum isxValType valType)
/* Get name of reg for given type. */
{
switch (valType)
    {
    case ivBit:
    case ivByte:
	return reg->byteName;
    case ivShort:
	return reg->shortName;
    case ivInt:
	return reg->intName;
    case ivLong:
	return reg->longName;
    case ivFloat:
	return reg->floatName;
    case ivDouble:
	return reg->doubleName;
    case ivObject:
    case ivString:
	return reg->pointerName;
    default:
	internalErr();
	return NULL;
    }
}

void isxAddressDump(struct isxAddress *iad, FILE *f)
/* Print variable name or n/a */
{
fprintf(f, "%s:", isxValTypeToString(iad->valType));
/* Convert isxValType to string. */
switch (iad->adType)
    {
    case iadZero:
	fprintf(f, "#zero");
	break;
    case iadConst:
	{
	struct pfToken *tok = iad->val.tok;
	union pfTokVal val = tok->val;
	switch(tok->type)
	    {
	    case pftInt:
		fprintf(f, "#%d", val.i);
		break;
	    case pftLong:
		fprintf(f, "#%lld", val.l);
		break;
	    case pftFloat:
		fprintf(f, "#%f", val.x);
		break;
	    case pftString:
		fprintf(f, "#'%s'", val.s);
		break;
	    default:
	        internalErr();
		break;
	    }
	break;
	}
    case iadRealVar:
    case iadTempVar:
	fprintf(f, "%s*%2.1f", iad->name, iad->weight);
	break;
    case iadInStack:
	fprintf(f, "in(%d)", iad->val.ioOffset);
        break;
    case iadOutStack:
	fprintf(f, "out(%d)", iad->val.ioOffset);
        break;
    case iadReturnVar:
	fprintf(f, "ret(%d)", iad->val.ioOffset);
        break;
    case iadOperator:
    case iadCtar:
	fprintf(f, "#%s", iad->name);
	break;
    case iadLabel:
        fprintf(f, "@%s", iad->name);
	break;
    case iadLoopInfo:
	{
	struct isxLoopInfo *loopy = iad->val.loopy;
	struct isxLoopVar *lv;
        fprintf(f, "~%dx%d", loopy->iteration, 
		loopy->instructionCount);
	if (loopy->children == NULL)
	     fprintf(f, "[inner]");
	fprintf(f, "[");
	for (lv = loopy->hotLive; lv != NULL; lv = lv->next)
	    {
	    fprintf(f, "%s*%2.1f", lv->iad->name, lv->iad->weight);
	    if (lv->reg != NULL)
	        fprintf(f, "@%s", isxRegName(lv->reg, lv->iad->valType));
	    if (lv->next != NULL)
	        fprintf(f, ",");
	    }
	fprintf(f, "]");
	}
	break;
    default:
        internalErr();
	break;
    }
if (iad->reg != NULL)
    fprintf(f, "@%s", isxRegName(iad->reg, iad->valType));
}

void isxDump(struct isx *isx, FILE *f)
/* Dump out isx code */
{
fprintf(f, "%s ", isxOpTypeToString(isx->opType));
if (isx->dest != NULL)
    {
    fprintf(f, " ");
    isxAddressDump(isx->dest, f);
    }
fprintf(f, " =");
if (isx->left != NULL)
    {
    fprintf(f, " ");
    isxAddressDump(isx->left, f);
    }
if (isx->right != NULL)
    {
    fprintf(f, " ");
    isxAddressDump(isx->right, f);
    }
if (isx->liveList != NULL)
    {
    struct isxLiveVar *live;
    fprintf(f, "\t{");
    for (live = isx->liveList; live != NULL; live = live->next)
        {
	struct isxAddress *iad = live->var;
	fprintf(f, "%s", iad->name);
	fprintf(f, ":%d", live->useCount);
	fprintf(f, "@%d", live->usePos[0]);
	if (live->useCount > 1)
	    fprintf(f, "@%d", live->usePos[1]);
	if (iad->reg != NULL)
	    fprintf(f, "~%s", isxRegName(iad->reg, iad->valType));
	if (live->next != NULL)
	    fprintf(f, ",");
	}
    fprintf(f, "}");
    }
}

void isxDumpList(struct dlList *list, FILE *f)
/* Dump all of isx in list */
{
struct dlNode *node;
for (node = list->head; !dlEnd(node); node = node->next)
    {
    isxDump(node->val, f);
    fprintf(f, "\n");
    }
}

struct isx *isxNew(struct pfCompile *pfc, enum isxOpType opType,
	struct isxAddress *dest, struct isxAddress *left,
	struct isxAddress *right)
/* Make new isx instruction. */
{
struct isx *isx;
AllocVar(isx);
isx->opType = opType;
isx->dest = dest;
isx->left = left;
isx->right = right;
return isx;
}

struct isx *isxAddNew(struct pfCompile *pfc, enum isxOpType opType,
	struct isxAddress *dest, struct isxAddress *left,
	struct isxAddress *right, struct dlList *iList)
/* Make new isx instruction, and hang it on iList */
{
struct isx *isx = isxNew(pfc, opType, dest, left, right);;
dlAddValTail(iList, isx);
return isx;
}

enum isxValType isxValTypeFromTy(struct pfCompile *pfc, struct pfType *ty)
/* Return isxValType corresponding to pfType  */
{
struct pfBaseType *base = ty->base;
if (base == pfc->bitType)
    return ivBit;
else if (base == pfc->byteType)
    return ivByte;
else if (base == pfc->shortType)
    return ivShort;
else if (base == pfc->intType)
    return ivInt;
else if (base == pfc->longType)
    return ivLong;
else if (base == pfc->floatType)
    return ivFloat;
else if (base == pfc->doubleType)
    return ivDouble;
else if (base == pfc->stringType || base == pfc->dyStringType)
    return ivString;
else
    return ivObject;
}

static enum isxValType ppToIsxValType(struct pfCompile *pfc, 
	struct pfParse *pp)
/* Return isxValType corresponding to pp */
{
return isxValTypeFromTy(pfc, pp->ty);
}

struct isxAddress *isxConstAddress(struct pfToken *tok, 
	enum isxValType valType)
/* Get place to put constant. */
{
struct isxAddress *iad;
AllocVar(iad);
iad->adType = iadConst;
iad->valType = valType;
iad->val.tok = tok;
return iad;
}

static struct isxAddress *zeroAddress()
/* Get place representing zero or nil. */
{
static struct isxAddress zero;
return &zero;
}

struct isxAddress *isxTempVarAddress(struct pfCompile *pfc, struct hash *hash,
	double weight, enum isxValType valType)
/* Create a new temporary var */
{
struct isxAddress *iad;
char buf[18];
safef(buf, sizeof(buf), "$t%X", ++(pfc->tempLabelMaker));
AllocVar(iad);
iad->adType = iadTempVar;
iad->valType = valType;
iad->weight = weight;
hashAddSaveName(hash, buf, iad, &iad->name);
return iad;
}


static struct isxAddress *varAddress(struct pfVar *var, struct hash *hash,
	double weight, enum isxValType valType)
/* Create reference to a real var */
{
struct isxAddress *iad = hashFindVal(hash, var->cName);
if (iad == NULL)
    {
    AllocVar(iad);
    iad->adType = iadRealVar;
    iad->valType = valType;
    iad->val.var = var;
    hashAddSaveName(hash, var->cName, iad, &iad->name);
    }
iad->weight += weight;
return iad;
}

struct isxAddress *isxCallAddress(char *name)
/* Create reference to a function */
{
struct isxAddress *iad;
AllocVar(iad);
iad->adType = iadOperator;
iad->name = name;
return iad;
}


struct isxAddress *isxIoAddress(int offset, enum isxValType valType,
	enum isxAddressType adType)
/* Return reference to an io stack address */
{
struct isxAddress *iad;
AllocVar(iad);
iad->adType = adType;
iad->valType = valType;
iad->val.ioOffset = offset;
return iad;
}

static struct isxAddress *operatorAddress(char *name)
/* Create reference to a build-in operator call */
{
struct isxAddress *iad;
AllocVar(iad);
iad->adType = iadOperator;
iad->valType = ivJump;
iad->name = name;
return iad;
}

struct isxAddress *isxTempLabelAddress(struct pfCompile *pfc)
/* Create reference to a temporary label for jumping to. */
{
struct isxAddress *iad;
char buf[18];
safef(buf, sizeof(buf), "L%d", ++pfc->isxLabelMaker);
AllocVar(iad);
iad->adType = iadLabel;
iad->valType = ivJump;
iad->name = cloneString(buf);
return iad;
}

static struct isxAddress *isxBinaryOp(struct pfCompile *pfc, 
	struct pfParse *pp, struct hash *varHash, 
	enum isxOpType op, double weight, struct dlList *iList)
/* Generate intermediate code for expression. Return destination */
{
struct isxAddress *left = isxExpression(pfc, pp->children,
	varHash, weight, iList);
struct isxAddress *right = isxExpression(pfc, pp->children->next,
	varHash, weight, iList);
struct isxAddress *dest = isxTempVarAddress(pfc, varHash, weight, ppToIsxValType(pfc,pp));
isxAddNew(pfc, op, dest, left, right, iList);
return dest;
}

static struct isxAddress *isxUnaryOp(struct pfCompile *pfc, 
	struct pfParse *pp, struct hash *varHash, 
	enum isxOpType op, double weight, struct dlList *iList)
/* Generate intermediate code for unary expression. Return destination */
{
struct isxAddress *target = isxExpression(pfc, pp->children,
	varHash, weight, iList);
struct isxAddress *dest = isxTempVarAddress(pfc, varHash, weight, 
	ppToIsxValType(pfc,pp));
isxAddNew(pfc, op, dest, target, NULL, iList);
return dest;
}

static void isxOpEquals(struct pfCompile *pfc, 
	struct pfParse *pp, struct hash *varHash, 
	enum isxOpType op, double weight, struct dlList *iList)
{
struct pfParse *use = pp->children;
struct isxAddress *exp = isxExpression(pfc, use->next,
	varHash, weight, iList);
struct isxAddress *dest = varAddress(use->var, varHash, 
	weight, ppToIsxValType(pfc, use));
isxAddNew(pfc, op, dest, dest, exp, iList);
}

static struct isxAddress *isxStringCat(struct pfCompile *pfc,
	struct pfParse *pp, struct hash *varHash, double weight,
	struct dlList *iList)
/* Create string cat op */
{
uglyAbort("Can't handle stringCat yet");
return NULL;
}

static struct isxAddress *isxCall(struct pfCompile *pfc,
	struct pfParse *pp, struct hash *varHash, double weight,
	struct dlList *iList)
/* Create call op */
{
struct pfParse *function = pp->children;
struct pfParse *inTuple = function->next;
struct pfParse *p;
struct pfType *outTuple = function->ty->children->next, *ty;
struct isxAddress *source, *dest, *destList = NULL;
int offset;

for (p = inTuple->children, offset=0; p != NULL; p = p->next, ++offset)
    {
    for (source = isxExpression(pfc, p, varHash, weight, iList);
    	source != NULL; source = source->next)
	{
	dest = isxIoAddress(offset, source->valType, iadInStack);
	isxAddNew(pfc, poInput, dest, source, NULL, iList);
	}
    }
source = isxCallAddress(function->var->cName);
isxAddNew(pfc, poCall, NULL, source, NULL, iList);
for (ty = outTuple->children, offset=0; ty != NULL; ty = ty->next, ++offset)
    {
    enum isxValType valType = isxValTypeFromTy(pfc, ty);
    source = isxIoAddress(offset, valType, iadOutStack);
    dest = isxTempVarAddress(pfc, varHash, weight, valType);
    slAddTail(&destList, dest);
    isxAddNew(pfc, poOutput, dest, source, NULL, iList);
    }
return destList;
}

void isxAddReturnInfo(struct pfCompile *pfc, struct pfParse *outTuple, 
	struct hash *varHash, struct dlList *iList)
/* Add instructions for return parameters. */
{
struct pfParse *pp;
int offset=slCount(outTuple->children)-1;
/* Code generator's job is easier if we reverse these. */
slReverse(&outTuple->children);
for (pp = outTuple->children; pp != NULL; pp = pp->next, --offset)
    {
    struct isxAddress *source = varAddress(pp->var, varHash, 1.0, 
	ppToIsxValType(pfc, pp));
    struct isxAddress *dest = isxIoAddress(offset, source->valType, iadReturnVar);
    isxAddNew(pfc, poReturnVal, dest, source, NULL, iList);
    }
slReverse(&outTuple->children);
}

static struct isxAddress *isxExpression(struct pfCompile *pfc, 
	struct pfParse *pp, struct hash *varHash, 
	double weight, struct dlList *iList)
/* Generate intermediate code for expression. Return destination */
{
switch (pp->type)
    {
    case pptConstBit:
    case pptConstByte:
    case pptConstChar:
    case pptConstShort:
    case pptConstInt:
    case pptConstLong:
    case pptConstFloat:
    case pptConstDouble:
    case pptConstString:
	return isxConstAddress(pp->tok, ppToIsxValType(pfc, pp));
    case pptVarUse:
	return varAddress(pp->var, varHash, weight, ppToIsxValType(pfc, pp));
    case pptPlus:
	return isxBinaryOp(pfc, pp, varHash, poPlus, weight, iList);
    case pptMinus:
	return isxBinaryOp(pfc, pp, varHash, poMinus, weight, iList);
    case pptMul:
	return isxBinaryOp(pfc, pp, varHash, poMul, weight, iList);
    case pptDiv:
	return isxBinaryOp(pfc, pp, varHash, poDiv, weight, iList);
    case pptMod:
	return isxBinaryOp(pfc, pp, varHash, poMod, weight, iList);
    case pptBitAnd:
	return isxBinaryOp(pfc, pp, varHash, poBitAnd, weight, iList);
    case pptBitOr:
	return isxBinaryOp(pfc, pp, varHash, poBitOr, weight, iList);
    case pptBitXor:
	return isxBinaryOp(pfc, pp, varHash, poBitXor, weight, iList);
    case pptShiftLeft:
	return isxBinaryOp(pfc, pp, varHash, poShiftLeft, weight, iList);
    case pptShiftRight:
	return isxBinaryOp(pfc, pp, varHash, poShiftRight, weight, iList);
    case pptNegate:
        return isxUnaryOp(pfc, pp, varHash, poNegate, weight, iList);
    case pptFlipBits:
        return isxUnaryOp(pfc, pp, varHash, poFlipBits, weight, iList);
    case pptStringCat:
        return isxStringCat(pfc, pp, varHash, weight, iList);
    case pptCall:
        return isxCall(pfc, pp, varHash, weight, iList);
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
        return isxUnaryOp(pfc, pp, varHash, poCast, weight, iList);
    default:
	pfParseDump(pp, 3, uglyOut);
        internalErr();
	return NULL;
    }
}

static void cmpAndJump(struct pfCompile *pfc, struct pfParse *cond, 
	struct hash *varHash, struct isxAddress *trueLabel, 
	struct isxAddress *falseLabel,enum isxOpType op,  
	double weight, struct dlList *iList)
/* Generate code that jumps to the true label if cond is true, and
 * to the false label otherwise. */
{
struct pfParse *left = cond->children;
struct pfParse *right = left->next;
struct isxAddress *leftAddr = isxExpression(pfc, left, varHash, weight, iList);
struct isxAddress *rightAddr = isxExpression(pfc,right, varHash, weight, iList);
isxAddNew(pfc, op, trueLabel, leftAddr, rightAddr, iList);
if (falseLabel)
    isxAddNew(pfc, poJump, falseLabel, NULL, NULL, iList);
}


static enum pfParseType invLogOp(enum pfParseType op)
/* Convert != to ==, > to <=, etc. */
{
switch (op)
     {
     case pptGreater: 		return pptLessOrEquals;
     case pptGreaterOrEquals: 	return pptLess;
     case pptLess: 		return pptGreaterOrEquals;
     case pptLessOrEquals:	return pptGreater;
     case pptSame:		return pptNotSame;
     case pptNotSame:		return pptSame;
     case pptLogAnd:		return pptLogOr;
     case pptLogOr:		return pptLogAnd;
     default:
          {
	  return op;
	  }
     }
}


static void isxConditionalJump(struct pfCompile *pfc, struct pfParse *cond, 
	struct hash *varHash, struct isxAddress *trueLabel, 
	struct isxAddress *falseLabel, boolean invert, 
	double weight, struct dlList *iList)
/* Generate code that jumps to the true label if cond is true, and
 * to the false label otherwise. */
{
enum pfParseType ppt = cond->type;
if (invert)
    ppt = invLogOp(ppt);
switch (ppt)
    {
    case pptLess:
	cmpAndJump(pfc, cond, varHash, trueLabel, falseLabel, poBlt, 
		weight, iList);
        break;
    case pptLessOrEquals:
	cmpAndJump(pfc, cond, varHash, trueLabel, falseLabel, poBle, 
		weight, iList);
        break;
    case pptGreaterOrEquals:
	cmpAndJump(pfc, cond, varHash, trueLabel, falseLabel, poBge, 
		weight, iList);
        break;
    case pptGreater:
	cmpAndJump(pfc, cond, varHash, trueLabel, falseLabel, poBgt, 
		weight, iList);
        break;
    case pptSame:
	cmpAndJump(pfc, cond, varHash, trueLabel, falseLabel, poBeq, 
		weight, iList);
        break;
    case pptNotSame:
	cmpAndJump(pfc, cond, varHash, trueLabel, falseLabel, poBne, 
		weight, iList);
        break;
    case pptLogOr:
        {
	struct pfParse *left = cond->children;
	struct pfParse *right = left->next;
	isxConditionalJump(pfc, left, varHash, trueLabel, NULL, invert,
		weight, iList);
	isxConditionalJump(pfc, right, varHash, trueLabel, falseLabel, invert,
		weight, iList);
	break;
	}
    case pptLogAnd:
        {
	struct pfParse *left = cond->children;
	struct pfParse *right = left->next;
	isxConditionalJump(pfc, left, varHash, falseLabel, NULL, !invert,
		weight, iList);
	isxConditionalJump(pfc, right, varHash, trueLabel, falseLabel, invert,
		weight, iList);
	break;
	}
    case pptNot:
        {
	isxConditionalJump(pfc, cond->children, varHash, trueLabel, falseLabel, 
	    !invert, weight, iList);
	break;
	}
    case pptCastByteToBit:
    case pptCastShortToBit:
    case pptCastIntToBit:
    case pptCastLongToBit:
    case pptCastFloatToBit:
    case pptCastDoubleToBit:
    case pptCastStringToBit:
        {
	struct isxAddress *source = isxExpression(pfc, cond->children, 
		varHash, weight, iList);
	enum isxOpType op = (invert ? poBz : poBnz);
	isxAddNew(pfc, op, trueLabel, source, NULL, iList);
	if (falseLabel)
	    isxAddNew(pfc, poJump, falseLabel, NULL, NULL, iList);
	break;
	}
    case pptConstBit:
        {
	int isTrue = cond->tok->val.i;
	if (isTrue)
	    isxAddNew(pfc, poJump, trueLabel, NULL, NULL, iList);
	else
	    isxAddNew(pfc, poJump, falseLabel, NULL, NULL, iList);
	break;
	}
    default:
	{
	internalErrAt(cond->tok);
	break;
	}
    }
}


static void isxAssign(struct pfCompile *pfc, 
	struct pfParse *pp, struct hash *varHash, 
	double weight, struct dlList *iList)
/* Code an assignment */
{
struct pfParse *use = pp->children;
struct pfParse *val = use->next;
if (use->type == pptVarUse)
    {
    struct isxAddress *source = isxExpression(pfc, val, varHash, 
	    weight, iList);
    struct isxAddress *dest = varAddress(use->var, varHash, 
	    weight, ppToIsxValType(pfc, use));
    if (source->adType == iadTempVar)
	{
	struct isx *prevIsx = iList->tail->val;
	prevIsx->dest = dest;
	}
    else
	{
	isxAddNew(pfc, poAssign, dest, source, NULL, iList);
	}
    }
else if (use->type == pptTuple)
    {
    struct isxAddress *sourceList = isxExpression(pfc, val, varHash, 
	    weight, iList);
    int tupSize = slCount(use->children);
    struct isxAddress *source;
    struct pfParse *pp;
    assert(tupSize == slCount(sourceList));
    for (source = sourceList,pp=use->children; 
    	source != NULL; source = source->next, pp=pp->next)
        {
	struct isxAddress *dest;
	if (pp->type != pptVarInit && pp->type != pptVarUse)
	     internalErrAt(pp->tok);
	dest = varAddress(pp->var, varHash, weight, ppToIsxValType(pfc, pp));
	isxAddNew(pfc, poAssign, dest, source, NULL, iList);
	}
    }
else
    {
    internalErrAt(pp->tok);
    }
}

void isxStatement(struct pfCompile *pfc, struct pfParse *pp, 
	struct hash *varHash, double weight, struct dlList *iList)
/* Generate intermediate code for statement. */
{
switch (pp->type)
    {
    case pptCompound:
    case pptTuple:
        {
	for (pp = pp->children; pp != NULL; pp = pp->next)
	    isxStatement(pfc, pp, varHash, weight, iList);
	break;
	}
    case pptVarInit:
	{
	struct pfParse *init = pp->children->next->next;
	struct isxAddress *dest = varAddress(pp->var, varHash, 
		weight, ppToIsxValType(pfc, pp));
	struct isxAddress *source;
	if (init)
	    source = isxExpression(pfc, init, varHash, weight, iList);
	else
	    source = zeroAddress();
	isxAddNew(pfc, poInit, dest, source, NULL, iList);
	break;
	}
    case pptAssign:
	isxAssign(pfc, pp, varHash, weight, iList);
	break;
    case pptPlusEquals:
	isxOpEquals(pfc, pp, varHash, poPlus, weight, iList);
	break;
    case pptMinusEquals:
	isxOpEquals(pfc, pp, varHash, poMinus, weight, iList);
	break;
    case pptMulEquals:
	isxOpEquals(pfc, pp, varHash, poMul, weight, iList);
	break;
    case pptDivEquals:
	isxOpEquals(pfc, pp, varHash, poDiv, weight, iList);
	break;
    case pptIf:
        {
	struct pfParse *test = pp->children;
	struct pfParse *truePp = test->next;
	struct pfParse *falsePp = truePp->next;
	struct isxAddress *trueLabel = isxTempLabelAddress(pfc);
	struct isxAddress *falseLabel = isxTempLabelAddress(pfc);
	struct isxAddress *endLabel = isxTempLabelAddress(pfc);
	isxConditionalJump(pfc, test, varHash, trueLabel, falseLabel, FALSE,
		weight, iList);
	isxAddNew(pfc, poCondStart, NULL, NULL, NULL, iList);
	isxAddNew(pfc, poCondCase, trueLabel, NULL, NULL, iList);
	isxStatement(pfc, truePp, varHash, weight*0.6, iList);
	isxAddNew(pfc, poJump, endLabel, NULL, NULL, iList);
	isxAddNew(pfc, poCondCase, falseLabel, NULL, NULL, iList);
	if (falsePp)
	    isxStatement(pfc, falsePp, varHash, weight*0.6, iList);
	isxAddNew(pfc, poCondEnd, endLabel, NULL, NULL, iList);
	break;
	}
    case pptFor:
        {
	struct pfParse *init = pp->children;
	struct pfParse *test = init->next;
	struct pfParse *end = test->next;
	struct pfParse *body = end->next;
	struct isxAddress *startLabel = isxTempLabelAddress(pfc);
	struct isxAddress *condLabel = isxTempLabelAddress(pfc);
	struct isxAddress *endLabel = isxTempLabelAddress(pfc);
	double loopWeight = weight*10;
	isxStatement(pfc, init, varHash, weight, iList);
	isxAddNew(pfc, poLoopStart, startLabel, NULL, condLabel, iList);
	isxStatement(pfc, body, varHash, loopWeight, iList);
	isxStatement(pfc, end, varHash, loopWeight, iList);
	isxAddNew(pfc, poLabel, condLabel, NULL, NULL, iList);
	isxConditionalJump(pfc, test, varHash, startLabel, NULL, 
		FALSE, loopWeight, iList);
	isxAddNew(pfc, poLoopEnd, endLabel, NULL, NULL, iList);
	break;
	}
    case pptNop:
        break;
    default:
	isxExpression(pfc, pp, varHash, weight, iList);
	break;
    }
}

static void rIsxModuleVars(struct pfCompile *pfc, struct pfParse *pp, 
	struct hash *varHash, struct dlList *iList)
/* Convert module-level and static function variables to members of iList. */
{
double weight = 1.0;
switch (pp->type)
    {
    case pptVarInit:
	{
	struct pfVar *var = pp->var;
	struct pfScope *scope = var->scope;
	if (!scope->isLocal)	
	    {
	    struct isxAddress *dest = varAddress(pp->var, varHash, 
		    weight,  ppToIsxValType(pfc, pp));
	    struct isxAddress *source;
	    struct pfParse *init = pp->children->next->next;
	    boolean doInit = FALSE;
	    doInit =  (init != NULL && pfParseIsConst(init));
	    if (doInit)
		source = isxExpression(pfc, init, varHash, weight, iList);
	    else
		source = zeroAddress();
	    isxAddNew(pfc, poInit, dest, source, NULL, iList);
	    }
	else if (var->ty->access == paStatic)
	    internalErrAt(pp->tok);	// FIXME
        break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rIsxModuleVars(pfc, pp, varHash, iList);
}

struct isxList *isxListNew()
/* Create new empty isxList */
{
struct isxList *isxList;
AllocVar(isxList);
isxList->iList = dlListNew(0);
return isxList;
}

struct isxList *isxModuleVars(struct pfCompile *pfc, struct pfParse *module)
/* Convert module-level and static function variables to an isxList */
{
struct isxList *isxList = isxListNew();
struct hash *varHash = hashNew(0);
rIsxModuleVars(pfc, module, varHash, isxList->iList);
hashFree(&varHash);
return isxList;
}

