/* raSqlQuery - Do a SQL-like query on a RA file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "obscure.h"
#include "ra.h"
#include "localmem.h"
#include "tokenizer.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: raSqlQuery.c,v 1.6 2009/11/20 04:57:14 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "raSqlQuery - Do a SQL-like query on a RA file.\n"
  "usage:\n"
  "   raSqlQuery raFile(s) query-options\n"
  "One of the following query-options must be specified\n"
  "   -queryFile=fileName\n"
  "   \"-query=select list,of,fields where field='this'\"\n"
  "The queryFile just has a query in it in the same form as the query option, but\n"
  "without needing the quotes necessarily.\n"
  "  The syntax of a query statement is very SQL-like.  It must begin with either\n"
  "'select' or 'count'.  Select is followed by a field list, or '*' for all fields\n"
  "Count is not followed by anything.  The 'where' clause is optional, and if it\n"
  "exists it can contain expressions involving fields, numbers, strings, arithmetic, 'and'\n"
  "'or' and so forth.\n"
  "Other options:\n"
  "   -merge=keyField - If there are multiple raFiles, records with the same keyField will be\n"
  "          merged together with fields in later files overriding fields in earlier files\n"
  "   -skipMissing - If merging, skip records without keyfield rather than abort\n"
  "The output will be to stdout, in the form of a .ra file if the select command is used\n"
  "and just a simple number if the count command is used\n"
  );
}

static char *clQueryFile = NULL;
static char *clQuery = NULL;
static char *clMerge = NULL;
static boolean clSkipMissing = FALSE;

static struct optionSpec options[] = {
   {"queryFile", OPTION_STRING},
   {"query", OPTION_STRING},
   {"merge", OPTION_STRING},
   {"skipMissing", OPTION_BOOLEAN},
   {NULL, 0},
};

struct raField
/* A single field. */
    {
    struct raField *next;	/* Next in list. */
    char *name;		/* Field name. */
    char *val;	/* Field value. */
    };

struct raRecord
/* A single RA record. */
    {
    struct raRecord *next;	/* Next in list. */
    struct raField *key;		/* Key field if any. */
    struct raField *fieldList;	/* List of fields. */
    };

enum rqlParseOp
    {
    rqlParseUnknown,	/* Should not occur */
    rqlParseLiteral,        /* Literal string or number. */
    rqlParseSymbol,	/* A symbol name. */
    rqlParseEq,	/* An equals comparison */
    rqlParseNe,	/* A not equals comparison */

    rqlParseStringToBoolean,
    rqlParseStringToInt,
    rqlParseStringToDouble,
    rqlParseBooleanToInt,
    rqlParseBooleanToDouble,
    rqlParseIntToDouble,

    rqlParseUnaryMinusDouble,

    rqlParseGt,  /* Greater than comparison. */
    rqlParseLt,  /* Less than comparison. */
    rqlParseGe,  /* Greater than or equals comparison. */
    rqlParseLe,  /* Less than or equals comparison. */
    rqlParseLike, /* SQL wildcard compare. */

    rqlParseAnd,     /* An and */
    rqlParseOr,      /* An or */
    };

char *rqlParseOpToString(enum rqlParseOp op)
/* Return string representation of parse op. */
{
switch (op)
    {
    case rqlParseLiteral:
	return "rqlParseLiteral";
    case rqlParseSymbol:
	return "rqlParseSymbol";
    case rqlParseEq:
	return "rqlParseEq";
    case rqlParseNe:
	return "rqlParseNe";
    case rqlParseAnd:
	return "rqlParseAnd";
    case rqlParseOr:
	return "rqlParseOr";
    
    case rqlParseStringToBoolean:
        return "rqlParseStringToBoolean";
    case rqlParseStringToInt:
        return "rqlParseStringToInt";
    case rqlParseStringToDouble:
        return "rqlParseStringToDouble";
    case rqlParseBooleanToInt:
        return "rqlParseBooleanToInt";
    case rqlParseBooleanToDouble:
        return "rqlParseBooleanToDouble";
    case rqlParseIntToDouble:
        return "rqlParseIntToDouble";

    case rqlParseUnaryMinusDouble:
        return "rqlParseUnaryMinusDouble";

    case rqlParseGt:
        return "rqlParseGt";
    case rqlParseLt:
        return "rqlParseLt";
    case rqlParseGe:
        return "rqlParseGe";
    case rqlParseLe:
        return "rqlParseLe";
    case rqlParseLike:
	return "rqlParseLike";

    default:
	return "rqlParseUnknown";
    }
}

enum rqlType
/* A type */
    {
    rqlTypeBoolean = 1,
    rqlTypeString = 2,
    rqlTypeInt = 3,
    rqlTypeDouble = 4,
    };

union rqlVal
/* Some value of arbirary type that can be of any type corresponding to rqlType */
    {
    boolean b;
    char *s;
    int i;
    double x;
    };

struct rqlEval
/* Result of evaluation of parse tree. */
    {
    enum rqlType type;
    union rqlVal val;
    };

struct rqlParse
/* A rql parse-tree. */
    {
    struct rqlParse *next;	/* Points to younger sibling if any. */
    struct rqlParse *children;	/* Points to oldest child if any. */
    enum rqlParseOp op;		/* Operation at this node. */
    enum rqlType type;		/* Return type of this operation. */
    union rqlVal val;		/* Return value of this operation. */
    };

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

struct rqlStatement
/* A parsed out RQL statement */
    {
    char *next;		/* Next in list */
    char *command;	/* Generally the first word in the statement. */
    struct slName *fieldList;	/* List of fields if any. */
    struct rqlParse *whereClause;	/* Where clause if any - tokenized. */
    };

void rqlParseDump(struct rqlParse *p, int depth, FILE *f)
/* Dump out rqlParse tree and children. */
{
spaceOut(f, 3*depth);
fprintf(f, "%s ", rqlParseOpToString(p->op));
rqlValDump(p->val, p->type,  f);
fprintf(f, "\n");
struct rqlParse *child;
for (child = p->children; child != NULL; child= child->next)
    rqlParseDump(child, depth+1, f);
}

struct rqlParse *rqlParseAtom(struct tokenizer *tkz)
/* Return low level (symbol or literal) */
{
char *tok = tokenizerMustHaveNext(tkz);
struct rqlParse *p;
AllocVar(p);
char c = tok[0];
if (c == '\'' || c == '"')
    {
    p->op = rqlParseLiteral;
    p->type = rqlTypeString;
    int len = strlen(tok+1);
    p->val.s = cloneStringZ(tok+1, len-1);
    }
else if (isalpha(c) || c == '_')
    {
    p->op = rqlParseSymbol;
    p->type = rqlTypeString;	/* String until promoted at least. */
    p->val.s = cloneString(tok);
    }
else if (isdigit(c))
    {
    p->op = rqlParseLiteral;
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
else
    {
    errAbort("Unexpected %s line %d of %s", tok, tkz->lf->lineIx, tkz->lf->fileName);
    }
return p;
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

enum rqlType commonTypeForBop(enum rqlType left, enum rqlType right)
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

enum rqlParseOp booleanCastOp(enum rqlType oldType)
/* Return op to convert oldType to boolean. */
{
switch (oldType)
    {
    case rqlTypeString:
        return rqlParseStringToBoolean;
    default:
        internalErr();
	return rqlParseUnknown;
    }
}

enum rqlParseOp intCastOp(enum rqlType oldType)
/* Return op to convert oldType to int. */
{
switch (oldType)
    {
    case rqlTypeString:
        return rqlParseStringToInt;
    case rqlTypeBoolean:
        return rqlParseBooleanToInt;
    default:
        internalErr();
	return rqlParseUnknown;
    }
}

enum rqlParseOp doubleCastOp(enum rqlType oldType)
/* Return op to convert oldType to double. */
{
switch (oldType)
    {
    case rqlTypeString:
        return rqlParseStringToDouble;
    case rqlTypeBoolean:
        return rqlParseBooleanToDouble;
    case rqlTypeInt:
        return rqlParseIntToDouble;
    default:
        internalErr();
	return rqlParseUnknown;
    }
}


struct rqlParse *rqlParseCoerce(struct rqlParse *p, enum rqlType type)
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

struct rqlParse *rqlParseUnaryMinus(struct tokenizer *tkz)
/* Return unary minus sort of parse tree if there's a leading '-' */
{
char *tok = tokenizerMustHaveNext(tkz);
if (tok[0] == '-')
    {
    struct rqlParse *c = rqlParseAtom(tkz);
    c = rqlParseCoerce(c, rqlTypeDouble);
    struct rqlParse *p;
    AllocVar(p);
    p->op = rqlParseUnaryMinusDouble;
    p->type = rqlTypeDouble;
    p->children = c;
    return p;
    }
else
    {
    tokenizerReuse(tkz);
    return rqlParseAtom(tkz);
    }
}

boolean eatMatchingTok(struct tokenizer *tkz, char *s)
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

struct rqlParse *rqlParseCmp(struct tokenizer *tkz)
/* Parse out comparison. */
{
struct rqlParse *l = rqlParseUnaryMinus(tkz);
struct rqlParse *p = l;
char *tok = tokenizerNext(tkz);
boolean forceString = FALSE;
if (tok != NULL)
    {
    enum rqlParseOp op = rqlParseUnknown;
    if (sameString(tok, "="))
        {
	op = rqlParseEq;
	}
    else if (sameString(tok, "!"))
        {
	op = rqlParseNe;
	skipOverRequired(tkz, "=");
	}
    else if (sameString(tok, ">"))
        {
	if (eatMatchingTok(tkz, "="))
	    op = rqlParseGe;
	else
	    op = rqlParseGt;
	}
    else if (sameString(tok, "<"))
        {
	if (eatMatchingTok(tkz, "="))
	    op = rqlParseGe;
	else
	    op = rqlParseLe;
	}
    else if (sameString(tok, "like"))
        {
	forceString = TRUE;
	op = rqlParseLike;
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
    }
return p;
}

struct rqlParse *rqlParseClause(struct tokenizer *tkz)
/* Parse out a clause, usually a where clause. */
{
return rqlParseCmp(tkz);
}

char *rqlParseFieldSpec(struct tokenizer *tkz, struct dyString *buf)
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
   if (c == '?' || c == '*' || isalpha(c) || c == '_')
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

void rqlStatementDump(struct rqlStatement *rql, FILE *f)
/* Print out statement to file. */
{
fprintf(f, "%s", rql->command);
if (rql->fieldList)
    {
    fprintf(f, " ");
    struct slName *field = rql->fieldList;
    fprintf(f, "%s", field->name);
    for (field = field->next; field != NULL; field = field->next)
        fprintf(f, ",%s", field->name);
    }
if (rql->whereClause)
    {
    fprintf(f, " where:\n");
    rqlParseDump(rql->whereClause, 0, f);
    }
fprintf(f, "\n");
}

struct rqlEval rqlEvalOnRecord(struct rqlParse *p, struct raRecord *ra);
/* Evaluate self on ra. */

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

struct raField *raRecordField(struct raRecord *ra, char *fieldName)
/* Return named field if it exists, otherwise NULL */
{
struct raField *field;
for (field = ra->fieldList; field != NULL; field = field->next)
    {
    if (sameString(field->name, fieldName))
        return field;
    }
return NULL;
}

struct rqlEval rqlEvalEq(struct rqlParse *p, struct raRecord *ra)
/* Return true if two children are equal, doing some casting if need be. */
{
struct rqlParse *lp = p->children;
struct rqlParse *rp = lp->next;
struct rqlEval lv = rqlEvalOnRecord(lp, ra);
struct rqlEval rv = rqlEvalOnRecord(rp, ra);
struct rqlEval res;
res.type = rqlTypeBoolean;
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

struct rqlEval rqlEvalLt(struct rqlParse *p, struct raRecord *ra)
/* Return true if r < l . */
{
struct rqlParse *lp = p->children;
struct rqlParse *rp = lp->next;
struct rqlEval lv = rqlEvalOnRecord(lp, ra);
struct rqlEval rv = rqlEvalOnRecord(rp, ra);
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

struct rqlEval rqlEvalGt(struct rqlParse *p, struct raRecord *ra)
/* Return true if r > l . */
{
struct rqlParse *lp = p->children;
struct rqlParse *rp = lp->next;
struct rqlEval lv = rqlEvalOnRecord(lp, ra);
struct rqlEval rv = rqlEvalOnRecord(rp, ra);
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

struct rqlEval rqlEvalLike(struct rqlParse *p, struct raRecord *ra)
/* Return true if r like l . */
{
struct rqlParse *lp = p->children;
struct rqlParse *rp = lp->next;
struct rqlEval lv = rqlEvalOnRecord(lp, ra);
struct rqlEval rv = rqlEvalOnRecord(rp, ra);
struct rqlEval res;
res.type = rqlTypeBoolean;
assert(rv.type == rqlTypeString);
assert(rv.type == lv.type);
res.val.b = sqlMatchLike(rv.val.s, lv.val.s);
return res;
}

struct rqlEval rqlEvalOnRecord(struct rqlParse *p, struct raRecord *ra)
/* Evaluate self on ra. */
{
struct rqlEval res;
switch (p->op)
    {
    case rqlParseLiteral:
	res.val = p->val;
	res.type = p->type;
	break;
    case rqlParseSymbol:
	res.type = rqlTypeString;
	struct raField *f = raRecordField(ra, p->val.s);
	if (f == NULL)
	    res.val.s = "";
	else
	    res.val.s = f->val;
	break;
    case rqlParseEq:
	res = rqlEvalEq(p, ra);
	break;
    case rqlParseNe:
	res = rqlEvalEq(p, ra);
	res.val.b = !res.val.b;
	break;

    /* Inequalities. */
    case rqlParseLt:
        res = rqlEvalLt(p, ra);
	break;
    case rqlParseGt:
        res = rqlEvalGt(p, ra);
	break;
    case rqlParseLe:
        res = rqlEvalGt(p, ra);
	res.val.b = !res.val.b;
	break;
    case rqlParseGe:
        res = rqlEvalLt(p, ra);
	res.val.b = !res.val.b;
	break;
    case rqlParseLike:
        res = rqlEvalLike(p,ra);
	break;

    /* Type casts. */
    case rqlParseStringToBoolean:
	res = rqlEvalOnRecord(p->children, ra);
	res.type = rqlTypeBoolean;
	res.val.b = (res.val.s[0] != 0);
	break;
    case rqlParseStringToInt:
	res = rqlEvalOnRecord(p->children, ra);
	res.type = rqlTypeInt;
	res.val.i = atoi(res.val.s);
	break;
    case rqlParseStringToDouble:
	res = rqlEvalOnRecord(p->children, ra);
	res.type = rqlTypeDouble;
	res.val.x = atof(res.val.s);
	break;
    case rqlParseBooleanToInt:
	res = rqlEvalOnRecord(p->children, ra);
	res.type = rqlTypeInt;
	res.val.i = res.val.b;
	break;
    case rqlParseBooleanToDouble:
	res = rqlEvalOnRecord(p->children, ra);
	res.type = rqlTypeDouble;
	res.val.x = res.val.b;
	break;
    case rqlParseIntToDouble:
	res = rqlEvalOnRecord(p->children, ra);
	res.type = rqlTypeDouble;
	res.val.x = res.val.b;
	break;

    case rqlParseUnaryMinusDouble:
        res = rqlEvalOnRecord(p->children, ra);
	res.type = rqlTypeDouble;
	res.val.x = -res.val.x;
	break;

    default:
        errAbort("Unknown op %s\n", rqlParseOpToString(p->op));
	res.type = rqlTypeInt;	// Keep compiler from complaining.
	res.val.i = 0;	// Keep compiler from complaining.
	break;
    }
return res;
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
	struct slName *field = slNameAddHead(&list, rqlParseFieldSpec(tkz, buf));
	}
    slReverse(&list);
    rql->fieldList = list;
    dyStringFree(&buf);
    }
else if (sameString(rql->command, "count"))
    {
    /* No parameters to count. */
    }
else
    errAbort("Unknown RQL command '%s line %d of %s\n", rql->command, lf->lineIx, lf->fileName);
    
char *where = tokenizerNext(tkz);
if (where != NULL)
    {
    if (!sameString(where, "where"))
        errAbort("Unknown clause '%s' line %d of %s", where, lf->lineIx, lf->fileName);
    rql->whereClause = rqlParseClause(tkz);
    }

char *extra = tokenizerNext(tkz);
if (extra != NULL)
    errAbort("Extra stuff starting with '%s' past end of statement line %d of %s", 
    	extra, lf->lineIx, lf->fileName);
return rql;
}

struct raField *raFieldFromLine(char *line, struct lm *lm)
/* Parse out line and convert it to a raField.  Will return NULL on empty lines. 
 * Will insert some zeroes into the input line as well. */
{
char *word = nextWord(&line);
if (word == NULL)
    return NULL;
struct raField *field;
lmAllocVar(lm, field);
field->name = lmCloneString(lm, word);
char *val = emptyForNull(skipLeadingSpaces(line));
field->val = lmCloneString(lm, val);
return field;
}

struct raRecord *raRecordReadOne(struct lineFile *lf, struct lm *lm)
/* Read next record from file. Returns NULL at end of file. */
{
struct raField *field, *fieldList = NULL;
char *line, *word;

/* Read first line and start fieldList on it. */
if (!lineFileNextReal(lf, &line))
    return NULL;
fieldList = raFieldFromLine(line, lm);

/* Keep going until get a blank line. */
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        break;
    field = raFieldFromLine(line, lm);
    if (field == NULL)
        break;
    slAddHead(&fieldList, field);
    }

slReverse(&fieldList);
struct raRecord *record;
lmAllocVar(lm, record);
record->fieldList = fieldList;
return record;
}

static struct raRecord *readRaRecords(int inCount, char *inNames[], char *mergeField, struct lm *lm)
/* Scan through files, merging records on mergeField if it is non-NULL. */
{
if (inCount <= 0)
    return NULL;
if (mergeField)
    {
    errAbort("mergeField not yet supported");
    return NULL;
    }
else
    {
    struct raRecord *recordList = NULL;
    int i;
    for (i=0; i<inCount; ++i)
        {
	char *fileName = inNames[i];
	struct lineFile *lf = lineFileOpen(fileName, TRUE);
	struct raRecord *record;
	while ((record = raRecordReadOne(lf, lm)) != NULL)
	    {
	    slAddHead(&recordList, record);
	    }
	lineFileClose(&lf);
	}
    slReverse(&recordList);
    return recordList;
    }
}

boolean rqlStatementMatch(struct rqlStatement *rql, struct raRecord *ra)
/* Return TRUE if where clause in statement evaluates true for ra. */
{
struct rqlParse *whereClause = rql->whereClause;
if (whereClause == NULL)
    return TRUE;
else
    {
    struct rqlEval res = rqlEvalOnRecord(whereClause, ra);
    res = rqlEvalCoerceToBoolean(res);
    return res.val.b;
    }
}

void rqlStatementOutput(struct rqlStatement *rql, struct raRecord *ra, FILE *out)
/* Output fields  from ra to file. */
{
struct slName *fieldList = rql->fieldList, *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    struct raField *r;
    boolean doWild = anyWild(field->name);
    for (r = ra->fieldList; r != NULL; r = r->next)
        {
	boolean match;
	if (doWild)
	    match = wildMatch(field->name, r->name);
	else
	    match = (strcmp(field->name, r->name) == 0);
	if (match)
	    fprintf(out, "%s %s\n", r->name, r->val);
	}
    }
fprintf(out, "\n");
}

void raSqlQuery(int inCount, char *inNames[], struct lineFile *query, char *mergeField, struct lm *lm,
	FILE *out)
/* raSqlQuery - Do a SQL-like query on a RA file.. */
{
struct raRecord *raList = readRaRecords(inCount, inNames, mergeField, lm);
struct rqlStatement *rql = rqlStatementParse(query);
verbose(2, "Got %d records in raFiles\n", slCount(raList));
if (verboseLevel() > 1)
    rqlStatementDump(rql, stderr);
struct raRecord *ra;
int matchCount = 0;
boolean doSelect = sameString(rql->command, "select");
for (ra = raList; ra != NULL; ra = ra->next)
    {
    if (rqlStatementMatch(rql, ra))
        {
	matchCount += 1;
	if (doSelect)
	    {
	    rqlStatementOutput(rql, ra, out);
	    }
	}
    }
if (!doSelect)
    printf("%d\n", matchCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
clMerge = optionVal("merge", NULL);
clSkipMissing = optionExists("skipMissing");
clQueryFile = optionVal("queryFile", NULL);
clQuery = optionVal("query", NULL);
if (clQueryFile == NULL && clQuery == NULL)
    errAbort("Please specify either the query or queryFile option.");
if (clQueryFile != NULL && clQuery != NULL)
    errAbort("Please specify just one of the query or queryFile options.");
struct lineFile *query = NULL;
if (clQuery)
    query = lineFileOnString("query", TRUE, cloneString(clQuery));
else
    query = lineFileOpen(clQueryFile, TRUE);
struct lm *lm = lmInit(0);
raSqlQuery(argc-1, argv+1, query, clMerge, lm, stdout);
return 0;
}
