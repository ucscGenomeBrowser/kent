/* bedList - get list of beds in region that pass filtering. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "localmem.h"
#include "dystring.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "trackDb.h"
#include "customTrack.h"
#include "hdb.h"
#include "hui.h"
#include "hCommon.h"
#include "web.h"
#include "featureBits.h"
#include "portable.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: bedList.c,v 1.8 2004/07/23 22:03:55 kent Exp $";


struct bed *getRegionAsBed(
	char *db, char *table, 	/* Database and table. */
	struct region *region,  /* Region to get data for. */
	char *filter, 		/* Filter to add to SQL where clause if any. */
	struct hash *idHash, 	/* Restrict to id's in this hash if non-NULL. */
	struct lm *lm)		/* Where to allocate memory. */
/* Return a bed list of all items in the given range in table.
 * Cleanup result via lmCleanup(&lm) rather than bedFreeList.  */
{
struct dyString *fields = newDyString(0);
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
struct hTableInfo *hti;
struct bed *bedList=NULL, *bed;
char **row;
char rangeStr[32];
char *strand, tStrand, qStrand;
int i, blockCount;
int fieldCount;
boolean isPsl, isGenePred, isBedWithBlocks;
boolean pslKnowIfProtein = FALSE, pslIsProtein = FALSE;

/* Caller can give us either a full table name or root table name. */
hti = hFindTableInfoDb(db, region->chrom, table);
if (hti == NULL)
    errAbort("Could not find table info for table %s", table);
fieldCount = hTableInfoBedFieldCount(hti);
isPsl = sameString("tStarts", hti->startsField);
isGenePred = sameString("exonEnds", hti->endsSizesField);
isBedWithBlocks = (sameString("chromStarts", hti->startsField) 
         && sameString("blockSizes", hti->endsSizesField));

/* All beds have at least chrom,start,end.  We omit the chrom
 * form the query since we already know it. */
dyStringPrintf(fields, "%s,%s", hti->startField, hti->endField);
if (fieldCount >= 4)
    {
    if (hti->nameField[0] != 0)
	dyStringPrintf(fields, ",%s", hti->nameField);
    else /* Put in . as placeholder. */
	dyStringPrintf(fields, ",'.'");  
    }
if (fieldCount >= 5)
    {
    if (hti->scoreField[0] != 0)
	dyStringPrintf(fields, ",%s", hti->scoreField);
    else
	dyStringPrintf(fields, ",0", hti->startField);  
    }
if (fieldCount >= 6)
    {
    if (hti->strandField[0] != 0)
	dyStringPrintf(fields, ",%s", hti->strandField);
    else
	dyStringPrintf(fields, ",'.'");  
    }
if (fieldCount >= 8)
    {
    if (hti->cdsStartField[0] != 0)
	dyStringPrintf(fields, ",%s,%s", hti->cdsStartField, hti->cdsEndField);
    else
	dyStringPrintf(fields, ",%s,%s", hti->startField, hti->endField);  
    }
if (fieldCount >= 12)
    {
    dyStringPrintf(fields, ",%s,%s,%s", hti->countField, 
        hti->endsSizesField, hti->startsField);
    }
if (isPsl)
    {
    /* For psl format we need to know chrom size as well
     * to handle reverse strand case. */
    dyStringPrintf(fields, ",tSize");
    }
sr = regionQuery(conn, table, fields->string, region, TRUE, filter);
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* If have a name field apply hash filter. */
    if (fieldCount >= 4 && idHash != NULL)
        if (!hashLookup(idHash, row[2]))
	    continue;
    lmAllocVar(lm, bed);
    slAddHead(&bedList, bed);
    bed->chrom = region->chrom;
    bed->chromStart = sqlUnsigned(row[0]);
    bed->chromEnd = sqlUnsigned(row[1]);
    if (fieldCount < 4)
        continue;
    bed->name = lmCloneString(lm, row[2]);
    if (fieldCount < 5)
        continue;
    bed->score = sqlSigned(row[3]);
    if (fieldCount < 6)
        continue;
    strand = row[4];
    qStrand = strand[0];
    tStrand = strand[1];
    if (tStrand == 0)
        bed->strand[0] = qStrand;
    else
        {
	/* psl: use XOR of qStrand,tStrand if both are given. */
	if (tStrand == qStrand)
	    bed->strand[0] = '+';
	else
	    bed->strand[0] = '-';
	}
    if (fieldCount < 8)
        continue;
    bed->thickStart = sqlUnsigned(row[5]);
    bed->thickEnd   = sqlUnsigned(row[6]);
    if (fieldCount < 12)
        continue;
    bed->blockCount = blockCount = sqlUnsigned(row[7]);
    lmAllocArray(lm, bed->blockSizes, blockCount);
    sqlUnsignedArray(row[8], bed->blockSizes, blockCount);
    lmAllocArray(lm, bed->chromStarts, blockCount);
    sqlUnsignedArray(row[9], bed->chromStarts, blockCount);
    if (isGenePred)
        {
	/* Translate end coordinates to sizes. */
	for (i=0; i<bed->blockCount; ++i)
	    bed->blockSizes[i] -= bed->chromStarts[i];
	}
    else if (isPsl)
        {
	if (!pslKnowIfProtein)
	    {
	    /* Figure out if is protein using a rather elaborate but
	     * working test I think Angie or Brian must have figured out. */
	    if (tStrand == '-')
	        {
		int tSize = sqlUnsigned(row[10]);
		pslIsProtein = 
		       (bed->chromStart == 
			tSize - (3*bed->blockSizes[bed->blockCount - 1]  + 
			bed->chromStarts[bed->blockCount - 1]));
		}
	    else
	        {
		pslIsProtein = (bed->chromEnd == 
			3*bed->blockSizes[bed->blockCount - 1]  + 
			bed->chromStarts[bed->blockCount - 1]);
		}
	    pslKnowIfProtein = TRUE;
	    }
	if (pslIsProtein)
	    {
	    /* if protein then blockSizes are in protein space */
	    for (i=0; i<blockCount; ++i)
		bed->blockSizes[i] *= 3;
	    }
	if (tStrand == '-')
	    {
	    /* psl: if target strand is '-', flip the coords.
	     * (this is the target part of pslRcBoth from src/lib/psl.c) */
	    int tSize = sqlUnsigned(row[10]);
	    for (i=0; i<blockCount; ++i)
		{
		bed->chromStarts[i] = tSize - 
			(bed->chromStarts[i] + bed->blockSizes[i]);
		}
	    reverseInts(bed->chromStarts, bed->blockCount);
	    reverseInts(bed->blockSizes, bed->blockCount);
	    }
	}
    if (!isBedWithBlocks)
	{
	/* non-bed: translate absolute starts to relative starts */
	for (i=0;  i < bed->blockCount;  i++)
	    bed->chromStarts[i] -= bed->chromStart;
	}
    }
dyStringFree(&fields);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&bedList);
return(bedList);
}

static struct bed *dbGetFilteredBedsOnRegions(struct sqlConnection *conn, 
	struct trackDb *track, struct region *regionList, struct lm *lm)
/* getBed - get list of beds from database in region that pass filtering. */
{
struct region *region;
struct bed *bedList = NULL, *bed;
char *table = track->tableName;
struct hash *idHash = identifierHash();
char *filter = filterClause(database, track->tableName);

for (region = regionList; region != NULL; region = region->next)
    {
    struct bed *nextBed;
    struct bed *bedListRegion = getRegionAsBed(database, table, 
        region, filter, idHash, lm);
    for (bed = bedListRegion; bed != NULL; bed = nextBed)
        {
	nextBed = bed->next;
	slAddHead(&bedList, bed);
	}
    }
slReverse(&bedList);

freez(&filter);
hashFree(&idHash);
return bedList;
}

struct bed *getFilteredBedsOnRegions(struct sqlConnection *conn, 
	struct trackDb *track, struct region *regionList, struct lm *lm)
/* get list of beds in regionList that pass filtering. */
{
if (isCustomTrack(track->tableName))
    return customTrackGetFilteredBeds(track->tableName, regionList, 
    	lm, NULL, NULL);
else
    return dbGetFilteredBedsOnRegions(conn, track, regionList, lm);
}

struct bed *getFilteredBeds(struct sqlConnection *conn,
	struct trackDb *track, struct region *region, struct lm *lm)
/* Get list of beds on single region that pass filtering. */
{
struct region *oldNext = region->next;
struct bed *bedList = NULL;
region->next = NULL;
bedList = getFilteredBedsOnRegions(conn, track, region, lm);
region->next = oldNext;
return bedList;
}

struct bed *getAllFilteredBeds(struct sqlConnection *conn, 
	struct trackDb *track, struct lm *lm)
/* getAllFilteredBeds - get list of beds in selected regions 
 * that pass filtering. */
{
return getFilteredBedsOnRegions(conn, track, getRegions(), lm);
}

/* Droplist menu for custom track visibility: */
char *ctVisMenu[] =
{
    "hide",
    "dense",
    "squish",
    "pack",
    "full",
};
int ctVisMenuSize = 5;

void doBedOrCtOptions(struct trackDb *track, struct sqlConnection *conn, 
	boolean doCt)
/* Put up form to get options on BED or custom track output. */
/* (Taken from hgText.c/doBedCtOptions) */
{
char *table = track->tableName;
char *table2 = NULL;	/* For now... */
struct hTableInfo *hti = getHti(database, table);
char buf[256];
char *setting;
htmlOpen("Output %s as %s", (doCt ? "Custom Track" : "BED"), track->shortLabel);
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=GET>\n");
hPrintf("%s\n", "<A HREF=\"/goldenPath/help/hgTextHelp.html#BEDOptions\">"
     "<B>Help</B></A><P>");
hPrintf("%s\n", "<TABLE><TR><TD>");
if (doCt)
    {
    hPrintf("%s\n", "</TD><TD>"
	 "<A HREF=\"/goldenPath/help/customTrack.html\" TARGET=_blank>"
	 "Custom track</A> header: </B>");
    }
else
    {
    cgiMakeCheckBox(hgtaPrintCustomTrackHeaders,
	    cartCgiUsualBoolean(cart, hgtaPrintCustomTrackHeaders, FALSE));
    hPrintf("%s\n", "</TD><TD> <B> Include "
	 "<A HREF=\"/goldenPath/help/customTrack.html\" TARGET=_blank>"
	 "custom track</A> header: </B>");
    }
hPrintf("%s\n", "</TD></TR><TR><TD></TD><TD>name=");
safef(buf, sizeof(buf), "tb_%s", hti->rootName);
setting = cgiUsualString(hgtaCtName, buf);
cgiMakeTextVar(hgtaCtName, setting, 16);
hPrintf("%s\n", "</TD></TR><TR><TD></TD><TD>description=");
safef(buf, sizeof(buf), "table browser query on %s%s%s",
	 table,
	 (table2 ? ", " : ""),
	 (table2 ? table2 : ""));
setting = cgiUsualString(hgtaCtDesc, buf);
cgiMakeTextVar(hgtaCtDesc, setting, 50);
hPrintf("%s\n", "</TD></TR><TR><TD></TD><TD>visibility=");
setting = cartCgiUsualString(cart, hgtaCtVis, ctVisMenu[3]);
cgiMakeDropList(hgtaCtVis, ctVisMenu, ctVisMenuSize, setting);
hPrintf("%s\n", "</TD></TR><TR><TD></TD><TD>url=");
setting = cartCgiUsualString(cart, hgtaCtUrl, "");
cgiMakeTextVar(hgtaCtUrl, setting, 50);
hPrintf("%s\n", "</TD></TR><TR><TD></TD><TD>");
hPrintf("%s\n", "</TD></TR></TABLE>");
hPrintf("%s\n", "<P> <B> Create one BED record per: </B>");
fbOptionsHtiCart(hti, cart);
if (doCt)
    {
    cgiMakeButton(hgtaDoGetCustomTrack, "Get Custom Track");
    hPrintf(" ");
    cgiMakeButton(hgtaDoGetCustomTrackFile, "Get Custom Track File");
    }
else
    {
    cgiMakeButton(hgtaDoGetBed, "Get BED");
    }
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf("</FORM>\n");
htmlClose();
}

void doOutBed(struct trackDb *track, struct sqlConnection *conn)
/* Put up form to select BED output format. */
{
doBedOrCtOptions(track, conn, FALSE);
}

void doOutCustomTrack(struct trackDb *track, struct sqlConnection *conn)
/* Put up form to select Custom Track output format. */
{
doBedOrCtOptions(track, conn, TRUE);
}

void doGetBedOrCt(struct sqlConnection *conn, boolean doCt, boolean doCtFile)
/* Actually output bed or custom track. */
{
char *table = curTrack->tableName;
struct hTableInfo *hti = getHti(database, table);
struct featureBits *fbList = NULL, *fbPtr;
struct customTrack *ctNew = NULL;
boolean doCtHdr = (cartBoolean(cart, hgtaPrintCustomTrackHeaders) 
	|| doCt || doCtFile);
char *ctName = cgiUsualString(hgtaCtName, table);
char *ctDesc = cgiUsualString(hgtaCtDesc, table);
char *ctVis  = cgiUsualString(hgtaCtVis, "dense");
char *ctUrl  = cgiUsualString(hgtaCtUrl, "");
char *fbQual = fbOptionsToQualifier();
char fbTQ[128];
int fields = hTableInfoBedFieldCount(hti);
boolean gotResults = FALSE;
struct region *region, *regionList = getRegions();

if (!doCt)
    {
    textOpen();
    }


for (region = regionList; region != NULL; region = region->next)
    {
    struct bed *bedList, *bed;
    struct lm *lm = lmInit(64*1024);
    bedList = cookedBedList(conn, curTrack, region, lm);
    if (doCtHdr && (bedList != NULL) && !gotResults)
	{
	int visNum = (int) hTvFromStringNoAbort(ctVis);
	if (visNum < 0)
	    visNum = 0;
	if (doCt)
	    ctNew = newCt(ctName, ctDesc, visNum, ctUrl, fields);
	else
	    hPrintf("track name=\"%s\" description=\"%s\" visibility=%d url=%s \n",
		   ctName, ctDesc, visNum, ctUrl);
	}

    if ((fbQual == NULL) || (fbQual[0] == 0))
	{
	for (bed = bedList;  bed != NULL;  bed = bed->next)
	    {
	    char *ptr = strchr(bed->name, ' ');
	    if (ptr != NULL)
		*ptr = 0;
	    if (!doCt)
		bedTabOutN(bed, fields, stdout);
	    else
		{
		struct bed *dupe = cloneBed(bed); /* Out of local memory. */
	        slAddHead(&ctNew->bedList, dupe);
		}
	    gotResults = TRUE;
	    }
	}
    else
	{
	safef(fbTQ, sizeof(fbTQ), "%s:%s", hti->rootName, fbQual);
	fbList = fbFromBed(fbTQ, hti, bedList, 0, 0, FALSE, FALSE);
	for (fbPtr=fbList;  fbPtr != NULL;  fbPtr=fbPtr->next)
	    {
	    char *ptr = strchr(fbPtr->name, ' ');
	    if (ptr != NULL)
		*ptr = 0;
	    if (! doCt)
		{
		struct bed *fbBed = fbToBedOne(fbPtr);
		slAddHead(&ctNew->bedList, fbBed );
		}
	    else
		{
		hPrintf("%s\t%d\t%d\t%s\t%d\t%c\n",
		       fbPtr->chrom, fbPtr->start, fbPtr->end, fbPtr->name,
		       0, fbPtr->strand);
		}
	    gotResults = TRUE;
	    }
	featureBitsFreeList(&fbList);
	}

    if ((ctNew != NULL) && (ctNew->bedList != NULL))
	{
	/* Load existing custom tracks and add this new one: */
	struct customTrack *ctList = getCustomTracks();
	char *ctFileName = cartOptionalString(cart, "ct");
	struct tempName tn;
	slReverse(&ctNew->bedList);
	slAddHead(&ctList, ctNew);
	/* Save the custom tracks out to file (overwrite the old file): */
	if (ctFileName == NULL)
	    {
	    makeTempName(&tn, "hgtct", ".bed");
	    ctFileName = cloneString(tn.forCgi);
	    }
	customTrackSave(ctList, ctFileName);
	cartSetString(cart, "ct", ctFileName);
	}
    bedList = NULL;
    lmCleanup(&lm);
    }
if (!gotResults)
    {
    if (doCt)
       textOpen();
    hPrintf("\n# No results returned from query.\n\n");
    }
else if (doCt)
    {
    char browserUrl[256];
    char headerText[512];
    char posBuf[256];
    char *position = cartOptionalString(cart, "position");
    int redirDelay = 3;
    if (position == NULL)
	{
	struct bed *bed = ctNew->bedList;
        safef(posBuf, sizeof(posBuf), 
		"%s:%d-%d", bed->chrom, bed->chromStart+1, bed->chromEnd);
	position = posBuf;
	}
    safef(browserUrl, sizeof(browserUrl),
	  "%s?db=%s&position=%s", hgTracksName(), database, position);
    safef(headerText, sizeof(headerText),
	  "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"%d;URL=%s\">",
	  redirDelay, browserUrl);
    webStartHeader(cart, headerText,
		   "Table Browser: %s %s: %s", hOrganism(database), 
		   freezeName, "Get Custom Track");
    hPrintf("You will be automatically redirected to the genome browser in\n"
	   "%d seconds, or you can <BR>\n"
	   "<A HREF=\"%s\">click here to continue</A>.\n",
	   redirDelay, browserUrl);
    }
}

void doGetBed(struct sqlConnection *conn)
/* Get BED output (UI has already told us how). */
{
doGetBedOrCt(conn, FALSE, FALSE);
}

void doGetCustomTrack(struct sqlConnection *conn)
/* Get Custom Track output (UI has already told us how). */
{
doGetBedOrCt(conn, TRUE, FALSE);
}

void doGetCustomTrackFile(struct sqlConnection *conn)
/* Get Custom Track file output (UI has already told us how). */
{
doGetBedOrCt(conn, FALSE, TRUE);
}

