/* custom - stuff related to custom tracks. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "trackDb.h"
#include "grp.h"
#include "customTrack.h"

#include "hgGenome.h"

static char const rcsid[] = "$Id: custom.c,v 1.3 2008/09/03 19:18:54 markd Exp $";

struct customTrack *theCtList = NULL;	/* List of custom tracks. */
struct slName *browserLines = NULL;	/* Browser lines in custom tracks. */

struct customTrack *getCustomTracks()
/* Get custom track list. */
{
//fprintf(stdout,"database %s in cart %s", database, cartString(cart, "db"));
cartSetString(cart, "db", database);
if (theCtList == NULL)
    theCtList = customTracksParseCart(database, cart, &browserLines, NULL);
return(theCtList);
}

void flushCustomTracks()
/* Flush custom track list. */
{
theCtList = NULL;
}


struct customTrack *lookupCt(char *name)
/* Find named custom track. */
{
struct customTrack *ctList = getCustomTracks();
struct customTrack *ct;
for (ct=ctList;  ct != NULL;  ct=ct->next)
    {
    if (sameString(ct->tdb->tableName, name))
	return ct;
    }
return NULL;
}

struct customTrack *newCt(char *ctName, char *ctDesc, int visNum, char *ctUrl,
			  int fields)
/* Make a new custom track record for the query results. */
{
struct customTrack *ct;
struct trackDb *tdb;
char buf[256];

AllocVar(ct);
AllocVar(tdb);
tdb->tableName = customTrackTableFromLabel(ctName);
tdb->shortLabel = ctName;
tdb->longLabel = ctDesc;
safef(buf, sizeof(buf), "bed %d .", fields);
tdb->type = cloneString(buf);
tdb->visibility = visNum;
tdb->url = ctUrl;
ct->tdb = tdb;
ct->fieldCount = fields;
ct->needsLift = FALSE;
ct->fromPsl = FALSE;
ct->wiggle = FALSE;
ct->wigAscii = (char *)NULL;
ct->wigFile = (char *)NULL;
ct->wibFile = (char *)NULL;
ctAddToSettings(ct, CT_UNPARSED, "true");
return ct;
}


struct hTableInfo *ctToHti(struct customTrack *ct)
/* Create an hTableInfo from a customTrack. */
{
struct hTableInfo *hti;

if (ct == NULL)
    return(NULL);

AllocVar(hti);
hti->rootName = cloneString(ct->tdb->tableName);
hti->isPos = TRUE;
hti->isSplit = FALSE;
hti->hasBin = FALSE;
hti->type = cloneString(ct->tdb->type);
if (ct->fieldCount >= 3)
    {
    strncpy(hti->chromField, "chrom", 32);
    strncpy(hti->startField, "chromStart", 32);
    strncpy(hti->endField, "chromEnd", 32);
    }
if (ct->fieldCount >= 4)
    {
    strncpy(hti->nameField, "name", 32);
    }
if (ct->fieldCount >= 5)
    {
    strncpy(hti->scoreField, "score", 32);
    }
if (ct->fieldCount >= 6)
    {
    strncpy(hti->strandField, "strand", 32);
    }
if (ct->fieldCount >= 8)
    {
    strncpy(hti->cdsStartField, "thickStart", 32);
    strncpy(hti->cdsEndField, "thickEnd", 32);
    hti->hasCDS = TRUE;
    }
if (ct->fieldCount >= 12)
    {
    strncpy(hti->countField, "blockCount", 32);
    strncpy(hti->startsField, "chromStarts", 32);
    strncpy(hti->endsSizesField, "blockSizes", 32);
    hti->hasBlocks = TRUE;
    }

return(hti);
}

struct slName *getBedFields(int fieldCount)
/* Get list of fields for bed of given size. */
{
struct slName *fieldList = NULL, *field;
if (fieldCount >= 3)
    {
    field = newSlName("chrom");
    slAddHead(&fieldList, field);
    field = newSlName("chromStart");
    slAddHead(&fieldList, field);
    field = newSlName("chromEnd");
    slAddHead(&fieldList, field);
    }
if (fieldCount >= 4)
    {
    field = newSlName("name");
    slAddHead(&fieldList, field);
    }
if (fieldCount >= 5)
    {
    field = newSlName("score");
    slAddHead(&fieldList, field);
    }
if (fieldCount >= 6)
    {
    field = newSlName("strand");
    slAddHead(&fieldList, field);
    }
if (fieldCount >= 8)
    {
    field = newSlName("thickStart");
    slAddHead(&fieldList, field);
    field = newSlName("thickEnd");
    slAddHead(&fieldList, field);
    }
if (fieldCount >= 9)
    {
    field = newSlName("itemRgb");
    slAddHead(&fieldList, field);
    }
if (fieldCount >= 12)
    {
    field = newSlName("blockCount");
    slAddHead(&fieldList, field);
    field = newSlName("blockSizes");
    slAddHead(&fieldList, field);
    field = newSlName("chromStarts");
    slAddHead(&fieldList, field);
    }
if (fieldCount >= 15)
    {
    field = newSlName("expCount");
    slAddHead(&fieldList, field);
    field = newSlName("expIds");
    slAddHead(&fieldList, field);
    field = newSlName("expScores");
    slAddHead(&fieldList, field);    
    }
slReverse(&fieldList);
return fieldList;
}



void removeNamedCustom(struct customTrack **pList, char *name)
/* Remove named custom track from list if it's on there. */
{
struct customTrack *newList = NULL, *ct, *next;
for (ct = *pList; ct != NULL; ct = next)
    {
    next = ct->next;
    if (!sameString(ct->tdb->tableName, name))
        {
	slAddHead(&newList, ct);
	}
    }
slReverse(&newList);
*pList = newList;
}

static void customTrackBedOnChrom(
	char *chrom,	        /* chrom to get data from. */
	struct customTrack *ct, /* Custom track. */
	struct lm *lm,		/* Local memory pool. */
	struct bed **pBedList   /* Output get's appended to this list */
	)
/* Get the custom tracks passing filter on a single chrom. */
{
struct bed *bed;

if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    char query[512];
    int rowOffset = 0;
    char **row;
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    struct sqlResult *sr = NULL;

    safef(query, sizeof(query), "select * from %s where chrom='%s'", ct->dbTableName, chrom);
    sr = sqlGetResult(conn, query);
    if (sameString("bin",sqlFieldName(sr)))
	++rowOffset;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, fieldCount);
	struct bed *copy = lmCloneBed(bed, lm);
	slAddHead(pBedList, copy);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
else
    {
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
	{
	if (sameString(bed->chrom, chrom))
	    {
	    struct bed *copy = lmCloneBed(bed, lm);
	    slAddHead(pBedList, copy);
	    }
	}
    }
}



struct bed *customTrackGetBedsForChrom(char *name, char *chrom,
	struct lm *lm,	int *retFieldCount)
/* Get list of beds from custom track of given name that are
 * in given chrom. You can bedFree this when done. */ 
{
struct customTrack *ct = lookupCt(name);
struct bed *bedList = NULL;
int fieldCount;

if (ct == NULL)
    errAbort("Can't find custom track %s", name);

/* Figure out how to filter things. */
fieldCount = ct->fieldCount;

/* Grab beds for a chrom. */
customTrackBedOnChrom(chrom, ct, lm, &bedList);

/* Set return variables and clean up. */
slReverse(&bedList);

if (retFieldCount != NULL)
    *retFieldCount = fieldCount;
return bedList;
}

