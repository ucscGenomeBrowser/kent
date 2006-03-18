/* isix - intermediate code -  hexes of
 *     label destType destName left op right 
 * For instance:
 *     100 + int t1 1 t2  (means int t1 = 1 + t2)
 *     101 * double v 0.5 t3  (means double v = 0.5 * t3)
 */
#ifndef ISIX_H
#define ISIX_H

#ifndef DLIST_H
#include "dlist.h"
#endif 

enum isixValType
    {
    ivByte,
    ivShort,
    ivInt,
    ivLong,
    ivFloat,
    ivDouble,
    ivObject,
    ivJump,	
    };
char *isixValTypeToString(enum isixValType val);
/* Convert isixValType to string. */

enum isixOpType
    {
    poZero,
    poAssign,	
    poPlus,
    poMinus,
    poMul,
    poDiv,
    poGoTo,	/* Unconditional jump */
    poBranch,	/* Conditional jump */
    };

char *isixOpTypeToString(enum isixOpType val);
/* Convert isixValType to string. */

enum isixAddressType
    {
    iadConst,
    iadRealVar,
    iadTempVar,
    };

union isixAddressVal
    {
    struct pfToken *tok;	/* For constants */
    struct pfVar *var;		/* For real vars */
    int	tempId;			/* ID for temp vals */
    };

struct isixAddress
/* A piece of data somewhere */
    {
    char *name;			/* Not allocated here. */
    union isixAddressVal val;	/* Address value */
    enum isixAddressType type;	/* Address type */
    };

struct isix
    {
    int label;			/* Numerical label */
    enum isixOpType opType;	/* Opcode - add, mul, branch, etc. */
    enum isixValType valType;	/* Type of expression - long,short etc. */
    struct isixAddress *dest;	/* Destination var. */
    struct isixAddress *left;	/* Left side of operation. */
    struct isixAddress *right;	/* Right side of operation. */
    };

struct isix *isixNew(struct pfCompile *pfc, 
	enum isixOpType opType,
	enum isixValType valType,
	struct isixAddress *dest,
	struct isixAddress *left,
	struct isixAddress *right, 
	struct dlList *iList);
/* Return new isix */

void isixDump(struct isix *isix, FILE *f);
/* Dump out isix code */

void isixDumpList(struct dlList *list, FILE *f);
/* Dump all of isix in list */

struct dlList *isixFromParse(struct pfCompile *pfc, struct pfParse *pp);
/* Convert parse tree to isix. */

#endif /* ISIX_H */

