/* Joining.c - stuff to help create joined queries.  Most of
 * the joining is happening in C rather than SQL, mostly so
 * that we can filter on an ID hash as we go, and also deal
 * with prefixes and suffixes to the dang keys. */

#include "common.h"
#include "hash.h"
#include "localmem.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "joiner.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: joining.c,v 1.4 2004/07/17 02:25:14 kent Exp $";

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
    int rowCount;	/* Number of rows. */
    };

void dumpJt(struct joinableTable *jt)
/* Write out fields. */
{
struct joinableRow *jr;
for (jr = jt->rowList; jr != NULL; jr = jr->next)
    {
    int i;
    for (i=0; i<jt->fieldCount; ++i)
        printf("\t%s", jr->fields[i]);
    printf("\n");
    }
}

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
    jr->keys[i] = lmCloneString(lm, row[i]);
slAddHead(&jt->rowList, jr);
jt->rowCount += 1;
return jr;
}

char *chopKey(struct slName *prefixList, struct slName *suffixList, char *key)
/* Chop off any prefixes/suffixes from key. */
{
struct slName *n;

for (n=prefixList; n != NULL; n = n->next)
    {
    if (startsWith(n->name, key))
	{
        key += strlen(n->name);
	break;
	}
    }
for (n=suffixList; n!=NULL; n = n->next)
    {
    char *e = strstr(key, n->name);
    if (e != NULL)
        {
	*e = 0;
	break;
	}
    }
return key;
}

struct joinableTable *fetchKeyedFields(struct region *regionList,
	struct sqlConnection *conn, char *table, boolean isPositional, char *fields, 
	char *keyIn, struct hash *keyInHash, 
	struct slName *chopBefore, struct slName *chopAfter,
	struct slName *keyOutList)
/* This returns a list of keyedRows filtering out those that
 * don't match on keyIn.  */
{
struct joinableTable *jt = joinableTableNew(table, fields, keyOutList);
struct dyString *fieldSpec = dyStringNew(0);
struct slName *keyOut;
struct region *region;
int keyInField = -1;

uglyf("fetchKeyFields from %s,  fields %s, keyIn %s\n",
	table, fields, keyIn);
dyStringAppend(fieldSpec, fields);
for (keyOut = keyOutList; keyOut != NULL; keyOut = keyOut->next)
    dyStringPrintf(fieldSpec, ",%s", keyOut->name);
if (keyIn != NULL)
    {
    dyStringPrintf(fieldSpec, ",%s", keyIn);
    uglyf("jt->keyCount = %d, jt->fieldCount = %d\n", jt->keyCount, jt->fieldCount);
    assert(keyInHash != NULL);
    keyInField = jt->keyCount + jt->fieldCount;
    }
uglyf("fieldSpec = %s\n", fieldSpec->string);
uglyf("keyInField = %d\n", keyInField);

for (region = regionList; region != NULL; region = region->next)
    {
    char **row;
    struct sqlResult *sr = regionQuery(conn, table, 
    	fieldSpec->string, region, isPositional);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (keyInField < 0)
	    jrAddRow(jt, row);
	else
	    {
	    char *key = row[keyInField];
	    char *e;
	    if (hashLookup(keyInHash, chopKey(chopBefore, chopAfter, key)))
		jrAddRow(jt, row);
	    }
	}
    sqlFreeResult(&sr);
    if (!isPositional)
        break;
    }
slReverse(&jt->rowList);
return jt;
}

struct hash *hashKeyField(struct joinableTable *jt, char *keyField,
	struct slName *chopPrefix, struct slName *chopSuffix)
/* Make a hash based on key field. */
{
int ix = stringArrayIx(keyField, jt->keyNames, jt->keyCount);
int hashSize = digitsBaseTwo(jt->rowCount);
struct hash *hash = NULL;
struct joinableRow *jr;

if (hashSize > 20)
    hashSize = 20;
hash = newHash(hashSize);
if (ix < 0)
    internalErr();
for (jr = jt->rowList; jr != NULL; jr = jr->next)
    {
    char *key = chopKey(chopPrefix, chopSuffix, jr->keys[ix]);
    hashAdd(hash, key, jr);
    }
return hash;
}

void oldDoTest(struct sqlConnection *conn)
/* Put up a page to see what happens. */
{
struct slName *keysOut = NULL, *key;
struct hash *keyInHash = NULL;
struct region *regionList = getRegions(conn);
struct joinableTable *jt = NULL, *jt2;
struct hash *hash2;
struct joiner *joiner = joinerRead("all.joiner");

/* Open for text output. */
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

jt = fetchKeyedFields(regionList, conn, "ensGene", 
	TRUE, "name,chrom,id,strand", "id", keyInHash, NULL, NULL, keysOut);
dumpJt(jt);
uglyf("Fetched %d rows from %s\n\n", slCount(jt->rowList), jt->tableName);

hash2 = hashKeyField(jt, "name", NULL, NULL);
jt2 = fetchKeyedFields(regionList, conn, "ensGtp", 
	FALSE, "gene,protein", "transcript", hash2, NULL, slNameNew("."), NULL);
uglyf("Fetched %d rows from %s\n", slCount(jt2->rowList), jt2->tableName);
dumpJt(jt2);
}

struct joinerDtf *fieldsToDtfs(struct slName *fieldList)
/* Convert list from dotted triple to dtf format. */
{
struct joinerDtf *dtfList = NULL, *dtf;
struct slName *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    dtf = joinerDtfFromDottedTriple(field->name);
    slAddHead(&dtfList, dtf);
    }
slReverse(&dtfList);
return dtfList;
}

struct dyString *makeOrderedCommaFieldList(struct sqlConnection *conn, char *table, 
	struct joinerDtf *dtfList)
/* Assumes that dtfList all points to same table.  This will return a comma-separated
 * field list in the same order as the fields are in database. */
{
struct joinerDtf *dtf;
struct slName *dbField, *dbFieldList = sqlListFields(conn, table);
boolean first = TRUE;
struct dyString *dy = dyStringNew(0);
struct hash *dtfHash = newHash(8);

/* Build up hash of field names. */
for (dtf = dtfList; dtf != NULL; dtf = dtf->next)
    hashAdd(dtfHash, dtf->field, NULL);
for (dbField = dbFieldList; dbField != NULL; dbField = dbField->next)
    {
    if (hashLookup(dtfHash, dbField->name))
        {
	if (first)
	    first = FALSE;
	else
	    dyStringAppendC(dy, ',');
	dyStringAppend(dy, dbField->name);
	}
    }
hashFree(&dtfHash);
slFreeList(&dbFieldList);
return dy;
}

struct tableFields
/* List of fields in a single table. */
    {
    struct tableFields *next;	/* Next in list. */
    char *database;		/* Database we're in.  Not alloced here. */
    char *table;		/* Table we're in.  Not alloced here. */
    struct joinerDtf *fieldList;	/* Fields. */
    };

void tableFieldsFree(struct tableFields **pTf)
/* Free up memory associated with tableFields. */
{
struct tableFields *tf = *pTf;
if (tf != NULL)
    {
    joinerDtfFreeList(&tf->fieldList);
    freez(pTf);
    }
}

void tableFieldsFreeList(struct tableFields **pList)
/* Free a list of dynamically allocated tableFields's */
{
struct tableFields *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    tableFieldsFree(&el);
    }
*pList = NULL;
}


struct tableFields *bundleFieldsIntoTables(struct joinerDtf **pDtfList)
/* Convert list of fields to list of tables.  This will eat up the
 * input list and transfer it to the fieldList element of the tables. */
{
struct hash *hash = newHash(0);
char fullName[256];
struct joinerDtf *dtf, *next;
struct tableFields *tfList = NULL, *tf;

for (dtf = *pDtfList; dtf != NULL; dtf = next)
    {
    next = dtf->next;
    safef(fullName, sizeof(fullName), "%s.%s", dtf->database, dtf->table);
    tf = hashFindVal(hash, fullName);
    if (tf == NULL)
        {
	AllocVar(tf);
	tf->database = dtf->database;
	tf->table = dtf->table;
	slAddHead(&tfList, tf);
	}
    slAddHead(&tf->fieldList, dtf);
    }
slReverse(&tfList);
return tfList;
}

struct joinerDtf *tableFirstFieldList(struct tableFields *tfList)
/* Make dtfList that is just the first one in each table's field list. */
{
struct joinerDtf *list = NULL, *dtf;
struct tableFields *tf;

for (tf = tfList; tf != NULL; tf = tf->next)
    {
    struct joinerDtf *dtf = tf->fieldList;
    struct joinerDtf *dupe = joinerDtfNew(dtf->database, dtf->table, dtf->field);
    slAddHead(&list, dupe);
    }
slReverse(&list);
return list;
}

static struct tableFields *findTable(struct tableFields *tfList, char *db, char *table)
/* Find table that matches. */
{
struct tableFields *tf;
for (tf = tfList; tf != NULL; tf = tf->next)
    if (sameString(tf->database,db) && sameString(tf->table, table))
        return tf;
return NULL;
}

int tableFieldsCmp(const void *va, const void *vb)
/* Compare two tableFields. */
{
const struct tableFields *a = *((struct tableFields **)va);
const struct tableFields *b = *((struct tableFields **)vb);
int diff;
diff = strcmp(a->database, b->database);
if (diff == 0)
    diff = strcmp(a->table, b->table);
return diff;
}

void orderTables(struct tableFields **pTfList, char *primaryDb, char *primaryTable)
/* Order table list so that primary table is first and the rest are
 * alphabetical.  This is the order they are listed in the UI.
 * (However we get them scrambled since the CGI variables are not
 * ordered.)*/
{
struct tableFields *tfList = *pTfList;
struct tableFields *primaryTf = findTable(tfList, primaryDb, primaryTable);
if (primaryTf != NULL)
    slRemoveEl(tfList, primaryTf);
slSort(&tfList, tableFieldsCmp);
if (primaryTf != NULL)
    {
    slAddHead(&tfList, primaryTf);
    }
*pTfList = tfList;
}


void tabOutFields(
	char *primaryDb,		/* The primary database. */
	char *primaryTable, 		/* The primary table. */
	struct slName *fieldList)	/* List of db.table.field */
/* Do tab-separated output. */
{
struct joinerDtf *dtfList = NULL;
struct tableFields *tfList = NULL, *tf;

dtfList = fieldsToDtfs(fieldList);
tfList = bundleFieldsIntoTables(&dtfList);

if (slCount(tfList) == 1)
    {
    tf = tfList;
    struct sqlConnection *conn = sqlConnect(tf->database);
    struct dyString *dy = makeOrderedCommaFieldList(conn, tf->table, tf->fieldList);
    doTabOutTable(tf->table, conn, dy->string);
    }
else
    {
    struct joiner *joiner = joinerRead("all.joiner");
    struct joinerPair *routeList, *route;
    struct joinerDtf *tableDtfs;

    orderTables(&tfList, primaryDb, primaryTable);
    tableDtfs = tableFirstFieldList(tfList);
    routeList = joinerFindRouteThroughAll(joiner, tableDtfs);
    uglyf("Got %d hops in route\n", slCount(routeList));
    for (route = routeList; route != NULL; route = route->next)
        uglyf("%s.%s.%s -> %s.%s.%s\n", route->a->database, route->a->table, route->a->field, route->b->database, route->b->table, route->b->field);
    uglyAbort("All for now");
    joinerDtfFreeList(&tableDtfs);
    }
tableFieldsFreeList(&tfList);
}

void doTest(struct sqlConnection *conn)
/* Put up a page to see what happens. */
{
struct slName *fieldList = NULL, *field;
field = slNameNew("hg16.ensGtp.protein");
slAddTail(&fieldList, field);
field = slNameNew("hg16.ensGene.exonCount");
slAddTail(&fieldList, field);
field = slNameNew("hg16.ensGene.name");
slAddTail(&fieldList, field);

textOpen();
tabOutFields("hg16", "ensGene", fieldList);
}


