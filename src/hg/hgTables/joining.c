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
#include "hdb.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: joining.c,v 1.7 2004/07/17 06:48:17 kent Exp $";

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
    struct lm *lm;	/* Local memory structure - used for all allocations. */
    char **fieldNames;	/* Names of fields. */
    int fieldCount;	/* Number of fields user has requested. */
    char **keyNames;	/* Names of keys. */
    int keyCount;	/* Number of keys. */
    struct joinedRow *rowList;	/* Rows. */
    int rowCount;	/* Number of rows. */
    };

void dumpJt(struct joinedTables *jt)
/* Write out fields. */
{
struct joinedRow *jr;
for (jr = jt->rowList; jr != NULL; jr = jr->next)
    {
    int i;
    for (i=0; i<jt->fieldCount; ++i)
        printf("\t%s", jr->fields[i]);
    printf("\n");
    }
}

struct joinedTables *joinedTablesNew(int fieldCount, int keyCount)
/* Make up new empty joinedTables. */
{
struct joinedTables *jt;
struct lm *lm;

AllocVar(jt);
lm = jt->lm = lmInit(64*1024);
jt->fieldCount = fieldCount;
lmAllocArray(lm, jt->fieldNames, jt->fieldCount);
jt->keyCount = keyCount;
lmAllocArray(lm, jt->keyNames, jt->keyCount);
return jt;
}

void joinedTablesFree(struct joinedTables **pJt)
/* Free up joinable table and associated rows. */
{
struct joinedTables *jt = *pJt;
if (jt != NULL)
    {
    lmCleanup(&jt->lm);
    freez(pJt);
    }
}

struct joinedRow *jrRowAdd(struct joinedTables *joined, char **row,
	int fieldCount, int keyCount)
/* Add new row to joinable table. */
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

void jrRowExpand(struct joinedTables *joined, struct joinedRow *jr, char **row,
     int fieldOffset, int fieldCount, int keyOffset, int keyCount)
/* Add some more to row. */
{
int i;
char **keys = jr->keys + keyOffset;
char **fields = jr->fields + fieldOffset;
struct lm *lm = joined->lm;

if (fieldCount + fieldOffset > joined->keyCount)
    internalErr();
if (keyCount + keyOffset > joined->keyCount)
    internalErr();
for (i=0; i<fieldCount; ++i)
    fields[i] = lmCloneString(lm, row[i]);
row += fieldCount;
for (i=0; i<keyCount; ++i)
    jr->keys[i] = lmCloneString(lm, row[i]);
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

#ifdef OLD
struct joinedTables *fetchKeyedFields(struct region *regionList,
	struct sqlConnection *conn, char *table, boolean isPositional, char *fields, 
	char *keyIn, struct hash *keyInHash, 
	struct slName *chopBefore, struct slName *chopAfter,
	struct slName *keyOutList)
/* This returns a list of keyedRows filtering out those that
 * don't match on keyIn.  */
{
struct joinedTables *jt = joinedTablesNew(table, fields, keyOutList);
struct dyString *fieldSpec = dyStringNew(0);
struct slName *keyOut;
struct region *region;
int keyInField = -1;

uglyf("fetchKeyedFields from %s,  fields %s, keyIn %s\n",
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
	    jrRowAdd(jt, row);
	else
	    {
	    char *key = row[keyInField];
	    char *e;
	    if (hashLookup(keyInHash, chopKey(NULL, NULL, key)))
		jrRowAdd(jt, row);
	    }
	}
    sqlFreeResult(&sr);
    if (!isPositional)
        break;
    }
slReverse(&jt->rowList);
return jt;
}
#endif /* OLD */

struct hash *hashKeyField(struct joinedTables *jt, char *keyField,
	struct slName *chopPrefix, struct slName *chopSuffix)
/* Make a hash based on key field. */
{
int ix = stringArrayIx(keyField, jt->keyNames, jt->keyCount);
int hashSize = digitsBaseTwo(jt->rowCount);
struct hash *hash = NULL;
struct joinedRow *jr;

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

struct tableJoiner
/* List of fields in a single table. */
    {
    struct tableJoiner *next;	/* Next in list. */
    char *database;		/* Database we're in.  Not alloced here. */
    char *table;		/* Table we're in.  Not alloced here. */
    struct joinerDtf *fieldList;	/* Fields. */
    struct slRef *keysOut;	/* Keys that connect to output - value is joinerPair. */
    boolean loaded;	/* If true is loaded. */
    };

void tableFieldsFree(struct tableJoiner **pTf)
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

void tableFieldsFreeList(struct tableJoiner **pList)
/* Free a list of dynamically allocated tableJoiner's */
{
struct tableJoiner *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    tableFieldsFree(&el);
    }
*pList = NULL;
}


struct tableJoiner *bundleFieldsIntoTables(struct joinerDtf **pDtfList)
/* Convert list of fields to list of tables.  This will eat up the
 * input list and transfer it to the fieldList element of the tables. */
{
struct hash *hash = newHash(0);
char fullName[256];
struct joinerDtf *dtf, *next;
struct tableJoiner *tjList = NULL, *tj;

for (dtf = *pDtfList; dtf != NULL; dtf = next)
    {
    next = dtf->next;
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
    slAddHead(&tj->fieldList, dtf);
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
	uglyf("Adding empty fielded table %s for joining", fullName);
	AllocVar(tj);
	tj->database = a->database;
	tj->table = a->table;
	slAddTail(pTfList, tj);
	hashAdd(tableHash, fullName, tj);
	}
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
    char *idField, struct hash *idHash, struct tableJoiner *tj,
    struct hTableInfo *hti, boolean isFirst)
/* Load up rows. */
{
struct region *region;
struct dyString *sqlFields = dyStringNew(0);
struct joinerDtf *dtf;
struct slRef *ref;
struct joinerPair *jp;
int fieldCount = 0, keyCount = 0;
int idFieldIx = -1;
boolean isPositional = htiIsPositional(hti);
struct sqlConnection *conn = sqlConnect(tj->database);

/* Create field spec for sql - first fields user will see, and
 * second keys if any. */
for (dtf = tj->fieldList; dtf != NULL; dtf = dtf->next)
    {
    dyStringAddList(sqlFields, dtf->field);
    ++fieldCount;
    }
for (ref = tj->keysOut; ref != NULL; ref = ref->next)
    {
    jp = ref->val;
    dyStringAddList(sqlFields, jp->a->field);
    ++keyCount;
    }
if (idHash != NULL)
    {
    if (idField == NULL)
        internalErr();
    dyStringAddList(sqlFields, idField);
    idFieldIx = fieldCount + keyCount;
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
	    jrRowAdd(joined, row, fieldCount, keyCount);
	    }
	else
	    {
	    char *id = row[idFieldIx];
	    char *e;
	    if (isFirst)
		{
		if (hashLookup(idHash, chopKey(NULL, NULL, id)))
		    {
		    jrRowAdd(joined, row, fieldCount, keyCount);
		    }
		}
	    else
		{
		struct joinedRow *jr = hashFindVal(idHash, id);
		if (jr != NULL)
		    jrRowExpand(joined, jr, row, 
		    	fieldOffset, fieldCount, keyOffset, keyCount);
		}
	    }
	}
    sqlFreeResult(&sr);
    if (!isPositional)
        break;
    }
if (isFirst)
    slReverse(&joined->rowList);
tj->Loaded = TRUE;
sqlDisconnect(&conn);
}
	
struct joinedTables *tjLoadFirst(struct region *regionList,
	struct tableJoiner *tj, int totalFieldCount,
	int totalKeyCount)
/* Load up first table in series of joined tables.  This allocates
 * field and key arrays big enough for all. */
{
struct joinedTables *joined = joinedTablesNew(totalFieldCount, totalKeyCount);
struct hash *idHash = NULL;
char *idField = NULL;
struct hTableInfo *hti = getHti(tj->database, tj->table);
if (hti->nameField[0] != 0)
    {
    idHash = identifierHash();
    idField = hti->nameField;
    }
tjLoadSome(regionList, joined, 0, 0, idField, idHash, tj, hti, TRUE);
hashFree(&idHash);
return joined;
}

void tabOutFields(
	char *primaryDb,		/* The primary database. */
	char *primaryTable, 		/* The primary table. */
	struct slName *fieldList)	/* List of db.table.field */
/* Do tab-separated output. */
{
struct joinerDtf *dtfList = NULL;
struct tableJoiner *tjList = NULL, *tj;

dtfList = fieldsToDtfs(fieldList);
tjList = bundleFieldsIntoTables(&dtfList);

if (slCount(tjList) == 1)
    {
    tj = tjList;
    struct sqlConnection *conn = sqlConnect(tj->database);
    struct dyString *dy = makeOrderedCommaFieldList(conn, tj->table, tj->fieldList);
    doTabOutTable(tj->table, conn, dy->string);
    sqlDisconnect(&conn);
    }
else
    {
    struct joiner *joiner = joinerRead("all.joiner");
    struct joinerPair *routeList, *route, *lastRoute;
    struct joinerDtf *tableDtfs;
    struct joinedTables *joined = NULL;
    struct hash *tableHash = newHash(8);
    struct sqlConnection *conn = sqlConnect(primaryDb);
    struct region *regionList = getRegions(conn);
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

    uglyf("Got %d hops in route\n", slCount(routeList));
    uglyf("%d total keys, %d total fields\n", totalKeyCount, totalFieldCount);
    for (route = routeList; route != NULL; route = route->next)
        uglyf("%s.%s.%s -> %s.%s.%s\n", route->a->database, route->a->table, route->a->field, route->b->database, route->b->table, route->b->field);

    uglyf("\n");
    uglyf("got %d tables\n", slCount(tjList));

    /* Do first table.  This one uses identifier hash if any. */
	{
	struct joinedTables *jt = tjLoadFirst(regionList,
		tjList, totalFieldCount, totalKeyCount);
	curKeyCount = slCount(tjList->keysOut);
	curFieldCount = slCount(tjList->fieldList);
	}

    for (tj = tjList->next; tj != NULL; tj = tj->next)
        {
	}
#ifdef SOON
    for (tj = tjList; tj != NULL; tj = tj->next)
        {
	struct slRef *keyRef;
	uglyf("%s.%s", tj->database, tj->table);
	for (keyRef = tj->keysOut; keyRef != NULL; keyRef = keyRef->next)
	    {
	    struct joinerPair *jp = keyRef->val;
	    uglyf(" %s", jp->a->field);
	    }
	uglyf("\n");
	}

    lastRoute = NULL;
    for (route = routeList; route != NULL; route = route->next)
         {
	 char fullName[256];
	 struct joinerDtf *a = route->a;
	 safef(fullName, sizeof(fullName), "%s.%s", a->database, a->table);
	 tj = hashMustFindVal(tableHash, fullName);
	 uglyf("I'll do something on this route someday\n");
	 lastRoute = route;
	 }
    sqlDisconnect(&conn);
    joinerDtfFreeList(&tableDtfs);
    hashFree(&tableHash);
#endif /* SOON */
    }
tableFieldsFreeList(&tjList);
}

void doTest(struct sqlConnection *conn)
/* Put up a page to see what happens. */
{
struct slName *fieldList = NULL, *field;
field = slNameNew("hg16.softberryGene.name");
slAddTail(&fieldList, field);
field = slNameNew("hg16.softberryHom.description");
slAddTail(&fieldList, field);
#ifdef SOON
field = slNameNew("hg16.knownGene.name");
slAddTail(&fieldList, field);
field = slNameNew("hg16.kgXref.geneSymbol");
slAddTail(&fieldList, field);
field = slNameNew("hg16.bioCycPathway.mapID");
slAddTail(&fieldList, field);
#endif

textOpen();
tabOutFields("hg16", "softberryGene", fieldList);
}


