/* isx - intermediate sequenced executable -  the paraFlow
 * intermediate code.  A fairly standard "three address code"
 * that is a lower level representation than the parse tree but
 * higher level than actual machine code.  The format is
 *    opCode dest left right
 * which is interpreted as 
 *    dest = left opCode right
 * so
 *    + x 3 8		represents x = 3 + 8
 *    * t1 t2 t3	represents t1 = t2*t3
 *    = x y		represents x = y
 *    - u v             represents u = -v
 */

#ifndef ISX_H
#define ISX_H

#ifndef DLIST_H
#include "dlist.h"
#endif 

#ifndef ISXLIVEVAR_H
#include "isxLiveVar.h"
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
    poLabel,	 /* Some misc label. */
    poLoopStart, /* Start of some sort of loop. Also a label. */
    poLoopEnd,	 /* End of some sort of loop. Also a label. */
    poCondStart, /* Start of if/else or case */
    poCondCase,  /* Start of a particular case. Also a label.  */
    poCondEnd,	 /* End of if/else or case. Also a label. */
    poJump,	/* Unconditional jump */
    poBeq,	/* Branch if two things are the same. */
    poBne,	/* Branch if two things are different. */
    poBlt,	/* Branch if a < b. */
    poBle,	/* Branch if a <= b. */
    poBgt,	/* Branch if a > b. */
    poBge,	/* Branch if a >= b. */
    poBz,	/* Branch if a == 0 */
    poBnz,	/* Branch if a != 0 */
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
    iadLabel,
    iadLoopInfo,
    };

struct isxLoopInfo
/* Information associated with a loop. */
    {
    struct dlNode *start;	 /* Loop start instruction. */
    struct dlNode *end;		 /* Loop end instruction. */
    int instructionCount;	 /* Number of instructions in loop */
    struct isxAddress *iad;	 /* Associated isxAddress */
    int iteration;		 /* How many times we've been through */
    };

union isxAddressVal
/* Variable part of an isx code address. */
    {
    struct pfToken *tok;	/* For constants */
    struct pfVar *var;		/* For real vars */
    int tempMemLoc;		/* Memory location for temps, 0 for none */
    int stackOffset;		/* Stack offset. */
    struct isxLoopInfo *loopy;  /* Information on loop */
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
    float weight;	/* Number of uses, scaled for loops */
    };

struct isxLiveVar;	/* Defined in isxLiveVar.h */

struct isx
/* An instruction in our intermediate language. */
    {
    enum isxOpType opType;	/* Opcode - add, mul, branch, etc. */
    struct isxAddress *dest;	/* Destination address */
    struct isxAddress *left;	/* Left (or only) source address. */
    struct isxAddress *right;	/* Right (optional) source address.*/
    struct isxLiveVar *liveList;/* List of live vars after instruction.  */
    void *cpuInfo;		/* Machine-specific register info. */
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
    bool isLive;		/* True if holds live variable. */
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

#define isxPrefixC "_"		/* Prefix before symbols shared with C. */

#endif /* ISX_H */

