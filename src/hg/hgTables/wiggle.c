/* wiggle - stuff to process wiggle tracks.
 * Much of this is lifted from hgText/hgWigText.c */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "localmem.h"
#include "portable.h"
#include "obscure.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "cart.h"
#include "web.h"
#include "bed.h"
#include "hdb.h"
#include "hui.h"
#include "hgColors.h"
#include "trackDb.h"
#include "customTrack.h"
#include "wiggle.h"
#include "hmmstats.h"
#include "correlate.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: wiggle.c,v 1.79 2010/06/03 18:54:00 kent Exp $";

extern char *maxOutMenu[];


/*	a common set of stuff is accumulating in the various
 *	routines that call getData.  For the moment make it a macro,
 *	perhaps later it can turn into a routine of its own.
 */
#define WIG_INIT \
if (isCustomTrack(table)) \
    { \
    ct = ctLookupName(table); \
    isCustom = TRUE; \
    if (! ct->wiggle) \
	errAbort("called to work on a custom track '%s' that isn't wiggle data ?", table); \
 \
    if (ct->dbTrack) \
	safef(splitTableOrFileName,ArraySize(splitTableOrFileName), "%s", \
		ct->dbTableName); \
    else \
	safef(splitTableOrFileName,ArraySize(splitTableOrFileName), "%s", \
		ct->wigFile); \
    hasConstraint = checkWigDataFilter("ct", table, &dataConstraint, &ll, &ul); \
    } \
else \
    hasConstraint = checkWigDataFilter(database, table, &dataConstraint, \
			&ll, &ul); \
\
wds = wiggleDataStreamNew(); \
\
if (anyIntersection()) \
    { \
    char *t2 = cartString(cart, hgtaIntersectTable); \
    if (table && t2 && differentWord(t2,table)) \
	table2 = t2; \
    } \
\
if (hasConstraint)\
    wds->setDataConstraint(wds, dataConstraint, ll, ul);

boolean checkWigDataFilter(char *db, char *table,
	char **constraint, double *ll, double *ul)
/*	check if filter exists, return its values, call with db="ct" for
 *	custom tracks	*/
{
char varPrefix[128];
struct hashEl *varList, *var;
char *pat = NULL;
char *cmp = NULL;
if (constraint != NULL)
    *constraint = NULL;	// Make sure return variable gets set to something at least.

if (isCustomTrack(table))
    db = "ct";

safef(varPrefix, sizeof(varPrefix), "%s%s.%s.", hgtaFilterVarPrefix, db, table);

varList = cartFindPrefix(cart, varPrefix);
if (varList == NULL)
    return FALSE;

/*	check varList, look for dataValue.pat and dataValue.cmp	*/

for (var = varList; var != NULL; var = var->next)
    {
    if (endsWith(var->name, ".pat"))
	{
	char *name;
	name = cloneString(var->name);
	tolowers(name);
	/*	make sure we are actually looking at datavalue	*/
	if (stringIn("datavalue", name) || stringIn("score", name))
	    {
	    pat = cloneString(var->val);
	    }
	freeMem(name);
	}
    if (endsWith(var->name, ".cmp"))
	{
	char *name;
	name = cloneString(var->name);
	tolowers(name);
	/*	make sure we are actually looking at datavalue	*/
	if (stringIn("datavalue", name) || stringIn("score", name))
	    {
	    cmp = cloneString(var->val);
	    tolowers(cmp);
	    if (stringIn("ignored", cmp))
		freez(&cmp);
	    }
	freeMem(name);
	}
    }

/*	Must get them both for this to work	*/
if (cmp && pat)
    {
    int wordCount = 0;
    char *words[2];
    char *dupe = cloneString(pat);

    wordCount = chopString(dupe, ", \t\n", words, ArraySize(words));
    switch (wordCount)
	{
	case 2: if (ul) *ul = sqlDouble(words[1]);
	case 1: if (ll) *ll = sqlDouble(words[0]);
		break;
	default:
	    warn("can not understand numbers input for dataValue filter");
	    errAbort(
	    "dataValue filter must be one or two numbers (two for 'in range')");
	}
    if (sameWord(cmp,"in range") && (wordCount != 2))
	errAbort("'in range' dataValue filter must have two numbers input\n");

    if (constraint)
	*constraint = cmp;

    return TRUE;
    }
else
    return FALSE;
}	/*	static boolean checkWigDataFilter()	*/


static void wigDataHeader(char *name, char *description, char *visibility,
	enum wigOutputType wigOutType)
/* Write out custom track header for this wiggle region. */
{
switch (wigOutType)
    {
    case wigOutBed:
	hPrintf("track ");
        break;
    default:
    case wigOutData:
	hPrintf("track type=wiggle_0");
        break;
    };

if (name != NULL)
    hPrintf(" name=\"%s\"", name);
if (description != NULL)
    hPrintf(" description=\"%s\"", description);
if (visibility != NULL)
    hPrintf(" visibility=%s", visibility);
hPrintf("\n");
}

static void addBedElement(struct bed **bedList, char *chrom,
	unsigned start, unsigned end, unsigned count, struct lm *lm)
{
struct bed *bed;
char name[128];

lmAllocVar(lm, bed);
bed->chrom = lmCloneString(lm, chrom);
bed->chromStart = start;
bed->chromEnd = end;
safef(name,ArraySize(name), "%s.%u", bed->chrom, count);
bed->name = lmCloneString(lm, name);
slAddHead(bedList, bed);
}

static struct bed *invertBedList(struct bed *bedList, struct lm *lm,
	char *chrom, unsigned chromStart, unsigned chromEnd, unsigned chromSize)
/*	bed list result is everything NOT in the given bedList	*/
{
unsigned elCount = 1;
unsigned start = 0;				/*	start == end	*/
unsigned end = 0;				/*	means no element yet */
unsigned lastEnd = 0;			/*	in the bedList	*/
struct bed *inverseBedList = NULL;		/*	new list	*/
struct bed *el;

slSort(&bedList, bedLineCmp);	/* make sure it is sorted */

for (el = bedList; el != NULL; el=el->next)
    {
    /*	past end of chrom, bad bed list	*/
    if (el->chromStart >= chromSize)
	{
	end = chromSize;
	break;
	}
    if (el->chromStart > end)
	end = el->chromStart + 1;
    if (end > start)			/* do we have an element */
	{
	addBedElement(&inverseBedList, chrom, start, end, elCount++, lm);
	start = el->chromEnd - 1;	/*	reset to no element yet */
	end = start;
	}
    else if (el->chromEnd > start)
	{
	start = el->chromEnd - 1;	/*	reset to no element yet */
	end = start;
	}
    lastEnd = max(lastEnd,el->chromEnd);
    }

/*	A last element ?	*/
if (lastEnd < chromSize)
    end = chromSize;

if (end > start)			/* potential last element	*/
    addBedElement(&inverseBedList, chrom, start, end, elCount++, lm);

slSort(&inverseBedList, bedLineCmp);	/* make sure it is sorted */

return inverseBedList;
}

static struct bed *bedTable2(struct sqlConnection *conn,
	struct region *region, char *table2)
/*	get a bed list, possibly complement, for table2	*/
{
/* This use of bedTable rather than a bitmap is not really working. The
 * rest of the table browser does intersection at the exon level, while
 * the wig code, which this is part of, does it at the gene level.  I
 * noticed it while working on the corresponding routines for bigWig,
 * which I'm building to work with bitmaps at the exon level.  I'm not
 * sure it's worth fixing this code since nobody has complained, and we're
 * probably going to be doing mostly bigWig rather than wig in the future.
 *    -JK */
boolean invTable2 = cartCgiUsualBoolean(cart, hgtaInvertTable2, FALSE);
char *op = cartString(cart, hgtaIntersectOp);
struct bed *bedList = NULL;
struct lm *lm1 = lmInit(64*1024);

/*	fetch table 2 as a bed list	*/
bedList = getFilteredBeds(conn, table2, region, lm1, NULL);

/*	If table 2 bed list needs to be complemented (!table2), then do so */
if (invTable2 || sameString("none", op))
    {
    unsigned chromStart = 0;		/*	start == end == 0	*/
    unsigned chromEnd = 0;		/*	means do full chrom	*/
    unsigned chromSize = hChromSize(database, region->chrom);
    struct lm *lm2 = lmInit(64*1024);
    struct bed *inverseBedList = NULL;		/*	new list	*/

    if ((region->start != 0) || (region->end != 0))
	{
	chromStart = region->start;
	chromEnd = region->end;
	}

    if ((struct bed *)NULL == bedList)
	{
	if (0 == region->end)
	    chromEnd = chromSize;
	addBedElement(&inverseBedList, region->chrom, chromStart, chromEnd,
		1, lm2);
	}
    else
	inverseBedList=invertBedList(bedList, lm2, region->chrom, chromStart,
	    chromEnd, chromSize);

    lmCleanup(&lm1);			/*	== bedFreeList(&bedList) */

    return inverseBedList;
    }
else
    return bedList;
}

static unsigned long long getWigglePossibleIntersection(
    struct wiggleDataStream *wds, struct region *region, char *db,
	char *table2, struct bed **intersectBedList,
	    char splitTableOrFileName[256], int operations)
{
unsigned long long valuesMatched = 0;

/*	Intersection is either type "any" or "none"
 *	The "none" case is already taken care of during the loading of
 *	table 2 since it was then inverted at that time so it is already
 *	"none" of itself.
 */


/* If table2 is NULL, it means that the WIG_INIT macro recognized that we
 * are working on table2 now, so we should not use intersectBedList
 * (in fact we may be trying to compute it here). */
if ((table2 != NULL) && anyIntersection())
    {
    if (*intersectBedList)
	{
	valuesMatched = wds->getDataViaBed(wds, db, splitTableOrFileName,
	    operations, intersectBedList);
	}
    }
else
    {
    valuesMatched = wds->getData(wds, db, splitTableOrFileName, operations);
    }

return valuesMatched;
}

static void intersectDataVector(char *table, struct dataVector *dataVector1,
			struct region *region, struct sqlConnection *conn)
/* Perform intersection (if specified) on dataVector. */
{
/* If table is type wig (not bedGraph), then intersection has already been
 * performed on each input (other selected subtracks must be the same type
 * as table).
 * Otherwise, handle intersection here. */
if (anyIntersection() && !isWiggle(database, table) && !isBigWigTable(table))
    {
    char *track2 = cartString(cart, hgtaIntersectTrack);
    char *table2 = cartString(cart, hgtaIntersectTable);
    if (table2 && differentWord(table2, table))
	{
	struct trackDb *tdb2 = findTrack(track2, fullTrackList);
	struct trackTable *tt2 = trackTableNew(tdb2, table2, conn);
	struct dataVector *dataVector2 = dataVectorFetchOneRegion(tt2, region,
								  conn);
	char *op = cartString(cart, hgtaIntersectOp);
	boolean dv2IsWiggle = (isWiggle(database, table2) || isBigWigTable(table2) ||
			       isBedGraph(table2));
	dataVectorIntersect(dataVector1, dataVector2,
			    dv2IsWiggle, sameString(op, "none"));
	dataVectorFree(&dataVector2);
	}
    }
}

int wigPrintDataVectorOut(struct dataVector *dataVectorList,
			      enum wigOutputType wigOutType, int maxOut,
			      char *description)
/* Print out bed or data points from list of dataVectors. */
{
int count = 0;
switch (wigOutType)
    {
    case wigOutBed:
	count = dataVectorWriteBed(dataVectorList, "stdout", maxOut,
				   description);
	break;
    case wigDataNoPrint:
	break;
    case wigOutData:
    default:
	count = dataVectorWriteWigAscii(dataVectorList, "stdout", maxOut,
					description);
	break;
    };
return count;
}

struct dataVector *mergedWigDataVector(char *table,
	struct sqlConnection *conn, struct region *region)
/* Perform the specified subtrack merge wiggle-operation on table and
 * all other selected subtracks and intersect if necessary. */
{
struct trackDb *tdb1 = hTrackDbForTrack(database, table);
struct trackTable *tt1 = trackTableNew(tdb1, table, conn);
struct dataVector *dataVector1 = dataVectorFetchOneRegion(tt1, region, conn);
struct trackDb *cTdb = hCompositeTrackDbForSubtrack(database, tdb1);
int numSubtracks = 1;
char *op = cartString(cart, hgtaSubtrackMergeWigOp);
boolean requireAll = cartBoolean(cart, hgtaSubtrackMergeRequireAll);
boolean useMinScore = cartBoolean(cart, hgtaSubtrackMergeUseMinScore);
float minScore = atof(cartString(cart, hgtaSubtrackMergeMinScore));

if (cTdb == NULL)
    errAbort("mergedWigDataVector: could not find parent/composite trackDb "
	     "entry for subtrack %s", table);

if (dataVector1 == NULL)
    {
    return NULL;
    }

struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(cTdb->subtracks);
struct slRef *tdbRef;
for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
    {
    struct trackDb *sTdb = tdbRef->val;
    if (isSubtrackMerged(sTdb->table) &&
	! sameString(tdb1->table, sTdb->table) &&
	hSameTrackDbType(tdb1->type, sTdb->type))
	{
	struct trackTable *tt2 = trackTableNew(sTdb, sTdb->table, conn);
	struct dataVector *dataVector2 = dataVectorFetchOneRegion(tt2, region,
								  conn);
	numSubtracks++;
	if (dataVector2 == NULL)
	    {
	    if (requireAll)
		{
		freeDataVector(&dataVector1);
		return NULL;
		}
	    continue;
	    }
	if (sameString(op, "average") || sameString(op, "sum"))
	    dataVectorSum(dataVector1, dataVector2, requireAll);
	else if (sameString(op, "product"))
	    dataVectorProduct(dataVector1, dataVector2, requireAll);
	else if (sameString(op, "min"))
	    dataVectorMin(dataVector1, dataVector2, requireAll);
	else if (sameString(op, "max"))
	    dataVectorMax(dataVector1, dataVector2, requireAll);
	else
	    errAbort("mergedWigOutRegion: unknown WigOp %s", op);
	dataVectorFree(&dataVector2);
	}
    }
slFreeList(&tdbRefList);
if (sameString(op, "average"))
    dataVectorNormalize(dataVector1, numSubtracks);
if (useMinScore)
    dataVectorFilterMin(dataVector1, minScore);

intersectDataVector(table, dataVector1, region, conn);

return dataVector1;
}

static int mergedWigOutRegion(char *table, struct sqlConnection *conn,
			      struct region *region, int maxOut,
			      enum wigOutputType wigOutType)
/* Perform the specified subtrack merge wiggle-operation on table and
 * all other selected subtracks, intersect if necessary, and print out. */
{
struct dataVector *dv = mergedWigDataVector(table, conn, region);
int resultCount =
    wigPrintDataVectorOut(dv, wigOutType, maxOut, describeSubtrackMerge("#\t"));
dataVectorFree(&dv);
return resultCount;
}


static int bedGraphOutRegion(char *table, struct sqlConnection *conn,
			     struct region *region, int maxOut,
			     enum wigOutputType wigOutType)
/* Read in bedGraph as dataVector (filtering is handled there),
 * intersect if necessary, and print it out. */
{
struct dataVector *dv =bedGraphDataVector(table, conn, region);
int resultCount = wigPrintDataVectorOut(dv, wigOutType, maxOut, NULL);
dataVectorFree(&dv);
return resultCount;
}

static int wigOutRegion(char *table, struct sqlConnection *conn,
	struct region *region, int maxOut, enum wigOutputType wigOutType,
	struct wigAsciiData **data, int spanConstraint)
/* Write out wig data in region.  Write up to maxOut elements.
 * Returns number of elements written. */
{
int linesOut = 0;
char splitTableOrFileName[256];
struct customTrack *ct = NULL;
boolean isCustom = FALSE;
boolean hasConstraint = FALSE;
struct wiggleDataStream *wds = NULL;
unsigned long long valuesMatched = 0;
int operations = wigFetchAscii;
char *dataConstraint;
double ll = 0.0;
double ul = 0.0;
char *table2 = NULL;
struct bed *intersectBedList = NULL;

switch (wigOutType)
    {
    case wigOutBed:
	operations = wigFetchBed;
	break;
    default:
    case wigDataNoPrint:
    case wigOutData:
	operations = wigFetchAscii;
	break;
    };

WIG_INIT;  /* ct, isCustom, hasConstraint, wds and table2 are set here */

if (hasConstraint)
    freeMem(dataConstraint);	/* been cloned into wds */

wds->setMaxOutput(wds, maxOut);
wds->setChromConstraint(wds, region->chrom);
wds->setPositionConstraint(wds, region->start, region->end);

if (table2)
    intersectBedList = bedTable2(conn, region, table2);

if (isCustom)
    {
    if (ct->dbTrack)
	{
	if (spanConstraint)
	    wds->setSpanConstraint(wds,spanConstraint);
	else
	    {
	    struct sqlConnection *trashConn = hAllocConn(CUSTOM_TRASH);
	    struct trackDb *tdb = findTdbForTable(database, curTrack, table, ctLookupName);
	    unsigned span = minSpan(trashConn, splitTableOrFileName,
		region->chrom, region->start, region->end, cart, tdb);
	    wds->setSpanConstraint(wds, span);
	    hFreeConn(&trashConn);
	    }
	valuesMatched = getWigglePossibleIntersection(wds, region,
	    CUSTOM_TRASH, table2, &intersectBedList,
		splitTableOrFileName, operations);
	}
    else
	valuesMatched = getWigglePossibleIntersection(wds, region, NULL, table2,
	    &intersectBedList, splitTableOrFileName, operations);
    }
else
    {
    boolean hasBin = FALSE;

    if (hFindSplitTable(database, region->chrom, table, splitTableOrFileName, &hasBin))
	{
	/* XXX TBD, watch for a span limit coming in as an SQL filter */
	if (intersectBedList)
	    {
	    struct trackDb *tdb = findTdbForTable(database, curTrack, table, ctLookupName);
	    unsigned span;
	    span = minSpan(conn, splitTableOrFileName, region->chrom,
		region->start, region->end, cart, tdb);
	    wds->setSpanConstraint(wds, span);
	    }
	else if (spanConstraint)
	    wds->setSpanConstraint(wds,spanConstraint);

	valuesMatched = getWigglePossibleIntersection(wds, region, database,
	    table2, &intersectBedList, splitTableOrFileName, operations);
	}
    }

switch (wigOutType)
    {
    case wigDataNoPrint:
	if (data)
	    {
	    if (*data != NULL)	/* no exercise of this function yet	*/
		{	/*	data not null, add to existing list	*/
		struct wigAsciiData *asciiData;
		struct wigAsciiData *next;
		for (asciiData = *data; asciiData; asciiData = next)
		    {
		    next = asciiData->next;
		    slAddHead(&wds->ascii, asciiData);
		    }
		}
	    wds->sortResults(wds);
	    *data = wds->ascii;	/* moving the list to *data */
	    wds->ascii = NULL;	/* gone as far as wds is concerned */
	    }
	    linesOut = valuesMatched;
	break;
    case wigOutBed:
	linesOut = wds->bedOut(wds, "stdout", TRUE);/* TRUE == sort output */
	break;
    default:
    case wigOutData:
	linesOut = wds->asciiOut(wds, database, "stdout", TRUE, FALSE);
	break;		/* TRUE == sort output, FALSE == not raw data out */
    };

wiggleDataStreamFree(&wds);

return linesOut;
}	/*	static int wigOutRegion()	*/

int bigFileMaxOutput()
/*	return maxOut value (cart variable defined on curTable)	*/
{
char *maxOutputStr = NULL;
char *name;
int maxOut;
char *maxOutput = NULL;

if (isCustomTrack(curTable))
    name = filterFieldVarName("ct", curTable, "_", filterMaxOutputVar);
else
    name = filterFieldVarName(database, curTable, "_", filterMaxOutputVar);

maxOutputStr = cartOptionalString(cart, name);
/*	Don't modify(stripChar) the values sitting in the cart hash	*/
if (NULL == maxOutputStr)
    maxOutput = cloneString(maxOutMenu[0]);
else
    maxOutput = cloneString(maxOutputStr);

stripChar(maxOutput, ',');
maxOut = sqlUnsigned(maxOutput);
freeMem(maxOutput);

return maxOut;
}

static void doOutWig(struct trackDb *track, char *table, struct sqlConnection *conn,
	enum wigOutputType wigOutType)
{
struct region *regionList = getRegions(), *region;
int maxOut = 0, outCount, curOut = 0;
char *shortLabel = table, *longLabel = table;

if (track == NULL)
    errAbort("Sorry, can't find necessary track information for %s.  "
	     "If you reached this page by selecting \"All tables\" as the "
	     "group, please go back and select the same table via a regular "
	     "track group if possible.",
	     table);

maxOut = bigFileMaxOutput();

if (cartUsualBoolean(cart, hgtaDoGreatOutput, FALSE))
    fputs("#", stdout);
else
    textOpen();

if (track != NULL)
    {
    if (!sameString(track->table, table) && track->subtracks != NULL)
	{
	struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(track->subtracks);
	struct slRef *tdbRef;
	for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
	    {
	    struct trackDb *tdb = tdbRef->val;
	    if (sameString(tdb->table, table))
		{
		track = tdb;
		break;
		}
	    }
	slFreeList(&tdbRefList);
	}
    shortLabel = track->shortLabel;
    longLabel = track->longLabel;
    }
wigDataHeader(shortLabel, longLabel, NULL, wigOutType);

for (region = regionList; region != NULL; region = region->next)
    {
    int curMaxOut = maxOut - curOut;
    if (anySubtrackMerge(database, table))
	outCount = mergedWigOutRegion(table, conn, region, curMaxOut,
				      wigOutType);
    else if (startsWithWord("bedGraph", track->type))
	outCount = bedGraphOutRegion(table, conn, region, curMaxOut,
				     wigOutType);
    else if (startsWithWord("bigWig", track->type))
        outCount = bigWigOutRegion(table, conn, region, curMaxOut, wigOutType);
    else
	outCount = wigOutRegion(table, conn, region, curMaxOut,
				wigOutType, NULL, 0);
    curOut += outCount;
    if (curOut >= maxOut)
        break;
    }
if (curOut >= maxOut)
    warn("Reached output limit of %d data values, please make region smaller,\n\tor set a higher output line limit with the filter settings.", curOut);
}


/***********   PUBLIC ROUTINES  *********************************/

struct dataVector *bedGraphDataVector(char *table,
	struct sqlConnection *conn, struct region *region)
/* Read in bedGraph as dataVector and return it.  Filtering, subtrack merge
 * and intersection are handled. */
{
struct dataVector *dv = NULL;

if (anySubtrackMerge(database, table))
    dv = mergedWigDataVector(table, conn, region);
else
    {
    struct trackDb *tdb;
    if (isCustomTrack(table))
        {
        struct customTrack *ct = ctLookupName(table);
        tdb = ct->tdb;
        conn = hAllocConn(CUSTOM_TRASH);
        }
    else
        {
        tdb = hTrackDbForTrack(database, table);
        }
    struct trackTable *tt1 = trackTableNew(tdb, table, conn);
    dv = dataVectorFetchOneRegion(tt1, region, conn);
    intersectDataVector(table, dv, region, conn);
    if (isCustomTrack(table))
        hFreeConn(&conn);
    }
return dv;
}


struct dataVector *wiggleDataVector(struct trackDb *tdb, char *table,
	struct sqlConnection *conn, struct region *region)
/* Read in wiggle as dataVector and return it.  Filtering, subtrack merge
 * and intersection are handled. */
{
struct dataVector *dv = NULL;

if (anySubtrackMerge(database, table))
    dv = mergedWigDataVector(table, conn, region);
else
    {
    struct trackTable *tt1 = trackTableNew(tdb, table, conn);
    dv = dataVectorFetchOneRegion(tt1, region, conn);
    }
return dv;
}

struct bed *getBedGraphAsBed(struct sqlConnection *conn, char *table,
			     struct region *region)
/* Extract a bedList in region from the given bedGraph table -- filtering and
 * intersection are handled inside of this function. */
{
struct dataVector *dv = NULL;

dv = bedGraphDataVector(table, conn, region);

return dataVectorToBedList(dv);
}

void wiggleMinMax(struct trackDb *tdb, double *min, double *max)
{
/*	obtain wiggle data limits from trackDb or cart or settings */
if ((tdb != NULL) && (tdb->type != NULL))
    {
    double tDbMin, tDbMax;
    char *typeLine = cloneString(tdb->type);
    char *words[8];
    int wordCount;
    wordCount = chopLine(typeLine, words);
    wigFetchMinMaxY(tdb, min, max, &tDbMin, &tDbMax, wordCount, words);
    if (tDbMin < *min)
	*min = tDbMin;
    if (tDbMax > *max)
	*max = tDbMax;
    freeMem(typeLine);
    }
}

boolean isWiggle(char *db, char *table)
/* Return TRUE if db.table is a wiggle. */
{
boolean typeWiggle = FALSE;

if (db != NULL && table != NULL)
    {
    if (isCustomTrack(table))
	{
	struct customTrack *ct = ctLookupName(table);
	if (ct != NULL && ct->wiggle)
	    typeWiggle = TRUE;
	}
    else
	{
	struct hTableInfo *hti = maybeGetHtiOnDb(db, table);
	typeWiggle = (hti != NULL && HTI_IS_WIGGLE);
	}
    }
return(typeWiggle);
}

boolean isBedGraph(char *table)
/* Return TRUE if table is specified as a bedGraph in the current database's
 * trackDb. */
{
return trackIsType(database, table, NULL, "bedGraph", ctLookupName);
}

struct bed *getWiggleAsBed(
    char *db, char *table, 	/* Database and table. */
    struct region *region,	/* Region to get data for. */
    char *filter, 		/* Filter to add to SQL where clause if any. */
    struct hash *idHash, 	/* Restrict to id's in this hash if non-NULL. */
    struct lm *lm,		/* Where to allocate memory. */
    struct sqlConnection *conn)	/* SQL connection to work with */
/* Return a bed list of all items in the given range in table.
 * Cleanup result via lmCleanup(&lm) rather than bedFreeList.  */
/* filter, idHash and lm are currently unused, perhaps future use	*/
{
struct bed *bedList=NULL;
char splitTableOrFileName[256];
struct customTrack *ct = NULL;
boolean isCustom = FALSE;
boolean hasConstraint = FALSE;
struct wiggleDataStream *wds = NULL;
unsigned long long valuesMatched = 0;
int operations = wigFetchBed;
char *dataConstraint;
double ll = 0.0;
double ul = 0.0;
char *table2 = NULL;
struct bed *intersectBedList = NULL;
int maxOut;

WIG_INIT;  /* ct, isCustom, hasConstraint, wds and table2 are set here */

if (hasConstraint)
    freeMem(dataConstraint);	/* been cloned into wds */

maxOut = bigFileMaxOutput();

wds->setMaxOutput(wds, maxOut);

wds->setChromConstraint(wds, region->chrom);
wds->setPositionConstraint(wds, region->start, region->end);

if (table2)
    intersectBedList = bedTable2(conn, region, table2);

if (isCustom)
    {
    if (ct->dbTrack)
	{
	unsigned span = 0;
	struct sqlConnection *trashConn = hAllocConn(CUSTOM_TRASH);
	struct trackDb *tdb = findTdbForTable(database, curTrack, table, ctLookupName);
	valuesMatched = getWigglePossibleIntersection(wds, region,
	    CUSTOM_TRASH, table2, &intersectBedList,
		splitTableOrFileName, operations);
	span = minSpan(trashConn, splitTableOrFileName, region->chrom,
	    region->start, region->end, cart, tdb);
	wds->setSpanConstraint(wds, span);
	hFreeConn(&trashConn);
	}
    else
	valuesMatched = getWigglePossibleIntersection(wds, region, NULL, table2,
	    &intersectBedList, splitTableOrFileName, operations);
    }
else
    {
    boolean hasBin;

    if (conn == NULL)
	errAbort( "getWiggleAsBed: NULL conn given for database table");

    if (hFindSplitTable(database, region->chrom, table, splitTableOrFileName, &hasBin))
	{
	struct trackDb *tdb = findTdbForTable(database, curTrack, table, ctLookupName);
	unsigned span = 0;

	/* XXX TBD, watch for a span limit coming in as an SQL filter */
	span = minSpan(conn, splitTableOrFileName, region->chrom,
	    region->start, region->end, cart, tdb);
	wds->setSpanConstraint(wds, span);

	valuesMatched = getWigglePossibleIntersection(wds, region, database,
	    table2, &intersectBedList, splitTableOrFileName, operations);
	}
    }

if (valuesMatched > 0)
    {
    struct bed *bed;

    wds->sortResults(wds);
    for (bed = wds->bed; bed != NULL; bed = bed->next)
	{
	struct bed *copy = lmCloneBed(bed, lm);
	slAddHead(&bedList, copy);
	}
    slReverse(&bedList);
    }

wiggleDataStreamFree(&wds);

return bedList;
}	/*	struct bed *getWiggleAsBed()	*/

struct wigAsciiData *getWiggleAsData(struct sqlConnection *conn, char *table,
	struct region *region)
/*	return the wigAsciiData list	*/
{
int maxOut = 0;
struct wigAsciiData *data = NULL;
int outCount;

maxOut = bigFileMaxOutput();
outCount = wigOutRegion(table, conn, region, maxOut, wigDataNoPrint, &data, 0);

return data;
}

struct wigAsciiData *getWiggleData(struct sqlConnection *conn, char *table,
	struct region *region, int maxOut, int spanConstraint)
/*	like getWiggleAsData above, but with specific spanConstraint and
 *	a different data limit count, return the wigAsciiData list	*/
{
struct wigAsciiData *data = NULL;
int outCount;

outCount = wigOutRegion(table, conn, region, maxOut, wigDataNoPrint, &data,
	spanConstraint);

return data;
}

void doOutWigData(struct trackDb *track, char *table, struct sqlConnection *conn)
/* Return wiggle data in variableStep format. */
{
doOutWig(track, table, conn, wigOutData);
}

void doOutWigBed(struct trackDb *track, char *table, struct sqlConnection *conn)
/* Return wiggle data in bed format. */
{
doOutWig(track, table, conn, wigOutBed);
}

void doSummaryStatsWiggle(struct sqlConnection *conn)
/* Put up page showing summary stats for wiggle track. */
{
struct trackDb *track = curTrack;
char *table = curTable;
struct region *region, *regionList = getRegions();
char *regionName = getRegionName();
long long regionSize = 0;
long long gapTotal = 0;
long startTime = 0, wigFetchTime = 0;
char splitTableOrFileName[256];
struct customTrack *ct = NULL;
boolean isCustom = FALSE;
struct wiggleDataStream *wds = NULL;
unsigned long long valuesMatched = 0;
int regionCount = 0;
int regionsDone = 0;
unsigned span = 0;
char *dataConstraint;
double ll = 0.0;
double ul = 0.0;
boolean hasConstraint = FALSE;
char *table2 = NULL;
boolean fullGenome = FALSE;
boolean statsHeaderDone = FALSE;
boolean gotSome = FALSE;
char *shortLabel = table;
long long statsItemCount = 0;	/*	global accumulators for overall */
int statsSpan = 0;		/*	stats summary on a multiple region */
double statsSumData = 0.0;	/*	output */
double statsSumSquares = 0.0;		/*	"  "	*/
double lowerLimit = INFINITY;		/*	"  "	*/
double upperLimit = -1.0 * INFINITY;	/*	"  "	*/

startTime = clock1000();
if (track != NULL)
     shortLabel = track->shortLabel;

/*	Count the regions, when only one, we can do more stats */
for (region = regionList; region != NULL; region = region->next)
    ++regionCount;

htmlOpen("%s (%s) Wiggle Summary Statistics", shortLabel, table);

if (anySubtrackMerge(database, curTable))
    hPrintf("<P><EM><B>Note:</B> subtrack merge is currently ignored on this "
	    "page (not implemented yet).  Statistics shown here are only for "
	    "the primary table %s (%s).</EM>", shortLabel, table);

fullGenome = fullGenomeRegion();

WIG_INIT;  /* ct, isCustom, hasConstraint, wds and table2 are set here */

for (region = regionList; region != NULL; region = region->next)
    {
    struct bed *intersectBedList = NULL;
    boolean hasBin;
    int operations;

    ++regionsDone;

    if (table2)
	intersectBedList = bedTable2(conn, region, table2);

    operations = wigFetchStats;
#if defined(NOT)
    /*	can't do the histogram now, that operation times out	*/
    if (1 == regionCount)
	operations |= wigFetchAscii;
#endif

    wds->setChromConstraint(wds, region->chrom);

    if (fullGenome)
	wds->setPositionConstraint(wds, 0, 0);
    else
	wds->setPositionConstraint(wds, region->start, region->end);

    if (hasConstraint)
	wds->setDataConstraint(wds, dataConstraint, ll, ul);

    /* depending on what is coming in on regionList, we may need to be
     * smart about how often we call getData for these custom tracks
     * since that is potentially a large file read each time.
     */
    if (isCustom)
	{
	if (ct->dbTrack)
	    {
	    struct sqlConnection *trashConn = hAllocConn(CUSTOM_TRASH);
	    struct trackDb *tdb = findTdbForTable(database, curTrack, table, ctLookupName);
	    span = minSpan(trashConn, splitTableOrFileName, region->chrom,
		region->start, region->end, cart, tdb);
	    wds->setSpanConstraint(wds, span);
	    valuesMatched = getWigglePossibleIntersection(wds, region,
		CUSTOM_TRASH, table2, &intersectBedList,
		    splitTableOrFileName, operations);
	    hFreeConn(&trashConn);
	    }
	else
	    {
	    valuesMatched = getWigglePossibleIntersection(wds, region, NULL,
		table2, &intersectBedList, splitTableOrFileName, operations);

	/*  XXX We need to properly get the smallest span for custom tracks */
	    /*	This is not necessarily the correct answer here	*/
	    if (wds->stats)
		span = wds->stats->span;
	    else
		span = 1;
	    }
	}
    else
	{
	if (hFindSplitTable(database, region->chrom, table, splitTableOrFileName, &hasBin))
	    {
	    span = minSpan(conn, splitTableOrFileName, region->chrom,
		region->start, region->end, cart, track);
	    wds->setSpanConstraint(wds, span);
	    valuesMatched = getWigglePossibleIntersection(wds, region,
		database, table2, &intersectBedList, splitTableOrFileName,
		    operations);
	    if (intersectBedList)
		span = 1;
	    }
	}
    /*	when doing multiple regions, we need to print out each result as
     *	it happens to keep the connection open to the browser and
     *	prevent any timeout since this could take a while.
     *	(worst case test is quality track on panTro1)
     */
    if (wds->stats)
	statsItemCount += wds->stats->count;
    if (wds->stats && (regionCount > 1) && (valuesMatched > 0))
	{
	double sumData = wds->stats->mean * wds->stats->count;
	double sumSquares;

	if (wds->stats->count > 1)
	    sumSquares = (wds->stats->variance * (wds->stats->count - 1)) +
		((sumData * sumData)/wds->stats->count);
	else
	    sumSquares = sumData * sumData;

	/*	global accumulators for overall summary	*/
	statsSpan = wds->stats->span;
	statsSumData += sumData;
	statsSumSquares += sumSquares;
	if (wds->stats->lowerLimit < lowerLimit)
	    lowerLimit = wds->stats->lowerLimit;
	if ((wds->stats->lowerLimit + wds->stats->dataRange) > upperLimit)
	    upperLimit = wds->stats->lowerLimit + wds->stats->dataRange;

	if (statsHeaderDone)
	    wds->statsOut(wds, database, "stdout", TRUE, TRUE, FALSE, TRUE);
	else
	    {
	    wds->statsOut(wds, database, "stdout", TRUE, TRUE, TRUE, TRUE);
	    statsHeaderDone = TRUE;
	    }
	wds->freeStats(wds);
	gotSome = TRUE;
	}
    if ((regionCount > MAX_REGION_DISPLAY) &&
		(regionsDone >= MAX_REGION_DISPLAY))
	{
	hPrintf("<TR><TH ALIGN=CENTER COLSPAN=12> Can not display more "
	    "than %d regions, <BR> would take too much time </TH></TR>\n",
		MAX_REGION_DISPLAY);
	break;	/*	exit this for loop	*/
	}
    }	/*for (region = regionList; region != NULL; region = region->next) */

if (hasConstraint)
    freeMem(dataConstraint);	/* been cloned into wds */

if (1 == regionCount)
    {
    statsPreamble(wds, regionList->chrom, regionList->start, regionList->end,
	span, valuesMatched, table2);
    /* 3 X TRUE = sort results, html table output, with header,
     *	the FALSE means close the table after printing, no more rows to
     *	come.  The case in the if() statement was already taken care of
     *	in the statsPreamble() printout.  No need to do that again.
     */

    if ( ! ((valuesMatched == 0) && table2) )
	wds->statsOut(wds, database, "stdout", TRUE, TRUE, TRUE, FALSE);
    regionSize = basesInRegion(regionList,0);
    gapTotal = gapsInRegion(conn, regionList,0);
    }
else
    {	/* this is a bit of a kludge here since these printouts are done in the
	 *	library source wigDataStream.c statsOut() function and
	 *	this is a clean up of that.  That function should be
	 *	pulled out of there and made independent and more
	 *	versatile.
	 */
    long long realSize;
    double variance;
    double stddev;

    /*	Too expensive to lookup the numbers for thousands of regions */
    regionSize = basesInRegion(regionList,MAX_REGION_DISPLAY);
    gapTotal = gapsInRegion(conn, regionList,MAX_REGION_DISPLAY);
    realSize = regionSize - gapTotal;

    /*	close the table which was left open in the loop above	*/
    if (!gotSome)
	hPrintf("<TR><TH ALIGN=CENTER COLSPAN=12> No data found matching this request </TH></TR>\n");

    hPrintf("<TR><TH ALIGN=LEFT> SUMMARY: </TH>\n");
    hPrintf("\t<TD> &nbsp; </TD>\n");	/*	chromStart	*/
    hPrintf("\t<TD> &nbsp; </TD>\n");	/*	chromEnd	*/
    hPrintf("\t<TD ALIGN=RIGHT> ");
    printLongWithCommas(stdout, statsItemCount);
    hPrintf(" </TD>\n" );
    hPrintf("\t<TD ALIGN=RIGHT> %d </TD>\n", statsSpan);
    hPrintf("\t<TD ALIGN=RIGHT> ");
    printLongWithCommas(stdout, statsItemCount*statsSpan);
    hPrintf("&nbsp;(%.2f%%) </TD>\n",
	100.0*(double)(statsItemCount*statsSpan)/(double)realSize);
    hPrintf("\t<TD ALIGN=RIGHT> %g </TD>\n", lowerLimit);
    hPrintf("\t<TD ALIGN=RIGHT> %g </TD>\n", upperLimit);
    hPrintf("\t<TD ALIGN=RIGHT> %g </TD>\n", upperLimit - lowerLimit);
    if (statsItemCount > 0)
	hPrintf("\t<TD ALIGN=RIGHT> %g </TD>\n", statsSumData/statsItemCount);
    else
	hPrintf("\t<TD ALIGN=RIGHT> 0.0 </TD>\n");
    stddev = 0.0;
    variance = 0.0;
    if (statsItemCount > 1)
	{
	variance = (statsSumSquares -
	    ((statsSumData * statsSumData)/(double) statsItemCount)) /
		(double) (statsItemCount - 1);
	if (variance > 0.0)
	    stddev = sqrt(variance);
	}
    hPrintf("\t<TD ALIGN=RIGHT> %g </TD>\n", variance);
    hPrintf("\t<TD ALIGN=RIGHT> %g </TD>\n", stddev);
    hPrintf("</TR>\n");
    wigStatsTableHeading(stdout, TRUE);
    hPrintf("</TABLE></TD></TR></TABLE></P>\n");
    }


#if defined(NOT)
/*	can't do the histogram now, that operation times out	*/
/*	Single region, we can do the histogram	*/
if ((valuesMatched > 1) && (1 == regionCount))
    {
    float *valuesArray = NULL;
    size_t valueCount = 0;
    struct histoResult *histoGramResult;

    /*	convert the ascii data listings to one giant float array 	*/
    valuesArray = wds->asciiToDataArray(wds, valuesMatched, &valueCount);

    /*	histoGram() may return NULL if it doesn't work	*/

    histoGramResult = histoGram(valuesArray, valueCount,
	    NAN, (unsigned) 0, NAN, (float) wds->stats->lowerLimit,
		(float) (wds->stats->lowerLimit + wds->stats->dataRange),
		(struct histoResult *)NULL);

    printHistoGram(histoGramResult, TRUE);	/* TRUE == html output */

    freeHistoGram(&histoGramResult);
    wds->freeAscii(wds);
    wds->freeArray(wds);
    }
#endif

wds->freeStats(wds);
wiggleDataStreamFree(&wds);

wigFetchTime = clock1000() - startTime;
webNewSection("Region and Timing Statistics");
hTableStart();
stringStatRow("region", regionName);
numberStatRow("bases in region", regionSize);
numberStatRow("bases in gaps", gapTotal);
floatStatRow("load and calc time", 0.001*wigFetchTime);
wigFilterStatRow(conn);
stringStatRow("intersection", cartUsualString(cart, hgtaIntersectTable, "off"));
hTableEnd();
htmlClose();
}	/*	void doSummaryStatsWiggle(struct sqlConnection *conn)	*/

void wigShowFilter(struct sqlConnection *conn)
/* print out wiggle data value filter */
{
double ll, ul;
char *constraint;

if (checkWigDataFilter(database, curTable, &constraint, &ll, &ul))
    {
    if (constraint && sameWord(constraint, "in range"))
	{
	hPrintf("&nbsp;&nbsp;data value %s [%g : %g)\n", constraint, ll, ul);
	}
    else
	hPrintf("&nbsp;&nbsp;data value %s %g\n", constraint, ll);
    freeMem(constraint);
    }
}
