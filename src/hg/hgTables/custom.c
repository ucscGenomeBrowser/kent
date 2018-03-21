/* custom - stuff related to custom tracks. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "hgTables.h"


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

struct customTrack *ctLookupName(char *name)
/* Lookup name in custom track list */
{
return ctFind(getCustomTracks(),name);
}

void flushCustomTracks()
/* Flush custom track list. */
{
theCtList = NULL;
}

struct customTrack *newCt(char *ctName, char *ctDesc, int visNum, char *ctUrl,
			  int fields)
/* Make a new custom track record for the query results. */
{
struct customTrack *ct;
struct trackDb *tdb;
char buf[256];

tdb = customTrackTdbDefault();
tdb->table = customTrackTableFromLabel(ctName);
tdb->track = cloneString(tdb->table);
tdb->shortLabel = ctName;
tdb->longLabel = ctDesc;
safef(buf, sizeof(buf), "bed %d .", fields);
tdb->type = cloneString(buf);
tdb->canPack = 0;
tdb->visibility = visNum;
tdb->url = ctUrl;

AllocVar(ct);
ct->tdb = tdb;
ct->genomeDb = cloneString(database);
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
int fieldCount = 3;

if (ct == NULL)
    return(NULL);

AllocVar(hti);
hti->rootName = cloneString(ct->tdb->table);
hti->isPos = TRUE;
hti->isSplit = FALSE;
hti->hasBin = FALSE;
hti->type = cloneString(ct->tdb->type);
if (sameString("pgSnp", hti->type))
    fieldCount = 4; /* only 4 bed-like */
else if (sameString("barChart", hti->type))
    fieldCount = 6; /* only 6 bed-like */
else if (sameString("interact", hti->type))
    fieldCount = 5; /* only 5 bed-like */
else if (sameString("bedDetail", hti->type))
    fieldCount = ct->fieldCount - 2; /* bed part 4-12 */
else
    fieldCount = ct->fieldCount;
if (fieldCount >= 3)
    {
    strncpy(hti->chromField, "chrom", 32);
    strncpy(hti->startField, "chromStart", 32);
    strncpy(hti->endField, "chromEnd", 32);
    }
if (fieldCount >= 4)
    {
    strncpy(hti->nameField, "name", 32);
    }
if (fieldCount >= 5)
    {
    strncpy(hti->scoreField, "score", 32);
    }
if (fieldCount >= 6)
    {
    strncpy(hti->strandField, "strand", 32);
    }
if (fieldCount >= 8)
    {
    strncpy(hti->cdsStartField, "thickStart", 32);
    strncpy(hti->cdsEndField, "thickEnd", 32);
    hti->hasCDS = TRUE;
    }
if (fieldCount >= 12)
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

#ifdef UNUSED
static void tabBedRow(struct bed *bed, struct slName *fieldList)
/* Print out named fields from bed. */
{
struct slName *field;
boolean needTab = FALSE;
for (field = fieldList; field != NULL; field = field->next)
    {
    char *type = field->name;
    if (needTab)
        hPrintf("\t");
    else
        needTab = TRUE;
    if (sameString(type, "chrom"))
        hPrintf("%s", bed->chrom);
    else if (sameString(type, "chromStart"))
        hPrintf("%u", bed->chromStart);
    else if (sameString(type, "chromEnd"))
        hPrintf("%u", bed->chromEnd);
    else if (sameString(type, "name"))
        hPrintf("%s", bed->name);
    else if (sameString(type, "score"))
        hPrintf("%d", bed->score);
    else if (sameString(type, "strand"))
        hPrintf("%s", bed->strand);
    else if (sameString(type, "thickStart"))
        hPrintf("%u", bed->thickStart);
    else if (sameString(type, "thickEnd"))
        hPrintf("%u", bed->thickEnd);
    else if (sameString(type, "itemRgb"))
	{
	int rgb = bed->itemRgb;
	hPrintf("%d,%d,%d", (rgb & 0xff0000) >> 16,
		(rgb & 0xff00) >> 8, (rgb & 0xff));
	}
    else if (sameString(type, "blockCount"))
        hPrintf("%u", bed->blockCount);
    else if (sameString(type, "blockSizes"))
	{
	unsigned i;
	for (i=0; i<bed->blockCount; ++i)
	    hPrintf("%u,", bed->blockSizes[i]);
	}
    else if (sameString(type, "chromStarts"))
	{
	unsigned i;
	for (i=0; i<bed->blockCount; ++i)
	    hPrintf("%u,", bed->chromStarts[i]);
	}
    else if (sameString(type, "expCount"))
        hPrintf("%u", bed->expCount);
    else if (sameString(type, "expIds"))
	{
	unsigned i;
	for (i=0; i<bed->expCount; ++i)
	    hPrintf("%u,", bed->expIds[i]);
	}
    else if (sameString(type, "expScores"))
	{
	unsigned i;
	for (i=0; i<bed->expCount; ++i)
	    hPrintf("%f,", bed->expScores[i]);
	}
    else
        errAbort("Unrecognized bed field %s", type);
    }
hPrintf("\n");
}
#endif /* UNUSED */

static void tabBedRowFile(struct bed *bed, struct slName *fieldList, FILE *f)
/* Print out to a file named fields from bed. */
{
struct slName *field;
boolean needTab = FALSE;
for (field = fieldList; field != NULL; field = field->next)
    {
    char *type = field->name;
    if (needTab)
        fprintf(f, "\t");
    else
        needTab = TRUE;
    if (sameString(type, "chrom"))
        fprintf(f, "%s", bed->chrom);
    else if (sameString(type, "chromStart"))
        fprintf(f, "%u", bed->chromStart);
    else if (sameString(type, "chromEnd"))
        fprintf(f, "%u", bed->chromEnd);
    else if (sameString(type, "name"))
        fprintf(f, "%s", bed->name);
    else if (sameString(type, "score"))
        fprintf(f, "%d", bed->score);
    else if (sameString(type, "strand"))
        fprintf(f, "%s", bed->strand);
    else if (sameString(type, "thickStart"))
        fprintf(f, "%u", bed->thickStart);
    else if (sameString(type, "thickEnd"))
        fprintf(f, "%u", bed->thickEnd);
    else if (sameString(type, "itemRgb"))
	{
	int rgb = bed->itemRgb;
	fprintf(f, "%d,%d,%d", (rgb & 0xff0000) >> 16,
		(rgb & 0xff00) >> 8, (rgb & 0xff));
	}
    else if (sameString(type, "blockCount"))
        fprintf(f, "%u", bed->blockCount);
    else if (sameString(type, "blockSizes"))
	{
	unsigned i;
	for (i=0; i<bed->blockCount; ++i)
	    fprintf(f, "%u,", bed->blockSizes[i]);
	}
    else if (sameString(type, "chromStarts"))
	{
	unsigned i;
	for (i=0; i<bed->blockCount; ++i)
	    fprintf(f, "%u,", bed->chromStarts[i]);
	}
    else if (sameString(type, "expCount"))
        fprintf(f, "%u", bed->expCount);
    else if (sameString(type, "expIds"))
	{
	unsigned i;
	for (i=0; i<bed->expCount; ++i)
	    fprintf(f, "%u,", bed->expIds[i]);
	}
    else if (sameString(type, "expScores"))
	{
	unsigned i;
	for (i=0; i<bed->expCount; ++i)
	    fprintf(f, "%f,", bed->expScores[i]);
	}
    else
        errAbort("Unrecognized bed field %s", type);
    }
fprintf(f, "\n");
}

struct bedFilter *bedFilterForCustomTrack(char *ctName)
/* If the user specified constraints, then translate them to a bedFilter. */
{
struct hashEl *var, *varList = cartFindPrefix(cart, hgtaFilterVarPrefix);
int prefixSize = strlen(hgtaFilterVarPrefix);
struct bedFilter *bf = NULL;
int *trash;

for (var = varList; var != NULL; var = var->next)
    {
    char *dbTrackFieldType = cloneString(var->name + prefixSize);
    char *parts[4];
    int partCount;
    partCount = chopByChar(dbTrackFieldType, '.', parts, ArraySize(parts));
    if (partCount == 4 && sameString(parts[1], ctName))
	{
	char *db = parts[0];
	char *table = parts[1];
	char *field = parts[2];
	char *type = parts[3];
	if (sameString(type, filterPatternVar))
	    {
	    char *pat = cloneString(var->val);
	    char *varName, *dd, *cmp;
	    if (bf == NULL)
	        AllocVar(bf);
	    varName = filterFieldVarName(db, table, field, filterDdVar);
	    dd = cartOptionalString(cart, varName);
	    varName = filterFieldVarName(db, table, field, filterCmpVar);
	    cmp = cartOptionalString(cart, varName);
	    if (sameString("chrom", field))
		cgiToStringFilter(dd, pat, &(bf->chromFilter), &(bf->chromVals),
				  &(bf->chromInvert));
	    else if (sameString("chromStart", field))
		cgiToIntFilter(cmp, pat,
			       &(bf->chromStartFilter), &(bf->chromStartVals));
	    else if (sameString("chromEnd", field))
		cgiToIntFilter(cmp, pat,
			       &(bf->chromEndFilter), &(bf->chromEndVals));
	    else if (sameString("name", field))
		cgiToStringFilter(dd, pat, &(bf->nameFilter), &(bf->nameVals),
				  &(bf->nameInvert));
	    else if (sameString("score", field))
		cgiToIntFilter(cmp, pat,
			       &(bf->scoreFilter), &(bf->scoreVals));
	    else if (sameString("strand", field))
		cgiToCharFilter(dd, pat, &(bf->strandFilter), &(bf->strandVals),
				&(bf->strandInvert));
	    else if (sameString("thickStart", field))
		cgiToIntFilter(cmp, pat,
			       &(bf->thickStartFilter), &(bf->thickStartVals));
	    else if (sameString("thickEnd", field))
		cgiToIntFilter(cmp, pat,
			       &(bf->thickEndFilter), &(bf->thickEndVals));
	    else if (sameString("blockCount", field))
		cgiToIntFilter(cmp, pat,
			       &(bf->blockCountFilter), &(bf->blockCountVals));
	    else if (sameString("chromLength", field))
		cgiToIntFilter(cmp, pat,
			       &(bf->chromLengthFilter), &(bf->chromLengthVals));
	    else if (sameString("thickLength", field))
		cgiToIntFilter(cmp, pat,
			       &(bf->thickLengthFilter), &(bf->thickLengthVals));
	    else if (sameString("compareStarts", field))
		cgiToIntFilter(cmp, pat,
			       &(bf->compareStartsFilter), &trash);
	    else if (sameString("compareEnds", field))
		cgiToIntFilter(cmp, pat,
			       &(bf->compareEndsFilter), &trash);
	    freez(&pat);
	    }
	}
    }

hashElFreeList(&varList);
return bf;
}

static void customTrackFilteredBedOnRegion(
	struct region *region,	/* Region to get data from. */
	struct customTrack *ct, /* Custom track. */
	struct hash *idHash,	/* Hash of identifiers or NULL */
	struct bedFilter *bf,	/* Filter or NULL */
	struct lm *lm,		/* Local memory pool. */
	struct bed **pBedList  /* Output get's appended to this list */
	)
/* Get the custom tracks passing filter on a single region. */
{
struct bed *bed;

if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    char query[512];
    int rowOffset;
    char **row;
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    struct sqlResult *sr = NULL;

    sqlSafef(query, sizeof(query), "select * from %s", ct->dbTableName);
    sr = hRangeQuery(conn, ct->dbTableName, region->chrom,
	region->start, region->end, NULL, &rowOffset);

    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, fieldCount);
	if ((idHash == NULL || hashLookup(idHash, bed->name)) &&
	    (bf == NULL || bedFilterOne(bf, bed)))
	    {
	    struct bed *copy = lmCloneBed(bed, lm);
	    slAddHead(pBedList, copy);
	    }
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
else
    {
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
	{
	if (idHash == NULL || hashLookup(idHash, bed->name))
	    {
	    if (sameString(bed->chrom, region->chrom))
		{
		if (bed->chromStart < region->end && bed->chromEnd > region->start)
		    {
		    if (bf == NULL || bedFilterOne(bf, bed))
			{
			struct bed *copy = lmCloneBed(bed, lm);
			slAddHead(pBedList, copy);
			}
		    }
		}
	    }
	}
    }
}

struct bed *customTrackGetFilteredBeds(char *db, char *name, struct region *regionList,
	struct lm *lm, int *retFieldCount)
/* Get list of beds from custom track of given name that are
 * in current regions and that pass filters.  You can bedFree
 * this when done. */
{
struct customTrack *ct = ctLookupName(name);
struct bedFilter *bf = NULL;
struct bed *bedList = NULL;
struct hash *idHash = NULL;
struct region *region;
int fieldCount;

if (ct == NULL)
    errAbort("Can't find custom track %s", name);
char *type = ct->dbTrackType;

if (type != NULL && (startsWithWord("makeItems", type) || 
        sameWord("bedDetail", type) || 
        sameWord("barChart", type) || 
        sameWord("interact", type) || 
        sameWord("pgSnp", type)))
    {
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    bedList = dbGetFilteredBedsOnRegions(conn, CUSTOM_TRASH, db, ct->dbTableName, name,
    	regionList, lm, retFieldCount);
    hFreeConn(&conn);
    fieldCount = 9;
    if (sameWord("bedDetail", type))
        fieldCount = *retFieldCount;
    else if (sameWord("pgSnp", type))
        fieldCount = 4;
    else if (sameWord("barChart", type))
        fieldCount = 6;
    else if (sameWord("interact", type))
        fieldCount = 5;
    }
else if (ct->wiggle)
    {
    struct bed *wigBedList = NULL, *bed;

    /* Grab filtered beds for each region. */
    for (region = regionList; region != NULL; region = region->next)
	{
	wigBedList = getWiggleAsBed(NULL, name, region, NULL, NULL, lm, NULL);
	for (bed = wigBedList; bed != NULL; bed = bed->next)
	    {
	    struct bed *copy = lmCloneBed(bed, lm);
	    slAddHead(&bedList, copy);
	    }
	/*bedFree(&wigBedList); do not free local memory*/
	}
    fieldCount = 4;
    }
else
    {
    /* Figure out how to filter things. */
    fieldCount = ct->fieldCount;
    bf = bedFilterForCustomTrack(name);
    if (ct->fieldCount > 3)
	idHash = identifierHash(db, name);

    /* Grab filtered beds for each region. */
    for (region = regionList; region != NULL; region = region->next)
	customTrackFilteredBedOnRegion(region, ct, idHash, bf, lm, &bedList);

    /* clean up. */
    hashFree(&idHash);
    slReverse(&bedList);
    }
if (retFieldCount != NULL)
    *retFieldCount = fieldCount;
return bedList;
}

static void doTabOutBedLike(struct customTrack *ct, char *table,
	struct sqlConnection *conn, char *fields, FILE *f)
/* Print out selected fields from a bed-like custom track.  If fields
 * is NULL, then print out all fields. */
{
struct region *regionList = getRegions(), *region;
struct slName *chosenFields, *field;
int count = 0;
if (fields == NULL)
    chosenFields = getBedFields(ct->fieldCount);
else
    chosenFields = commaSepToSlNames(fields);

if (f == NULL)
    f = stdout;
fprintf(f, "#");
for (field = chosenFields; field != NULL; field = field->next)
    {
    if (field != chosenFields)
	fprintf(f, "\t");
    fprintf(f, "%s", field->name);
    }
fprintf(f, "\n");

for (region = regionList; region != NULL; region = region->next)
    {
    struct lm *lm = lmInit(64*1024);
    struct bed *bed, *bedList = cookedBedList(conn, table,
	region, lm, NULL);
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	tabBedRowFile(bed, chosenFields, f);
	++count;
	}
    lmCleanup(&lm);
    }
if (count == 0)
    explainWhyNoResults(f);
}

void doTabOutCustomTracks(char *db, char *table, struct sqlConnection *conn,
	char *fields, FILE *f)
/* Print out selected fields from custom track.  If fields
 * is NULL, then print out all fields. */
{
struct customTrack *ct = ctLookupName(table);
char *type = ct->tdb->type;
if (startsWithWord("makeItems", type) || 
        sameWord("bedDetail", type) || 
        sameWord("barChart", type) ||
        sameWord("interact", type) ||
        sameWord("pgSnp", type))
    {
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    doTabOutDb(CUSTOM_TRASH, db, ct->dbTableName, table, f, conn, fields);
    hFreeConn(&conn);
    }
else
    doTabOutBedLike(ct, table, conn, fields, f);
}


void removeNamedCustom(struct customTrack **pList, char *name)
/* Remove named custom track from list if it's on there. */
{
struct customTrack *newList = NULL, *ct, *next;
for (ct = *pList; ct != NULL; ct = next)
    {
    next = ct->next;
    if (!sameString(ct->tdb->table, name))
        {
	slAddHead(&newList, ct);
	}
    }
slReverse(&newList);
*pList = newList;
}


void doRemoveCustomTrack(struct sqlConnection *conn)
/* Remove custom track file. */
{
getCustomTracks();
if (theCtList)
    removeNamedCustom(&theCtList, curTable);
customTracksSaveCart(database, cart, theCtList);
initGroupsTracksTables();
doMainPage(conn);
}

