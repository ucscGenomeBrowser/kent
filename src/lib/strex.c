/* strex - implementation of STRing EXPression language,  currently used in tabToTabDir
 * to describe how the output fields are filled in from input fields. 
 *
 * This is a little language implemented as a recursive descent parser that creates a
 * parse tree that can be quickly evaluated to produce a string output given an input
 * set of symbols.  It is up to the evaluator to make up a symbol table, but an example of
 * using a hash as a symbol table is available.
 *
 * The language handles variables, string constants, numerical constants, and
 * a very limited set of built in predefined functions.   The numerical constants are only used for
 * array indexes and some of the built in functions.  All functions return string values, and
 * all variables have string values.
 *
 * You can build up strings with the '+' operator, which concatenates two strings.
 * String constants can be either " or ' delimited
 *
 * You can parse apart strings with the built in functions and with the subscript [] operator.
 * The built in functions are described in a separate doc,  which will hopefully still be
 * available in the same directory as this file as strex.doc when you read this.
 *
 * The subscript operator treats the string it is applied to as a comma separated value array.
 * It really is not very efficient alas.
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
/* One of these for each builtIn.  We'll just do a switch to implement 
 * Each built in function needs a value here, to keep it simple there's
 * aa correspondence between these names and the built in function name */
    {
    strexBuiltInTrim,
    strexBuiltInBetween,
    strexBuiltInSplit,
    strexBuiltInNow,
    strexBuiltInMd5,
    strexBuiltInSeparate,
    };

struct strexBuiltIn
/* Information to describe a built in function */
    {
    char *name;		/* Name in strex language:  trim, split, etc */
    enum strexBuiltInFunc func;  /* enum version: strexBuiltInTrim strexBuiltInSplit etc. */
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
    strexOpSymbol,	/* A symbol name. */

    strexOpBuiltInCall,	/* Call a built in function */
    strexOpArrayIx,	/* An array with an index. */

    /* Unary minus for numbers */
    strexOpUnaryMinusInt,
    strexOpUnaryMinusDouble,

    /* Binary operations. */
    strexOpAdd,

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
    };

/* Some predefined lists of parameter types */
static enum strexType oneString[] = {strexTypeString};
// static enum strexType twoStrings[] = {strexTypeString, strexTypeString};
static enum strexType threeStrings[] = {strexTypeString, strexTypeString, strexTypeString};
static enum strexType stringInt[] = {strexTypeString, strexTypeInt};
static enum strexType stringStringInt[] = {strexTypeString, strexTypeString, strexTypeInt};

/* There's one element here for each built in function.  There's also a few switches you'll need to
 * fill in if you add a new built in function. */
static struct strexBuiltIn builtins[] = {
    { "trim", strexBuiltInTrim, 1, oneString, },
    { "between", strexBuiltInBetween, 3, threeStrings, },
    { "split", strexBuiltInSplit, 2, stringInt },
    { "now", strexBuiltInNow, 0, NULL },
    { "md5", strexBuiltInMd5, 1, oneString },
    { "separate", strexBuiltInSeparate, 3, stringStringInt },
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

static struct strexIn *strexInNew(char *expression, char *fileName, int fileLineNumber)
/* Return a new strexIn structure wrapped around expression */
{
struct lineFile *lf = lineFileOnString(fileName, TRUE, expression);
lf->lineIx = fileLineNumber;
struct tokenizer *tkz = tokenizerOnLineFile(lf);
tkz->leaveQuotes = TRUE;
struct strexIn *si;
AllocVar(si);
si->tkz = tkz;
si->builtInHash = hashBuiltIns();
return si;
}

static void strexInFree(struct strexIn **pSi)
/* Free up memory associated with strexIn structure */
{
struct strexIn *si = *pSi;
if (si != NULL)
    {
    hashFree(&si->builtInHash);
    tokenizerFree(&si->tkz);
    freez(pSi);
    }
}

static void strexValDump(union strexVal val, enum strexType type, FILE *f)
/* Dump out value to file. */
{
switch (type)
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

    case strexOpBuiltInCall:
        return "strexOpBuiltInCall";

    case strexOpArrayIx:
        return "strexOpArrayIx";

    default:
	return "strexOpUnknown";
    }
}

void strexParseDump(struct strexParse *p, int depth, FILE *f)
/* Dump out strexParse tree and children. */
{
spaceOut(f, 3*depth);
fprintf(f, "%s ", strexOpToString(p->op));
strexValDump(p->val, p->type,  f);
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

static void skipOverRequired(struct strexIn *in, char *expecting)
/* Make sure that next token is tok, and skip over it. */
{
tokenizerMustHaveNext(in->tkz);
if (!sameWord(in->tkz->string, expecting))
    expectingGot(in, expecting, in->tkz->string);
}


static struct strexParse *strexParseExpression(struct strexIn *in);
/* Parse out an expression with a single value */

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

static enum strexType commonTypeForBop(enum strexType left, enum strexType right)
/* Return type that will work for a binary operation. */
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

static struct strexParse *strexParseFunction(struct strexIn *in)
/* Handle the (a,b,c) in funcCall(a,b,c).  Convert it into tree:
*         strexOpBuiltInCall
*            strexParse(a)
*            strexParse(b)
*            strexParse(c)
* or something like that.  With no parameters 
*            strexParseFunction */
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
    struct strexBuiltIn *builtIn = hashFindVal(in->builtInHash, functionName);
    if (builtIn == NULL)
        errAbort("No built in function %s exists line %d of %s", functionName, tkz->lf->lineIx,
	    tkz->lf->fileName);

    /* We're going to reuse this current op */
    function->op = strexOpBuiltInCall;
    function->type = strexTypeString;
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
else
    tokenizerReuse(tkz);
return function;
}

static struct strexParse *strexParseIndex(struct strexIn *in)
/* Handle the [] in this[6].  Convert it into tree:
*         strexOpArrayIx
*            strexParseFunction
*            strexParseFunction */
{
struct tokenizer *tkz = in->tkz;
struct strexParse *collection = strexParseFunction(in);
struct strexParse *p = collection;
char *tok = tokenizerNext(tkz);
if (tok == NULL)
    tokenizerReuse(tkz);
else if (tok[0] == '[')
    {
    struct strexParse *index = strexParseExpression(in);
    // struct strexParse *index = strexParseFunction(in);
    index = strexParseCoerce(index, strexTypeInt);
    skipOverRequired(in, "]");
    AllocVar(p);
    p->op = strexOpArrayIx;
    p->type = strexTypeString;
    p->children = collection;
    p->val.s = cloneString("");
    collection->next = index;
    }
else
    tokenizerReuse(tkz);
return p;
}


static struct strexParse *strexParseUnaryMinus(struct strexIn *in)
/* Return unary minus sort of parse tree if there's a leading '-' */
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
struct strexParse *p = strexParseUnaryMinus(in);
for (;;)
    {
    char *tok = tokenizerNext(tkz);
    if (tok == NULL || differentString(tok, "+"))
	{
	tokenizerReuse(tkz);
	return p;
	}

    /* What we've parsed so far becomes left side of binary op, next term ends up on right. */
    struct strexParse *l = p;
    struct strexParse *r = strexParseUnaryMinus(in);

    /* Make left and right side into a common type */
    enum strexType childType = commonTypeForBop(l->type, r->type);
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


static struct strexParse *strexParseExpression(struct strexIn *in)
/* Parse out an expression. Leaves input at next expression. */
{
return strexParseSum(in);
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

struct strexParse *strexParseString(char *s, char *fileName, int fileLineNumber)
/* Parse out string expression in s and return root of tree. */
{
struct strexIn *si = strexInNew(s, fileName, fileLineNumber);
struct strexParse *parseTree = strexParseExpression(si);
ensureAtEnd(si);
strexInFree(&si);
return parseTree;
}

/* The parsing section is done, now for the evaluation section. */

static struct strexEval strexLocalEval(struct strexParse *p, void *record, StrexEvalLookup lookup, 
	struct lm *lm);
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

static struct strexEval strexEvalAdd(struct strexParse *p, void *record, StrexEvalLookup lookup,
	struct lm *lm)
/* Return a + b. */
{
struct strexParse *lp = p->children;
struct strexParse *rp = lp->next;
struct strexEval lv = strexLocalEval(lp, record, lookup, lm);
struct strexEval rv = strexLocalEval(rp, record, lookup, lm);
struct strexEval res;
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
	char *s = lmAlloc(lm, lLen + rLen + 1);
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

static struct strexEval strexEvalArrayIx(struct strexParse *p, void *record, StrexEvalLookup lookup,
	struct lm *lm)
/* Handle parse tree generated by an indexed array. */
{
struct strexParse *array = p->children;
struct strexParse *index = array->next;
struct strexEval arrayVal = strexLocalEval(array, record, lookup, lm);
struct strexEval indexVal = strexLocalEval(index, record, lookup, lm);
struct strexEval res;
struct dyString *scratch = dyStringNew(0);
char *val = emptyForNull(csvParseOneOut(arrayVal.val.s, indexVal.val.i, scratch));
res.val.s = cloneString(val);
res.type = strexTypeString;
return res;
}

static char *splitString(char *words,  int ix,  struct lm *lm)
/* Return the space-delimited word of index ix as clone into lm */
{
char *s = words;
int i;
for (i=0; ; ++i)
    {
    s = skipLeadingSpaces(s);
    if (isEmpty(s))
        errAbort("There aren't %d words in %s", ix+1, words);
    char *end = skipToSpaces(s);
    if (i == ix)
        {
	if (end == NULL)
	    return lmCloneString(lm, s);
	else
	    return lmCloneMem(lm, s, end - s);
	}
    s = end;
    }
}

static char *separateString(char *string, char *splitter, int ix, struct lm *lm)
/* Return the ix'th part of string as split apart by splitter */
{
int splitterSize = strlen(splitter);
if (splitterSize != 1)
    errAbort("Separator parameter to split must be a single character, not %s", splitter);
int count = chopByChar(string, splitter[0], NULL, 0);
if (ix >= count)
    errAbort("There aren't %d fields separated by %s in %s", ix+1, splitter, string);
char **row;
lmAllocArray(lm, row, count);
char *scratch = lmCloneString(lm, string);
chopByChar(scratch, splitter[0], row, count);
return row[ix];
}

static struct strexEval strexEvalCallBuiltIn(struct strexParse *p, 
    void *record, StrexEvalLookup lookup, struct lm *lm)
/* Handle parse tree generated by an indexed array. */
{
struct strexBuiltIn *builtIn = p->val.builtIn;
struct strexEval res;
res.type = strexTypeString;
switch (builtIn->func)
    {
    case strexBuiltInTrim:
	{
        struct strexEval a = strexLocalEval(p->children, record, lookup, lm);
	res.val.s = trimSpaces(a.val.s);
	break;
	}
    case strexBuiltInBetween:
	{
        struct strexEval a = strexLocalEval(p->children, record, lookup, lm);
        struct strexEval b = strexLocalEval(p->children->next, record, lookup, lm);
        struct strexEval c = strexLocalEval(p->children->next->next, record, lookup, lm);
	char *between = stringBetween(a.val.s, c.val.s, b.val.s);
	res.val.s = lmCloneString(lm, between);
	freeMem(between);
        break;
	}
    case strexBuiltInSplit:
        {
        struct strexEval a = strexLocalEval(p->children, record, lookup, lm);
        struct strexEval b = strexLocalEval(p->children->next, record, lookup, lm);
	res.val.s = splitString(a.val.s, b.val.i, lm);
	break;
	}
    case strexBuiltInNow:
        {
	time_t now = time(NULL);
	res.val.s = lmCloneString(lm, ctime(&now));
	eraseTrailingSpaces(res.val.s);
	break;
	}
    case strexBuiltInMd5:
        {
        struct strexEval a = strexLocalEval(p->children, record, lookup, lm);
	char *md5 = hmacMd5("", a.val.s);
	res.val.s = lmCloneString(lm, md5);
	freez(&md5);
	break;
	}
    case strexBuiltInSeparate:
        {
        struct strexEval a = strexLocalEval(p->children, record, lookup, lm);
        struct strexEval b = strexLocalEval(p->children->next, record, lookup, lm);
        struct strexEval c = strexLocalEval(p->children->next->next, record, lookup, lm);
	res.val.s = separateString(a.val.s, b.val.s, c.val.i, lm);
	break;
	}
    }
return res;
}


static struct strexEval strexLocalEval(struct strexParse *p, void *record, StrexEvalLookup lookup, 
	struct lm *lm)
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
	char *s = lookup(record, p->val.s);
	if (s == NULL)
	    res.val.s = "";
	else
	    res.val.s = s;
	break;

    /* Type casts. */
    case strexOpStringToBoolean:
	res = strexLocalEval(p->children, record, lookup, lm);
	res.type = strexTypeBoolean;
	res.val.b = (res.val.s[0] != 0);
	break;
    case strexOpIntToBoolean:
        res = strexLocalEval(p->children, record, lookup, lm);
	res.type = strexTypeBoolean;
	res.val.b = (res.val.i != 0);
	break;
    case strexOpDoubleToBoolean:
        res = strexLocalEval(p->children, record, lookup, lm);
	res.type = strexTypeBoolean;
	res.val.b = (res.val.x != 0.0);
	break;
    case strexOpStringToInt:
	res = strexLocalEval(p->children, record, lookup, lm);
	res.type = strexTypeInt;
	res.val.i = atoll(res.val.s);
	break;
    case strexOpDoubleToInt:
	res = strexLocalEval(p->children, record, lookup, lm);
	res.type = strexTypeInt;
	res.val.i = res.val.x;
	break;

    case strexOpStringToDouble:
	res = strexLocalEval(p->children, record, lookup, lm);
	res.type = strexTypeDouble;
	res.val.x = atof(res.val.s);
	break;
    case strexOpBooleanToInt:
	res = strexLocalEval(p->children, record, lookup, lm);
	res.type = strexTypeInt;
	res.val.i = res.val.b;
	break;
    case strexOpBooleanToDouble:
	res = strexLocalEval(p->children, record, lookup, lm);
	res.type = strexTypeDouble;
	res.val.x = res.val.b;
	break;
    case strexOpIntToDouble:
	res = strexLocalEval(p->children, record, lookup, lm);
	res.type = strexTypeDouble;
	res.val.x = res.val.b;
	break;

    case strexOpIntToString:
        {
	res = strexLocalEval(p->children, record, lookup, lm);
	res.type = strexTypeString;
	char buf[32];
	safef(buf, sizeof(buf), "%lld", res.val.i);
	res.val.s = lmCloneString(lm, buf);
	break;
	}
    case strexOpDoubleToString:
        {
	res = strexLocalEval(p->children, record, lookup, lm);
	res.type = strexTypeString;
	char buf[32];
	safef(buf, sizeof(buf), "%g", res.val.x);
	res.val.s = lmCloneString(lm, buf);
	break;
	}
    case strexOpBooleanToString:
	res = strexLocalEval(p->children, record, lookup, lm);
	res.type = strexTypeString;
        res.val.s = (res.val.b ? "true" : "false");
	break;

    /* Arithmetical negation. */
    case strexOpUnaryMinusInt:
        res = strexLocalEval(p->children, record, lookup, lm);
	res.val.i = -res.val.i;
	break;
    case strexOpUnaryMinusDouble:
        res = strexLocalEval(p->children, record, lookup, lm);
	res.val.x = -res.val.x;
	break;

    case strexOpArrayIx:
       res = strexEvalArrayIx(p, record, lookup, lm);
       break;

    case strexOpBuiltInCall:
       res = strexEvalCallBuiltIn(p, record, lookup, lm);
       break;

    /* Mathematical ops, simple binary type */
    case strexOpAdd:
       res = strexEvalAdd(p, record, lookup, lm);
       break;

    default:
        errAbort("Unknown op %s\n", strexOpToString(p->op));
	res.type = strexTypeInt;	// Keep compiler from complaining.
	res.val.i = 0;	// Keep compiler from complaining.
	break;
    }
return res;
}

char *strexEvalAsString(struct strexParse *p, void *record, StrexEvalLookup lookup)
/* Evaluating a strex expression on a symbol table with a lookup function for variables and
 * return result as a string value. */
{
struct lm *lm = lmInit(0);
struct strexEval res = strexLocalEval(p, record, lookup, lm);
char numBuf[32];
struct strexEval strRes = strexEvalCoerceToString(res, numBuf, sizeof(numBuf));
char *ret = cloneString(strRes.val.s);
lmCleanup(&lm);
return ret;
}

