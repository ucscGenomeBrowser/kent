/* parseDump - all you need to dump out a parse tree in 
 * a human-readable form. */

#include "common.h"
#include "hash.h"
#include "pfToken.h"
#include "pfType.h"
#include "pfParse.h"

char *pfParseTypeAsString(enum pfParseType type)
/* Return string corresponding to pfParseType */
{
switch (type)
    {
    case pptProgram:
    	return "pptProgram";
    case pptInclude:
        return "pptInclude";
    case pptImport:
        return "pptImport";
    case pptModule:
        return "pptModule";
    case pptModuleRef:
        return "pptModuleRef";
    case pptMainModule:
        return "pptMainModule";
    case pptNop:
    	return "pptNop";
    case pptCompound:
    	return "pptCompound";
    case pptTuple:
        return "pptTuple";
    case pptOf:
        return "pptOf";
    case pptDot:
        return "pptDot";
    case pptModuleDotType:
	return "pptModuleDotType";
    case pptKeyVal:
        return "pptKeyVal";
    case pptIf:
    	return "pptIf";
    case pptElse:
    	return "pptElse";
    case pptWhile:
    	return "pptWhile";
    case pptFor:
	return "pptFor";
    case pptForeach:
	return "pptForeach";
    case pptForEachCall:
	return "pptForEachCall";
    case pptBreak:
    	return "pptBreak";
    case pptContinue:
    	return "pptContinue";
    case pptClass:
	return "pptClass";
    case pptInterface:
	return "pptInterface";
    case pptVarDec:
	return "pptVarDec";
    case pptNameUse:
	return "pptNameUse";
    case pptFieldUse:
	return "pptFieldUse";
    case pptVarUse:
	return "pptVarUse";
    case pptModuleUse:
	return "pptModuleUse";
    case pptConstUse:
	return "pptConstUse";
    case pptPolymorphic:
        return "pptPolymorphic";
    case pptToDec:
	return "pptToDec";
    case pptFlowDec:
	return "pptFlowDec";
    case pptReturn:
	return "pptReturn";
    case pptCall:
	return "pptCall";
    case pptIndirectCall:
	return "pptIndirectCall";
    case pptAssign:
	return "pptAssign";
    case pptPlusEquals:
	return "pptPlusEquals";
    case pptMinusEquals:
	return "pptMinusEquals";
    case pptMulEquals:
	return "pptMulEquals";
    case pptDivEquals:
	return "pptDivEquals";
    case pptIndex:
	return "pptIndex";
    case pptPlus:
	return "pptPlus";
    case pptMinus:
	return "pptMinus";
    case pptMul:
	return "pptMul";
    case pptDiv:
	return "pptDiv";
    case pptShiftLeft:
        return "pptShiftLeft";
    case pptShiftRight:
        return "pptShiftRight";
    case pptMod:
	return "pptMod";
    case pptComma:
	return "pptComma";
    case pptSame:
	return "pptSame";
    case pptNotSame:
	return "pptNotSame";
    case pptGreater:
	return "pptGreater";
    case pptLess:
	return "pptLess";
    case pptGreaterOrEquals:
	return "pptGreaterOrEquals";
    case pptLessOrEquals:
	return "pptLessOrEquals";
    case pptNegate:
	return "pptNegate";
    case pptNot:
        return "pptNot";
    case pptFlipBits:
        return "pptFlipBits";
    case pptBitAnd:
	return "pptBitAnd";
    case pptBitOr:
	return "pptBitOr";
    case pptBitXor:
	return "pptBitXor";
    case pptLogAnd:
	return "pptLogAnd";
    case pptLogOr:
	return "pptLogOr";
    case pptVarInit:
        return "pptVarInit";
    case pptFormalParameter:
        return "pptFormalParameter";
    case pptPlaceholder:
        return "pptPlaceholder";
    case pptSymName:
	return "pptSymName";
    case pptTypeName:
	return "pptTypeName";
    case pptTypeTuple:
	return "pptTypeTuple";
    case pptTypeFlowPt:
	return "pptTypeFlowPt";
    case pptTypeToPt:
	return "pptTypeToPt";
    case pptStringCat:
	return "pptStringCat";
    case pptParaDo:
	return "pptParaDo";
    case pptParaAdd:
	return "pptParaAdd";
    case pptParaMultiply:
	return "pptParaMultiply";
    case pptParaAnd:
	return "pptParaAnd";
    case pptParaOr:
	return "pptParaOr";
    case pptParaMin:
	return "pptParaMin";
    case pptParaMax:
	return "pptParaMax";
    case pptParaArgMin:
	return "pptParaArgMin";
    case pptParaArgMax:
	return "pptParaArgMax";
    case pptParaGet:
	return "pptParaGet";
    case pptParaFilter:
	return "pptParaFilter";
    case pptUntypedElInCollection:
	return "pptUntypedElInCollection";
    case pptUntypedKeyInCollection:
	return "pptUntypedKeyInCollection";
    case pptOperatorDec:
	return "pptOperatorDec";
    case pptArrayAppend:
	return "pptArrayAppend";
    case pptIndexRange:
	return "pptIndexRange";
    case pptClassAllocFromInit:
	return "pptClassAllocFromInit";
    case pptClassAllocFromTuple:
	return "pptClassAllocFromTuple";
    case pptKeyName:
	return "pptKeyName";
    case pptTry:
	return "pptTry";
    case pptCatch:
	return "pptCatch";
    case pptSubstitute:
        return "pptSubstitute";
    case pptStringDupe:
        return "pptStringDupe";
    case pptNew:
        return "pptNew";
    case pptCase:
	return "pptCase";
    case pptCaseList:
	return "pptCaseList";
    case pptCaseItem:
	return "pptCaseItem";
    case pptCaseElse:
	return "pptCaseElse";

    case pptCastBitToBit:
        return "pptCastBitToBit";
    case pptCastBitToByte:
        return "pptCastBitToByte";
    case pptCastBitToChar:
        return "pptCastBitToChar";
    case pptCastBitToShort:
	return "pptCastBitToShort";
    case pptCastBitToInt:
	return "pptCastBitToInt";
    case pptCastBitToLong:
	return "pptCastBitToLong";
    case pptCastBitToFloat:
	return "pptCastBitToFloat";
    case pptCastBitToDouble:
	return "pptCastBitToDouble";
    case pptCastBitToString:
        return "pptCastBitToString";

    case pptCastByteToBit:
        return "pptCastByteToBit";
    case pptCastByteToByte:
        return "pptCastByteToByte";
    case pptCastByteToChar:
        return "pptCastByteToChar";
    case pptCastByteToShort:
	return "pptCastByteToShort";
    case pptCastByteToInt:
	return "pptCastByteToInt";
    case pptCastByteToLong:
	return "pptCastByteToLong";
    case pptCastByteToFloat:
	return "pptCastByteToFloat";
    case pptCastByteToDouble:
	return "pptCastByteToDouble";
    case pptCastByteToString:
        return "pptCastByteToString";

    case pptCastCharToBit:
	return "pptCastCharToBit";
    case pptCastCharToByte:
	return "pptCastCharToByte";
    case pptCastCharToChar:
	return "pptCastCharToChar";
    case pptCastCharToShort:
	return "pptCastCharToShort";
    case pptCastCharToInt:
	return "pptCastCharToInt";
    case pptCastCharToLong:
	return "pptCastCharToLong";
    case pptCastCharToFloat:
	return "pptCastCharToFloat";
    case pptCastCharToDouble:
	return "pptCastCharToDouble";
    case pptCastCharToString:
	return "pptCastCharToString";

    case pptCastShortToBit:
        return "pptCastShortToBit";
    case pptCastShortToByte:
	return "pptCastShortToByte";
    case pptCastShortToChar:
        return "pptCastShortToChar";
    case pptCastShortToInt:
	return "pptCastShortToInt";
    case pptCastShortToLong:
	return "pptCastShortToLong";
    case pptCastShortToFloat:
	return "pptCastShortToFloat";
    case pptCastShortToDouble:
	return "pptCastShortToDouble";
    case pptCastShortToString:
        return "pptCastShortToString";

    case pptCastIntToBit:
        return "pptCastIntToBit";
    case pptCastIntToByte:
	return "pptCastIntToByte";
    case pptCastIntToChar:
        return "pptCastIntToChar";
    case pptCastIntToShort:
	return "pptCastIntToShort";
    case pptCastIntToLong:
	return "pptCastIntToLong";
    case pptCastIntToFloat:
	return "pptCastIntToFloat";
    case pptCastIntToDouble:
	return "pptCastIntToDouble";
    case pptCastIntToString:
        return "pptCastIntToString";

    case pptCastLongToBit:
        return "pptCastLongToBit";
    case pptCastLongToByte:
	return "pptCastLongToByte";
    case pptCastLongToChar:
        return "pptCastLongToChar";
    case pptCastLongToShort:
	return "pptCastLongToShort";
    case pptCastLongToInt:
	return "pptCastLongToInt";
    case pptCastLongToFloat:
	return "pptCastLongToFloat";
    case pptCastLongToDouble:
	return "pptCastLongToDouble";
    case pptCastLongToString:
        return "pptCastLongToString";

    case pptCastFloatToBit:
        return "pptCastFloatToBit";
    case pptCastFloatToByte:
	return "pptCastFloatToByte";
    case pptCastFloatToChar:
        return "pptCastFloatToChar";
    case pptCastFloatToShort:
	return "pptCastFloatToShort";
    case pptCastFloatToInt:
	return "pptCastFloatToInt";
    case pptCastFloatToLong:
	return "pptCastFloatToLong";
    case pptCastFloatToDouble:
	return "pptCastFloatToDouble";
    case pptCastFloatToString:
        return "pptCastFloatToString";

    case pptCastDoubleToBit:
        return "pptCastDoubleToBit";
    case pptCastDoubleToByte:
	return "pptCastDoubleToByte";
    case pptCastDoubleToChar:
        return "pptCastDoubleToChar";
    case pptCastDoubleToShort:
	return "pptCastDoubleToShort";
    case pptCastDoubleToInt:
	return "pptCastDoubleToInt";
    case pptCastDoubleToLong:
	return "pptCastDoubleToLong";
    case pptCastDoubleToFloat:
	return "pptCastDoubleToFloat";
    case pptCastDoubleToDouble:
	return "pptCastDoubleToDouble";
    case pptCastDoubleToString:
        return "pptCastDoubleToString";

    case pptCastStringToBit:
        return "pptCastStringToBit";
    case pptCastObjectToBit:
        return "pptCastObjectToBit";
    case pptCastClassToInterface:
    	return "pptCastClassToInterface";
    case pptCastFunctionToPointer:
	return "pptCastFunctionToPointer";
    case pptCastTypedToVar:
        return "pptCastTypedToVar";
    case pptCastVarToTyped:
        return "pptCastVarToTyped";
    case pptCastCallToTuple:
	return "pptCastCallToTuple";
    case pptUniformTuple:
        return "pptUniformTuple";
    case pptConstBit:
	return "pptConstBit";
    case pptConstByte:
	return "pptConstByte";
    case pptConstShort:
	return "pptConstShort";
    case pptConstInt:
	return "pptConstInt";
    case pptConstLong:
	return "pptConstLong";
    case pptConstFloat:
	return "pptConstFloat";
    case pptConstDouble:
	return "pptConstDouble";
    case pptConstChar:
	return "pptConstChar";
    case pptConstString:
	return "pptConstString";
    case pptConstZero:
	return "pptConstZero";

    default:
        internalErr();
	return NULL;
    }
}

static void pfDumpConst(struct pfToken *tok, FILE *f)
/* Dump out constant to file */
{
switch (tok->type)
    {
    case pftString:
    case pftSubstitute:
	{
	#define MAXLEN 32
	char buf[MAXLEN+4];
	char *s = tok->val.s;
	int len = strlen(s);
	if (len > MAXLEN) len = MAXLEN;
	memcpy(buf, s, len);
	buf[len] = 0;
	strcpy(buf+MAXLEN, "...");
	printEscapedString(f, buf);
	break;
	#undef MAXLEN
	}
    case pftLong:
        fprintf(f, "%lld", tok->val.l);
	break;
    case pftInt:
        fprintf(f, "%d", tok->val.i);
	break;
    case pftFloat:
        fprintf(f, "%g", tok->val.x);
	break;
    case pftNil:
        fprintf(f, "nil");
	break;
    default:
        fprintf(f, "unknownType");
	break;
    }
}

void pfParseDumpOne(struct pfParse *pp, int level, FILE *f)
/* Dump out single pfParse record at given level of indent. */
{
spaceOut(f, level*3);
fprintf(f, "%s", pfParseTypeAsString(pp->type));
if (pp->ty != NULL)
    {
    fprintf(f, " ");
    pfTypeDump(pp->ty, f);
    }
else
    {
    if (pp->access != paUsual)
	fprintf(f, " %s", pfAccessTypeAsString(pp->access));
    if (pp->isConst)
	fprintf(f, " const");
    }
if (pp->name != NULL)
    fprintf(f, " %s", pp->name);
switch (pp->type)
    {
    case pptConstUse:
    case pptConstBit:
    case pptConstByte:
    case pptConstShort:
    case pptConstInt:
    case pptConstLong:
    case pptConstFloat:
    case pptConstDouble:
    case pptConstChar:
    case pptConstString:
    case pptConstZero:
    case pptSubstitute:
	fprintf(f, " ");
	pfDumpConst(pp->tok, f);
	break;
    }
fprintf(f, "\n");
}

void pfParseDump(struct pfParse *pp, int level, FILE *f)
/* Write out pp (and it's children) to file at given level of indent */
{
pfParseDumpOne(pp, level, f);
level += 1;
for (pp = pp->children; pp != NULL; pp = pp->next)
    pfParseDump(pp, level, f);
}

