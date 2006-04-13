/* pentCast - generate pentium code to cast from one type to another */

#include "common.h"
#include "dlist.h"
#include "pfParse.h"
#include "pfToken.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfCompile.h"
#include "isx.h"
#include "pentCode.h"
#include "pentCast.h"

static void castViaMov(struct pfCompile *pfc,
	struct isx *isx, struct dlNode *nextNode, struct pentCoder *coder)
/* Do a cast just involving a simple instruction operation between
 * general purpose registers. */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isxReg *reg = pentFreeReg(pfc, isx, dest->valType, nextNode, coder);
if (source->reg != reg)
    {
    enum isxValType valType = dest->valType;
    if (source->reg != NULL)
	{
	/* If register/register always do it as a movl, so that
	 * can use si/di as source. */
	char *sourceRegName = isxRegName(source->reg, ivInt);
	char *destRegName = isxRegName(reg, ivInt);
	pentCoderAdd(coder, "movl", sourceRegName, destRegName);
	}
    else
	{
	char *op = pentOpOfType(opMov, valType);
	pentPrintAddress(pfc, coder, source, coder->sourceBuf);
	pentCoderAdd(coder, op, coder->sourceBuf, isxRegName(reg, valType));
	}
    }
pentLinkRegSave(pfc, dest, reg, coder);
}

static void castByOneViaType(struct pfCompile *pfc,
	struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder, char *op, enum isxValType viaType)
/* Cast using just a single instruction. */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isxReg *reg = pentFreeReg(pfc, isx, dest->valType, nextNode, coder);
pentPrintAddress(pfc, coder, source, coder->sourceBuf);
pentCoderAdd(coder, op, coder->sourceBuf, isxRegName(reg, viaType));
pentLinkRegSave(pfc, dest, reg, coder);
}

static void castByOneInstruction(struct pfCompile *pfc,
	struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder, char *op)
/* Cast using just a single instruction. */
{
castByOneViaType(pfc, isx, nextNode, coder, op, isx->dest->valType);
}

static void castIntToBit(struct pfCompile *pfc,
	struct isx *isx, struct dlNode *nextNode, struct pentCoder *coder)
/* Convert byte/short/int to bit using combination of test/setnz */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isxReg *reg = pentFreeReg(pfc, isx, dest->valType, nextNode, coder);
char *wideRegName = isxRegName(reg, source->valType);
char *narrowRegName = isxRegName(reg, ivBit);
char *testOp = pentOpOfType(opTest, source->valType);
if (source->reg != reg)
    pentCodeDestReg(pfc, opMov, source, reg, coder);
pentCoderAdd(coder, testOp, wideRegName, wideRegName);
pentCoderAdd(coder, "setnz", NULL, narrowRegName);
pentLinkRegSave(pfc, dest, reg, coder);
}

static void castViaInt(struct pfCompile *pfc,
	struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder, char *opToInt, char *opToOther)
/* Convert to long or floating point using an intermediate integer
 * register */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
enum isxValType destType = dest->valType;
struct isxReg *reg1 = pentFreeReg(pfc, isx, ivInt, nextNode, coder);
struct isxReg *reg2 = pentFreeReg(pfc, isx, destType, nextNode, coder);
char *reg1Name = isxRegName(reg1, ivInt);

pentPrintAddress(pfc, coder, source, coder->sourceBuf);
pentCoderAdd(coder, opToInt, coder->sourceBuf, reg1Name);
pentCoderAdd(coder, opToOther, reg1Name, isxRegName(reg2, destType));

/* We've used up the int register, but put nothing in it. */
if (reg1->contents)
    {
    reg1->contents->reg = NULL;
    reg1->contents = NULL;
    }
pentLinkRegSave(pfc, dest, reg2, coder);
}

static void castFromBitOrByte(struct pfCompile *pfc, struct isx *isx,
	struct dlNode *nextNode, struct pentCoder *coder)
/* Create code for cast from byte to something else. */
{
switch (isx->dest->valType)
    {
    case ivBit:
	castIntToBit(pfc, isx, nextNode, coder);
	break;
    case ivByte:
	castViaMov(pfc, isx, nextNode, coder);
	break;
    case ivShort:
	castByOneInstruction(pfc, isx, nextNode, coder, "movsbw");
	break;
    case ivInt:
	castByOneInstruction(pfc, isx, nextNode, coder, "movsbl");
	break;
    case ivLong:
	castViaInt(pfc, isx, nextNode, coder, "movsbl", "movd");
	break;
    case ivFloat:
	castViaInt(pfc, isx, nextNode, coder, "movsbl", "cvtsi2ss");
	break;
    case ivDouble:
	castViaInt(pfc, isx, nextNode, coder, "movsbl", "cvtsi2sd");
	break;
    default:
        internalErr();
	break;
    }
}

static void castFromShort(struct pfCompile *pfc, struct isx *isx,
	struct dlNode *nextNode, struct pentCoder *coder)
/* Create code for cast from short to something else. */
{
switch (isx->dest->valType)
    {
    case ivBit:
	castIntToBit(pfc, isx, nextNode, coder);
	break;
    case ivByte:
	castViaMov(pfc, isx, nextNode, coder);
	break;
    case ivInt:
	castByOneInstruction(pfc, isx, nextNode, coder, "movswl");
	break;
    case ivLong:
	castViaInt(pfc, isx, nextNode, coder, "movswl", "movd");
	break;
    case ivFloat:
	castViaInt(pfc, isx, nextNode, coder, "movswl", "cvtsi2ss");
	break;
    case ivDouble:
	castViaInt(pfc, isx, nextNode, coder, "movswl", "cvtsi2sd");
	break;
    default:
        internalErr();
	break;
    }
}

static void castFromInt(struct pfCompile *pfc, struct isx *isx,
	struct dlNode *nextNode, struct pentCoder *coder)
/* Create code for cast from int to something else. */
{
switch (isx->dest->valType)
    {
    case ivBit:
	castIntToBit(pfc, isx, nextNode, coder);
	break;
    case ivByte:
    case ivShort:
	castViaMov(pfc, isx, nextNode, coder);
	break;
    case ivLong:
	castByOneInstruction(pfc, isx, nextNode, coder, "movd");
	break;
    case ivFloat:
	castByOneInstruction(pfc, isx, nextNode, coder, "cvtsi2ss");
	break;
    case ivDouble:
	castByOneInstruction(pfc, isx, nextNode, coder, "cvtsi2sd");
	break;
    default:
        internalErr();
	break;
    }
}

static void castLongToBit(struct pfCompile *pfc,
	struct isx *isx, struct dlNode *nextNode, struct pentCoder *coder)
/* Convert long to bit using move to memory, then loading high part into
 * a 8/16/32 bit register, oring in low part, and then testl/setnz */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isxReg *reg = pentFreeReg(pfc, isx, dest->valType, nextNode, coder);
char *wideRegName = isxRegName(reg, ivInt);
char *narrowRegName = isxRegName(reg, ivBit);

/* Force memory location for long. */
if (source->reg && pentTempJustInReg(source))
    pentSwapTempFromReg(pfc, source->reg,  coder);

/* Code move/or/test/setnz */
pentPrintVarMemAddress(source, coder->sourceBuf, 4);
pentCoderAdd(coder, "movl", coder->sourceBuf, wideRegName);
pentPrintVarMemAddress(source, coder->sourceBuf, 0);
pentCoderAdd(coder, "orl", coder->sourceBuf, wideRegName);
pentCoderAdd(coder, "testl", wideRegName, wideRegName);
pentCoderAdd(coder, "setnz", NULL, narrowRegName);
pentLinkRegSave(pfc, dest, reg, coder);
}

static void castLongToByteShortInt(struct pfCompile *pfc,
	struct isx *isx, struct dlNode *nextNode, struct pentCoder *coder)
/* Convert long to byte.  If it's in a register then do
 * movd/movb combo.  Otherwise do movb from memory location */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
enum isxValType destType = dest->valType;
struct isxReg *reg = pentFreeReg(pfc, isx, destType, nextNode, coder);
if (source->reg)
    pentCoderAdd(coder, "movd", isxRegName(source->reg, ivLong), 
    	isxRegName(reg, ivInt));
else
    {
    char *movOp = pentOpOfType(opMov, destType);
    pentPrintVarMemAddress(source, coder->sourceBuf, 0);
    pentCoderAdd(coder, movOp, coder->sourceBuf, isxRegName(reg, destType));
    }
pentLinkRegSave(pfc, dest, reg, coder);
}

static void castLongToFloatOrDouble(struct pfCompile *pfc,
	struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder, int fpSize, char *fpPop, char *movOp)
/* Convert long to double using move to memory, then fildll to load memory
 * into top of floating point stack, then fstp to store to a temp location
 * in memory, and finally movsd to get into xmm register. */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
int tempOffset = (coder->tempIx -= fpSize);
struct isxReg *reg = pentFreeReg(pfc, isx, dest->valType, nextNode, coder);

/* Force memory location for long. */
if (source->reg && pentTempJustInReg(source))
    pentSwapTempFromReg(pfc, source->reg,  coder);

/* Save and mark as trashed all mmx registers since using floating point
 * stack. Also flip processor out of mmx mode. */
pentSwapAllMmx(pfc, isx, coder);
pentCoderAdd(coder, "emms", NULL, NULL);

/* Code it up. */
pentPrintVarMemAddress(source, coder->sourceBuf, 0);
pentCoderAdd(coder, "fildll", coder->sourceBuf, NULL);
safef(coder->destBuf, pentCodeBufSize, "%d(%%ebp)", tempOffset);
pentCoderAdd(coder, fpPop, NULL,coder->destBuf);
pentCoderAdd(coder, movOp, coder->destBuf, isxRegName(reg, dest->valType));
pentLinkRegSave(pfc, dest, reg, coder);
}

static void castFromLong(struct pfCompile *pfc, struct isx *isx,
	struct dlNode *nextNode, struct pentCoder *coder)
/* Create code for cast from long to something else. */
{
switch (isx->dest->valType)
    {
    case ivBit:
        castLongToBit(pfc, isx, nextNode, coder);
	break;
    case ivByte:
    case ivShort:
    case ivInt:
	castLongToByteShortInt(pfc, isx, nextNode, coder);
	break;
    case ivFloat:
	castLongToFloatOrDouble(pfc, isx, nextNode, coder, 4, "fstps", "movss");
	break;
    case ivDouble:
	castLongToFloatOrDouble(pfc, isx, nextNode, coder, 8, "fstpl", "movsd");
	break;
    default:
        internalErr();
	break;
    }
}

static void castFloatOrDoubleToBit(struct pfCompile *pfc,
	struct isx *isx, struct dlNode *nextNode, struct pentCoder *coder)
/* Cast floating point number to bit. */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isxReg *sourceReg = source->reg;
enum isxValType sourceType = source->valType;
struct isxReg *destReg = pentFreeReg(pfc, isx, ivBit, nextNode, coder);
if (sourceReg == NULL)
    {
    sourceReg = pentFreeReg(pfc, isx, source->valType, nextNode, coder);
    pentCodeDestReg(pfc, opMov, source, sourceReg, coder);
    pentLinkReg(source, sourceReg);
    }
pentCoderAdd(coder, pentOpOfType(opCmp, sourceType), 
	"bunchOfZero", isxRegName(sourceReg, sourceType));
pentCoderAdd(coder, "setnz", NULL, isxRegName(destReg, ivBit));
pentLinkRegSave(pfc, dest, destReg, coder);
}

static void castFloatOrDoubleToLong(struct pfCompile *pfc,
	struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder, int fpSize, char *fpPush)
/* Cast floating point number to long. */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
int tempOffset = (coder->tempIx -= fpSize);
struct isxReg *reg = pentFreeReg(pfc, isx, dest->valType, nextNode, coder);

/* Force memory location for source. */
if (source->reg && pentTempJustInReg(source))
    pentSwapTempFromReg(pfc, source->reg,  coder);

/* Save and mark as trashed all mmx registers since using floating point
 * stack. Also flip processor out of mmx mode. */
pentSwapAllMmx(pfc, isx, coder);
pentCoderAdd(coder, "emms", NULL, NULL);

/* Code it up. */
pentPrintVarMemAddress(source, coder->sourceBuf, 0);
pentCoderAdd(coder, fpPush, coder->sourceBuf, NULL);
safef(coder->destBuf, pentCodeBufSize, "%d(%%ebp)", tempOffset);
pentCoderAdd(coder, "fistpll", NULL,coder->destBuf);
pentCoderAdd(coder, "movq", coder->destBuf, isxRegName(reg, dest->valType));
pentLinkRegSave(pfc, dest, reg, coder);
}

static void castFromFloat(struct pfCompile *pfc, struct isx *isx,
	struct dlNode *nextNode, struct pentCoder *coder)
/* Create code for cast from float to something else. */
{
switch (isx->dest->valType)
    {
    case ivBit:
        castFloatOrDoubleToBit(pfc, isx, nextNode, coder);
	break;
    case ivByte:
    case ivShort:
    case ivInt:
	castByOneViaType(pfc, isx, nextNode, coder, "cvtss2si", ivInt);
	break;
    case ivLong:
        castFloatOrDoubleToLong(pfc, isx, nextNode, coder, 4, "flds");
	break;
    case ivDouble:
	castByOneInstruction(pfc, isx, nextNode, coder, "cvtss2sd");
	break;
    default:
        internalErr();
	break;
    }
}

static void castFromDouble(struct pfCompile *pfc, struct isx *isx,
	struct dlNode *nextNode, struct pentCoder *coder)
/* Create code for cast from float to something else. */
{
switch (isx->dest->valType)
    {
    case ivBit:
        castFloatOrDoubleToBit(pfc, isx, nextNode, coder);
	break;
    case ivByte:
    case ivShort:
    case ivInt:
	castByOneViaType(pfc, isx, nextNode, coder, "cvtsd2si", ivInt);
	break;
    case ivLong:
        castFloatOrDoubleToLong(pfc, isx, nextNode, coder, 8, "fldl");
	break;
    case ivFloat:
	castByOneInstruction(pfc, isx, nextNode, coder, "cvtsd2ss");
	break;
    default:
        internalErr();
	break;
    }
}

void pentCast(struct pfCompile *pfc, struct isx *isx, struct dlNode *nextNode, 
	struct pentCoder *coder)
/* Create code for a cast instruction */
{
switch (isx->left->valType)
    {
    case ivBit:
    case ivByte:
        castFromBitOrByte(pfc, isx, nextNode, coder);
	break;
    case ivShort:
        castFromShort(pfc, isx, nextNode, coder);
	break;
    case ivInt:
        castFromInt(pfc, isx, nextNode, coder);
	break;
    case ivLong:
        castFromLong(pfc, isx, nextNode, coder);
	break;
    case ivFloat:
        castFromFloat(pfc, isx, nextNode, coder);
	break;
    case ivDouble:
        castFromDouble(pfc, isx, nextNode, coder);
	break;
    default:
        internalErr();
    }
}
