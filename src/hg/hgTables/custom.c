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
#include "hgTables.h"

static char const rcsid[] = "$Id: custom.c,v 1.36 2008/05/27 23:48:28 hiram Exp $";

struct customTrack *theCtList = NULL;	/* List of custom tracks. */
struct slName *browserLines = NULL;	/* Browser lines in custom tracks. */

struct customTrack *getCustomTracks()
/* Get custom track list. */
{
//fprintf(stdout,"database %s in cart %s", database, cartString(cart, "db"));
cartSetString(cart, "db", database);
if (theCtList == NULL)
    theCtList = customTracksParseCart(cart, &browserLines, NULL);
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

tdb = customTrackTdbDefault();
tdb->tableName = customTrackTableFromLabel(ctName);
tdb->shortLabel = ctName;
tdb->longLabel = ctDesc;
safef(buf, sizeof(buf), "bed %d .", fields);
tdb->type = cloneString(buf);
tdb->canPack = 0;
tdb->visibility = visNum;
tdb->url = ctUrl;

AllocVar(ct);
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

static void cgiToCharFilter(char *dd, char *pat, enum charFilterType *retCft,
		     char **retVals, boolean *retInv)
/* Given a "does/doesn't" and a (list of) literal chars from CGI, fill in 
 * retCft, retVals and retInv to make a filter. */
{
char *vals, *ptrs[32];
int numWords;
int i;

assert(retCft != NULL);
assert(retVals != NULL);
assert(retInv != NULL);
assert(sameString(dd, "does") || sameString(dd, "doesn't"));

/* catch null-constraint cases.  ? will be treated as a literal match, 
 * which would make sense for bed strand and maybe other single-char things: */
if (pat == NULL)
    pat = "";
pat = trimSpaces(pat);
if ((pat[0] == 0) || sameString(pat, "*"))
    {
    *retCft = cftIgnore;
    return;
    }

*retCft = cftMultiLiteral;
numWords = chopByWhite(pat, ptrs, ArraySize(ptrs));
vals = needMem((numWords+1) * sizeof(char));
for (i=0;  i < numWords;  i++)
    vals[i] = ptrs[i][0];
vals[i] = 0;
*retVals = vals;
*retInv = sameString("doesn't", dd);
}

static void cgiToStringFilter(char *dd, char *pat, enum stringFilterType *retSft,
		       char ***retVals, boolean *retInv)
/* Given a "does/doesn't" and a (list of) regexps from CGI, fill in 
 * retCft, retVals and retInv to make a filter. */
{
char **vals, *ptrs[32];
int numWords;
int i;

assert(retSft != NULL);
assert(retVals != NULL);
assert(retInv != NULL);
assert(sameString(dd, "does") || sameString(dd, "doesn't"));

/* catch null-constraint cases: */
if (pat == NULL)
    pat = "";
pat = trimSpaces(pat);
if ((pat[0] == 0) || sameString(pat, "*"))
    {
    *retSft = sftIgnore;
    return;
    }

*retSft = sftMultiRegexp;
numWords = chopByWhite(pat, ptrs, ArraySize(ptrs));
vals = needMem((numWords+1) * sizeof(char *));
for (i=0;  i < numWords;  i++)
    vals[i] = cloneString(ptrs[i]);
vals[i] = NULL;
*retVals = vals;
*retInv = sameString("doesn't", dd);
}

static void cgiToIntFilter(char *cmp, char *pat, enum numericFilterType *retNft,
		    int **retVals)
/* Given a comparison operator and a (pair of) integers from CGI, fill in 
 * retNft and retVals to make a filter. */
{
char *ptrs[3];
int *vals;
int numWords;

assert(retNft != NULL);
assert(retVals != NULL);

/* catch null-constraint cases: */
if (pat == NULL)
    pat = "";
pat = trimSpaces(pat);
if ((pat[0] == 0) || sameString(pat, "*") || sameString(cmp, "ignored"))
    {
    *retNft = nftIgnore;
    return;
    }
else if (sameString(cmp, "in range"))
    {
    *retNft = nftInRange;
    numWords = chopString(pat, " \t,", ptrs, ArraySize(ptrs));
    if (numWords != 2)
	errAbort("For \"in range\" constraint, you must give two numbers separated by whitespace or comma.");
    vals = needMem(2 * sizeof(int)); 
    vals[0] = atoi(ptrs[0]);
    vals[1] = atoi(ptrs[1]);
    if (vals[0] > vals[1])
	{
	int tmp = vals[0];
	vals[0] = vals[1];
	vals[1] = tmp;
	}
    *retVals = vals;
   }
else
    {
    if (sameString(cmp, "<"))
	*retNft = nftLessThan;
    else if (sameString(cmp, "<="))
	*retNft = nftLTE;
    else if (sameString(cmp, "="))
	*retNft = nftEqual;
    else if (sameString(cmp, "!="))
	*retNft = nftNotEqual;
    else if (sameString(cmp, ">="))
	*retNft = nftGTE;
    else if (sameString(cmp, ">"))
	*retNft = nftGreaterThan;
    else
	errAbort("Unrecognized comparison operator %s", cmp);
    vals = needMem(sizeof(int));
    vals[0] = atoi(pat);
    *retVals = vals;
    }
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
    struct sqlConnection *conn = sqlCtConn(TRUE);
    struct sqlResult *sr = NULL;

    safef(query, sizeof(query), "select * from %s", ct->dbTableName);
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
    sqlDisconnect(&conn);
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

struct bed *customTrackGetFilteredBeds(char *name, struct region *regionList,
	struct lm *lm, int *retFieldCount)
/* Get list of beds from custom track of given name that are
 * in current regions and that pass filters.  You can bedFree
 * this when done. */
{
struct customTrack *ct = lookupCt(name);
struct bedFilter *bf = NULL;
struct bed *bedList = NULL;
struct hash *idHash = NULL;
struct region *region;
int fieldCount;

if (ct == NULL)
    errAbort("Can't find custom track %s", name);

if (ct->wiggle)
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
	idHash = identifierHash(database, name);

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

void doTabOutCustomTracks(struct trackDb *track, struct sqlConnection *conn,
	char *fields, FILE *f)
/* Print out selected fields from custom track.  If fields
 * is NULL, then print out all fields. */
{
struct region *regionList = getRegions(), *region;
struct slName *chosenFields, *field;
int count = 0;
if (fields == NULL)
    {
    struct customTrack *ct = lookupCt(track->tableName);
    chosenFields = getBedFields(ct->fieldCount);
    }
else
    chosenFields = commaSepToSlNames(fields);

if (f == NULL)
    {
    hPrintf("#");
    for (field = chosenFields; field != NULL; field = field->next)
        {
        if (field != chosenFields)
            hPrintf("\t");
        hPrintf("%s", field->name);
        }
    hPrintf("\n");
    }
else
    {
    fprintf(f, "#");
    for (field = chosenFields; field != NULL; field = field->next)
        {
        if (field != chosenFields)
            fprintf(f, "\t");
        fprintf(f, "%s", field->name);
        }
    fprintf(f, "\n");
    }

for (region = regionList; region != NULL; region = region->next)
    {
    struct lm *lm = lmInit(64*1024);
    struct bed *bed, *bedList = cookedBedList(conn, track->tableName, 
    	region, lm, NULL);
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
        if (f == NULL)
	    tabBedRow(bed, chosenFields);
        else
            tabBedRowFile(bed, chosenFields, f);
	++count;
	}
    lmCleanup(&lm);
    }
if (count == 0)
    explainWhyNoResults(f);
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


void doRemoveCustomTrack(struct sqlConnection *conn)
/* Remove custom track file. */
{
getCustomTracks();
if (theCtList)
    removeNamedCustom(&theCtList, curTable);
customTracksSaveCart(cart, theCtList);
initGroupsTracksTables(conn);
doMainPage(conn);
}

