/* pfParse build up parse tree from token stream */
#ifndef PFPARSE_H
#define PFPARSE_H

enum pfParseType
/* Parse type */
    {
    pptNone = 0,
    pptProgram,
    pptScope,
    pptInto,
    pptModule,
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
    };

struct pfParse
/* The para parse tree. */
    {
    struct pfParse *next;	/* Next in list */
    enum pfParseType type;	/* Node type */
    struct pfToken *tok;	/* Token associated with node. */
    struct pfParse *parent;	/* Parent statement if any. */
    struct pfParse *children;	/* subparts. */
    struct pfVar *var;		/* Associated variable if any. */
    };

struct pfParse *pfParseFile(char *fileName, struct pfTokenizer *tkz);
/* Convert file to parse tree using tkz. */

void pfParseDump(struct pfParse *pp, int level, FILE *f);
/* Write out pp (and it's children) to file at given level of indent */

#endif /* PFPARSE_H */
