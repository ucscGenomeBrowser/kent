/* isxToPentium - convert isx code to Pentium code.  This basically 
 * follows the register picking algorithm in Aho/Ulman. */

#include "common.h"
#include "dlist.h"
#include "pfParse.h"
#include "pfToken.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfCompile.h"
#include "pfPreamble.h"
#include "ctar.h"
#include "isx.h"
#include "gnuMac.h"
#include "pentCode.h"

enum pentRegs
/* These need to be in same order as regInfo table. */
   {
   ax=0, bx=1, cx=2, dx=3, si=4, di=5, st0=6, pentRegCount=7,
   };

static struct isxReg regInfo[] = {
    { 4, "%al", "%ax", "%eax", NULL, NULL, NULL, "%eax"},
    { 4, "%bl", "%bx", "%ebx", NULL, NULL, NULL, "%ebx"},
    { 4, "%cl", "%cx", "%ecx", NULL, NULL, NULL, "%ecx"},
    { 4, "%dl", "%dx", "%edx", NULL, NULL, NULL, "%edx"},
    { 4, NULL, "%si", "%esi", NULL, NULL, NULL, "%esi"},
    { 4, NULL, "%di", "%edi", NULL, NULL, NULL, "%edi"},
#ifdef SOON
    { 8, NULL, NULL, NULL, NULL, "%st0", "%st0, NULL},
    { 8, NULL, NULL, NULL, NULL, "%st1", "%st1, NULL},
    { 8, NULL, NULL, NULL, NULL, "%st2", "%st2, NULL},
    { 8, NULL, NULL, NULL, NULL, "%st3", "%st3, NULL},
    { 8, NULL, NULL, NULL, NULL, "%st4", "%st4, NULL},
    { 8, NULL, NULL, NULL, NULL, "%st5", "%st5, NULL},
    { 8, NULL, NULL, NULL, NULL, "%st6", "%st6, NULL},
    { 8, NULL, NULL, NULL, NULL, "%st7", "%st7, NULL},
#endif /* SOON */
};

struct pentFunctionInfo *pentFunctionInfoNew()
/* Create new pentFunctionInfo */
{
struct pentFunctionInfo *pfi;
AllocVar(pfi);
pfi->regCount = ArraySize(regInfo);
AllocArray(pfi->regsUsed, pfi->regCount);
pfi->coder = pentCoderNew();
return pfi;
}

static void initRegInfo()
/* Do some one-time initialization of regInfo */
{
int i;
for (i=0; i<ArraySize(regInfo); ++i)
    regInfo[i].regIx = i;
}

struct regStomper
/* Information on registers. */
    {
    int stompPos[ArraySize(regInfo)];	
    struct isxAddress *contents[ArraySize(regInfo)];
    };

enum pentDataOp
/* Some of the pentium op codes that apply to many types */
    {
    opMov, opAdd, opSub, opMul, opDiv, opAnd, opOr, opXor, opSal, 
    opSar, opNeg, opNot, opCmp, opTest,
    };

struct dataOpTable
/* Opcode table */
    {
    enum pentDataOp op;
    char *byteName;
    char *shortName;
    char *intName;
    char *longName;
    char *floatName;
    char *doubleName;
    char *pointerName;
    };

static struct dataOpTable dataOpTable[] =
    {
    {opMov, "movb", "movw", "movl", NULL, NULL, NULL, "movl"},
    {opAdd, "addb", "addw", "addl", NULL, "fadd", "fadd", "addl"},
    {opSub, "subb", "subw", "subl", NULL, "fsub", "fsub", "subl"},
    {opMul, "imulb", "imulw", "imull", NULL, "fmul", "fmul", NULL},
    {opDiv, "idivb", "idivw", "idivl", NULL, "fdiv", "fdiv", NULL},
    {opAnd, "andb", "andw", "andl", NULL, NULL, NULL, NULL},
    {opOr, "orb", "orw", "orl", NULL, NULL, NULL, NULL},
    {opXor, "xorb", "xorw", "xorl", NULL, NULL, NULL, NULL},
    {opSal, "salb", "salw", "sall", NULL, NULL, NULL, NULL},
    {opSar, "sarb", "sarw", "sarl", NULL, NULL, NULL, NULL},
    {opNeg, "negb", "negw", "negl", NULL, NULL, NULL, NULL},
    {opNot, "notb", "notw", "notl", NULL, NULL, NULL, NULL},
    {opCmp, "cmpb", "cmpw", "cmpl", NULL, NULL, NULL, NULL},
    {opTest, "testb", "testw", "testl", NULL, NULL, NULL, NULL},
    };

static char *opOfType(enum pentDataOp opType, enum isxValType valType)
/* Return string for given opCode and data type */
{
char *opString;
switch (valType)
    {
    case ivByte:
	opString = dataOpTable[opType].byteName;
	break;
    case ivShort:
	opString = dataOpTable[opType].shortName;
	break;
    case ivInt:
	opString = dataOpTable[opType].intName;
	break;
    case ivLong:
	opString = dataOpTable[opType].longName;
	break;
    case ivFloat:
	opString = dataOpTable[opType].floatName;
	break;
    case ivDouble:
	opString = dataOpTable[opType].doubleName;
	break;
    case ivObject:
    case ivString:
	opString = dataOpTable[opType].pointerName;
	break;
    default:
	internalErr();
	opString = NULL;
	break;
    }
assert(opString != NULL);
return opString;
}

int pentTypeSize(enum isxValType valType)
/* Return size of a val type */
{
switch (valType)
    {
    case ivByte:
	return 1;
    case ivShort:
	return 2;
    case ivInt:
	return 4;
    case ivLong:
	return 8;
    case ivFloat:
	return 4;
    case ivDouble:
	return 8;
    case ivObject:
    case ivString:
	return 4;
    default:
	internalErr();
	return 0;
    }
}

static int liveListIx(struct isxLiveVar *liveList, struct isxAddress *var)
/* Return index of ref on list, or -1 if not there. */
{
struct isxLiveVar *live;
int ix = 0;
for (live = liveList; live != NULL; live = live->next,++ix)
    if (live->var == var)
        return ix;
return -1;
}

static int findNextUse(struct isxAddress *iad, struct isxLiveVar *liveList)
/* Find next use of node. */
{
struct isxLiveVar *live;
for (live = liveList; live != NULL; live = live->next)
    {
    if (live->var == iad)
        return live->usePos[0];
    }
assert(FALSE);
return 0;
}

static void pentPrintAddress(struct isxAddress *iad, char *buf)
/* Print out an address for an instruction. */
{
switch (iad->adType)
    {
    case iadConst:
	{
	safef(buf, pentCodeBufSize, "$%d", iad->val.tok->val.i);
	break;
	}
    case iadRealVar:
	{
	struct pfVar *var = iad->val.var;
	if (iad->reg != NULL)
	    safef(buf, pentCodeBufSize, "%s", 
	    	isxRegName(iad->reg, iad->valType));
	else if (var->scope->isLocal)
	    {
	    safef(buf, pentCodeBufSize, "%d(%%ebp)", iad->stackOffset);
	    }
	else
	    {
	    safef(buf, pentCodeBufSize, "%s%s", isxPrefixC, iad->name);
	    }
	break;
	}
    case iadTempVar:
	{
	if (iad->reg != NULL)
	    safef(buf, pentCodeBufSize, "%s", 
	    	isxRegName(iad->reg, iad->valType));
	else
	    safef(buf, pentCodeBufSize, "%d(%%ebp)", iad->val.tempMemLoc);
	break;
	}
    case iadInStack:
        {
	safef(buf, pentCodeBufSize, "%d(%%esp)", iad->val.ioOffset);
	break;
	}
    case iadOutStack:
        {
	safef(buf, pentCodeBufSize, "%d(uglyOut)", iad->val.ioOffset);
	break;
	}
    default:
        internalErr();
	break;
    }
}

static void codeOp(enum pentDataOp opType, struct isxAddress *source,
	struct isxAddress *dest, struct pentCoder *coder)
/* Print op to file */
{
char *opName = opOfType(opType, source->valType);
char *destString = NULL;
pentPrintAddress(source, coder->sourceBuf);
if (dest != NULL)
    {
    pentPrintAddress(dest, coder->destBuf);
    destString = coder->destBuf;
    }
pentCoderAdd(coder, opName, coder->sourceBuf, destString);
}

static void codeOpSourceReg(enum pentDataOp opType, struct isxReg *reg,
	struct isxAddress *dest,  struct pentCoder *coder)
/* Print op where source is a register. */
{
enum isxValType valType = dest->valType;
char *opName = opOfType(opType, valType);
char *regName = isxRegName(reg, valType);
pentPrintAddress(dest, coder->destBuf);
pentCoderAdd(coder, opName, regName, coder->destBuf);
}

static void codeOpDestReg(enum pentDataOp opType, 
	struct isxAddress *source, struct isxReg *reg,  struct pentCoder *coder)
/* Print op where dest is a register. */
{
enum isxValType valType = source->valType;
char *opName = opOfType(opType, valType);
char *regName = isxRegName(reg, valType);
pentPrintAddress(source, coder->sourceBuf);
pentCoderAdd(coder, opName, coder->sourceBuf, regName);
}

static void unaryOpReg(enum pentDataOp opType, struct isxReg *reg, 
	enum isxValType valType, struct pentCoder *coder)
/* Do unary op on register. */
{
char *regName = isxRegName(reg, valType);
char *opName = opOfType(opType, valType);
pentCoderAdd(coder, opName, NULL, regName);
}


static void clearReg(struct isxReg *reg,  struct pentCoder *coder)
/* Clear out register. */
{
char *name = reg->pointerName;
assert(name != NULL);
pentCoderAdd(coder, "xorl", name, name);
}


static void pentSwapTempFromReg(struct isxReg *reg,  struct pentCoder *coder)
/* If reg contains something not also in memory then save it out. */
{
struct isxAddress *iad = reg->contents;
struct isxAddress old = *iad;
static int tempIx;

coder->tempIx -= reg->size;
iad->reg = NULL;
iad->val.tempMemLoc = coder->tempIx;
codeOp(opMov, &old, iad, coder);
}

static void pentSwapOutIfNeeded(struct isxReg *reg, 
	struct isxLiveVar *liveList, struct pentCoder *coder)
/* Swap out register to memory if need be. */
{
struct isxAddress *iad = reg->contents;
if (iad != NULL)
    {
    if (iad->adType == iadTempVar && iad->val.tempMemLoc == 0)
	{
	if (isxLiveVarFind(liveList, iad))
	    pentSwapTempFromReg(reg, coder);
	}
    else
	iad->reg = NULL;
    }
reg->contents = NULL;
}

static struct isxReg *freeReg(struct isx *isx, enum isxValType valType,
	struct dlNode *nextNode,  struct pentCoder *coder)
/* Find free register for instruction result. */
{
int i;
struct isxAddress *dest = isx->dest;
struct isxReg *regs = regInfo;
int regCount = ArraySize(regInfo);
struct isxReg *reg, *freeReg = NULL;
struct regStomper *stomp = isx->cpuInfo;
struct isxLiveVar *live;

if (dest != NULL)
    {
    /* If destination is reserved or is the one that stomps, by all means
     * use corresponding register.  */
    for (i=0; i<regCount; ++i)
	{
	reg = &regs[i];
	if (reg->reserved == dest || stomp->contents[i] == dest)
	    return reg;
	}
    }

/* Look for a register holding a source that is no longer live. */
    {
    int freeIx = BIGNUM;
    for (i=0; i<regCount; ++i)
	{
	reg = &regs[i];
	if (reg->reserved == NULL && isxRegName(reg, valType))
	    {
	    struct isxAddress *iad = reg->contents;
	    if (iad != NULL)
		 {
		 int sourceIx = 0;
		 if (isx->left == iad)
		     sourceIx = 1;
		 else if (isx->right == iad)
		     sourceIx = 2;
		 if (sourceIx)
		     {
		     if (!isxLiveVarFind(isx->liveList, iad))
			 {
			 if (sourceIx < freeIx)
			     {
			     freeIx = sourceIx;	/* Prefer first source */
			     freeReg = reg;
			     }
			 }
		     }
		 }
	    }
	}
    if (freeReg)
	return freeReg;
    }

/* Mark registers not used by live list as free. */
for (i=0; i<regCount; ++i)
    regs[i].isLive = FALSE;
for (live = isx->liveList; live != NULL; live = live->next)
    if ((reg = live->var->reg) != NULL)
	reg->isLive = TRUE;
for (i=0; i<regCount; ++i)
     {
     reg = &regs[i];
     if (!reg->isLive && !reg->reserved)
	 {
	 if (reg->contents)
	     reg->contents->reg = NULL;
         reg->contents = NULL;
	 }
     }

/* Do a couple of things for which we need to know the dest register... */
if (dest != NULL)
    {
    /* Look for a free register that will get stomped as soon after we disappear
     * ourselves as possible.  We only know when we disappear if our use count
     * is two or less. */
    int destLifetime = 0;
    struct isxLiveVar *destLive = isxLiveVarFind(isx->liveList, dest);
    if (destLive)
        {
	if (destLive->useCount <= 2)
	    destLifetime = destLive->usePos[destLive->useCount-1];
	else
	    destLifetime = -1;
	}
    if (destLifetime >= 0)
        {
	int minUnstomped = BIGNUM;
	for (i=0; i<regCount; ++i)
	    {
	    reg = &regs[i];
	    if (reg->reserved == NULL && isxRegName(reg, valType))
		{
		struct isxAddress *iad = reg->contents;
		if (iad == NULL)
		    {
		    int stompPos = stomp->stompPos[i];
		    if (stompPos >= destLifetime && stompPos < minUnstomped) 
			{
			minUnstomped = stompPos;
			freeReg = reg;
			}
		    }
		}
	    }
	if (freeReg != NULL)
	    {
	    return freeReg;
	    }
	}
    else  /* Here we have at least three references, try for stable register */
        {
	int maxUnstomped = -1;
	int nextUse = destLive->usePos[0];
	for (i=0; i<regCount; ++i)
	    {
	    reg = &regs[i];
	    if (reg->reserved == NULL && isxRegName(reg, valType))
		{
		struct isxAddress *iad = reg->contents;
		if (iad == NULL)
		    {
		    int stompPos = stomp->stompPos[i];
		    if (stompPos >= nextUse && stompPos > maxUnstomped)
			{
			maxUnstomped = stompPos;
			freeReg = reg;
			}
		    }
		}
	    }
	if (freeReg != NULL)
	    {
	    return freeReg;
	    }
	}

    /* If we got to here then still have no free register.  One reason might
     * be that something stable, but little used, is hanging out in one
     * of the stable registers.  See if this is the case and can evict
     * something. */
	{
	double minWeight = 0;
	int useCount = destLive->useCount;
	int usePos;
	if (useCount > 2) useCount = 2;
	usePos = destLive->usePos[useCount-1];
	for (i=0; i<regCount; ++i)
	    {
	    struct isxReg *reg = &regs[i];
	    if (reg->reserved == NULL && isxRegName(reg, valType))
		{
		struct isxAddress *iad = reg->contents;
		if (iad != NULL && iad->adType == iadRealVar 
			&& iad->weight < dest->weight)
		    {
		    int stompPos = stomp->stompPos[i];
		    if (stompPos >= usePos)
		        {
			if (freeReg == NULL)
			    {
			    minWeight = iad->weight;
			    freeReg = reg;
			    }
			else if (minWeight > iad->weight)
			    {
			    minWeight = iad->weight;
			    freeReg = reg;
			    }
			}
		    }
		}
	    }
	if (freeReg != NULL)
	    {
	    uglyf("Evicting %s*%2.1f from %s for %s*%2.1f\n", freeReg->contents->name, freeReg->contents->weight, isxRegName(freeReg, valType), dest->name, dest->weight);
	    return freeReg;
	    }
	}
    }

/* If no luck yet, look for any free register */
for (i=0; i<regCount; ++i)
    {
    struct isxReg *reg = &regs[i];
    if (reg->reserved == NULL && isxRegName(reg, valType))
	{
	struct isxAddress *iad = reg->contents;
	if (iad == NULL)
	    return reg;
	}
    }

/* No free registers, well dang.  Then use a register that
 * holds a variable that is also in memory and is one of our
 * sources. If there's a choice pick one that won't be used for
 * longer. */
    {
    int soonestUse = 0;
    for (i=0; i<regCount; ++i)
	{
	struct isxReg *reg = &regs[i];
	if (reg->reserved == NULL && isxRegName(reg, valType))
	    {
	    struct isxAddress *iad = reg->contents;
	    if ((iad->adType == iadRealVar) ||
		(iad->adType == iadTempVar && iad->val.tempMemLoc != 0))
		{
		if (iad == isx->left || iad == isx->right)
		    {
		    int nextUse = findNextUse(iad, isx->liveList);
		    if (nextUse > soonestUse)
			{
			soonestUse = nextUse;
			freeReg = reg;
			}
		    }
		}
	    }
	}
    if (freeReg)
	return freeReg;
    }

/* Still no free registers, well rats.  Then try for a register
 * that holds a value also in memory, but doesn't need to be 
 * a source.  If there's a choice use the one holding the variable
 * that won't be used for the longest time. */
    {
    int soonestUse = 0;
    for (i=0; i<regCount; ++i)
	{
	struct isxReg *reg = &regs[i];
	if (reg->reserved == NULL && isxRegName(reg, valType))
	    {
	    struct isxAddress *iad = reg->contents;
	    if ((iad->adType == iadRealVar) ||
		(iad->adType == iadTempVar && iad->val.tempMemLoc != 0))
		{
		int nextUse = findNextUse(iad, isx->liveList);
		if (nextUse > soonestUse)
		    {
		    soonestUse = nextUse;
		    freeReg = reg;
		    }
		}
	    }
	}
    if (freeReg)
	return freeReg;
    }

/* Ok, at this point we'll have to save one of the temps in a register
 * to memory, and then return it's register. We'll use the temp that
 * will not be used for the longest. */
    {
    int soonestUse = 0;
    struct isxAddress *iad;
    for (i=0; i<regCount; ++i)
	{
	struct isxReg *reg = &regs[i];
	if (reg->reserved == NULL && isxRegName(reg, valType))
	    {
	    struct isxAddress *iad = reg->contents;
	    int nextUse = findNextUse(iad, isx->liveList);
	    if (nextUse > soonestUse)
		{
		soonestUse = nextUse;
		freeReg = reg;
		}
	    }
	}
    pentSwapTempFromReg(freeReg, coder);
    return freeReg;
    }
}


static void copyRealVarToMem(struct isxAddress *iad, struct pentCoder *coder)
/* Copy address to memory if it's in a register and it's a real var. */
{
if (iad->adType == iadRealVar)
    {
    struct isxReg *reg = iad->reg;
    iad->reg = NULL;
    if (reg != NULL)
	codeOpSourceReg(opMov, reg, iad, coder);
    iad->reg = reg;
    }
}


static void pentAssign(struct isx *isx, struct dlNode *nextNode,  
	struct pentCoder *coder)
/* Code assignment */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isxReg *reg;
if (source->adType == iadZero)
    {
    reg = freeReg(isx, isx->dest->valType, nextNode, coder);
    clearReg(reg, coder);
    }
else
    {
    reg = dest->reg;
    if (reg == NULL)
	reg = freeReg(isx, isx->dest->valType, nextNode, coder);
    if (source->reg != reg)
	codeOpDestReg(opMov, source, reg, coder);
    }

if (reg->contents != NULL)
    reg->contents->reg = NULL;

reg->contents = dest;
dest->reg = reg;
copyRealVarToMem(dest, coder);
}

static void pentBinaryOp(struct isx *isx, struct dlNode *nextNode, 
	enum pentDataOp opCode, boolean isSub,  struct pentCoder *coder)
/* Code most binary ops.  Division is harder. */
{
struct isxReg *reg = freeReg(isx, isx->dest->valType, nextNode, coder);
struct isxAddress *dest;
struct isxAddress *iad = NULL;

/* Figure out if the reg already holds one of our sources. */
if (reg == isx->left->reg)
    iad = isx->right;
else if (reg == isx->right->reg)
    iad = isx->left;
if (iad != NULL)
    {
    /* We're lucky, the destination register is also one of our sources.
     * See if we need to swap for subtraction though. */
    boolean isSwapped = (iad == isx->left);
    if (isSub && isSwapped)
        {
	unaryOpReg(opNeg, reg, iad->valType, coder);
	codeOpDestReg(opAdd, iad, reg, coder);
	}
    else
	{
	codeOpDestReg(opCode, iad, reg, coder);
	}
    }
else
    {
    codeOpDestReg(opMov, isx->left, reg, coder);
    codeOpDestReg(opCode, isx->right, reg, coder);
    }
if (reg->contents != NULL)
    reg->contents->reg = NULL;
dest = isx->dest;
reg->contents = dest;
dest->reg = reg;
copyRealVarToMem(dest, coder);
}

static struct isxLiveVar *dummyLive(struct isxAddress *iad)
/* Construct a live var of lifetime 0 around isx.  This is
 * needed for divides and shifts where may swap out something
 * that is live during this instruction, but not after. */
{
struct isxLiveVar *live;
AllocVar(live);
live->var = iad;
return live;
}

static void addDummyToLive(struct isxAddress *iad, struct isxLiveVar **pList)
/* If iad is a variable then add a dummy ref of it to live list. */
{
if (iad->adType == iadTempVar || iad->adType == iadRealVar)
    {
    if (!isxLiveVarFind(*pList, iad))
        {
	struct isxLiveVar *live = dummyLive(iad);
	slAddHead(pList, live);
	}
    }
}

static void trimDummiesFromLive(struct isxLiveVar *extraLive, 
	struct isxLiveVar *origLive)
/* Free up any extra live vars up until hit origLive. */
{
struct isxLiveVar *live, *next;
for (live = extraLive; live != origLive; live = next)
   {
   next = live->next;
   isxLiveVarFree(&live);
   }
}

static void pentModDivide(struct isx *isx, struct dlNode *nextNode, 
	boolean isMod,  struct pentCoder *coder)
/* Generate code for mod or divide. */
{
struct isxAddress *p = isx->left;
struct isxAddress *q = isx->right;
struct isxAddress *d = isx->dest;
struct isxReg *eax = &regInfo[ax];
struct isxReg *edx = &regInfo[dx];
struct isxReg *ecx = &regInfo[cx];
boolean swappedOutC = FALSE;
struct isxLiveVar *extraLive = isx->liveList;
int extraLiveCount = 0;

addDummyToLive(p, &extraLive);
addDummyToLive(q, &extraLive);

if (q->adType == iadConst)
    {
    pentSwapOutIfNeeded(ecx, extraLive, coder);
    codeOpDestReg(opMov, q, ecx, coder);
    ecx->contents = NULL;
    swappedOutC = TRUE;
    }
if (eax->contents != p)
    {
    pentSwapOutIfNeeded(eax, extraLive, coder);
    codeOpDestReg(opMov, p, eax, coder);
    }
pentSwapOutIfNeeded(edx,extraLive, coder);
clearReg(edx, coder);
if (swappedOutC)
    unaryOpReg(opDiv, ecx, d->valType, coder);
else
    codeOp(opDiv, q, NULL, coder);
if (eax->contents != NULL)
    eax->contents->reg = NULL;
if (isMod)
    {
    /* Jean would like code to add q to a negative result here. */
    eax->contents = NULL;
    edx->contents = d;
    d->reg = edx;
    }
else
    {
    edx->contents = NULL;
    eax->contents = d;
    d->reg = eax;
    }
trimDummiesFromLive(extraLive, isx->liveList);
copyRealVarToMem(d, coder);
}

static void pentShiftOp(struct isx *isx, struct dlNode *nextNode, 
	enum pentDataOp opCode,  struct pentCoder *coder)
/* Code shift ops.  */
{
struct isxAddress *p = isx->left;
struct isxAddress *q = isx->right;
struct isxAddress *d = isx->dest;

if (q->adType == iadConst)
    {
    /* The constant case can be handled as normal. */
    pentBinaryOp(isx, nextNode, opCode, FALSE, coder);
    return;
    }
else
    {
    /* Here where we shift by a non-constant amount.
     * The code is only moderately optimized.  It always
     * grabs eax as the destination and ecx as the thing
     * to hold the amount to shift.  The use of ecx is
     * constrained by the pentium instruction set.  The
     * eax use is not necessary, but our regular register allocation
     * scheme won't work, because it might end up returning
     * ecx.  Since this is relatively rare code, it doesn't
     * seem worth it to optimize it further. */
    struct isxReg *eax = &regInfo[ax];
    struct isxReg *ecx = &regInfo[cx];
    struct isxLiveVar *extraLive = isx->liveList;
    addDummyToLive(p, &extraLive);
    addDummyToLive(q, &extraLive);
    if (eax != p->reg)
	{
	pentSwapOutIfNeeded(eax,extraLive, coder);
	codeOpDestReg(opMov, p, eax, coder);
	}
    else
        {
	p->reg = NULL;
	}
    if (ecx != q->reg)
	{
	pentSwapOutIfNeeded(ecx,extraLive, coder);
	codeOpDestReg(opMov, q, ecx, coder);
	}
    pentCoderAdd(coder, opOfType(opCode, p->valType), 
    	isxRegName(ecx, ivByte), isxRegName(eax, p->valType));
    eax->contents = d;
    d->reg = eax;
    trimDummiesFromLive(extraLive, isx->liveList);
    copyRealVarToMem(d, coder);
    }
}

static void pentInput(struct isx *isx, struct dlNode *nextNode,  
	struct pentCoder *coder)
/* Output code to load an input parameter before a call. */
{
struct isxAddress *source = isx->left;
if (source->reg || source->adType == iadConst)
    codeOp(opMov, source, isx->dest, coder);
else
    {
    struct isxReg *reg = freeReg(isx, isx->dest->valType, nextNode, coder);
    codeOpDestReg(opMov, source, reg, coder);
    reg->contents = source;
    source->reg = reg;
    codeOp(opMov, source, isx->dest, coder);
    }
}

static void pentOutput(struct isx *isx, struct dlNode *nextNode,  
	struct pentCoder *coder)
/* Output code to save an output parameter after a call. */
{
/* This needs to get a lot more complex eventually....  For now it just
 * handles up to three int-sized result. */
struct isxReg *reg;
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
enum isxValType valType = source->valType;
int sourceIx = source->val.ioOffset;
if (sourceIx > 2)
    errAbort("Can't handle more than three pentOutput\n");
if (valType == ivLong || valType == ivFloat || valType == ivDouble)
    errAbort("Can't handle long or floating point pentOutput\n");
switch (sourceIx)
    {
    case 0:
       reg = &regInfo[ax];
       break;
    case 1:
       reg = &regInfo[cx];
       break;
    case 2:
       reg = &regInfo[dx];
       break;
    default:
       internalErr();
       reg = NULL;
       break;
    }
dest->reg = reg;
reg->contents = dest;
}

static void pentCall(struct isx *isx,  struct pentCoder *coder)
/* Output code to actually do call */
{
struct pfVar *funcVar = isx->left->val.var;
pentSwapOutIfNeeded(&regInfo[ax],isx->liveList, coder);
pentSwapOutIfNeeded(&regInfo[cx],isx->liveList, coder);
pentSwapOutIfNeeded(&regInfo[dx],isx->liveList, coder);
safef(coder->destBuf, pentCodeBufSize, "%s%s", isxPrefixC, funcVar->cName);
pentCoderAdd(coder, "call", NULL, coder->destBuf);
}


static void forgetUnreservedRegs()
/* Forget whatever is in non-reserved registers */
{
int i;
/* Clear non-reserved registers. */
for (i=0; i<ArraySize(regInfo); ++i)
    {
    struct isxReg *reg = &regInfo[i];
    if (reg->contents != NULL && reg->reserved == NULL)
	{
	reg->contents->reg = NULL;
	reg->contents = NULL;
	}
    }
}

static void pentLoopStart(struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder)
/* Code beginning of a loop.  This includes loading up the loop
 * register variables and marking them as reserved, jumping
 * to the loop condition, and printing the loop start label. */
{
struct isxLoopInfo *loopy = isx->left->val.loopy;
struct isxLoopVar *lv;
int i;

/* Add reserved registers. */
for (lv = loopy->hotLive; lv != NULL; lv = lv->next)
    {
    struct isxReg *reg = lv->reg;
    struct isxAddress *iad = lv->iad;
    if (reg == NULL)
        break;
    if (reg->contents != iad)
        {
	if (reg->contents != NULL)
	    {
	    reg->contents->reg = NULL;
	    reg->contents = NULL;
	    }
	if (iad->reg != reg)
	    codeOpDestReg(opMov, iad, reg, coder);
	reg->reserved = reg->contents = iad;
	iad->reg = reg;
	}
    reg->reserved = iad;
    }
forgetUnreservedRegs();

pentCoderAdd(coder, "jmp", isx->right->name, NULL);
pentCoderAdd(coder, NULL, NULL, isx->dest->name);
}

static void pentLoopEnd(struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder)
/* Code end of a loop.  This is just unreserving the registers 
 * and printing the end of loop label. */
{
struct isxLoopInfo *loopy = isx->left->val.loopy;
struct isxLoopVar *lv;
for (lv = loopy->hotLive; lv != NULL; lv = lv->next)
    {
    struct isxReg *reg = lv->reg;
    struct isxAddress *iad = lv->iad;
    if (reg == NULL)
        break;
    reg->reserved = NULL;
    }
pentCoderAdd(coder, NULL, NULL, isx->dest->name);
}

static void pentJump(struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder)
/* Do unconditional jump. */
{
pentCoderAdd(coder, "jmp", isx->dest->name, NULL);
}

static char *flipRightLeftInJump(char *jmpOp)
/* Turn jump to jump you'd get if comparison were reversed. */
{
if (sameString(jmpOp, "jl")) jmpOp = "jge";
else if (sameString(jmpOp, "jle")) jmpOp = "jg";
else if (sameString(jmpOp, "jge")) jmpOp = "jl";
else if (sameString(jmpOp, "jg")) jmpOp = "jle";
return jmpOp;
}


static void pentCmpJump(struct isx *isx, struct dlNode *nextNode,
	char *jmpOp, struct pentCoder *coder)
/* Output conditional jump code for == != < <= => > . */
{
if (isx->left->reg)
    {
    codeOp(opCmp, isx->right, isx->left, coder);
    }
else if (isx->right->reg)
    {
    codeOp(opCmp, isx->left, isx->right, coder);
    jmpOp = flipRightLeftInJump(jmpOp);
    }
else
    {
    struct isxReg *reg = freeReg(isx, isx->left->valType, nextNode, coder);
    codeOpDestReg(opMov, isx->left, reg, coder);
    isx->left->reg = reg;
    reg->contents = isx->left;
    codeOpDestReg(opCmp, isx->right, reg, coder);
    }
pentCoderAdd(coder, jmpOp, NULL, isx->dest->name);
}

static void pentTestJump(struct isx *isx, struct dlNode *nextNode,
	char *jmpOp, struct pentCoder *coder)
/* Output conditional jump code for == 0/!= 0. */
{
if (isx->left->reg)
    codeOp(opTest, isx->left, isx->left, coder);
else
    {
    struct isxReg *reg = freeReg(isx, isx->left->valType, nextNode, coder);
    codeOpDestReg(opMov, isx->left, reg, coder);
    isx->left->reg = reg;
    reg->contents = isx->left;
    codeOpDestReg(opTest, isx->left, reg, coder);
    }
pentCoderAdd(coder, jmpOp, NULL, isx->dest->name);
}

static void calcInputOffsets(struct pentFunctionInfo *pfi, struct dlList *iList)
/* Go through and fix stack offsets for input parameters */
{
struct dlNode *node;
int offset = 0;
for (node = iList->head; !dlEnd(node); node = node->next)
    {
    struct isx *isx = node->val;
    switch (isx->opType)
        {
	case poInput:
	    isx->dest->val.ioOffset = offset;
	    offset += pentTypeSize(isx->dest->valType);
	    if (offset > pfi->callParamSize)
	         pfi->callParamSize = offset;
	    break;
	case poCall:
	    offset = 0;
	    break;
	}
    }
}

struct regStompStack
    {
    struct regStompStack *next;	/* Next in stack. */
    struct regStomper *stomp;	/* Info at this node. */
    struct dlNode *node;	/* Node where we pushed this */
    int loopCount;		/* How many times we've gone through loop. */
    struct regStomper *condStomp;/* Stomper at start of condition. */
    boolean gotCondStomp;	/* True if we've set cond info */
    };


static void pushRegStomper(struct regStompStack **pStack, 
	struct regStomper *stomp, struct dlNode *node)
/* Alloc and push live stack entry */
{
struct regStompStack *is;
AllocVar(is);
is->stomp = stomp;
is->node = node;
slAddHead(pStack, is);
}

static void popRegStomper(struct regStompStack **pStack)
/* Pop and free live stack entry */
{
struct regStompStack *is = *pStack;
assert(is != NULL);
*pStack = is->next;
freeMem(is);
}

static void foldInCaseRegStomper(struct regStompStack *is, 
	struct regStomper *stomp)
/* Fold the liveList into the condLiveList. */
{
struct regStomper *cond = is->condStomp;
int i;
if (is->gotCondStomp)
    {
    for (i=0; i<ArraySize(stomp->stompPos); ++i)
	{
	cond->stompPos[i] = min(cond->stompPos[i], stomp->stompPos[i]);
	cond->contents[i] = NULL;
	}
    }
else
    {
    AllocVar(cond);
    is->condStomp = cond;
    *cond = *stomp;
    is->gotCondStomp = TRUE;
    }
}

static void initStompPos(struct regStomper *stomp)
/* Initialize all stompPos in stomp to val.   This will
 * by default convince system that ax is the first register
 * to use for unstable values, and bx the last. */
{
int i;
assert(ArraySize(stomp->stompPos) <= 6);/* Reminder to fix for floating point */
stomp->stompPos[ax] = 1;
stomp->stompPos[dx] = 2;
stomp->stompPos[cx] = 3;
stomp->stompPos[si] = 4;
stomp->stompPos[di] = 5;
stomp->stompPos[bx] = 6;
for (i=0; i<ArraySize(stomp->contents); ++i)
    stomp->contents[i] = NULL;
}


static void stompFromIsx(struct isx *isx, struct regStomper *stomp)
/* Set stompPos to 0 where required by isx instruction.  Doesn't include
 * loops. */
{
switch (isx->opType)
    {
    case poCall:
       stomp->stompPos[ax] = 0;
       stomp->contents[ax] = 0;
       stomp->stompPos[cx] = 0;
       stomp->contents[cx] = 0;
       stomp->stompPos[dx] = 0;
       stomp->contents[dx] = 0;
       break;
    case poDiv:
    case poMod:
       switch (isx->dest->valType)
	   {
	   case ivByte:
	   case ivShort:
	   case ivInt:
	       stomp->stompPos[ax] = 0;
	       stomp->contents[ax] = isx->left;
	       stomp->stompPos[dx] = 0;
	       stomp->contents[dx] = 0;
	       break;
	   }
	if (isx->right->adType == iadConst)
	   {
	   stomp->stompPos[cx] = 0;
	   stomp->contents[cx] = 0;
	   }
	break;
    case poShiftLeft:
    case poShiftRight:
	if (isx->right->adType != iadConst)
	    {
	    stomp->stompPos[cx] = 0;
	    stomp->contents[cx] = isx->right;
	    }
	break;
    }
}

static void addRegStomper(struct dlList *iList)
/* Add regStomper to all instructions. */
{
int i;
struct dlNode *node;
struct regStomper *stomp;
struct regStompStack *stompStack = NULL;
AllocVar(stomp);

/* Initialize stomp positions to past end of code */
initStompPos(stomp);

for (node = iList->tail; !dlStart(node); node = node->prev)
    {
    struct isx *isx = node->val;

    /* Save away current stomp list. */
    isx->cpuInfo = stomp;
    switch (isx->opType)
        {
	case poCondEnd:
	    pushRegStomper(&stompStack, stomp, node);
	    break;
	case poCondCase:
	    foldInCaseRegStomper(stompStack, stomp);
	    break;
	case poCondStart:
	    stomp = stompStack->condStomp;
	    stompStack->condStomp = NULL;
	    popRegStomper(&stompStack);
	    break;
	}

    /* Make new copy of stomper and bump all stomp positions. */
    stomp = CloneVar(stomp);
    for (i=0; i<ArraySize(stomp->stompPos); ++i)
	stomp->stompPos[i] += 1;

    /* Snoop through stomping instructions, and set stomps. */
    stompFromIsx(isx, stomp);
    if (isx->opType == poLoopStart)
        {
	/* Stomp variables used in loops */
	struct isxLoopVar *lv;
	struct isxLoopInfo *loopy = isx->left->val.loopy;
	for (lv = loopy->hotLive; lv != NULL; lv = lv->next)
	    {
	    int ix;
	    if (lv->reg == NULL)
	        break;
	    ix = lv->reg->regIx;
	    stomp->stompPos[ix] = 0;
	    stomp->contents[ix] = lv->iad;
	    }
	}
    else
        stompFromIsx(isx, stomp);

    }
assert(stompStack == NULL);
freeMem(stomp);
}

static int findUnstomped(struct regStomper *stomp, enum isxValType valType)
/* Find the unstomped register with the shortest life */
{
int lifetime = BIGNUM;
int bestReg = -1;
int i;
for (i=0; i<ArraySize(stomp->stompPos); ++i)
    {
    if (isxRegName(&regInfo[i], valType) != NULL)
	{
	int pos = stomp->stompPos[i];
	if (pos != 0 && pos < lifetime)
	     {
	     lifetime = pos;
	     bestReg = i;
	     }
	}
    }
return bestReg;
}

static void regStomperDump(struct regStomper *stomp, enum isxValType valType,
	FILE *f)
/* Dump out contents of stomper. */
{
int i;
for (i=0; i<ArraySize(stomp->stompPos); ++i)
    fprintf(f, "%s:%d ", isxRegName(&regInfo[i], valType), stomp->stompPos[i]);
}

static void mergeStomper(struct regStomper *a, struct regStomper *b, 
	struct regStomper *d)
/* Merge a/b into d */
{
int i;
for (i=0; i<ArraySize(d->stompPos); ++i)
    {
    d->stompPos[i] = min(a->stompPos[i], b->stompPos[i]);
    }
}

static void rCalcLoopRegVars(struct isxLoopInfo *loopy, 
	struct regStomper *stomp)
/* Do depth-first-recursion to assign register loop variables. */
{
struct isxLoopInfo *l;
struct dlNode *node;
struct isx *isx;
struct isxLoopVar *lv;
int i;
struct regStomper origStomp = *stomp;

/* Fill in info about children. */
for (l = loopy->children; l != NULL; l = l->next)
    {
    struct regStomper childStomp = origStomp;
    rCalcLoopRegVars(l, &childStomp);
    mergeStomper(&childStomp, stomp, stomp);
    }

/* Figure out all registers stomped. */
for (node = loopy->start; node != loopy->end; node = node->next)
    {
    isx = node->val;
    stompFromIsx(isx, stomp);
    }

/* Always stomp on eax,edx, to prevent system from reserving too many
 * variables for loops. (Might be better to do a calculation of
 * number of temporaries required here.) */
stomp->stompPos[ax] = 0;
stomp->stompPos[dx] = 0;

/* Reserve registers from most frequently used to least frequently
 * used. */
for (lv = loopy->hotLive; lv != NULL; lv = lv->next)
    {
    int unstomped = findUnstomped(stomp, lv->iad->valType);
    if (unstomped >= 0)
        {
	lv->reg = &regInfo[unstomped];
	stomp->stompPos[unstomped] = 0;
	}
    else
        break;
    }
}

static void calcLoopRegVars(struct isxList *isxList)
/* Calculate register vars to use in loops */
{
struct isxLoopInfo *loopy;
struct regStomper stomp;

ZeroVar(&stomp);

for (loopy = isxList->loopList; loopy != NULL; loopy  = loopy->next)
    {
    initStompPos(&stomp);
    rCalcLoopRegVars(loopy, &stomp);
    }
}

void pentFromIsx(struct isxList *isxList, struct pentFunctionInfo *pfi)
/* Convert isx code to pentium instructions in file. */
{
int i;
struct isxReg *destReg;
struct dlNode *node, *nextNode;
struct isx *isx;
struct pentCoder *coder = pfi->coder;
struct regStomper *stomp;
struct dlList *iList = isxList->iList;
int stackUse;

initRegInfo();
calcInputOffsets(pfi, iList);
calcLoopRegVars(isxList);
addRegStomper(iList);

stackUse = pfi->outVarSize + pfi->locVarSize + pfi->callParamSize;
coder->tempIx -= stackUse;

for (node = iList->head; !dlEnd(node); node = nextNode)
    {
    nextNode = node->next;
    isx = node->val;
    stomp = isx->cpuInfo;
#undef HELP_DEBUG
#ifdef HELP_DEBUG
    fprintf(f, "# ");	
    isxDump(isx, f);
    fprintf(f, "[");
    for (i=0; i<ArraySize(regInfo); ++i)
        {
	struct isxReg *reg = &regInfo[i];
	fprintf(f, "%s", isxRegName(reg, ivInt));
	if (reg->contents != NULL)
	    fprintf(f, "@%s", reg->contents->name);
	if (reg->reserved != NULL)
	    fprintf(f, "!%s", reg->reserved->name);
	fprintf(f, " ");
	}
    fprintf(f, "]\n");	
    fprintf(f, "# stomps ax bx cx dx si di ");
    for (i=0; i<ArraySize(regInfo); ++i)
	fprintf(f, " %d", stomp->stompPos[i]);
    fprintf(f, "\n");
#endif /* HELP_DEBUG */
    switch (isx->opType)
        {
	case poInit:
	case poAssign:
	    pentAssign(isx, nextNode, coder);
	    break;
	case poPlus:
	    pentBinaryOp(isx, nextNode, opAdd, FALSE, coder);
	    break;
	case poMul:
	    pentBinaryOp(isx, nextNode, opMul, FALSE, coder);
	    break;
	case poMinus:
	    pentBinaryOp(isx, nextNode, opSub, TRUE, coder);
	    break;
	case poDiv:
	    pentModDivide(isx, nextNode, FALSE, coder);
	    break;
	case poMod:
	    pentModDivide(isx, nextNode, TRUE, coder);
	    break;
	case poBitAnd:
	    pentBinaryOp(isx, nextNode, opAnd, TRUE, coder);
	    break;
	case poBitOr:
	    pentBinaryOp(isx, nextNode, opOr, TRUE, coder);
	    break;
	case poBitXor:
	    pentBinaryOp(isx, nextNode, opXor, TRUE, coder);
	    break;
	case poShiftLeft:
	    pentShiftOp(isx, nextNode, opSal, coder);
	    break;
	case poShiftRight:
	    pentShiftOp(isx, nextNode, opSar, coder);
	    break;
	case poInput:
	    pentInput(isx, nextNode, coder);
	    break;
	case poOutput:
	    pentOutput(isx, nextNode, coder);
	    break;
	case poCall:
	    pentCall(isx, coder);
	    break;
	case poLoopStart:
	    pentLoopStart(isx, nextNode, coder);
	    break;
	case poLoopEnd:
	    pentLoopEnd(isx, nextNode, coder);
	    break;
	case poLabel:
	case poCondEnd:
	    pentCoderAdd(coder, NULL, NULL, isx->dest->name);
	    forgetUnreservedRegs();
	    break;
	case poCondCase:
	    pentCoderAdd(coder, NULL, NULL, isx->dest->name);
	    break;
	case poCondStart:
	    break;
	case poBlt:
	    pentCmpJump(isx, nextNode, "jl", coder);
	    break;
	case poBle:
	    pentCmpJump(isx, nextNode, "jle", coder);
	    break;
	case poBge:
	    pentCmpJump(isx, nextNode, "jge", coder);
	    break;
	case poBgt:
	    pentCmpJump(isx, nextNode, "jg", coder);
	    break;
	case poBeq:
	    pentCmpJump(isx, nextNode, "je", coder);
	    break;
	case poBne:
	    pentCmpJump(isx, nextNode, "jne", coder);
	    break;
	case poBz:
	    pentTestJump(isx, nextNode, "jz", coder);
	    break;
	case poBnz:
	    pentTestJump(isx, nextNode, "jnz", coder);
	    break;
	case poJump:
	    pentJump(isx, nextNode, coder);
	    break;
	case poFuncStart:
	    break;
	case poFuncEnd:
	    break;
	default:
	    warn("unimplemented\t%s\n", isxOpTypeToString(isx->opType));
	    break;
	}
    }
slReverse(&coder->list);
pfi->tempVarSize = stackUse + coder->tempIx;
// uglyf("stackUse %d, coder->tempIx %d\n", stackUse, coder->tempIx);
}

static int alignOffset(int offset, int size)
/* Align offset to go with variable of a given size */
{
if (size == 2)
    offset = ((offset+1)&0xFFFFFFFE);
else if (size >= 4)
    offset = ((offset+3)&0xFFFFFFFC);
return offset;
}

static int calcVarListSize(struct pfCompile *pfc, struct slRef *varRefList, 
	int varCount)
/* Figure out size of vars - align things to be on even boundaries. */
{
int i, size1, offset = 0;
struct slRef *varRef; 
for (i=0,varRef = varRefList; i<varCount; ++i, varRef=varRef->next)
    {
    struct pfVar *var = varRef->val;
    size1 = pentTypeSize(isxValTypeFromTy(pfc, var->ty));
    offset = alignOffset(offset, size1);
    offset += size1;
    }
offset = alignOffset(offset, 4);
return offset;
}

static void fillInVarOffsets(struct pfCompile *pfc, int offset,
	struct slRef *varRefList, int varCount, struct hash *varHash)
/* Fill in stack offsets */
{
int i, size1;
struct slRef *varRef; 
for (i=0,varRef = varRefList; i<varCount; ++i, varRef=varRef->next)
    {
    struct pfVar *var = varRef->val;
    struct isxAddress *iad = hashFindVal(varHash, var->cName);
    size1 = pentTypeSize(isxValTypeFromTy(pfc, var->ty));
    offset = alignOffset(offset, size1);
    iad->stackOffset = offset;
    // uglyf("fillInVarOffset %s@%d\n", var->cName, offset);
    offset += size1;
    }
}

void pentInitFuncVars(struct pfCompile *pfc, struct ctar *ctar, 
	struct hash *varHash, struct pentFunctionInfo *pfi)
/* Set up variables and offsets for parameters and local variables
 * in hash. */
{
struct slRef *varRef;
struct pfVar *var;
struct isxAddress *iad;
int stackSubAmount;
int retPlusBp = 8;	/* Space for return address and ebp */
int savedRegs = 12;	/* Space for ebx, esi, edi */

pfi->savedContextSize = retPlusBp + savedRegs; 
pfi->outVarSize = calcVarListSize(pfc, ctar->outRefList, ctar->outCount);
pfi->locVarSize = calcVarListSize(pfc, ctar->localRefList, ctar->localCount);
fillInVarOffsets(pfc, retPlusBp, ctar->inRefList, ctar->inCount, 
	varHash);
stackSubAmount = savedRegs+pfi->outVarSize;
fillInVarOffsets(pfc, -stackSubAmount, ctar->outRefList, ctar->outCount, 
	varHash);
stackSubAmount += pfi->locVarSize;
fillInVarOffsets(pfc, -stackSubAmount, ctar->localRefList, ctar->localCount, 
	varHash);
}

void pentFunctionStart(struct pfCompile *pfc, struct pentFunctionInfo *pfi, 
	char *cName, boolean isGlobal, FILE *f)
/* Finish coding up a function in pentium assembly language. */
{
int stackSubAmount;
// uglyf("pentFunctionStart %s\n", cName);
// uglyf("outVarSize %d, locVarSize %d, tempVarSize %d, callParamSize %d\n", pfi->outVarSize, pfi->locVarSize, pfi->tempVarSize, pfi->callParamSize);
if (isGlobal)
    fprintf(f, ".globl %s%s\n", isxPrefixC, cName);
fprintf(f, "%s%s:\n", isxPrefixC, cName);
fprintf(f, "%s",
"	pushl	%ebp\n"
"	movl	%esp,%ebp\n"
"	pushl	%ebx\n"
"	pushl	%esi\n"
"	pushl	%edi\n");
stackSubAmount = pfi->outVarSize + pfi->locVarSize + pfi->tempVarSize 
			+ pfi->callParamSize + pfi->savedContextSize;
stackSubAmount = ((stackSubAmount+15)&0xFFFFFFF0);
stackSubAmount -= pfi->savedContextSize;
pfi->stackSubAmount = stackSubAmount;
fprintf(f, "\tsubl\t$%d,%%esp\n", stackSubAmount);
}

void pentFunctionEnd(struct pfCompile *pfc, struct pentFunctionInfo *pfi, 
	FILE *f)
/* Finish coding up a function in pentium assembly language. */
{
fprintf(f, "\taddl\t$%d,%%esp\n", pfi->stackSubAmount);
fprintf(f, "%s",
"	popl	%edi\n"
"	popl	%esi\n"
"	popl	%ebx\n"
"	popl	%ebp\n"
"	ret\n");
}

