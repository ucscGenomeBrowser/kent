/* pfParse build up parse tree from token stream */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "localmem.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"

char *pfParseTypeAsString(enum pfParseType type)
/* Return string corresponding to pfParseType */
{
switch (type)
    {
    case pptProgram:
    	return "pptProgram";
    case pptScope:
        return "pptScope";
    case pptInto:
        return "pptInto";
    case pptModule:
        return "pptModule";
    case pptModuleRef:
        return "pptModuleRef";
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
    case pptBreak:
    	return "pptBreak";
    case pptContinue:
    	return "pptContinue";
    case pptClass:
	return "pptClass";
    case pptVarDec:
	return "pptVarDec";
    case pptNameUse:
	return "pptNameUse";
    case pptVarUse:
	return "pptVarUse";
    case pptConstUse:
	return "pptConstUse";
    case pptToDec:
	return "pptToDec";
    case pptFlowDec:
	return "pptFlowDec";
    case pptParaDec:
	return "pptParaDec";
    case pptReturn:
	return "pptReturn";
    case pptCall:
	return "pptCall";
    case pptAssignment:
	return "pptAssignment";
    case pptPlusEquals:
	return "pptPlusEquals";
    case pptMinusEquals:
	return "pptMinusEquals";
    case pptMulEquals:
	return "pptMulEquals";
    case pptDivEquals:
	return "pptDivEquals";
    case pptModEquals:
	return "pptModEquals";
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
    case pptRoot:
	return "pptRoot";
    case pptParent:
	return "pptParent";
    case pptSys:
	return "pptSys";
    case pptUser:
	return "pptUser";
    case pptSysOrUser:
	return "pptSysOrUser";
    case pptVarInit:
        return "pptVarInit";
    case pptSymName:
	return "pptSymName";
    case pptTypeName:
	return "pptTypeName";
    case pptTypeTuple:
	return "pptTypeTuple";
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
    case pptCastShortToByte:
	return "pptCastShortToByte";
    case pptCastShortToInt:
	return "pptCastShortToInt";
    case pptCastShortToLong:
	return "pptCastShortToLong";
    case pptCastShortToFloat:
	return "pptCastShortToFloat";
    case pptCastShortToDouble:
	return "pptCastShortToDouble";
    case pptCastIntToByte:
	return "pptCastIntToByte";
    case pptCastIntToShort:
	return "pptCastIntToShort";
    case pptCastIntToLong:
	return "pptCastIntToLong";
    case pptCastIntToFloat:
	return "pptCastIntToFloat";
    case pptCastIntToDouble:
	return "pptCastIntToDouble";
    case pptCastLongToByte:
	return "pptCastLongToByte";
    case pptCastLongToShort:
	return "pptCastLongToShort";
    case pptCastLongToInt:
	return "pptCastLongToInt";
    case pptCastLongToFloat:
	return "pptCastLongToFloat";
    case pptCastLongToDouble:
	return "pptCastLongToDouble";
    case pptCastFloatToByte:
	return "pptCastFloatToByte";
    case pptCastFloatToShort:
	return "pptCastFloatToShort";
    case pptCastFloatToInt:
	return "pptCastFloatToInt";
    case pptCastFloatToLong:
	return "pptCastFloatToLong";
    case pptCastFloatToDouble:
	return "pptCastFloatToDouble";
    case pptCastDoubleToByte:
	return "pptCastDoubleToByte";
    case pptCastDoubleToShort:
	return "pptCastDoubleToShort";
    case pptCastDoubleToInt:
	return "pptCastDoubleToInt";
    case pptCastDoubleToLong:
	return "pptCastDoubleToLong";
    case pptCastDoubleToFloat:
	return "pptCastDoubleToFloat";
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
    case pptConstString:
	return "pptConstString";

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
	{
	#define MAXLEN 32
	char buf[MAXLEN+4];
	char *s = tok->val.s;
	int len = strlen(s);
	if (len > MAXLEN) len = MAXLEN;
	memcpy(buf, s, len);
	buf[len] = 0;
	strcpy(buf+MAXLEN, "...");
	fprintf(f, "\"%s\"", buf);
	break;
	#undef MAXLEN
	}
    case pftInt:
        fprintf(f, "%lld", tok->val.i);
	break;
    case pftFloat:
        fprintf(f, "%g", tok->val.x);
	break;
    default:
        fprintf(f, "unknownType");
	break;
    }
}

void pfParseDump(struct pfParse *pp, int level, FILE *f)
/* Write out pp (and it's children) to file at given level of indent */
{
struct pfParse *child;
spaceOut(f, level*3);
fprintf(f, "%s", pfParseTypeAsString(pp->type));
if (pp->ty != NULL)
    {
    fprintf(f, " ");
    pfTypeDump(pp->ty, f);
    }
if (pp->name != NULL)
    fprintf(f, " %s", pp->name);
switch (pp->type)
    {
    case pptConstUse:
    case pptConstByte:
    case pptConstShort:
    case pptConstInt:
    case pptConstLong:
    case pptConstFloat:
    case pptConstDouble:
    case pptConstString:
	fprintf(f, " ");
	pfDumpConst(pp->tok, f);
	break;
    }
fprintf(f, "\n");
for (child = pp->children; child != NULL; child = child->next)
    pfParseDump(child, level+1, f);
}


struct pfParse *pfParseSemilessStatement(struct pfParse *parent, 
	struct pfToken **pTokList, struct pfScope *scope);

struct pfParse *pfParseStatement(struct pfParse *parent, 
	struct pfToken **pTokList, struct pfScope *scope);

struct pfParse *pfParseExpression(struct pfParse *parent, 
	struct pfToken **pTokList, struct pfScope *scope);

static void syntaxError(struct pfToken *tok, int code)
/* Complain about syntax error and quit. */
{
errAt(tok, "Syntax error #%d", code);
}

struct pfParse *pfParseNew(enum pfParseType type,
	struct pfToken *tok, struct pfParse *parent, struct pfScope *scope)
/* Return new parse node.  It's up to caller to fill in
 * children later. */
{
struct pfParse *pp;
AllocVar(pp);
pp->type = type;
pp->tok = tok;
pp->parent = parent;
pp->scope = scope;
return pp;
}

struct pfParse *emptyTuple(struct pfParse *parent, struct pfToken *tok,
	struct pfScope *scope)
/* Return empty tuple. */
{
return pfParseNew(pptTuple, tok, parent, scope);
}

struct pfParse *emptyStatement(struct pfParse *parent, struct pfToken *tok,
	struct pfScope *scope)
/* Return no-op statement. */
{
return pfParseNew(pptNop, tok, parent, scope);
}

struct pfParse *singleTuple(struct pfParse *parent, struct pfToken *tok, struct pfParse *single)
/* Wrap tuple around single. */
{
struct pfParse *tuple = pfParseNew(pptTuple, tok, parent, parent->scope);
tuple->children = single;
single->parent = tuple;
single->next = NULL;
return tuple;
}

static void skipRequiredName(char *name, struct pfToken **pTokList)
/* Make sure next token matches name, and then skip it. */
{
struct pfToken *tok = *pTokList;
if (tok->type != pftName || !sameString(tok->val.s, name))
    expectingGot(name, tok);
*pTokList = tok->next;
}

struct pfParse *parseNameUse(struct pfParse *parent, struct pfToken **pTokList, struct pfScope *scope)
/* Make sure have a name, and create a varUse type node
 * based on it. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptNameUse, tok, parent, scope);
if (tok->type != pftName)
    {
    errAt(tok, "Expecting variable.");
    }
pp->name = tok->val.s;
*pTokList = tok->next;
return pp;
}

struct pfParse *parseDottedNames(struct pfParse *parent, struct pfToken **pTokList, struct pfScope *scope)
/* Parse this.that.the.other */
{
struct pfToken *tok = *pTokList;
struct pfParse *dots, *first = NULL, *pp;
boolean startsSpecial = FALSE;

/* If it's a name try and take shortcut if there's no
 * following dot,  otherwise set up first from name. */
if (tok->type == pftName)
    {
    first = parseNameUse(parent, &tok, scope);
    if (tok->type != '.')
	{
	*pTokList = tok;
	return first;
	}
    else
        tok = tok->next;
    }
/* Eat through special token at front of name. */
else
    {
    enum pfParseType type = pptNone;
    startsSpecial = TRUE;
    switch (tok->type)
	{
	case pftRoot:
	    type = pptRoot;
	    break;
	case pftParent:
	    type = pptParent;
	    break;
	case pftSys:
	    type = pptSys;
	    break;
	case pftUser:
	    type = pptUser;
	    break;
	case pftSysOrUser:
	    type = pptSysOrUser;
	    break;
	default:
	    expectingGot("name", tok);
	    break;
	}
    first = pfParseNew(type, tok, NULL, scope);
    tok = tok->next;
    }

/* Create dot list parse and add first element to it. */
dots = pfParseNew(pptDot, tok, parent, scope);
first->parent = dots;
slAddHead(&dots->children, first);

/* In some cases there may be additional parents to add in front. */
if (startsSpecial)
    {
    while (tok->type == pftParent)
	{
	tok = tok->next;
	pp = pfParseNew(pptParent, tok, dots, scope);
	slAddHead(&dots->children, pp);
	}
    }

/* Keep adding names until we run out of dots. */
for (;;)
    {
    if (tok->type != pftName)
        expectingGot("name", tok);
    pp = parseNameUse(dots, &tok, scope);
    slAddHead(&dots->children, pp);
    if (tok->type != '.')
        break;
    tok = tok->next;
    }

/* Clean up and go home. */
slReverse(&dots->children);
*pTokList = tok;
return dots;
}


struct pfParse *parseOfs(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse of separated expression. */
{
struct pfParse *pp = parseDottedNames(parent, pTokList, scope);
struct pfToken *tok = *pTokList;
struct pfParse *ofs = NULL;
if (tok->type == pftOf)
    {
    ofs = pfParseNew(pptOf, tok, parent, scope);
    ofs->children = pp;
    pp->parent = ofs;
    while (tok->type == pftOf)
	{
	tok = tok->next;
	pp = parseDottedNames(ofs, &tok, scope);
	slAddHead(&ofs->children, pp);
	}
    }
*pTokList = tok;
if (ofs != NULL)
    {
    slReverse(&ofs->children);
    return ofs;
    }
else
    return pp;
}

struct pfParse *varUseOrDeclare(struct pfParse *parent, struct pfToken **pTokList, struct pfScope *scope)
/* Make sure have a name, and create a varUse type node
 * based on it. */
{
struct pfToken *tok = *pTokList;
struct pfBaseType *baseType = NULL;

if (tok->type == pftName)
    baseType = pfScopeFindType(scope, tok->val.s);
if (baseType != NULL)
    {
    struct pfToken *nextTok = tok->next;
    if (nextTok->type == pftName || nextTok->type == pftOf)
	{
	struct pfParse *name, *type;
	struct pfParse *pp = pfParseNew(pptVarDec, tok, parent, scope);
	type = parseOfs(pp, &tok, scope);
	name = parseDottedNames(pp, &tok, scope);
	pp->children = type;
	type->next = name;
	*pTokList = tok;
	return pp;
	}
    }
return parseDottedNames(parent, pTokList, scope);
}

struct pfParse *constUse(struct pfParse *parent, struct pfToken **pTokList, struct pfScope *scope)
/* Create constant use */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptConstUse, tok, parent, scope);
*pTokList = tok->next;
return pp;
}


static struct pfParse *parseAtom(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse atomic expression - const, variable, or parenthesized expression. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = NULL;
switch (tok->type)
    {
    case pftName:
    case pftRoot:
    case pftParent:
    case pftSys:
    case pftUser:
    case pftSysOrUser:
	pp = varUseOrDeclare(parent, &tok, scope);
	break;
    case pftString:
    case pftInt:
    case pftFloat:
        pp = constUse(parent, &tok, scope);
	break;
    case '(':
        {
	struct pfToken *startTok = tok;
	tok = tok->next;
	pp = pfParseExpression(parent, &tok, scope);
	if (tok->type != ')')
	    errAt(tok, "Unclosed parenthesis.");
	tok = tok->next;
	break;
	}
    default:
        syntaxError(tok, 1);
	break;
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseCall(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse out function call. */
{
struct pfParse *pp = parseAtom(parent, pTokList, scope);
struct pfToken *tok = *pTokList;
if (tok->type == '(')
    {
    struct pfParse *func = pp;
    struct pfParse *parameters = NULL;
    pp = pfParseNew(pptCall, tok, parent, scope);
    func->parent = pp;
    pp->children = func;
    tok = tok->next;
    if (tok->type != ')')
        {
	parameters = pfParseExpression(pp, &tok, scope);
	if (parameters->type != pptTuple)
	    {
	    struct pfParse *tuple = singleTuple(pp, tok, parameters);
	    parameters->parent = tuple;
	    parameters = tuple;
	    }
	}
    else
        {
	parameters = emptyTuple(pp, tok, scope);
	}
    func->next = parameters;
    if (tok->type != ')')
        expectingGot(")", tok);
    tok = tok->next;
    *pTokList = tok;
    }
return pp;
}

struct pfParse *parseNegation(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse unary minus. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
if (tok->type == '-')
    {
    pp = pfParseNew(pptNegate, tok, parent, scope);
    tok = tok->next;
    pp->children = parseNegation(pp, &tok, scope);
    }
else if (tok->type == '!')
    {
    pp = pfParseNew(pptNot, tok, parent, scope);
    tok = tok->next;
    pp->children = parseNegation(pp, &tok, scope);
    }
else if (tok->type == '~')
    {
    pp = pfParseNew(pptFlipBits, tok, parent, scope);
    tok = tok->next;
    pp->children = parseNegation(pp, &tok, scope);
    }
else
    {
    pp = parseCall(parent, &tok, scope);
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseProduct(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse things separated by '*' or '/' or '%'. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseNegation(parent, &tok, scope);
while (tok->type == '*' || tok->type == '/' || tok->type == '%'
	|| tok->type == pftShiftLeft || tok->type == pftShiftRight
	|| tok->type == '&')
    {
    struct pfParse *left = pp, *right;
    enum pfTokType tt = pptNone;
    switch (tok->type)
        {
	case '*':
	   tt = pptMul;
	   break;
	case '/':
	   tt = pptDiv;
	   break;
	case '%':
	   tt = pptMod;
	   break;
	case pftShiftLeft:
	   tt = pptShiftLeft;
	   break;
	case pftShiftRight:
	   tt = pptShiftRight;
	   break;
	case '&':
	   tt = pptBitAnd;
	   break;
	}
    pp = pfParseNew(tt, tok, parent, scope);
    left->parent = pp;
    tok = tok->next;
    right = parseNegation(pp, &tok, scope);
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseSum(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse + and - part of expression. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseProduct(parent, &tok, scope);
while (tok->type == '+' || tok->type == '-' || tok->type == '|' || tok->type == '^')
    {
    struct pfParse *left = pp, *right;
    enum pfTokType tt = pptNone;
    switch (tok->type)
        {
	case '+':
	   tt = pptPlus;
	   break;
	case '-':
	   tt = pptMinus;
	   break;
	case '|':
	   tt = pptBitOr;
	   break;
	case '^':
	   tt = pptBitXor;
	   break;
	}
    pp = pfParseNew(tt, tok, parent, scope);
    left->parent = pp;
    tok = tok->next;
    right = parseProduct(pp, &tok, scope);
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseCmp(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse == >=, <= > < and != part of expression. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseSum(parent, &tok, scope);
while (tok->type == pftEqualsEquals || tok->type == pftNotEquals 
	|| tok->type == pftGreaterOrEquals || tok->type == pftLessOrEquals
	|| tok->type == '<' || tok->type == '>')
    {
    struct pfParse *left = pp, *right;
    enum pfTokType tt = 0;
    switch (tok->type)
	{
	case pftEqualsEquals:
	    tt = pptSame;
	    break;
	case pftNotEquals:
	    tt = pptNotSame;
	    break;
	case pftGreaterOrEquals:
	    tt = pptGreaterOrEquals;
	    break;
	case pftLessOrEquals:
	    tt = pptLessOrEquals;
	    break;
	case '>':
	    tt = pptGreater;
	    break;
	case '<':
	    tt = pptLess;
	    break;
	}
    pp = pfParseNew(tt, tok, parent, scope);
    left->parent = pp;
    tok = tok->next;
    right = parseSum(pp, &tok, scope);
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseLogAnd(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse logical and. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseCmp(parent, &tok, scope);
if (tok->type == pftLogAnd)
    {
    struct pfParse *left = pp, *right;
    pp = pfParseNew(pptLogAnd, tok, parent, scope);
    left->parent = pp;
    tok = tok->next;
    right = parseCmp(pp, &tok, scope);
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseLogOr(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse logical and. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseLogAnd(parent, &tok, scope);
if (tok->type == pftLogOr)
    {
    struct pfParse *left = pp, *right;
    pp = pfParseNew(pptLogOr, tok, parent, scope);
    left->parent = pp;
    tok = tok->next;
    right = parseLogAnd(pp, &tok, scope);
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseAssign(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse '=' separated expression */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseLogOr(parent, &tok, scope);
struct pfParse *assign = NULL;
enum pfParseType type = pptNone;

switch (tok->type)
    {
    case '=':
        type = pptAssignment;
	break;
    case pftPlusEquals:
	type = pptPlusEquals;
	break;
    case pftMinusEquals:
	type = pptMinusEquals;
	break;
    case pftMulEquals:
	type = pptMulEquals;
	break;
    case pftDivEquals:
	type = pptDivEquals;
	break;
    case pftModEquals:
	type = pptModEquals;
	break;
    case ':':
        type = pptKeyVal;
	break;
    }
if (type != pptNone)
    {
    assign = pfParseNew(type, tok, parent, scope);
    assign->children = pp;
    pp->parent = assign;
    for (;;)
	{
	tok = tok->next;
	pp = parseLogOr(assign, &tok, scope);
	slAddHead(&assign->children, pp);
	if (tok->type != '=' || type != pptAssignment)
	    break;
	}
    }
*pTokList = tok;
if (assign != NULL)
    {
    slReverse(&assign->children);
    return assign;
    }
else
    return pp;
}

static void flipNameUseToVarDec(struct pfParse *pp, struct pfParse *type, struct pfParse *parent)
{
struct pfParse *dupeType, *dupeName;


AllocVar(dupeName);
*dupeName = *pp;

AllocVar(dupeType);
*dupeType = *type;
pp->children = dupeType;
dupeType->next = dupeName;
dupeName->next = NULL;
pp->type = pptVarDec;
}

static void addMissingTypesInDeclareTuple(struct pfParse *tuple)
/* If first element of tuple is typed, then make sure rest
 * is typed too. */
{
struct pfParse *vars = tuple->children, *next;
if (vars != NULL)
    {
    if (vars->type == pptVarDec || 
        vars->type == pptAssignment && vars->children->type == pptVarDec)
	{
	struct pfParse *type = NULL;
	struct pfParse *pp;
	for (pp = vars; pp != NULL; pp = pp->next)
	    {
	    if (pp->type == pptVarDec)
		{
	        type = pp->children;
		pp->name = type->next->name;
		}
	    else if (pp->type == pptNameUse)
	        {
		flipNameUseToVarDec(pp, type, tuple);
		}
	    else if (pp->type == pptAssignment)
	        {
		struct pfParse *sub = pp->children;
		if (sub->type == pptVarDec)
		    {
		    type = pp->children->children;
		    sub->name = type->next->name;
		    }
		else if (sub->type == pptNameUse)
		    {
		    flipNameUseToVarDec(sub, type, pp);
		    }
		}
	    }
	}
    }
}


struct pfParse *parseTuple(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse , separated expression. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseAssign(parent, &tok, scope);
struct pfParse *tuple = NULL;
if (tok->type == ',')
    {
    tuple = pfParseNew(pptTuple, tok, parent, scope);
    tuple->children = pp;
    pp->parent = tuple;
    while (tok->type == ',')
	{
	tok = tok->next;
	pp = parseAssign(tuple, &tok, scope);
	slAddHead(&tuple->children, pp);
	}
    }
*pTokList = tok;
if (tuple != NULL)
    {
    slReverse(&tuple->children);
    addMissingTypesInDeclareTuple(tuple);
    return tuple;
    }
else
    return pp;
}

struct pfParse *pfParseExpression(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse expression. */
{
return parseTuple(parent, pTokList, scope);
}

static struct pfParse *parseCompound(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse a compound statement (statement list surrounded
 * by brackets */
{
struct pfParse *pp;
struct pfToken *tok = *pTokList;
struct pfParse *statement;
if (tok->type != '{')
    expectingGot("{", tok);
tok->val.scope->parent = scope;
scope = tok->val.scope;
pp = pfParseNew(pptCompound, *pTokList, parent, scope);
tok = tok->next;
for (;;)
    {
    if (tok == NULL)
	{
	errAt(*pTokList, "End of file in compound statement");
	}
    if (tok->type == '}')
        {
	tok = tok->next;
	break;
	}
    statement = pfParseStatement(pp, &tok, scope);
    slAddHead(&pp->children, statement);
    }
*pTokList = tok;
slReverse(&pp->children);
return pp;
}

static struct pfParse *parseFunction(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope, enum pfParseType type)
/* Parse something (...) [into (...)] */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
struct pfParse *name, *input, *output = NULL, *body;
scope = pfScopeNew(scope, 0);
pp = pfParseNew(type, tok, parent, scope);
tok = tok->next;	/* Skip something (implicit in type) */
name = parseNameUse(parent, &tok, scope);
pp->name = name->name;

if (tok->type != '(')
    expectingGot("(", tok);
tok = tok->next;
if (tok->type == ')')
    {
    input = emptyTuple(pp, tok, scope);
    tok = tok->next;
    }
else
    {
    input = pfParseExpression(pp, &tok, scope);
    if (input->type != pptTuple)
	input = singleTuple(pp, tok, input);
    if (tok->type != ')')
	expectingGot(")", tok);
    tok = tok->next;
    }
if (tok->type == pftInto)
    {
    tok = tok->next;
    output = pfParseExpression(pp, &tok, scope);
    if (output->type != pptTuple)
	output = singleTuple(pp, tok, output);
    }
else
    output = emptyTuple(pp, tok, scope);
body = parseCompound(pp, &tok, scope);

input->type  = pptTypeTuple;
output->type  = pptTypeTuple;

slAddHead(&pp->children, body);
slAddHead(&pp->children, output);
slAddHead(&pp->children, input);
slAddHead(&pp->children, name);
*pTokList = tok;
return pp;
}

static struct pfParse *parseTo(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse to (...) [into (...)] */
{
return parseFunction(parent, pTokList, scope, pptToDec);
}

static struct pfParse *parsePara(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse para (...) [into (...)] */
{
return parseFunction(parent, pTokList, scope, pptParaDec);
}


static struct pfParse *parseFlow(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse flow (...) [into (...)] */
{
return parseFunction(parent, pTokList, scope, pptFlowDec);
}


static struct pfParse *parseClass(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse class declaration statement. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
struct pfParse *name, *body;
pp = pfParseNew(pptClass, tok, parent, scope);
tok = tok->next;	/* Skip 'class' token */
name = parseNameUse(pp, &tok, scope);
body = parseCompound(pp, &tok, scope);
slAddHead(&pp->children, body);
slAddHead(&pp->children, name);
*pTokList = tok;
return pp;
}


static struct pfParse *parseIf(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse if statement (which may include else) */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptIf, tok, parent, scope);
struct pfParse *conditional, *trueBody, *falseBody = NULL;
tok = tok->next;	/* Skip 'if' */

/* Get conditional. */
if (tok->type != '(')
    expectingGot("(", tok);
tok = tok->next;
conditional = pfParseExpression(pp, &tok, scope);
if (tok->type != ')')
    expectingGot(")", tok);
tok = tok->next;

/* Body */
trueBody = pfParseStatement(pp, &tok, scope);

/* Else clause. */
if (tok->type == pftElse)
    {
    tok = tok->next;
    falseBody = pfParseStatement(pp, &tok, scope);
    }
*pTokList = tok;

pp->children = conditional;
conditional->next = trueBody;
trueBody->next = falseBody;
return pp;
}

static struct pfParse *parseWhile(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse if statement (which may include else) */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptWhile, tok, parent, scope);
struct pfParse *conditional, *body;

tok = tok->next;	/* Skip 'while' */

/* Get conditional. */
if (tok->type != '(')
    expectingGot("(", tok);
tok = tok->next;
conditional = pfParseExpression(pp, &tok, scope);
if (tok->type != ')')
    expectingGot(")", tok);
tok = tok->next;

/* Body */
body = pfParseStatement(pp, &tok, scope);

*pTokList = tok;

pp->children = conditional;
conditional->next = body;
return pp;
}


static struct pfParse *parseForeach(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse foreach statement */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptForeach, tok, parent, scope);
struct pfParse *element;
struct pfParse *collection;
struct pfParse *statement;

tok = tok->next;	/* Skip over 'foreach' */
element = parseDottedNames(pp, &tok, scope);
skipRequiredName("in", &tok);
collection = parseDottedNames(pp, &tok, scope);
statement = pfParseStatement(pp, &tok, scope);
slAddHead(&pp->children, statement);
slAddHead(&pp->children, collection);
slAddHead(&pp->children, element);
*pTokList = tok;
return pp;
}

static struct pfParse *parseFor(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse for statement */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
struct pfParse *init;
struct pfParse *check;
struct pfParse *advance;
struct pfParse *statement;

scope = pfScopeNew(scope, 2);
pp = pfParseNew(pptFor, tok, parent, scope);
tok = tok->next;	/* Skip over 'for' */
if (tok->type != '(')
    expectingGot("(", tok);
tok = tok->next;

/* Parse up through first semicolon */
if (tok->type == ';')
    init = emptyStatement(pp, tok, scope);
else
    init = pfParseSemilessStatement(pp, &tok, scope);
if (tok->type != ';')
    expectingGot(";", tok);
tok = tok->next;

/* Parse up through through semicolon */
if (tok->type == ';')
    check = emptyStatement(pp, tok, scope);
else
    check = pfParseExpression(pp, &tok, scope);
if (tok->type != ';')
    expectingGot(";", tok);
tok = tok->next;

/* Parse advance statment and closing ')' */
if (tok->type == ')')
    advance = emptyStatement(pp, tok, scope);
else
    advance = pfParseSemilessStatement(pp, &tok, scope);
if (tok->type != ')')
    expectingGot(")", tok);
tok = tok->next;

/* Get body of loop. */
statement = pfParseStatement(pp, &tok, scope);

/* Hang various subparts of for statement onto children list. */
slAddHead(&pp->children, statement);
slAddHead(&pp->children, advance);
slAddHead(&pp->children, check);
slAddHead(&pp->children, init);
*pTokList = tok;
return pp;
}


static struct pfParse *parseWordStatement(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope, enum pfParseType type)
/* Parse one word statement. */
{
struct pfToken *tok = *pTokList;
*pTokList = tok->next;
return pfParseNew(type, tok, parent, scope);
}

static struct pfParse *parseBreak(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse break statement */
{
return parseWordStatement(parent, pTokList, scope, pptBreak);
}

static struct pfParse *parseContinue(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse continue statement */
{
return parseWordStatement(parent, pTokList, scope, pptContinue);
}
    	
static struct pfParse *parseReturn(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse return statement */
{
return parseWordStatement(parent, pTokList, scope, pptReturn);
}

static struct pfParse *parseInto(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse into statement */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptInto, tok, parent, scope);

tok = tok->next;	/* Have covered 'into' */
pp->children = parseDottedNames(pp, &tok, scope);

*pTokList = tok;
return pp;
}

    	
struct pfParse *pfParseSemilessStatement(struct pfParse *parent, 
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse a single statement, and add results to (head) of
 * parent->childList. */
{
struct pfToken *tok = *pTokList;
struct pfParse *statement;

switch (tok->type)
    {
    case pftInto:
        statement = parseInto(parent, &tok, scope);
	break;
    case '{':
	statement = parseCompound(parent, &tok, scope);
	break;
    case ';':
	statement = emptyStatement(parent, tok, scope);
	break;
    case pftIf:
        statement = parseIf(parent, &tok, scope);
	break;
    case pftWhile:
        statement = parseWhile(parent, &tok, scope);
	break;
    case pftForeach:
	statement = parseForeach(parent, &tok, scope);
	break;
    case pftFor:
	statement = parseFor(parent, &tok, scope);
	break;
    case pftBreak:
    	statement = parseBreak(parent, &tok, scope);
	break;
    case pftContinue:
        statement = parseContinue(parent, &tok, scope);
        break;
    case pftReturn:
    	statement = parseReturn(parent, &tok, scope);
	break;
    case pftClass:
	statement = parseClass(parent, &tok, scope);
	break;
    case pftTo:
	statement = parseTo(parent, &tok, scope);
	break;
    case pftPara:
	statement = parsePara(parent, &tok, scope);
	break;
    case pftFlow:
	statement = parseFlow(parent, &tok, scope);
	break;
    case pftName:
    case '(':
	statement = pfParseExpression(parent, &tok, scope);
	break;
    default:
        expectingGot("statement", tok);
	statement = NULL;
	break;
    }
*pTokList = tok;
return statement;
}


struct pfParse *pfParseStatement(struct pfParse *parent, 
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse a statement followed by any number of semicolons. */
{
struct pfParse *statement = pfParseSemilessStatement(parent, pTokList, scope);
struct pfToken *tok = *pTokList;
while (tok->type == ';')
    tok = tok->next;
*pTokList = tok;
return statement;
}

static struct pfParse *pfParseTokens(struct pfToken *tokList, struct pfScope *scope,
	struct pfParse *program)
/* Convert token list to parsed program. */
{
struct pfToken *tok = tokList;

while (tok->type != pftEnd)
    {
    struct pfParse *statement = pfParseStatement(program, &tok, scope);
    slAddHead(&program->children, statement);
    }
slReverse(&program->children);
return program;
}


void addClasses(struct pfToken *tokList, struct pfScope *scope)
/* Add types in classes to appropriate scope.  We do this as
 * a first pass to help disambiguate the grammar. */
{
struct pfToken *tok;
for (tok = tokList; tok->type != pftEnd; tok = tok->next)
    {
    if (tok->type == '{' || tok->type == '}')
	{
        scope = tok->val.scope;
	}
    if (tok->type == pftClass)
	{
	tok = tok->next;
	if (tok->type != pftName)
	    expectingGot("class name", tok);
	pfScopeAddType(scope, tok->val.s, FALSE, NULL);
	}
    }
}

struct pfParse *pfParseFile(char *fileName, struct pfCompile *pfc, 
	struct pfParse *parent)
/* Convert file to parse tree using tkz. */
{
struct pfTokenizer *tkz = pfc->tkz;
struct pfToken *tokList = NULL, *tok;
int endCount = 3;
struct pfScope *scope = pfScopeNew(pfc->scope, 16);
struct pfParse *modParse = pfParseNew(pptModule, NULL, parent, scope);
char *module = hashStoreName(pfc->modules, fileName);

modParse->name = module;

/* Read tokens, add scoping info, and add to list. */
while ((tok = pfTokenizerNext(tkz)) != NULL)
    {
    slAddHead(&tokList, tok);
    if (tok->type == '{')
        {
	scope = pfScopeNew(scope, 0);
	tok->val.scope = scope;
	}
    else if (tok->type == '}')
        {
	scope = scope->parent;
	tok->val.scope = scope;
	}
    }

/* Add enough ends to satisfy all look-aheads */
while (--endCount >= 0)
    {
    AllocVar(tok);
    tok->type = pftEnd;
    tok->source = tkz->source;
    tok->text = tkz->endPos-1;
    slAddHead(&tokList, tok);
    }

slReverse(&tokList);

addClasses(tokList, scope);
pfParseTokens(tokList, scope, modParse);
return modParse;
}

void collectDots(struct dyString *dy, struct pfParse *pp)
/* Make sure that pp is really a pptDots or a pptName. 
 * Output name plus any necessary dots into dy. */
{
if (pp->type == pptNameUse)
    {
    dyStringAppend(dy, pp->name);
    return;
    }
else if (pp->type == pptDot)
    {
    for (pp = pp->children; pp != NULL; pp = pp->next)
        {
	switch (pp->type)
	    {
	    case pptNameUse:
		dyStringAppend(dy, pp->name);
		if (pp->next != NULL)
		    dyStringAppend(dy, ".");
		break;
	    case pptRoot:
	        dyStringAppend(dy, "/");
		break;
	    case pptParent:
	        dyStringAppend(dy, "../");
		break;
	    case pptSys:
	    case pptUser:
	    case pptSysOrUser:
	        break;
	    }
	}
    }
else
    expectingGot("name", pp->tok);
}

void expandInto(struct pfParse *program, 
	struct pfCompile *pfc, struct pfParse *pp)
/* Go through program walking into modules. */
{
if (pp->type == pptInto)
    {
    struct dyString *dy = dyStringNew(0);
    struct hashEl *hel;
    char *module = NULL;
    collectDots(dy, pp->children);
    dyStringAppend(dy, ".pf");

    hel = hashLookup(pfc->modules, dy->string);
    if (hel != NULL)
        module = hel->name;

    if (module == NULL)
        {
	struct pfTokenizer *tkz = pfc->tkz;
	struct pfSource *oldSource = tkz->source;
	char *oldPos = tkz->pos;
	char *oldEndPos = tkz->endPos;
	struct pfParse *mod;
	tkz->source = pfSourceNew(dy->string);
	tkz->pos = tkz->source->contents;
	tkz->endPos = tkz->pos + tkz->source->contentSize;
	mod = pfParseFile(dy->string, pfc, program);
	expandInto(program, pfc, mod);
	tkz->source = oldSource;
	tkz->pos = oldPos;
	tkz->endPos = oldEndPos;
	slAddHead(&program->children, mod);
	module = hashStoreName(pfc->modules, dy->string);
	}
    pp->name = module;
    dyStringFree(&dy);
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    expandInto(program, pfc, pp);
}

void pfParseTypeSub(struct pfParse *pp, enum pfParseType oldType,
	enum pfParseType newType)
/* Convert type of pp and any children that are of oldType to newType */
{
if (pp->type == oldType)
    pp->type = newType;
for (pp = pp->children; pp != NULL; pp = pp->next)
    pfParseTypeSub(pp, oldType, newType);
}

void varDecAndAssignToVarInit(struct pfParse *pp)
/* Convert pptVarDec to pptVarInit. */
{
if (pp->type == pptVarDec)
    {
    pp->type = pptVarInit;
    pp->name = pp->children->next->name;
    }
else if (pp->type == pptAssignment)
    {
    struct pfParse *left = pp->children;
    if (left->type == pptVarDec)
        {
	struct pfParse *right = left->next;
	pp->type = pptVarInit;
	pp->children = left->children;
	pp->name = left->name;
	slAddTail(&pp->children, right);
	}
    }
if (pp->type == pptVarInit)
    {
    struct pfParse *type = pp->children;
    struct pfParse *name = type->next;
    pfParseTypeSub(type, pptNameUse, pptTypeName);
    name->type = pptSymName;
    }
/* Note in this case we *don't* want depth first, which
 * effectively is why this has to be done in a second pass. */
for (pp = pp->children; pp != NULL; pp = pp->next)
    varDecAndAssignToVarInit(pp);
}


struct pfParse *pfParseProgram(char *fileName, struct pfCompile *pfc)
/* Return parse tree of file and any files gone into. Variables are
 * not bound yet. */
{
struct pfParse *program = pfParseNew(pptProgram, NULL, NULL, pfc->scope);
struct pfParse *pp = pfParseFile(fileName, pfc, program);

/* Go into other modules, and put initial module at end of list. */
expandInto(program, pfc, pp);
slAddHead(&program->children, pp);
slReverse(&program->children);

/* Tidy up the parse tree a bit. */
varDecAndAssignToVarInit(program);
return program;
}

