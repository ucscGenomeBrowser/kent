/* isx - intermediate sequenced executable -  the paraFlow
 * intermediate code.  This contains the fields:
 *    label opCode destType destList sourceList
 * For instance:
 *    100 + x 3 8		represents x = 3 + 8
 *    101 * t1 t2 t3		represents t1 = t2*t3
 *    102 = x y			represents x = y
 * It ends up being handy to have a list for the sources rather than
 * just two sources as is traditional.  Many ops take only a single
 * source for one thing.  For another this way we can represent
 * function calls with a single intermediate code sequence. */

#ifndef ISIX_H
#define ISIX_H

#ifndef DLIST_H
#include "dlist.h"
#endif 

enum isxOpType
/* What sort of operation - corresponds roughly to an
 * assembly language instruction without type or address
 * considerations. */
    {
    poInit,	
    poAssign,	
    poPlus,
    poMinus,
    poMul,
    poDiv,
    poMod,
    poGoTo,	/* Unconditional jump */
    poBranch,	/* Conditional jump */
    poCall,	/* Call subroutine */
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
    iadOperator,
    };

union isxAddressVal
    {
    struct pfToken *tok;	/* For constants */
    struct pfVar *var;		/* For real vars */
    int tempMemLoc;		/* Memory location for temps, 0 for none */
    };

struct isxAddress
/* A piece of data somewhere */
    {
    char *name;			/* Not allocated here. */
    enum isxAddressType adType;/* Address type */
    enum isxValType valType;	/* Value type */
    union isxAddressVal val;	/* Address value */
    struct regInfo *reg;	/* Register if any */
    };

struct isx
/* An instruction in our intermediate language. */
    {
    int label;			/* Numerical label */
    enum isxOpType opType;	/* Opcode - add, mul, branch, etc. */
    struct slRef *destList;	/* Destination addresses */
    struct slRef *sourceList;	/* Source addresses */
    struct slRef *liveList;	/* List of live vars after instruction  */
    };

struct isx *isxNew(struct pfCompile *pfc, 
	enum isxOpType opType,
	struct slRef *destList,
	struct slRef *sourceList,
	struct dlList *iList);
/* Return new isx */

void isxDump(struct isx *isx, FILE *f);
/* Dump out isx code */

void isxDumpList(struct dlList *list, FILE *f);
/* Dump all of isx in list */

struct dlList *isxFromParse(struct pfCompile *pfc, struct pfParse *pp);
/* Convert parse tree to isx. */

void isxToPentium(struct dlList *iList, FILE *f);
/* Convert isx code to pentium instructions in file. */

#endif /* ISIX_H */

