/* isxToPentium - convert isx code to Pentium code.  This basically 
 * follows the register picking algorithm in Aho/Ulman. */

#include "common.h"
#include "dlist.h"
#include "pfParse.h"
#include "pfToken.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfCompile.h"
#include "isx.h"

struct regInfo
    {
    char *name;			 /* Register name */
    enum isxValType *types;	 /* Types it can handle */
    int typeCount;		 /* Count of types it can handle */
    struct isxAddress *contents; /* What if anything register holds */
    };

static enum isxValType abcdRegTypes[] = { ivInt, ivObject,};
static enum isxValType siDiRegTypes[] = {ivInt, ivObject,};
static enum isxValType stRegTypes[] = {ivFloat, ivDouble,};

struct regInfo regInfo[] = {
    { "eax", abcdRegTypes, ArraySize(abcdRegTypes),},
    { "ebx", abcdRegTypes, ArraySize(abcdRegTypes),},
    { "ecx", abcdRegTypes, ArraySize(abcdRegTypes),},
    { "edx", abcdRegTypes, ArraySize(abcdRegTypes),},
#ifdef SOON
    { "esi", siDiRegTypes, ArraySize(siDiRegTypes),},
    { "edi", siDiRegTypes, ArraySize(siDiRegTypes),},
    { "st0", stRegTypes, ArraySize(stRegTypes),},
    { "st1", stRegTypes, ArraySize(stRegTypes),},
    { "st2", stRegTypes, ArraySize(stRegTypes),},
    { "st3", stRegTypes, ArraySize(stRegTypes),},
    { "st4", stRegTypes, ArraySize(stRegTypes),},
    { "st5", stRegTypes, ArraySize(stRegTypes),},
    { "st6", stRegTypes, ArraySize(stRegTypes),},
    { "st7", stRegTypes, ArraySize(stRegTypes),},
#endif /* SOON */
};

int refListIx(struct slRef *refList, void *val)
/* Return index of ref on list, or -1 if not there. */
{
struct slRef *ref;
int ix = 0;
for (ref = refList; ref != NULL; ref = ref->next,++ix)
    if (ref->val == val)
        return ix;
return -1;
}

int findNextUse(struct isxAddress *iad, struct dlNode *node)
/* Find next use of node. */
{
int count = 1;
for (; !dlEnd(node); node = node->next, ++count)
    {
    struct isx *isx = node->val;
    if (refOnList(isx->sourceList, iad))
        break;
    }
return count;
}

static int tempIx;

static struct regInfo *freeReg(struct isx *isx, struct dlNode *nextNode, FILE *f)
/* Find free register for instruction result. */
{
int i;
struct regInfo *regs = regInfo;
int regCount = ArraySize(regInfo);
struct regInfo *freeReg = NULL;

/* First look for a register holding a source that is no longer live. */
    {
    int freeIx = BIGNUM;
    for (i=0; i<regCount; ++i)
	{
	struct regInfo *reg = &regs[i];
	struct isxAddress *iad = reg->contents;
	if (iad != NULL)
	     {
	     int sourceIx = refListIx(isx->sourceList, iad);
	     if (sourceIx >= 0)
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
    if (freeReg)
	return freeReg;
    }

/* If no luck yet, look for any free register */
for (i=0; i<regCount; ++i)
    {
    struct regInfo *reg = &regs[i];
    struct isxAddress *iad = reg->contents;
    if (iad == NULL)
        return reg;
    }

/* No free registers, well dang.  Then use a register that
 * holds a variable that is also in memory if possible.  Try
 * and do it for the variable that is not going to be used for a
 * while.... */
    {
    int soonestUse = 0;
    for (i=0; i<regCount; ++i)
	{
	struct regInfo *reg = &regs[i];
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
    if (freeReg)
        {
	struct isxAddress *iad = freeReg->contents;
	iad->reg = NULL;
	return freeReg;
	}
    }

/* Ok, at this point we'll have to save one of the temps in a register
 * to memory, and then return it's register. We'll use the temp that
 * will not be used for the longest. */
    {
    int soonestUse = 0;
    struct isxAddress *iad;
    for (i=0; i<regCount; ++i)
	{
	struct regInfo *reg = &regs[i];
	struct isxAddress *iad = reg->contents;
	int nextUse = findNextUse(iad, nextNode);
	if (nextUse > soonestUse)
	    {
	    soonestUse = nextUse;
	    freeReg = reg;
	    }
	}
    tempIx -= 4;	// FIXME
    fprintf(f, "\tmov %s,%d(ebp)\n", freeReg->name, tempIx);
    iad = freeReg->contents;
    iad->reg = NULL;
    return freeReg;
    }
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
	    fprintf(f, "%s", iad->reg->name);
	else
	    fprintf(f, "%s", iad->name);
	break;
	}
    case iadTempVar:
	{
	if (iad->reg != NULL)
	    fprintf(f, "%s", iad->reg->name);
	else
	    fprintf(f, "%d(ebp)", iad->val.tempMemLoc);
	break;
	}
    default:
        internalErr();
	break;
    }
}

static void pentAssign(struct isx *isx, struct dlNode *nextNode, FILE *f)
/* Code assignment */
{
struct isxAddress *source = isx->sourceList->val;
struct isxAddress *dest = isx->destList->val;
struct regInfo *reg;
if (source->adType == iadZero)
    {
    reg = freeReg(isx, nextNode, f);
    fprintf(f, "\tsub\t%s,%s\n", reg->name, reg->name);
    }
else
    {
    reg = source->reg;
    if (reg == NULL)
	{
	reg = freeReg(isx, nextNode, f);
	fprintf(f, "\tmov\t");
	pentPrintAddress(source,f);
	fprintf(f, ",%s\n", reg->name);
	}
    }
if (dest->adType == iadRealVar)
    fprintf(f, "\tmov\t%s,%s\n", reg->name, dest->name);
if (reg->contents != NULL)
    reg->contents->reg = NULL;
reg->contents = dest;
dest->reg = reg;
}

static void pentCommutativeOp(struct isx *isx, struct dlNode *nextNode, FILE *f)
/* Code multiplication or addition */
{
struct regInfo *reg = freeReg(isx, nextNode, f);
struct slRef *ref, *regSource = NULL;
struct isxAddress *dest;
char *opCode = (isx->opType == poPlus ? "add" : "imul");

/* Figure out if the reg already holds one of our sources. */
for (ref = isx->sourceList; ref != NULL; ref = ref->next)
    {
    struct isxAddress *iad = ref->val;
    if (iad->reg == reg)
        regSource = ref;
    }
if (regSource != NULL)
    {
    /* We're lucky, the destination register is also one of our sources.
     * we find the other source and add it in. */
    struct slRef *otherSource = NULL;
    struct isxAddress *iad;
    for (ref = isx->sourceList; ref != NULL; ref = ref->next)
        {
	if (ref != regSource)
	    {
	    otherSource = ref;
	    break;
	    }
	}
    iad = otherSource->val;
    fprintf(f, "\t%s\t", opCode);
    pentPrintAddress(iad,f);
    }
else
    {
    struct slRef *sourceList = isx->sourceList;
    struct isxAddress *a,*b;
    a = sourceList->val;
    b = sourceList->next->val;
    fprintf(f, "\tmov\t");
    pentPrintAddress(a, f);
    fprintf(f, ",%s\n", reg->name);
    fprintf(f, "\t%s\t", opCode);
    pentPrintAddress(b, f);
    }
fprintf(f, ",%s\n", reg->name);
if (reg->contents != NULL)
    reg->contents->reg = NULL;
dest = isx->destList->val;
reg->contents = dest;
dest->reg = reg;
}

void isxToPentium(struct dlList *iList, FILE *f)
/* Convert isx code to pentium instructions in file. */
{
struct regInfo *destReg;
struct dlNode *node, *nextNode;
struct isx *isx;
fprintf(f, "------------Theoretically generating pentium code------------\n");

for (node = iList->head; !dlEnd(node); node = nextNode)
    {
    nextNode = node->next;
    isx = node->val;
    fprintf(f, "# ");	// uglyf
    isxDump(isx, f);	// uglyf
    fprintf(f, "\n");	// uglyf
    switch (isx->opType)
        {
	case poInit:
	case poAssign:
	    pentAssign(isx, nextNode, f);
	    break;
	case poPlus:
	case poMul:
	case poMinus:	// uglyf
	    pentCommutativeOp(isx, nextNode, f);
	    break;
#ifdef SOON
	case poMinus:
	    pentSubOp(isx, nextNode, f);
	    break;
#endif /* SOON */
	}
    }
}

