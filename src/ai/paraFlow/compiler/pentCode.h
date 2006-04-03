#ifndef PENTCODE_H
#define PENTCODE_H

#ifndef ISX_H
#include "isx.h"
#endif 

#ifndef PENTCONST_H
#include "pentConst.h"
#endif 


#define pentCodeBufSize 256
#define pentRegCount 22

struct pentCoder
/* A factory for pentCodes */
    {
    struct hash *storeHash;	/* Save strings, floats here to reuse */
    struct hash *constStringHash; /* String constants, not allocated here. */
    struct pentCode *list;	/* List of instructions */
    char destBuf[pentCodeBufSize];	/* Space to print dest */
    char sourceBuf[pentCodeBufSize];	/* Space to print source */
    int tempIx;				/* Index (negative) for temps */
    bool regsUsed[pentRegCount];	/* One for each register, set to TRUE if
					 * register is used. */
    };

struct pentCoder *pentCoderNew(struct hash *constStringHash);
/* Make new pentCoder. */

void pentCoderFree(struct pentCoder **pCoder);
/* Free up pentCoder. */

void pentCoderAdd(struct pentCoder *coder, char *op, char *source, char *dest);
/* Add new instruction */

struct pentCode
/* Pentium code.  None of the strings are allocated here. */
    {
    struct pentCode *next;	/* Next in list */
    char *op;			/* Opcode. NULL here means just a label */
    char *source;		/* Source operand, may be NULL */
    char *dest;			/* Destination operand, may be NULL */
    };

struct pentCode *pentCodeNew(struct pentCoder *coder,
	char *op, char *source, char *dest);
/* Make a new bit of pentium code. */

#define pentCodeFree(ppt) freez(ppt)
/* Free up a pentium code. */

#define pentCodeFreeList(list) slFreeList(list)
/* Free up a list of pentium code. */

void pentCodeSave(struct pentCode *code, FILE *f);
/* Save single instruction to file */

void pentCodeSaveAll(struct pentCode *code, FILE *f);
/* Save list of instructions. */

struct pentFunctionInfo
/* Info about a function */
    {
    int savedContextSize;/* Size of saved regs & return address on stack */
    int savedRegSize;	/* Saved registers. */
    int savedRetEbpSize;/* Saved ebp/return address */
    int outVarSize;	/* Output variables. */
    int locVarSize;	/* Local variables. */
    int tempVarSize;	/* Temp variables. */
    int callParamSize;	/* Size needed for calls */
    int stackSubAmount;	/* Total amount to subtract from stack */
    struct pentCoder *coder;  /* Place for instructions */
    };

struct pentFunctionInfo *pentFunctionInfoNew(struct hash *constStringHash);
/* Create new pentFunctionInfo */

void pentSubCallsForHardStuff(struct pfCompile *pfc, struct hash *varHash,
	struct isxList *isxList);
/* Substitute subroutine calls for some of the harder
 * instructions, particularly acting on longs. */

void pentFromIsx(struct pfCompile *pfc, struct isxList *isxList, 
	struct pentFunctionInfo *pfi);
/* Convert isx code to pentium instructions in pfi. */

int pentTypeSize(enum isxValType valType);
/* Return size of a val type */

void pentFunctionStart(struct pfCompile *pfc, struct pentFunctionInfo *pfi, 
	char *cName, boolean isGlobal, FILE *asmFile);
/* Start coding up a function in pentium assembly language. */

void pentFunctionEnd(struct pfCompile *pfc, struct pentFunctionInfo *pfi, 
	FILE *asmFile);
/* Finish coding up a function in pentium assembly language. */

void pentInitFuncVars(struct pfCompile *pfc, struct ctar *ctar, 
	struct hash *varHash, struct pentFunctionInfo *pfi);
/* Set up variables and offsets for parameters and local variables
 * in hash. */

#endif /* PENTCODE_H */

