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

static struct hgPos *bigBedIntervalListToHgPositions(struct cart *cart, struct trackDb *tdb,
                        struct bbiFile *bbi, char *term, struct bigBedInterval *intervalList,
                        char *description, struct hgFindSpec *hfs)
/* Given an open bigBed file, and an interval list, return a pointer to a list of hgPos structures. */
{
struct hgPos *posList = NULL;
char chromName[bbi->chromBpt->keySize+1];
int lastChromId = -1;
struct bigBedInterval *interval;
struct slInt *labelColumns = NULL;

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
        }
    }

return posList;
}

static struct hgPos *getPosFromBigBed(struct cart *cart, struct trackDb *tdb, struct bbiFile *bbi,
                        char *indexField, char *term, char *description, struct hgFindSpec *hfs)
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

    posList = bigBedIntervalListToHgPositions(cart, tdb,  bbi, term, intervalList, description, hfs);
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
                        struct hgFindSpec *hfs)
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
struct hgPos *posList = NULL;
for ( ; tsList != NULL; tsList = tsList->next)
    {
    struct slName *oneIndex = indices;
    for (; oneIndex; oneIndex = oneIndex->next)
	{
	struct hgPos *posList2 = getPosFromBigBed(cart, tdb, bbi, oneIndex->name,
                                                  tsList->itemId, description, hfs);

	posList = slCat(posList, posList2);
	}
    }

return posList;
}


boolean findBigBedPosInTdbList(struct cart *cart, char *db, struct trackDb *tdbList, char *term, struct hgPositions *hgp, struct hgFindSpec *hfs)
/* Given a list of trackDb entries, check each of them for a searchIndex */
{
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
struct trackDb *tdb;
boolean found = FALSE;
for(tdb=tdbList; tdb; tdb = tdb->next)
    {
    if (tdb->subtracks)
        {
        found = findBigBedPosInTdbList(cart, db, tdb->subtracks, term, hgp, hfs) || found;
        continue;
        }
    if (startsWith("bigWig", tdb->type) || !startsWith("big", tdb->type))
        continue;

    // Which field(s) to search?  Look for searchIndex in search spec, then in trackDb for
    // backwards compat.
    char *indexField = NULL;
    if (hfs)
        indexField = hgFindSpecSetting(hfs, "searchIndex");
    if (!indexField)
        indexField = trackDbSetting(tdb, "searchIndex");
    if (!indexField && !hfs)
        continue;

    // If !indexField but we do have a non-NULL hfs, then open file to see if it has a name index.
    char *fileName = trackDbSetting(tdb, "bigDataUrl");
    if (!fileName && !trackHubDatabase(db))
	{
	struct sqlConnection *conn = hAllocConnTrack(db, tdb);
	fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
	hFreeConn(&conn);
	}
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
        continue;
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
                                    NULL, hfs);
            }
        errCatchEnd(errCatch);
        if (errCatch->gotError)
            warn("trix search failure for %s: %s", tdb->table, dyStringContents(errCatch->message));
        errCatchFree(&errCatch);
	}

    // if no trix file or we didn't find anything from a trix search
    if (!posList1)
        {
        // now search for the raw id's
        struct slName *oneIndex=indexList;
        for (; oneIndex; oneIndex = oneIndex->next)
            {
            posList2 = getPosFromBigBed(cart, tdb, bbi, oneIndex->name, term, NULL, hfs);
            posList1 = slCat(posList1, posList2);
            }
        }

    if (posList1 != NULL)
	{
	struct hgPosTable *table;

	found = TRUE;
	AllocVar(table);
	slAddHead(&hgp->tableList, table);
	table->description = cloneString(description ? description : tdb->longLabel);
	table->name = cloneString(tdb->table);

	table->posList = posList1;
	}
    bigBedFileClose(&bbi);
    }
freeMem(description);
return found;
}
