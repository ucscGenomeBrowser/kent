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
#include "wiggle.h"

static char const rcsid[] = "$Id: bedList.c,v 1.29 2004/12/03 20:36:07 kent Exp $";

boolean htiIsPsl(struct hTableInfo *hti)
/* Return TRUE if table looks to be in psl format. */
{
return sameString("tStarts", hti->startsField);
}

void bedSqlFieldsExceptForChrom(struct hTableInfo *hti,
	int *retFieldCount, char **retFields)
/* Given tableInfo figure out what fields are needed to
 * add to a database query to have information to create
 * a bed. The chromosome is not one of these fields - we
 * assume that is already known since we're processing one
 * chromosome at a time.   Return a comma separated (no last
 * comma) list of fields that can be freeMem'd, and the count
 * of fields (this *including* the chromosome). */
{
struct dyString *fields = dyStringNew(128);
int fieldCount = fieldCount = hTableInfoBedFieldCount(hti);

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
if (htiIsPsl(hti))
    {
    /* For psl format we need to know chrom size as well
     * to handle reverse strand case. */
    dyStringPrintf(fields, ",tSize");
    }
*retFieldCount = fieldCount;
*retFields = dyStringCannibalize(&fields);
}

struct bed *bedFromRow(
	char *chrom, 		  /* Chromosome bed is on. */
	char **row,  		  /* Row with other data for bed. */
	int fieldCount,		  /* Number of fields in final bed. */
	boolean isPsl, 		  /* True if in PSL format. */
	boolean isGenePred,	  /* True if in GenePred format. */
	boolean isBedWithBlocks,  /* True if BED with block list. */
	boolean *pslKnowIfProtein,/* Have we figured out if psl is protein? */
	boolean *pslIsProtein,    /* True if we know psl is protien. */
	struct lm *lm)		  /* Local memory pool */
/* Create bed from a database row when we already understand
 * the format pretty well.  The bed is allocated inside of
 * the local memory pool lm.  Generally use this in conjunction
 * with the results of a SQL query constructed with the aid
 * of the bedSqlFieldsExceptForChrom function. */
{
char *strand, tStrand, qStrand;
struct bed *bed;
int i, blockCount;

lmAllocVar(lm, bed);
bed->chrom = chrom;
bed->chromStart = sqlUnsigned(row[0]);
bed->chromEnd = sqlUnsigned(row[1]);

if (fieldCount < 4)
    return bed;
bed->name = lmCloneString(lm, row[2]);
if (fieldCount < 5)
    return bed;
bed->score = atoi(row[3]);
if (fieldCount < 6)
    return bed;
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
    return bed;
bed->thickStart = sqlUnsigned(row[5]);
bed->thickEnd   = sqlUnsigned(row[6]);
if (fieldCount < 12)
    return bed;
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
    if (!*pslKnowIfProtein)
	{
	/* Figure out if is protein using a rather elaborate but
	 * working test I think Angie or Brian must have figured out. */
	if (tStrand == '-')
	    {
	    int tSize = sqlUnsigned(row[10]);
	    *pslIsProtein = 
		   (bed->chromStart == 
		    tSize - (3*bed->blockSizes[bed->blockCount - 1]  + 
		    bed->chromStarts[bed->blockCount - 1]));
	    }
	else
	    {
	    *pslIsProtein = (bed->chromEnd == 
		    3*bed->blockSizes[bed->blockCount - 1]  + 
		    bed->chromStarts[bed->blockCount - 1]);
	    }
	*pslKnowIfProtein = TRUE;
	}
    if (*pslIsProtein)
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
return bed;
}

static struct bed *getRegionAsBed(
	char *db, char *table, 	/* Database and table. */
	struct region *region,  /* Region to get data for. */
	char *filter, 		/* Filter to add to SQL where clause if any. */
	struct hash *idHash, 	/* Restrict to id's in this hash if non-NULL. */
	struct lm *lm,		/* Where to allocate memory. */
	int *retFieldCount)	/* Number of fields. */
/* Return a bed list of all items in the given range in table.
 * Cleanup result via lmCleanup(&lm) rather than bedFreeList.  */
{
char *fields = NULL;
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
struct hTableInfo *hti;
struct bed *bedList=NULL, *bed;
char **row;
int fieldCount;
boolean isPsl, isGenePred, isBedWithBlocks;
boolean pslKnowIfProtein = FALSE, pslIsProtein = FALSE;

hti = hFindTableInfoDb(db, region->chrom, table);
if (hti == NULL)
    errAbort("Could not find table info for table %s", table);

if (isWiggle(db, table))
    {
    bedList = getWiggleAsBed(db, table, region, filter, idHash, lm, conn);
    fieldCount = 4;
    }
else
    {
    bedSqlFieldsExceptForChrom(hti, &fieldCount, &fields);
    isPsl = htiIsPsl(hti);
    isGenePred = sameString("exonEnds", hti->endsSizesField);
    isBedWithBlocks = (
    	(sameString("chromStarts", hti->startsField) ||
	 sameString("blockStarts", hti->startsField))
	     && sameString("blockSizes", hti->endsSizesField));

    /* All beds have at least chrom,start,end.  We omit the chrom
     * from the query since we already know it. */
    sr = regionQuery(conn, table, fields, region, TRUE, filter);
    while (sr != NULL && (row = sqlNextRow(sr)) != NULL)
	{
	/* If have a name field apply hash filter. */
	if (fieldCount >= 4 && idHash != NULL)
	    if (!hashLookup(idHash, row[2]))
		continue;
	bed = bedFromRow(region->chrom, row, fieldCount, isPsl, isGenePred,
			 isBedWithBlocks, &pslKnowIfProtein, &pslIsProtein, lm);
	slAddHead(&bedList, bed);
	}
    freez(&fields);
    sqlFreeResult(&sr);
    sqlDisconnect(&conn);
    slReverse(&bedList);
    }
if (retFieldCount)
    *retFieldCount = fieldCount;
return(bedList);
}

static struct bed *dbGetFilteredBedsOnRegions(struct sqlConnection *conn, 
	char *table, struct region *regionList, struct lm *lm, 
	int *retFieldCount)
/* getBed - get list of beds from database in region that pass filtering. */
{
struct region *region;
struct bed *bedList = NULL, *bed;
struct hash *idHash = identifierHash();

for (region = regionList; region != NULL; region = region->next)
    {
    struct bed *nextBed;
    char *filter = filterClause(database, table, region->chrom);
    struct bed *bedListRegion = getRegionAsBed(database, table, 
        region, filter, idHash, lm, retFieldCount);
    for (bed = bedListRegion; bed != NULL; bed = nextBed)
        {
	nextBed = bed->next;
	slAddHead(&bedList, bed);
	}
    freez(&filter);
    }
slReverse(&bedList);

hashFree(&idHash);
return bedList;
}

struct bed *getFilteredBedsOnRegions(struct sqlConnection *conn, 
	char *table, struct region *regionList, struct lm *lm, 
	int *retFieldCount)
/* get list of beds in regionList that pass filtering. */
{
if (isCustomTrack(table))
    return customTrackGetFilteredBeds(table, regionList, 
    	lm, NULL, NULL, retFieldCount);
else
    return dbGetFilteredBedsOnRegions(conn, table, regionList, lm, retFieldCount);
}

struct bed *getFilteredBeds(struct sqlConnection *conn,
	char *table, struct region *region, struct lm *lm, int *retFieldCount)
/* Get list of beds on single region that pass filtering. */
{
struct region *oldNext = region->next;
struct bed *bedList = NULL;
region->next = NULL;

bedList = getFilteredBedsOnRegions(conn, table, region, lm, retFieldCount);
region->next = oldNext;
return bedList;
}

struct bed *getAllFilteredBeds(struct sqlConnection *conn, 
	char *table, struct lm *lm, int *retFieldCount)
/* getAllFilteredBeds - get list of beds in selected regions 
 * that pass filtering. */
{
return getFilteredBedsOnRegions(conn, table, getRegions(), lm, retFieldCount);
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

void doBedOrCtOptions(char *table, struct sqlConnection *conn, 
	boolean doCt)
/* Put up form to get options on BED or custom track output. */
/* (Taken from hgText.c/doBedCtOptions) */
{
char *table2 = NULL;	/* For now... */
struct hTableInfo *hti = getHti(database, table);
char buf[256];
char *setting;
htmlOpen("Output %s as %s", table, (doCt ? "Custom Track" : "BED"));
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=GET>\n");
cartSaveSession(cart);
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

if (isWiggle(database, table))
    {
    char *setting = NULL;
    hPrintf("<P> <B> Select type of data output: </B> <BR>\n");
    setting = cartCgiUsualString(cart, hgtaCtWigOutType, outWigData);
    cgiMakeRadioButton(hgtaCtWigOutType, outWigBed, sameString(setting, outWigBed));
    hPrintf("BED format (no data value information, only position)<BR>\n");
    cgiMakeRadioButton(hgtaCtWigOutType, outWigData, sameString(setting, outWigData));
    hPrintf("DATA VALUE format (position and real valued data)</P>\n");
    }
else
    {
    hPrintf("%s\n", "<P> <B> Create one BED record per: </B>");
    fbOptionsHtiCart(hti, cart);
    }
if (doCt)
    {
    cgiMakeButton(hgtaDoGetCustomTrackTb, "Get Custom Track in Table Browser");
    hPrintf(" ");
    cgiMakeButton(hgtaDoGetCustomTrackFile, "Get Custom Track in File");
    hPrintf("<BR>\n");
    cgiMakeButton(hgtaDoGetCustomTrackGb, "Get Custom Track in Genome Browser");
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

void doOutBed(char *table, struct sqlConnection *conn)
/* Put up form to select BED output format. */
{
doBedOrCtOptions(table, conn, FALSE);
}

void doOutCustomTrack(char *table, struct sqlConnection *conn)
/* Put up form to select Custom Track output format. */
{
doBedOrCtOptions(table, conn, TRUE);
}

void doGetBedOrCt(struct sqlConnection *conn, boolean doCt, 
	boolean doCtFile, boolean redirectToGb)
/* Actually output bed or custom track. */
{
char *table = curTable;
struct hTableInfo *hti = getHti(database, table);
struct featureBits *fbList = NULL, *fbPtr;
struct customTrack *ctNew = NULL;
boolean doCtHdr = (cartUsualBoolean(cart, hgtaPrintCustomTrackHeaders, FALSE) 
	|| doCt || doCtFile);
char *ctName = cgiUsualString(hgtaCtName, table);
char *ctDesc = cgiUsualString(hgtaCtDesc, table);
char *ctVis  = cgiUsualString(hgtaCtVis, "dense");
char *ctUrl  = cgiUsualString(hgtaCtUrl, "");
char *ctWigOutType = cartCgiUsualString(cart, hgtaCtWigOutType, outWigData);
char *fbQual = fbOptionsToQualifier();
char fbTQ[128];
int fields = hTableInfoBedFieldCount(hti);
boolean gotResults = FALSE;
struct region *region, *regionList = getRegions();
boolean wigOutData = FALSE;
boolean isWig = FALSE;
struct wigAsciiData *wigDataList = NULL;

if (!doCt)
    {
    textOpen();
    }

isWig = isWiggle(database, table);

if (isWig && sameString(outWigData, ctWigOutType))
    wigOutData = TRUE;

for (region = regionList; region != NULL; region = region->next)
    {
    struct bed *bedList = NULL, *bed;
    struct lm *lm = lmInit(64*1024);

    if (isWig && wigOutData)
	{
	int count = 0;
	struct wigAsciiData *wigData = NULL;
	struct wigAsciiData *asciiData;
	struct wigAsciiData *next;

	wigData = getWiggleAsData(conn, curTable, region, lm);
	for (asciiData = wigData; asciiData; asciiData = next)
	    {
	    next = asciiData->next;
	    if (asciiData->count)
		{
		slAddHead(&wigDataList, asciiData);
		++count;
		}
	    }
	slReverse(&wigDataList);
	}
    else
	{
	bedList = cookedBedList(conn, curTable, region, lm, &fields);
	}

    /*	this is a one-time only initial creation of the custom track
     *	structure to receive the results.  gotResults turns it off after
     *	the first time.
     */
    if (doCtHdr && ((bedList != NULL) || (wigDataList != NULL)) && !gotResults)
	{
	int visNum = (int) hTvFromStringNoAbort(ctVis);
	if (visNum < 0)
	    visNum = 0;
	if (doCt)
	    {
	    ctNew = newCt(ctName, ctDesc, visNum, ctUrl, fields);
	    if (isWig && wigOutData)
		{
		struct dyString *wigSettings = newDyString(0);
		struct tempName tn;
		makeTempName(&tn, hgtaCtTempNamePrefix, ".wig");
		ctNew->wigFile = cloneString(tn.forCgi);
		makeTempName(&tn, hgtaCtTempNamePrefix, ".wib");
		ctNew->wibFile = cloneString(tn.forCgi);
		makeTempName(&tn, hgtaCtTempNamePrefix, ".wia");
		ctNew->wigAscii = cloneString(tn.forCgi);
		ctNew->wiggle = TRUE;
		dyStringPrintf(wigSettings,
                            "type='wiggle_0'\twigFile='%s'\twibFile='%s'",
                            ctNew->wigFile, ctNew->wibFile);
                ctNew->tdb->settings = dyStringCannibalize(&wigSettings);
		}
	    }
	else
	    {
	    if (isWig)
		hPrintf("track type=wiggle_0 name=\"%s\" description=\"%s\" visibility=%d url=%s \n",
		   ctName, ctDesc, visNum, ctUrl);
	    else
		hPrintf("track name=\"%s\" description=\"%s\" visibility=%d url=%s \n",
		   ctName, ctDesc, visNum, ctUrl);
	    }
	}

    if (isWig && wigOutData && wigDataList)
	gotResults = TRUE;
    else
	{
	if ((fbQual == NULL) || (fbQual[0] == 0))
	    {
	    for (bed = bedList;  bed != NULL;  bed = bed->next)
		{
		if (bed->name != NULL)
		    {
		    char *ptr = strchr(bed->name, ' ');
		    if (ptr != NULL)
			*ptr = 0;
		    }
		if (doCt)
		    {
		    struct bed *dupe = cloneBed(bed); /* Out of local memory. */
		    slAddHead(&ctNew->bedList, dupe);
		    }
		else
		    bedTabOutN(bed, fields, stdout);

		gotResults = TRUE;
		}
	    }
	else
	    {
	    safef(fbTQ, sizeof(fbTQ), "%s:%s", hti->rootName, fbQual);
	    fbList = fbFromBed(fbTQ, hti, bedList, 0, 0, FALSE, FALSE);
	    for (fbPtr=fbList;  fbPtr != NULL;  fbPtr=fbPtr->next)
		{
		if (fbPtr->name != NULL)
		    {
		    char *ptr = strchr(fbPtr->name, ' ');
		    if (ptr != NULL)
			*ptr = 0;
		    }
		if (doCt)
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
	}
    bedList = NULL;
    lmCleanup(&lm);
    }
if (!gotResults)
    {
    if (!doCt)
	hPrintf("\n# No results returned from query.\n\n");
    }
else if (doCt)
    {
    char *wigPosition = NULL;

    /* Load existing custom tracks and add this new one: */
	{
	struct customTrack *ctList = getCustomTracks();
	char *ctFileName = cartOptionalString(cart, "ct");
	struct tempName tn;
	removeNamedCustom(&ctList, ctNew->tdb->tableName);
	if (isWig && wigOutData)
	    {
	    unsigned i;
	    unsigned chromEnd;
	    struct asciiDatum *aData;
	    char posBuf[256];
	    struct wiggleDataStream *wds = NULL;
	    /*	create an otherwise empty wds so we can print out the list */
	    wds = wiggleDataStreamNew();
	    wds->ascii = wigDataList;
	    wds->asciiOut(wds, ctNew->wigAscii, TRUE, FALSE);
#if defined(DEBUG)    /*      dbg     */
	    /* allow file readability for debug */
	    chmod(ctNew->wigAscii, 0666);
#endif
	    aData = wds->ascii->data;
	    chromEnd = 0;
	    for( i = 0; i < wds->ascii->count; ++i, ++aData)
		if (aData->chromStart > chromEnd) chromEnd = aData->chromStart;

	    chromEnd += wds->ascii->span;

	    safef(posBuf, sizeof(posBuf), 
		"%s:%d-%d", wds->ascii->chrom, wds->ascii->data->chromStart,
		chromEnd);
	    wigPosition = cloneString(posBuf);
	    
	    wiggleDataStreamFree(&wds);
	    }
	else
	    slReverse(&ctNew->bedList);

	slAddHead(&ctList, ctNew);
	/* Save the custom tracks out to file (overwrite the old file): */
	if (ctFileName == NULL)
	    {
	    makeTempName(&tn, hgtaCtTempNamePrefix, ".bed");
	    ctFileName = cloneString(tn.forCgi);
	    }
	customTrackSave(ctList, ctFileName);
	cartSetString(cart, "ct", ctFileName);
	}
    /*  Put up redirect-to-browser page. */
    if (redirectToGb)
	{
	char browserUrl[256];
	char headerText[512];
	char posBuf[256];
	char *position;
	int redirDelay = 3;
	if (isWig && wigOutData)
	    position = wigPosition;
	else
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
else if (isWig && wigOutData)
    {
    /*	create an otherwise empty wds so we can print out the list */
    struct wiggleDataStream *wds = NULL;
    wds = wiggleDataStreamNew();
    wds->ascii = wigDataList;
    wds->asciiOut(wds, "stdout", TRUE, FALSE);
    wiggleDataStreamFree(&wds);
    }
}

void doGetBed(struct sqlConnection *conn)
/* Get BED output (UI has already told us how). */
{
doGetBedOrCt(conn, FALSE, FALSE, FALSE);
}

void doGetCustomTrackGb(struct sqlConnection *conn)
/* Get Custom Track output (UI has already told us how). */
{
doGetBedOrCt(conn, TRUE, FALSE, TRUE);
}

void doGetCustomTrackTb(struct sqlConnection *conn)
/* Get Custom Track output (UI has already told us how). */
{
doGetBedOrCt(conn, TRUE, FALSE, FALSE);
flushCustomTracks();
initGroupsTracksTables(conn);
doMainPage(conn);
}

void doGetCustomTrackFile(struct sqlConnection *conn)
/* Get Custom Track file output (UI has already told us how). */
{
doGetBedOrCt(conn, FALSE, TRUE, FALSE);
}

