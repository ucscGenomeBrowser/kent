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

static void castViaMov(struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder)
/* Do a cast just involving a simple instruction operation between
 * general purpose registers. */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isxReg *reg = pentFreeReg(isx, dest->valType, nextNode, coder);
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
	pentPrintAddress(coder, source, coder->sourceBuf);
	pentCoderAdd(coder, op, coder->sourceBuf, isxRegName(reg, valType));
	}
    }
pentLinkRegSave(dest, reg, coder);
}

static void castByOneInstruction(struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder, char *op)
/* Cast int to long. */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isxReg *reg = pentFreeReg(isx, dest->valType, nextNode, coder);
pentPrintAddress(coder, source, coder->sourceBuf);
pentCoderAdd(coder, op, coder->sourceBuf, isxRegName(reg, dest->valType));
pentLinkRegSave(dest, reg, coder);
}

static void castIntToBit(struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder)
/* Convert byte/short/int to bit using combination of test/setnz */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isxReg *reg = pentFreeReg(isx, dest->valType, nextNode, coder);
char *wideRegName = isxRegName(reg, source->valType);
char *narrowRegName = isxRegName(reg, ivBit);
char *testOp = pentOpOfType(opTest, source->valType);
if (source->reg != reg)
    pentCodeDestReg(opMov, source, reg, coder);
pentCoderAdd(coder, testOp, wideRegName, wideRegName);
pentCoderAdd(coder, "setnz", NULL, narrowRegName);
pentLinkRegSave(dest, reg, coder);
}

static void castViaInt(struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder, char *opToInt, char *opToOther)
/* Convert to long or floating point using an intermediate integer
 * register */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
enum isxValType destType = dest->valType;
struct isxReg *reg1 = pentFreeReg(isx, ivInt, nextNode, coder);
struct isxReg *reg2 = pentFreeReg(isx, destType, nextNode, coder);
char *reg1Name = isxRegName(reg1, ivInt);

pentPrintAddress(coder, source, coder->sourceBuf);
pentCoderAdd(coder, opToInt, coder->sourceBuf, reg1Name);
pentCoderAdd(coder, opToOther, reg1Name, isxRegName(reg2, destType));

/* We've used up the int register, but put nothing in it. */
if (reg1->contents)
    {
    reg1->contents->reg = NULL;
    reg1->contents = NULL;
    }
pentLinkRegSave(dest, reg2, coder);
}

static void castFromBitOrByte(struct pfCompile *pfc, struct isx *isx,
	struct dlNode *nextNode, struct pentCoder *coder)
/* Create code for cast from byte to something else. */
{
switch (isx->dest->valType)
    {
    case ivBit:
	castIntToBit(isx, nextNode, coder);
	break;
    case ivByte:
	castViaMov(isx, nextNode, coder);
	break;
    case ivShort:
	castByOneInstruction(isx, nextNode, coder, "movsbw");
	break;
    case ivInt:
	castByOneInstruction(isx, nextNode, coder, "movsbl");
	break;
    case ivLong:
	castViaInt(isx, nextNode, coder, "movsbl", "movd");
	break;
    case ivFloat:
	castViaInt(isx, nextNode, coder, "movsbl", "cvtsi2ss");
	break;
    case ivDouble:
	castViaInt(isx, nextNode, coder, "movsbl", "cvtsi2sd");
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
	castIntToBit(isx, nextNode, coder);
	break;
    case ivByte:
	castViaMov(isx, nextNode, coder);
	break;
    case ivInt:
	castByOneInstruction(isx, nextNode, coder, "movswl");
	break;
    case ivLong:
	castViaInt(isx, nextNode, coder, "movswl", "movd");
	break;
    case ivFloat:
	castViaInt(isx, nextNode, coder, "movswl", "cvtsi2ss");
	break;
    case ivDouble:
	castViaInt(isx, nextNode, coder, "movswl", "cvtsi2sd");
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
	castIntToBit(isx, nextNode, coder);
	break;
    case ivByte:
    case ivShort:
	castViaMov(isx, nextNode, coder);
	break;
    case ivLong:
	castByOneInstruction(isx, nextNode, coder, "movd");
	break;
    case ivFloat:
	castByOneInstruction(isx, nextNode, coder, "cvtsi2ss");
	break;
    case ivDouble:
	castByOneInstruction(isx, nextNode, coder, "cvtsi2sd");
	break;
    default:
        internalErr();
	break;
    }
}

static void castLongToBit(struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder)
/* Convert long to bit using move to memory, then loading high part into
 * a 8/16/32 bit register, oring in low part, and then testl/setnz */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isxReg *reg = pentFreeReg(isx, dest->valType, nextNode, coder);
char *wideRegName = isxRegName(reg, ivInt);
char *narrowRegName = isxRegName(reg, ivBit);

/* Force memory location for long. */
if (source->reg && pentTempJustInReg(source))
    pentSwapTempFromReg(source->reg,  coder);

/* Code move/or/test/setnz */
pentPrintVarMemAddress(source, coder->sourceBuf, 4);
pentCoderAdd(coder, "movl", coder->sourceBuf, wideRegName);
pentPrintVarMemAddress(source, coder->sourceBuf, 0);
pentCoderAdd(coder, "orl", coder->sourceBuf, wideRegName);
pentCoderAdd(coder, "testl", wideRegName, wideRegName);
pentCoderAdd(coder, "setnz", NULL, narrowRegName);
pentLinkRegSave(dest, reg, coder);
}

static void castLongToByteShortInt(struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder)
/* Convert long to byte.  If it's in a register then do
 * movd/movb combo.  Otherwise do movb from memory location */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
enum isxValType destType = dest->valType;
struct isxReg *reg = pentFreeReg(isx, destType, nextNode, coder);
if (source->reg)
    pentCoderAdd(coder, "movd", isxRegName(source->reg, ivLong), 
    	isxRegName(reg, ivInt));
else
    {
    char *movOp = pentOpOfType(opMov, destType);
    pentPrintVarMemAddress(source, coder->sourceBuf, 0);
    pentCoderAdd(coder, movOp, coder->sourceBuf, isxRegName(reg, destType));
    }
pentLinkRegSave(dest, reg, coder);
}

static void castFromLong(struct pfCompile *pfc, struct isx *isx,
	struct dlNode *nextNode, struct pentCoder *coder)
/* Create code for cast from long to something else. */
{
switch (isx->dest->valType)
    {
    case ivBit:
        castLongToBit(isx, nextNode, coder);
	break;
    case ivByte:
    case ivShort:
    case ivInt:
	castLongToByteShortInt(isx, nextNode, coder);
	break;
    case ivFloat:
    case ivDouble:
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
    default:
        internalErr();
    }
}
