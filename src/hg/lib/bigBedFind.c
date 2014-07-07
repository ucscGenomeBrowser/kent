#include "common.h"
#include "bPlusTree.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "hgFind.h"
#include "trix.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "hdb.h"

static struct hgPos *bigBedIntervalListToHgPositions(struct bbiFile *bbi, char *term, struct bigBedInterval *intervalList, char *description)
/* Given an open bigBed file, and an interval list, return a pointer to a list of hgPos structures. */
{
struct hgPos *posList = NULL;
char chromName[bbi->chromBpt->keySize+1];
int lastChromId = -1;
struct bigBedInterval *interval;

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
    hgPos->name = cloneString(term);
    hgPos->browserName = cloneString(term);
    hgPos->description = cloneString(description);
    }

return posList;
}

static struct hgPos *getPosFromBigBed(char *bigDataUrl, char *indexField, char *term, char *description)
/* Given a bigBed file with a search index, check for term. */
{
struct bbiFile *bbi = bigBedFileOpen(bigDataUrl);
int fieldIx;
struct bptFile *bpt = bigBedOpenExtraIndex(bbi, indexField, &fieldIx);
struct lm *lm = lmInit(0);
struct bigBedInterval *intervalList;
intervalList = bigBedNameQuery(bbi, bpt, fieldIx, term, lm);

struct hgPos *posList = bigBedIntervalListToHgPositions(bbi, term, 
    intervalList, description);
bbiFileClose(&bbi);
return posList;
}

static struct hgPos *doTrixSearch(char *trixFile, struct slName  *indices, char *bigDataUrl, char *term)
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

struct trixSearchResult *tsList = trixSearch(trix, trixWordCount, trixWords, TRUE);
struct hgPos *posList = NULL;
char *description = NULL;   // we're not filling in this field at the moment
for ( ; tsList != NULL; tsList = tsList->next)
    {
    struct slName *oneIndex = indices;
    for (; oneIndex; oneIndex = oneIndex->next)
	{
	struct hgPos *posList2 = getPosFromBigBed(bigDataUrl, oneIndex->name, tsList->itemId, description);

	posList = slCat(posList, posList2);
	}
    }

return posList;
}


boolean findBigBedPosInTdbList(char *db, struct trackDb *tdbList, char *term, struct hgPositions *hgp)
/* Given a list of trackDb entries, check each of them for a searchIndex */
{
struct trackDb *tdb;
boolean found = FALSE;

for(tdb=tdbList; tdb; tdb = tdb->next)
    {
    char *indexField = trackDbSetting(tdb, "searchIndex");
    char *fileName = NULL;

    if (isHubTrack(tdb->table))
	fileName = trackDbSetting(tdb, "bigDataUrl");
    else
	{
	struct sqlConnection *conn = hAllocConnTrack(db, tdb);
	fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
	hFreeConn(&conn);
	}

    if (!(indexField && fileName))
	continue;

    struct slName *indexList = slNameListFromString(indexField, ',');
    struct hgPos *posList1 = NULL, *posList2 = NULL;
    char *trixFile = trackDbSetting(tdb, "searchTrix");
    // if there is a trix file, use it to search for the term
    if (trixFile != NULL)
	posList1 = doTrixSearch(trixFile, indexList, fileName, term);

    // now search for the raw id's
    struct slName *oneIndex=indexList;
    for (; oneIndex; oneIndex = oneIndex->next)
	{
	posList2 = getPosFromBigBed(fileName, oneIndex->name, term, NULL);
	posList1 = slCat(posList1, posList2);
	}

    if (posList1 != NULL)
	{
	struct hgPosTable *table;

	found = TRUE;
	AllocVar(table);
	slAddHead(&hgp->tableList, table);
	table->description = cloneString(tdb->table);
	table->name = cloneString(tdb->table);

	table->posList = posList1;
	}
    }

return found;
}
