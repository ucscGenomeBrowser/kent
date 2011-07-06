/* intersect - handle intersecting beds. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "trackDb.h"
#include "bits.h"
#include "bed.h"
#include "hdb.h"
#include "featureBits.h"
#include "jsHelper.h"
#include "hgTables.h"
#include "customTrack.h"
#include "hubConnect.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: intersect.c,v 1.54 2010/06/03 18:53:59 kent Exp $";

/* We keep two copies of variables, so that we can
 * cancel out of the page. */

static char *curVars[] = {hgtaIntersectGroup, hgtaIntersectTrack,
	hgtaIntersectTable,
	hgtaIntersectOp, hgtaMoreThreshold, hgtaLessThreshold,
	hgtaInvertTable, hgtaInvertTable2,
	};
static char *nextVars[] = {hgtaNextIntersectGroup, hgtaNextIntersectTrack,
	hgtaNextIntersectTable,
	hgtaNextIntersectOp, hgtaNextMoreThreshold, hgtaNextLessThreshold,
	hgtaNextInvertTable, hgtaNextInvertTable2,
	};

/* This is already duplicated in correlate.c and is handy -- should be
 * libified, probably in cart.h. */
void removeCartVars(struct cart *cart, char **vars, int varCount);

static boolean canIntersect(char *db, char *table)
/* Return true if table exists and is positional. */
{
if (isCustomTrack(table) && ctLookupName(table) != NULL)
    return TRUE;
if (isBamTable(table))
    return TRUE;
if (isBigWigTable(table))
    return TRUE;
if (isBigBed(database, table, curTrack, ctLookupName))
    return TRUE;
if (isHubTrack(table))
    return TRUE;
if (sameWord(table, WIKI_TRACK_TABLE))
    return TRUE;
if (hTableOrSplitExists(db, table))
    return isPositional(db, table);
return FALSE;
}

boolean anyIntersection()
/* Return TRUE if there's an intersection to do. */
{
boolean specd = (cartVarExists(cart, hgtaIntersectTrack) &&
		 cartVarExists(cart, hgtaIntersectTable));
if (specd && canIntersect(database, curTable))
    {
    char *table = cartString(cart, hgtaIntersectTable);
    if (canIntersect(database, table))
	return TRUE;
    else
	{
	/* If the specified intersect table doesn't exist (e.g. if we
	 * just changed databases), clear the settings. */
	removeCartVars(cart, curVars, ArraySize(curVars));
	return FALSE;
	}
    }
else
    return FALSE;
}

boolean intersectionIsBpWise()
/* Return TRUE if the intersection/union operation is base pair-wise. */
{
char *op = cartUsualString(cart, hgtaIntersectOp, "any");
return (sameString(op, "and") || sameString(op, "or"));
}

static char *onChangeEnd(struct dyString **pDy)
/* Finish up javascript onChange command. */
{
dyStringAppend(*pDy, "document.hiddenForm.submit();\"");
return dyStringCannibalize(pDy);
}

static struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = dyStringNew(1024);
dyStringAppend(dy, "onChange=\"");
jsDropDownCarryOver(dy, hgtaNextIntersectGroup);
jsDropDownCarryOver(dy, hgtaNextIntersectTrack);
jsDropDownCarryOver(dy, hgtaNextIntersectTable);
jsTrackedVarCarryOver(dy, hgtaNextIntersectOp, "op");
jsTextCarryOver(dy, hgtaNextMoreThreshold);
jsTextCarryOver(dy, hgtaNextLessThreshold);
if (!isBigWigTable(curTable))
    jsTrackedVarCarryOver(dy, hgtaNextInvertTable, "invertTable");
jsTrackedVarCarryOver(dy, hgtaNextInvertTable2, "invertTable2");
return dy;
}

static char *onChangeEither()
/* Get group-changing javascript. */
{
struct dyString *dy = onChangeStart();
return onChangeEnd(&dy);
}

void makeOpButton(char *val, char *selVal)
/* Make region radio button including a little Javascript
 * to save selection state. */
{
jsMakeTrackingRadioButton(hgtaNextIntersectOp, "op", val, selVal);
}

struct trackDb *showGroupTrackRow(char *groupVar, char *groupScript,
    char *trackVar, char *trackScript, struct sqlConnection *conn)
/* Show group & track row of controls.  Returns selected track */
{
struct trackDb *track;
struct grp *selGroup;
hPrintf("<TR><TD>");
selGroup = showGroupField(groupVar, groupScript, conn, FALSE);
track = showTrackField(selGroup, trackVar, trackScript);
hPrintf("</TD></TR>\n");
return track;
}


void doIntersectMore(struct sqlConnection *conn)
/* Continue working in intersect page. */
{
struct trackDb *iTrack;
char *name = curTableLabel();
char *iName, *iTable;
char *onChange = onChangeEither();
char *op, *setting;
boolean wigOptions = (isWiggle(database, curTable) || isBedGraph(curTable));
// Note - bigWig is purposely left out of wigOptions.   It supports more intersection options
// than wig does.
struct hTableInfo *hti1 = maybeGetHti(database, curTable, conn), *hti2 = NULL;
htmlOpen("Intersect with %s", name);

hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=GET>\n", getScriptName());
cartSaveSession(cart);
hPrintf("<TABLE BORDER=0>\n");
/* Print group and track line. */

hPrintf("Select a group, track and table to intersect with:\n");
iTrack = showGroupTrackRow(hgtaNextIntersectGroup, onChange,
	hgtaNextIntersectTrack, onChange, conn);
iName = iTrack->shortLabel;

hPrintf("<TR><TD>\n");
iTable = showTableField(iTrack, hgtaNextIntersectTable, FALSE);
hti2 = maybeGetHti(database, iTable, conn);
hPrintf("</TD></TR>\n");
hPrintf("</TABLE>\n");

if (!wigOptions)
    {
    if (hti1 && hti1->hasBlocks)
	hPrintf("<BR>Note: %s has gene/alignment structure.  "
		"Only the exons/blocks will be considered.\n", name);
    if (hti2 && hti2->hasBlocks)
	hPrintf("<BR>Note: %s has gene/alignment structure.  "
		"Only the bases covered by its exons/blocks will be "
		"considered.\n", iName);
    hPrintf("<H4>Intersect %s items with bases covered by %s:</H4>\n",
	    name, iName);
    hPrintf("These combinations will maintain the names and "
	    "gene/alignment structure (if any) of %s: <P>\n",
	    name);
    }
else
    hPrintf("<P>\n");

op = cartUsualString(cart, hgtaNextIntersectOp, "any");
jsTrackingVar("op", op);
makeOpButton("any", op);
printf("All %s records that have any overlap with %s <BR>\n",
       name, iName);
makeOpButton("none", op);
printf("All %s records that have no overlap with %s <BR>\n",
       name, iName);

if (!wigOptions)
    {
    makeOpButton("more", op);
    printf("All %s records that have at least ",
	   name);
    setting = cartCgiUsualString(cart, hgtaNextMoreThreshold, "80");
    cgiMakeTextVar(hgtaNextMoreThreshold, setting, 3);
    printf(" %% overlap with %s <BR>\n", iName);
    makeOpButton("less", op);
    printf("All %s records that have at most ",
	   name);
    setting = cartCgiUsualString(cart, hgtaNextLessThreshold, "80");
    cgiMakeTextVar(hgtaNextLessThreshold, setting, 3);
    printf(" %% overlap with %s <P>\n", iName);
    }
else
    {
    /*	keep javaScript onClick happy	*/
    hPrintf("<input TYPE=HIDDEN NAME=\"hgta_nextMoreThreshold\" VALUE=80>\n");
    hPrintf("<input TYPE=HIDDEN NAME=\"hgta_nextLessThreshold\" VALUE=80>\n");
    hPrintf(" <P>\n");
    }


if (!wigOptions)
    {
    boolean bigWig = isBigWigTable(curTable);
    hPrintf("<H4>Intersect bases covered by %s and/or %s:</H4>\n",
	    name, iName);
    if (!bigWig)
	hPrintf("These combinations will discard the names and "
		"gene/alignment structure (if any) of %s and produce a simple "
		"list of position ranges.<P>\n",
		name);
    makeOpButton("and", op);
    printf("Base-pair-wise intersection (AND) of %s and %s <BR>\n",
	name, iName);
    makeOpButton("or", op);
    printf("Base-pair-wise union (OR) of %s and %s <P>\n",
	name, iName);
    hPrintf("Check the following boxes to complement one or both tables.  "
	    "To complement a table means to include a base pair in the "
	    "intersection/union if it is <I>not</I> included in the table."
	    "<P>\n");
    if (!bigWig)
	{
	jsMakeTrackingCheckBox(cart, hgtaNextInvertTable, "invertTable", FALSE);
	printf("Complement %s before base-pair-wise intersection/union <BR>\n",
	       name);
	}
    jsMakeTrackingCheckBox(cart, hgtaNextInvertTable2, "invertTable2", FALSE);
    printf("Complement %s before base-pair-wise intersection/union <P>\n",
	   iName);
    }
else
    {
    /*	keep javaScript onClick happy	*/
    jsTrackingVar("op", op);
    hPrintf("<SCRIPT>\n");
    hPrintf("var invertTable=0;\n");
    hPrintf("var invertTable2=0;\n");
    hPrintf("</SCRIPT>\n");
    hPrintf("(data track %s is not composed of gene records.  Specialized intersection operations are not available.)<P>\n", name);
    }

cgiMakeButton(hgtaDoIntersectSubmit, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("</FORM>\n");

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[32];
    int varCount = ArraySize(nextVars);
    memcpy(saveVars, nextVars, varCount * sizeof(saveVars[0]));
    saveVars[varCount] = hgtaDoIntersectMore;
    jsCreateHiddenForm(cart, getScriptName(), saveVars, varCount+1);
    }

htmlClose();
}

void removeCartVars(struct cart *cart, char **vars, int varCount)
/* Remove array of variables from cart. */
{
int i;
for (i=0; i<varCount; ++i)
    cartRemove(cart, vars[i]);
}

void copyCartVars(struct cart *cart, char **source, char **dest, int count)
/* Copy from source to dest. */
{
int i;
for (i=0; i<count; ++i)
    {
    char *s = cartOptionalString(cart, source[i]);
    if (s != NULL)
        cartSetString(cart, dest[i], s);
    else
        cartRemove(cart, dest[i]);
    }
}


void doIntersectPage(struct sqlConnection *conn)
/* Respond to intersect create/edit button */
{
if (ArraySize(curVars) != ArraySize(nextVars))
    internalErr();
copyCartVars(cart, curVars, nextVars, ArraySize(curVars));
doIntersectMore(conn);
}

void doClearIntersect(struct sqlConnection *conn)
/* Respond to click on clear intersection. */
{
removeCartVars(cart, curVars, ArraySize(curVars));
doMainPage(conn);
}

void doIntersectSubmit(struct sqlConnection *conn)
/* Respond to submit on intersect page. */
{
copyCartVars(cart, nextVars, curVars, ArraySize(curVars));
doMainPage(conn);
}


static void bitSetClippedRange(Bits *bits, int bitSize, int s, int e)
/* Clip start and end to [0,bitSize) and set range. */
{
int w;
if (e > bitSize) e = bitSize;
if (s < 0) s = 0;
w = e - s;
if (w > 0)
    bitSetRange(bits, s, w);
}

static void bedOrBits(Bits *bits, int bitSize, struct bed *bedList,
		      boolean hasBlocks, int bitOffset)
/* Or in bits.   Bits should have bitSize bits.  */
{
struct bed *bed;
if (hasBlocks)
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	int i;
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    int s = bed->chromStart + bed->chromStarts[i] - bitOffset;
	    int e = s + bed->blockSizes[i];
	    bitSetClippedRange(bits, bitSize, s, e);
	    }
	}
else
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	bitSetClippedRange(bits, bitSize, (bed->chromStart - bitOffset),
			   (bed->chromEnd - bitOffset));
	}
}

static int countBasesOverlap(struct bed *bedItem, Bits *bits, boolean hasBlocks,
			     int chromSize)
/* Return the number of bases belonging to bedItem covered by bits. */
{
int count = 0;
int i;

if (bedItem->chromStart < 0 || bedItem->chromEnd > chromSize)
    errAbort("Item out of range [0,%d): %s %s:%d-%d",
	     chromSize, (bedItem->name ? bedItem->name : ""),
	     bedItem->chrom, bedItem->chromStart, bedItem->chromEnd);

if (bedItem->chromStart == bedItem->chromEnd)
    {
    /* Zero-size item: count overlap with adjacent bases. */
    for (i = bedItem->chromStart-1;  i < bedItem->chromEnd+1;  i++)
	if (i >= 0 && i < chromSize && bitReadOne(bits, i))
	    count++;
    }
else if (hasBlocks)
    {
    for (i=0;  i < bedItem->blockCount;  i++)
	{
	int start = bedItem->chromStart + bedItem->chromStarts[i];
	count += bitCountRange(bits, start, bedItem->blockSizes[i]);
	}
    }
else
    {
    count = bitCountRange(bits, bedItem->chromStart, bedItem->chromEnd - bedItem->chromStart);
    }
return(count);
}

static struct bed *bitsToBed4List(Bits *bits, int bitSize,
	char *chrom, int minSize, int rangeStart, int rangeEnd,
	struct lm *lm)
/* Translate ranges of set bits to bed 4 items. */
{
struct bed *bedList = NULL, *bed;
int start = 0;
int end = 0;
int id = 0;
char name[128];

if (rangeStart < 0)
    rangeStart = 0;
if (rangeEnd > bitSize)
    rangeEnd = bitSize;
end = rangeStart;

/* We depend on extra zero BYTE at end in case bitNot was used on bits. */
for (;;)
    {
    start = bitFindSet(bits, end, rangeEnd);
    if (start >= rangeEnd)
        break;
    end = bitFindClear(bits, start, rangeEnd);
    if (end - start >= minSize)
	{
	lmAllocVar(lm, bed);
	bed->chrom = chrom;
	bed->chromStart = start;
	bed->chromEnd = end;
	snprintf(name, sizeof(name), "%s.%d", chrom, ++id);
	bed->name = lmCloneString(lm, name);
	slAddHead(&bedList, bed);
	}
    }
slReverse(&bedList);
return(bedList);
}

boolean intersectOverlapFilter(char *op, double moreThresh, double lessThresh, double overlap)
/* Return TRUE if have enough (or not too much) overlap according to thresholds and op. */
{
return ((sameString("any", op) && (overlap > 0.0)) ||
    (sameString("none", op) && (overlap <= 0.0)) ||
    (sameString("more", op) && (overlap >= moreThresh)) ||
    (sameString("less", op) && (overlap <= lessThresh)));
}

static struct bed *filterBedByOverlap(struct bed *bedListIn, boolean hasBlocks,
				      char *op, double moreThresh, double lessThresh,
				      Bits *bits, int chromSize)
/* Return list of beds that pass overlap filter. */
{
struct bed *intersectedBedList = NULL;
struct bed *bed = NULL, *nextBed = NULL;
/* Loop through primary bed list seeing if each one intersects
 * enough to keep. */
for (bed = bedListIn;  bed != NULL;  bed = nextBed)
    {
    int numBasesOverlap = countBasesOverlap(bed, bits, hasBlocks, chromSize);
    int length = 0;
    double pctBasesOverlap;
    nextBed = bed->next;
    if (hasBlocks)
	{
	int i;
	for (i=0;  i < bed->blockCount;  i++)
	    length += bed->blockSizes[i];
	}
    else
	length = (bed->chromEnd - bed->chromStart);
    if (length == 0)
	length = 2;
    pctBasesOverlap = ((numBasesOverlap * 100.0) / length);
    if (intersectOverlapFilter(op, moreThresh, lessThresh, pctBasesOverlap))
	{
	slAddHead(&intersectedBedList, bed);
	}
    }
slReverse(&intersectedBedList);
return intersectedBedList;
}

void expandZeroSize(struct bed *bedList, boolean hasBlocks, int chromSize)
/* Expand any bed items with size=0 to size=2 so insertion points are not
 * invisible when intersecting with items (as opposed to base-pair-wise). */
{
struct bed *bed;
for (bed = bedList;  bed != NULL;  bed = bed->next)
    if (bed->chromStart == bed->chromEnd)
	{
	if (bed->chromStart > 0)
	    bed->chromStart--;
	if (bed->chromEnd < chromSize)
	    bed->chromEnd++;
	if (hasBlocks)
	    {
	    if (bed->blockCount != 1)
		errAbort("expandZeroItem: how did a zero-length bed (%s:%d-%d) with blocks "
			 "get a blockCount of %d not 1?",
			 bed->chrom, bed->chromStart, bed->chromEnd, bed->blockCount);
	    bed->chromStarts[0] = 0;
	    bed->blockSizes[0] = bed->chromEnd - bed->chromStart;
	    }
	}
}

Bits *bitsForIntersectingTable(struct sqlConnection *conn, struct region *region, int chromSize,
	boolean isBpWise)
/* Get a bitmap that corresponds to the table we are intersecting with.
 * Consult CGI vars to figure out what table it is. */
{
boolean invTable2 = cartCgiUsualBoolean(cart, hgtaInvertTable2, FALSE);
char *table2 = cartString(cart, hgtaIntersectTable);
struct hTableInfo *hti2 = getHti(database, table2, conn);
struct lm *lm2 = lmInit(64*1024);
Bits *bits2 = bitAlloc(chromSize+8);
struct bed *bedList2;
if (isBigWigTable(table2))
    bedList2 = bigWigIntervalsToBed(conn, table2, region, lm2);
else
    // We should go straight to raw beds here, not through the routines that
    // do filter & intersections, because the secondary table has no filter
    // and sure shouldn't be intersected. :)
    bedList2 = getFilteredBeds(conn, table2, region, lm2, NULL);
if (!isBpWise)
    expandZeroSize(bedList2, hti2->hasBlocks, chromSize);
bedOrBits(bits2, chromSize, bedList2, hti2->hasBlocks, 0);
if (invTable2)
    bitNot(bits2, chromSize);
lmCleanup(&lm2);
return bits2;
}

char *intersectOp()
/* Get intersect op from CGI var and make sure it's ok. */
{
char *op = cartString(cart, hgtaIntersectOp);
if ((!sameString("any", op)) &&
    (!sameString("none", op)) &&
    (!sameString("more", op)) &&
    (!sameString("less", op)) &&
    (!sameString("and", op)) &&
    (!sameString("or", op)))
    {
    errAbort("Invalid value \"%s\" of CGI variable %s", op, hgtaIntersectOp);
    }
return op;
}

static struct bed *intersectOnRegion(
	struct sqlConnection *conn,	/* Open connection to database. */
	struct region *region, 		/* Region to work inside */
	char *table1,			/* Table input list is from. */
	struct bed *bedList1,	/* List before intersection, should be
	                                 * all within region. */
	struct lm *lm,	   /* Local memory pool. */
	int *retFieldCount)	   /* Field count. */
/* Intersect bed list, consulting CGI vars to figure out
 * with what table and how.  Return intersected result,
 * which is independent from input.  This potentially will
 * chew up bedList1. */
{
/* Grab parameters for intersection from cart. */
double moreThresh = cartCgiUsualDouble(cart, hgtaMoreThreshold, 0);
double lessThresh = cartCgiUsualDouble(cart, hgtaLessThreshold, 100);
boolean invTable = cartCgiUsualBoolean(cart, hgtaInvertTable, FALSE);
char *op = intersectOp();
/* --- TODO MIKE - replace bedList2, bits2 with baseMask stuff. */
/* Load up intersecting bedList2 (to intersect with) */
int chromSize = hChromSize(database, region->chrom);
boolean isBpWise = (sameString("and", op) || sameString("or", op));
Bits *bits2 = bitsForIntersectingTable(conn, region, chromSize, isBpWise);
/* Set up some other local vars. */
struct hTableInfo *hti1 = getHti(database, table1, conn);
struct bed *intersectedBedList = NULL;

/* Produce intersectedBedList. */
if (isBpWise)
    {
/* --- TODO MIKE - replace, bits1 with baseMask stuff. */
    /* Base-pair-wise operation: get bitmap  for primary table too */
    Bits *bits1 = bitAlloc(chromSize+8);
    boolean hasBlocks = hti1->hasBlocks;
    if (retFieldCount != NULL && (*retFieldCount < 12))
	hasBlocks = FALSE;
    bedOrBits(bits1, chromSize, bedList1, hasBlocks, 0);
    /* invert inputs if necessary */
    if (invTable)
	bitNot(bits1, chromSize);
    /* do the intersection/union */
    if (sameString("and", op))
	bitAnd(bits1, bits2, chromSize);
    else
	bitOr(bits1, bits2, chromSize);
    /* clip to region if necessary: */
    if (region->start > 0)
	bitClearRange(bits1, 0, region->start);
    if (region->end < chromSize)
	bitClearRange(bits1, region->end, (chromSize - region->end));
    /* translate back to bed */
    intersectedBedList = bitsToBed4List(bits1, chromSize,
    	region->chrom, 1, region->start, region->end, lm);
    if (retFieldCount != NULL)
	*retFieldCount = 4;
    bitFree(&bits1);
    }
else
    intersectedBedList = filterBedByOverlap(bedList1, hti1->hasBlocks, op,
					    moreThresh, lessThresh, bits2,
					    chromSize);
bitFree(&bits2);
return intersectedBedList;
}

struct bed *getRegionAsMergedBed(
	char *db, char *table, 	/* Database and table. */
	struct region *region,  /* Region to get data for. */
	char *filter, 		/* Filter to add to SQL where clause if any. */
	struct hash *idHash, 	/* Restrict to id's in this hash if non-NULL. */
	struct lm *lm,		/* Where to allocate memory. */
	int *retFieldCount)	/* Number of fields. */
/* Return a bed list of all items in the given range in subtrack-merged table.
 * Cleanup result via lmCleanup(&lm) rather than bedFreeList.  */
{
if (! anySubtrackMerge(db, table))
    return getRegionAsBed(db, table, region, filter, idHash, lm, retFieldCount);
else
    {
    struct hTableInfo *hti = getHtiOnDb(database, table);
    int chromSize = hChromSize(database, region->chrom);
    Bits *bits1 = NULL;
    Bits *bits2 = NULL;
    struct bed *bedMerged = NULL;
    struct trackDb *subtrack = NULL;
    char *primaryType = findTypeForTable(database,curTrack,table, ctLookupName);
    char *op = cartString(cart, hgtaSubtrackMergeOp);
    boolean isBpWise = (sameString(op, "and") || sameString(op, "or"));
    double moreThresh = cartDouble(cart, hgtaSubtrackMergeMoreThreshold);
    double lessThresh = cartDouble(cart, hgtaSubtrackMergeLessThreshold);
    boolean firstTime = TRUE;
    if (sameString(op, "cat"))
	{
	struct bed *bedList = getRegionAsBed(db, table, region, filter,
					     idHash, lm, retFieldCount);
	struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(curTrack->subtracks);
	struct slRef *tdbRef;
	for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
	    {
	    subtrack = tdbRef->val;
	    if (! sameString(curTable, subtrack->table) &&
		isSubtrackMerged(subtrack->table) &&
		sameString(subtrack->type, primaryType))
		{
		struct bed *bedList2 =
		    getRegionAsBed(db, subtrack->table, region, NULL,
				   idHash, lm, retFieldCount);
		bedList = slCat(bedList, bedList2);
		}
	    }
	slFreeList(&tdbRefList);
	return bedList;
	}
    bits1 = bitAlloc(chromSize+8);
    bits2 = bitAlloc(chromSize+8);
    /* If doing a base-pair-wise operation, then start with the primary
     * subtrack's ranges in bits1, and AND/OR all the selected subtracks'
     * ranges into bits1.  If doing a non-bp-wise intersection, then
     * start with all bits clear in bits1, and then OR selected subtracks'
     * ranges into bits1.  */
    if (isBpWise)
	{
	struct lm *lm2 = lmInit(64*1024);
	struct bed *bedList1 = getRegionAsBed(db, table, region, filter,
					      idHash, lm2, retFieldCount);
	bedOrBits(bits1, chromSize, bedList1, hti->hasBlocks, 0);
	lmCleanup(&lm2);
	}
    struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(curTrack->subtracks);
    struct slRef *tdbRef;
    for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
	{
	subtrack = tdbRef->val;
	if (! sameString(curTable, subtrack->table) &&
	    isSubtrackMerged(subtrack->table) &&
	    sameString(subtrack->type, primaryType))
	    {
	    struct hTableInfo *hti2 = getHtiOnDb(database, subtrack->table);
	    struct lm *lm2 = lmInit(64*1024);
	    struct bed *bedList2 =
		getRegionAsBed(db, subtrack->table, region, NULL, idHash,
			       lm2, NULL);
	    if (firstTime)
		firstTime = FALSE;
	    else
		bitClear(bits2, chromSize);
	    bedOrBits(bits2, chromSize, bedList2, hti2->hasBlocks, 0);
	    if (sameString(op, "and"))
		bitAnd(bits1, bits2, chromSize);
	    else
		bitOr(bits1, bits2, chromSize);
	    lmCleanup(&lm2);
	    }
	}
    slFreeList(&tdbRefList);
    if (isBpWise)
	{
	bedMerged = bitsToBed4List(bits1, chromSize, region->chrom, 1,
				   region->start, region->end, lm);
	if (retFieldCount != NULL)
	    *retFieldCount = 4;
	}
    else
	{
	struct bed *bedList1 = getRegionAsBed(db, table, region, filter,
					      idHash, lm, retFieldCount);
	bedMerged = filterBedByOverlap(bedList1, hti->hasBlocks, op,
				       moreThresh, lessThresh, bits1,
				       chromSize);
	}
    bitFree(&bits1);
    bitFree(&bits2);
    return bedMerged;
    }
}


static struct bed *getIntersectedBeds(struct sqlConnection *conn,
	char *table, struct region *region, struct lm *lm, int *retFieldCount)
/* Get list of beds in region that pass intersection
 * (and filtering) */
{
struct bed *bedList = getFilteredBeds(conn, table, region, lm, retFieldCount);
/*	wiggle tracks have already done the intersection if there was one */
if (!isWiggle(database, table) && anyIntersection())
    {
    struct bed *iBedList = intersectOnRegion(conn, region, table, bedList,
    	lm, retFieldCount);
    return iBedList;
    }
else
    return bedList;
}

struct bed *cookedBedList(struct sqlConnection *conn,
	char *table, struct region *region, struct lm *lm, int *retFieldCount)
/* Get data for track in region after all processing steps (filtering
 * intersecting etc.) in BED format.  The retFieldCount variable will be
 * updated if the cooking process takes us down to bed 4 (which happens)
 * with bitwise intersections. */
{
return getIntersectedBeds(conn, table, region, lm, retFieldCount);
}


struct bed *cookedBedsOnRegions(struct sqlConnection *conn,
	char *table, struct region *regionList, struct lm *lm, int *retFieldCount)
/* Get cooked beds on all regions. */
{
struct bed *bedList = NULL;
struct region *region;
for (region = regionList; region != NULL; region = region->next)
    {
    struct bed *rBedList = getIntersectedBeds(conn, table, region, lm, retFieldCount);
    bedList = slCat(bedList, rBedList);
    }
return bedList;
}

