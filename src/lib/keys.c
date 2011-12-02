/* keys.c - Stuff to manage a little key/value pair table and
 * evaluate expressions on it. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "keys.h"
#include "kxTok.h"


struct kvt
/* Key/value table. */
    {
    int used;             /* Number of keys used in table. */
    int alloced;          /* Number allocated in table. */
    struct keyVal *table; /* The table itself. */
    };

struct kvt *newKvt(int size)
/* Get a new key value table. */
{
struct kvt *kvt;
AllocVar(kvt);
kvt->alloced = size;
kvt->table = needMem(size * sizeof(kvt->table[0]));
return kvt;
}


void freeKvt(struct kvt **pKvt)
/* Free up key value table. */
{
struct kvt *kvt;
if ((kvt = *pKvt) != NULL)
    {
    freeMem(kvt->table);
    freez(pKvt);
    }
}

void kvtClear(struct kvt *kvt)
/* Clear the keys table. */
{
kvt->used = 0;
}

struct keyVal *kvtAdd(struct kvt *kvt, char *key, char *val)
/* Add in new key. */
{
struct keyVal *kv;
if (kvt->used == kvt->alloced)
    errAbort("Too many keys in keyVal(%s %s)", key, val);
kv = &kvt->table[kvt->used++];
kv->key = key;
kv->val = val;
return kv;
}

void kvtParseAdd(struct kvt *kvt, char *text)
/* Add in keys from text.  Text is in format:
 *     key val
 * for each line of text. Text gets many of it's
 * space characters and newlines replaced by 0's
 * and should persist until call to keysClear(). */
{
char *lines[256];
int lineCount;
int i;
char *k, *v;

lineCount = chopString(text, "\n\r", lines, ArraySize(lines));
for (i=0; i<lineCount; ++i)
    {
    k = lines[i];
    if ((v = strchr(k, ' ')) != NULL)
        {
        *v++ = 0;
        kvtAdd(kvt, k, v);
        }
    }
}

struct keyVal* kvtGet(struct kvt *kvt, char *key)
/* get the keyVal for the specified key, of NULL if not found. */
{
int i;
struct keyVal *keyTable = kvt->table;
int keysUsed = kvt->used;

for (i=0; i<keysUsed; ++i)
    {
    if (sameString(key, keyTable[i].key))
        return &keyTable[i];
    }
return NULL;
}

char *kvtLookup(struct kvt *kvt, char *key)
/* Search table for key.  Return key value, or NULL if
 * key not found. */
{
struct keyVal *keyVal = kvtGet(kvt, key);
if (keyVal == NULL)
    return NULL;
else
    return keyVal->val;
}

void kvtWriteAll(struct kvt *kvt, FILE *f, struct slName *hideList)
/* Write all keys to file except the ones in hideList */
{
int i;
static char lf = '\n';
struct keyVal *kv = kvt->table;
int keyCount = kvt->used;

for (i=0; i<keyCount; ++i)
    {
    char *key = kv->key;
    if (kv->val != NULL && !slNameInList(hideList, key))
        fprintf(f, "%s %s\n", key, kv->val);
    ++kv;
    }
mustWrite(f, &lf, 1);   /* Instead of fputc for error checking. */
}


/* Expression evaluator - evaluates a parsed tree. */
enum keyExpType
    {
    kxMatch,
    kxWildMatch,
    kxGT,      /* Greater Than */
    kxGE,      /* Greater Than or Equal */
    kxLT,      /* Less Than */
    kxLE,      /* Less Than or Equal */
    kxAnd,
    kxOr,
    kxNot,
    kxXor,
    };

struct exp
    {
    void *left;
    void *right;
    enum keyExpType type;
    };

static void getIntVals(struct kvt *kvt, struct exp *exp, int *retLeft, int *retRight)
/* Look up value for key on left hand side of expression and
 * literal string from right hand side.  Convert both to ints. */
{
char *rightString = exp->right;
char *leftKey = exp->left;
char *leftString = kvtLookup(kvt, leftKey);

if (leftString == NULL)
    *retLeft = 0;
else
    *retLeft = atoi(leftString);
if (rightString == NULL)
    *retRight = 0;
else
    *retRight = atoi(rightString);
}

#if 0 /* unused */
static void dumpExp(struct kvt *kvt, struct exp *exp)
/* Print out expression. */
{
switch (exp->type)
    {
    case kxMatch:
        {
        char *key = exp->left;
        char *matcher = exp->right;
        char *val = kvtLookup(kvt, key);
	printf("%s(%s) match %s\n", key, val, matcher);
	break;
        }
    case kxWildMatch:
        {
        char *key = exp->left;
        char *matcher = exp->right;
        char *val = kvtLookup(kvt, key);
	printf("%s(%s) wildMatch %s\n", key, val, matcher);
	break;
        }
    case kxGT:
        {
        int left, right;
        getIntVals(kvt, exp, &left, &right);
	printf("%d > %d\n", left, right);
	break;
        }
    case kxGE:
        {
        int left, right;
        getIntVals(kvt, exp, &left, &right);
	printf("%d >= %d\n", left, right);
	break;
        }
    case kxLT:
        {
        int left, right;
        getIntVals(kvt, exp, &left, &right);
	printf("%d < %d\n", left, right);
	break;
        }
    case kxLE:
        {
        int left, right;
        getIntVals(kvt, exp, &left, &right);
	printf("%d <= %d\n", left, right);
	break;
        }
    
    case kxNot:
        {
	printf("!\n");
	break;
        }
    case kxAnd:
        {
	printf("&\n");
	break;
        }
    case kxOr:
        {
	printf("|\n");
	break;
        }
    case kxXor:
        {
	printf("^\n");
	break;
        }
    }
}
#endif

static boolean rkeyEval(struct kvt *kvt, struct exp *exp)
/* Recursively evaluate expression. */
{
if (exp == NULL)
    return TRUE;
switch (exp->type)
    {
    case kxMatch:
        {
        char *key = exp->left;
        char *matcher = exp->right;
        char *val = kvtLookup(kvt, key);
        if (val == NULL)
            return sameWord(matcher, "null");
        else
            return sameWord(matcher, val);
        }
    case kxWildMatch:
        {
        char *key = exp->left;
        char *matcher = exp->right;
        char *val = kvtLookup(kvt, key);
        if (val == NULL)
            return sameString(matcher, "*");
        else
            return wildMatch(matcher, val);
        }
    case kxGT:
        {
        int left, right;
        getIntVals(kvt, exp, &left, &right);
        return left > right;
        }
    case kxGE:
        {
        int left, right;
        getIntVals(kvt, exp, &left, &right);
        return left >= right;
        }
    case kxLT:
        {
        int left, right;
        getIntVals(kvt, exp, &left, &right);
        return left < right;
        }
    case kxLE:
        {
        int left, right;
        getIntVals(kvt, exp, &left, &right);
        return left <= right;
        }
    
    case kxNot:
        {
        return !rkeyEval(kvt, exp->right);
        }
    case kxAnd:
        {
        return rkeyEval(kvt, exp->left) && rkeyEval(kvt, exp->right);
        }
    case kxOr:
        {
        return rkeyEval(kvt, exp->left) || rkeyEval(kvt, exp->right);
        }
    case kxXor:
        {
        return rkeyEval(kvt, exp->left) ^ rkeyEval(kvt, exp->right);
        }
    default:
        {
	errAbort("unknown expression type %d", exp->type);
	return 0;
	}
    }
}

boolean keyExpEval(struct keyExp *keyExp, struct kvt *kvt)
/* Evaluate key expression. */
{
return rkeyEval(kvt, keyExp->rootExp);
}

/***** A little recursive descent parser. *****/

static struct kxTok *token;      /* Next token for parser. */

static void advanceToken()
/* Move to next token. */
{
token = token->next;
}

static struct exp *nextExp();       /* Get next expression. */

static struct exp *parseRelation()
/* Parse key=wildcard. */
{
struct kxTok *key, *match;

if (token == NULL)
    return NULL;
if (token->type != kxtString)
    errAbort("Expecting key got %s", token->string);
key = token;
advanceToken();
if (token->type == kxtEquals)
    {
    advanceToken();
    if (token->type == kxtString || token->type == kxtWildString)
        {
        struct exp *exp;
        match = token;
        advanceToken();
        AllocVar(exp);
        exp->left = key->string;
        exp->right = match->string;
        exp->type = (match->type == kxtString ? kxMatch : kxWildMatch);
        return exp;
        }
    else
        {
        errAbort("Expecting string to match in key=match expression,\ngot %s", token->string);
        }
    }
else if (token->type == kxtGT || token->type == kxtGE || token->type == kxtLT || token->type == kxtLE)
    {
    enum kxTokType relation = token->type;
    advanceToken();
    if (isdigit(token->string[0]))
        {
        struct exp *exp;
        match = token;
        advanceToken();
        AllocVar(exp);
        exp->left = key->string;
        exp->right = match->string;
        if (relation == kxtGT) exp->type = kxGT;
        else if (relation == kxtGE) exp->type = kxGE;
        else if (relation == kxtLT) exp->type = kxLT;
        else if (relation == kxtLE) exp->type = kxLE;
        return exp;
        }
    else
        {
        errAbort("Expecting number got %s", token->string);
        }
    }
else
    errAbort("Expecting = got %s", token->string);
return NULL;
}

static struct exp *parseParenthesized()
/* Parse parenthesized expressions. */
{
struct exp *exp;
if (token == NULL)
    return NULL;
if (token->type == kxtOpenParen)
    {
    advanceToken();
    exp = nextExp();
    if (token->type != kxtCloseParen)
        errAbort("Unmatched parenthesis");
    advanceToken();
    return exp;
    }
else
    {
    return parseRelation();
    }            
}

static struct exp *parseNot()
/* Parse not */
{
struct exp *exp;
struct exp *right;

if (token == NULL)
    return NULL;
if (token->type == kxtNot)
    {
    advanceToken();
    right = nextExp();
    AllocVar(exp);
    exp->right = right;
    exp->type = kxNot;
    return exp;
    }
else
    return parseParenthesized();
}

static struct exp *parseAndExp()
/* Parse and level expressions. */
{
struct exp *left;
struct exp *right, *exp;
struct kxTok *tok;
enum kxTokType type;

if ((left = parseNot()) == NULL)
    return NULL;
if ((tok = token) == NULL)
    return left;
type = token->type;
if (type == kxtAnd)
    {
    advanceToken();
    right = nextExp();
    if (right == NULL)
        errAbort("Expecting expression on the other side of %s", tok->string);
    AllocVar(exp);
    exp->left = left;
    exp->right = right;
    exp->type = kxAnd;
    return exp;
    }
else
    return left;    
}

static struct exp *parseOrExp()
/* Parse lowest level of precedent expressions - or and xor. */
{
struct exp *left;
struct exp *right, *exp;
struct kxTok *tok;
enum kxTokType type;

if ((left = parseAndExp()) == NULL)
    return NULL;
if ((tok = token) == NULL)
    return left;
type = token->type;
if (type == kxtOr || type == kxtXor)
    {
    advanceToken();
    right = nextExp();
    if (right == NULL)
        errAbort("Expecting expression on the other side of %s", tok->string);
    AllocVar(exp);
    exp->left = left;
    exp->right = right;
    if (type == kxtOr)
        exp->type = kxOr;
    else
        exp->type = kxXor;
    return exp;
    }
else
    return left;    
}

static struct exp *nextExp()
/* Get another expression. */
{
return parseOrExp();
}

static struct exp *parseExp(struct kxTok *tokList)
/* Convert key expression from token stream to parse tree. */
{
struct exp *exp;
token = tokList;
exp =  nextExp();
if (token->type != kxtEnd)
    {
    errAbort("Extra tokens past end of expression.  Missing &?");
    }
return exp;
}

struct keyExp *keyExpParse(char *text)
/* Parse text into key expression.  Squawk and die if it
 * fails. */
{
struct keyExp *ke;
struct kxTok *tok;
AllocVar(ke);
ke->tokenList = tok = kxTokenize(text, TRUE);
ke->rootExp = parseExp(tok);

return ke;
}



boolean keyTextScan(char *text, char *key, char *valBuf, int valBufSize)
/* Get value of key. Return FALSE if key doesn't exist. */
{
int keySize = strlen(key);
char *s, *nl;
boolean ok = FALSE;

for (s = text; !isspace(s[0]); s = nl+1)
    {
    nl = strchr(s, '\n');
    assert(nl != NULL);
    if (s[keySize] == ' ' && memcmp(s, key, keySize) == 0)
        {
        char *val = s + keySize + 1;
        int valSize = nl - val;
        if (valSize >= valBufSize)
            valSize = valBufSize-1;
        memcpy(valBuf, val, valSize);
        valBuf[valSize] = 0;
        ok = TRUE;
        break;
        }
    }
return ok;
}

