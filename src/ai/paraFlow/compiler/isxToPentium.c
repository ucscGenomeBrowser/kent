/* isxToPentium - convert isx code to Pentium code.  This is done
 * one function at a time.
 *
 * The register picking algorithm in the routine getFreeReg may
 * be the most important part of this code. The algorithm originated
 * in Aho/Ulman 1979, but has diverged here quite a bit.
 *
 * The isx instructions that are the input to this module are primarily
 * in the form:
 *        dest = left <op> right
 * where collectively left and right are called "sources."
 * The getFreeReg routine is called to find a register for the dest.
 * The system keeps track of which variables are in which registers,
 * how soon (if ever) these variables will be used again, and how soon
 * the register will be "stomped" by some event that will demand that
 * register in particular. The getFreeReg routine tries to return
 * a register that will get stomped right after the dest variable is
 * no longer needed.  Failing this it searches in order for:
 *         a register that holds a source that will no longer be used
 *         any register that isn't holding a live value
 *         a register holding a live value that is also in memory
 *         a register holding a live value that must be saved to memory
 *
 * The life-time of variables and the stomp-time of registers is
 * calculated by scanning the isx instructions backwards.  This
 * scan is complicated a bit by loops and conditional statements.
 * Within the straight-line code the lifetime of a variable starts
 * when it appears as either a left or right source operand in the isx
 * code, and the lifetime ends when the variable a dest operand.
 * In the backwards scan, loops are passed through twice, and
 * the second iteration is used to determine life-times.  Various
 * branches of conditionals are done separately, and the results 
 * merge at the start of the condition.
 *
 * Registers are stomped by various events.  Certain pentium instructions
 * such as integer divide, demand the use of certain registers.
 * Subroutine calls will stomp all registers except esi, ebi, and ebx.
 * The most commonly used variables in a loop are assigned to registers,
 * and so effectively loops can stomp registers as well. 
 *
 * I'm sure the register allocation could be improved (it is an NP
 * complete problem after all!) but it is not bad. The system does
 * make sure to write user-declared variables to memory as soon as
 * possible as well as keeping them in memory.  The first optimization
 * to this would probably be to relax this constraint.  However
 * it would be a little tricky, since the exception handling automatic
 * cleanup does need to know where all the variables are.  Basically
 * this would force all the variables in registers to be saved at
 * every subroutine call anyway, since ParaFlow does not track which
 * subroutines can cause an exception.  There's a few places where
 * the code generator assumes that real variables are in memory as well
 * currently.
 *
 * This generates code for Pentium 4/Xeon processors and above, which
 * are what Intel has been producing since 2000.  In particular it depends
 * on having the SSE2 extensions.  These let us do floating point via
 * the xmm registers rather than the older 8087 floating point stack.
 * This lets us us the same register allocation scheme for floating point
 * as regular values.  Somewhat perversely then we use use the mm registers
 * for 64-bit-longs.  These share the same space as the 8087 stack.  The
 * 64-bit-longs fit somewhat awkwardly in the mm registers, but using
 * them avoids having to devote eax:edx to long operations, which would
 * starve the pointer/integer registers more than they already are.
 * Here's a register-by-register description:
 *   eax - used as al for bytes, as ax for shorts, as eax for ints and pointers
 *         Also used for integer divide.
 *   ebx - used as bl for bytes, as bx for shorts, as ebx for ints and pointers
 *   ecx - used as cl for bytes, as cx for shorts, as ecx for ints and pointers
 *         Also used for shifts by non-constant amounts, and for string ops
 *   edx - used as dl for bytes, as dx for shorts, as edx for ints and pointers
 *         Also used for integer divide.
 *   esi - used as si for shorts, as esi for ints and pointers
 *         Also used for string ops.
 *   edi - used as di for shorts, as edi for ints and pointers
 *         Also used for string ops.
 *   ebp - Not allocated, used in stack frames.
 *   esp - Not allocated, used as stack pointer.
 *   mm0-mm7 - Used for long (64 bit) integers
 *   xmm0-xmm7 - Used for float (32 bit) and double (64 bit) numbers.
 *
 * The function call register conventions are compatible with Pentium
 * C conventions.  All input is passed on the stack.  The registers esp, 
 * ebp, esi, edi, ebx are preserved by function calls, and all others are 
 * potentially trashed. Return values are stored in:
 *     eax - for pointer and integers
 *     eax:edx - for long longs
 *     st0 - for floating point values
 * For functions returning multiple values the first value follows the
 * C conventions.  Other registers will be used up to a point, and
 * then past that the values will be returned on the stack.  The convention
 * in full is:
 *            int    long/long   float
 *      1     eax     eax:edx    st0
 *      2     edx     mm1        xmm1
 *      3     ecx     mm2        xmm2
 *      4     stack   mm3        xmm3
 *      5     stack   mm4        xmm4
 *      6     stack   mm5        xmm5
 *      7     stack   mm6        xmm6
 *      8     stack   mm7        xmm7
 *      9+    stack   stack      stack
 * If a return value is on the stack, it is in the same place it
 * would be if it were an input on the stack.  For instance in
 * this function:
 *      to doSomething(int a, b, string c, short d, float e, double f, int g)
 *      into (int aa, bb, string cc, short dd, float ee, double ff, int gg)
 * The positions of various input and outputs are:
 *         a - 0(esp)    aa - eax
 *         b - 4(esp)    bb - edx
 *         c - 8(esp)    cc - 8(esp)
 *         d - 12(esp)   dd - 12(esp)
 *         e - 16(esp)   ee - xmm4
 *         f - 20(esp)   ff - xmm5
 *         g - 28(esp)   gg - 28(esp) 
 * Note in particular that items of less than 4 bytes will still take 4
 * bytes on stack. */

#include "common.h"
#include "hash.h"
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
#include "pentCast.h"

enum pentRegs
/* These need to be in same order as regInfo table. */
   {
   ax=0, bx=1, cx=2, dx=3, si=4, di=5, 
   xmm0=6,xmm1=7,xmm2=8,xmm3=9,xmm4=10,xmm5=11,xmm6=12,xmm7=13,
   mm0=14, mm1=15, mm2=16, mm3=17, mm4=18, mm5=19, mm6=20, mm7=21,
   };

static struct isxReg regInfo[pentRegCount] = {
    { 4, "%al", "%ax", "%eax", NULL, NULL, NULL, "%eax"},
    { 4, "%bl", "%bx", "%ebx", NULL, NULL, NULL, "%ebx"},
    { 4, "%cl", "%cx", "%ecx", NULL, NULL, NULL, "%ecx"},
    { 4, "%dl", "%dx", "%edx", NULL, NULL, NULL, "%edx"},
    { 4, NULL, "%si", "%esi", NULL, NULL, NULL, "%esi"},
    { 4, NULL, "%di", "%edi", NULL, NULL, NULL, "%edi"},
    { 8, NULL, NULL, NULL, NULL, "%xmm0", "%xmm0", NULL},
    { 8, NULL, NULL, NULL, NULL, "%xmm1", "%xmm1", NULL},
    { 8, NULL, NULL, NULL, NULL, "%xmm2", "%xmm2", NULL},
    { 8, NULL, NULL, NULL, NULL, "%xmm3", "%xmm3", NULL},
    { 8, NULL, NULL, NULL, NULL, "%xmm4", "%xmm4", NULL},
    { 8, NULL, NULL, NULL, NULL, "%xmm5", "%xmm5", NULL},
    { 8, NULL, NULL, NULL, NULL, "%xmm6", "%xmm6", NULL},
    { 8, NULL, NULL, NULL, NULL, "%xmm7", "%xmm7", NULL},
    { 8, NULL, NULL, NULL, "%mm0", NULL, NULL, NULL},
    { 8, NULL, NULL, NULL, "%mm1", NULL, NULL, NULL},
    { 8, NULL, NULL, NULL, "%mm2", NULL, NULL, NULL},
    { 8, NULL, NULL, NULL, "%mm3", NULL, NULL, NULL},
    { 8, NULL, NULL, NULL, "%mm4", NULL, NULL, NULL},
    { 8, NULL, NULL, NULL, "%mm5", NULL, NULL, NULL},
    { 8, NULL, NULL, NULL, "%mm6", NULL, NULL, NULL},
    { 8, NULL, NULL, NULL, "%mm7", NULL, NULL, NULL},
};

struct pentFunctionInfo *pentFunctionInfoNew(struct hash *constStringHash)
/* Create new pentFunctionInfo */
{
struct pentFunctionInfo *pfi;
AllocVar(pfi);
pfi->savedRegSize = 12;
pfi->savedRetEbpSize = 8;
pfi->savedContextSize = pfi->savedRetEbpSize + pfi->savedRegSize; 
pfi->coder = pentCoderNew(constStringHash);
return pfi;
}

static void initRegInfo()
/* Do some one-time initialization of regInfo */
{
int i;
for (i=0; i<pentRegCount; ++i)
    regInfo[i].regIx = i;
}

struct regStomper
/* Information on registers. */
    {
    int stompPos[pentRegCount];	
    struct isxAddress *contents[pentRegCount];
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
    {opMov, "movb", "movw", "movl", "movq", "movss", "movsd", "movl"},
    {opAdd, "addb", "addw", "addl", "paddq", "addss", "addsd", "addl"},
    {opSub, "subb", "subw", "subl", "psubq", "subss", "subsd", "subl"},
    {opMul, "imulb", "imulw", "imull", NULL, "mulss", "mulsd", NULL},
    {opDiv, "idivb", "idivw", "idivl", NULL, "divss", "divsd", NULL},
    {opAnd, "andb", "andw", "andl", "pand", NULL, NULL, NULL},
    {opOr,  "orb",  "orw",  "orl",  "por",  NULL, NULL, NULL},
    {opXor, "xorb", "xorw", "xorl", "pxor", NULL, NULL, NULL},
    {opSal, "salb", "salw", "sall", "psllq", NULL, NULL, NULL},
    {opSar, "sarb", "sarw", "sarl", NULL, NULL, NULL, NULL},
    {opNeg, "negb", "negw", "negl", NULL, NULL, NULL, NULL},
    {opNot, "notb", "notw", "notl", NULL, NULL, NULL, NULL},
    {opCmp, "cmpb", "cmpw", "cmpl", NULL, "ucomiss", "ucomisd", NULL},
    {opTest, "testb", "testw", "testl", NULL, NULL, NULL, "testl"},
    };

char *pentOpOfType(enum pentDataOp opType, enum isxValType valType)
/* Return string for given opCode and data type */
{
char *opString;
switch (valType)
    {
    case ivBit:
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
    case ivVar:
        opString = dataOpTable[opType].intName;
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
    case ivBit:
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
    case ivVar:
        return 12;
    default:
	internalErr();
	return 0;
    }
}

int pentTypeSize4(enum isxValType valType)
/* Return size of a val type - rounding up to 4*/
{
int ret = pentTypeSize(valType);
if (ret < 4) ret = 4;
return ret;
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

void pentPrintVarMemAddress(struct isxAddress *iad, char *buf, int offset)
/* Print out an address for variable in memory to buffer. */
{
switch (iad->adType)
    {
    case iadRealVar:
	{
	struct pfVar *var = iad->val.var;
	if (var->scope->isLocal)
	    {
	    safef(buf, pentCodeBufSize, "%d(%%ebp)", iad->stackOffset + offset);
	    }
	else
	    {
	    if (offset)
		safef(buf, pentCodeBufSize, "%s%s+%d", isxPrefixC, iad->name, offset);
	    else
		safef(buf, pentCodeBufSize, "%s%s", isxPrefixC, iad->name);
	    }
	break;
	}
    case iadTempVar:
	{
	safef(buf, pentCodeBufSize, "%d(%%ebp)", iad->val.tempMemLoc + offset);
	break;
	}
    case iadInStack:
    case iadOutStack:
	{
	safef(buf, pentCodeBufSize, "%d(%%esp)", iad->stackOffset + offset);
	break;
	}
    case iadConst:
	{
	switch (iad->valType)
	    {
	    case ivFloat:
	    case ivDouble:
	    case ivLong:
		pentFloatOrLongLabel(buf, pentCodeBufSize, iad->valType, iad);
		if (offset)
		    {
		    int len = strlen(buf);
		    safef(buf+len, pentCodeBufSize - len, "+%d", offset);
		    }
		break;
	    default:
	        internalErr();
		break;
	    }
	}
    }
}

void pentPrintAddress(struct pentCoder *coder,
	struct isxAddress *iad, char *buf)
/* Print out an address for an instruction. */
{
if (iad->reg != NULL)
    {
    safef(buf, pentCodeBufSize, "%s", 
	isxRegName(iad->reg, iad->valType));
    }
else
    {
    switch (iad->adType)
	{
	case iadConst:
	    {
	    switch (iad->valType)
		{
		case ivFloat:
		case ivDouble:
		case ivLong:
		    pentFloatOrLongLabel(buf, pentCodeBufSize, iad->valType, iad);
		    break;
		case ivString:
		    {
		    char *s = hashFindVal(coder->constStringHash, 
		    	iad->val.isxTok.val.s);
		    assert(s != NULL);
		    safef(buf, pentCodeBufSize, "$%s", s);
		    break;
		    }
		default:
		    safef(buf, pentCodeBufSize, "$%d", iad->val.isxTok.val.i);
		    break;
		}
	    break;
	    }
	case iadRealVar:
	case iadTempVar:
	case iadInStack:
	case iadOutStack:
	    pentPrintVarMemAddress(iad, buf, 0);
	    break;
	case iadReturnVar:
	    {
	    safef(buf, pentCodeBufSize, "%d(%%ebp)", iad->stackOffset);
	    break;
	    }
	case iadRecodedType:
	    {
	    safef(buf, pentCodeBufSize, "%d*8+__pf_lti_varType", iad->val.recodedType);
	    break;
	    }
	default:
	    internalErr();
	    break;
	}
    }
}

static void codeOp(enum pentDataOp opType, struct isxAddress *source,
	struct isxAddress *dest, struct pentCoder *coder)
/* Print op to file */
{
char *opName = pentOpOfType(opType, source->valType);
char *destString = NULL;
pentPrintAddress(coder, source, coder->sourceBuf);
if (dest != NULL)
    {
    pentPrintAddress(coder, dest, coder->destBuf);
    destString = coder->destBuf;
    }
pentCoderAdd(coder, opName, coder->sourceBuf, destString);
}

static void pentCodeSourceRegVarType(enum pentDataOp opType, struct isxReg *reg,
	struct isxAddress *dest,  enum isxValType valType, 
	struct pentCoder *coder)
/* Print op where source is a register, and valType might not match
 * dest type */
{
char *opName = pentOpOfType(opType, valType);
char *regName = isxRegName(reg, valType);
pentPrintAddress(coder, dest, coder->destBuf);
pentCoderAdd(coder, opName, regName, coder->destBuf);
}

static void pentCodeSourceReg(enum pentDataOp opType, struct isxReg *reg,
	struct isxAddress *dest,  struct pentCoder *coder)
/* Print op where source is a register. */
{
pentCodeSourceRegVarType(opType, reg, dest, dest->valType, coder);
}

void pentCodeDestReg(enum pentDataOp opType, struct isxAddress *source, 
	struct isxReg *reg,  struct pentCoder *coder)
/* Code op where dest is a register. */
{
enum isxValType valType = source->valType;
char *opName = pentOpOfType(opType, valType);
char *regName = isxRegName(reg, valType);
pentPrintAddress(coder, source, coder->sourceBuf);
pentCoderAdd(coder, opName, coder->sourceBuf, regName);
}

static void unaryOpReg(enum pentDataOp opType, struct isxReg *reg, 
	enum isxValType valType, struct pentCoder *coder)
/* Do unary op on register. */
{
char *regName = isxRegName(reg, valType);
char *opName = pentOpOfType(opType, valType);
pentCoderAdd(coder, opName, NULL, regName);
}


static void clearReg(struct isxReg *reg,  struct pentCoder *coder)
/* Clear out register. */
{
switch (reg->regIx)
    {
    case ax:
    case bx:
    case cx:
    case dx:
    case si:
    case di:
	{
	char *name = reg->pointerName;
	pentCoderAdd(coder, "xorl", name, name);
	break;
	}
    case xmm0:
    case xmm1:
    case xmm2:
    case xmm3:
    case xmm4:
    case xmm5:
    case xmm6:
    case xmm7:
        {
	char *name = reg->doubleName;
	pentCoderAdd(coder, "subsd", name, name);
	break;
	}
    case mm0:
    case mm1:
    case mm2:
    case mm3:
    case mm4:
    case mm5:
    case mm6:
    case mm7:
        {
	char *name = reg->longName;
	pentCoderAdd(coder, "pxor", name, name);
	break;
	}
    default:
        internalErr();
	break;
    }
}

static void clearRegContents(struct isxReg *reg)
/* Clear out register contents */
{
if (reg->contents)
    reg->contents->reg = NULL;
reg->contents = NULL;
}

void pentSwapTempFromReg(struct isxReg *reg,  struct pentCoder *coder)
/* If reg contains something not also in memory then save it out. */
{
struct isxAddress *iad = reg->contents;
struct isxAddress old = *iad;

coder->tempIx -= reg->size;
iad->reg = NULL;
iad->val.tempMemLoc = coder->tempIx;
codeOp(opMov, &old, iad, coder);
}

boolean pentTempJustInReg(struct isxAddress *iad)
/* Return TRUE if address is for a temp that only exists in
 * a register, not in memory */
{
return (iad->adType == iadTempVar && iad->val.tempMemLoc == 0);
}

static void pentSwapOutIfNeeded(struct isxReg *reg, 
	struct isxLiveVar *liveList, struct pentCoder *coder)
/* Swap out register to memory if need be. */
{
struct isxAddress *iad = reg->contents;
if (iad != NULL)
    {
    if (pentTempJustInReg(iad))
	{
	if (isxLiveVarFind(liveList, iad))
	    pentSwapTempFromReg(reg, coder);
	}
    else
	iad->reg = NULL;
    }
reg->contents = NULL;
}

static struct isxReg *getFreeReg(struct isx *isx, enum isxValType valType,
	struct dlNode *nextNode,  struct pentCoder *coder)
/* Find free register for instruction result. */
{
int i;
struct isxAddress *dest = isx->dest;
struct isxReg *regs = regInfo;
int regStartIx, regEndIx;
struct isxReg *reg, *freeReg = NULL;
struct regStomper *stomp = isx->cpuInfo;
struct isxLiveVar *live;

switch (valType)
    {
    case ivFloat:
    case ivDouble:
	 regStartIx = xmm0;
	 regEndIx =  xmm7;
	 break;
    case ivLong:
    	regStartIx = mm0;
	regEndIx = mm7;
	break;
    case ivByte:
    case ivBit:
        regStartIx = ax;
	regEndIx = dx;
	break;
    default:
	regStartIx = ax;
	regEndIx = di;
	break;
    }
if (dest != NULL)
    {
    /* If destination is reserved or is the one that stomps, by all means
     * use corresponding register.  */
    for (i=regStartIx; i<=regEndIx; ++i)
	{
	reg = &regs[i];
	if (reg->reserved == dest || stomp->contents[i] == dest)
	    return reg;
	}
    }

/* Look for a register holding a source that is no longer live. */
    {
    int freeIx = BIGNUM;
    for (i=regStartIx; i<=regEndIx; ++i)
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
for (i=regStartIx; i<=regEndIx; ++i)
    regs[i].isLive = FALSE;
for (live = isx->liveList; live != NULL; live = live->next)
    if ((reg = live->var->reg) != NULL)
	reg->isLive = TRUE;
for (i=regStartIx; i<=regEndIx; ++i)
     {
     reg = &regs[i];
     if (!reg->isLive && !reg->reserved)
	 {
	 clearRegContents(reg);
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
	for (i=regStartIx; i<=regEndIx; ++i)
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
	for (i=regStartIx; i<=regEndIx; ++i)
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
	for (i=regStartIx; i<=regEndIx; ++i)
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
	    // uglyf("Evicting %s*%2.1f from %s for %s*%2.1f\n", freeReg->contents->name, freeReg->contents->weight, isxRegName(freeReg, valType), dest->name, dest->weight);
	    return freeReg;
	    }
	}
    }

/* If no luck yet, look for any free register */
for (i=regStartIx; i<=regEndIx; ++i)
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
    for (i=regStartIx; i<=regEndIx; ++i)
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
    for (i=regStartIx; i<=regEndIx; ++i)
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
    for (i=regStartIx; i<=regEndIx; ++i)
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

struct isxReg *pentFreeReg(struct isx *isx, enum isxValType valType,
	struct dlNode *nextNode,  struct pentCoder *coder)
/* Find free register for instruction result. */
{
struct isxReg *reg = getFreeReg(isx, valType, nextNode, coder);
coder->regsUsed[reg->regIx] = TRUE;
return reg;
}


static void copyRealVarToMem(struct isxAddress *iad, struct pentCoder *coder)
/* Copy address to memory if it's in a register and it's a real var. */
{
if (iad->adType == iadRealVar)
    {
    struct isxReg *reg = iad->reg;
    iad->reg = NULL;
    if (reg != NULL)
	pentCodeSourceReg(opMov, reg, iad, coder);
    iad->reg = reg;
    }
}

void pentLinkReg(struct isxAddress *iad, struct isxReg *reg)
/* Link register to new address. */
{
if (reg->contents != NULL)
    reg->contents->reg = NULL;
reg->contents = iad;
iad->reg = reg;
}


void pentLinkRegSave(struct isxAddress *dest, struct isxReg *reg,
	struct pentCoder *coder)
/* Unlink whatever old variable was in register and link in dest.
 * Also copy dest to memory if it's a real variable. */
{
pentLinkReg(dest, reg);
copyRealVarToMem(dest, coder);
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
    reg = pentFreeReg(isx, isx->dest->valType, nextNode, coder);
    clearReg(reg, coder);
    }
else
    {
    reg = dest->reg;
    if (reg == NULL)
	reg = pentFreeReg(isx, isx->dest->valType, nextNode, coder);
    if (source->reg != reg)
	pentCodeDestReg(opMov, source, reg, coder);
    }
pentLinkRegSave(dest, reg, coder);
}

static void pentVarAssign(struct isx *isx, struct dlNode *nextNode,  
	struct pentCoder *coder)
/* Code variable assignment */
{
struct isxAddress *source = isx->left;
struct isxAddress *sourceType = isx->right;
struct isxAddress *dest = isx->dest;
struct isxReg *reg;
char *regName;

if (source->valType == ivVar)
    {
    int i;
    assert(source->adType == iadRealVar || source->adType == iadTempVar);
    reg = pentFreeReg(isx, ivInt, nextNode, coder);
    regName = isxRegName(reg, ivInt);
    for (i=0; i<12; i += 4)
	{
	pentPrintVarMemAddress(source, coder->sourceBuf, i);
	pentCoderAdd(coder, "movl", coder->sourceBuf, regName);
	pentPrintVarMemAddress(dest, coder->destBuf, i);
	pentCoderAdd(coder, "movl", regName, coder->destBuf);
	}
    }
else
    {
    /* Move variable to dest */
    if (source->reg)
	{
	pentCodeSourceRegVarType(opMov, source->reg, dest, source->valType, 
		coder);
	}
    else
	{
	reg = pentFreeReg(isx, source->valType, nextNode, coder);
	if (source->adType == iadZero)
	    clearReg(reg, coder);
	else
	    pentCodeDestReg(opMov, source, reg, coder);
	pentCodeSourceRegVarType(opMov, reg, dest, source->valType, coder);
	clearRegContents(reg);
	}
    /* Move type to dest. */
    reg = pentFreeReg(isx, ivInt, nextNode, coder);
    regName = isxRegName(reg, ivInt);
    pentPrintAddress(coder, sourceType, coder->sourceBuf);
    pentCoderAdd(coder, "movl", coder->sourceBuf, regName);
    pentPrintVarMemAddress(dest, coder->destBuf, 8);
    pentCoderAdd(coder, "movl", regName, coder->destBuf);
    }
clearRegContents(reg);
}

static void pentVarInput(struct isx *isx, struct dlNode *nextNode,  
	struct pentCoder *coder)
/* Output code to load a variable input parameter before a call. */
{
pentVarAssign(isx, nextNode, coder);
}

static void pentBinaryOp(struct isx *isx, struct dlNode *nextNode, 
	enum pentDataOp opCode, boolean isSub,  struct pentCoder *coder)
/* Code most binary ops.  Division is harder. */
{
struct isxReg *reg = pentFreeReg(isx, isx->dest->valType, nextNode, coder);
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
	pentCodeDestReg(opAdd, iad, reg, coder);
	}
    else
	{
	pentCodeDestReg(opCode, iad, reg, coder);
	}
    }
else
    {
    pentCodeDestReg(opMov, isx->left, reg, coder);
    pentCodeDestReg(opCode, isx->right, reg, coder);
    }
pentLinkRegSave(isx->dest, reg, coder);
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

static void pentIntModDivide(struct isx *isx, struct dlNode *nextNode, 
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

addDummyToLive(p, &extraLive);
addDummyToLive(q, &extraLive);

if (q->adType == iadConst)
    {
    pentSwapOutIfNeeded(ecx, extraLive, coder);
    pentCodeDestReg(opMov, q, ecx, coder);
    ecx->contents = NULL;
    swappedOutC = TRUE;
    }
if (eax->contents != p)
    {
    pentSwapOutIfNeeded(eax, extraLive, coder);
    pentCodeDestReg(opMov, p, eax, coder);
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

static void pentFloatDivide(struct isx *isx, struct dlNode *nextNode, 
	struct pentCoder *coder)
/* Generate code for floating point divide. */
{
struct isxReg *reg = pentFreeReg(isx, isx->dest->valType, nextNode, coder);
if (reg != isx->left->reg)
    pentCodeDestReg(opMov, isx->left, reg, coder);
pentCodeDestReg(opDiv, isx->right, reg, coder);
pentLinkRegSave(isx->dest, reg, coder);
}

static void pentDivide(struct isx *isx, struct dlNode *nextNode, 
	struct pentCoder *coder)
/* Generate code for floating point or integer divide. */
{
enum isxValType valType = isx->dest->valType;
switch (valType)
    {
    case ivFloat:
    case ivDouble:
	pentFloatDivide(isx, nextNode, coder);
        break;
    case ivLong:
        internalErr();
	break;
    default:
         pentIntModDivide(isx, nextNode, FALSE, coder);
	 break;
    }
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
	pentCodeDestReg(opMov, p, eax, coder);
	}
    else
        {
	p->reg = NULL;
	}
    if (ecx != q->reg)
	{
	pentSwapOutIfNeeded(ecx,extraLive, coder);
	pentCodeDestReg(opMov, q, ecx, coder);
	}
    pentCoderAdd(coder, pentOpOfType(opCode, p->valType), 
    	isxRegName(ecx, ivByte), isxRegName(eax, p->valType));
    eax->contents = d;
    d->reg = eax;
    trimDummiesFromLive(extraLive, isx->liveList);
    copyRealVarToMem(d, coder);
    }
}

static void pentUnaryOp(struct isx *isx, enum pentDataOp opCode,
	struct dlNode *nextNode, struct pentCoder *coder)
/* Handle unary operation on byte/short/int data types */
{
enum isxValType valType = isx->dest->valType;
struct isxReg *reg = pentFreeReg(isx, valType, nextNode, coder);
if (reg != isx->left->reg)
    pentCodeDestReg(opMov, isx->left, reg, coder);
unaryOpReg(opCode, reg, valType, coder);
pentLinkRegSave(isx->dest, reg, coder);
}

static void pentNegate(struct isx *isx, struct dlNode *nextNode,  
	struct pentCoder *coder)
/* Output code to implement unary minus. */
{
enum isxValType valType = isx->dest->valType;
switch (valType)
    {
    case ivByte:
    case ivShort:
    case ivInt:
        pentUnaryOp(isx, opNeg, nextNode, coder);
	break;
    default:
        internalErr();
	break;
    }
}

static void pentFlipBitsLong(struct isx *isx, struct dlNode *nextNode,  
	struct pentCoder *coder)
/* Output code to implement binary not for long values. */
{
enum isxValType valType = isx->dest->valType;
struct isxReg *destReg = pentFreeReg(isx, valType, nextNode, coder);
char *destRegName = isxRegName(destReg, valType);
if (destReg != isx->left->reg)
    pentCodeDestReg(opMov, isx->left, destReg, coder);
pentCoderAdd(coder, "pandn", "longLongMinusOne", destRegName);
pentLinkRegSave(isx->dest, destReg, coder);
}

static void pentFlipBits(struct isx *isx, struct dlNode *nextNode,  
	struct pentCoder *coder)
/* Output code to implement binary not. */
{
enum isxValType valType = isx->dest->valType;
switch (valType)
    {
    case ivByte:
    case ivShort:
    case ivInt:
        pentUnaryOp(isx, opNot, nextNode, coder);
	break;
    case ivLong:
        pentFlipBitsLong(isx, nextNode, coder);
	break;
    default:
        internalErr();
	break;
    }
}

static void codeMoveExtendParam(struct isxAddress *source, 
	struct isxAddress *dest, struct pentCoder *coder)
/* Do extension to word if necessary on a parameter. */
{
struct isxReg *reg = source->reg;
enum isxValType valType = source->valType;
if (reg != NULL && (valType == ivByte || valType == ivShort))
    {
    char *regName = isxRegName(reg, ivInt);
    char *op = (valType == ivByte ? "movsbl" : "movswl");
    pentCoderAdd(coder, op, isxRegName(reg, valType), regName);
    pentPrintAddress(coder, dest, coder->destBuf);
    pentCoderAdd(coder, "movl", regName, coder->destBuf);
    }
else
    codeOp(opMov, source, dest, coder);
}
	
static void pentInput(struct isx *isx, struct dlNode *nextNode,  
	struct pentCoder *coder)
/* Output code to load an input parameter before a call. */
{
struct isxAddress *source = isx->left;
if (source->reg || (source->adType == iadConst 
	&& source->valType != ivFloat && source->valType != ivDouble
	&& source->valType != ivLong))
    codeMoveExtendParam(source, isx->dest, coder);
else
    {
    struct isxReg *reg = pentFreeReg(isx, isx->dest->valType, nextNode, coder);
    pentCodeDestReg(opMov, source, reg, coder);
    reg->contents = source;
    source->reg = reg;
    codeMoveExtendParam(source, isx->dest, coder);
    }
}

struct isxReg *outOffsetToReg(int ioOffset, enum isxValType valType)
/* Convert output offset to an output register */
{
struct isxReg *reg = NULL;

switch (valType)
    {
    case ivLong:
	if (ioOffset < 8)
	    reg = &regInfo[mm0 + ioOffset];
	break;
    case ivFloat:
    case ivDouble:
	if (ioOffset < 8)
	    reg = &regInfo[xmm0 + ioOffset];
	break;
    default:
	switch (ioOffset)
	    {
	    case 0:
		reg = &regInfo[ax];
		break;
	    case 1:
		reg = &regInfo[dx];
		break;
	    case 2:
		reg = &regInfo[cx];
		break;
	    }
	break;
    }
return reg;
}

static void pentOutput(struct isx *isx, struct dlNode *nextNode,  
	struct pentCoder *coder)
/* Output code to save an output parameter after a call. */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
enum isxValType valType = source->valType;
int sourceIx = source->val.ioOffset;
struct isxReg *reg = outOffsetToReg(sourceIx, valType);
if (reg != NULL)
    {
    dest->reg = reg;
    reg->contents = dest;
    if (sourceIx == 0)
	{
	/* The first floating point parameter is a bit gnarly.  Move it
	 * from old fashioned floating point stack top to xmm0 register.
	 * Long also a little gnarly.  Move from eax:edx to mm0 */
	switch(valType)
	    {
	    case ivFloat:
		safef(coder->destBuf, pentCodeBufSize, "%d(%%esp)",
		    source->stackOffset);
		pentCoderAdd(coder, "fstps", NULL, coder->destBuf);
		pentCoderAdd(coder, "movss", coder->destBuf, "%xmm0");
		break;
	    case ivDouble:
		safef(coder->destBuf, pentCodeBufSize, "%d(%%esp)",
		    source->stackOffset);
		pentCoderAdd(coder, "fstpl", NULL, coder->destBuf);
		pentCoderAdd(coder, "movsd", coder->destBuf, "%xmm0");
		break;
	    case ivLong:
		safef(coder->destBuf, pentCodeBufSize, "%d(%%esp)",
		    source->stackOffset+4);
		pentCoderAdd(coder, "movl", "%edx", coder->destBuf);
		safef(coder->destBuf, pentCodeBufSize, "%d(%%esp)",
		    source->stackOffset);
		pentCoderAdd(coder, "movl", "%eax", coder->destBuf);
		pentCoderAdd(coder, "movq", coder->destBuf, "%mm0");
		break;
	    }
	}
    }
else
    {
    reg = pentFreeReg(isx, valType, nextNode, coder);
    pentCodeDestReg(opMov, source, reg, coder);
    pentLinkRegSave(dest, reg, coder);
    }
}

void pentSwapAllMmx(struct isx *isx, struct pentCoder *coder)
/* Swap out all Mmx registers to memory locations */
{
int i;
for (i=mm0; i<= mm7; ++i)
    pentSwapOutIfNeeded(&regInfo[i],isx->liveList, coder);
}

static void pentCall(struct isx *isx,  struct pentCoder *coder)
/* Output code to actually do call */
{
int i;
pentSwapOutIfNeeded(&regInfo[ax],isx->liveList, coder);
pentSwapOutIfNeeded(&regInfo[cx],isx->liveList, coder);
pentSwapOutIfNeeded(&regInfo[dx],isx->liveList, coder);
for (i=xmm0; i<= mm7; ++i)
    pentSwapOutIfNeeded(&regInfo[i],isx->liveList, coder);
safef(coder->destBuf, pentCodeBufSize, "%s%s", isxPrefixC, isx->left->name);
pentCoderAdd(coder, "call", NULL, coder->destBuf);
}


static void forgetUnreservedRegs()
/* Forget whatever is in non-reserved registers */
{
int i;
/* Clear non-reserved registers. */
for (i=0; i<pentRegCount; ++i)
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
	    pentCodeDestReg(opMov, iad, reg, coder);
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
if (sameString(jmpOp, "jb")) jmpOp = "jnb";
else if (sameString(jmpOp, "jbe")) jmpOp = "jnbe";
else if (sameString(jmpOp, "jnb")) jmpOp = "jb";
else if (sameString(jmpOp, "jnbe")) jmpOp = "jbe";
else if (sameString(jmpOp, "jl")) jmpOp = "jge";
else if (sameString(jmpOp, "jle")) jmpOp = "jg";
else if (sameString(jmpOp, "jge")) jmpOp = "jl";
else if (sameString(jmpOp, "jg")) jmpOp = "jle";
return jmpOp;
}

static void pentCmpJumpLong(struct pfCompile *pfc, struct isx *isx, 
	struct dlNode *nextNode, char *jmpOp, struct pentCoder *coder)
/* Output conditional jump code for == != < <= => > involving longs.
 * This is a bit of work since we have to move out of the mmx registers
 * into memory, and then from memory to eax. */
{
struct isxLiveVar *extraLive = isx->liveList;
struct isxAddress *left = isx->left, *right = isx->right;
struct isxReg *eax = &regInfo[ax];
char *eaxName = isxRegName(eax, ivInt);
struct isxAddress *skipAddress = isxTempLabelAddress(pfc);

/* Force operands into memory, since no long comparison is possible
 * in the mmx registers. */
addDummyToLive(left, &extraLive);
addDummyToLive(right, &extraLive);
if (left->reg)
    pentSwapOutIfNeeded(left->reg, extraLive, coder);
if (right->reg)
    pentSwapOutIfNeeded(right->reg, extraLive, coder);

/* Free up eax */
pentSwapOutIfNeeded(eax, isx->liveList, coder);
eax->contents = NULL;

/* Do comparison of high order 32 bits jumping to end if
 * they are not equal. */
pentPrintVarMemAddress(left, coder->sourceBuf, 4);
pentCoderAdd(coder, "movl", coder->sourceBuf, eaxName);
pentPrintVarMemAddress(right, coder->sourceBuf, 4);
pentCoderAdd(coder, "cmpl", coder->sourceBuf, eaxName);
pentCoderAdd(coder, "jne", skipAddress->name, NULL);

/* Do comparison of low order 32 bits. */
pentPrintVarMemAddress(left, coder->sourceBuf, 0);
pentCoderAdd(coder, "movl", coder->sourceBuf, eaxName);
pentPrintVarMemAddress(right, coder->sourceBuf, 0);
pentCoderAdd(coder, "cmpl", coder->sourceBuf, eaxName);

/* Code label for shortcut, and the conditional jump. */
pentCoderAdd(coder, NULL, NULL, skipAddress->name);
pentCoderAdd(coder, jmpOp, NULL, isx->dest->name);

/* Clean up. */
trimDummiesFromLive(extraLive, isx->liveList);
}

static void pentCmpJump(struct pfCompile *pfc, struct isx *isx, 
	struct dlNode *nextNode, char *jmpOp, char *floatJmpOp, 
	struct pentCoder *coder)
/* Output conditional jump code for == != < <= => > . */
{
enum isxValType valType = isx->left->valType;
if (valType == ivLong)
    pentCmpJumpLong(pfc, isx, nextNode, jmpOp, coder);
else
    {
    if (valType == ivFloat || valType == ivDouble)
	jmpOp = floatJmpOp;
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
	struct isxReg *reg = pentFreeReg(isx, valType, nextNode, coder);
	pentCodeDestReg(opMov, isx->left, reg, coder);
	isx->left->reg = reg;
	reg->contents = isx->left;
	pentCodeDestReg(opCmp, isx->right, reg, coder);
	}
    pentCoderAdd(coder, jmpOp, NULL, isx->dest->name);
    }
}

static void pentTestJumpLong(struct pfCompile *pfc, struct isx *isx, 
	struct dlNode *nextNode, char *jmpOp, struct pentCoder *coder)
/* Output conditional jump code for == 0, != 0  involving longs.
 * This is a bit of work since we have to move out of the mmx registers
 * into memory, and then from memory to eax. */
{
struct isxLiveVar *extraLive = isx->liveList;
struct isxAddress *left = isx->left;
struct isxReg *eax = &regInfo[ax];
char *eaxName = isxRegName(eax, ivInt);
struct isxAddress *skipAddress = isxTempLabelAddress(pfc);

/* Force operands into memory, since no long comparison is possible
 * in the mmx registers. */
addDummyToLive(left, &extraLive);
if (left->reg)
    pentSwapOutIfNeeded(left->reg, extraLive, coder);

/* Free up eax */
pentSwapOutIfNeeded(eax, isx->liveList, coder);
eax->contents = NULL;

/* Do comparison of high order 32 bits jumping to end if
 * they are not equal. */
pentPrintVarMemAddress(left, coder->sourceBuf, 4);
pentCoderAdd(coder, "movl", coder->sourceBuf, eaxName);
pentCoderAdd(coder, "testl", eaxName, eaxName);
pentCoderAdd(coder, "jne", skipAddress->name, NULL);

/* Do comparison of low order 32 bits. */
pentPrintVarMemAddress(left, coder->sourceBuf, 0);
pentCoderAdd(coder, "movl", coder->sourceBuf, eaxName);
pentCoderAdd(coder, "testl", eaxName, eaxName);

/* Code label for shortcut, and the conditional jump. */
pentCoderAdd(coder, NULL, NULL, skipAddress->name);
pentCoderAdd(coder, jmpOp, NULL, isx->dest->name);

/* Clean up. */
trimDummiesFromLive(extraLive, isx->liveList);
}

static void pentTestJumpString(struct pfCompile *pfc, struct isx *isx, 
	struct dlNode *nextNode, char *jmpOp, struct pentCoder *coder)
/* Generate code to test string for NULL, and then for empty */
{
struct isxAddress *iad = isx->left;
struct isxAddress *skipAddress = isxTempLabelAddress(pfc);
struct isxReg *reg = pentFreeReg(isx, ivString, nextNode, coder);
char *regName = isxRegName(reg, ivString);

/* Get a register to work in and detatch it from anything else. */
if (reg != iad->reg)
    pentCodeDestReg(opMov, iad, reg, coder);
if (reg->contents != NULL)
    reg->contents->reg = NULL;
reg->contents = NULL;

/* Test for NULL, no sense going any further if NULL */
pentCoderAdd(coder, "testl", regName, regName);
pentCoderAdd(coder, "jz", skipAddress->name, NULL);

/* Now load up size field and test it for NULL */
safef(coder->sourceBuf, pentCodeBufSize, "pfSize(%s)", regName);
pentCoderAdd(coder, "movl", coder->sourceBuf, regName);
pentCoderAdd(coder, "testl", regName, regName);
pentCoderAdd(coder, NULL, NULL, skipAddress->name);
pentCoderAdd(coder, jmpOp, NULL, isx->dest->name);
}

static void pentTestJump(struct pfCompile *pfc, struct isx *isx, 
	struct dlNode *nextNode, char *jmpOp, struct pentCoder *coder)
/* Output conditional jump code for == 0/!= 0. */
{
struct isxAddress *left = isx->left;
enum isxValType valType = left->valType;
if (valType == ivLong)
    pentTestJumpLong(pfc, isx, nextNode, jmpOp, coder);
else if (valType == ivString)
    pentTestJumpString(pfc, isx, nextNode, jmpOp, coder);
else
    {
    if (left->reg)
	codeOp(opTest, left, left, coder);
    else
	{
	struct isxReg *reg = pentFreeReg(isx, valType, nextNode, coder);
	pentCodeDestReg(opMov, left, reg, coder);
	pentLinkReg(left, reg);
	pentCodeDestReg(opTest, left, reg, coder);
	}
    pentCoderAdd(coder, jmpOp, NULL, isx->dest->name);
    }
}

static void calcInOutOffsets(struct pentFunctionInfo *pfi, struct dlList *iList)
/* Go through and fix stack offsets for input and output parameters */
{
struct dlNode *node;
int inOffset = 0, outOffset = 0;
for (node = iList->head; !dlEnd(node); node = node->next)
    {
    struct isx *isx = node->val;
    switch (isx->opType)
        {
	case poInput:
	    isx->dest->stackOffset = inOffset;
	    inOffset += pentTypeSize4(isx->dest->valType);
	    if (inOffset > pfi->callParamSize)
	         pfi->callParamSize = inOffset;
	    break;
	case poOutput:
	    isx->left->stackOffset = outOffset;
	    outOffset += pentTypeSize4(isx->left->valType);
	    if (outOffset > pfi->callParamSize)
	         pfi->callParamSize = outOffset;
	    break;
	case poCall:
	    inOffset = outOffset = 0;
	    break;
	}
    }
}

static void calcRetOffsets(struct pentFunctionInfo *pfi, struct dlList *iList)
/* Go through and fix stack offsets for return values. */
{
struct dlNode *node;
int retOffset = 8;
for (node = iList->tail; !dlStart(node); node = node->prev)
    {
    struct isx *isx = node->val;
    switch (isx->opType)
        {
	case poReturnVal:
	    isx->dest->stackOffset = retOffset;
	    retOffset += pentTypeSize4(isx->dest->valType);
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
    for (i=0; i<pentRegCount; ++i)
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

static void pentReturnVal(struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder)
/* Move return variable to return location - usually a register,
 * but possibly the parameter stack if there are many return vals. */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
enum isxValType valType = dest->valType;
int ioOffset = dest->val.ioOffset;
struct isxReg *reg = outOffsetToReg(ioOffset, valType);
if (reg != NULL)
    {
    if (ioOffset == 0 && (valType == ivFloat || valType == ivDouble))
	{
	/* Deal with first floating point return value - which we have
	 * to store in the top of the floating point stack. */
	/* Note - here we are assuming variables all live in memory
	 * as well as any registers... */
	char *op = (valType == ivFloat ? "flds" : "fldl");
	assert(source->adType == iadRealVar);
	source->reg = NULL;
	pentPrintAddress(coder, source, coder->sourceBuf);
	pentCoderAdd(coder, op, NULL, coder->sourceBuf);
	}
    else if (ioOffset == 0 && valType == ivLong)
        {
	internalErr();
	}
    else
	{
	if (source->reg != reg)
	    pentCodeDestReg(opMov, source, reg, coder);
	}
    }
else
    {
    reg = pentFreeReg(isx, valType, nextNode, coder);
    if (source->reg != reg)
	pentCodeDestReg(opMov, source, reg, coder);
    pentCodeSourceReg(opMov, reg, dest, coder);
    if (reg->contents != NULL)
	reg->contents->reg = NULL;
    reg->contents = NULL;
    }
}

static void initStompPos(struct regStomper *stomp)
/* Initialize all stompPos in stomp to val.   This will
 * by default convince system that ax is the first register
 * to use for unstable values, and bx the last. */
{
int i;
stomp->stompPos[ax] = 1;
stomp->stompPos[dx] = 2;
stomp->stompPos[cx] = 3;
stomp->stompPos[si] = 4;
stomp->stompPos[di] = 5;
stomp->stompPos[bx] = 6;
for (i=xmm0; i<=mm7; ++i)
    stomp->stompPos[i] = i;
for (i=0; i<pentRegCount; ++i)
    stomp->contents[i] = NULL;
}


static void stompFromIsx(struct isx *isx, struct regStomper *stomp,
	struct pentCoder *coder)
/* Set stompPos to 0 where required by isx instruction.  Doesn't include
 * loops. */
{
int i;
switch (isx->opType)
    {
    case poCall:
       stomp->stompPos[ax] = 0;
       stomp->contents[ax] = NULL;
       coder->regsUsed[ax] = TRUE;
       stomp->stompPos[cx] = 0;
       stomp->contents[cx] = NULL;
       coder->regsUsed[cx] = TRUE;
       stomp->stompPos[dx] = 0;
       stomp->contents[dx] = NULL;
       coder->regsUsed[dx] = TRUE;
       for (i=xmm0; i<=mm7; ++i)
	    {
	    stomp->stompPos[i] = 0;
	    stomp->contents[i] = NULL;
	    coder->regsUsed[i] = TRUE;
	    }
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
	       coder->regsUsed[ax] = TRUE;
	       stomp->stompPos[dx] = 0;
	       stomp->contents[dx] = 0;
	       coder->regsUsed[dx] = TRUE;
	       break;
	   }
	if (isx->right->adType == iadConst)
	   {
	   stomp->stompPos[cx] = 0;
	   stomp->contents[cx] = NULL;
	   coder->regsUsed[cx] = TRUE;
	   }
	break;
    case poShiftLeft:
    case poShiftRight:
	if (isx->right->adType != iadConst)
	    {
	    stomp->stompPos[cx] = 0;
	    stomp->contents[cx] = isx->right;
	    coder->regsUsed[cx] = TRUE;
	    }
	break;
    case poReturnVal:
        {
	struct isxAddress *dest = isx->dest;
	enum isxValType valType = dest->valType;
	int ioOffset = dest->val.ioOffset;
	struct isxReg *reg = outOffsetToReg(ioOffset, valType);
	if (reg != NULL)
	    {
	    int regIx = reg->regIx;
	    stomp->stompPos[regIx] = 0;
	    stomp->contents[regIx] = isx->left;
	    coder->regsUsed[regIx] = TRUE;
	    }
	break;
	}
    case poBeq:
    case poBne:
    case poBlt:
    case poBle:
    case poBgt:
    case poBge:
    case poBz:
    case poBnz:
        {
        if (isx->dest->valType == ivLong)
	    {
	    stomp->stompPos[ax] = 0;
	    stomp->contents[ax] = NULL;
	    coder->regsUsed[ax] = TRUE;
	    }
	break;
	}
    case poCast:
        {
	if (isx->left->valType == ivLong)
	    {
	    enum isxValType destType = isx->dest->valType;
	    if (destType == ivFloat || destType == ivDouble)
	        {
	        for (i=mm0; i<=mm7; ++i)
		    {
		    stomp->stompPos[i] = 0;
		    stomp->contents[i] = NULL;
		    coder->regsUsed[i] = TRUE;
		    }
		}
	    }
	}
    }
}

static void addRegStomper(struct dlList *iList, struct pentCoder *coder)
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
    for (i=0; i<pentRegCount; ++i)
	stomp->stompPos[i] += 1;

    /* Snoop through stomping instructions, and set stomps. */
    stompFromIsx(isx, stomp, coder);
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
	    coder->regsUsed[ix] = TRUE;
	    }
	}
    else
        stompFromIsx(isx, stomp, coder);

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
for (i=0; i<pentRegCount; ++i)
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
for (i=0; i<pentRegCount; ++i)
    fprintf(f, "%s:%d ", isxRegName(&regInfo[i], valType), stomp->stompPos[i]);
}

static void mergeStomper(struct regStomper *a, struct regStomper *b, 
	struct regStomper *d)
/* Merge a/b into d */
{
int i;
for (i=0; i<pentRegCount; ++i)
    {
    d->stompPos[i] = min(a->stompPos[i], b->stompPos[i]);
    }
}

static void rCalcLoopRegVars(struct isxLoopInfo *loopy, 
	struct regStomper *stomp, struct pentCoder *coder)
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
    rCalcLoopRegVars(l, &childStomp, coder);
    mergeStomper(&childStomp, stomp, stomp);
    }

/* Figure out all registers stomped. */
for (node = loopy->start; node != loopy->end; node = node->next)
    {
    isx = node->val;
    stompFromIsx(isx, stomp, coder);
    }

/* Always stomp on eax,edx,xmm0,xmm1 to prevent system from reserving too many
 * variables for loops. (Might be better to do a calculation of
 * number of temporaries required here.) */
stomp->stompPos[ax] = 0;
stomp->stompPos[dx] = 0;
stomp->stompPos[xmm0] = 0;
stomp->stompPos[xmm1] = 0;
stomp->stompPos[mm0] = 0;
stomp->stompPos[mm1] = 0;

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

static void calcLoopRegVars(struct isxList *isxList, struct pentCoder *coder)
/* Calculate register vars to use in loops */
{
struct isxLoopInfo *loopy;
struct regStomper stomp;

ZeroVar(&stomp);

for (loopy = isxList->loopList; loopy != NULL; loopy  = loopy->next)
    {
    initStompPos(&stomp);
    rCalcLoopRegVars(loopy, &stomp, coder);
    }
}

void pentFromIsx(struct pfCompile *pfc, struct isxList *isxList, 
	struct pentFunctionInfo *pfi)
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
calcInOutOffsets(pfi, iList);
calcRetOffsets(pfi, iList);
calcLoopRegVars(isxList, coder);
addRegStomper(iList, pfi->coder);

stackUse = pfi->outVarSize + pfi->locVarSize 
	+ pfi->savedRegSize;
coder->tempIx = -stackUse;

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
    for (i=0; i<pentRegCount; ++i)
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
    for (i=0; i<pentRegCount; ++i)
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
	    pentDivide(isx, nextNode, coder);
	    break;
	case poMod:
	    pentIntModDivide(isx, nextNode, TRUE, coder);
	    break;
	case poBitAnd:
	    pentBinaryOp(isx, nextNode, opAnd, FALSE, coder);
	    break;
	case poBitOr:
	    pentBinaryOp(isx, nextNode, opOr, FALSE, coder);
	    break;
	case poBitXor:
	    pentBinaryOp(isx, nextNode, opXor, FALSE, coder);
	    break;
	case poShiftLeft:
	    pentShiftOp(isx, nextNode, opSal, coder);
	    break;
	case poShiftRight:
	    pentShiftOp(isx, nextNode, opSar, coder);
	    break;
	case poNegate:
	    pentNegate(isx, nextNode, coder);
	    break;
	case poFlipBits:
	    pentFlipBits(isx, nextNode, coder);
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
	    pentCmpJump(pfc, isx, nextNode, "jl", "jb", coder);
	    break;
	case poBle:
	    pentCmpJump(pfc, isx, nextNode, "jle", "jbe", coder);
	    break;
	case poBge:
	    pentCmpJump(pfc, isx, nextNode, "jge", "jnb", coder);
	    break;
	case poBgt:
	    pentCmpJump(pfc, isx, nextNode, "jg", "jnbe", coder);
	    break;
	case poBeq:
	    pentCmpJump(pfc, isx, nextNode, "je", "je", coder);
	    break;
	case poBne:
	    pentCmpJump(pfc, isx, nextNode, "jne", "jne", coder);
	    break;
	case poBz:
	    pentTestJump(pfc, isx, nextNode, "jz", coder);
	    break;
	case poBnz:
	    pentTestJump(pfc, isx, nextNode, "jnz", coder);
	    break;
	case poJump:
	    pentJump(isx, nextNode, coder);
	    break;
	case poFuncStart:
	    break;
	case poFuncEnd:
	    break;
	case poReturnVal:
	    pentReturnVal(isx, nextNode, coder);
	    break;
	case poCast:
	    pentCast(pfc, isx, nextNode, coder);
	    break;
	case poVarInit:
	case poVarAssign:
	    pentVarAssign(isx, nextNode, coder);
	    break;
	case poVarInput:
	    pentVarInput(isx, nextNode, coder);
	    break;
	default:
	    warn("unimplemented\t%s\n", isxOpTypeToString(isx->opType));
	    break;
	}
    }
slReverse(&coder->list);
pfi->tempVarSize = -(stackUse + coder->tempIx);
}

#ifdef OLD
static int alignOffset(int offset, int size)
/* Align offset to go with variable of a given size */
{
if (size == 2)
    offset = ((offset+1)&0xFFFFFFFE);
else if (size >= 4)
    offset = ((offset+3)&0xFFFFFFFC);
return offset;
}
#endif /* OLD */

static int calcVarListSize(struct pfCompile *pfc, struct slRef *varRefList, 
	int varCount)
/* Figure out size of vars - align things to be on even boundaries. */
{
int i, size1, offset = 0;
struct slRef *varRef; 
for (i=0,varRef = varRefList; i<varCount; ++i, varRef=varRef->next)
    {
    struct pfVar *var = varRef->val;
    size1 = pentTypeSize4(isxValTypeFromTy(pfc, var->ty));
    offset += size1;
    }
return offset;
}

static void fillInStackVarOffsets(struct pfCompile *pfc, int offset,
	struct slRef *varRefList, int varCount, struct hash *varHash)
/* Fill in stack offsets of variables that live on stack.  This will
 * pad them to be at least 4 bytes long, and also force alignment
 * if need be. */
{
int i, size1;
struct slRef *varRef; 
for (i=0,varRef = varRefList; i<varCount; ++i, varRef=varRef->next)
    {
    struct pfVar *var = varRef->val;
    struct isxAddress *iad = hashFindVal(varHash, var->cName);
    size1 = pentTypeSize4(isxValTypeFromTy(pfc, var->ty));
    if (size1 < 4)
        size1 = 4;
    iad->stackOffset = offset;
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

pfi->outVarSize = calcVarListSize(pfc, ctar->outRefList, ctar->outCount);
pfi->locVarSize = calcVarListSize(pfc, ctar->localRefList, ctar->localCount);
fillInStackVarOffsets(pfc, pfi->savedRetEbpSize, ctar->inRefList, ctar->inCount, 
	varHash);
stackSubAmount = pfi->savedRegSize+pfi->outVarSize;
fillInStackVarOffsets(pfc, -stackSubAmount, ctar->outRefList, ctar->outCount, 
	varHash);
stackSubAmount += pfi->locVarSize;
fillInStackVarOffsets(pfc, -stackSubAmount, ctar->localRefList, ctar->localCount, 
	varHash);
}

void pentFunctionStart(struct pfCompile *pfc, struct pentFunctionInfo *pfi, 
	char *cName, boolean isGlobal, char *protectLabel, char *skipLabel,
	FILE *f)
/* Start coding up a function in pentium assembly language. */
{
/* This gets a little complex because we have to maintain the
 * stack at an even multiple of 16, on the Mac at least. */
int stackSubAmount;
int skipPushSize = 0;	/* Amount extra to move stack for missing pushes */
bool *regsUsed = pfi->coder->regsUsed;
if (isGlobal)
    fprintf(f, ".globl %s%s\n", isxPrefixC, cName);
fprintf(f, "%s%s:\n", isxPrefixC, cName);
fprintf(f, "%s",
"	pushl	%ebp\n"
"	movl	%esp,%ebp\n");
if (regsUsed[bx])
    fprintf(f, "\tpushl\t%%ebx\n");
else
    skipPushSize += 4;
if (regsUsed[si])
    fprintf(f, "\tpushl\t%%esi\n");
else
    skipPushSize += 4;
if (regsUsed[di])
    fprintf(f, "\tpushl\t%%edi\n");
else
    skipPushSize += 4;
stackSubAmount = pfi->outVarSize + pfi->locVarSize + pfi->tempVarSize 
			+ pfi->callParamSize + pfi->savedContextSize;
stackSubAmount = ((stackSubAmount+15)&0xFFFFFFF0);
stackSubAmount -= pfi->savedContextSize;
stackSubAmount += skipPushSize;
pfi->stackSubAmount = stackSubAmount;
fprintf(f, "\tsubl\t$%d,%%esp\n", stackSubAmount);
if (protectLabel)
    {
    fprintf(f, "\tmovb\t%s,%%al\n", protectLabel);
    fprintf(f, "\ttestb\t%%al,%%al\n");
    fprintf(f, "\tjnz\t%s\n", skipLabel);
    fprintf(f, "\tinc\t%%al\n");
    fprintf(f, "\tmov\t%%al,%s\n", protectLabel);
    }
}

void pentFunctionEnd(struct pfCompile *pfc, struct pentFunctionInfo *pfi, 
	char *skipLabel, FILE *f)
/* Finish coding up a function in pentium assembly language. */
{
bool *regsUsed = pfi->coder->regsUsed;
if (skipLabel)
    fprintf(f, "%s:\n", skipLabel);
fprintf(f, "\taddl\t$%d,%%esp\n", pfi->stackSubAmount);
if (regsUsed[di])
    fprintf(f, "\tpopl\t%%edi\n");
if (regsUsed[si])
    fprintf(f, "\tpopl\t%%esi\n");
if (regsUsed[bx])
    fprintf(f, "\tpopl\t%%ebx\n");
fprintf(f, "%s",
"	popl	%ebp\n"
"	ret\n");
}

static struct dlNode *subInBinary(struct pfCompile *pfc, struct dlNode *node, 
	struct isx *isx, char *call)
/* Substitute call to function for a binary operation. */
{
struct dlNode *retNode;
enum isxValType valType = isx->dest->valType;
struct isx *firstIn = isxNew(pfc, poInput, isxIoAddress(0, valType, iadInStack),
	isx->left, NULL);
struct isx *secondIn = isxNew(pfc, poInput, isxIoAddress(1, valType, iadInStack),
	isx->right, NULL);
struct isx *callIsx = isxNew(pfc, poCall, NULL, isxCallAddress(call), NULL);
struct isx *outIsx = isxNew(pfc, poOutput, isx->dest, 
	isxIoAddress(0, valType, iadOutStack), NULL);
retNode = dlAddValAfter(node, outIsx);
dlAddValAfter(node, callIsx);
dlAddValAfter(node, secondIn);
dlAddValAfter(node, firstIn);
dlRemove(node);
freez(&node);
freez(&isx);
return retNode;
}

static struct isxAddress *constZeroAddress(enum isxValType valType)
/* Fill in zero token of correct type */
{
static union pfTokVal val;
enum pfTokType type;
switch (valType)
    {
    case ivLong:
         type = pftLong;
	 break;
    case ivFloat:
    case ivDouble:
         type = pftFloat;
	 break;
    case ivInt:
    case ivShort:
         type = pftInt;
	 break;
    default:
         internalErr();
	 type = pftInt;
	 break;
    }
return isxConstAddress(type, val, valType);
}

void subFromZeroForNeg(struct pfCompile *pfc, struct dlNode *node,
	struct isx *isx)
/* convert -x  to  0 - x. */
{
enum isxValType valType = isx->dest->valType;
isx->opType = poMinus;
isx->right = isx->left;
isx->left = constZeroAddress(valType);
}

void cmpFloatToZero(struct pfCompile *pfc, struct dlNode *node,
	struct isx *isx)
/* Convert x  to x == 0 */
{
enum isxValType valType = isx->left->valType;
if (isx->opType == poBz)
    isx->opType = poBeq;
else
    isx->opType = poBne;
isx->right = constZeroAddress(valType);
}

void cmpStrings(struct pfCompile *pfc, struct hash *varHash, 
	struct dlNode *node, struct isx *isx)
/* Convert  $a cmp $b  to 
 *        temp = cmpStrings($a,$b);
 *        $tmp cmp 0 */
{
struct isx saveIsx = *isx;
struct isxAddress *tempVar = isxTempVarAddress(pfc, varHash, 1.0, ivString);
struct isx *cmpIsx, *callOutIsx;
isx->dest = tempVar;
node = subInBinary(pfc, node, isx, "_pfStringCmp");
callOutIsx = node->val;
callOutIsx->dest->valType = ivInt;
cmpIsx = isxNew(pfc, saveIsx.opType, saveIsx.dest, callOutIsx->dest, 
	constZeroAddress(ivInt));
dlAddValAfter(node, cmpIsx);
}

#ifdef MAYBE
void castCall(struct pfCompile *pfc, struct dlNode *node, struct isx *isx,
	char *call)
/* Generate code to cast via a function call. */
{
struct dlNode *retNode;
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isx *inIsx = isxNew(pfc, poInput, 
	isxIoAddress(0, source->valType, iadInStack), source, NULL);
struct isx *callIsx = isxNew(pfc, poCall, NULL, isxCallAddress(call), NULL);
struct isx *outIsx = isxNew(pfc, poOutput, dest, 
	isxIoAddress(0, dest->valType, iadOutStack), NULL);
retNode = dlAddValAfter(node, outIsx);
dlAddValAfter(node, callIsx);
dlAddValAfter(node, inIsx);
dlRemove(node);
freez(&node);
freez(&isx);
}
#endif /* MAYBE */

void pentSubCallsForHardStuff(struct pfCompile *pfc, struct hash *varHash,
	struct isxList *isxList)
/* Substitute subroutine calls for some of the harder
 * instructions, particularly acting on longs. */
{
struct dlNode *node, *next;
for (node = isxList->iList->head; !dlEnd(node); node = next)
    {
    struct isx *isx = node->val;
    next = node->next;
    if (isx->dest != NULL)
	{
	switch (isx->dest->valType)
	    {
	    case ivLong:
		{
		switch (isx->opType)
		    {
		    case poMul:
			subInBinary(pfc, node, isx, "_pfLongMul");
			break;
		    case poDiv:
			subInBinary(pfc, node, isx, "_pfLongDiv");
			break;
		    case poMod:
			subInBinary(pfc, node, isx, "_pfLongMod");
			break;
		    case poShiftRight:
			subInBinary(pfc, node, isx, "_pfLongShiftRight");
			break;
		    case poNegate:
			subFromZeroForNeg(pfc, node, isx);
			break;
		    }
		break;
		}
	    case ivFloat:
	    case ivDouble:
		{
		switch (isx->opType)
		    {
		    case poNegate:
			subFromZeroForNeg(pfc, node, isx);
			break;
		    case poBz:
		    case poBnz:
		        cmpFloatToZero(pfc, node, isx);
			break;
		    }
		break;
		}
	    }
	}
    if (isx->left != NULL)
	{
	switch (isx->left->valType)
	    {
	    case ivFloat:
	    case ivDouble:
		{
		switch (isx->opType)
		    {
		    case poBz:
		    case poBnz:
		        cmpFloatToZero(pfc, node, isx);
			break;
		    }
		break;
		}
#ifdef MAYBE
	    case ivLong:
	        {
		switch (isx->opType)
		    {
		    case poCast:
		        {
			switch (isx->dest->valType)
			    {
			    case ivFloat:
			        castCall(pfc, node, isx, "_pfLongToFloat");
				break;
			    case ivDouble:
			        castCall(pfc, node, isx, "_pfLongToDouble");
				break;
			    }
			break;
			}
		    }
		break;
		}
#endif /* MAYBE */
	    case ivString:
	        {
		switch (isx->opType)
		    {
		    case poBlt:
		    case poBle:
		    case poBge:
		    case poBgt:
		    case poBeq:
		    case poBne:
		        cmpStrings(pfc, varHash, node, isx);
			break;
		    }
		break;
		}
	    }
	}
    }
}
