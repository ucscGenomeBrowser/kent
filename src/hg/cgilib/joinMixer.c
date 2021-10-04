/* joinMixer - implement multiple joins by sql joins and/or hashJoins, depending on which
 * should work best for each joined table. */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "joinMixer.h"
#include "hdb.h"

static void addUniqueJoinerDt(struct joinerDtf **pTableList, struct hash *uniqHash,
                              struct joinerDtf *dt, char *db)
/* Use a string key into the hash to build up a unique list of cloned joinerDtf's with
 * ignored (possibly NULL) fields (table-only, hence "dt" not "dtf"). */
{
char dbTable[PATH_LEN];
joinerDtfToSqlTableString(dt, db, dbTable, sizeof(dbTable));
if (! hashLookup(uniqHash, dbTable))
    {
    slAddHead(pTableList, joinerDtfClone(dt));
    hashAdd(uniqHash, dbTable, NULL);
    }
}

static struct joinerDtf *tablesInOutput(char *db, char *mainTable,
                                        struct joinerDtf *outputFieldList)
/* Return a uniquified list of tables (NULL for field) that we need for selected output fields
 * and filters, if applicable.  Joining these tables may require additional intermediate tables. */
{
struct hash *uniqHash = hashNew(0);
struct joinerDtf *tableList = NULL;
// Always include the main table, even if its fields aren't in output.
struct joinerDtf *mainDt = joinerDtfNew(db, mainTable, NULL);
addUniqueJoinerDt(&tableList, uniqHash, mainDt, db);
struct joinerDtf *dtf;
for (dtf = outputFieldList;  dtf != NULL;  dtf = dtf->next)
    addUniqueJoinerDt(&tableList, uniqHash, dtf, db);
hashFree(&uniqHash);
slReverse(&tableList);
return tableList;
}

static boolean noFancyKeywords(struct joinerField *jfA, struct joinerField *jfB)
/* Return TRUE unless jfA or jfB has a keyword that is too fancy for sql joins. */
{
if (isNotEmpty(jfA->separator) && differentString(jfA->separator, ","))
    return FALSE;
if (isNotEmpty(jfB->separator))
    return FALSE;
if (jfA->chopBefore || jfA->chopAfter || jfB->chopBefore || jfB->chopAfter)
    return FALSE;
return TRUE;
}

static boolean dependsOn(struct joinerPair *jpA, struct joinerPair *jpBList)
/* Return TRUE if jpA->a's table is found among the ->b's in jpBList. */
{
struct joinerPair *jpB;
for (jpB = jpBList;  jpB != NULL;  jpB = jpB->next)
    {
    if (joinerDtfSameTable(jpA->a, jpB->b))
        return TRUE;
    }
return FALSE;
}

static void partitionJoins(char *db, struct joinerPair *routeList, uint mainTableRowCount,
                           struct joinerPair **retSqlRouteList,
                           struct joinerPair **retHashJoinRouteList)
/* For each table pair in routeList, figure out whether a sql join is possible and
 * perhaps faster than brute force join.  If it looks like sql is faster, add to
 * retSqlRouteList, otherwise to retHashJoinRouteList. */
{
struct sqlConnection *conn = hAllocConn(db);
boolean mysqlFasterForMainTable = mainTableRowCount < 10000;
struct joinerPair *jp, *jpNext;
for (jp = routeList;  jp != NULL;  jp = jpNext)
    {
    jpNext = jp->next;
    boolean useSql = FALSE;
    struct joinerField *jfA = joinerSetFindField(jp->identifier, jp->a);
    struct joinerField *jfB = joinerSetFindField(jp->identifier, jp->b);
    if (noFancyKeywords(jfA, jfB))
        {
        char dbTable[PATH_LEN];
        joinerDtfToSqlTableString(jp->b, db, dbTable, sizeof(dbTable));
        uint rowCountB = sqlRowCount(conn, dbTable);
        boolean relatedMuchBigger = mainTableRowCount + 10000000 < rowCountB;
        if (mysqlFasterForMainTable || relatedMuchBigger)
            useSql = TRUE;
        }
    if (useSql && !dependsOn(jp, *retHashJoinRouteList))
        slAddHead(retSqlRouteList, jp);
    else
        slAddHead(retHashJoinRouteList, jp);
    }
slReverse(retSqlRouteList);
slReverse(retHashJoinRouteList);
hFreeConn(&conn);
}

static struct hash *tablesInRouteList(char *db, char *mainTable, struct joinerPair *routeList)
/* Return a hash of names of tables in routeList to NULL values. */
{
struct hash *hash = hashNew(0);
// Always add mainTable
char dbTable[PATH_LEN];
hashAdd(hash, mainTable, NULL);
// Add all pairs' ->b's
struct joinerPair *jp;
for (jp = routeList;  jp != NULL;  jp = jp->next)
    {
    joinerDtfToSqlTableString(jp->b, db, dbTable, sizeof(dbTable));
    hashAdd(hash, dbTable, NULL);
    }
return hash;
}

static void addField(struct hash *fieldToIx, struct joinerDtf *dtf, char *db, uint *pIx)
/* If stringified dtf is not already in hash, add it with value=*pIx and increment *pIx. */
{
char dtField[PATH_LEN];
joinerDtfToSqlFieldString(dtf, db, dtField, sizeof(dtField));
if (!hashLookup(fieldToIx, dtField))
    {
    hashAddInt(fieldToIx, dtField, *pIx);
    *pIx = *pIx + 1;
    }
}

static uint mustFindIx(struct hash *fieldToIx, struct joinerDtf *dtf, char *db, char *errMsg)
/* Look up nonnegative int value of stringified dtf; die if not found, otherwise return it. */
{
char dtField[PATH_LEN];
joinerDtfToSqlFieldString(dtf, db, dtField, sizeof(dtField));
int bigRowKeyIx = hashIntValDefault(fieldToIx, dtField, -1);
if (bigRowKeyIx < 0)
    errAbort("%s '%s'", errMsg, dtField);
return (uint)bigRowKeyIx;
}

struct hjTableInfo
// Info accumulator for constructing a new hashJoin
{
    struct hjTableInfo *next;
    char *dbTable;
    struct joinerPair *jp;
    struct joinerDtf *hashValCols;
};

static struct hjTableInfo *hjTableInfoNew(char *dbTable, struct joinerPair *jp,
                                          struct joinerDtf *hashValCol)
// Return a new hjTableInfo for dbTable; if jp is non-NULL, jp->b is the hash key field.
{
struct hjTableInfo *self;
AllocVar(self);
self->dbTable = cloneString(dbTable);
self->jp = jp;
self->hashValCols = joinerDtfClone(hashValCol);
return self;
}

static struct hjTableInfo *hjTableInfoFind(struct hjTableInfo *list, char *dbTable)
// Just use slPair find because the first two fields are next and char *name.
{
return (struct hjTableInfo *)slPairFind((struct slPair *)list, dbTable);
}

static void hjTableInfoAddKey(struct hjTableInfo **pList, struct joinerPair *jp, char *db)
// jp->b is the hash key field for its table; look up jp->b's table in pList, add if necessary,
// setting the table's jp.
{
char dbTable[PATH_LEN];
joinerDtfToSqlTableString(jp->b, db, dbTable, sizeof(dbTable));
struct hjTableInfo *infoForTable = hjTableInfoFind(*pList, dbTable);
if (infoForTable == NULL)
    slAddHead(pList, hjTableInfoNew(dbTable, jp, NULL));
else if (infoForTable->jp != NULL)
    errAbort("Multiple keys for %s: %s and %s", dbTable, infoForTable->jp->b->field, jp->b->field);
else
    infoForTable->jp = jp;
}

static void hjTableInfoAddVal(struct hjTableInfo **pList, struct joinerDtf *dtf, char *db)
// Find dtf in pList -- it should be there already if hjRouteList is traversed in order --
// and add dtf to its hash value column list if not already there.
{
char dbTable[PATH_LEN];
joinerDtfToSqlTableString(dtf, db, dbTable, sizeof(dbTable));
struct hjTableInfo *infoForTable = hjTableInfoFind(*pList, dbTable);
if (infoForTable == NULL)
    {
    errAbort("hjTableInfoAddVal: can't find table '%s' in list", dbTable);
    }
else if (! joinerDtfFind(infoForTable->hashValCols, dtf))
    slAddTail(&infoForTable->hashValCols, joinerDtfClone(dtf));
}

static void hjTableInfoFree(struct hjTableInfo **pHjti)
// Free up *pHjti & contents if not NULL.
{
if (! pHjti || ! *pHjti)
    return;
struct hjTableInfo *hjti = *pHjti;
freeMem(hjti->dbTable);
joinerDtfFreeList(&hjti->hashValCols);
freez(pHjti);
}

static void hjTableInfoFreeList(struct hjTableInfo **pHjtiList)
// Free up every member of *pHjtiList if not NULL.
{
if (! pHjtiList || ! *pHjtiList)
    return;
struct hjTableInfo *hjti = *pHjtiList;
while (hjti != NULL)
    {
    struct hjTableInfo *hjtiNext = hjti->next;
    hjTableInfoFree(&hjti);
    hjti = hjtiNext;
    }
*pHjtiList = NULL;
}

struct joinMixer *joinMixerNew(struct joiner *joiner, char *db, char *mainTable,
                               struct joinerDtf *outputFieldList,
                               uint mainTableRowCount, boolean naForMissing)
/* If outputFieldList contains fields from more than one table, use joiner to figure
 * out the route of table joins to relate all fields; for each table, predict whether
 * it would be more efficient to join by sql or by hashJoin, taking into account
 * anticipated row counts.  Return info sufficient for building a sql query, a list
 * of hashJoins, bigRow size, and indexes in bigRow for each output.
 * If naForMissing is TRUE then the hashJoiner result columns will contain "n/a" when
 * there is no match in the hash. */
{
struct joinMixer *jmOut;
AllocVar(jmOut);
// Figure out what tables we need to query, and whether each one should be joined
// by sql or hash:
struct joinerDtf *tablesNeeded = tablesInOutput(db, mainTable, outputFieldList);
struct joinerPair *routeList = joinerFindRouteThroughAll(joiner, tablesNeeded);
struct joinerPair *hjRouteList = NULL;
partitionJoins(db, routeList, mainTableRowCount, &jmOut->sqlRouteList, &hjRouteList);
// routeList was clobbered, make sure we don't try to use it again:
routeList = NULL;
char dbTable[PATH_LEN];
// Split output fields into those that come from sql and those that come from hashJoins:
// Assign indices in external row to each output from mysql.
struct hash *fieldToIx = hashNew(0);
uint bigRowIx = 0;
struct hash *sqlTables = tablesInRouteList(db, mainTable, jmOut->sqlRouteList);
struct joinerDtf *hjFieldList = NULL;
struct joinerDtf *dtf;
for (dtf = outputFieldList;  dtf != NULL;  dtf = dtf->next)
    {
    joinerDtfToSqlTableString(dtf, db, dbTable, sizeof(dbTable));
    if (hashLookup(sqlTables, dbTable) || sameString(dbTable, mainTable))
        {
        slAddHead(&jmOut->sqlFieldList, joinerDtfClone(dtf));
        addField(fieldToIx, dtf, db, &bigRowIx);
        }
    else
        {
        slAddHead(&hjFieldList, joinerDtfClone(dtf));
        // Don't add to bigRow/fieldToIx because we might need to tack on some sql fields
        // for hashJoin keys.
        }
    }
slReverse(&jmOut->sqlFieldList);
slReverse(&hjFieldList);

// Add hashJoin jp->a keys that come from sqlTables to sqlFieldList and bigRow if not already
// in sqlFieldList
struct joinerPair *jp;
for (jp = hjRouteList;  jp != NULL;  jp = jp->next)
    {
    joinerDtfToSqlTableString(jp->a, db, dbTable, sizeof(dbTable));
    if (hashLookup(sqlTables, dbTable) && !joinerDtfFind(jmOut->sqlFieldList, jp->a))
        {
        slAddTail(&jmOut->sqlFieldList, joinerDtfClone(jp->a));
        addField(fieldToIx, jp->a, db, &bigRowIx);
        }
    }

// Now that sqlFieldList is complete, add hashJoin output fields to bigRow.
for (dtf = hjFieldList;  dtf != NULL;  dtf = dtf->next)
    addField(fieldToIx, dtf, db, &bigRowIx);

// Add hashJoin key info (jp->b) to hjTableCols.  If any hashJoin key fields take values
// produced by other hashJoins (jp->a), but those columns don't appear in output, add them
// to bigRow.
struct hjTableInfo *hjTableCols = NULL;
for (jp = hjRouteList;  jp != NULL;  jp = jp->next)
    {
    hjTableInfoAddKey(&hjTableCols, jp, db);
    joinerDtfToSqlTableString(jp->a, db, dbTable, sizeof(dbTable));
    if (! hashLookup(sqlTables, dbTable))
        hjTableInfoAddVal(&hjTableCols, jp->a, db);
    addField(fieldToIx, jp->a, db, &bigRowIx);
    }

// Done assigning bigRow indices; set bigRowSize.
jmOut->bigRowSize = bigRowIx;

// Add hash output fields to hjTableCols
for (dtf = hjFieldList;  dtf != NULL;  dtf = dtf->next)
    hjTableInfoAddVal(&hjTableCols, dtf, db);

// Build up jmOut->hashJoins
slReverse(&hjTableCols);
struct hjTableInfo *hjti;
int i;
for (i = 0, hjti = hjTableCols;  hjti != NULL;  hjti = hjti->next, i++)
    {
    if (hjti->jp == NULL)
        errAbort("hjTableInfo for %s has NULL jp", hjti->dbTable);
    if (hjti->hashValCols == NULL)
        errAbort("hjTableInfo for %s has NULL hashValCols", hjti->dbTable);
    uint bigRowKeyIx = mustFindIx(fieldToIx, hjti->jp->a, db,
                                  "joinMixerNew: Can't find index of hashJoin index");
    struct joinerField *jfA = joinerSetFindField(hjti->jp->identifier, hjti->jp->a);
    struct joinerField *jfB = joinerSetFindField(hjti->jp->identifier, hjti->jp->b);
    uint valCount = slCount(hjti->hashValCols);
    uint bigRowColIxs[valCount];
    struct joinerDtf *col;
    int colIx;
    for (colIx = 0, col = hjti->hashValCols;  col != NULL;  col = col->next, colIx++)
        bigRowColIxs[colIx] = mustFindIx(fieldToIx, col, db,
                                         "joinMixerNew: Missing bigRow ix for");
    slAddHead(&jmOut->hashJoins,
              hashJoinNew(hjti->jp->b, bigRowKeyIx,
                          hjti->hashValCols, bigRowColIxs,
                          jfA, jfB, naForMissing));
    }
slReverse(&jmOut->hashJoins);

// Fill in each output field's index into bigRow
AllocArray(jmOut->outIxs, slCount(outputFieldList));
uint outRowIx;
for (outRowIx = 0, dtf = outputFieldList;  dtf != NULL;  dtf = dtf->next, outRowIx++)
    jmOut->outIxs[outRowIx] = mustFindIx(fieldToIx, dtf, db,
                                        "joinMixerNew: no bigRowIx for output field");

joinerPairFreeList(&hjRouteList);
joinerDtfFreeList(&hjFieldList);
hjTableInfoFreeList(&hjTableCols);
hashFree(&fieldToIx);
hashFree(&sqlTables);
joinerDtfFreeList(&tablesNeeded);
return jmOut;
}

void joinMixerFree(struct joinMixer **pJm)
/* Free joinMixer's holdings unless already NULL. */
{
if (!pJm || !*pJm)
    return;
struct joinMixer *jm = *pJm;
joinerPairFreeList(&jm->sqlRouteList);
joinerDtfFreeList(&jm->sqlFieldList);
hashJoinFreeList(&jm->hashJoins);
freeMem(jm->outIxs);
freez(pJm);
}
