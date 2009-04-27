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
#include "correlate.h"
#include "bedCart.h"
#include "trashDir.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: bedList.c,v 1.60 2008/09/03 19:18:58 markd Exp $";

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
int fieldCount = hTableInfoBedFieldCount(hti);

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
	dyStringPrintf(fields, ",0");
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
sqlSignedArray(row[8], bed->blockSizes, blockCount);
lmAllocArray(lm, bed->chromStarts, blockCount);
sqlSignedArray(row[9], bed->chromStarts, blockCount);
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
	 * (this is the target part of pslRc from src/lib/psl.c) */
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

struct bed *getRegionAsBed(
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

hti = hFindTableInfo(db, region->chrom, table);
if (hti == NULL)
    errAbort("Could not find table info for table %s.%s", db,table);

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
    slReverse(&bedList);
    }
sqlDisconnect(&conn);
if (retFieldCount)
    *retFieldCount = fieldCount;
return(bedList);
}

struct bed *getFilteredBeds(struct sqlConnection *conn,
	char *table, struct region *region, struct lm *lm, int *retFieldCount)
/* Get list of beds on single region that pass filtering. */
{
/* region may be part of a list, and the routines we call work on lists of 
 * regions.  Temporarily force region->next to NULL and restore at end. */
struct region *oldNext = region->next;
struct bed *bedList = NULL;
region->next = NULL;

if (isCustomTrack(table))
    bedList = customTrackGetFilteredBeds(table, region, lm, retFieldCount);
else if (sameWord(table, WIKI_TRACK_TABLE))
    bedList = wikiTrackGetFilteredBeds(table, region, lm, retFieldCount);
else
    bedList = dbGetFilteredBedsOnRegions(conn, table, region, lm,
					 retFieldCount);
region->next = oldNext;
return bedList;
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

char *ctVisWigMenu[] =
{
    "hide",
    "dense",
    "full",
};
int ctVisWigMenuSize = 3;

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
if (doGalaxy()) 
    startGalaxyForm();
else
    hPrintf("<FORM ACTION=\"%s\" METHOD=GET>\n", getScriptName());
cartSaveSession(cart);
hPrintf("%s\n", "<TABLE><TR><TD>");
if (doCt)
    {
    hPrintf("%s\n", "</TD><TD>"
	 "<A HREF=\"../goldenPath/help/customTrack.html\" TARGET=_blank>"
	 "Custom track</A> header: </B>");
    }
else
    {
    cgiMakeCheckBox(hgtaPrintCustomTrackHeaders,
	    cartCgiUsualBoolean(cart, hgtaPrintCustomTrackHeaders, FALSE));
    hPrintf("%s\n", "</TD><TD> <B> Include "
	 "<A HREF=\"../goldenPath/help/customTrack.html\" TARGET=_blank>"
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
if (isWiggle(database, table))
    {
    setting = cartCgiUsualString(cart, hgtaCtVis, ctVisWigMenu[2]);
    cgiMakeDropList(hgtaCtVis, ctVisWigMenu, ctVisWigMenuSize, setting);
    }
else
    {
    setting = cartCgiUsualString(cart, hgtaCtVis, ctVisMenu[3]);
    cgiMakeDropList(hgtaCtVis, ctVisMenu, ctVisMenuSize, setting);
    }
hPrintf("%s\n", "</TD></TR><TR><TD></TD><TD>url=");
setting = cartCgiUsualString(cart, hgtaCtUrl, "");
cgiMakeTextVar(hgtaCtUrl, setting, 50);
hPrintf("%s\n", "</TD></TR><TR><TD></TD><TD>");
hPrintf("%s\n", "</TD></TR></TABLE>");

if (isWiggle(database, table) || isBedGraph(table))
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
    if ((anyIntersection() && intersectionIsBpWise()) ||
	(anySubtrackMerge(database, table) && subtrackMergeIsBpWise()))
	{
	/* The original table may have blocks/CDS, described in hti, but 
	 * that info will be lost after base pair-wise operations.  So make 
	 * a temporary copy of hti with its flags tweaked: */
	struct hTableInfo simplifiedHti;
	memcpy(&simplifiedHti, hti, sizeof(simplifiedHti));
	simplifiedHti.hasBlocks = FALSE;
	simplifiedHti.hasCDS = FALSE;
	fbOptionsHtiCart(&simplifiedHti, cart);
	}
    else
	fbOptionsHtiCart(hti, cart);
    }
if (doCt)
    {
    if (doGalaxy()) 
        {
        /* send the action parameter with the form as well */
        cgiMakeHiddenVar(hgtaDoGetCustomTrackFile, "get custom track in file");
        printGalaxySubmitButtons();
        }
    else 
        {
        cgiMakeButton(hgtaDoGetCustomTrackTb, "get custom track in table browser");
        hPrintf(" ");
        cgiMakeButton(hgtaDoGetCustomTrackFile, "get custom track in file");
        hPrintf("<BR>\n");
        cgiMakeButton(hgtaDoGetCustomTrackGb, "get custom track in genome browser");
        }
    }
else
    {
    if (doGalaxy()) 
        {
        cgiMakeHiddenVar(hgtaDoGetBed, "get BED");
        printGalaxySubmitButtons();
        }
    else 
        cgiMakeButton(hgtaDoGetBed, "get BED");
    }
if (!doGalaxy()) 
    {
    hPrintf(" ");
    cgiMakeButton(hgtaDoMainPage, "cancel");
    hPrintf("</FORM>\n");
    }
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

static struct customTrack *beginCustomTrack(char *table, int fields,
	boolean doCt, boolean doWigHdr, boolean doDataPoints)
/* If doCt, return a new custom track object from TB cart settings and params;
 * if !doCt, return NULL but print out a header line. */
{
char *ctName = cgiUsualString(hgtaCtName, table);
char *ctDesc = cgiUsualString(hgtaCtDesc, table);
char *ctUrl  = cgiUsualString(hgtaCtUrl, "");
char *ctVis  = cgiUsualString(hgtaCtVis, "dense");
int visNum = (int) hTvFromStringNoAbort(ctVis);
struct customTrack *ctNew = NULL;

if (visNum < 0)
    visNum = 0;
if (doCt)
    {
    ctNew = newCt(ctName, ctDesc, visNum, ctUrl, fields);
    if (doDataPoints)
	{
	struct dyString *wigSettings = newDyString(0);
	struct tempName tn;
	trashDirFile(&tn, "ct", hgtaCtTempNamePrefix, ".wib");
	ctNew->wibFile = cloneString(tn.forCgi);
	char *wiggleFile = cloneString(ctNew->wibFile);
	chopSuffix(wiggleFile);
	strcat(wiggleFile, ".wia");
	ctNew->wigAscii = cloneString(wiggleFile);
	chopSuffix(wiggleFile);
	/* .wig file will be created upon encoding in customFactory */
	/*strcat(wiggleFile, ".wig");
	ctNew->wigFile = cloneString(wiggleFile);
	*/
	ctNew->wiggle = TRUE;
	dyStringPrintf(wigSettings,
		       "type wiggle_0\nwibFile %s\n", ctNew->wibFile);
	ctNew->tdb->settings = dyStringCannibalize(&wigSettings);
	freeMem(wiggleFile);
	}
    }
else
    {
    if (doWigHdr)
	hPrintf("track type=wiggle_0 name=\"%s\" description=\"%s\" "
		"visibility=%d url=%s \n",
		ctName, ctDesc, visNum, ctUrl);
    else
	hPrintf("track name=\"%s\" description=\"%s\" visibility=%d url=%s \n",
		ctName, ctDesc, visNum, ctUrl);
    }
return ctNew;
}

boolean doGetBedOrCt(struct sqlConnection *conn, boolean doCt, 
		     boolean doCtFile, boolean redirectToGb)
/* Actually output bed or custom track. Return TRUE unless no results. */
{
char *db = sqlGetDatabase(conn);
char *table = curTable;
struct hTableInfo *hti = getHti(db, table);
struct featureBits *fbList = NULL, *fbPtr;
struct customTrack *ctNew = NULL;
boolean doCtHdr = (cartUsualBoolean(cart, hgtaPrintCustomTrackHeaders, FALSE) 
	|| doCt || doCtFile);
char *ctWigOutType = cartCgiUsualString(cart, hgtaCtWigOutType, outWigData);
char *fbQual = fbOptionsToQualifier();
char fbTQ[128];
int fields = hTableInfoBedFieldCount(hti);
boolean gotResults = FALSE;
struct region *region, *regionList = getRegions();
boolean isBedGr = isBedGraph(curTable);
boolean needSubtrackMerge = anySubtrackMerge(database, curTable);
boolean doDataPoints = FALSE;
boolean isWig = isWiggle(database, table);
struct wigAsciiData *wigDataList = NULL;
struct dataVector *dataVectorList = NULL;

if (!doCt)
    {
    textOpen();
    }

if ((isWig || isBedGr) && sameString(outWigData, ctWigOutType))
    doDataPoints = TRUE;

for (region = regionList; region != NULL; region = region->next)
    {
    struct bed *bedList = NULL, *bed;
    struct lm *lm = lmInit(64*1024);
    struct dataVector *dv = NULL;

    if (isWig && doDataPoints)
	{
	if (needSubtrackMerge)
	    {
	    dv = wiggleDataVector(curTrack, curTable, conn, region);
	    if (dv != NULL)
		slAddHead(&dataVectorList, dv);
	    }
	else
	    {
	    int count = 0;
	    struct wigAsciiData *wigData = NULL;
	    struct wigAsciiData *asciiData;
	    struct wigAsciiData *next;
	    
	    wigData = getWiggleAsData(conn, curTable, region);
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
	}
    else if (isBedGr && doDataPoints)
	{
	dv = bedGraphDataVector(curTable, conn, region);
	if (dv != NULL)
	    slAddHead(&dataVectorList, dv);
	}
    else if (isWig)
	{
	dv = wiggleDataVector(curTrack, curTable, conn, region);
	bedList = dataVectorToBedList(dv);
	dataVectorFree(&dv);
	}
    else if (isBedGr)
	{
	bedList = getBedGraphAsBed(conn, curTable, region);
	}
    else
	{
	bedList = cookedBedList(conn, curTable, region, lm, &fields);
	}

    /*	this is a one-time only initial creation of the custom track
     *	structure to receive the results.  gotResults turns it off after
     *	the first time.
     */
    if (doCtHdr && !gotResults &&
	((bedList != NULL) || (wigDataList != NULL) ||
	 (dataVectorList != NULL)))
	{
	ctNew = beginCustomTrack(table, fields,
				 doCt, (isWig || isBedGr), doDataPoints);
	}

    if (doDataPoints && (wigDataList || dataVectorList))
	gotResults = TRUE;
    else
	{
	if ((fbQual == NULL) || (fbQual[0] == 0))
	    {
	    for (bed = bedList;  bed != NULL;  bed = bed->next)
		{
		if (bed->name != NULL)
		    {
		    subChar(bed->name, ' ', '_');
		    }
		if (doCt)
		    {
		    struct bed *dupe = cloneBed(bed); /* Out of local memory. */
		    slAddHead(&ctNew->bedList, dupe);
		    }
		else
		    {
		    if (bedItemRgb(curTrack))
			bedTabOutNitemRgb(bed, fields, stdout);
		    else
			bedTabOutN(bed, fields, stdout);
		    }

		gotResults = TRUE;
		}
	    }
	else
	    {
	    safef(fbTQ, sizeof(fbTQ), "%s:%s", hti->rootName, fbQual);
	    fbList = fbFromBed(db, fbTQ, hti, bedList, 0, 0, FALSE, FALSE);
	    if (fields >= 6)
		fields = 6;
	    else if (fields >= 4)
		fields = 4;
	    else
		fields = 3;
	    if (doCt && ctNew)
		{
		ctNew->fieldCount = fields;
		safef(ctNew->tdb->type, strlen(ctNew->tdb->type)+1,
		      "bed %d", fields);
		}
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
		    if (fields >= 6)
			hPrintf("%s\t%d\t%d\t%s\t%d\t%c\n",
			   fbPtr->chrom, fbPtr->start, fbPtr->end, fbPtr->name,
			   0, fbPtr->strand);
		    else if (fields >= 4)
			hPrintf("%s\t%d\t%d\t%s\n",
			  fbPtr->chrom, fbPtr->start, fbPtr->end, fbPtr->name);
		    else
			hPrintf("%s\t%d\t%d\n",
				fbPtr->chrom, fbPtr->start, fbPtr->end);
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
    hPrintf("\n# No results returned from query.\n\n");
    }
else if (doCt)
    {
    int wigDataSize = 0;

    /* Load existing custom tracks and add this new one: */
	{
	struct customTrack *ctList = getCustomTracks();
	removeNamedCustom(&ctList, ctNew->tdb->tableName);
	if (doDataPoints)
	    {
	    if (needSubtrackMerge || isBedGr)
		{
		slReverse(&dataVectorList);
		wigDataSize =
	      dataVectorWriteWigAscii(dataVectorList, ctNew->wigAscii,
				      0, NULL);
		}
	    else
		{
		struct asciiDatum *aData;
		struct wiggleDataStream *wds = NULL;
		/* create an otherwise empty wds so we can print out the list */
		wds = wiggleDataStreamNew();
		wds->ascii = wigDataList;
		wigDataSize = wds->asciiOut(wds, db, ctNew->wigAscii, TRUE, FALSE);
#if defined(DEBUG)    /*      dbg     */
		/* allow file readability for debug */
		chmod(ctNew->wigAscii, 0666);
#endif
		aData = wds->ascii->data;
		wiggleDataStreamFree(&wds);
		}
	    }
	else
	    slReverse(&ctNew->bedList);

	slAddHead(&ctList, ctNew);
	/* Save the custom tracks out to file (overwrite the old file): */
        customTracksSaveCart(db, cart, ctList);
	}
    /*  Put up redirect-to-browser page. */
    if (redirectToGb)
	{
	char browserUrl[256];
	char headerText[512];
	int redirDelay = 3;
	safef(browserUrl, sizeof(browserUrl),
	      "%s?%s&db=%s", hgTracksName(), cartSidUrlString(cart), database);
	safef(headerText, sizeof(headerText),
	      "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"%d;URL=%s\">",
	      redirDelay, browserUrl);
	webStartHeader(cart, database, headerText,
		       "Table Browser: %s %s: %s", hOrganism(database), 
		       freezeName, "get custom track");
	if (doDataPoints)
	    {
	    hPrintf("There are %d data points in custom track. ", wigDataSize);
	    }
	else
	    {
	    hPrintf("There are %d items in custom track. ",
		    slCount(ctNew->bedList));
	    }
	hPrintf("You will be automatically redirected to the genome browser in\n"
	       "%d seconds, or you can \n"
	       "<A HREF=\"%s\">click here to continue</A>.\n",
	       redirDelay, browserUrl);
	}
    }
else if (doDataPoints)
    {
    if (needSubtrackMerge || isBedGr)
	{
	slReverse(&dataVectorList);
	dataVectorWriteWigAscii(dataVectorList, "stdout", 0, NULL);
	}
    else
	{
	/*	create an otherwise empty wds so we can print out the list */
	struct wiggleDataStream *wds = NULL;
	wds = wiggleDataStreamNew();
	wds->ascii = wigDataList;
	wds->asciiOut(wds, db, "stdout", TRUE, FALSE);
	wiggleDataStreamFree(&wds);
	}
    }
return gotResults;
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
boolean gotResults = doGetBedOrCt(conn, TRUE, FALSE, FALSE);
if (gotResults)
    {
    flushCustomTracks();
    initGroupsTracksTables(conn);
    doMainPage(conn);
    }
}

void doGetCustomTrackFile(struct sqlConnection *conn)
/* Get Custom Track file output (UI has already told us how). */
{
doGetBedOrCt(conn, FALSE, TRUE, FALSE);
}

