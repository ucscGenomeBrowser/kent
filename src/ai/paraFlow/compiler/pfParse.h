/* pfParse build up parse tree from token stream */
#ifndef PFPARSE_H
#define PFPARSE_H

#ifndef PFSCOPE_H
#include "pfScope.h"
#endif

enum pfParseType
/* Parse type */
    {
    pptNone = 0,
    pptProgram,
    pptScope,
    pptInto,
    pptModule,
    pptModuleRef,
    pptNop,
    pptCompound,
    pptTuple,
    pptDot,
    pptKeyVal,
    pptOf,
    pptIf,
    pptElse,
    pptWhile,
    pptFor,
    pptForeach,
    pptBreak,
    pptContinue,
    pptClass,
    pptVarDec,
    pptNameUse,
    pptVarType,
    pptConstUse,
    pptToDec,
    pptFlowDec,
    pptParaDec,
    pptReturn,
    pptCall,
    pptAssignment,
    pptPlusEquals,
    pptMinusEquals,
    pptMulEquals,
    pptDivEquals,
    pptModEquals,
    pptIndex,
    pptPlus,
    pptMinus,
    pptMul,
    pptDiv,
    pptShiftLeft,
    pptShiftRight,
    pptMod,
    pptComma,
    pptSame,
    pptNotSame,
    pptGreater,
    pptLess,
    pptGreaterOrEquals,
    pptLessOrEquals,
    pptNegate,
    pptNot,
    pptFlipBits,
    pptBitAnd,
    pptBitOr,
    pptBitXor,
    pptLogAnd,
    pptLogOr,
    pptRoot,
    pptParent,
    pptSys,
    pptUser,
    pptSysOrUser,
    pptVarInit,
    };

char *pfParseTypeAsString(enum pfParseType type);
/* Return string corresponding to pfParseType */

struct pfParse
/* The para parse tree. */
    {
    struct pfParse *next;	/* Next in list */
    enum pfParseType type;	/* Node type */
    char *name;			/* Node name - not allocated here */
    struct pfToken *tok;	/* Token associated with node. */
    struct pfScope *scope;	/* Associated scope. */
    struct pfType *expType;	/* Type of expression associated with parse. */
    struct pfParse *parent;	/* Parent statement if any. */
    struct pfParse *children;	/* subparts. */
    };

struct pfParse *pfParseNew(enum pfParseType type,
	struct pfToken *tok, struct pfParse *parent, struct pfScope *scope);
/* Return new parse node.  It's up to caller to fill in
 * children later. */

struct pfParse *pfParseFile(char *fileName, struct pfTokenizer *tkz, 
	struct pfParse *parent);
/* Convert file to parse tree using tkz. */

void pfParseDump(struct pfParse *pp, int level, FILE *f);
/* Write out pp (and it's children) to file at given level of indent */

#endif /* PFPARSE_H */
