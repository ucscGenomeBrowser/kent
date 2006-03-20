/* isx - intermediate sequenced executable -  the paraFlow
 * intermediate code.  A fairly standard "three address code"
 * that is a lower level representation than the parse tree but
 * higher level than actual machine code.  The format is
 *    label opCode dest left right
 * which is interpreted as 
 *    label:  dest = left opCode right
 * so
 *    100 + x 3 8		represents x = 3 + 8
 *    101 * t1 t2 t3		represents t1 = t2*t3
 *    102 = x y			represents x = y
 *    103 - u v                 represents u = -v
 */

#ifndef ISX_H
#define ISX_H

#ifndef DLIST_H
#include "dlist.h"
#endif 

enum isxOpType
/* What sort of operation - corresponds roughly to an
 * assembly language instruction without type or address
 * considerations. */
    {
    poInit,	 /* Variable initialization */
    poAssign,	 /* Variable (re)assignment */
    poPlus,	 /* dest = left + right */
    poMinus,	 /* dest = left - right */
    poMul,	 /* dest = left * right */
    poDiv,	 /* dest = left / right */
    poMod,	 /* dest = left % right */
    poBitAnd,	 /* dest = left & right */
    poBitOr,     /* dest = left | right */
    poBitXor,	 /* dest = left ^ right */
    poShiftLeft, /* dest = left << right */
    poShiftRight,/* dest = left >> right */
    poNegate,	 /* dest = -left */
    poFlipBits,  /* dest = ~left */
    poInput,	 /* An input parameter: dest = left */
    poCall,	 /* Call subroutine:    call left   */
    poOutput,	 /* An output parameter dest = left */
    poGoTo,	/* Unconditional jump */
    poBranch,	/* Conditional jump */
    };

char *isxOpTypeToString(enum isxOpType val);
/* Convert isxValType to string. */

enum isxValType
/* Simplified value type system.  */
    {
    ivZero,
    ivByte,
    ivShort,
    ivInt,
    ivLong,
    ivFloat,
    ivDouble,
    ivString,
    ivObject,
    ivJump,	
    };

char *isxValTypeToString(enum isxValType val);
/* Convert isxValType to string. */

enum isxAddressType
/* Address type */
    {
    iadZero,
    iadConst,
    iadRealVar,
    iadTempVar,
    iadInStack,
    iadOutStack,
    iadOperator,
    };

union isxAddressVal
    {
    struct pfToken *tok;	/* For constants */
    struct pfVar *var;		/* For real vars */
    int tempMemLoc;		/* Memory location for temps, 0 for none */
    int stackOffset;		/* Stack offset. */
    };

struct isxAddress
/* A piece of data somewhere */
    {
    struct isxAddress *next;	/* For multivalued functions */
    char *name;			/* Not allocated here. */
    enum isxAddressType adType;/* Address type */
    enum isxValType valType;	/* Value type */
    union isxAddressVal val;	/* Address value */
    struct isxReg *reg;	/* Register if any */
    };

struct isx
/* An instruction in our intermediate language. */
    {
    int label;			/* Numerical label */
    enum isxOpType opType;	/* Opcode - add, mul, branch, etc. */
    struct isxAddress *dest;	/* Destination address */
    struct isxAddress *left;	/* Left (or only) source address. */
    struct isxAddress *right;	/* Right (optional) source address.*/
    struct slRef *liveList;	/* List of live vars after instruction  */
    };

struct isxReg
/* Represents a CPU register. */
    {
    int size;			 /* Size in bytes. */
    char *byteName;		 /* Register name when used for bytes. */
    char *shortName;		 /* Register name when used for shorts. */
    char *intName;		 /* Register name when used for ints. */
    char *longName;		 /* Register name when used for longs. */
    char *floatName;		 /* Register name when used for floats. */
    char *doubleName;		 /* Register name when used for doubles. */
    char *pointerName;		 /* Register name when used for pointers. */
    struct isxAddress *contents; /* What if anything register holds */
    };


struct isx *isxNew(struct pfCompile *pfc, enum isxOpType opType,
	struct isxAddress *dest, struct isxAddress *left,
	struct isxAddress *right, struct dlList *iList);
/* Make new isx instruction, and hang it on iList */

char *isxRegName(struct isxReg *reg, enum isxValType valType);
/* Get name of reg for given type. */

void isxDump(struct isx *isx, FILE *f);
/* Dump out isx code */

void isxDumpList(struct dlList *list, FILE *f);
/* Dump all of isx in list */

struct dlList *isxFromParse(struct pfCompile *pfc, struct pfParse *pp);
/* Convert parse tree to isx. */

void isxToPentium(struct dlList *iList, FILE *f);
/* Convert isx code to pentium instructions in file. */

#endif /* ISX_H */

