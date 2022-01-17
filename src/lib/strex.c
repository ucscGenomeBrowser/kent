/* strex - implementation of STRing EXPression language,  currently used in tabToTabDir
 * to describe how the output fields are filled in from input fields. 
 *
 * This is a little language implemented as a recursive descent parser that creates a
 * parse tree that can be quickly evaluated to produce a string output given an input
 * set of symbols.  It is up to the evaluator to make up a symbol table, but an example of
 * using a hash as a symbol table is available.
 *
 * The language handles variables, string constants, numerical constants, and
 * a set of built in predefined functions.   The numerical constants are only used for
 * array indexes and some of the built in functions.  All functions return string values, and
 * all variables have string values.
 *
 * You can build up strings with the '+' operator, which concatenates two strings.
 * String constants can be either " or ' delimited
 *
 * You can parse apart strings with the built in functions and with the subscript [] operator.
 * The built in functions are described in a separate doc,  which will hopefully still be
 * available in the same directory as this file as strex.doc when you read this.
 */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "sqlNum.h"
#include "localmem.h"
#include "csv.h"
#include "tokenizer.h"
#include "hmac.h"
#include "errAbort.h"
#include "strex.h"


enum strexType
/* A type within parse tree or built in function specification. */
    {
    strexTypeBoolean = 1,
    strexTypeString = 2,
    strexTypeInt = 3,
    strexTypeDouble = 4,
    };

enum strexBuiltInFunc
/* One of these for each builtIn.  We'll just do a switch to implement.
 * Each built in function needs a value here, to keep it simple there's
 * aa correspondence between these names and the built in function name */
    {
    strexBuiltInTrim,
    strexBuiltInBetween,
    strexBuiltInWord,
    strexBuiltInNow,
    strexBuiltInMd5,
    strexBuiltInChop,
    strexBuiltInUncsv,
    strexBuiltInUntsv,
    strexBuiltInReplace,
    strexBuiltInFix,
    strexBuiltInLookup,
    strexBuiltInStrip,
    strexBuiltInLen,
    strexBuiltInSymbol,
    strexBuiltInSymbolId,
    strexBuiltInLower,
    strexBuiltInUpper,
    strexBuiltInIn, 
    strexBuiltInStarts,
    strexBuiltInEnds,
    strexBuiltInSame,
    strexBuiltInTidy,
    strexBuiltInWarn,
    strexBuiltInError,
    strexBuiltInLetterRange,
    strexBuiltInWordRange,
    strexBuiltInChopRange,
    };

struct strexBuiltIn
/* Information to describe a built in function */
    {
    char *name;		/* Name in strex language:  trim, split, etc */
    enum strexBuiltInFunc func;  /* enum version: strexBuiltInTrim strexBuiltInChop etc. */
    enum strexType returnType;	 /* Type of return value */
    int paramCount;	/* Number of parameters, not flexible in this language! */
    enum strexType *paramTypes;  /* Array of types, one for each parameter */
    };

union strexVal
/* Some value of arbirary type that can be of any type corresponding to strexType */
    {
    boolean b;
    char *s;
    long long i;
    double x;
    struct strexBuiltIn *builtIn;
    };

struct strexEval
/* Result of evaluation of parse tree. */
    {
    enum strexType type;
    union strexVal val;
    };

enum strexOp
/* An operation in the parse tree. */
    {
    strexOpUnknown,	/* Should not occur */
    strexOpLiteral,        /* Literal string or number. */
    strexOpSymbol,	/* Symbol/variable. */

    strexOpBuiltInCall,	/* Call a built in function */
    strexOpPick,	/* Similar to built in but pick deserves it's own op. */
    strexOpConditional,	/* Conditional trinary operation */

    strexOpArrayIx,	/* An array with an index. */
    strexOpArrayRange,	/* An array with a range. */

    strexOpStrlen,	/* Length of a string */

    /* Unary minus for numbers, logical not */
    strexOpUnaryMinusInt,
    strexOpUnaryMinusDouble,
    strexOpNot,

    /* Binary operations. */
    strexOpAdd,
    strexOpSubtract,
    strexOpGt,
    strexOpLt,
    strexOpGe,
    strexOpLe,
    strexOpEq,
    strexOpNe,
    strexOpOr,
    strexOpAnd,

    /* Type conversions - possibly a few more than we actually need at the moment. */
    strexOpStringToBoolean,
    strexOpIntToBoolean,
    strexOpDoubleToBoolean,
    strexOpStringToInt,
    strexOpDoubleToInt,
    strexOpBooleanToInt,
    strexOpStringToDouble,
    strexOpBooleanToDouble,
    strexOpIntToDouble,
    strexOpBooleanToString,
    strexOpIntToString,
    strexOpDoubleToString,
    };

struct strexParse
/* A strex parse-tree node.  The tree itself is just the root node. */
    {
    struct strexParse *next;	/* Points to younger sibling if any. */
    struct strexParse *children;	/* Points to oldest child if any. */
    enum strexOp op;		/* Operation at this node. */
    enum strexType type;		/* Return type of this operation. */
    union strexVal val;		/* Return value of this operation. */
    };

struct strexIn
/* Input to the strex parser - tokenizer and a hash full of built in functions. */
    {
    struct tokenizer *tkz;  /* Get next text input from here */
    struct hash *builtInHash;  /* Hash of built in functions */
    void *symbols;	    /* NULL or pointer to a symbol table to check */
    StrexLookup lookup; /* lookup something in symbol table if we have it */
    struct hash *importHash;   /* Hash of importex expressions keyed by file name */
    };

/* Some predefined lists of parameter types */
static enum strexType oneString[] = {strexTypeString};
static enum strexType twoStrings[] = {strexTypeString, strexTypeString};
static enum strexType threeStrings[] = {strexTypeString, strexTypeString, strexTypeString};
static enum strexType stringInt[] = {strexTypeString, strexTypeInt};
static enum strexType stringStringInt[] = {strexTypeString, strexTypeString, strexTypeInt};
static enum strexType stringIntInt[] = {strexTypeString, strexTypeInt, strexTypeInt};
static enum strexType stringStringIntInt[] = {strexTypeString, strexTypeString,
    strexTypeInt, strexTypeInt};

/* There's one element here for each built in function.  There's also a few switches you'll need to
 * fill in if you add a new built in function. */
static struct strexBuiltIn builtins[] = {
    { "trim", strexBuiltInTrim, strexTypeString, 1, oneString, },
    { "between", strexBuiltInBetween, strexTypeString, 3, threeStrings },
    { "word", strexBuiltInWord, strexTypeString, 2, stringInt },
    { "now", strexBuiltInNow, strexTypeString, 0, NULL },
    { "md5", strexBuiltInMd5, strexTypeString, 1, oneString },
    { "chop", strexBuiltInChop, strexTypeString, 3, stringStringInt },
    { "uncsv", strexBuiltInUncsv, strexTypeString, 2, stringInt },
    { "untsv", strexBuiltInUntsv, strexTypeString, 2, stringInt },
    { "replace", strexBuiltInReplace, strexTypeString, 3, threeStrings },
    { "fix", strexBuiltInFix, strexTypeString, 3, threeStrings },
    { "lookup", strexBuiltInLookup, strexTypeString, 2, twoStrings},
    { "strip", strexBuiltInStrip, strexTypeString, 2, twoStrings },
    { "len", strexBuiltInLen, strexTypeInt, 1, oneString},
    { "symbol", strexBuiltInSymbol, strexTypeString, 2, twoStrings },
    { "symbol_id", strexBuiltInSymbolId, strexTypeString, 2, twoStrings },
    { "upper", strexBuiltInUpper, strexTypeString, 1, oneString },
    { "lower", strexBuiltInLower, strexTypeString, 1, oneString },
    { "in", strexBuiltInIn, strexTypeBoolean, 2, twoStrings },
    { "starts_with", strexBuiltInStarts, strexTypeBoolean, 2, twoStrings}, 
    { "ends_with", strexBuiltInEnds, strexTypeBoolean, 2, twoStrings}, 
    { "same", strexBuiltInSame, strexTypeBoolean, 2, twoStrings}, 
    { "tidy", strexBuiltInTidy, strexTypeString, 3, threeStrings },
    { "warn", strexBuiltInWarn, strexTypeString, 1, oneString},
    { "error", strexBuiltInError, strexTypeString, 1, oneString},
    { "letter_range", strexBuiltInLetterRange, strexTypeString, 3, stringIntInt},
    { "word_range", strexBuiltInWordRange, strexTypeString, 3, stringIntInt},
    { "chop_range", strexBuiltInChopRange, strexTypeString, 4, stringStringIntInt},
};

static struct hash *hashBuiltIns()
/* Build a hash of builtins keyed by name */
{
struct hash *hash = hashNew(0);
int i;
for (i=0; i<ArraySize(builtins); ++i)
    hashAdd(hash, builtins[i].name, &builtins[i]);
return hash;
}

static struct strexIn *strexInNew(struct lineFile *lf,
    void *symbols, StrexLookup lookup)
/* Return a new strexIn structure wrapped around lineFile */
{
struct tokenizer *tkz = tokenizerOnLineFile(lf);
tkz->leaveQuotes = TRUE;
tkz->twoCharOps = TRUE;
struct strexIn *si;
AllocVar(si);
si->tkz = tkz;
si->builtInHash = hashBuiltIns();
si->importHash = hashNew(4);
si->symbols = symbols;
si->lookup = lookup;
return si;
}

static void strexInFree(struct strexIn **pSi)
/* Free up memory associated with strexIn structure */
{
struct strexIn *si = *pSi;
if (si != NULL)
    {
    hashFree(&si->builtInHash);
    hashFree(&si->importHash);
    tokenizerFree(&si->tkz);
    freez(pSi);
    }
}

struct strexParse *strexParseNew(enum strexOp op, enum strexType type)
/* Return a fresh strexParse of the given op and type with the val set to 0/NULL */
{
struct strexParse *p;
AllocVar(p);
p->op = op;
p->type = type;
return p;
}

static void strexParseValDump(struct strexParse *p, FILE *f)
/* Dump out value to file. */
{
union strexVal val = p->val;
switch (p->op)
    {
    case strexOpBuiltInCall:
        fprintf(f, "%s", val.builtIn->name);
	return;
    default:
        break;
    }

switch (p->type)
    {
    case strexTypeBoolean:
        fprintf(f, "%s", (val.b ? "true" : "false") );
	break;
    case strexTypeString:
        fprintf(f, "%s", (val.s == NULL ? "(null)" : val.s));
	break;
    case strexTypeInt:
        fprintf(f, "%lld", val.i);
	break;
    case strexTypeDouble:
        fprintf(f, "%f", val.x);
	break;
    }
}

static char *strexTypeToString(enum strexType type)
/* Return a string representation of type */
{
switch (type)
    {
    case strexTypeBoolean:
	return "boolean";
	break;
    case strexTypeString:
	return "string";
	break;
    case strexTypeInt:
	return "integer";
	break;
    case strexTypeDouble:
	return "floating point";
	break;
    default:
        internalErr();
	return NULL;
    }
}

static char *strexOpToString(enum strexOp op)
/* Return string representation of parse op. */
{
switch (op)
    {
    case strexOpLiteral:
	return "strexOpLiteral";
    case strexOpSymbol:
	return "strexOpSymbol";
    
    case strexOpStringToBoolean:
        return "strexOpStringToBoolean";
    case strexOpIntToBoolean:
        return "strexOpIntToBoolean";
    case strexOpDoubleToBoolean:
        return "strexOpDoubleToBoolean";
    case strexOpStringToInt:
        return "strexOpStringToInt";
    case strexOpDoubleToInt:
        return "strexOpDoubleToInt";
    case strexOpBooleanToInt:
        return "strexOpBooleanToInt";
    case strexOpStringToDouble:
        return "strexOpStringToDouble";
    case strexOpBooleanToDouble:
        return "strexOpBooleanToDouble";
    case strexOpIntToDouble:
        return "strexOpIntToDouble";
    case strexOpBooleanToString:
        return "strexOpBooleanToString";
    case strexOpIntToString:
        return "strexOpIntToString";
    case strexOpDoubleToString:
        return "strexOpDoubleToString";

    case strexOpUnaryMinusInt:
        return "strexOpUnaryMinusInt";
    case strexOpUnaryMinusDouble:
        return "strexOpUnaryMinusDouble";

    case strexOpAdd:
	return "strexOpAdd";
    case strexOpSubtract:
        return "strexOpSubtract";
    case strexOpEq:
        return "strexOpEq";
    case strexOpNe:
        return "strexOpNe";
    case strexOpGt:
        return "strexOpGt";
    case strexOpLt:
        return "strexOpLt";
    case strexOpGe:
        return "strexOpGe";
    case strexOpLe:
        return "strexOpLe";

    case strexOpOr:
	return "strexOpOr";
    case strexOpAnd:
	return "strexOpAnd";

    case strexOpBuiltInCall:
        return "strexOpBuiltInCall";
    case strexOpPick:
        return "strexOpPick";
    case strexOpConditional:
        return "strexOpConditional";

    case strexOpArrayIx:
        return "strexOpArrayIx";
    case strexOpArrayRange:
        return "strexOpArrayRange";

    case strexOpStrlen:
        return "strexOpStrlen";
    default:
	return "strexOpUnknown";
    }
}

void strexParseDump(struct strexParse *p, int depth, FILE *f)
/* Dump out strexParse tree and children. */
{
spaceOut(f, 3*depth);
fprintf(f, "%s ", strexOpToString(p->op));
strexParseValDump(p,  f);
fprintf(f, "\n");
struct strexParse *child;
for (child = p->children; child != NULL; child= child->next)
    strexParseDump(child, depth+1, f);
}

static void expectingGot(struct strexIn *in, char *expecting, char *got)
/* Print out error message about unexpected input. */
{
errAbort("Expecting %s, got %s, line %d of %s", expecting, got, in->tkz->lf->lineIx,
	in->tkz->lf->fileName);
}

static void strexParseFree(struct strexParse **pParse)
/* Free up memory resources associeated with a strexParse */
{
struct strexParse *p = *pParse;
if (p != NULL)
    {
    if (p->type == strexTypeString)
	freeMem(p->val.s);
    freez(pParse);
    }
}

static void skipOverRequired(struct strexIn *in, char *expecting)
/* Make sure that next token is tok, and skip over it. */
{
tokenizerMustHaveNext(in->tkz);
if (!sameWord(in->tkz->string, expecting))
    expectingGot(in, expecting, in->tkz->string);
}


static struct strexParse *strexParseExpression(struct strexIn *in);
/* Parse out an expression with a single value */

static struct strexParse *strexParseOr(struct strexIn *in);
/* Parse out logical/string or binary operator. Lowest level logical op, before conditional */

static struct strexParse *strexParseAtom(struct strexIn *in)
/* Return low level (symbol or literal) or a parenthesis enclosed expression. */
{
struct tokenizer *tkz = in->tkz;
char *tok = tokenizerMustHaveNext(tkz);
struct strexParse *p;
AllocVar(p);
char c = tok[0];
if (c == '\'' || c == '"')
    {
    p->op = strexOpLiteral;
    p->type = strexTypeString;
    int len = strlen(tok+1);
    p->val.s = cloneStringZ(tok+1, len-1);
    }
else if (isalpha(c) || c == '_')
    {
    p->op = strexOpSymbol;
    p->type = strexTypeString;	/* String until promoted at least. */
    struct dyString *dy = dyStringNew(64);
    for (;;)  // Join together . separated things into single symbol */
	{
	dyStringAppend(dy, tok);
	if ((tok = tokenizerNext(tkz)) == NULL)
	    break;
	if (tok[0] != '.')
	    {
	    tokenizerReuse(tkz);
	    break;
	    }
	dyStringAppend(dy, tok);
	if ((tok = tokenizerNext(tkz)) == NULL)
	    break;
	}
    p->val.s = dyStringCannibalize(&dy);
    // We look ahead a little to see if it's a function - hmm
    tok = tokenizerNext(tkz);
    boolean isFunct = (tok != NULL && tok[0] == '(');
    tokenizerReuse(tkz);
    if (!isFunct && in->symbols != NULL && in->lookup(in->symbols, p->val.s) == NULL )
	{
	errAbort("No field %s exists line %d of %s", p->val.s, 
	    in->tkz->lf->lineIx, in->tkz->lf->fileName);
	}
    }
else if (isdigit(c))
    {
    p->op = strexOpLiteral;
    p->type = strexTypeInt;
    p->val.i = sqlUnsigned(tok);
    if ((tok = tokenizerNext(tkz)) != NULL)
	{
	if (tok[0] == '.')
	    {
	    char buf[32];
	    tok = tokenizerMustHaveNext(tkz);
	    safef(buf, sizeof(buf), "%lld.%s", p->val.i, tok);
	    p->type = strexTypeDouble;
	    p->val.x = sqlDouble(buf);
	    }
	else
	    tokenizerReuse(tkz);
	}
    }
else if (c == '(')
    {
    p = strexParseExpression(in);
    skipOverRequired(in, ")");
    }
else
    {
    errAbort("Unexpected %s line %d of %s", tok, tkz->lf->lineIx, tkz->lf->fileName);
    }
return p;
}

static enum strexType commonTypeForMathBop(enum strexType left, enum strexType right)
/* Return type that will work for a math binary operation. */
{
if (left == right)
    return left;
else if (left == strexTypeString || right == strexTypeString)
    return strexTypeString;
else if (left == strexTypeDouble || right == strexTypeDouble)
    return strexTypeDouble;
else if (left == strexTypeInt || right == strexTypeInt)
    return strexTypeInt;
else if (left == strexTypeBoolean || right == strexTypeBoolean)
    return strexTypeBoolean;
else
    {
    errAbort("Can't find commonTypeForBop");
    return strexTypeString;
    }
}

static enum strexType commonTypeForLogicBop(enum strexType left, enum strexType right)
/* Return type that will work for a boolean binary operation. */
{
if (left == right)
    return left;
else
    return strexTypeBoolean;
}

static enum strexOp booleanCastOp(enum strexType oldType)
/* Return op to convert oldType to boolean. */
{
switch (oldType)
    {
    case strexTypeString:
        return strexOpStringToBoolean;
    case strexTypeInt:
        return strexOpIntToBoolean;
    case strexTypeDouble:
        return strexOpDoubleToBoolean;
    default:
        internalErr();
	return strexOpUnknown;
    }
}

static enum strexOp intCastOp(enum strexType oldType)
/* Return op to convert oldType to int. */
{
switch (oldType)
    {
    case strexTypeString:
        return strexOpStringToInt;
    case strexTypeBoolean:
        return strexOpBooleanToInt;
    case strexTypeDouble:
        return strexOpDoubleToInt;
    default:
        internalErr();
	return strexOpUnknown;
    }
}

static enum strexOp doubleCastOp(enum strexType oldType)
/* Return op to convert oldType to double. */
{
switch (oldType)
    {
    case strexTypeString:
        return strexOpStringToDouble;
    case strexTypeBoolean:
        return strexOpBooleanToDouble;
    case strexTypeInt:
        return strexOpIntToDouble;
    default:
        internalErr();
	return strexOpUnknown;
    }
}

static enum strexOp stringCastOp(enum strexType oldType)
/* Return op to convert oldType to double. */
{
switch (oldType)
    {
    case strexTypeDouble:
        return strexOpDoubleToString;
    case strexTypeBoolean:
        return strexOpBooleanToString;
    case strexTypeInt:
        return strexOpIntToString;
    default:
        internalErr();
	return strexOpUnknown;
    }
}



static struct strexParse *strexParseCoerce(struct strexParse *p, enum strexType type)
/* If p is not of correct type, wrap type conversion node around it. */
{
if (p->type == type)
    return p;
else
    {
    struct strexParse *cast;
    AllocVar(cast);
    cast->children = p;
    cast->type = type;
    switch (type)
        {
	case strexTypeString:
	    cast->op = stringCastOp(p->type);
	    break;
	case strexTypeBoolean:
	    cast->op = booleanCastOp(p->type);
	    break;
	case strexTypeInt:
	    cast->op = intCastOp(p->type);
	    break;
	case strexTypeDouble:
	    cast->op = doubleCastOp(p->type);
	    break;
	default:
	    internalErr();
	    break;
	}
    return cast;
    }
}

static union strexVal strexValEmptyForType(enum strexType type)
/* Create default empty/zero value - from empty string to 0.0 */
{
union strexVal ret;
switch (type)
    {
    case strexTypeBoolean:
	ret.b = FALSE;
	break;
    case strexTypeString:
        ret.s = "";
	break;
    case strexTypeInt:
        ret.i = 0;
	break;
    case strexTypeDouble:
        ret.x = 0;
	break;
    }
return ret;
}

#ifdef OLD
static struct strexEval nullValForType(enum strexType type)
/* Return 0, "", 0.0 depending */
{
struct strexEval res = {.type=type, .val=strexValEmptyForType(type)};
return res;
}
#endif /* OLD */

static struct strexParse *strexParseFunction(struct strexIn *in)
/* Handle the (a,b,c) in funcCall(a,b,c).  Convert it into tree:
*         strexOpBuiltInCall
*            strexParse(a)
*            strexParse(b)
*            strexParse(c)
* or something like that.  With no parameters 
*        strexOpBuiltInCall */
{
struct tokenizer *tkz = in->tkz;
struct strexParse *function = strexParseAtom(in);
char *tok = tokenizerNext(tkz);
if (tok == NULL)
    tokenizerReuse(tkz);
else if (tok[0] == '(')
    {
    /* Check that the current op, is a pure symbol. */
    if (function->op != strexOpSymbol)
        errAbort("Unexpected '(' line %d of %s", tkz->lf->lineIx, tkz->lf->fileName);

    /* Look up function to call and complain if it doesn't exist */
    char *functionName = function->val.s;

    /* Deal with special named ops like pick */
    if (sameString(functionName, "pick"))
        {
	/* Yay, the pick operation.  It looks like
	 *    pick( keyExp,  key1, val1, key2, val2, ..., keyN, valN)
	 * the logic is to evaluate keyExp, and then pick one of the valN's to return,
	 * the one where the keyN is the same as keyExp */
	struct strexParse *keyExp = strexParseOr(in);
	skipOverRequired(in, "?");

	/* Loop through parsing key/val pairs.  We'll save away the first value
	 * and the default value if we see one. */
	struct strexParse *firstVal = NULL;
	struct strexParse *defaultVal = NULL;
	for (;;)
	    {
	    /* Peek at next token to see if it is "default" */
	    char *tok = tokenizerNext(tkz);
	    boolean isDefault = FALSE;
	    struct strexParse *val;
	    if (sameString(tok, "default"))
	        {
		if (defaultVal != NULL)
		    {
		    errAbort("multiple defaults in pick, line %d of %s",
		        tkz->lf->lineIx, tkz->lf->fileName);
		    }
		skipOverRequired(in, ":");
		defaultVal = val = strexParseExpression(in);
		isDefault = TRUE;
		}
	    else
		{
		/* Nope not default, back up input stream and evaluate key expression */
		tokenizerReuse(tkz);
		struct strexParse *key = strexParseCoerce(strexParseExpression(in), keyExp->type);
		slAddHead(&function->children, key);
		skipOverRequired(in, ":");
		val = strexParseExpression(in);
		}

	    /* Keep track of overall expression type by storing the first value we see and
	     * making sure subsequent types agree with first one. */
	    if (firstVal == NULL)
		firstVal = val;
	    else
		{
		if (firstVal->type != val->type)
		    {
		    errAbort("Mixed value types %s and %s in pick() expression line %d of %s",
			strexTypeToString(firstVal->type), strexTypeToString(val->type),
			tkz->lf->lineIx, tkz->lf->fileName);
		    }
		val = strexParseCoerce(val, firstVal->type);
		}
	    if (!isDefault)
		slAddHead(&function->children, val);

	    tok = tokenizerMustHaveNext(tkz);
	    if (tok[0] == ')')
		break;
	    else if (tok[0] != ',')
		errAbort("Error in pick parameter list line %d of %s", 
		    tkz->lf->lineIx, tkz->lf->fileName);
	    }
	slReverse(&function->children);

	/* Now deal with adding default value as first child */
	if (defaultVal == NULL)
	    {
	    /* Need to make up empty default */
	    defaultVal = strexParseNew(strexOpLiteral, firstVal->type);
	    defaultVal->val = strexValEmptyForType(firstVal->type);
	    }
	slAddHead(&function->children, defaultVal);

	/* Finally put the key expression at the very head */
	slAddHead(&function->children, keyExp);

	/* Going to reuse current op, turn it into pick */
	function->op = strexOpPick;
	function->type = firstVal->type;
	}
    else if sameString(functionName, "import")
        {
	/* We save away the current op for now.  The function variable is where we'll
	 * return the expression we import. */
	struct strexParse *importer = function;
	function = NULL;

	/* Parse out the file name.  We'll insist it's a constant string */
	struct strexParse *fileExp = strexParseAtom(in);
	if (fileExp->op != strexOpLiteral || fileExp->type != strexTypeString)
	   errAbort("Paramater to import needs to be a quoted file name line %d of %s",
	       tkz->lf->lineIx,  tkz->lf->fileName);
	char *fileName = fileExp->val.s;
	
	/* Look up imported parse tree in hash,  reading it from file if need be. */
	function = hashFindVal(in->importHash, fileName);
	if (function == NULL)
	    {
	    function = strexParseFile(fileName, in->symbols, in->lookup);
	    hashAdd(in->importHash, fileName, function);
	    }

	skipOverRequired(in, ")");

	/* Clean up */
	strexParseFree(&fileExp);
	strexParseFree(&importer);
	}
    else
	{
	/* It's a builtin function as opposed to a special op.  Figure out which one.*/
	struct strexBuiltIn *builtIn = hashFindVal(in->builtInHash, functionName);
	if (builtIn == NULL)
	    errAbort("No built in function %s exists line %d of %s", functionName, tkz->lf->lineIx,
		tkz->lf->fileName);

	/* We're going to reuse this current op */
	function->op = strexOpBuiltInCall;
	function->type = builtIn->returnType;
	function->val.builtIn = builtIn;

	tok = tokenizerMustHaveNext(tkz);
	if (tok[0] != ')')
	    {
	    tokenizerReuse(tkz);
	    for (;;)
		{
		struct strexParse *param = strexParseExpression(in);
		slAddHead(&function->children, param);
		tok = tokenizerMustHaveNext(tkz);
		if (tok[0] == ')')
		    break;
		else if (tok[0] != ',')
		    errAbort("Error in parameter list for %s line %d of %s", function->val.s, 
			tkz->lf->lineIx, tkz->lf->fileName);
		}
	    slReverse(&function->children);
	    }

	/* Check function parameter count */
	int childCount = slCount(function->children);
	if (childCount != builtIn->paramCount)
	    errAbort("Function %s has %d parameters but needs %d line %d of %s",
		builtIn->name, childCount, builtIn->paramCount, tkz->lf->lineIx, tkz->lf->fileName);
		
	/* Check function parameter types */
	int i;
	struct strexParse *p;
	for (i=0, p=function->children; i<childCount; ++i, p = p->next)
	    {
	    if (p->type != builtIn->paramTypes[i])
		{
		errAbort("Parameter #%d to %s needs to be type %s not %s line %d of %s",
		    i, builtIn->name,  strexTypeToString(builtIn->paramTypes[i]), 
		    strexTypeToString(p->type), tkz->lf->lineIx, tkz->lf->fileName);
		}
	    }
	}
    }
else
    tokenizerReuse(tkz);
return function;
}

struct strexParse *arrayRangeTree(struct strexParse *array, 
    struct strexParse *firstIndex, struct strexParse *secondIndex)
/* Creat an array range parse tree */
{
struct strexParse *p = strexParseNew(strexOpArrayRange, strexTypeString);
p->children = array;
array->next = firstIndex;
firstIndex->next = secondIndex;
return p;
}

static struct strexParse *strexParseIndex(struct strexIn *in)
/* Handle the [] in this[6].  Convert it into tree:
*         strexOpArrayIx
*            strexParseFunction
*            strexParseFunction */
{
struct tokenizer *tkz = in->tkz;
struct strexParse *array = strexParseFunction(in);
struct strexParse *p = array;
char *tok = tokenizerNext(tkz);
if (tok == NULL)
    tokenizerReuse(tkz);
else if (tok[0] == '[')
    {
    array = strexParseCoerce(array, strexTypeString);
    tok = tokenizerMustHaveNext(tkz);
    if (tok[0] == ':')  // Case where is a range with empty beginning.  We can even compute index.
        {
	tok = tokenizerMustHaveNext(tkz);
	if (tok[0] == ']')
	    {
	    tokenizerReuse(tkz);    // Range is just whole array, do nothing really
	    }
	else
	    {
	    tokenizerReuse(tkz);    
	    struct strexParse *firstIndex = strexParseNew(strexOpLiteral, strexTypeInt);
	    struct strexParse *secondIndex = strexParseCoerce(strexParseExpression(in), strexTypeInt);
	    p = arrayRangeTree(array, firstIndex, secondIndex);
	    }
        }
    else
	{
	tokenizerReuse(tkz);
	struct strexParse *firstIndex = strexParseCoerce(strexParseExpression(in), strexTypeInt);
	tok = tokenizerMustHaveNext(tkz);
	if (tok[0] == ':')
	    {
	    struct strexParse *secondIndex;
	    tok = tokenizerMustHaveNext(tkz);
	    if (tok[0] == ']')  // Case where second half of range is empty
		{
	        tokenizerReuse(tkz);
		secondIndex = strexParseNew(strexOpStrlen, strexTypeInt);
		secondIndex->children = array;
		}
	    else
	        {
	        tokenizerReuse(tkz);
		secondIndex = strexParseCoerce(strexParseExpression(in), strexTypeInt);
		}
	    p = arrayRangeTree(array, firstIndex, secondIndex);
	    }
	else
	    {
	    // Simple no range case
	    tokenizerReuse(tkz);
	    p = strexParseNew(strexOpArrayIx, strexTypeString);
	    p->children = array;
	    array->next = firstIndex;
	    }
	}
    skipOverRequired(in, "]");
    }
else
    tokenizerReuse(tkz);
return p;
}


static struct strexParse *strexParseUnaryNeg(struct strexIn *in)
/* Return parse tree for unary minus or logical not */
{
struct tokenizer *tkz = in->tkz;
char *tok = tokenizerMustHaveNext(tkz);
if (tok[0] == '-')
    {
    struct strexParse *c = strexParseIndex(in);
    struct strexParse *p;
    AllocVar(p);
    if (c->type == strexTypeInt)
        {
	p->op = strexOpUnaryMinusInt;
	p->type = strexTypeInt;
	}
    else
	{
	c = strexParseCoerce(c, strexTypeDouble);
	p->op = strexOpUnaryMinusDouble;
	p->type = strexTypeDouble;
	}
    p->children = c;
    return p;
    }
else if (sameString(tok, "not"))
    {
    struct strexParse *c = strexParseIndex(in);
    struct strexParse *p;
    AllocVar(p);
    if (c->type == strexTypeBoolean)
        {
	p->op = strexOpNot;
	}
    else
	{
	c = strexParseCoerce(c, strexTypeBoolean);
	p->op = strexOpNot;
	}
    p->type = strexTypeBoolean;
    p->children = c;
    return p;
    }
else
    {
    tokenizerReuse(tkz);
    return strexParseIndex(in);
    }
}

static struct strexParse *strexParseSum(struct strexIn *in)
/* Parse out plus or minus. */
{
struct tokenizer *tkz = in->tkz;
struct strexParse *p = strexParseUnaryNeg(in);
for (;;)
    {
    char *tok = tokenizerNext(tkz);
    if (tok == NULL || (differentString(tok, "+") && differentString(tok, "-")))
	{
	tokenizerReuse(tkz);
	return p;
	}
    enum strexOp op = (tok[0] == '+' ? strexOpAdd : strexOpSubtract);

    /* What we've parsed so far becomes left side of binary op, next term ends up on right. */
    struct strexParse *l = p;
    struct strexParse *r = strexParseUnaryNeg(in);

    /* Make left and right side into a common type */
    enum strexType childType = commonTypeForMathBop(l->type, r->type);
    if (op == strexOpSubtract)
        {
	if (childType == strexTypeString)
	    {
	    errAbort("Expecting numbers around '-' sign line %d of %s",
		in->tkz->lf->lineIx, in->tkz->lf->fileName);
	    }
	}
    l = strexParseCoerce(l, childType);
    r = strexParseCoerce(r, childType);

    /* Create the binary operation */
    AllocVar(p);
    p->op = strexOpAdd;
    p->type = childType;

    /* Now hang children onto node. */
    p->children = l;
    l->next = r;
    }
}

boolean isCmpOp(char *tok)
/* Return TRUE if tok is one of t > < >= <= = and !=. */
{
char t0 = tok[0];
char t1 = tok[1];
switch (t0)
   {
   case '>':
   case '<':
        return t1 == 0 || t1 == '=';
   case '=':
        return t1 == 0;
   case '!':
        return t1 == '=';
   default:
       return FALSE;
   }
}

enum strexOp strexOpForCmpTok(char *tok)
/* Return TRUE if tok is one of t > < >= <= = and !=. */
{
if (sameString(tok, "="))
    return strexOpEq;
else if (sameString(tok, "!="))
    return strexOpNe;
else if (sameString(tok, ">"))
    return strexOpGt;
else if (sameString(tok, ">="))
    return strexOpGe;
else if (sameString(tok, "<"))
    return strexOpLt;
else if (sameString(tok, "<="))
    return strexOpLe;
else
    {
    warn("unrecognized cmp op token %s", tok);
    internalErr();
    return strexOpUnknown;
    }
}


static struct strexParse *strexParseCompareOp(struct strexIn *in)
/* Parse out > < >= <= = and !=. */
{
struct tokenizer *tkz = in->tkz;
struct strexParse *p = strexParseSum(in);
char *tok = tokenizerNext(tkz);
if (tok == NULL || !isCmpOp(tok))
    {
    tokenizerReuse(tkz);
    return p;
    }
enum strexOp cmpOp = strexOpForCmpTok(tok);

/* What we've parsed so far becomes left side of binary op, next term ends up on right. */
struct strexParse *l = p;
struct strexParse *r = strexParseSum(in);

/* Make left and right side into a common type */
enum strexType commonType = commonTypeForMathBop(l->type, r->type);
if (commonType == strexTypeBoolean)
    commonType = strexTypeInt;

l = strexParseCoerce(l, commonType);
r = strexParseCoerce(r, commonType);

/* Create the binary operation */
AllocVar(p);
p->op = cmpOp;
p->type = strexTypeBoolean;

/* Now hang children onto node and return it. */
p->children = l;
l->next = r;
return p;
}

static struct strexParse *strexParseAnd(struct strexIn *in)
/* Parse out plus or minus. */
{
struct tokenizer *tkz = in->tkz;
struct strexParse *p = strexParseCompareOp(in);
for (;;)
    {
    char *tok = tokenizerNext(tkz);
    if (tok == NULL || differentString(tok, "and"))
	{
	tokenizerReuse(tkz);
	return p;
	}

    /* What we've parsed so far becomes left side of binary op, next term ends up on right. */
    struct strexParse *l = p;
    struct strexParse *r = strexParseCompareOp(in);

    /* Make left and right side into a common type */
    enum strexType childType = commonTypeForLogicBop(l->type, r->type);
    l = strexParseCoerce(l, childType);
    r = strexParseCoerce(r, childType);

    /* Create the binary operation */
    AllocVar(p);
    p->op = strexOpAnd;
    p->type = childType;

    /* Now hang children onto node. */
    p->children = l;
    l->next = r;
    }
}

static struct strexParse *strexParseOr(struct strexIn *in)
/* Parse out logical/string or binary operator. */
{
struct tokenizer *tkz = in->tkz;
struct strexParse *p = strexParseAnd(in);
for (;;)
    {
    char *tok = tokenizerNext(tkz);
    if (tok == NULL || differentString(tok, "or"))
	{
	tokenizerReuse(tkz);
	return p;
	}

    /* What we've parsed so far becomes left side of binary op, next term ends up on right. */
    struct strexParse *l = p;
    struct strexParse *r = strexParseAnd(in);

    /* Make left and right side into a common type */
    enum strexType childType = commonTypeForLogicBop(l->type, r->type);
    l = strexParseCoerce(l, childType);
    r = strexParseCoerce(r, childType);

    /* Create the binary operation */
    AllocVar(p);
    p->op = strexOpOr;
    p->type = childType;

    /* Now hang children onto node. */
    p->children = l;
    l->next = r;
    }
}

static struct strexParse *strexParseConditional(struct strexIn *in)
/* Handle the ternary operator ?:  usually written as ( boolean ? trueExp : falseExp)
 * because of it's ridiculously low precedence.  Makes this parse tree:
 *         strexOpConditional
 *            boolean exp  (always boolean type)
 *            true exp     (same type as false exp)
 *            false exp    (same type as true exp)      */
{
struct tokenizer *tkz = in->tkz;
struct strexParse *p = strexParseOr(in);
char *tok = tokenizerNext(tkz);
if (tok == NULL)
    tokenizerReuse(tkz);
else if (tok[0] == '?')
    {
    struct strexParse *booleanExp = strexParseCoerce(p, strexTypeBoolean);
    struct strexParse *trueExp = strexParseExpression(in);
    skipOverRequired(in, ":");
    struct strexParse *falseExp = strexParseExpression(in);
    if (trueExp->type != falseExp->type)
	errAbort("Mixed value types %s and %s in conditional expression (?:) line %d of %s",
	    strexTypeToString(trueExp->type), strexTypeToString(falseExp->type),
	    tkz->lf->lineIx, tkz->lf->fileName);

    /* Make conditional expression and hook up it's three children. */
    struct strexParse *conditional = strexParseNew(strexOpConditional, trueExp->type);
    conditional->children = booleanExp;
    booleanExp->next = trueExp;
    trueExp->next = falseExp;
    p = conditional;
    }
else
    tokenizerReuse(tkz);
return p;
}


static struct strexParse *strexParseExpression(struct strexIn *in)
/* Parse out an expression. Leaves input at next expression. */
{
return strexParseConditional(in);
}

static void ensureAtEnd(struct strexIn *in)
/* Make sure that we are at end of input. */
{
struct tokenizer *tkz = in->tkz;
char *leftover = tokenizerNext(tkz);
if (leftover != NULL)
    errAbort("Extra input starting with '%s' line %d of %s", leftover, tkz->lf->lineIx,
	tkz->lf->fileName);
}

struct strexParse *strexParseString(char *s, char *fileName, int fileLineNumber,
    void *symbols, StrexLookup lookup)
/* Parse out string expression in s and return root of tree.  The fileName and
 * fileLineNumber should be filled in with where string came from.  */
{
struct lineFile *lf = lineFileOnString(fileName, TRUE, s);
lf->lineIx = fileLineNumber;
struct strexIn *si = strexInNew(lf, symbols, lookup);
struct strexParse *parseTree = strexParseExpression(si);
if (verboseLevel() >= 2)
   strexParseDump(parseTree, 1, stderr);
ensureAtEnd(si);
strexInFree(&si);
return parseTree;
}

struct strexParse *strexParseFile(char *fileName, void *symbols, StrexLookup lookup)
/* Parse string expression out of a file */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct strexIn *si = strexInNew(lf, symbols, lookup);
si->tkz->uncommentShell = TRUE;   // Yay to comments.
struct strexParse *parseTree = strexParseExpression(si);
ensureAtEnd(si);
strexInFree(&si);
return parseTree;
}

/************ The parsing section is done, now for the evaluation section. **************/

struct strexRun
/* What we need to run a strex expression evaluation */
    {
    void *symbols;	/* Pointer to symbol table */
    StrexLookup lookup; /* Something that can look up things in symbol table */
    struct lm *lm;	/* Local memory for evaluation */
    void (*warnHandler)(void *symbols, char *warning);  /* Send warning messages here */
    void (*abortHandler)(void *symbols);	    /* Call this guy to abort */
    };

static void strexDefaultAbort()
/* Default abort handler */
{
noWarnAbort();
}

static struct strexRun *strexRunNew(void *symbols, StrexLookup lookup, 
    void (*warnHandler)(void *symbols, char *warning), void (*abortHandler)(void *symbols) )
/* Return new strexRun structure */
{
struct strexRun *run;
AllocVar(run);
run->lm = lmInit(0);
run->symbols = symbols;
run->lookup = lookup;
run->warnHandler = warnHandler;
run->abortHandler = (abortHandler != NULL ? abortHandler : strexDefaultAbort);
return run;
}

static void strexRunFree(struct strexRun **pRun)
/* Free up strex run structure */
{
struct strexRun *run = *pRun;
if (run != NULL)
    {
    lmCleanup(&run->lm);
    freez(pRun);
    }
}

static struct strexEval strexLocalEval(struct strexParse *p, struct strexRun *run);
/* Evaluate self on parse tree, allocating memory if needed from lm. */


static struct strexEval strexEvalCoerceToString(struct strexEval r, char *buf, int bufSize)
/* Return a version of r with .val.s filled in with something reasonable even
 * if r input is not a string */
{
assert(bufSize >= 32);
switch (r.type)
    {
    case strexTypeBoolean:
        r.val.s = (r.val.b ? "true" : "false");
	break;
    case strexTypeString:
	break;	/* It's already done. */
    case strexTypeInt:
	safef(buf, bufSize, "%lld", r.val.i);
	r.val.s = buf;
	break;
    case strexTypeDouble:
	safef(buf, bufSize, "%g", r.val.x);
	r.val.s = buf;
	break;
    default:
	internalErr();
	r.val.s = NULL;
	break;
    }
r.type = strexTypeString;
return r;
}

static struct strexEval strexEvalSubtract(struct strexParse *p, struct strexRun *run)
/* Return a - b. */
{
struct strexParse *lp = p->children;
struct strexParse *rp = lp->next;
struct strexEval lv = strexLocalEval(lp, run);
struct strexEval rv = strexLocalEval(rp, run);
struct strexEval res;
assert(lv.type == rv.type);   // Is our type automatic casting working?
switch (lv.type)
    {
    case strexTypeInt:
	res.val.i = (lv.val.i - rv.val.i);
	break;
    case strexTypeDouble:
	res.val.x = (lv.val.x - rv.val.x);
	break;
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
res.type = lv.type;
return res;
}

static struct strexEval strexEvalAdd(struct strexParse *p, struct strexRun *run)
/* Return a + b. */
{
struct strexParse *lp = p->children;
struct strexParse *rp = lp->next;
struct strexEval lv = strexLocalEval(lp, run);
struct strexEval rv = strexLocalEval(rp, run);
struct strexEval res;
assert(lv.type == rv.type);   // Is our type automatic casting working?
switch (lv.type)
    {
    case strexTypeInt:
	res.val.i = (lv.val.i + rv.val.i);
	break;
    case strexTypeDouble:
	res.val.x = (lv.val.x + rv.val.x);
	break;
    case strexTypeString:
	{
	char numBuf[32];
	if (rv.type != strexTypeString)  // Perhaps later could coerce to string
	    {
	    rv = strexEvalCoerceToString(rv, numBuf, sizeof(numBuf));
	    }
	int lLen = strlen(lv.val.s);
	int rLen = strlen(rv.val.s);
	char *s = lmAlloc(run->lm, lLen + rLen + 1);
	memcpy(s, lv.val.s, lLen);
	memcpy(s+lLen, rv.val.s, rLen);
	res.val.s = s;
	break;
	}
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
res.type = lv.type;
return res;
}

static struct strexEval strexEvalEq(struct strexParse *p, struct strexRun *run)
/* Return a == b. */
{
struct strexParse *lp = p->children;
struct strexParse *rp = lp->next;
struct strexEval lv = strexLocalEval(lp, run);
struct strexEval rv = strexLocalEval(rp, run);
struct strexEval res;
assert(lv.type == rv.type);   // Is our type automatic casting working?
switch (lv.type)
    {
    case strexTypeInt:
	res.val.b = (lv.val.i == rv.val.i);
	break;
    case strexTypeDouble:
	res.val.b = (lv.val.x == rv.val.x);
	break;
    case strexTypeString:
	{
	res.val.b = (strcasecmp(lv.val.s, rv.val.s) == 0);
	break;
	}
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
res.type = strexTypeBoolean;
return res;
}

static struct strexEval strexEvalNe(struct strexParse *p, struct strexRun *run)
/* Return a != b. */
{
struct strexParse *lp = p->children;
struct strexParse *rp = lp->next;
struct strexEval lv = strexLocalEval(lp, run);
struct strexEval rv = strexLocalEval(rp, run);
struct strexEval res;
assert(lv.type == rv.type);   // Is our type automatic casting working?
switch (lv.type)
    {
    case strexTypeInt:
	res.val.b = (lv.val.i != rv.val.i);
	break;
    case strexTypeDouble:
	res.val.b = (lv.val.x != rv.val.x);
	break;
    case strexTypeString:
	{
	res.val.b = (strcasecmp(lv.val.s, rv.val.s) != 0);
	break;
	}
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
res.type = strexTypeBoolean;
return res;
}


static struct strexEval strexEvalGt(struct strexParse *p, struct strexRun *run)
/* Return a > b. */
{
struct strexParse *lp = p->children;
struct strexParse *rp = lp->next;
struct strexEval lv = strexLocalEval(lp, run);
struct strexEval rv = strexLocalEval(rp, run);
struct strexEval res;
assert(lv.type == rv.type);   // Is our type automatic casting working?
switch (lv.type)
    {
    case strexTypeInt:
	res.val.b = (lv.val.i > rv.val.i);
	break;
    case strexTypeDouble:
	res.val.b = (lv.val.x > rv.val.x);
	break;
    case strexTypeString:
	{
	res.val.b = (cmpWordsWithEmbeddedNumbers(lv.val.s, rv.val.s) > 0);
	break;
	}
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
res.type = strexTypeBoolean;
return res;
}

static struct strexEval strexEvalLt(struct strexParse *p, struct strexRun *run)
/* Return a < b. */
{
struct strexParse *lp = p->children;
struct strexParse *rp = lp->next;
struct strexEval lv = strexLocalEval(lp, run);
struct strexEval rv = strexLocalEval(rp, run);
struct strexEval res;
assert(lv.type == rv.type);   // Is our type automatic casting working?
switch (lv.type)
    {
    case strexTypeInt:
	res.val.b = (lv.val.i < rv.val.i);
	break;
    case strexTypeDouble:
	res.val.b = (lv.val.x < rv.val.x);
	break;
    case strexTypeString:
	{
	res.val.b = (cmpWordsWithEmbeddedNumbers(lv.val.s, rv.val.s) < 0);
	break;
	}
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
res.type = strexTypeBoolean;
return res;
}

static struct strexEval strexEvalGe(struct strexParse *p, struct strexRun *run)
/* Return a > b. */
{
struct strexParse *lp = p->children;
struct strexParse *rp = lp->next;
struct strexEval lv = strexLocalEval(lp, run);
struct strexEval rv = strexLocalEval(rp, run);
struct strexEval res;
assert(lv.type == rv.type);   // Is our type automatic casting working?
switch (lv.type)
    {
    case strexTypeInt:
	res.val.b = (lv.val.i >= rv.val.i);
	break;
    case strexTypeDouble:
	res.val.b = (lv.val.x >= rv.val.x);
	break;
    case strexTypeString:
	{
	res.val.b = (cmpWordsWithEmbeddedNumbers(lv.val.s, rv.val.s) >= 0);
	break;
	}
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
res.type = strexTypeBoolean;
return res;
}

static struct strexEval strexEvalLe(struct strexParse *p, struct strexRun *run)
/* Return a < b. */
{
struct strexParse *lp = p->children;
struct strexParse *rp = lp->next;
struct strexEval lv = strexLocalEval(lp, run);
struct strexEval rv = strexLocalEval(rp, run);
struct strexEval res;
assert(lv.type == rv.type);   // Is our type automatic casting working?
switch (lv.type)
    {
    case strexTypeInt:
	res.val.b = (lv.val.i <= rv.val.i);
	break;
    case strexTypeDouble:
	res.val.b = (lv.val.x <= rv.val.x);
	break;
    case strexTypeString:
	{
	res.val.b = (cmpWordsWithEmbeddedNumbers(lv.val.s, rv.val.s) <= 0);
	break;
	}
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
res.type = strexTypeBoolean;
return res;
}


static struct strexEval strexEvalOr(struct strexParse *p, struct strexRun *run)
/* Return a or b. */
{
struct strexParse *lp = p->children;
struct strexParse *rp = lp->next;
struct strexEval res = strexLocalEval(lp, run);
boolean gotIt = TRUE;
switch (res.type)
    {
    case strexTypeBoolean:
	gotIt = res.val.b != 0;
	break;
    case strexTypeInt:
	gotIt = (res.val.i != 0);
	break;
    case strexTypeDouble:
	gotIt = (res.val.x != 0);
	break;
    case strexTypeString:
	gotIt = (res.val.s[0] != 0);
	break;
    }
if (gotIt)
   return res;

// We didn't get it, so have evaluate the other side */
return strexLocalEval(rp, run);
}

static struct strexEval strexEvalAnd(struct strexParse *p, struct strexRun *run)
/* Return a and b. */
{
struct strexParse *lp = p->children;
struct strexParse *rp = lp->next;
struct strexEval lv = strexLocalEval(lp, run);
struct strexEval rv = strexLocalEval(rp, run);
struct strexEval res;
assert(lv.type == rv.type);   // Is our type automatic casting working?
switch (lv.type)
    {
    case strexTypeBoolean:
        res.val.b = (lv.val.b && rv.val.b);
	break;
    case strexTypeInt:
	res.val.i = (lv.val.i ? rv.val.i : lv.val.i);
	break;
    case strexTypeDouble:
	res.val.x = (lv.val.x != 0.0 ? rv.val.x : lv.val.x);
	break;
    case strexTypeString:
	res.val.s = (lv.val.s[0] ? rv.val.s : lv.val.s);
	break;
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
res.type = lv.type;
return res;
}


static char *csvParseOneOut(char *csvIn, int ix, struct dyString *scratch)
/* Return csv value of given index or NULL if at end */
{
char *pos = csvIn;
int i;
for (i=0; i<ix; ++i)
    {
    if (csvParseNext(&pos,scratch) == NULL)
        return NULL;
    }
return csvParseNext(&pos, scratch);
}

static struct strexEval strexEvalArrayIx(struct strexParse *p, struct strexRun *run)
/* Handle parse tree generated by an indexed array. */
{
struct strexParse *array = p->children;
struct strexParse *index = array->next;
struct strexEval arrayVal = strexLocalEval(array, run);
struct strexEval indexVal = strexLocalEval(index, run);
int ix = indexVal.val.i;
if (ix  < 0)
    ix = strlen(arrayVal.val.s) + ix;
struct strexEval res;
res.val.s = lmCloneStringZ(run->lm, arrayVal.val.s + ix, 1);
res.type = strexTypeString;
return res;
}

static void pythonInterpretEnds(int *pStart, int *pEnd, int len)
/* Adjusts *pStart and *pEnd from python ideas that may include negative
 * numbers back to our ideas */
{
int start = *pStart, end = *pEnd;
// Negative numbers count from end of array
if (start < 0)
    start = len + start;
if (end < 0)
    end = len + end;

// Do some clipping to keep results in range
if (start < 0)
   start = 0;
if (end > len)
    end = len;
if (end < start) end = start;

// Return results
*pStart = start;
*pEnd = end;
}

static char *joinString(char **parts, char *joiner, int start, int end, struct strexRun *run)
/* Return a string that is just all the parts concatenated together with the joiner char
 * between them */
{
int joinerSize = strlen(joiner);

/* Figure out output string size. */
int i;
int outSize = -joinerSize;  // Simplifies calcs on size to init to this
for (i=start; i<end; ++i)
    outSize += strlen(parts[i]) + joinerSize;

/* Allocate output string and copy first word in range to it.  */
char *result = lmAlloc(run->lm, outSize + 1);
char *out = result;
char *onePart = parts[start];
int oneSize = strlen(onePart);
strcpy(out, onePart);
out += oneSize;

/* Fall into loop to copy joiner/part/joiner/part/.../joiner/part */
for (i=start+1; i<end; ++i)
     {
     strcpy(out, joiner);
     out += joinerSize;
     onePart = parts[i];
     oneSize = strlen(onePart);
     strcpy(out, onePart);
     out += oneSize;
     }
return result;
}

static char *strexEvalWordRange(char *string, int start, int end, 
    struct strexRun *run)
{
int wordCount = chopByWhite(string, NULL, 0);
pythonInterpretEnds(&start, &end, wordCount);
if (start >= end || wordCount <= 0)
    return "";
else
    {
    /* We aren't super efficient here, we parse them all out and then glue
     * the ones we want back together */
    char **words;
    lmAllocArray(run->lm, words, wordCount);
    char *clone = lmCloneString(run->lm, string);
    chopByWhite(clone, words, wordCount);
    return joinString(words, " ", start, end, run);
    }
}

static char *strexEvalChopRange(char *string, char *splits, int start, int end, 
    struct strexRun *run)
{
char splitter = splits[0];
int wordCount = chopByChar(string, splitter, NULL, 0);
pythonInterpretEnds(&start, &end, wordCount);
if (start >= end || wordCount <= 0)
    return "";
else
    {
    /* We aren't super efficient here, we parse them all out and then glue
     * the ones we want back together */
    char **words;
    lmAllocArray(run->lm, words, wordCount);
    char *clone = lmCloneString(run->lm, string);
    chopByChar(clone, splitter, words, wordCount);
    char joiner[2] = {splitter, 0};
    return joinString(words, joiner, start, end, run);
    }
}


static struct strexEval strexEvalArrayStartEnd(struct strexParse *array, 
    struct strexParse *index1, struct strexParse *index2, struct strexRun *run)
/* Evaluate expression to return result of character array selection between
 * index1 and index2 */
{
struct strexEval arrayVal = strexLocalEval(array, run);
struct strexEval rangeStart = strexLocalEval(index1, run);
struct strexEval rangeEnd = strexLocalEval(index2, run);
char *arraySource = arrayVal.val.s;
int start = rangeStart.val.i;
int end = rangeEnd.val.i;
int len = strlen(arraySource); 
pythonInterpretEnds(&start, &end, len);
struct strexEval res;
res.val.s = lmCloneStringZ(run->lm, arrayVal.val.s + start, end-start);
res.type = strexTypeString;
return res;
}

static struct strexEval strexEvalArrayRange(struct strexParse *p, struct strexRun *run)
/* Handle parse tree generated by array range expression, which by now
 * has just been turned into two integer values. */
{
struct strexParse *array = p->children;
struct strexParse *index1 = array->next;
struct strexParse *index2 = index1->next;
return strexEvalArrayStartEnd(array, index1, index2, run);
}

static char *splitString(char *words,  int ix,  struct lm *lm)
/* Return the space-delimited word of index ix as clone into lm */
{
if (ix < 0)	// Negative index.  We got to count, dang
    {
    int wordCount = chopByWhite(words, NULL, 0);   
    ix = wordCount + ix;
    if (ix < 0)
        return "";
    }
char *s = words;
int i;
for (i=0; ; ++i)
    {
    s = skipLeadingSpaces(s);
    if (isEmpty(s))
	return "";
    char *end = skipToSpaces(s);
    if (i == ix)
	{
	if (end == NULL)
	    return lmCloneString(lm, s);
	else
	    return lmCloneStringZ(lm, s, end - s);
	}
    s = end;
    }
}

static char *uncsvString(char *csvIn,  int ix,  struct lm *lm)
/* Return the comma separated value of index ix. Memory for result is lm */
{
struct dyString *scratch = dyStringNew(0);
char *one = csvParseOneOut(csvIn, ix, scratch); 
char *res = emptyForNull(lmCloneString(lm, one));	// Save in more permanent memory
dyStringFree(&scratch);
return res;
}

static char *separateString(char *string, char *splitter, int ix, struct lm *lm)
/* Return the ix'th part of string as split apart by splitter */
{
int splitterSize = strlen(splitter);
if (splitterSize != 1)
    errAbort("Separator parameter to split must be a single character, not %s", splitter);
int count = chopByChar(string, splitter[0], NULL, 0);
if (ix < 0)	// Negative index.  No problem, count from the end
    {
    ix = count + ix;
    if (ix < 0)
        return "";	// Still out of range, oh well
    }
if (ix >= count)
    return "";
char **row;
lmAllocArray(lm, row, count);
char *scratch = lmCloneString(lm, string);
chopByChar(scratch, splitter[0], row, count);
return row[ix];
}

static char *untsvString(char *tsvIn, int ix, struct lm *lm)
/* Return the tab separated value at given index living somewhere in lm. */
{
return separateString(tsvIn, "\t", ix, lm);
}

static char *replaceString(char *in, char *oldVal, char *newVal, struct lm *lm)
/* Replace every occurrence of oldVal with newVal in string */
{
if (sameString(in, oldVal) )
    return lmCloneString(lm, newVal);  // Simple case that also handles empty oldVal match empty in
else
    {
    if (isEmpty(oldVal))
        return lmCloneString(lm, in);
    else
	{
	char *s = replaceChars(in, oldVal, newVal);
	char *result = lmCloneString(lm, s);  // Move to local memory
	freeMem(s);
	return result;
	}
    }
}

static char *stripAll(char *in, char *toRemove, struct lm *lm)
/* Remove every occurrence of any of the chars in toRemove from in. */
{
char *result = lmCloneString(lm, in);  // Move to local memory
char c, *s = toRemove;
while ((c = *s++) != 0)
    stripChar(result, c);
return result;
}

static char *symbolify(char *prefix, char *original, struct lm *lm)
/* Convert original to something could use as a C language symbol with dots maybe. */
{
if (isEmpty(original))
    return original;
int prefixSize = strlen(prefix);
int originalSize = strlen(original);
int allocSize = prefixSize + 2*originalSize + 1;    // worse case all hexed
char *result = lmAlloc(lm, allocSize);  // Move to local memory
strcpy(result, prefix);
char *in = skipLeadingSpaces(original); 
char *out = result + prefixSize;
unsigned char c;
while ((c = *in++) != 0)
     {
     if (isspace(c) || c == '-' || c == '.')
	 *out++ = '_';
     else if (isalnum(c) || c == '_')
         *out++ = c;
     else
         {
	 char hexBuf[8];
	 safef(hexBuf, sizeof(hexBuf), "%02X", c);
	 strcpy(out, hexBuf);
	 out += strlen(hexBuf);
	 }
     }
*out++ = 0;
int len = strlen(result) - prefixSize;
if (len > 32)
    {
    char *md5 = hmacMd5("", original);
    strcpy(result + prefixSize, md5);
    freeMem(md5);
    }

return result;
}

static char *idify(char *prefix, char *original, struct lm *lm)
/* Convert original to something could use as a prefix plus a numeric id */
{
static struct hash *prefixHash = NULL;
if (prefixHash == NULL)
    prefixHash = hashNew(0);
struct hash *idHash = hashFindVal(prefixHash, prefix);
if (idHash == NULL)
    {
    idHash = hashNew(0);
    hashAdd(prefixHash, prefix, idHash);
    }
char *id = hashFindVal(idHash, original);
if (id == NULL)
    {
    char symbuf[128];
    safef(symbuf, sizeof(symbuf), "%s%d", prefix, idHash->elCount + 1);
    id = lmCloneString(idHash->lm, symbuf);
    hashAdd(idHash, original, id);
    }
return id;
}

static struct hash *hashTwoColTsv(char *fileName)
/* Given a two column file (key, value) return a hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(16);
char *row[3];
int fields = 0;
while ((fields = lineFileChopTab(lf, row)) != 0)
    {
    lineFileExpectWords(lf, 2, fields);
    char *name = row[0];
    char *value = lmCloneString(hash->lm, row[1]);
    hashAdd(hash, name, value);
    }
lineFileClose(&lf);
return hash;
}

static char *lookupInTwoColFile(char *original, char *tsvName)
/* Lookup value in a two column file which we cache */
{
static struct hash *fileHash = NULL;
if (fileHash == NULL)
    fileHash = hashNew(0);
struct hash *idHash = hashFindVal(fileHash, tsvName);
if (idHash == NULL)
    {
    idHash = hashTwoColTsv(tsvName);
    hashAdd(fileHash, tsvName, idHash);
    }
return emptyForNull(hashFindVal(idHash, original));
}

char *finalMatchToSubstring(char *haystack,char *needle)
/* Return the final position of needle in haystack */
{
char *match = NULL;
for (;;)
    {
    haystack = strstr(haystack, needle);
    if (haystack == NULL)
        break;
    match = haystack;
    haystack += 1;  // Don't repeat last match
    }
return match;
}

static struct strexEval strexEvalCallBuiltIn(struct strexParse *p, struct strexRun *run)
/* Handle parse tree generated by call to a built in function. */
{
struct strexBuiltIn *builtIn = p->val.builtIn;
struct strexEval res;
res.type = builtIn->returnType;
struct lm *lm = run->lm;

switch (builtIn->func)
    {
    case strexBuiltInTrim:
	{
        struct strexEval a = strexLocalEval(p->children, run);
	res.val.s = trimSpaces(a.val.s);
	break;
	}
    case strexBuiltInBetween:
	{
        struct strexEval a = strexLocalEval(p->children, run);
        struct strexEval b = strexLocalEval(p->children->next, run);
        struct strexEval c = strexLocalEval(p->children->next->next, run);
	char *between = stringBetween(a.val.s, c.val.s, b.val.s);
	res.val.s = emptyForNull(lmCloneString(lm, between));
	freeMem(between);
        break;
	}
    case strexBuiltInWord:
        {
        struct strexEval a = strexLocalEval(p->children, run);
        struct strexEval b = strexLocalEval(p->children->next, run);
	res.val.s = splitString(a.val.s, b.val.i, lm);
	break;
	}
    case strexBuiltInNow:
        {
	time_t now;
	time(&now);
	char buf[64];
	// strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
	strftime(buf, sizeof buf, "%FT%T%z", localtime(&now));
	res.val.s = lmCloneString(lm, buf);
	eraseTrailingSpaces(res.val.s);
	break;
	}
    case strexBuiltInMd5:
        {
        struct strexEval a = strexLocalEval(p->children, run);
	char *md5 = hmacMd5("", a.val.s);
	res.val.s = lmCloneString(lm, md5);
	freez(&md5);
	break;
	}
    case strexBuiltInChop:
        {
        struct strexEval a = strexLocalEval(p->children, run);
        struct strexEval b = strexLocalEval(p->children->next, run);
        struct strexEval c = strexLocalEval(p->children->next->next, run);
	res.val.s = separateString(a.val.s, b.val.s, c.val.i, lm);
	break;
	}
    case strexBuiltInUncsv:
        {
        struct strexEval a = strexLocalEval(p->children, run);
        struct strexEval b = strexLocalEval(p->children->next, run);
	res.val.s = uncsvString(a.val.s, b.val.i, lm);
	break;
	}
    case strexBuiltInUntsv:
        {
        struct strexEval a = strexLocalEval(p->children, run);
        struct strexEval b = strexLocalEval(p->children->next, run);
	res.val.s = untsvString(a.val.s, b.val.i, lm);
	break;
	}
    case strexBuiltInReplace:
        {
        struct strexEval a = strexLocalEval(p->children, run);
        struct strexEval b = strexLocalEval(p->children->next, run);
        struct strexEval c = strexLocalEval(p->children->next->next, run);
	res.val.s = replaceString(a.val.s, b.val.s, c.val.s, lm);
	break;
	}
    case strexBuiltInFix:
        {
        struct strexEval string = strexLocalEval(p->children, run);
        struct strexEval oldVal = strexLocalEval(p->children->next, run);
        struct strexEval newVal = strexLocalEval(p->children->next->next, run);
	if (sameString(string.val.s, oldVal.val.s))
	    {
	    res.val.s = newVal.val.s;
	    }
	else
	    res.val.s = string.val.s;
	break;
	}
    case strexBuiltInLookup:
        {
        struct strexEval string = strexLocalEval(p->children, run);
        struct strexEval fileName = strexLocalEval(p->children->next, run);
	res.val.s = lookupInTwoColFile(string.val.s, fileName.val.s);
	break;
	}
    case strexBuiltInStrip:
        {
        struct strexEval a = strexLocalEval(p->children, run);
        struct strexEval b = strexLocalEval(p->children->next, run);
	res.val.s = stripAll(a.val.s, b.val.s, lm);
	break;
	}
    case strexBuiltInLen:
        {
        struct strexEval a = strexLocalEval(p->children, run);
	res.val.i = strlen(a.val.s);
	break;
	}
    case strexBuiltInSymbol:  // Convert string to something could use as a C language symbol
        {
        struct strexEval a = strexLocalEval(p->children, run);
        struct strexEval b = strexLocalEval(p->children->next, run);
	res.val.s = symbolify(a.val.s, b.val.s, lm);
	break;
	}
    case strexBuiltInSymbolId:  // Convert string to something could use numeric id
        {
        struct strexEval a = strexLocalEval(p->children, run);
        struct strexEval b = strexLocalEval(p->children->next, run);
	res.val.s = idify(a.val.s, b.val.s, lm);
	break;
	}
    case strexBuiltInLower:
        {
        struct strexEval a = strexLocalEval(p->children, run);
	res.val.s = lmCloneString(lm, a.val.s);
	tolowers(res.val.s);
	break;
	}
    case strexBuiltInUpper:
        {
        struct strexEval a = strexLocalEval(p->children, run);
	res.val.s = lmCloneString(lm, a.val.s);
	touppers(res.val.s);
	break;
	}
    case strexBuiltInIn: 
        {
        struct strexEval string = strexLocalEval(p->children, run);
        struct strexEval query = strexLocalEval(p->children->next, run);
	res.val.b = (strstr(string.val.s, query.val.s) != NULL);
	break;
	}
    case strexBuiltInStarts:
        {
        struct strexEval starts = strexLocalEval(p->children, run);
        struct strexEval string = strexLocalEval(p->children->next, run);
	res.val.b = startsWith(starts.val.s, string.val.s);
	break;
	}
    case strexBuiltInEnds:
        {
        struct strexEval string = strexLocalEval(p->children, run);
        struct strexEval end = strexLocalEval(p->children->next, run);
	res.val.b = endsWith(string.val.s, end.val.s);
	break;
	}
    case strexBuiltInSame:
        {
        struct strexEval a = strexLocalEval(p->children, run);
        struct strexEval b = strexLocalEval(p->children->next, run);
	res.val.b = (strcmp(a.val.s, b.val.s) == 0);
	break;
	}
    case strexBuiltInTidy:
	{
	/* Get parameters */
        struct strexEval a = strexLocalEval(p->children, run);
        struct strexEval b = strexLocalEval(p->children->next, run);
        struct strexEval c = strexLocalEval(p->children->next->next, run);
	char *before = a.val.s;
	char *orig = b.val.s;
	char *after = c.val.s;

	/* Figure out start position - start of string if before string is empty or doesn't match 
	 * otherwise right after the place where before matches. */
	char *ourStart = orig;
	if (!isEmpty(before))
	    {
	    char *newStart = strstr(orig, before);
	    if (newStart != NULL)
	        ourStart = newStart + strlen(before);
	    }

	/* Figure out end position */
	char *defaultEnd = ourStart + strlen(ourStart);
	char *ourEnd = defaultEnd;
	if (!isEmpty(after))
	    {
	    char *newEnd = finalMatchToSubstring(ourStart, after);
	    if (newEnd != NULL)
	        ourEnd = newEnd;
	    }

	int size = ourEnd - ourStart;
	assert(size >= 0);
	res.val.s = lmCloneStringZ(lm, ourStart, size);
	break;
	}
    case strexBuiltInWarn:
        {
	/* Figure out the message we want to convey, send it to warning handler
	 * before returning it. */
        struct strexEval a = strexLocalEval(p->children, run);
	char *message = a.val.s;
	char *output = lmJoinStrings(run->lm, "WARNING: ", message);
	if (run->warnHandler != NULL) run->warnHandler(run->symbols, output);
	res.val.s = output;
	break;
	}
    case strexBuiltInError:
        {
	/* Figure out the message we want to convey, send it to warning handler
	 * before returning it. */
        struct strexEval a = strexLocalEval(p->children, run);
	char *message = a.val.s;
	char *output = lmJoinStrings(run->lm, "ERROR: ", message);
	if (run->warnHandler != NULL) 
	    run->warnHandler(run->symbols, output);
	if (run->abortHandler != NULL) 
	    run->abortHandler(run->symbols);
	errAbort("%s", output);
	res.val.s = output;   // some sort of OCD makes me fill this in in spite of errAbort above
	break;
	}
    case strexBuiltInLetterRange:
        {
	res = strexEvalArrayStartEnd(p->children, p->children->next, p->children->next->next, run);
	break;
	}
    case strexBuiltInWordRange:
        {
        struct strexEval string = strexLocalEval(p->children, run);
        struct strexEval start = strexLocalEval(p->children->next, run);
        struct strexEval end = strexLocalEval(p->children->next->next, run);
	res.val.s = strexEvalWordRange(string.val.s, start.val.i, end.val.i, run);
	break;
	}
    case strexBuiltInChopRange:
        {
        struct strexEval string = strexLocalEval(p->children, run);
        struct strexEval splitter = strexLocalEval(p->children->next, run);
        struct strexEval start = strexLocalEval(p->children->next->next, run);
        struct strexEval end = strexLocalEval(p->children->next->next->next, run);
	res.val.s = strexEvalChopRange(string.val.s, splitter.val.s, start.val.i, end.val.i, run);
	break;
	}
    }
return res;
}

static struct strexEval strexEvalPick(struct strexParse *pick, struct strexRun *run)
/* Evaluate a pick operator. */
{
/* Evaluate the keyValue */
struct strexParse *p = pick->children;
struct strexEval keyVal = strexLocalEval(p, run);
p = p->next;

/* Get pointer to default expression but don't evaluate it yet */
struct strexParse *defaultExp = p;
p = p->next;

/* Loop through all the key/val pairs until find first that matches. */
boolean gotMatch = FALSE;
while (p != NULL)
    {
    struct strexEval key = strexLocalEval(p, run);
    p = p->next;  // Parser guarantees this non-null
    struct strexParse *valExp = p;
    p = p->next;
    switch (key.type)
         {
	 case strexTypeInt:
	      gotMatch = (keyVal.val.i == key.val.i);
	      break;
	 case strexTypeDouble:
	      gotMatch = (keyVal.val.x == key.val.x);
	      break;
	 case strexTypeBoolean:
	      gotMatch = (keyVal.val.b = key.val.b);
	      break;
	 case strexTypeString:
	      gotMatch = sameString(keyVal.val.s, key.val.s);
	      break;
	 }
    if (gotMatch)
	 {
         return strexLocalEval(valExp, run);
	 }
    }
return strexLocalEval(defaultExp, run);
}

static struct strexEval strexEvalConditional(struct strexParse *conditional, struct strexRun *run)
{
struct strexParse *child = conditional->children;
struct strexEval b = strexLocalEval(child, run);
struct strexEval res;
if (b.val.b)
    res = strexLocalEval(child->next, run); 
else
    res = strexLocalEval(child->next->next, run); 
return res;
}



static struct strexEval strexLocalEval(struct strexParse *p, struct strexRun *run)
/* Evaluate self on parse tree, allocating memory if needed from lm. */
{
struct strexEval res;
switch (p->op)
    {
    case strexOpLiteral:
	res.val = p->val;
	res.type = p->type;
	break;
    case strexOpSymbol:
	res.type = strexTypeString;
	char *s = run->lookup(run->symbols, p->val.s);
	if (s == NULL)
	    res.val.s = "";
	else
	    res.val.s = s;
	break;

    /* Type casts. */
    case strexOpStringToBoolean:
	res = strexLocalEval(p->children, run);
	res.type = strexTypeBoolean;
	res.val.b = (res.val.s[0] != 0);
	break;
    case strexOpIntToBoolean:
        res = strexLocalEval(p->children, run);
	res.type = strexTypeBoolean;
	res.val.b = (res.val.i != 0);
	break;
    case strexOpDoubleToBoolean:
        res = strexLocalEval(p->children, run);
	res.type = strexTypeBoolean;
	res.val.b = (res.val.x != 0.0);
	break;
    case strexOpStringToInt:
	res = strexLocalEval(p->children, run);
	res.type = strexTypeInt;
	res.val.i = atoll(res.val.s);
	break;
    case strexOpDoubleToInt:
	res = strexLocalEval(p->children, run);
	res.type = strexTypeInt;
	res.val.i = res.val.x;
	break;

    case strexOpStringToDouble:
	res = strexLocalEval(p->children, run);
	res.type = strexTypeDouble;
	res.val.x = atof(res.val.s);
	break;
    case strexOpBooleanToInt:
	res = strexLocalEval(p->children, run);
	res.type = strexTypeInt;
	res.val.i = res.val.b;
	break;
    case strexOpBooleanToDouble:
	res = strexLocalEval(p->children, run);
	res.type = strexTypeDouble;
	res.val.x = res.val.b;
	break;
    case strexOpIntToDouble:
	res = strexLocalEval(p->children, run);
	res.type = strexTypeDouble;
	res.val.x = res.val.b;
	break;

    case strexOpIntToString:
        {
	res = strexLocalEval(p->children, run);
	res.type = strexTypeString;
	char buf[32];
	safef(buf, sizeof(buf), "%lld", res.val.i);
	res.val.s = lmCloneString(run->lm, buf);
	break;
	}
    case strexOpDoubleToString:
        {
	res = strexLocalEval(p->children, run);
	res.type = strexTypeString;
	char buf[32];
	safef(buf, sizeof(buf), "%g", res.val.x);
	res.val.s = lmCloneString(run->lm, buf);
	break;
	}
    case strexOpBooleanToString:
	res = strexLocalEval(p->children, run);
	res.type = strexTypeString;
        res.val.s = (res.val.b ? "true" : "false");
	break;

    /* Arithmetical negation. */
    case strexOpUnaryMinusInt:
        res = strexLocalEval(p->children, run);
	res.val.i = -res.val.i;
	break;
    case strexOpUnaryMinusDouble:
        res = strexLocalEval(p->children, run);
	res.val.x = -res.val.x;
	break;

    case strexOpArrayIx:
       res = strexEvalArrayIx(p, run);
       break;
    case strexOpArrayRange:
       res = strexEvalArrayRange(p, run);
       break;
    case strexOpStrlen:
       res = strexLocalEval(p->children, run);
       res.type = strexTypeInt;
       res.val.i = strlen(res.val.s);
       break;

    /* More complicated ops. */
    case strexOpBuiltInCall:
       res = strexEvalCallBuiltIn(p, run);
       break;
    case strexOpPick:
       res = strexEvalPick(p, run);
       break;
    case strexOpConditional:
       res = strexEvalConditional(p, run);
       break;


    /* Mathematical ops, simple binary type */
    case strexOpAdd:
       res = strexEvalAdd(p, run);
       break;
    case strexOpSubtract:
       res = strexEvalSubtract(p, run);
       break;

    /* Comparison ops, simple binary type */
    case strexOpEq:
       res = strexEvalEq(p, run);
       break;
    case strexOpNe:
       res = strexEvalNe(p, run);
       break;
    case strexOpGt:
       res = strexEvalGt(p, run);
       break;
    case strexOpLt:
       res = strexEvalLt(p, run);
       break;
    case strexOpGe:
       res = strexEvalGe(p, run);
       break;
    case strexOpLe:
       res = strexEvalLe(p, run);
       break;

    /* Logical ops, simple binary type and not*/
    case strexOpOr:
        res = strexEvalOr(p, run);
        break;
    case strexOpAnd:
        res = strexEvalAnd(p, run);
        break;
    case strexOpNot:
        res = strexLocalEval(p->children, run);
	res.val.b = !res.val.b;
	break;

    default:
        errAbort("Unknown op %s\n", strexOpToString(p->op));
	res.type = strexTypeInt;	// Keep compiler from complaining.
	res.val.i = 0;	// Keep compiler from complaining.
	break;
    }
return res;
}

/* ---- And the one public evaluation function ---- */

char *strexEvalAsString(struct strexParse *p, void *symbols, StrexLookup lookup,
    void (*warnHandler)(void *symbols, char *warning), void (*abortHandler)(void *symbols) )
/* Return result as a string value.  The warnHandler and abortHandler are optional,
 * and can be used to wrap your own warnings and abort processing onto what strex
 * provides.  For default behavior just pass in NULL. */

{
/* Set up run time environment for strex and evaluate.*/
struct strexRun *run = strexRunNew(symbols, lookup, warnHandler, abortHandler);
struct strexEval res = strexLocalEval(p,run);

/* Convert result to string and copy to memory that'll survive loss of run time environment */
char numBuf[32];
struct strexEval strRes = strexEvalCoerceToString(res, numBuf, sizeof(numBuf));
char *ret = cloneString(strRes.val.s);  

/* Clean up and go home */
strexRunFree(&run);
return ret;
}

