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
    pptSelfUse,
    pptFieldUse,
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
    pptPlaceholder,
    pptSymName,
    pptTypeName,
    pptTypeTuple,
    pptStringCat,

    /* Note the casts must be in this order relative to pptCastBitToBit */
    pptCastBitToBit,	/* Never emitted. */
    pptCastBitToByte,	
    pptCastBitToShort,
    pptCastBitToInt,
    pptCastBitToLong,
    pptCastBitToFloat,
    pptCastBitToDouble,
    pptCastBitToString,

    pptCastByteToBit,
    pptCastByteToByte,	/* Never emitted. */
    pptCastByteToShort,
    pptCastByteToInt,
    pptCastByteToLong,
    pptCastByteToFloat,
    pptCastByteToDouble,
    pptCastByteToString,

    pptCastShortToBit,
    pptCastShortToByte,
    pptCastShortToShort,	/* NEver emitted. */
    pptCastShortToInt,
    pptCastShortToLong,
    pptCastShortToFloat,
    pptCastShortToDouble,
    pptCastShortToString,

    pptCastIntToBit,
    pptCastIntToByte,
    pptCastIntToShort,
    pptCastIntToInt,		/* Never emitted. */
    pptCastIntToLong,
    pptCastIntToFloat,
    pptCastIntToDouble,
    pptCastIntToString,

    pptCastLongToBit,
    pptCastLongToByte,
    pptCastLongToShort,
    pptCastLongToInt,
    pptCastLongToLong,		/* Never emitted. */
    pptCastLongToFloat,
    pptCastLongToDouble,
    pptCastLongToString,

    pptCastFloatToBit,
    pptCastFloatToByte,
    pptCastFloatToShort,
    pptCastFloatToInt,
    pptCastFloatToLong,
    pptCastFloatToFloat,	/* Never emitted. */
    pptCastFloatToDouble,
    pptCastFloatToString,

    pptCastDoubleToBit,
    pptCastDoubleToByte,
    pptCastDoubleToShort,
    pptCastDoubleToInt,
    pptCastDoubleToLong,
    pptCastDoubleToFloat,
    pptCastDoubleToDouble,	/* Never emitted. */
    pptCastDoubleToString,

    pptCastStringToBit,
    pptCastTypedToVar,		
    pptCastVarToTyped,
    pptCastCallToTuple,
    pptUniformTuple,

    pptConstBit,
    pptConstByte,
    pptConstShort,
    pptConstInt,
    pptConstLong,
    pptConstFloat,
    pptConstDouble,
    pptConstString,
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

void pfParsePutChildrenInPlaceOfSelf(struct pfParse **pPp);
/* Replace self with children. */

void pfParseTypeSub(struct pfParse *pp, enum pfParseType oldType,
	enum pfParseType newType);
/* Convert type of pp and any children that are of oldType to newType */

struct pfParse *pfSingleTuple(struct pfParse *parent, struct pfToken *tok, 
	struct pfParse *single);
/* Wrap tuple around single and return it.  . */

void pfParseDump(struct pfParse *pp, int level, FILE *f);
/* Write out pp (and it's children) to file at given level of indent */

struct pfParse *pfParseProgram(struct pfCompile *pfc, char *builtinCode, char *fileName);
/* Return parse tree of file and any files gone into. */

#endif /* PFPARSE_H */
