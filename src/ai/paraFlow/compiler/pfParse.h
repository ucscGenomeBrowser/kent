/* pfParse build up parse tree from token stream */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#ifndef PFPARSE_H
#define PFPARSE_H

#ifndef PFCOMPILE_H
#include "pfCompile.h"
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
    pptVarUse,
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
    pptSymName,
    pptTypeName,
    pptTypeTuple,

    pptCastByteToShort,
    pptCastByteToInt,
    pptCastByteToLong,
    pptCastByteToFloat,
    pptCastByteToDouble,

    pptCastShortToByte,
    pptCastShortToInt,
    pptCastShortToLong,
    pptCastShortToFloat,
    pptCastShortToDouble,

    pptCastIntToByte,
    pptCastIntToShort,
    pptCastIntToLong,
    pptCastIntToFloat,
    pptCastIntToDouble,

    pptCastLongToByte,
    pptCastLongToShort,
    pptCastLongToInt,
    pptCastLongToFloat,
    pptCastLongToDouble,

    pptCastFloatToByte,
    pptCastFloatToShort,
    pptCastFloatToInt,
    pptCastFloatToLong,
    pptCastFloatToDouble,

    pptCastDoubleToByte,
    pptCastDoubleToShort,
    pptCastDoubleToInt,
    pptCastDoubleToLong,
    pptCastDoubleToFloat,

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
    struct pfType *ty;		/* Type of expression associated with parse. */
    struct pfVar *var;		/* Associated variable if any.  */
    struct pfParse *parent;	/* Parent statement if any. */
    struct pfParse *children;	/* subparts. */
    };

struct pfParse *pfParseNew(enum pfParseType type,
	struct pfToken *tok, struct pfParse *parent, struct pfScope *scope);
/* Return new parse node.  It's up to caller to fill in
 * children later. */

struct pfParse *pfParseFile(char *fileName, struct pfCompile *pfc, 
	struct pfParse *parent);
/* Convert file to parse tree using tkz. */

void pfParseTypeSub(struct pfParse *pp, enum pfParseType oldType,
	enum pfParseType newType);
/* Convert type of pp and any children that are of oldType to newType */

void pfParseDump(struct pfParse *pp, int level, FILE *f);
/* Write out pp (and it's children) to file at given level of indent */

struct pfParse *pfParseProgram(char *fileName, struct pfCompile *pfc);
/* Return parse tree of file and any files gone into. */

#endif /* PFPARSE_H */
