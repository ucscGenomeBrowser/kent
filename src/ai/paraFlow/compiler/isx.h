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

#ifndef PFSTRUCT_H
#include "pfStruct.h"
#endif

#ifndef DLIST_H
#include "dlist.h"
#endif 

#ifndef ISXLIVEVAR_H
#include "isxLiveVar.h"
#endif

#ifndef PFTOKEN_H
#include "pfToken.h"
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
    poFuncStart,/* Declare start of function */
    poFuncEnd,  /* Declare end of function. */
    poReturnVal,/* Declare return value. */
    poCast,	/* Convert from one type to another. */
    poVarInit,	 /* Variable type initialization */
    poVarAssign, /* Variable type (re)assignment */
    };

char *isxOpTypeToString(enum isxOpType val);
/* Convert isxValType to string. */

enum isxValType
/* Simplified value type system.  */
    {
    ivZero,
    ivBit,
    ivByte,
    ivShort,
    ivInt,
    ivLong,
    ivFloat,
    ivDouble,
    ivString,
    ivObject,
    ivVar,
    ivJump,	
    };

char *isxValTypeToString(enum isxValType val);
/* Convert isxValType to string. */

enum isxValType isxValTypeFromTy(struct pfCompile *pfc, struct pfType *ty);
/* Return isxValType corresponding to pfType  */

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
    iadReturnVar,
    iadRecodedType,
    };

struct isxLoopVar
/* Information about a "hot" variable in a loop.  That is one
 * that is live at end of loop, and used within the loop. */
    {
    struct isxLoopVar *next;
    struct isxAddress *iad;	/* Associated address including weight. */
    struct isxReg *reg;		/* Assigned register if any. */
    };

struct isxLoopInfo
/* Information associated with a loop. */
    {
    struct isxLoopInfo *next;	 /* Younger sibling. */
    struct isxLoopInfo *children;/* Eldest child. */
    struct dlNode *start;	 /* Loop start instruction. */
    struct dlNode *end;		 /* Loop end instruction. */
    int instructionCount;	 /* Number of instructions in loop */
    struct isxAddress *iad;	 /* Associated isxAddress */
    int iteration;		 /* How many times we've been through */
    struct isxLoopVar *hotLive;	 /* Variables good to put in registers, 
    				  * most important first. */
    };

#ifdef OLD
struct ctar;	
#endif /* OLD */

struct isxToken
/* A smaller version of lexical token for code generator. */
    {
    enum pfTokType type;	/* Token type. */
    union pfTokVal val;		/* Type-dependent value. */
    };

union isxAddressVal
/* Variable part of an isx code address. */
    {
    struct isxToken isxTok;	/* For constants */
    struct pfVar *var;		/* For real vars */
    int tempMemLoc;		/* Memory location for temps, 0 for none */
    int ioOffset;		/* Stack offset. */
    struct isxLoopInfo *loopy;  /* Information on loop */
#ifdef OLD
    struct ctar *ctar;		/* Information about function variables. */
#endif /* OLD */
    int recodedType;		/* Local type information */
    };

struct isxAddress
/* A piece of data somewhere */
    {
    struct isxAddress *next;	/* For multivalued functions */
    char *name;			/* Not allocated here. */
    enum isxAddressType adType;/* Address type */
    enum isxValType valType;	/* Value type */
    union isxAddressVal val;	/* Address value */
    int stackOffset;		/* Stack offset. */
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
    struct isxAddress *reserved; /* Register reserved for this var. */
    bool isLive;		/* True if holds live variable. */
    int regIx;			/* Index of register in regInfo */
    };


struct isx *isxNew(struct pfCompile *pfc, enum isxOpType opType,
	struct isxAddress *dest, struct isxAddress *left,
	struct isxAddress *right);
/* Make new isx instruction. */

struct isx *isxAddNew(struct pfCompile *pfc, enum isxOpType opType,
	struct isxAddress *dest, struct isxAddress *left,
	struct isxAddress *right, struct dlList *iList);
/* Make new isx instruction, and hang it on iList */

char *isxRegName(struct isxReg *reg, enum isxValType valType);
/* Get name of reg for given type. */

struct isxAddress *isxTempVarAddress(struct pfCompile *pfc, struct hash *hash,
	double weight, enum isxValType valType);
/* Create a new temporary var */

struct isxAddress *isxConstAddress(enum pfTokType tokType,
	union pfTokVal tokVal, enum isxValType valType);
/* Get place to put constant. */

struct isxAddress *isxRecodedTypeAddress(struct pfCompile *pfc, 
	struct pfType *type);
/* Get type info. */

struct isxAddress *isxIoAddress(int offset, enum isxValType valType,
	enum isxAddressType adType);
/* Return reference to an io stack address */

struct isxAddress *isxCallAddress(char *name);
/* Create reference to a function */

struct isxAddress *isxTempLabelAddress(struct pfCompile *pfc);
/* Create reference to a temporary label for jumping to. */

void isxAddressDump(struct isxAddress *iad, FILE *f);
/* Print variable name or n/a */

void isxDump(struct isx *isx, FILE *f);
/* Dump out isx code */

void isxDumpList(struct dlList *list, FILE *f);
/* Dump all of isx in list */

void isxStatement(struct pfCompile *pfc, struct pfParse *pp, 
	struct hash *varHash, double weight, struct dlList *iList);
/* Generate intermediate code for statement. */

void isxAddReturnInfo(struct pfCompile *pfc, struct pfParse *outTuple, 
	struct hash *varHash, struct dlList *iList);
/* Add instructions for return parameters. */

struct isxList
/* A list of instructions, and a list of loop information */
    {
    struct dlList *iList;		/* List of instructions */
    struct isxLoopInfo *loopList;	/* List of loops */
    };

struct isxList *isxListNew();
/* Create new empty isxList */

struct isxList *isxModuleVars(struct pfCompile *pfc, struct pfParse *module);
/* Convert module-level and static function variables to an isxList */

struct isxList *isxCodeFunction(struct pfCompile *pfc, struct pfParse *funcPp);
/* Generate code for a function */

#define isxPrefixC "_"	/* Prefix before symbols shared with C. */

#endif /* ISX_H */

