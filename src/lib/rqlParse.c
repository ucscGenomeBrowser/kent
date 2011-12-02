/* rqlParse - parse restricted sql-like query language.  Produce rqlParse tree.  See rqlEval.c
 * for the rqlParse interpreter. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "tokenizer.h"
#include "sqlNum.h"
#include "rql.h"


char *rqlOpToString(enum rqlOp op)
/* Return string representation of parse op. */
{
switch (op)
    {
    case rqlOpLiteral:
	return "rqlOpLiteral";
    case rqlOpSymbol:
	return "rqlOpSymbol";
    
    case rqlOpStringToBoolean:
        return "rqlOpStringToBoolean";
    case rqlOpIntToBoolean:
        return "rqlOpIntToBoolean";
    case rqlOpDoubleToBoolean:
        return "rqlOpDoubleToBoolean";
    case rqlOpStringToInt:
        return "rqlOpStringToInt";
    case rqlOpDoubleToInt:
        return "rqlOpDoubleToInt";
    case rqlOpBooleanToInt:
        return "rqlOpBooleanToInt";
    case rqlOpStringToDouble:
        return "rqlOpStringToDouble";
    case rqlOpBooleanToDouble:
        return "rqlOpBooleanToDouble";
    case rqlOpIntToDouble:
        return "rqlOpIntToDouble";

    case rqlOpEq:
	return "rqlOpEq";
    case rqlOpNe:
	return "rqlOpNe";
    case rqlOpGt:
        return "rqlOpGt";
    case rqlOpLt:
        return "rqlOpLt";
    case rqlOpGe:
        return "rqlOpGe";
    case rqlOpLe:
        return "rqlOpLe";
    case rqlOpLike:
	return "rqlOpLike";

    case rqlOpAnd:
	return "rqlOpAnd";
    case rqlOpOr:
	return "rqlOpOr";
    case rqlOpNot:
        return "rqlOpNot";

    case rqlOpUnaryMinusInt:
        return "rqlOpUnaryMinusInt";
    case rqlOpUnaryMinusDouble:
        return "rqlOpUnaryMinusDouble";

    case rqlOpArrayIx:
        return "rqlOpArrayIx";

    default:
	return "rqlOpUnknown";
    }
}

void rqlValDump(union rqlVal val, enum rqlType type, FILE *f)
/* Dump out value to file. */
{
switch (type)
    {
    case rqlTypeBoolean:
        fprintf(f, "%s", (val.b ? "true" : "false") );
	break;
    case rqlTypeString:
        fprintf(f, "%s", val.s);
	break;
    case rqlTypeInt:
        fprintf(f, "%d", val.i);
	break;
    case rqlTypeDouble:
        fprintf(f, "%f", val.x);
	break;
    }
}

void rqlParseDump(struct rqlParse *p, int depth, FILE *f)
/* Dump out rqlParse tree and children. */
{
spaceOut(f, 3*depth);
fprintf(f, "%s ", rqlOpToString(p->op));
rqlValDump(p->val, p->type,  f);
fprintf(f, "\n");
struct rqlParse *child;
for (child = p->children; child != NULL; child= child->next)
    rqlParseDump(child, depth+1, f);
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
if (!sameString(tkz->string, expecting))
    expectingGot(tkz, expecting, tkz->string);
}


struct rqlParse *rqlParseExpression(struct tokenizer *tkz);
/* Parse out a clause, usually a where clause. */

static struct rqlParse *rqlParseAtom(struct tokenizer *tkz)
/* Return low level (symbol or literal) */
{
char *tok = tokenizerMustHaveNext(tkz);
struct rqlParse *p;
AllocVar(p);
char c = tok[0];
if (c == '\'' || c == '"')
    {
    p->op = rqlOpLiteral;
    p->type = rqlTypeString;
    int len = strlen(tok+1);
    p->val.s = cloneStringZ(tok+1, len-1);
    }
else if (isalpha(c) || c == '_')
    {
    p->op = rqlOpSymbol;
    p->type = rqlTypeString;	/* String until promoted at least. */
    p->val.s = cloneString(tok);
    }
else if (isdigit(c))
    {
    p->op = rqlOpLiteral;
    p->type = rqlTypeInt;
    p->val.i = sqlUnsigned(tok);
    if ((tok = tokenizerNext(tkz)) != NULL)
	{
	if (tok[0] == '.')
	    {
	    char buf[32];
	    tok = tokenizerMustHaveNext(tkz);
	    safef(buf, sizeof(buf), "%d.%s", p->val.i, tok);
	    p->type = rqlTypeDouble;
	    p->val.x = sqlDouble(buf);
	    }
	else
	    tokenizerReuse(tkz);
	}
    }
else if (c == '(')
    {
    p = rqlParseExpression(tkz);
    skipOverRequired(tkz, ")");
    }
else
    {
    errAbort("Unexpected %s line %d of %s", tok, tkz->lf->lineIx, tkz->lf->fileName);
    }
return p;
}

static enum rqlType commonTypeForBop(enum rqlType left, enum rqlType right)
/* Return type that will work for a binary operation. */
{
if (left == right)
    return left;
else if (left == rqlTypeDouble || right == rqlTypeDouble)
    return rqlTypeDouble;
else if (left == rqlTypeInt || right == rqlTypeInt)
    return rqlTypeInt;
else if (left == rqlTypeBoolean || right == rqlTypeBoolean)
    return rqlTypeBoolean;
else if (left == rqlTypeString || right == rqlTypeString)
    return rqlTypeString;
else
    {
    errAbort("Can't find commonTypeForBop");
    return rqlTypeInt;
    }
}

static enum rqlOp booleanCastOp(enum rqlType oldType)
/* Return op to convert oldType to boolean. */
{
switch (oldType)
    {
    case rqlTypeString:
        return rqlOpStringToBoolean;
    case rqlTypeInt:
        return rqlOpIntToBoolean;
    case rqlTypeDouble:
        return rqlOpDoubleToBoolean;
    default:
        internalErr();
	return rqlOpUnknown;
    }
}

static enum rqlOp intCastOp(enum rqlType oldType)
/* Return op to convert oldType to int. */
{
switch (oldType)
    {
    case rqlTypeString:
        return rqlOpStringToInt;
    case rqlTypeBoolean:
        return rqlOpBooleanToInt;
    case rqlTypeDouble:
        return rqlOpDoubleToInt;
    default:
        internalErr();
	return rqlOpUnknown;
    }
}

static enum rqlOp doubleCastOp(enum rqlType oldType)
/* Return op to convert oldType to double. */
{
switch (oldType)
    {
    case rqlTypeString:
        return rqlOpStringToDouble;
    case rqlTypeBoolean:
        return rqlOpBooleanToDouble;
    case rqlTypeInt:
        return rqlOpIntToDouble;
    default:
        internalErr();
	return rqlOpUnknown;
    }
}


static struct rqlParse *rqlParseCoerce(struct rqlParse *p, enum rqlType type)
/* If p is not of correct type, wrap type conversion node around it. */
{
if (p->type == type)
    return p;
else
    {
    struct rqlParse *cast;
    AllocVar(cast);
    cast->children = p;
    cast->type = type;
    switch (type)
        {
	case rqlTypeBoolean:
	    cast->op = booleanCastOp(p->type);
	    break;
	case rqlTypeInt:
	    cast->op = intCastOp(p->type);
	    break;
	case rqlTypeDouble:
	    cast->op = doubleCastOp(p->type);
	    break;
	default:
	    internalErr();
	    break;
	}
    return cast;
    }
}

static struct rqlParse *rqlParseIndex(struct tokenizer *tkz)
/* Handle the [] in this[6].  Convert it into tree:
*         rqlOpArrayIx
*            rqlParseAtom
*            rqlParseAtom */
{
struct rqlParse *collection = rqlParseAtom(tkz);
struct rqlParse *p = collection;
char *tok = tokenizerNext(tkz);
if (tok == NULL)
    tokenizerReuse(tkz);
else if (tok[0] == '[')
    {
    // struct rqlParse *index = rqlParseExpression(tkz);
    struct rqlParse *index = rqlParseAtom(tkz);
    index = rqlParseCoerce(index, rqlTypeInt);
    skipOverRequired(tkz, "]");
    AllocVar(p);
    p->op = rqlOpArrayIx;
    p->type = rqlTypeString;
    p->children = collection;
    collection->next = index;
    }
else
    tokenizerReuse(tkz);
return p;
}


static struct rqlParse *rqlParseUnaryMinus(struct tokenizer *tkz)
/* Return unary minus sort of parse tree if there's a leading '-' */
{
char *tok = tokenizerMustHaveNext(tkz);
if (tok[0] == '-')
    {
    struct rqlParse *c = rqlParseIndex(tkz);
    struct rqlParse *p;
    AllocVar(p);
    if (c->type == rqlTypeInt)
        {
	p->op = rqlOpUnaryMinusInt;
	p->type = rqlTypeInt;
	}
    else
	{
	c = rqlParseCoerce(c, rqlTypeDouble);
	p->op = rqlOpUnaryMinusDouble;
	p->type = rqlTypeDouble;
	}
    p->children = c;
    return p;
    }
else
    {
    tokenizerReuse(tkz);
    return rqlParseIndex(tkz);
    }
}

static boolean eatMatchingTok(struct tokenizer *tkz, char *s)
/* If next token matches s then eat it and return TRUE */
{
char *tok = tokenizerNext(tkz);
if (tok != NULL && sameString(tok, s))
    return TRUE;
else
    {
    tokenizerReuse(tkz);
    return FALSE;
    }
}

static struct rqlParse *rqlParseCmp(struct tokenizer *tkz)
/* Parse out comparison. */
{
struct rqlParse *l = rqlParseUnaryMinus(tkz);
struct rqlParse *p = l;
char *tok = tokenizerNext(tkz);
boolean forceString = FALSE;
boolean needNot = FALSE;
if (tok != NULL)
    {
    enum rqlOp op = rqlOpUnknown;
    if (sameString(tok, "="))
        {
	op = rqlOpEq;
	}
    else if (sameString(tok, "!"))
        {
	op = rqlOpNe;
	skipOverRequired(tkz, "=");
	}
    else if (sameString(tok, ">"))
        {
	if (eatMatchingTok(tkz, "="))
	    op = rqlOpGe;
	else
	    op = rqlOpGt;
	}
    else if (sameString(tok, "<"))
        {
	if (eatMatchingTok(tkz, "="))
	    op = rqlOpGe;
	else
	    op = rqlOpLe;
	}
    else if (sameString(tok, "not"))
        {
	forceString = TRUE;
	op = rqlOpLike;
	needNot = TRUE;
	skipOverRequired(tkz, "like");
	}
    else if (sameString(tok, "like"))
        {
	forceString = TRUE;
	op = rqlOpLike;
	}
    else
        {
	tokenizerReuse(tkz);
	return p;
	}
    struct rqlParse *r = rqlParseUnaryMinus(tkz);
    AllocVar(p);
    p->op = op;
    p->type = rqlTypeBoolean;

    /* Now force children to be the same type, inserting casts if need be. */
    if (forceString)
	{
	if (l->type != rqlTypeString || r->type != rqlTypeString)
	    {
	    errAbort("Expecting string type around comparison line %d of %s",
	    	tkz->lf->lineIx, tkz->lf->fileName);
	    }
	}
    else
	{
	enum rqlType childType = commonTypeForBop(l->type, r->type);
	l = rqlParseCoerce(l, childType);
	r = rqlParseCoerce(r, childType);
	}

    /* Now hang children onto node. */
    p->children = l;
    l->next = r;

    /* Put in a not around self if need be. */
    if (needNot)
        {
	struct rqlParse *n;
	AllocVar(n);
	n->op = rqlOpNot;
	n->type = rqlTypeBoolean;
	n->children = p;
	p = n;
	}
    }
return p;
}

static struct rqlParse *rqlParseNot(struct tokenizer *tkz)
/* parse out a logical not. */
{
char *tok = tokenizerNext(tkz);
if (sameString(tok, "not"))
    {
    struct rqlParse *p = rqlParseCoerce(rqlParseCmp(tkz), rqlTypeBoolean);
    struct rqlParse *n;
    AllocVar(n);
    n->op = rqlOpNot;
    n->type = rqlTypeBoolean;
    n->children = p;
    return n;
    }
else
    {
    tokenizerReuse(tkz);
    return rqlParseCmp(tkz);
    }
}

static struct rqlParse *rqlParseAnd(struct tokenizer *tkz)
/* Parse out and or or. */
{
struct rqlParse *p = rqlParseNot(tkz);
struct rqlParse *parent = NULL;
for (;;)
    {
    char *tok = tokenizerNext(tkz);
    if (tok == NULL || !sameString(tok, "and"))
        {
	tokenizerReuse(tkz);
	return p;
	}
    else
        {
	if (parent == NULL)
	    {
	    p = rqlParseCoerce(p, rqlTypeBoolean);
	    AllocVar(parent);
	    parent->op = rqlOpAnd;
	    parent->type = rqlTypeBoolean;
	    parent->children = p;
	    p = parent;
	    }
	struct rqlParse *r = rqlParseCoerce(rqlParseNot(tkz), rqlTypeBoolean);
	slAddTail(&parent->children, r);
	}
    }
}

static struct rqlParse *rqlParseOr(struct tokenizer *tkz)
/* Parse out and or or. */
{
struct rqlParse *p = rqlParseAnd(tkz);
struct rqlParse *parent = NULL;
for (;;)
    {
    char *tok = tokenizerNext(tkz);
    if (tok == NULL || !sameString(tok, "or"))
        {
	tokenizerReuse(tkz);
	return p;
	}
    else
        {
	if (parent == NULL)
	    {
	    p = rqlParseCoerce(p, rqlTypeBoolean);
	    AllocVar(parent);
	    parent->op = rqlOpOr;
	    parent->type = rqlTypeBoolean;
	    parent->children = p;
	    p = parent;
	    }
	struct rqlParse *r = rqlParseCoerce(rqlParseAnd(tkz), rqlTypeBoolean);
	slAddTail(&parent->children, r);
	}
    }
}

struct rqlParse *rqlParseExpression(struct tokenizer *tkz)
/* Parse out a clause, usually a where clause. */
{
return rqlParseOr(tkz);
}

static char *rqlParseFieldSpec(struct tokenizer *tkz, struct dyString *buf)
/* Return a field spec, which may contain * and ?. Put results in buf, and 
 * return buf->string. */
{
boolean firstTime = TRUE;
dyStringClear(buf);
for (;;)
   {
   char *tok = tokenizerNext(tkz);
   if (tok == NULL)
       break;
   char c = *tok;
   if (c == '?' || c == '*' || isalpha(c) || c == '_' || c == '/' || c == '.')
       {
       if (firstTime)
	   dyStringAppend(buf, tok);
       else
           {
	   if (tkz->leadingSpaces == 0)
	       dyStringAppend(buf, tok);
	   else
	       {
	       tokenizerReuse(tkz);
	       break;
	       }
	   }
       }
   else
       {
       tokenizerReuse(tkz);
       break;
       }
    firstTime = FALSE;
    }
if (buf->stringSize == 0)
    errAbort("Expecting field name line %d of %s", tkz->lf->lineIx, tkz->lf->fileName);
return buf->string;
}


void rqlParseVarsUsed(struct rqlParse *p, struct slName **pVarList)
/* Put variables used by self and children onto varList. */
{
if (p->op == rqlOpSymbol)
    slNameStore(pVarList, p->val.s);
struct rqlParse *child;
for (child = p->children; child != NULL; child = child->next)
    rqlParseVarsUsed(child, pVarList);
}

struct rqlStatement *rqlStatementParse(struct lineFile *lf)
/* Parse an RQL statement out of text */
{
struct tokenizer *tkz = tokenizerOnLineFile(lf);
tkz->uncommentShell = TRUE;
tkz->uncommentC = TRUE;
tkz->leaveQuotes = TRUE;
struct rqlStatement *rql;
AllocVar(rql);
rql->command = cloneString(tokenizerMustHaveNext(tkz));
if (sameString(rql->command, "select"))
    {
    struct dyString *buf = dyStringNew(0);
    struct slName *list = NULL;
    char *tok = rqlParseFieldSpec(tkz, buf);
    /* Look for count(*) as special case. */
    boolean countOnly = FALSE;
    if (sameString(tok, "count"))
        {
	char *paren = tokenizerNext(tkz);
	if (paren[0] == '(')
	    {
	    while ((paren = tokenizerMustHaveNext(tkz)) != NULL)
	        {
		if (paren[0] == ')')
		    break;
		}
	    countOnly = TRUE;
	    freez(&rql->command);
	    rql->command = cloneString("count");
	    }
	else
	    {
	    tokenizerReuse(tkz);
	    }
	}
    if (!countOnly)
	{
	list = slNameNew(tok);
	for (;;)
	    {
	    /* Parse out comma-separated field list. */
	    char *comma = tokenizerNext(tkz);
	    if (comma == NULL || comma[0] != ',')
		{
		tokenizerReuse(tkz);
		break;
		}
	    slNameAddHead(&list, rqlParseFieldSpec(tkz, buf));
	    }
	slReverse(&list);
	rql->fieldList = list;
	}
    dyStringFree(&buf);
    }
else if (sameString(rql->command, "count"))
    {
    /* No parameters to count. */
    }
else
    errAbort("Unknown RQL command '%s line %d of %s\n", rql->command, lf->lineIx, lf->fileName);
    
char *from = tokenizerNext(tkz);
if (from != NULL)
    {
    if (sameString(from, "from"))
        {
	for (;;)
	    {
	    struct dyString *buf = dyStringNew(0);
	    char *table = rqlParseFieldSpec(tkz, buf);
	    slNameAddTail(&rql->tableList, table);
	    char *comma = tokenizerNext(tkz);
	    if (comma == NULL)
	        break;
	    if (comma[0] != ',')
	        {
		tokenizerReuse(tkz);
		break;
		}
	    dyStringFree(&buf);
	    }
	}
    else
	{
	errAbort("missing 'from' clause in %s\n", rql->command);
	}
    }

/* Parse where clause. */
char *where = tokenizerNext(tkz);
if (where != NULL)
    {
    if (!sameString(where, "where"))
	{
        tokenizerReuse(tkz);
	}
    else
        {
	rql->whereClause = rqlParseExpression(tkz);
	rqlParseVarsUsed(rql->whereClause, &rql->whereVarList);
	}
    }

/* Parse limit clause. */
char *limit = tokenizerNext(tkz);
rql->limit = -1;	
if (limit != NULL)
    {
    if (!sameString(limit, "limit"))
        errAbort("Unknown clause '%s' line %d of %s", limit, lf->lineIx, lf->fileName);
    char *count = tokenizerMustHaveNext(tkz);
    if (!isdigit(count[0]))
       errAbort("Expecting number after limit, got %s line %d of %s", 
       	count, lf->lineIx, lf->fileName);
    rql->limit = atoi(count);
    }

/* Check that are at end of statement. */
char *extra = tokenizerNext(tkz);
if (extra != NULL)
    errAbort("Extra stuff starting with '%s' past end of statement line %d of %s", 
    	extra, lf->lineIx, lf->fileName);
return rql;
}

void rqlStatementDump(struct rqlStatement *rql, FILE *f)
/* Print out statement to file. */
{
fprintf(f, "%s:", rql->command);
if (rql->fieldList)
    {
    fprintf(f, " ");
    struct slName *field = rql->fieldList;
    fprintf(f, "%s", field->name);
    for (field = field->next; field != NULL; field = field->next)
        fprintf(f, ",%s", field->name);
    }
fprintf(f, "\n");
if (rql->tableList)
    {
    fprintf(f, "from: ");
    struct slName *table = rql->tableList;
    fprintf(f, "%s", table->name);
    for (table = table->next; table != NULL; table = table->next)
        fprintf(f, ",%s", table->name);
    fprintf(f, "\n");
    }
if (rql->whereClause)
    {
    fprintf(f, " where:\n");
    rqlParseDump(rql->whereClause, 0, f);
    }
if (rql->limit >= 0)
    fprintf(f, "limit: %d\n", rql->limit);
}

static void rqlParseFreeRecursive(struct rqlParse *p)
/* Depth-first recursive free. */
{
struct rqlParse *child, *next;
for (child = p->children; child != NULL; child = next)
    {
    next = child->next;
    rqlParseFreeRecursive(child);
    }
freeMem(p);
}

void rqlStatementFree(struct rqlStatement **pRql)
/* Free up an rql statement. */
{
struct rqlStatement *rql = *pRql;
if (rql != NULL)
    {
    freeMem(rql->command);
    slFreeList(&rql->fieldList);
    slFreeList(&rql->tableList);
    if (rql->whereClause !=NULL)
	rqlParseFreeRecursive(rql->whereClause);
    slFreeList(&rql->whereVarList);
    freez(pRql);
    }
}

