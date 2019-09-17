/* rqlEval - evaluate tree returned by rqlParse given a record and function to lookup fields
 * in the record. . */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "localmem.h"
#include "tokenizer.h"
#include "sqlNum.h"
#include "rql.h"


static struct rqlEval rqlLocalEval(struct rqlParse *p, void *record, RqlEvalLookup lookup, 
	struct lm *lm);
/* Evaluate self on parse tree, allocating memory if needed from lm. */

struct rqlEval rqlEvalCoerceToBoolean(struct rqlEval r)
/* Return TRUE if it's a nonempty string or a non-zero number. */
{
switch (r.type)
    {
    case rqlTypeBoolean:
	break;	/* It's already done. */
    case rqlTypeString:
        r.val.b = (r.val.s != NULL && r.val.s[0] != 0);
	break;
    case rqlTypeInt:
        r.val.b = (r.val.i != 0);
	break;
    case rqlTypeDouble:
        r.val.b = (r.val.x != 0.0);
	break;
    default:
	internalErr();
	r.val.b = FALSE;
	break;
    }
r.type = rqlTypeBoolean;
return r;
}

struct rqlEval rqlEvalCoerceToString(struct rqlEval r, char *buf, int bufSize)
/* Return a version of r with .val.s filled in with something reasonable even
 * if r input is not a string */
{
assert(bufSize >= 32);
switch (r.type)
    {
    case rqlTypeBoolean:
        r.val.s = (r.val.b ? "true" : "false");
    case rqlTypeString:
	break;	/* It's already done. */
    case rqlTypeInt:
	safef(buf, bufSize, "%lld", r.val.i);
	r.val.s = buf;
	break;
    case rqlTypeDouble:
	safef(buf, bufSize, "%g", r.val.x);
	r.val.s = buf;
	break;
    default:
	internalErr();
	r.val.s = NULL;
	break;
    }
r.type = rqlTypeString;
return r;
}

static struct rqlEval rqlEvalEq(struct rqlParse *p, 
	void *record, RqlEvalLookup lookup, struct lm *lm)
/* Return true if two children are equal regardless of children type
 * (which are just gauranteed to be the same). */
{
struct rqlParse *lp = p->children;
struct rqlParse *rp = lp->next;
struct rqlEval lv = rqlLocalEval(lp, record, lookup, lm);
struct rqlEval rv = rqlLocalEval(rp, record, lookup, lm);
struct rqlEval res;
res.type = rqlTypeBoolean;
assert(lv.type == rv.type);
switch (lv.type)
    {
    case rqlTypeBoolean:
        res.val.b = (lv.val.b == rv.val.b);
	break;
    case rqlTypeString:
	res.val.b = sameString(lv.val.s, rv.val.s);
	break;
    case rqlTypeInt:
	res.val.b = (lv.val.i == rv.val.i);
	break;
    case rqlTypeDouble:
	res.val.b = (lv.val.x == rv.val.x);
	break;
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
return res;
}

static struct rqlEval rqlEvalLt(struct rqlParse *p, void *record, RqlEvalLookup lookup,
	struct lm *lm)
/* Return true if r < l . */
{
struct rqlParse *lp = p->children;
struct rqlParse *rp = lp->next;
struct rqlEval lv = rqlLocalEval(lp, record, lookup, lm);
struct rqlEval rv = rqlLocalEval(rp, record, lookup, lm);
struct rqlEval res;
res.type = rqlTypeBoolean;
switch (lv.type)
    {
    case rqlTypeBoolean:
        res.val.b = (lv.val.b < rv.val.b);
	break;
    case rqlTypeString:
	res.val.b = strcmp(lv.val.s, rv.val.s) < 0;
	break;
    case rqlTypeInt:
	res.val.b = (lv.val.i < rv.val.i);
	break;
    case rqlTypeDouble:
	res.val.b = (lv.val.x < rv.val.x);
	break;
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
return res;
}

static struct rqlEval rqlEvalGt(struct rqlParse *p, void *record, RqlEvalLookup lookup,
	struct lm *lm)
/* Return true if r > l . */
{
struct rqlParse *lp = p->children;
struct rqlParse *rp = lp->next;
struct rqlEval lv = rqlLocalEval(lp, record, lookup, lm);
struct rqlEval rv = rqlLocalEval(rp, record, lookup, lm);
struct rqlEval res;
res.type = rqlTypeBoolean;
switch (lv.type)
    {
    case rqlTypeBoolean:
        res.val.b = (lv.val.b > rv.val.b);
	break;
    case rqlTypeString:
	res.val.b = strcmp(lv.val.s, rv.val.s) > 0;
	break;
    case rqlTypeInt:
	res.val.b = (lv.val.i > rv.val.i);
	break;
    case rqlTypeDouble:
	res.val.b = (lv.val.x > rv.val.x);
	break;
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
return res;
}

static struct rqlEval rqlEvalAdd(struct rqlParse *p, void *record, RqlEvalLookup lookup,
	struct lm *lm)
/* Return a + b. */
{
struct rqlParse *lp = p->children;
struct rqlParse *rp = lp->next;
struct rqlEval lv = rqlLocalEval(lp, record, lookup, lm);
struct rqlEval rv = rqlLocalEval(rp, record, lookup, lm);
struct rqlEval res;
switch (lv.type)
    {
    case rqlTypeInt:
	res.val.i = (lv.val.i + rv.val.i);
	break;
    case rqlTypeDouble:
	res.val.x = (lv.val.x + rv.val.x);
	break;
    case rqlTypeString:
	{
	char numBuf[32];
	if (rv.type != rqlTypeString)  // Perhaps later could coerce to string
	    {
	    rv = rqlEvalCoerceToString(rv, numBuf, sizeof(numBuf));
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

static struct rqlEval rqlEvalSubtract(struct rqlParse *p, void *record, RqlEvalLookup lookup,
	struct lm *lm)
/* Return a - b. */
{
struct rqlParse *lp = p->children;
struct rqlParse *rp = lp->next;
struct rqlEval lv = rqlLocalEval(lp, record, lookup, lm);
struct rqlEval rv = rqlLocalEval(rp, record, lookup, lm);
struct rqlEval res;
switch (lv.type)
    {
    case rqlTypeInt:
	res.val.i = (lv.val.i - rv.val.i);
	break;
    case rqlTypeDouble:
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

static struct rqlEval rqlEvalMultiply(struct rqlParse *p, void *record, RqlEvalLookup lookup,
	struct lm *lm)
/* Return a * b. */
{
struct rqlParse *lp = p->children;
struct rqlParse *rp = lp->next;
struct rqlEval lv = rqlLocalEval(lp, record, lookup, lm);
struct rqlEval rv = rqlLocalEval(rp, record, lookup, lm);
struct rqlEval res;
switch (lv.type)
    {
    case rqlTypeInt:
	res.val.i = (lv.val.i * rv.val.i);
	break;
    case rqlTypeDouble:
	res.val.x = (lv.val.x * rv.val.x);
	break;
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
res.type = lv.type;
return res;
}

static struct rqlEval rqlEvalDivide(struct rqlParse *p, void *record, RqlEvalLookup lookup,
	struct lm *lm)
/* Return a / b. */
{
struct rqlParse *lp = p->children;
struct rqlParse *rp = lp->next;
struct rqlEval lv = rqlLocalEval(lp, record, lookup, lm);
struct rqlEval rv = rqlLocalEval(rp, record, lookup, lm);
struct rqlEval res;
switch (lv.type)
    {
    case rqlTypeInt:
	res.val.i = (lv.val.i / rv.val.i);
	break;
    case rqlTypeDouble:
	res.val.x = (lv.val.x / rv.val.x);
	break;
    default:
	internalErr();
	res.val.b = FALSE;
	break;
    }
res.type = lv.type;
return res;
}



static struct rqlEval rqlEvalLike(struct rqlParse *p, void *record, RqlEvalLookup lookup,
	struct lm *lm)
/* Return true if r like l . */
{
struct rqlParse *lp = p->children;
struct rqlParse *rp = lp->next;
struct rqlEval lv = rqlLocalEval(lp, record, lookup, lm);
struct rqlEval rv = rqlLocalEval(rp, record, lookup, lm);
struct rqlEval res;
res.type = rqlTypeBoolean;
assert(rv.type == rqlTypeString);
assert(rv.type == lv.type);
res.val.b = sqlMatchLike(rv.val.s, lv.val.s);
return res;
}

static struct rqlEval rqlEvalArrayIx(struct rqlParse *p, void *record, RqlEvalLookup lookup,
	struct lm *lm)
/* Handle parse tree generated by an indexed array. */
{
struct rqlParse *array = p->children;
struct rqlParse *index = array->next;
struct rqlEval arrayVal = rqlLocalEval(array, record, lookup, lm);
struct rqlEval indexVal = rqlLocalEval(index, record, lookup, lm);
struct rqlEval res;
res.type = rqlTypeString;
res.val.s = emptyForNull(lmCloneSomeWord(lm, arrayVal.val.s, indexVal.val.i));
return res;
}

static struct rqlEval rqlLocalEval(struct rqlParse *p, void *record, RqlEvalLookup lookup, 
	struct lm *lm)
/* Evaluate self on parse tree, allocating memory if needed from lm. */
{
struct rqlEval res;
switch (p->op)
    {
    case rqlOpLiteral:
	res.val = p->val;
	res.type = p->type;
	break;
    case rqlOpSymbol:
	res.type = rqlTypeString;
	char *s = lookup(record, p->val.s);
	if (s == NULL)
	    res.val.s = "";
	else
	    res.val.s = s;
	break;
    case rqlOpEq:
	res = rqlEvalEq(p, record, lookup, lm);
	break;
    case rqlOpNe:
	res = rqlEvalEq(p, record, lookup, lm);
	res.val.b = !res.val.b;
	break;

    /* Inequalities. */
    case rqlOpLt:
        res = rqlEvalLt(p, record, lookup, lm);
	break;
    case rqlOpGt:
        res = rqlEvalGt(p, record, lookup, lm);
	break;
    case rqlOpLe:
        res = rqlEvalGt(p, record, lookup, lm);
	res.val.b = !res.val.b;
	break;
    case rqlOpGe:
        res = rqlEvalLt(p, record, lookup, lm);
	res.val.b = !res.val.b;
	break;
    case rqlOpLike:
        res = rqlEvalLike(p,record, lookup, lm);
	break;

    /* Logical ops. */
    case rqlOpAnd:
	{
        res.type = rqlTypeBoolean;
	res.val.b = TRUE;
	struct rqlParse *c;
	for (c = p->children; c != NULL; c= c->next)
	    {
	    struct rqlEval e = rqlLocalEval(c, record, lookup, lm);
	    if (!e.val.b)
		{
	        res.val.b = FALSE;
		break;
		}
	    }
	break;
	}
    case rqlOpOr:
	{
        res.type = rqlTypeBoolean;
	res.val.b = FALSE;
	struct rqlParse *c;
	for (c = p->children; c != NULL; c= c->next)
	    {
	    struct rqlEval e = rqlLocalEval(c, record, lookup, lm);
	    if (e.val.b)
		{
	        res.val.b = TRUE;
		break;
		}
	    }
	break;
	}

    case rqlOpNot:
        res = rqlLocalEval(p->children, record, lookup, lm);
	res.val.b = !res.val.b;
	break;

    /* Type casts. */
    case rqlOpStringToBoolean:
	res = rqlLocalEval(p->children, record, lookup, lm);
	res.type = rqlTypeBoolean;
	res.val.b = (res.val.s[0] != 0);
	break;
    case rqlOpIntToBoolean:
        res = rqlLocalEval(p->children, record, lookup, lm);
	res.type = rqlTypeBoolean;
	res.val.b = (res.val.i != 0);
	break;
    case rqlOpDoubleToBoolean:
        res = rqlLocalEval(p->children, record, lookup, lm);
	res.type = rqlTypeBoolean;
	res.val.b = (res.val.x != 0.0);
	break;
    case rqlOpStringToInt:
	res = rqlLocalEval(p->children, record, lookup, lm);
	res.type = rqlTypeInt;
	res.val.i = atoll(res.val.s);
	break;
    case rqlOpDoubleToInt:
	res = rqlLocalEval(p->children, record, lookup, lm);
	res.type = rqlTypeInt;
	res.val.i = res.val.x;
	break;

    case rqlOpStringToDouble:
	res = rqlLocalEval(p->children, record, lookup, lm);
	res.type = rqlTypeDouble;
	res.val.x = atof(res.val.s);
	break;
    case rqlOpBooleanToInt:
	res = rqlLocalEval(p->children, record, lookup, lm);
	res.type = rqlTypeInt;
	res.val.i = res.val.b;
	break;
    case rqlOpBooleanToDouble:
	res = rqlLocalEval(p->children, record, lookup, lm);
	res.type = rqlTypeDouble;
	res.val.x = res.val.b;
	break;
    case rqlOpIntToDouble:
	res = rqlLocalEval(p->children, record, lookup, lm);
	res.type = rqlTypeDouble;
	res.val.x = res.val.b;
	break;

    /* Arithmetical negation. */
    case rqlOpUnaryMinusInt:
        res = rqlLocalEval(p->children, record, lookup, lm);
	res.val.i = -res.val.i;
	break;
    case rqlOpUnaryMinusDouble:
        res = rqlLocalEval(p->children, record, lookup, lm);
	res.val.x = -res.val.x;
	break;

    case rqlOpArrayIx:
       res = rqlEvalArrayIx(p, record, lookup, lm);
       break;

    /* Mathematical ops, simple binary type */
    case rqlOpAdd:
       res = rqlEvalAdd(p, record, lookup, lm);
       break;
    case rqlOpSubtract:
       res = rqlEvalSubtract(p, record, lookup, lm);
       break;
    case rqlOpMultiply:
       res = rqlEvalMultiply(p, record, lookup, lm);
       break;
    case rqlOpDivide:
       res = rqlEvalDivide(p, record, lookup, lm);
       break;

    default:
        errAbort("Unknown op %s\n", rqlOpToString(p->op));
	res.type = rqlTypeInt;	// Keep compiler from complaining.
	res.val.i = 0;	// Keep compiler from complaining.
	break;
    }
return res;
}

struct rqlEval rqlEvalOnRecord(struct rqlParse *p, void *record, RqlEvalLookup lookup, 
	struct lm *lm)
/* Evaluate parse tree on record, using lm for memory for string operations. */
{
return rqlLocalEval(p, record, lookup, lm);
}
