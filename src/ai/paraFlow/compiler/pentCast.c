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

static void castViaMovl(struct isx *isx, struct dlNode *nextNode,
	struct pentCoder *coder)
/* Do a cast just involving a simple movl instruction operation. */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isxReg *reg = pentFreeReg(isx, dest->valType, nextNode, coder);
if (source->reg != reg)
    {
    pentPrintAddress(coder, source, coder->sourceBuf);
    pentCoderAdd(coder, "movl", coder->sourceBuf, isxRegName(reg, ivInt));
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
/* Convert byte/short/long to bit using combination of test/setnz */
{
}

static void castFromInt(struct pfCompile *pfc, struct isx *isx,
	struct dlNode *nextNode, struct pentCoder *coder)
/* Create code for cast from long to something else. */
{
switch (isx->dest->valType)
    {
    case ivBit:
	uglyAbort("BIts from ints?!");
	break;
    case ivByte:
    case ivShort:
	castViaMovl(isx, nextNode, coder);
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

void pentCast(struct pfCompile *pfc, struct isx *isx, struct dlNode *nextNode, 
	struct pentCoder *coder)
/* Create code for a cast instruction */
{
switch (isx->left->valType)
    {
    case ivInt:
        castFromInt(pfc, isx, nextNode, coder);
	break;
    default:
        internalErr();
    }
}
