/* Joining.c - stuff to help create joined queries.  Most of
 * the joining is happening in C rather than SQL, mostly so
 * that we can filter on an ID hash as we go, and also deal
 * with prefixes and suffixes to the dang keys. */

#include "common.h"
#include "hash.h"
#include "localmem.h"
#include "memalloc.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "joiner.h"
#include "hdb.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: joining.c,v 1.19 2004/07/18 21:52:28 kent Exp $";

struct joinedRow
/* A row that is joinable.  Allocated in joinableResult->lm. */
    {
    struct joinedRow *next;
    char **fields;	/* Fields user has requested. */
    char **keys;	/* Fields to key from. */
    };

struct joinedTables
/* Database query result table that is joinable. */
    {
    struct joinedTables *next;
    struct lm *lm;	/* Local memory structure - used for row allocations. */
    struct joinerDtf *fieldList;	/* Fields user has requested. */
    int fieldCount;	/* Number of fields - calculated at start of load. */
    struct joinerDtf *keyList;	/* List of keys. */
    int keyCount;	/* Number of keys- calculated at start of load. */
    struct joinedRow *rowList;	/* Rows - allocated in lm. */
    int rowCount;	/* Number of rows. */
    int maxRowCount;	/* Max row count allowed (0 means no limit) */
    };

void joinedTablesTabOut(struct joinedTables *joined)
/* Write out fields. */
{
struct joinedRow *jr;
struct joinerDtf *field;
int outCount = 0;
hPrintf("#");
for (field = joined->fieldList; field != NULL; field = field->next)
    {
    hPrintf("%s.%s.%s", field->database, field->table, field->field);
    if (field->next == NULL)
        hPrintf("\n");
    else
        hPrintf("\t");
    }
for (jr = joined->rowList; jr != NULL; jr = jr->next)
    {
    int i;
    for (i=0; i<joined->fieldCount; ++i)
	{
	char *s;
	if (i != 0)
	    hPrintf("\t");
	s = jr->fields[i];
	if (s == NULL)
	    s = "n/a";
        hPrintf("%s", s);
	}
    hPrintf("\n");
    ++outCount;
    }
}

struct joinedTables *joinedTablesNew(int fieldCount, 
	int keyCount, int maxRowCount)
/* Make up new empty joinedTables. */
{
struct joinedTables *jt;
struct lm *lm;

AllocVar(jt);
lm = jt->lm = lmInit(64*1024);
jt->fieldCount = fieldCount;
jt->keyCount = keyCount;
jt->maxRowCount = maxRowCount;
return jt;
}

void joinedTablesFree(struct joinedTables **pJt)
/* Free up joinable table and associated rows. */
{
struct joinedTables *jt = *pJt;
if (jt != NULL)
    {
    lmCleanup(&jt->lm);
    joinerDtfFreeList(&jt->fieldList);
    joinerDtfFreeList(&jt->keyList);
    freez(pJt);
    }
}

static struct joinedRow *jrRowAdd(struct joinedTables *joined, char **row,
	int fieldCount, int keyCount)
/* Add new row to joinable table. */
{
if (joined->maxRowCount != 0 && joined->rowCount >= joined->maxRowCount)
    {
    warn("Stopping after %d rows, try restricting region or adding filter", 
    	joined->rowCount);
    return NULL;
    }
else
    {
    struct joinedRow *jr;
    int i;
    struct lm *lm = joined->lm;
    lmAllocVar(lm, jr);
    lmAllocArray(lm, jr->fields, joined->fieldCount);
    lmAllocArray(lm, jr->keys, joined->keyCount);
    for (i=0; i<fieldCount; ++i)
	jr->fields[i] = lmCloneString(lm, row[i]);
    row += fieldCount;
    for (i=0; i<keyCount; ++i)
	jr->keys[i] = lmCloneString(lm, row[i]);
    slAddHead(&joined->rowList, jr);
    joined->rowCount += 1;
    return jr;
    }
}

static void jrRowExpand(struct joinedTables *joined, 
	struct joinedRow *jr, char **row,
        int fieldOffset, int fieldCount, int keyOffset, int keyCount)
/* Add some more to row. */
{
int i;
char **keys = jr->keys + keyOffset;
char **fields = jr->fields + fieldOffset;
struct lm *lm = joined->lm;

if (fieldCount + fieldOffset > joined->fieldCount)
    internalErr();
if (keyCount + keyOffset > joined->keyCount)
    internalErr();
for (i=0; i<fieldCount; ++i)
    {
    if (fields[i] == NULL)
        fields[i] = lmCloneString(lm, row[i]);
    else
        {
	int oldLen = strlen(fields[i]);
	int newLen = oldLen + strlen(row[i]) + 2;
	char *newString = lmAlloc(lm, newLen);
	strcpy(newString, fields[i]);
	newString[oldLen] = ',';
	strcpy(newString+oldLen+1, row[i]);
	fields[i] = newString;
	}
    }
row += fieldCount;
for (i=0; i<keyCount; ++i)
    keys[i] = lmCloneString(lm, row[i]);
}

static char *chopKey(struct slName *prefixList, 
	struct slName *suffixList, char *key)
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

static struct joinerDtf *fieldsToDtfs(struct slName *fieldList)
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

static struct dyString *makeOrderedCommaFieldList(struct sqlConnection *conn, 
	char *table, struct joinerDtf *dtfList)
/* Assumes that dtfList all points to same table.  This will return 
 * a comma-separated field list in the same order as the fields are 
 * in database. */
{
struct joinerDtf *dtf;
char *split = chromTable(conn, table);
struct slName *dbField, *dbFieldList = sqlListFields(conn, split);
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
freez(&split);
return dy;
}

struct tableJoiner
/* List of fields in a single table. */
    {
    struct tableJoiner *next;	/* Next in list. */
    char *database;		/* Database we're in.  Not alloced here. */
    char *table;		/* Table we're in.  Not alloced here. */
    struct joinerDtf *fieldList;	/* Fields. */
    struct slRef *keysOut;	/* Keys that connect to output.
                                 * Value is joinerPair. */
    boolean loaded;	/* If true is loaded. */
    };

void tableJoinerFree(struct tableJoiner **pTf)
/* Free up memory associated with tableJoiner. */
{
struct tableJoiner *tj = *pTf;
if (tj != NULL)
    {
    joinerDtfFreeList(&tj->fieldList);
    slFreeList(&tj->keysOut);
    freez(pTf);
    }
}

void tableJoinerFreeList(struct tableJoiner **pList)
/* Free a list of dynamically allocated tableJoiner's */
{
struct tableJoiner *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    tableJoinerFree(&el);
    }
*pList = NULL;
}


struct tableJoiner *bundleFieldsIntoTables(struct joinerDtf *dtfList)
/* Convert list of fields to list of tables.  */
{
struct hash *hash = newHash(0);
char fullName[256];
struct joinerDtf *dtf, *dupe;
struct tableJoiner *tjList = NULL, *tj;

for (dtf = dtfList; dtf != NULL; dtf = dtf->next)
    {
    safef(fullName, sizeof(fullName), "%s.%s", dtf->database, dtf->table);
    tj = hashFindVal(hash, fullName);
    if (tj == NULL)
        {
	AllocVar(tj);
	hashAdd(hash, fullName, tj);
	tj->database = dtf->database;
	tj->table = dtf->table;
	slAddHead(&tjList, tj);
	}
    dupe = joinerDtfClone(dtf);
    slAddTail(&tj->fieldList, dupe);
    }
slReverse(&tjList);
return tjList;
}

struct joinerDtf *tableFirstFieldList(struct tableJoiner *tjList)
/* Make dtfList that is just the first one in each table's field list. */
{
struct joinerDtf *list = NULL, *dtf;
struct tableJoiner *tj;

for (tj = tjList; tj != NULL; tj = tj->next)
    {
    struct joinerDtf *dtf = tj->fieldList;
    struct joinerDtf *dupe = joinerDtfNew(dtf->database, dtf->table, dtf->field);
    slAddHead(&list, dupe);
    }
slReverse(&list);
return list;
}

static struct tableJoiner *findTable(struct tableJoiner *tjList, char *db, char *table)
/* Find table that matches. */
{
struct tableJoiner *tj;
for (tj = tjList; tj != NULL; tj = tj->next)
    if (sameString(tj->database,db) && sameString(tj->table, table))
        return tj;
return NULL;
}

int tableFieldsCmp(const void *va, const void *vb)
/* Compare two tableJoiner. */
{
const struct tableJoiner *a = *((struct tableJoiner **)va);
const struct tableJoiner *b = *((struct tableJoiner **)vb);
int diff;
diff = strcmp(a->database, b->database);
if (diff == 0)
    diff = strcmp(a->table, b->table);
return diff;
}

void orderTables(struct tableJoiner **pTfList, char *primaryDb, char *primaryTable)
/* Order table list so that primary table is first and the rest are
 * alphabetical.  This is the order they are listed in the UI.
 * (However we get them scrambled since the CGI variables are not
 * ordered.)*/
{
struct tableJoiner *tjList = *pTfList;
struct tableJoiner *primaryTf = findTable(tjList, primaryDb, primaryTable);
if (primaryTf != NULL)
    slRemoveEl(&tjList, primaryTf);
slSort(&tjList, tableFieldsCmp);
if (primaryTf != NULL)
    {
    slAddHead(&tjList, primaryTf);
    }
*pTfList = tjList;
}

boolean inKeysOut(struct tableJoiner *tj, struct joinerPair *route)
/* See if key implied by route is already in tableJoiner. */
{
struct slRef *ref;
for (ref = tj->keysOut; ref != NULL; ref = ref->next)
    {
    struct joinerPair *jp = ref->val;
    struct joinerDtf *a = jp->a;
    if (sameString(a->database, route->a->database)
     && sameString(a->table, route->a->table)
     && sameString(a->field, route->a->field))
         return TRUE;
    }
return FALSE;
}

void addOutKeys(struct hash *tableHash, struct joinerPair *routeList, 
	struct tableJoiner **pTfList)
/* Add output keys to tableJoiners.  These are in table hash.
 * We may also have to add some fieldLess tables to the mix,
 * which will get appended to *pTfList, and added to the hash
 * if need be. */
{
struct joinerPair *route;
struct tableJoiner *tj;
char fullName[256];
struct slName *key;

for (route = routeList; route != NULL; route = route->next)
    {
    struct joinerDtf *a = route->a;
    safef(fullName, sizeof(fullName), "%s.%s", a->database, a->table);
    tj = hashFindVal(tableHash, fullName);
    if (tj == NULL)
        {
	AllocVar(tj);
	tj->database = a->database;
	tj->table = a->table;
	slAddTail(pTfList, tj);
	hashAdd(tableHash, fullName, tj);
	}
    if (!inKeysOut(tj, route))
	refAdd(&tj->keysOut, route);
    }
}

void dyStringAddList(struct dyString *dy, char *s)
/* Add s to end of string.  If string is non-empty
 * first add comma.  This make it relatively easy to
 * build up comma-separates lists. */
{
if (dy->stringSize > 0)
    dyStringAppendC(dy, ',');
dyStringAppend(dy, s);
}

void tjLoadSome(struct region *regionList,
    struct joinedTables *joined, int fieldOffset, int keyOffset,
    char *idField, struct hash *idHash, 
    struct slName *chopBefore, struct slName *chopAfter,
    struct tableJoiner *tj, boolean isPositional, boolean isFirst)
/* Load up rows. */
{
struct region *region;
struct dyString *sqlFields = dyStringNew(0);
struct joinerDtf *dtf;
struct slRef *ref;
struct joinerPair *jp;
int fieldCount = 0, keyCount = 0;
int idFieldIx = -1;
struct sqlConnection *conn = sqlConnect(tj->database);

/* Create field spec for sql - first fields user will see, and
 * second keys if any. */
for (dtf = tj->fieldList; dtf != NULL; dtf = dtf->next)
    {
    struct joinerDtf *dupe = joinerDtfClone(dtf);
    slAddTail(&joined->fieldList, dupe);
    dyStringAddList(sqlFields, dtf->field);
    ++fieldCount;
    }
for (ref = tj->keysOut; ref != NULL; ref = ref->next)
    {
    struct joinerDtf *dupe;
    jp = ref->val;
    dupe = joinerDtfClone(jp->a);
    slAddTail(&joined->keyList, dupe);
    dyStringAddList(sqlFields, jp->a->field);
    ++keyCount;
    }
if (idHash != NULL)
    {
    if (idField == NULL)
        internalErr();
    idFieldIx = fieldCount + keyCount;
    dyStringAddList(sqlFields, idField);
    }

for (region = regionList; region != NULL; region = region->next)
    {
    char **row;
    struct sqlResult *sr = regionQuery(conn, tj->table, 
    	sqlFields->string, region, isPositional);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (idFieldIx < 0)
	    {
	    if (jrRowAdd(joined, row, fieldCount, keyCount) == NULL)
	        break;
	    }
	else
	    {
	    char *id = row[idFieldIx];
	    char *e;
	    if (isFirst)
		{
		if (hashLookup(idHash, id))
		    {
		    if (jrRowAdd(joined, row, fieldCount, keyCount) == NULL)
		        break;
		    }
		}
	    else
		{
		struct joinedRow *jr;
		struct hashEl *bucket;
		id = chopKey(chopBefore, chopAfter, id);
		for (bucket = hashLookup(idHash, id); bucket != NULL;
		     bucket = hashLookupNext(bucket))
		     {
		     jr = bucket->val;
		     jrRowExpand(joined, jr, row, 
		    	fieldOffset, fieldCount, keyOffset, keyCount);
		     }
		}
	    }
	}
    sqlFreeResult(&sr);
    if (!isPositional)
        break;
    }
if (isFirst)
    slReverse(&joined->rowList);
tj->loaded = TRUE;
sqlDisconnect(&conn);
}
	
struct joinedTables *tjLoadFirst(struct region *regionList,
	struct tableJoiner *tj, int totalFieldCount,
	int totalKeyCount, int maxRowCount)
/* Load up first table in series of joined tables.  This allocates
 * field and key arrays big enough for all. */
{
struct joinedTables *joined = joinedTablesNew(totalFieldCount, 
	totalKeyCount, maxRowCount);
struct hash *idHash = NULL;
char *idField = NULL;
struct hTableInfo *hti = getHti(tj->database, tj->table);
if (hti != NULL && hti->nameField[0] != 0)
    {
    idHash = identifierHash();
    idField = hti->nameField;
    }
tjLoadSome(regionList, joined, 0, 0, 
	idField, idHash, NULL, NULL, tj, 
	isPositional(tj->database, tj->table), TRUE);
hashFree(&idHash);
return joined;
}

int findDtfIndex(struct joinerDtf *list, struct joinerDtf *dtf)
/* Find dtf on list. */
{
struct joinerDtf *el;
int ix;
for (el = list, ix=0; el != NULL; el = el->next, ++ix)
    {
    if (sameString(el->database, dtf->database)
     && sameString(el->table, dtf->table)
     && sameString(el->field, dtf->field))
        return ix;
    }
return -1;
}

struct hash *hashKeyField(struct joinedTables *joined, int keyIx,
	struct slName *chopBefore, struct slName *chopAfter)
/* Make a hash based on key field. */
{
int hashSize = digitsBaseTwo(joined->rowCount);
struct hash *hash = NULL;
struct joinedRow *jr;

if (hashSize > 20)
    hashSize = 20;
hash = newHash(hashSize);
for (jr = joined->rowList; jr != NULL; jr = jr->next)
    {
    char *key = jr->keys[keyIx];
    if (key != NULL)
	{
	key = chopKey(chopBefore, chopAfter, jr->keys[keyIx]);
	hashAdd(hash, key, jr);
	}
    }
return hash;
}

struct tableJoiner *findTableJoiner(struct tableJoiner *list, 
    char *database, char *table)
/* Find tableJoiner for given database/table. */
{
struct tableJoiner *el;
for (el = list; el != NULL; el = el->next)
    {
    if (sameString(el->database, database) && sameString(el->table, table))
        return el;
    }
return NULL;
}

static struct joinerField *findJoinerField(struct joinerSet *js, 
	struct joinerDtf *dtf)
/* Find field in set if any that matches dtf */
{
struct joinerField *jf;
while (js != NULL)
    {
    for (jf = js->fieldList; jf != NULL; jf = jf->next)
	 {
	 if (sameString(dtf->table, jf->table) && sameString(dtf->field, jf->field))
	     {
	     if (slNameInList(jf->dbList, dtf->database))
		 return jf;
	     }
	 }
    js = js->parent;
    }
return NULL;
}



struct joinedTables *joinedTablesCreate( struct joiner *joiner, 
	char *primaryDb, char *primaryTable,
	struct joinerDtf *fieldList, int maxRowCount)
/* Create joinedTables structure from fields. */
{
struct tableJoiner *tj, *tjList = bundleFieldsIntoTables(fieldList);
struct joinerPair *routeList = NULL, *route;
struct joinerDtf *tableDtfs = NULL;
struct joinedTables *joined = NULL;
struct hash *tableHash = newHash(8);
struct region *regionList = getRegions();
int totalKeyCount = 0, totalFieldCount = 0;
int curKeyCount = 0, curFieldCount = 0;

for (tj = tjList; tj != NULL; tj = tj->next)
    {
    char buf[256];
    safef(buf, sizeof(buf), "%s.%s", tj->database, tj->table);
    hashAdd(tableHash, buf, tj);
    }
orderTables(&tjList, primaryDb, primaryTable);
tableDtfs = tableFirstFieldList(tjList);
routeList = joinerFindRouteThroughAll(joiner, tableDtfs);
addOutKeys(tableHash, routeList, &tjList);

/* Count up total fields and keys. */
for (tj = tjList; tj != NULL; tj = tj->next)
    {
    totalKeyCount += slCount(tj->keysOut);
    totalFieldCount += slCount(tj->fieldList);
    }

/* Do first table.  This one uses identifier hash if any. */
    {
    joined = tjLoadFirst(regionList,
	    tjList, totalFieldCount, totalKeyCount, maxRowCount);
    curKeyCount = slCount(tjList->keysOut);
    curFieldCount = slCount(tjList->fieldList);
    }

/* Follow routing list for rest. */
if (!sameString(tjList->database, routeList->a->database))
    internalErr();
if (!sameString(tjList->table, routeList->a->table))
    internalErr();
for (route = routeList; route != NULL; route = route->next)
    {
    struct tableJoiner *tj = findTableJoiner(tjList, 
	    route->b->database, route->b->table);
    struct joinerField *jfA = NULL, *jfB = NULL;
    if (tj == NULL)
	internalErr();
    jfA = findJoinerField(route->identifier, route->a);
    if (jfA == NULL)
	internalErr();
    jfB = findJoinerField(route->identifier, route->b);
    if (jfB == NULL)
	internalErr();
    if (!tj->loaded)
	{
	struct hTableInfo *hti;
	int keyIx;
	struct hash *keyHash = NULL;
	keyIx = findDtfIndex(joined->keyList, route->a);
	if (keyIx < 0)
	    internalErr();
	keyHash = hashKeyField(joined, keyIx, 
	    jfA->chopBefore, jfA->chopAfter);
	tjLoadSome(regionList, joined, curFieldCount, curKeyCount,
	    route->b->field, keyHash, 
	    jfB->chopBefore, jfB->chopAfter, 
	    tj, isPositional(tj->database, tj->table),  FALSE);
	curKeyCount += slCount(tj->keysOut);
	curFieldCount += slCount(tj->fieldList);
	hashFree(&keyHash);
	}
    }
joinerDtfFreeList(&tableDtfs);
hashFree(&tableHash);
tableJoinerFreeList(&tjList);
return joined;
}

void tabOutSelectedFields(
	char *primaryDb,		/* The primary database. */
	char *primaryTable, 		/* The primary table. */
	struct slName *fieldList)	/* List of db.table.field */
/* Do tab-separated output on selected fields, which may
 * or may not include multiple tables. */
{
struct joinerDtf *dtfList = fieldsToDtfs(fieldList);

if (joinerDtfAllSameTable(dtfList))
    {
    struct sqlConnection *conn = sqlConnect(dtfList->database);
    struct dyString *dy = makeOrderedCommaFieldList(conn, 
    	dtfList->table, dtfList);
    doTabOutTable(dtfList->database, dtfList->table, conn, dy->string);
    sqlDisconnect(&conn);
    }
else
    {
    struct joiner *joiner = joinerRead("all.joiner");
    struct joinedTables *joined = joinedTablesCreate(joiner, 
    	primaryDb, primaryTable, dtfList, 1000000);
    joinedTablesTabOut(joined);
    joinedTablesFree(&joined);
    }
joinerDtfFreeList(&dtfList);
}


