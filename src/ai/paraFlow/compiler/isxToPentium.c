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
#include "isx.h"
#include "gnuMac.h"
#include "isxToPentium.h"

enum pentRegs
/* These need to be in same order as regInfo table. */
   {
   ax=0, bx=1, cx=2, dx=3, si=4, di=5, st0=6,
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

enum pentDataOp
/* Some of the pentium op codes that apply to many types */
    {
    opMov, opAdd, opSub, opMul, opDiv, opAnd, opOr, opXor, opSal, 
    opSar, opNeg, opNot,
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

static int refListIx(struct slRef *refList, void *val)
/* Return index of ref on list, or -1 if not there. */
{
struct slRef *ref;
int ix = 0;
for (ref = refList; ref != NULL; ref = ref->next,++ix)
    if (ref->val == val)
        return ix;
return -1;
}

static int findNextUse(struct isxAddress *iad, struct dlNode *node)
/* Find next use of node. */
{
int count = 1;
for (; !dlEnd(node); node = node->next, ++count)
    {
    struct isx *isx = node->val;
    if (isx->left == iad || isx->right == iad)
        return count;
    }
return count+1;
}

static void pentPrintAddress(struct isxAddress *iad, FILE *f)
/* Print out an address for an instruction. */
{
switch (iad->adType)
    {
    case iadConst:
	{
	fprintf(f, "$%d", iad->val.tok->val.i);
	break;
	}
    case iadRealVar:
	{
	if (iad->reg != NULL)
	    fprintf(f, "%s", isxRegName(iad->reg, iad->valType));
	else
	    fprintf(f, "%s%s", isxPrefixC, iad->name);
	break;
	}
    case iadTempVar:
	{
	if (iad->reg != NULL)
	    fprintf(f, "%s", isxRegName(iad->reg, iad->valType));
	else
	    fprintf(f, "%d(%%ebp)", iad->val.tempMemLoc);
	break;
	}
    case iadInStack:
        {
	fprintf(f, "%d(%%esp)", iad->val.stackOffset);
	break;
	}
    case iadOutStack:
        {
	fprintf(f, "%d(uglyOut)", iad->val.stackOffset);
	break;
	}
    default:
        internalErr();
	break;
    }
}

static void printOp(enum pentDataOp opType, struct isxAddress *source,
	struct isxAddress *dest, FILE *f)
/* Print op to file */
{
char *opName = opOfType(opType, source->valType);
fprintf(f, "\t%s\t", opName);
pentPrintAddress(source, f);
if (dest != NULL)
    {
    fprintf(f, ",");
    pentPrintAddress(dest, f);
    }
fprintf(f, "\n");
}

static void printOpSourceReg(enum pentDataOp opType, struct isxReg *reg,
	struct isxAddress *dest, FILE *f)
/* Print op where source is a register. */
{
enum isxValType valType = dest->valType;
char *opName = opOfType(opType, valType);
char *regName = isxRegName(reg, valType);
fprintf(f, "\t%s\t%s,", opName, regName);
pentPrintAddress(dest, f);
fprintf(f, "\n");
}

static void printOpDestReg(enum pentDataOp opType, 
	struct isxAddress *source, struct isxReg *reg, FILE *f)
/* Print op where dest is a register. */
{
enum isxValType valType = source->valType;
char *opName = opOfType(opType, valType);
char *regName = isxRegName(reg, valType);
fprintf(f, "\t%s\t", opName);
pentPrintAddress(source, f);
fprintf(f, ",%s\n", regName);
}

static void unaryOpReg(enum pentDataOp opType, struct isxReg *reg, 
	enum isxValType valType,FILE *f)
/* Do unary op on register. */
{
char *regName = isxRegName(reg, valType);
char *opName = opOfType(opType, valType);
fprintf(f, "\t%s\t%s\n", opName, regName);
}


static void clearReg(struct isxReg *reg, FILE *f)
/* Clear out register. */
{
char *name = reg->pointerName;
assert(name != NULL);
fprintf(f, "\txorl\t%s,%s\n", name, name);
}


static void pentSwapTempFromReg(struct isxReg *reg, FILE *f)
/* If reg contains something not also in memory then save it out. */
{
struct isxAddress *iad = reg->contents;
struct isxAddress old = *iad;
static int tempIx;

tempIx -= reg->size;	/* Could do better... TODO FIXME */
iad->reg = NULL;
iad->val.tempMemLoc = tempIx;
printOp(opMov, &old, iad, f);
}

static void pentSwapOutIfNeeded(struct isxReg *reg, struct slRef *liveList, 
	FILE *f)
/* Swap out register to memory if need be. */
{
struct isxAddress *iad = reg->contents;
if (iad != NULL)
    {
    if (iad->adType == iadTempVar && iad->val.tempMemLoc == 0)
	{
	if (refOnList(liveList, iad))
	    pentSwapTempFromReg(reg, f);
	}
    else
	iad->reg = NULL;
    }
reg->contents = NULL;
}

static struct isxReg *freeReg(struct isx *isx, struct dlNode *nextNode, FILE *f)
/* Find free register for instruction result. */
{
int i;
enum isxValType valType = isx->dest->valType;
struct isxReg *regs = regInfo;
int regCount = ArraySize(regInfo);
struct isxReg *freeReg = NULL;

/* First look for a register holding a source that is no longer live. */
    {
    int freeIx = BIGNUM;
    for (i=0; i<regCount; ++i)
	{
	struct isxReg *reg = &regs[i];
	if (isxRegName(reg, valType))
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
		     if (!refOnList(isx->liveList, iad))
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

/* If no luck yet, look for any free register */
for (i=0; i<regCount; ++i)
    {
    struct isxReg *reg = &regs[i];
    if (isxRegName(reg, valType))
	{
	struct isxAddress *iad = reg->contents;
	if (iad == NULL)
	    return reg;
	if (!refOnList(isx->liveList, iad))
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
	if (isxRegName(reg, valType))
	    {
	    struct isxAddress *iad = reg->contents;
	    if ((iad->adType == iadRealVar) ||
		(iad->adType == iadTempVar && iad->val.tempMemLoc != 0))
		{
		if (iad == isx->left || iad == isx->right)
		    {
		    int nextUse = findNextUse(iad, nextNode);
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

/* Still no free registers, well dang.  Then try for a register
 * that holds a value also in memory, but doesn't need to be 
 * a source.  If there's a choice use the one holding the variable
 * that won't be used for the longest time. */
    {
    int soonestUse = 0;
    for (i=0; i<regCount; ++i)
	{
	struct isxReg *reg = &regs[i];
	if (isxRegName(reg, valType))
	    {
	    struct isxAddress *iad = reg->contents;
	    if ((iad->adType == iadRealVar) ||
		(iad->adType == iadTempVar && iad->val.tempMemLoc != 0))
		{
		int nextUse = findNextUse(iad, nextNode);
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
	if (isxRegName(reg, valType))
	    {
	    struct isxAddress *iad = reg->contents;
	    int nextUse = findNextUse(iad, nextNode);
	    if (nextUse > soonestUse)
		{
		soonestUse = nextUse;
		freeReg = reg;
		}
	    }
	}
    pentSwapTempFromReg(freeReg, f);
    return freeReg;
    }
}


static void pentAssign(struct isx *isx, struct dlNode *nextNode, FILE *f)
/* Code assignment */
{
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
struct isxReg *reg;
if (source->adType == iadZero)
    {
    reg = freeReg(isx, nextNode, f);
    clearReg(reg, f);
    }
else
    {
    reg = source->reg;
    if (reg == NULL)
	{
	reg = freeReg(isx, nextNode, f);
	printOpDestReg(opMov, source, reg, f);
	}
    }
if (dest->adType == iadRealVar)
    printOpSourceReg(opMov, reg, dest, f);
if (reg->contents != NULL)
    reg->contents->reg = NULL;
reg->contents = dest;
dest->reg = reg;
}

static void pentBinaryOp(struct isx *isx, struct dlNode *nextNode, 
	enum pentDataOp opCode, boolean isSub, FILE *f)
/* Code most binary ops.  Division is harder. */
{
struct isxReg *reg = freeReg(isx, nextNode, f);
struct slRef *ref;
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
	unaryOpReg(opNeg, reg, iad->valType, f);
	printOpDestReg(opAdd, iad, reg, f);
	}
    else
	{
	printOpDestReg(opCode, iad, reg, f);
	}
    }
else
    {
    printOpDestReg(opMov, isx->left, reg, f);
    printOpDestReg(opCode, isx->right, reg, f);
    }
if (reg->contents != NULL)
    reg->contents->reg = NULL;
dest = isx->dest;
reg->contents = dest;
dest->reg = reg;
}

static void pentModDivide(struct isx *isx, struct dlNode *nextNode, 
	boolean isMod, FILE *f)
/* Generate code for mod or divide. */
{
struct isxAddress *p = isx->left;
struct isxAddress *q = isx->right;
struct isxAddress *d = isx->dest;
struct isxReg *eax = &regInfo[ax];
struct isxReg *edx = &regInfo[dx];

if (eax->contents != p)
    {
    pentSwapOutIfNeeded(eax, isx->liveList, f);
    printOpDestReg(opMov, p, eax, f);
    }
pentSwapOutIfNeeded(edx,isx->liveList, f);
clearReg(edx, f);
printOp(opDiv, q, NULL, f);
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
}

static void pentShiftOp(struct isx *isx, struct dlNode *nextNode, 
	enum pentDataOp opCode, FILE *f)
/* Code shift ops.  */
{
struct isxAddress *p = isx->left;
struct isxAddress *q = isx->right;
struct isxAddress *d = isx->dest;

if (q->adType == iadConst)
    {
    /* The constant case can be handled as normal. */
    pentBinaryOp(isx, nextNode, opCode, FALSE, f);
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
    if (eax != p->reg)
	{
	pentSwapOutIfNeeded(eax,isx->liveList, f);
	printOpDestReg(opMov, p, eax, f);
	}
    else
        {
	p->reg = NULL;
	}
    if (ecx != q->reg)
	{
	pentSwapOutIfNeeded(ecx,isx->liveList, f);
	printOpDestReg(opMov, q, ecx, f);
	}
    fprintf(f, "\t%s\t%s,%s\n", opOfType(opCode, p->valType), 
    	isxRegName(ecx, ivByte), isxRegName(eax, p->valType));
    eax->contents = d;
    d->reg = eax;
    }
}

static void pentInput(struct isx *isx, struct dlNode *nextNode, FILE *f)
/* Output code to load an input parameter before a call. */
{
struct isxAddress *source = isx->left;
if (source->reg)
    printOp(opMov, source, isx->dest, f);
else
    {
    struct isxReg *reg = freeReg(isx, nextNode, f);
    printOpDestReg(opMov, source, reg, f);
    reg->contents = source;
    source->reg = reg;
    printOp(opMov, source, isx->dest, f);
    }
}

static void pentOutput(struct isx *isx, struct dlNode *nextNode, FILE *f)
/* Output code to save an output parameter after a call. */
{
/* This needs to get a lot more complex eventually....  For now it just
 * handles up to three int-sized result. */
struct isxReg *reg;
struct isxAddress *source = isx->left;
struct isxAddress *dest = isx->dest;
enum isxValType valType = source->valType;
int sourceIx = source->val.stackOffset;
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

static void pentCall(struct isx *isx, FILE *f)
/* Output code to actually do call */
{
struct pfVar *funcVar = isx->left->val.var;
pentSwapOutIfNeeded(&regInfo[ax],isx->liveList, f);
pentSwapOutIfNeeded(&regInfo[cx],isx->liveList, f);
pentSwapOutIfNeeded(&regInfo[dx],isx->liveList, f);
fprintf(f, "\tcall\t%s%s\n", isxPrefixC, funcVar->cName);
}

static void calcInputOffsets(struct dlList *iList)
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
	    isx->dest->val.stackOffset = offset;
	    offset += pentTypeSize(isx->dest->valType);
	    break;
	case poCall:
	    offset = 0;
	    break;
	}
    }
}

static void printFuncStart(char *name, int localSize, boolean isGlobal, FILE *f)
/* Print the start parts of a function */
{
fprintf(f, "\t.text\n");
if (isGlobal)
    fprintf(f, ".globl %s%s\n", isxPrefixC, name);
fprintf(f, "%s%s:\n", isxPrefixC, name);
fprintf(f, "%s",
"\tpushl\t%ebp\n"
"\tmovl\t%esp, %ebp\n"
);
fprintf(f, "\tsubl\t$%d,%%esp\n", localSize);
}

static void printFuncEnd(FILE *f)
/* Print end of function */
{
// fprintf(f, "\tleave\n\tret\n");
fprintf(f, "%s",
"\tmovl\t%ebp,%esp\n"
"\tpopl\t%ebp\n"
"\tret\n");
}

void pentFromIsx(struct dlList *iList, FILE *f)
/* Convert isx code to pentium instructions in file. */
{
int i;
struct isxReg *destReg;
struct dlNode *node, *nextNode;
struct isx *isx;

calcInputOffsets(iList);
gnuMacPreamble(iList, f);
fprintf(f, "\n# Starting code generation\n");

for (node = iList->head; !dlEnd(node); node = nextNode)
    {
    nextNode = node->next;
    isx = node->val;
#define HELP_DEBUG
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
	fprintf(f, " ");
	}
    fprintf(f, "]\n");	
#endif /* HELP_DEBUG */
    switch (isx->opType)
        {
	case poInit:
	case poAssign:
	    pentAssign(isx, nextNode, f);
	    break;
	case poPlus:
	    pentBinaryOp(isx, nextNode, opAdd, FALSE, f);
	    break;
	case poMul:
	    pentBinaryOp(isx, nextNode, opMul, FALSE, f);
	    break;
	case poMinus:
	    pentBinaryOp(isx, nextNode, opSub, TRUE, f);
	    break;
	case poDiv:
	    pentModDivide(isx, nextNode, FALSE, f);
	    break;
	case poMod:
	    pentModDivide(isx, nextNode, TRUE, f);
	    break;
	case poBitAnd:
	    pentBinaryOp(isx, nextNode, opAnd, TRUE, f);
	    break;
	case poBitOr:
	    pentBinaryOp(isx, nextNode, opOr, TRUE, f);
	    break;
	case poBitXor:
	    pentBinaryOp(isx, nextNode, opXor, TRUE, f);
	    break;
	case poShiftLeft:
	    pentShiftOp(isx, nextNode, opSal, f);
	    break;
	case poShiftRight:
	    pentShiftOp(isx, nextNode, opSar, f);
	    break;
	case poInput:
	    pentInput(isx, nextNode, f);
	    break;
	case poOutput:
	    pentOutput(isx, nextNode, f);
	    break;
	case poCall:
	    pentCall(isx, f);
	    break;
	default:
	    fprintf(f, "\tunimplemented\t%s\n", isxOpTypeToString(isx->opType));
	    break;
	}
    }
gnuMacPostscript(iList, f);
}

