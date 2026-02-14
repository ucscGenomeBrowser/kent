#include "common.h"
#include "bPlusTree.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "hgFind.h"
#include "trix.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "hdb.h"
#include "errCatch.h"
#include "bigBedLabel.h"
#include "bigBedFind.h"
#include "genbank.h"

static struct hgPos *bigBedIntervalListToHgPositions(struct cart *cart, struct trackDb *tdb,
                        struct bbiFile *bbi, char *term, struct bigBedInterval *intervalList,
                        char *description, struct hgFindSpec *hfs, char *db)
/* Given an open bigBed file, and an interval list, return a pointer to a list of hgPos structures. */
{
struct hgPos *posList = NULL;
char chromName[bbi->chromBpt->keySize+1];
int lastChromId = -1;
struct bigBedInterval *interval;
struct slInt *labelColumns = NULL;

struct asObject *as = NULL;
int ncbiIdIx = -1, geneNameIx = -1;
struct sqlConnection *conn = NULL;
if (hfs && (sameString(hfs->searchName, "mane") || sameString(hfs->searchName, "hgnc")))
    {
    // TODO: right now we are only doing this for MANE and HGNC, but if we are gonna add
    // special descriptions to more tracks in the future then we should have some generic
    // way of attaching a description to an hfs, whether that hfs is a mysql search or a
    // bigBed search
    as = bigBedAsOrDefault(bbi);
    if (sameString(hfs->searchName, "mane"))
        {
        conn = hAllocConn(db);
        ncbiIdIx = asColumnFindIx(as->columnList, "ncbiId");
        }
    else if (sameString(hfs->searchName, "hgnc"))
        geneNameIx = asColumnFindIx(as->columnList, "geneName");
    }

bigBedLabelCalculateFields(cart, tdb, bbi,  &labelColumns );
for (interval = intervalList; interval != NULL; interval = interval->next)
    {
    struct hgPos *hgPos;
    AllocVar(hgPos);
    slAddHead(&posList, hgPos);

    bbiCachedChromLookup(bbi, interval->chromId, lastChromId, chromName, sizeof(chromName));
    lastChromId = interval->chromId;

    hgPos->chrom = cloneString(chromName);
    hgPos->chromStart = interval->start;
    hgPos->chromEnd = interval->end;
    hgPos->name = bigBedMakeLabel(tdb, labelColumns, interval, chromName);
    hgPos->browserName = cloneString(term);
    hgPos->description = cloneString(description);
    if (hfs)
        {
        char *paddingStr = hgFindSpecSetting(hfs, "padding");
        int padding = isEmpty(paddingStr) ? 0 : atoi(paddingStr);
        if (padding > 0)
            {
            // highlight the item bases only, to distinguish from padding
            hgPos->highlight = addHighlight(cartString(cart, "db"),
                                            hgPos->chrom, hgPos->chromStart, hgPos->chromEnd);
            hgPos->chromStart -= padding;
            hgPos->chromEnd   += padding;
            if (hgPos->chromStart < 0)
                hgPos->chromStart = 0;
            }
        // special code here for per hfs bigBed searches
        if (sameString(hfs->searchName, "mane") || sameString(hfs->searchName, "hgnc"))
            {
            char startBuf[256], endBuf[256], *row[bbi->fieldCount];
            bigBedIntervalToRow(interval, chromName, startBuf, endBuf, row, bbi->fieldCount);
            if (sameString(hfs->searchName, "mane"))
                {
                // here the description comes from hgFixed.refLink.product, linked via mane.bb.ncbiId
                if (ncbiIdIx > 0)
                    {
                    struct dyString *query = sqlDyStringCreate("select product from %s where mrnaAcc=substring_index('%s', '.', 1)", refLinkTable, row[ncbiIdIx]);
                    hgPos->description = sqlQuickString(conn, dyStringCannibalize(&query));
                    }
                }
            else
                {
                // the description is the geneName field of the bigBed
                if (geneNameIx > 0)
                    hgPos->description = cloneString(row[geneNameIx]);
                }
            }
        }
    }

if (conn)
    hFreeConn(&conn);

return posList;
}

static struct hgPos *getPosFromBigBed(struct cart *cart, struct trackDb *tdb, struct bbiFile *bbi,
                        char *indexField, char *term, char *description, struct hgFindSpec *hfs, char *db)
/* Given a bigBed file with a search index, check for term. */
{
struct errCatch *errCatch = errCatchNew();
struct hgPos *posList = NULL;
if (errCatchStart(errCatch))
    {
    int fieldIx;
    struct bptFile *bpt = bigBedOpenExtraIndex(bbi, indexField, &fieldIx);
    struct lm *lm = lmInit(0);
    struct bigBedInterval *intervalList;
    intervalList = bigBedNameQuery(bbi, bpt, fieldIx, term, lm);

    posList = bigBedIntervalListToHgPositions(cart, tdb,  bbi, term, intervalList, description, hfs, db);
    bptFileDetach(&bpt);
    }
errCatchEnd(errCatch);
if (errCatch->gotError) 
    {
    // we fail silently if there is a problem e.g. bad index name
    return NULL;
    }

return posList;
}

static struct hgPos *doTrixSearch(struct cart *cart, struct trackDb *tdb, char *trixFile,
                        struct slName *indices, struct bbiFile *bbi, char *term, char *description,
                        struct hgFindSpec *hfs, char *db)
/* search a trix file in the "searchTrix" field of a bigBed trackDb */
{
struct trix *trix = trixOpen(trixFile);
int trixWordCount = 0;
char *tmp = cloneString(term);
char *val = nextWord(&tmp);
char *trixWords[128];

while (val != NULL)
    {
    trixWords[trixWordCount] = strLower(val);
    trixWordCount++;
    if (trixWordCount == sizeof(trixWords)/sizeof(char*))
	errAbort("exhausted space for trixWords");

    val = nextWord(&tmp);        
    }

if (trixWordCount == 0)
    return NULL;


struct trixSearchResult *tsList = trixSearch(trix, trixWordCount, trixWords, tsmExpand);
char *context = NULL;
if (hfs)
    context = hgFindSpecSetting(hfs, "searchTrixContext");
boolean doSnippets = FALSE;
if (context && sameString(context, "on"))
    {
    doSnippets = TRUE;
    initSnippetIndex(trix);
    }
struct hgPos *posList = NULL;
for ( ; tsList != NULL; tsList = tsList->next)
    {
    struct slName *oneIndex = indices;
    if (doSnippets)
        addSnippetForResult(tsList, trix);
    for (; oneIndex; oneIndex = oneIndex->next)
	{
	struct hgPos *posList2 = getPosFromBigBed(cart, tdb, bbi, oneIndex->name,
                                                  tsList->itemId, tsList->snippet, hfs, db);

	posList = slCat(posList, posList2);
	}
    }

return posList;
}

int posListCompare(const void *va, const void *vb)
/* Compare to sort based on name and then position. */
{
const struct hgPos *a = *((struct hgPos **)va);
const struct hgPos *b = *((struct hgPos **)vb);
int diff = strcmp(a->name, b->name);
if (diff == 0)
    {
    diff = strcmp(a->chrom, b->chrom);
    if (diff == 0)
        {
        diff = a->chromStart - b->chromStart;
        if (diff == 0)
            diff = a->chromEnd - b->chromEnd;
        }
    }
return diff;
}



boolean findBigBedPosInTdb(struct cart *cart, char *db, struct trackDb *tdb, char *term, struct hgPositions *hgp, struct hgFindSpec *hfs, boolean measureTiming)
/* Find a position in a single trackDb entry */
{
if (startsWith("bigWig", tdb->type) || !startsWith("big", tdb->type))
    return FALSE;
long startTime = clock1000();
boolean found = FALSE;
char *description = NULL;

if (hfs)
    {
    char buf[2048];
    if (isNotEmpty(hfs->searchDescription))
        truncatef(buf, sizeof(buf), "%s", hfs->searchDescription);
    else
        safef(buf, sizeof(buf), "%s", hfs->searchTable);
    description = cloneString(buf);
    }

// Which field(s) to search?  Look for searchIndex in search spec, then in trackDb for
// backwards compat.
char *indexField = NULL;
if (hfs)
    indexField = hgFindSpecSetting(hfs, "searchIndex");
if (!indexField)
    indexField = trackDbSetting(tdb, "searchIndex");
if (!indexField && !hfs)
    return FALSE;

// If !indexField but we do have a non-NULL hfs, then open file to see if it has a name index.
char *fileName = trackDbSetting(tdb, "bigDataUrl");
if (!fileName && !trackHubDatabase(db))
    {
    struct sqlConnection *conn = hAllocConnTrack(db, tdb);
    fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
    hFreeConn(&conn);
    }
if (!fileName)
    return FALSE;
// we fail silently if bigBed can't be opened.
struct bbiFile *bbi = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    bbi = bigBedFileOpen(fileName);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    return FALSE;

// Now (since hfs is non-NULL) check the file to see if it has a name index if we
// don't already have indexField.
if (!indexField)
    {
    struct slName *indexFields = bigBedListExtraIndexes(bbi);
    if (slNameInList(indexFields, "name"))
        indexField = "name";
    slNameFreeList(&indexFields);
    }
if (!indexField)
    {
    bigBedFileClose(&bbi);
    return FALSE;
    }

struct slName *indexList = slNameListFromString(indexField, ',');
struct hgPos *posList1 = NULL, *posList2 = NULL;
char *trixFile = trackDbSetting(tdb, "searchTrix");
// if there is a trix file, use it to search for the term
if (trixFile != NULL)
    {
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        posList1 = doTrixSearch(cart, tdb, hReplaceGbdb(trixFile), indexList, bbi, term,
                                NULL, hfs, db);
        }
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        warn("trix search failure for %s: %s", tdb->table, dyStringContents(errCatch->message));
    errCatchFree(&errCatch);
    }

// now search for the raw id's
struct slName *oneIndex=indexList;
for (; oneIndex; oneIndex = oneIndex->next)
    {
    posList2 = getPosFromBigBed(cart, tdb, bbi, oneIndex->name, term, NULL, hfs, db);
    posList1 = slCat(posList1, posList2);
    }
// the trix search and the id search may have found the same item so uniqify:
slUniqify(&posList1, posListCompare, hgPosFree);

if (posList1 != NULL)
    {
    struct hgPosTable *table;

    found = TRUE;
    AllocVar(table);
    slAddHead(&hgp->tableList, table);
    table->description = cloneString(description ? description : tdb->longLabel);
    table->name = cloneString(tdb->table);
    table->searchTime = -1;
        if (measureTiming)
            table->searchTime = clock1000() - startTime;

    table->posList = posList1;
    }
bigBedFileClose(&bbi);
freeMem(description);
return found;
}

boolean findBigBedPosInTdbList(struct cart *cart, char *db, struct trackDb *tdbList, char *term, struct hgPositions *hgp, struct hgFindSpec *hfs, boolean measureTiming)
/* Given a list of trackDb entries, check each of them for a searchIndex */
{
struct trackDb *tdb;
boolean found = FALSE;
for (tdb = tdbList; tdb; tdb = tdb->next)
    {
    if (tdb->subtracks)
        {
        found = findBigBedPosInTdbList(cart, db, tdb->subtracks, term, hgp, hfs, measureTiming) || found;
        continue;
        }
    found = findBigBedPosInTdb(cart, db, tdb, term, hgp, hfs, measureTiming) || found;
    }
return found;
}

boolean isTdbSearchable(struct trackDb *tdb)
/* Check if a single tdb is searchable */
{
if (tdb->subtracks)
    {
    boolean searchable = FALSE;
    struct trackDb *sub;
    for (sub = tdb->subtracks; sub != NULL; sub = sub->next)
        searchable |= isTdbSearchable(sub);
    return searchable;
    }
if (startsWith("bigWig", tdb->type) || !startsWith("big", tdb->type))
    return FALSE;

char *indexField = NULL;
indexField = trackDbSetting(tdb, "searchIndex");
if (indexField)
    return TRUE;

// If !indexField but we do have an index on the bigBed use that
char *fileName = trackDbSetting(tdb, "bigDataUrl");
if (!fileName)
    return FALSE;

// we fail silently if bigBed can't be opened.
struct bbiFile *bbi = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    bbi = bigBedFileOpen(fileName);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    return FALSE;

if (!indexField)
    {
    struct slName *indexFields = bigBedListExtraIndexes(bbi);
    if (slNameInList(indexFields, "name"))
        indexField = "name";
    slNameFreeList(&indexFields);
    }
if (!indexField)
    {
    bigBedFileClose(&bbi);
    return FALSE;
    }
return TRUE;
}

struct trackDb *getSearchableBigBeds(struct trackDb *tdbList)
/* Given a list of tracks from a hub, return those that are searchable */
{
struct trackDb *tdb, *next, *ret = NULL;
for (tdb = tdbList ; tdb; tdb = next)
    {
    next = tdb->next;
    if (tdb->subtracks)
        {
        struct trackDb *subtrackList = getSearchableBigBeds(tdb->subtracks);
        if (subtrackList)
            ret = slCat(subtrackList, ret);
        continue;
        }
    if (startsWith("bigWig", tdb->type) || !startsWith("big", tdb->type))
        continue;

    char *indexField = NULL;
    indexField = trackDbSetting(tdb, "searchIndex");
    if (!indexField)
        continue;

    // If !indexField but we do have an index on the bigBed use that
    char *fileName = trackDbSetting(tdb, "bigDataUrl");
    if (!fileName)
        continue;

    // we fail silently if bigBed can't be opened.
    struct bbiFile *bbi = NULL;
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        bbi = bigBedFileOpen(fileName);
        }
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        continue;

    if (!indexField)
        {
        struct slName *indexFields = bigBedListExtraIndexes(bbi);
        if (slNameInList(indexFields, "name"))
            indexField = "name";
        slNameFreeList(&indexFields);
        }
    if (!indexField)
        {
        bigBedFileClose(&bbi);
        continue;
        }

    // finally we have verified the track is searchable, add it to our list we're returning
    // careful not to mess up the old list
    slAddHead(&ret, CloneVar(tdb));
    }
return ret;
}
