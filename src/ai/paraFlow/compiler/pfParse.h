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
    pptInclude,
    pptImport,
    pptModule,
    pptMainModule,
    pptModuleRef,
    pptNop,
    pptCompound,
    pptTuple,
    pptDot,
    pptModuleDotType,
    pptKeyVal,
    pptOf,
    pptIf,
    pptElse,
    pptWhile,
    pptFor,
    pptForeach,
    pptForEachCall,
    pptBreak,
    pptContinue,
    pptClass,
    pptInterface,
    pptVarDec,	/* Removed by pfBindVars */
    pptNameUse,	/* Removed by pfBindVars */
    pptVarUse,
    pptModuleUse,
    pptFieldUse,
    pptConstUse,	/* Removed by pfType */
    pptPolymorphic,
    pptToDec,
    pptFlowDec,
    pptReturn,
    pptCall,
    pptIndirectCall,
    pptAssign,
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
    pptVarInit,
    pptFormalParameter,
    pptPlaceholder,
    pptSymName,
    pptTypeName,
    pptTypeTuple,
    pptTypeFlowPt,
    pptTypeToPt,
    pptStringCat,
    pptParaDo,
    pptParaAdd,
    pptParaMultiply,
    pptParaAnd,
    pptParaOr,
    pptParaMin,
    pptParaMax,
    pptParaArgMin,
    pptParaArgMax,
    pptParaGet,
    pptParaFilter,
    pptUntypedElInCollection,	/* Removed by pfType */
    pptUntypedKeyInCollection,	/* Removed by pfType */
    pptOperatorDec,
    pptArrayAppend,
    pptIndexRange,
    pptClassAllocFromTuple,
    pptClassAllocFromInit,
    pptKeyName,
    pptTry,
    pptCatch,
    pptSubstitute,
    pptStringDupe,
    pptNew,
    pptCase,
    pptCaseList,
    pptCaseItem,
    pptCaseElse,

    /* Note the casts must be in this order relative to pptCastBitToBit */
    pptCastBitToBit,	/* Never emitted. */
    pptCastBitToByte,	
    pptCastBitToChar,
    pptCastBitToShort,
    pptCastBitToInt,
    pptCastBitToLong,
    pptCastBitToFloat,
    pptCastBitToDouble,
    pptCastBitToString,

    pptCastByteToBit,
    pptCastByteToByte,	/* Never emitted. */
    pptCastByteToChar,
    pptCastByteToShort,
    pptCastByteToInt,
    pptCastByteToLong,
    pptCastByteToFloat,
    pptCastByteToDouble,
    pptCastByteToString,

    pptCastCharToBit,
    pptCastCharToByte,
    pptCastCharToChar,	/* Never emitted. */
    pptCastCharToShort,
    pptCastCharToInt,
    pptCastCharToLong,
    pptCastCharToFloat,
    pptCastCharToDouble,
    pptCastCharToString,


    pptCastShortToBit,
    pptCastShortToByte,
    pptCastShortToChar,
    pptCastShortToShort,	/* Never emitted. */
    pptCastShortToInt,
    pptCastShortToLong,
    pptCastShortToFloat,
    pptCastShortToDouble,
    pptCastShortToString,

    pptCastIntToBit,
    pptCastIntToByte,
    pptCastIntToChar,
    pptCastIntToShort,
    pptCastIntToInt,		/* Never emitted. */
    pptCastIntToLong,
    pptCastIntToFloat,
    pptCastIntToDouble,
    pptCastIntToString,

    pptCastLongToBit,
    pptCastLongToByte,
    pptCastLongToChar,
    pptCastLongToShort,
    pptCastLongToInt,
    pptCastLongToLong,		/* Never emitted. */
    pptCastLongToFloat,
    pptCastLongToDouble,
    pptCastLongToString,

    pptCastFloatToBit,
    pptCastFloatToByte,
    pptCastFloatToChar,
    pptCastFloatToShort,
    pptCastFloatToInt,
    pptCastFloatToLong,
    pptCastFloatToFloat,	/* Never emitted. */
    pptCastFloatToDouble,
    pptCastFloatToString,

    pptCastDoubleToBit,
    pptCastDoubleToByte,
    pptCastDoubleToChar,
    pptCastDoubleToShort,
    pptCastDoubleToInt,
    pptCastDoubleToLong,
    pptCastDoubleToFloat,
    pptCastDoubleToDouble,	/* Never emitted. */
    pptCastDoubleToString,

    pptCastStringToBit,
    pptCastObjectToBit,
    pptCastClassToInterface,
    pptCastFunctionToPointer,
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
    pptConstChar,
    pptConstString,
    pptConstZero,

    pptTypeCount,	 /* Not emitted, just to keep track of # of types */
    };

char *pfParseTypeAsString(enum pfParseType type);
/* Return string corresponding to pfParseType */

struct pfParse
/* The para parse tree. */
    {
    struct pfParse *next;	/* Next in list */
    UBYTE type;			/* Node type */
    UBYTE access;		/* a pfAccessType */
    UBYTE isConst;		/* TRUE if constant */
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

boolean pfParseIsConst(struct pfParse *pp);
/* Return TRUE if pp is a simple constant. */

struct pfParse *pfParseEnclosingClass(struct pfParse *pp);
/* Find enclosing class if any. */

struct pfParse *pfParseEnclosingFunction(struct pfParse *pp);
/* Find enclosing function if any.  */

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

void pfParseDumpOne(struct pfParse *pp, int level, FILE *f);
/* Dump out single pfParse record at given level of indent. */

#ifdef OLD
struct pfParse *pfParseSource(struct pfCompile *pfc, struct pfSource *source,
	struct pfParse *parent, struct pfScope *scope, enum pfParseType modType);
/* Tokenize and parse given source. */
#endif /* OLD */

struct pfParse *pfParseModule(struct pfCompile *pfc, struct pfModule *module,
      struct pfParse *parent, struct pfScope *scope, enum pfParseType modType);
/* Parse a module and return parse tree associated with it. */

#endif /* PFPARSE_H */
