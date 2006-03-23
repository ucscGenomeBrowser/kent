/* isx - intermediate code -  hexes of
 *     label destType destName left op right 
 * For instance:
 *     100 + int t1 1 t2  (means int t1 = 1 + t2)
 *     101 * double v 0.5 t3  (means double v = 0.5 * t3)
 */

#include "common.h"
#include "dlist.h"
#include "pfParse.h"
#include "pfToken.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfCompile.h"
#include "isx.h"

static struct isxAddress *isxExpression(struct pfCompile *pfc, 
	struct pfParse *pp, struct hash *varHash, struct dlList *iList);
/* Generate intermediate code for expression. Return destination */

static void isxStatement(struct pfCompile *pfc, struct pfParse *pp, 
	struct hash *varHash, struct dlList *iList);
/* Generate intermediate code for statement. */

char *isxValTypeToString(enum isxValType val)
/* Convert isxValType to string. */
{
switch (val)
    {
    case ivZero:
	return "zer";
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



static void isxAddrDump(struct isxAddress *iad, FILE *f)
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
	fprintf(f, "%s", iad->name);
	break;
    case iadInStack:
	fprintf(f, "in(%d)", iad->val.stackOffset);
        break;
    case iadOutStack:
	fprintf(f, "out(%d)", iad->val.stackOffset);
        break;
    case iadOperator:
	fprintf(f, "#%s", iad->name);
	break;
    case iadLabel:
        fprintf(f, "@%s", iad->name);
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
    isxAddrDump(isx->dest, f);
    }
fprintf(f, " =");
if (isx->left != NULL)
    {
    fprintf(f, " ");
    isxAddrDump(isx->left, f);
    }
if (isx->right != NULL)
    {
    fprintf(f, " ");
    isxAddrDump(isx->right, f);
    }
if (isx->liveList != NULL)
    {
    struct slRef *ref;
    fprintf(f, "\t{");
    for (ref = isx->liveList; ref != NULL; ref = ref->next)
        {
	struct isxAddress *iad = ref->val;
	fprintf(f, "%s", iad->name);
	if (iad->reg != NULL)
	    fprintf(f, "@%s", isxRegName(iad->reg, iad->valType));
	if (ref->next != NULL)
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
	struct isxAddress *right, struct dlList *iList)
/* Make new isx instruction, and hang it on iList */
{
struct isx *isx;
AllocVar(isx);
isx->opType = opType;
isx->dest = dest;
isx->left = left;
isx->right = right;
dlAddValTail(iList, isx);
return isx;
}

static enum isxValType tyToIsxValType(struct pfCompile *pfc, 
	struct pfType *ty)
/* Return isxValType corresponding to pp */
{
struct pfBaseType *base = ty->base;
if (base == pfc->bitType || base == pfc->byteType)
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
return tyToIsxValType(pfc, pp->ty);
}

static struct isxAddress *constAddress(struct pfToken *tok, 
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

static struct isxAddress *tempAddress(struct pfCompile *pfc, struct hash *hash,
	enum isxValType valType)
/* Create a new temporary */
{
struct isxAddress *iad;
char buf[18];
safef(buf, sizeof(buf), "$t%X", ++(pfc->tempLabelMaker));
AllocVar(iad);
iad->adType = iadTempVar;
iad->valType = valType;
hashAddSaveName(hash, buf, iad, &iad->name);
return iad;
}


static struct isxAddress *varAddress(struct pfVar *var, struct hash *hash,
	enum isxValType valType)
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
return iad;
}

static struct isxAddress *callAddress(struct pfVar *var, struct hash *hash)
/* Create reference to a function */
{
struct isxAddress *iad = varAddress(var, hash, ivJump);
iad->adType = iadOperator;
return iad;
}


static struct isxAddress *ioAddress(int offset, enum isxValType valType,
	enum isxAddressType adType)
/* Return reference to an io stack address */
{
struct isxAddress *iad;
AllocVar(iad);
iad->adType = adType;
iad->valType = valType;
iad->val.stackOffset = offset;
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

static struct isxAddress *tempLabelAddress(struct pfCompile *pfc)
/* Create reference to a label. */
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
	enum isxOpType op, struct dlList *iList)
/* Generate intermediate code for expression. Return destination */
{
struct isxAddress *left = isxExpression(pfc, pp->children,
	varHash, iList);
struct isxAddress *right = isxExpression(pfc, pp->children->next,
	varHash, iList);
struct isxAddress *dest = tempAddress(pfc, varHash, ppToIsxValType(pfc,pp));
isxNew(pfc, op, dest, left, right, iList);
return dest;
}

static struct isxAddress *isxUnaryOp(struct pfCompile *pfc, 
	struct pfParse *pp, struct hash *varHash, 
	enum isxOpType op, struct dlList *iList)
/* Generate intermediate code for expression. Return destination */
{
struct isxAddress *target = isxExpression(pfc, pp->children,
	varHash, iList);
struct isxAddress *dest = tempAddress(pfc, varHash, ppToIsxValType(pfc,pp));
isxNew(pfc, op, dest, target, NULL, iList);
return dest;
}

static struct isxAddress *isxStringCat(struct pfCompile *pfc,
	struct pfParse *pp, struct hash *varHash, struct dlList *iList)
/* Create string cat op */
{
uglyAbort("Can't handle stringCat yet");
return NULL;
}

static struct isxAddress *isxCall(struct pfCompile *pfc,
	struct pfParse *pp, struct hash *varHash, struct dlList *iList)
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
    source = isxExpression(pfc, p, varHash, iList);
    dest = ioAddress(offset, source->valType, iadInStack);
    isxNew(pfc, poInput, dest, source, NULL, iList);
    }
source = callAddress(function->var, varHash);
isxNew(pfc, poCall, NULL, source, NULL, iList);
for (ty = outTuple->children, offset=0; ty != NULL; ty = ty->next, ++offset)
    {
    enum isxValType valType = tyToIsxValType(pfc, ty);
    source = ioAddress(offset, valType, iadOutStack);
    dest = tempAddress(pfc, varHash, valType);
    slAddTail(&destList, dest);
    isxNew(pfc, poOutput, dest, source, NULL, iList);
    }
return destList;
}

static struct isxAddress *isxExpression(struct pfCompile *pfc, 
	struct pfParse *pp, struct hash *varHash, struct dlList *iList)
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
	return constAddress(pp->tok, ppToIsxValType(pfc, pp));
    case pptVarUse:
	return varAddress(pp->var, varHash, ppToIsxValType(pfc, pp));
    case pptPlus:
	return isxBinaryOp(pfc, pp, varHash, poPlus, iList);
    case pptMinus:
	return isxBinaryOp(pfc, pp, varHash, poMinus, iList);
    case pptMul:
	return isxBinaryOp(pfc, pp, varHash, poMul, iList);
    case pptDiv:
	return isxBinaryOp(pfc, pp, varHash, poDiv, iList);
    case pptMod:
	return isxBinaryOp(pfc, pp, varHash, poMod, iList);
    case pptBitAnd:
	return isxBinaryOp(pfc, pp, varHash, poBitAnd, iList);
    case pptBitOr:
	return isxBinaryOp(pfc, pp, varHash, poBitOr, iList);
    case pptBitXor:
	return isxBinaryOp(pfc, pp, varHash, poBitXor, iList);
    case pptShiftLeft:
	return isxBinaryOp(pfc, pp, varHash, poShiftLeft, iList);
    case pptShiftRight:
	return isxBinaryOp(pfc, pp, varHash, poShiftRight, iList);
    case pptNegate:
        return isxUnaryOp(pfc, pp, varHash, poNegate, iList);
    case pptFlipBits:
        return isxUnaryOp(pfc, pp, varHash, poFlipBits, iList);
    case pptStringCat:
        return isxStringCat(pfc, pp, varHash, iList);
    case pptCall:
        return isxCall(pfc, pp, varHash, iList);
    default:
	pfParseDump(pp, 3, uglyOut);
        internalErr();
	return NULL;
    }
}

static void cmpAndJump(struct pfCompile *pfc, struct pfParse *cond, 
	struct hash *varHash, struct isxAddress *trueLabel, 
	struct isxAddress *falseLabel,enum isxOpType op,  
	struct dlList *iList)
/* Generate code that jumps to the true label if cond is true, and
 * to the false label otherwise. */
{
struct pfParse *left = cond->children;
struct pfParse *right = left->next;
struct isxAddress *leftAddr = isxExpression(pfc, left, varHash, iList);
struct isxAddress *rightAddr = isxExpression(pfc, right, varHash, iList);
isxNew(pfc, op, trueLabel, leftAddr, rightAddr, iList);
if (falseLabel)
    isxNew(pfc, poJump, falseLabel, NULL, NULL, iList);
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
	  internalErr();
	  return op;
	  }
     }
}


static void isxConditionalJump(struct pfCompile *pfc, struct pfParse *cond, 
	struct hash *varHash, struct isxAddress *trueLabel, 
	struct isxAddress *falseLabel, boolean invert, struct dlList *iList)
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
		iList);
        break;
    case pptLessOrEquals:
	cmpAndJump(pfc, cond, varHash, trueLabel, falseLabel, poBle, 
		iList);
        break;
    case pptGreaterOrEquals:
	cmpAndJump(pfc, cond, varHash, trueLabel, falseLabel, poBge, 
		iList);
        break;
    case pptGreater:
	cmpAndJump(pfc, cond, varHash, trueLabel, falseLabel, poBgt, 
		iList);
        break;
    case pptSame:
	cmpAndJump(pfc, cond, varHash, trueLabel, falseLabel, poBeq, 
		iList);
        break;
    case pptNotSame:
	cmpAndJump(pfc, cond, varHash, trueLabel, falseLabel, poBne, 
		iList);
        break;
    case pptLogOr:
        {
	struct pfParse *left = cond->children;
	struct pfParse *right = left->next;
	isxConditionalJump(pfc, left, varHash, trueLabel, NULL, invert,
		iList);
	isxConditionalJump(pfc, right, varHash, trueLabel, falseLabel, invert,
		iList);
	break;
	}
    case pptLogAnd:
        {
	struct pfParse *left = cond->children;
	struct pfParse *right = left->next;
	isxConditionalJump(pfc, left, varHash, falseLabel, NULL, !invert,
		iList);
	isxConditionalJump(pfc, right, varHash, trueLabel, falseLabel, invert,
		iList);
	break;
	}
    default:
	pfParseDump(cond, 3, uglyOut);
        internalErr();
	break;
    }
}


static void isxStatement(struct pfCompile *pfc, struct pfParse *pp, 
	struct hash *varHash, struct dlList *iList)
/* Generate intermediate code for statement. */
{
switch (pp->type)
    {
    case pptCompound:
    case pptTuple:
        {
	for (pp = pp->children; pp != NULL; pp = pp->next)
	    isxStatement(pfc, pp, varHash, iList);
	break;
	}
    case pptVarInit:
	{
	struct pfParse *init = pp->children->next->next;
	struct isxAddress *dest = varAddress(pp->var, varHash, 
		ppToIsxValType(pfc, pp));
	struct isxAddress *source;
	if (init)
	    source = isxExpression(pfc, init, varHash, iList);
	else
	    source = zeroAddress();
	isxNew(pfc, poInit, dest, source, NULL, iList);
	break;
	}
    case pptAssign:
        {
	struct pfParse *use = pp->children;
	struct pfParse *val = use->next;
	struct isxAddress *source = isxExpression(pfc, val, varHash, iList);
	struct isxAddress *dest = varAddress(use->var, varHash, 
		ppToIsxValType(pfc, use));
	isxNew(pfc, poAssign, dest, source, NULL, iList);
	break;
	}
    case pptIf:
        {
	struct pfParse *test = pp->children;
	struct pfParse *truePp = test->next;
	struct pfParse *falsePp = truePp->next;
	struct isxAddress *trueLabel = tempLabelAddress(pfc);
	struct isxAddress *falseLabel = tempLabelAddress(pfc);
	struct isxAddress *endLabel = tempLabelAddress(pfc);
	isxConditionalJump(pfc, test, varHash, trueLabel, falseLabel, FALSE,
		iList);
	isxNew(pfc, poCondStart, NULL, NULL, NULL, iList);
	isxNew(pfc, poCondCase, trueLabel, NULL, NULL, iList);
	isxStatement(pfc, truePp, varHash, iList);
	isxNew(pfc, poJump, endLabel, NULL, NULL, iList);
	isxNew(pfc, poCondCase, falseLabel, NULL, NULL, iList);
	if (falsePp)
	    isxStatement(pfc, falsePp, varHash, iList);
	isxNew(pfc, poCondEnd, endLabel, NULL, NULL, iList);
	break;
	}
    case pptFor:
        {
	struct pfParse *init = pp->children;
	struct pfParse *test = init->next;
	struct pfParse *end = test->next;
	struct pfParse *body = end->next;
	struct isxAddress *startLabel = tempLabelAddress(pfc);
	struct isxAddress *condLabel = tempLabelAddress(pfc);
	struct isxAddress *endLabel = tempLabelAddress(pfc);
	isxStatement(pfc, init, varHash, iList);
	isxNew(pfc, poJump, condLabel, NULL, NULL, iList);
	isxNew(pfc, poLoopStart, startLabel, NULL, NULL, iList);
	isxStatement(pfc, body, varHash, iList);
	isxNew(pfc, poLabel, condLabel, NULL, NULL, iList);
	isxConditionalJump(pfc, test, varHash, startLabel, endLabel, 
		FALSE, iList);
	isxNew(pfc, poLoopEnd, endLabel, NULL, NULL, iList);
	break;
	}
    default:
	isxExpression(pfc, pp, varHash, iList);
	break;
    }
}

static void isxLiveList(struct dlList *iList)
/* Create list of live variables at each instruction by scanning
 * backwards. */
{
struct dlNode *node;
struct slRef *liveList = NULL;
for (node = iList->tail; !dlStart(node); node = node->prev)
    {
    struct slRef *newList = NULL, *ref;
    struct isx *isx = node->val;
    struct isxAddress *iad;

    /* Save away current live list. */
    isx->liveList = liveList;

    /* Make copy of live list minus any overwritten dests. */
    for (ref = liveList; ref != NULL; ref = ref->next)
	{
	struct isxAddress *iad = ref->val;
	if (iad != isx->dest)
	    refAdd(&newList, iad);
	}

    /* Add sources to live list */
    iad = isx->left;
    if (iad != NULL && (iad->adType == iadRealVar || iad->adType == iadTempVar))
	refAddUnique(&newList, iad);
    iad = isx->right;
    if (iad != NULL && (iad->adType == iadRealVar || iad->adType == iadTempVar))
	refAddUnique(&newList, iad);

    /* Flip to new live list */
    liveList = newList;
    }
slFreeList(&liveList);
}

void isxModule(struct pfCompile *pfc, struct pfParse *pp, 
	struct dlList *iList)
/* Generate instructions for module. */
{
struct hash *varHash = hashNew(16);
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    isxStatement(pfc, pp, varHash, iList);
    }
isxLiveList(iList);
}

struct dlList *isxFromParse(struct pfCompile *pfc, struct pfParse *pp)
/* Convert parse tree to isx. */
{
struct dlList *iList = dlListNew(0);
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    switch (pp->type)
        {
	case pptMainModule:
	case pptModule:
	    isxModule(pfc, pp, iList);
	    break;
	}
    }
return iList;
}

