/* Joining.c - stuff to help create joined queries.  Most of
 * the joining is happening in C rather than SQL, mostly so
 * that we can filter on an ID hash as we go. */

#include "common.h"
#include "hash.h"
#include "localmem.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: joining.c,v 1.1 2004/07/16 18:21:43 kent Exp $";

struct joinableRow
/* A row that is joinable.  Allocated in joinableResult->lm. */
    {
    struct joinableRow *next;
    char **fields;	/* Fields user has requested. */
    char **keys;	/* Fields to key from. */
    };

struct joinableTable
/* Database query result table that is joinable. */
    {
    struct joinableResult *next;
    struct lm *lm;	/* Local memory structure - used for all allocations. */
    char *tableName;	/* Name of table. */
    char **fieldNames;	/* Names of fields. */
    int fieldCount;	/* Number of fields user has requested. */
    char **keyNames;	/* Names of keys. */
    int keyCount;	/* Number of keys. */
    struct joinableRow *rowList;	/* Rows. */
    };

struct joinableTable *joinableTableNew(char *table, 
	char *fields, 	    /* Comma separated field list, no terminal comma. */
	struct slName *keyList) /* List of keys. */
/* Make up new joinable table. */
{
struct joinableTable *jt;
struct slName *field, *fieldList = commaSepToSlNames(fields);
struct slName *key;
struct lm *lm;
int i;

AllocVar(jt);
lm = jt->lm = lmInit(64*1024);
jt->tableName = lmCloneString(lm, table);
jt->fieldCount = slCount(fieldList);
lmAllocArray(lm, jt->fieldNames, jt->fieldCount);
for (field = fieldList, i=0; field != NULL; field = field->next, ++i)
    jt->fieldNames[i] = lmCloneString(lm, field->name);
jt->keyCount = slCount(keyList);
lmAllocArray(lm, jt->keyNames, jt->keyCount);
for (key = keyList, i=0; key != NULL; key = key->next, ++i)
    jt->keyNames[i] = lmCloneString(lm, key->name);
return jt;
}

void joinableTableFree(struct joinableTable **pJt)
/* Free up joinable table and associated rows. */
{
struct joinableTable *jt = *pJt;
if (jt != NULL)
    {
    lmCleanup(&jt->lm);
    freez(pJt);
    }
}

struct joinableRow *jrAddRow(struct joinableTable *jt, char **row)
/* Add new row to joinable table. */
{
struct joinableRow *jr;
int i;
struct lm *lm = jt->lm;
lmAllocVar(lm, jr);
lmAllocArray(lm, jr->fields, jt->fieldCount);
lmAllocArray(lm, jr->keys, jt->keyCount);
for (i=0; i<jt->fieldCount; ++i)
    jr->fields[i] = lmCloneString(lm, row[i]);
row += jt->fieldCount;
for (i=0; i<jt->keyCount; ++i)
    jr->keys[i] = row[i];
slAddHead(&jt->rowList, jr);
return jr;
}

struct joinableTable *fetchKeyedFields(struct region *regionList,
	struct sqlConnection *conn, char *table, boolean isPositional,
	char *fields, char *keyIn, struct hash *keyInHash, 
	struct slName *keyOutList)
/* This returns a list of keyedRows filtering out those that
 * don't match on keyIn.  */
{
struct joinableTable *jt = joinableTableNew(table, fields, keyOutList);
struct dyString *fieldSpec = dyStringNew(0);
struct slName *keyOut;

uglyf("fetchKeyFields from %s,  fields %s, keyIn %s\n",
	table, fields, keyIn);
dyStringAppend(fieldSpec, fields);
for (keyOut = keyOutList; keyOut != NULL; keyOut = keyOut->next)
    dyStringPrintf(fieldSpec, ",%s", keyOut->name);
if (keyIn != NULL)
    dyStringPrintf(fieldSpec, ",%s", keyIn);
uglyf("fieldSpec = %s\n", fieldSpec->string);
return NULL;
}

void doTest(struct sqlConnection *conn)
/* Put up a page to see what happens. */
{
struct slName *keysOut = NULL, *key;
struct hash *keyInHash = NULL;
struct region *regionList = getRegions(conn);
textOpen();

/* Put together output key list. */
key = slNameNew("name");
slAddHead(&keysOut, key);
key = slNameNew("id");
slAddHead(&keysOut, key);

/* Put together input key hash. */
keyInHash = hashNew(0);
hashAdd(keyInHash, "5263", NULL);
hashAdd(keyInHash, "5264", NULL);
hashAdd(keyInHash, "5265", NULL);
hashAdd(keyInHash, "5266", NULL);

fetchKeyedFields(regionList, conn, "ensGene", TRUE, "name,chrom,id,strand", 
	"id", keyInHash, keysOut);
}

