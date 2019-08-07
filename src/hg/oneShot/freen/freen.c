/* freen - My Pet Freen.  A pet freen is actually more dangerous than a wild one. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "portable.h"
#include "obscure.h"
#include "localmem.h"
#include "csv.h"
#include "tokenizer.h"

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

/*  strex -String Expression - stuff to implement a relatively simple string
 *  processing expression parser. */


enum strexType
/* A type */
    {
    strexTypeBoolean = 1,
    strexTypeString = 2,
    strexTypeInt = 3,
    strexTypeDouble = 4,
    };

union strexVal
/* Some value of arbirary type that can be of any type corresponding to strexType */
    {
    boolean b;
    char *s;
    long long i;
    double x;
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

    strexOpUnaryMinusInt,
    strexOpUnaryMinusDouble,

    /* Binary operations. */
    strexOpAdd,

    /* Type conversions */
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
/* A strex parse-tree. */
    {
    struct strexParse *next;	/* Points to younger sibling if any. */
    struct strexParse *children;	/* Points to oldest child if any. */
    enum strexOp op;		/* Operation at this node. */
    enum strexType type;		/* Return type of this operation. */
    union strexVal val;		/* Return value of this operation. */
    };

typedef char* (*StrexEvalLookup)(void *record, char *key);
/* Callback for strexEvalOnRecord to lookup a variable value. */

void strexValDump(union strexVal val, enum strexType type, FILE *f)
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

char *strexOpToString(enum strexOp op)
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



static void expectingGot(struct tokenizer *tkz, char *expecting, char *got)
/* Print out error message about unexpected input. */
{
errAbort("Expecting %s, got %s, line %d of %s", expecting, got, tkz->lf->lineIx,
	tkz->lf->fileName);
}

static void skipOverRequired(struct tokenizer *tkz, char *expecting)
/* Make sure that next token is tok, and skip over it. */
{
tokenizerMustHaveNext(tkz);
if (!sameWord(tkz->string, expecting))
    expectingGot(tkz, expecting, tkz->string);
}


struct strexParse *strexParseExpression(struct tokenizer *tkz);
/* Parse out an expression with a single value */

static struct strexParse *strexParseAtom(struct tokenizer *tkz)
/* Return low level (symbol or literal) */
{
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
    p = strexParseExpression(tkz);
    skipOverRequired(tkz, ")");
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

static struct strexParse *strexParseFunction(struct tokenizer *tkz)
/* Handle the (a,b,c) in funcCall(a,b,c).  Convert it into tree:
*         strexOpBuiltInCall
*            strexParse(a)
*            strexParse(b)
*            strexParse(c)
* or something like that.  With no parameters 
*            strexParseFunction */
{
struct strexParse *function = strexParseAtom(tkz);
char *tok = tokenizerNext(tkz);
if (tok == NULL)
    tokenizerReuse(tkz);
else if (tok[0] == '(')
    {
    uglyf("Making function I hope\n");
    /* Check that the current op, is a pure symbol. */
    if (function->op != strexOpSymbol)
        errAbort("Unexpected '(' line %d of %s", tkz->lf->lineIx, tkz->lf->fileName);

    /* We're going to reuse this current op */
    function->op = strexOpBuiltInCall;
    function->type = strexTypeString;

    tok = tokenizerMustHaveNext(tkz);
    if (tok[0] != ')')
        {
	tokenizerReuse(tkz);
	for (;;)
	    {
	    struct strexParse *param = strexParseExpression(tkz);
	    // struct strexParse *param = strexParseAtom(tkz);
	    slAddHead(&function->children, param);
	    tok = tokenizerMustHaveNext(tkz);
	    uglyf("in function got %s\n", tok);
	    if (tok[0] == ')')
	        break;
	    else if (tok[0] != ',')
	        errAbort("Error in parameter list for %s line %d of %s", function->val.s, 
		    tkz->lf->lineIx, tkz->lf->fileName);
	    }
	slReverse(&function->children);
	}
    }
else
    tokenizerReuse(tkz);
return function;
}

static struct strexParse *strexParseIndex(struct tokenizer *tkz)
/* Handle the [] in this[6].  Convert it into tree:
*         strexOpArrayIx
*            strexParseFunction
*            strexParseFunction */
{
struct strexParse *collection = strexParseFunction(tkz);
struct strexParse *p = collection;
char *tok = tokenizerNext(tkz);
if (tok == NULL)
    tokenizerReuse(tkz);
else if (tok[0] == '[')
    {
    struct strexParse *index = strexParseExpression(tkz);
    // struct strexParse *index = strexParseFunction(tkz);
    index = strexParseCoerce(index, strexTypeInt);
    skipOverRequired(tkz, "]");
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


static struct strexParse *strexParseUnaryMinus(struct tokenizer *tkz)
/* Return unary minus sort of parse tree if there's a leading '-' */
{
char *tok = tokenizerMustHaveNext(tkz);
if (tok[0] == '-')
    {
    struct strexParse *c = strexParseIndex(tkz);
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
    return strexParseIndex(tkz);
    }
}

static struct strexParse *strexParseSum(struct tokenizer *tkz)
/* Parse out plus or minus. */
{
struct strexParse *p = strexParseUnaryMinus(tkz);
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
    struct strexParse *r = strexParseUnaryMinus(tkz);

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


struct strexParse *strexParseExpression(struct tokenizer *tkz)
/* Parse out a clause, usually a where clause. */
{
return strexParseSum(tkz);
}

/* ~~~ start of strexEval */

static struct strexEval strexLocalEval(struct strexParse *p, void *record, StrexEvalLookup lookup, 
	struct lm *lm);
/* Evaluate self on parse tree, allocating memory if needed from lm. */


struct strexEval strexEvalCoerceToString(struct strexEval r, char *buf, int bufSize)
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


char *csvParseOneOut(char *csvIn, int ix, struct dyString *scratch)
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

static struct strexEval strexEvalCallBuiltIn(struct strexParse *p, void *record, StrexEvalLookup lookup,
	struct lm *lm)
/* Handle parse tree generated by an indexed array. */
{
char buf[256];
safef(buf, sizeof(buf), "%s(%d parames)", p->val.s,  slCount(p->children));
struct strexEval res;
res.val.s = cloneString(buf);
res.type = strexTypeString;
return res;
}


/* ~~~ evaluation part of strex */

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

struct strexEval strexEvalOnRecord(struct strexParse *p, void *record, StrexEvalLookup lookup, 
	struct lm *lm)
/* Evaluate parse tree on record, using lm for memory for string operations. */
{
return strexLocalEval(p, record, lookup, lm);
}

static struct strexEval strexLocalEval(struct strexParse *p, void *record, StrexEvalLookup lookup, 
	struct lm *lm);
/* Evaluate self on parse tree, allocating memory if needed from lm. */

/* ~~~ end of strex */

struct hash *hashFromFile(char *fileName)
/* Given a two column file (key, value) return a hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(16);
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *word = nextWord(&line);
    line = trimSpaces(line);
    hashAdd(hash, word, lmCloneString(hash->lm, line));
    }
lineFileClose(&lf);
return hash;
}

static char *symLookup(void *record, char *key)
/* Lookup symbol in hash */
{
struct hash *hash = record;
return hashFindVal(hash, key);
}

void freen(char *symbols, char *expression)
/* Test something */
{
struct hash *symHash = hashFromFile(symbols);
verbose(1, "Got %d symbols from %s\n", symHash->elCount, symbols);
struct lineFile *lf = lineFileOnString(expression, TRUE, cloneString(expression));
struct tokenizer *tkz = tokenizerOnLineFile(lf);
tkz->leaveQuotes = TRUE;
struct strexParse *parseTree = strexParseExpression(tkz);
strexParseDump(parseTree, 0, uglyOut);
struct lm *evalLm = lmInit(0);
struct strexEval res =strexEvalOnRecord(parseTree, symHash, symLookup, evalLm);
uglyf("res = %s\n", res.val.s);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
freen(argv[1], argv[2]);
return 0;
}

