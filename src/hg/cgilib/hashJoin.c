/* hashJoin - join one or more columns of a hashed database table to an externally provided
 * char **row that contains the key and empty slot(s) for the column value(s) */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "hashJoin.h"
#include "hdb.h"
#include "obscure.h"

struct hashJoin
// Implements table join as a hash lookup: the key is taken from some column of an externally
// provided row, and one or more values are retrieved and then stored in specified columns
// of the external row.  If a key has more than one set of matching columns, then each
// column's values are glommed into a comma-separated list for that column in the external row.
{
    struct hashJoin *next;
    struct hash *hash;			// Hash some kind of key to char **row of column values
    uint extRowKeyIx;			// Index of hash key to take from external row
    uint valCount;			// Number of columns in hash value rows
    uint *extRowValIxs;			// Index of each hash value column to store in external row
    struct dyString **colValues;	// Accumulators for hash value columns -- multiple
					// results from hash lookup become comma-sep strings
    struct lm *lm;			// For storing hash values, misc strings & arrays
    struct joinerField *jfA;		// If non-NULL, its separator, chopBefore and chopAfter
					// are applied to each key accessed by hashJoinOneRow.
    struct joinerField *jfB;		// If non-NULL, its chopBefore and chopAfter
					// are applied to each key passed to hashJoinAddMapping.
    char *db;				// Database from which to load hash
    char *table;			// Table from which to load hash
    char *query;			// SQL query to execute when loading hash
    boolean loaded;			// TRUE when table contents have been loaded into hash
    boolean naForMissing;		// If TRUE, then output "n/a" when there's no match
};

struct hashJoin *hashJoinNew(struct joinerDtf *keyDtf, uint extRowKeyIx,
                             struct joinerDtf *valDtfs, uint *extRowValIxs,
                             struct joinerField *jfA, struct joinerField *jfB,
                             boolean naForMissing)
/* Return a new hashJoin.  extRowKeyIx is the index in an external row of the key
 * to use in the join.  extRowValIxs[valCount] contains each hash val column's index
 * into an external row.  jfA and jfB are optional; if given, then jfA's separator,
 * chopBefore and chopAfter will be applied to each key retrieved from the external row
 * and jfB's separator, chopBefore and chopAfter will be applied to each hash key.
 * If naForMissing is TRUE then the result columns will contain "n/a" when there is
 * no match in the hash. */
{
struct hashJoin *self;
AllocVar(self);
self->extRowKeyIx = extRowKeyIx;
int valCount = slCount(valDtfs);
self->valCount = valCount;
// Save some inner-loop tests if no separating or chopping will be required:
if (jfA && (jfA->separator || jfA->chopBefore || jfA->chopAfter))
    self->jfA = jfA;
if (jfB && (jfB->separator || jfB->chopBefore || jfB->chopAfter))
    self->jfB = jfB;
self->lm = lmInit(0);
lmAllocArray(self->lm, self->extRowValIxs, valCount);
CopyArray(extRowValIxs, self->extRowValIxs, valCount);
lmAllocArray(self->lm, self->colValues, valCount);
int i;
for (i = 0;  i < valCount;  i++)
    self->colValues[i] = dyStringNew(0);
self->db = lmCloneString(self->lm, keyDtf->database);
self->table = lmCloneString(self->lm, keyDtf->table);
struct dyString *query = sqlDyStringCreate("select %s", keyDtf->field);
struct joinerDtf *dtf;
for (dtf = valDtfs;  dtf != NULL;  dtf = dtf->next)
    {
    if (differentString(dtf->database, self->db) ||
        differentString(dtf->table, self->table))
        errAbort("hashJoinNew: inconsistent key field (%s.%s.%s) and value field (%s.%s.%s)",
                 keyDtf->database, keyDtf->table, keyDtf->field,
                 dtf->database, dtf->table, dtf->field);
    dyStringAppendC(query, ',');
    dyStringAppend(query, dtf->field);
    }
dyStringPrintf(query, " from %s", self->table);
self->query = dyStringCannibalize(&query);
self->naForMissing = naForMissing;
return self;
}

struct hashJoin *hashJoinNext(struct hashJoin *el)
/* Get the next hashJoin in a list of hashJoins. */
{
return el->next;
}

struct hjAddOneContext
// joinerFieldIterateKey context for use by hashJoinAddOne
{
    struct hash *hash;
    char **clonedValues;
};

static void hashJoinAddOne(void *context, char *key)
/* Add values from context to hash from context for key.
 * This is a callback for joinerFieldIterateKey; context is struct hjAddOneContext *. */
{
struct hjAddOneContext *ctx = context;
hashAdd(ctx->hash, key, ctx->clonedValues);
}

static void hashJoinLoad(struct hashJoin *self)
/* Load table contents into hash. */
{
if (self->loaded)
    errAbort("hashJoinLoad: loaded flag already set");
struct sqlConnection *conn = hAllocConn(self->db);
int rowCount = sqlRowCount(conn, self->table);
int hashSize = min(digitsBaseTwo(rowCount), hashMaxSize);
self->hash = hashNew(hashSize);
char **row;
struct sqlResult *sr = sqlGetResult(conn, self->query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char **clonedValues = lmCloneRow(self->lm, row+1, self->valCount);
    struct hjAddOneContext context = { self->hash, clonedValues };
    // If necessary, process key according to self->jfA.
    if (self->jfB)
        joinerFieldIterateKey(self->jfB, hashJoinAddOne, &context, row[0]);
    else
        hashAdd(self->hash, row[0], clonedValues);
    }
self->loaded = TRUE;
hFreeConn(&conn);
}

struct hjKeyContext
{
    struct hashJoin *self;
    boolean includeEmpties;
    boolean matchCount;
};

static void hashJoinOneKey(void *context, char *key)
/* Look up some processed key in hash and accumulate results for each column.
 * This is a callback for joinerFieldIterateKey; context is struct hashJoin *. */
{
struct hjKeyContext *ctx = context;
struct hashJoin *self = ctx->self;
struct hashEl *helFirst = hashLookup(self->hash, key);
// hgTables accumulates multiple match values with slAddHead so they are
// printed in reverse.  Use arrays to accumulate multiple matched rows; we'll step
// through them backwards in hashJoinGlomMultipleMatches to match hgTables' order.
int helMaxCount = slCount(helFirst);
char **matchRows[helMaxCount];
struct hashEl *hel;
int matchIx;
for (matchIx = 0, hel = helFirst;  hel != NULL;  hel = hashLookupNext(hel), matchIx++)
    {
    char **row = hel->val;
    matchRows[matchIx] = row;
    }
int matchCount = matchIx;
ctx->matchCount += matchCount;
// When there are multiple matches, hgTables includes empty vals and prints a comma after each item.
boolean includeEmpties = ctx->includeEmpties || (matchCount > 1);
// Step through matchRows in reverse order to match hgTables.
for (matchIx = matchCount - 1;  matchIx >= 0;  matchIx--)
    {
    char **row = matchRows[matchIx];
    int valIx;
    for (valIx = 0;  valIx < self->valCount;  valIx++)
        {
        char *val = row[valIx];
        if (isNotEmpty(val) || includeEmpties)
            {
            // Skip over adjacent duplicate values
            struct dyString *colDy = self->colValues[valIx];
            int colDyLen = dyStringLen(colDy);
            boolean isDup = FALSE;
            if (matchIx < matchCount - 1)
                {
                char **prevRow = matchRows[matchIx+1];
                char *prevVal = (prevRow == NULL) ? NULL : prevRow[valIx];
                isDup = sameOk(val, prevVal);
                }
            else
                // If there's no previous row to compare to from this key, but colDy already
                // ends with the same value, consider this a duplicate:
                isDup = colDyLen > 0 && endsWithWordComma(colDy->string, val);
            if (! isDup)
                {
                if (includeEmpties)
                    {
                    if (isNotEmpty(val))
                        dyStringAppend(colDy, val);
                    dyStringAppendC(colDy, ',');
                    }
                else
                    {
                    if (colDyLen > 0)
                        dyStringAppendC(colDy, ',');
                    dyStringAppend(colDy, val);
                    }
                }
            }
        }
    }
}

static void hashJoinChopCommaKey(struct hjKeyContext *context, struct joinerField *jfA, char *key)
/* Chop key by comma, regardless of jfA->separator; for each item, apply jfA's chopBefore
 * and chopAfter if applicable, and try to join the result. */
{
context->includeEmpties = TRUE;
int len = strlen(key);
char keyClone[len+1];
safencpy(keyClone, sizeof(keyClone), key, len);
char *s = keyClone, *e;
while (isNotEmpty(s))
    {
    e = strchr(s, ',');
    if (e != NULL)
        *e++ = 0;
    if (jfA)
        s = joinerFieldChopKey(jfA, s);
    if (s[0] != 0)
        hashJoinOneKey(context, s);
    s = e;
    }
}

void hashJoinOneRow(struct hashJoin *self, char **extRow)
/* Look up some column of extRow in hash and place result(s) in other columns of extRow.
 * Don't call this again until done with extRow -- column value storage is reused. */
{
if (!self->loaded)
    hashJoinLoad(self);
char *key = extRow[self->extRowKeyIx];
if (isNotEmpty(key))
    {
    // Clear accumulators
    uint i;
    for (i = 0;  i < self->valCount;  i++)
        dyStringClear(self->colValues[i]);
    // If necessary, process key according to self->jfA. Look up key(s) and accumulate results.
    struct joinerField *jfA = self->jfA;
    struct hjKeyContext context = { self, FALSE, FALSE };
    if (jfA)
        {
        context.includeEmpties = TRUE;
        joinerFieldIterateKey(jfA, hashJoinOneKey, &context, key);
        }
    else
        hashJoinOneKey(&context, key);
    // In case we're processing comma-glommed results from some other hash join --
    // if there were no results, but the key contains commas and wasn't already comma-chopped
    // by joinerFieldIterateKey, try comma-chopping it and looking up the pieces.
    if (context.matchCount == 0 &&
        ! (jfA && sameOk(jfA->separator, ",")) &&
        strchr(key, ','))
        {
        hashJoinChopCommaKey(&context, jfA, key);
        }
    // When includeEmpties is set, we assume we're going to have multiple outputs.
    // However, there might be only one match among multiple keys.  If so, remove trailing commas.
    if (context.includeEmpties && context.matchCount == 1)
        {
        int valIx;
        for (valIx = 0;  valIx < self->valCount;  valIx++)
            {
            struct dyString *colDy = self->colValues[valIx];
            char *end = colDy->string + dyStringLen(colDy) - 1;
            if (*end == ',')
                *end = '\0';
            }
        }
    // Set the external row result columns to point to accumulated values.
    for (i = 0;  i < self->valCount;  i++)
        {
        struct dyString *colDy = self->colValues[i];
        if (self->naForMissing && context.matchCount == 0)
            dyStringAppend(colDy, "n/a");
        uint extRowValIx = self->extRowValIxs[i];
        extRow[extRowValIx] = colDy->string;
        }
    }
}

void hashJoinFree(struct hashJoin **pSelf)
/* Free hashJoin (if necessary). */
{
if (pSelf == NULL || *pSelf == NULL)
    return;
struct hashJoin *self = *pSelf;
hashFree(&self->hash);
uint i;
for (i = 0;  i < self->valCount;  i++)
    dyStringFree(&self->colValues[i]);
freeMem(self->query);
lmCleanup(&self->lm);
freez(pSelf);
}

void hashJoinFreeList(struct hashJoin **pList)
/* Free a list of hashJoins. */
{
if (pList == NULL || *pList == NULL)
    return;
struct hashJoin *el = *pList;
while (el != NULL)
    {
    struct hashJoin *elNext = el->next;
    hashJoinFree(&el);
    el = elNext;
    }
*pList = NULL;
}
