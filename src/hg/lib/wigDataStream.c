/*	wigDataStream - an object to access wiggle data values from
 *	either a DB access or from a .wig text file (==custom track)
 */
#include "common.h"
#include "memalloc.h"
#include "wiggle.h"

static char const rcsid[] = "$Id: wigDataStream.c,v 1.18 2004/08/17 19:36:14 hiram Exp $";

/*	PRIVATE	METHODS	************************************************/
static void addConstraint(struct wiggleDataStream *wDS, char *left, char *right)
{
struct dyString *constrain = dyStringNew(256);
if (wDS->sqlConstraint)
    dyStringPrintf(constrain, "%s AND ", wDS->sqlConstraint);

dyStringPrintf(constrain, "%s \"%s\"", left, right);

freeMem(wDS->sqlConstraint);
wDS->sqlConstraint = cloneString(constrain->string);
dyStringFree(&constrain);
}

static boolean nextRow(struct wiggleDataStream *wDS, char *row[], int maxRow)
/*	read next wig row from sql query or lineFile
 *	FALSE return on no more data	*/
{
int numCols;

if (wDS->isFile)
    {
    numCols = lineFileChopNextTab(wDS->lf, row, maxRow);
    if (numCols != maxRow) return FALSE;
    verbose(VERBOSE_PER_VALUE_LEVEL, "#\tnumCols = %d, row[0]: %s, row[1]: %s, row[%d]: %s\n",
	numCols, row[0], row[1], maxRow-1, row[maxRow-1]);
    }
else
    {
    int i;
    char **sqlRow;
    sqlRow = sqlNextRow(wDS->sr);
    if (sqlRow == NULL)
	return FALSE;
    /*	skip the bin column sqlRow[0]	*/
    for (i=1; i <= maxRow; ++i)
	{
	row[i-1] = sqlRow[i];
	}
    }
return TRUE;
}

static void openWibFile(struct wiggleDataStream *wDS, char *file)
{
if (wDS->wibFile)
    {		/*	close and open only if different */
    if (differentString(wDS->wibFile,file))
	{
	if (wDS->wibFH > 0)
	    close(wDS->wibFH);
	freeMem(wDS->wibFile);
	wDS->wibFile = cloneString(file);
	wDS->wibFH = open(wDS->wibFile, O_RDONLY);
	if (wDS->wibFH == -1)
	    errAbort("openWibFile: failed to open %s", wDS->wibFile);
	}
    }
else
    {
    wDS->wibFile = cloneString(file);	/* first time */
    wDS->wibFH = open(wDS->wibFile, O_RDONLY);
    if (wDS->wibFH == -1)
	errAbort("openWibFile: failed to open %s", wDS->wibFile);
    }
}

static void setCompareByte(struct wiggleDataStream *wDS,
	double lower, double range)
{
if (wDS->limit_0 < lower)
    wDS->ucLowerLimit = 0;
else
    wDS->ucLowerLimit = MAX_WIG_VALUE * ((wDS->limit_0 - lower)/range);
if (wDS->limit_1 > (lower+range))
    wDS->ucUpperLimit = MAX_WIG_VALUE;
else
    wDS->ucUpperLimit = MAX_WIG_VALUE * ((wDS->limit_1 - lower)/range);

verbose(VERBOSE_HIGHEST, "#\twigSetCompareByte: [%g : %g] becomes [%d : %d]\n",
	lower, lower+range, wDS->ucLowerLimit, wDS->ucUpperLimit);
}

static void resetStats(float *lowerLimit, float *upperLimit, float *sumData,
	float *sumSquares, unsigned *statsCount, long int *chromStart,
	long int *chromEnd)
{
*lowerLimit = INFINITY;
*upperLimit = -1.0 * INFINITY;
*sumData = 0.0;
*sumSquares = 0.0;
*statsCount = 0;
*chromStart = -1;
*chromEnd = 0;
}

static void accumStats(struct wiggleDataStream *wDS, float lowerLimit,
	float upperLimit, float sumData, float sumSquares,
	unsigned statsCount, long int chromStart,
	long int chromEnd)
{
if (statsCount > 0)
    {
    struct wiggleStats *ws;

    AllocVar(ws);
    ws->chrom = cloneString(wDS->currentChrom);
    ws->chromStart = chromStart;
    ws->chromEnd = chromEnd;
    ws->span = wDS->currentSpan;
    ws->count = statsCount;
    ws->lowerLimit = lowerLimit;
    ws->dataRange = upperLimit - lowerLimit;
    ws->mean = sumData / (double) statsCount;
    ws->variance = 0.0;
    ws->stddev = 0.0;
    if (statsCount > 1)
	{
	ws->variance = (sumSquares -
	    ((sumData * sumData)/(double) statsCount)) /
		(double) (statsCount - 1);
	if (ws->variance > 0.0)
	    ws->stddev = sqrt(ws->variance);
	}
    slAddHead(&wDS->stats, ws);
    }
}

static struct bed *bedElement(char *chrom, unsigned start, unsigned end,
        unsigned lineCount)
{
struct bed *bed;
char name[128];

AllocVar(bed);
bed->chrom = cloneString(chrom);
bed->chromStart = start;
bed->chromEnd = end;
snprintf(name, sizeof(name), "%s.%u",
    chrom, lineCount);
bed->name = cloneString(name);
return bed;
}

static void closeWibFile(struct wiggleDataStream *wDS)
/*	if there is a Wib file open, close it	*/
{
if (wDS->wibFH > 0)
    close(wDS->wibFH);
wDS->wibFH = -1;
freez(&wDS->wibFile);
}

static void closeWigConn(struct wiggleDataStream *wDS)
{
lineFileClose(&wDS->lf);
closeWibFile(wDS);	/*	closes only if it is open	*/
if (wDS->conn)
    {
    sqlFreeResult(&wDS->sr);
    sqlDisconnect(&wDS->conn);
    }
freez(&wDS->sqlConstraint);	/*	always reconstructed at open time */
}

static void openWigConn(struct wiggleDataStream *wDS)
/*	open connection to db or to a file, prepare SQL result for db */
{
if (!wDS->tblName)
  errAbort("openWigConn: table name missing.  setDbTable before open.");

if (wDS->isFile)
    {
    struct dyString *fileName = dyStringNew(256);
    lineFileClose(&wDS->lf);	/*	possibly a previous file */
    dyStringPrintf(fileName, "%s.wig", wDS->tblName);
    wDS->lf = lineFileOpen(fileName->string, TRUE);
    dyStringFree(&fileName);
    }
else
    {
    struct dyString *query = dyStringNew(256);
    dyStringPrintf(query, "select * from %s", wDS->tblName);
    if (wDS->chrName)
	addConstraint(wDS, "chrom =", wDS->chrName);
    if (wDS->winEnd)
	{
	char limits[256];
	safef(limits, ArraySize(limits), "%d", wDS->winEnd );
	addConstraint(wDS, "chromStart <", limits);
	safef(limits, ArraySize(limits), "%d", wDS->winStart );
	addConstraint(wDS, "chromEnd >", limits);
	}
    if (wDS->spanLimit)
	{
	struct dyString *dyTmp = dyStringNew(256);
	dyStringPrintf(dyTmp, "%u", wDS->spanLimit);
	addConstraint(wDS, "span =", dyTmp->string);
	dyStringFree(&dyTmp);
	}
    if (wDS->sqlConstraint)
	dyStringPrintf(query, " where (%s)",
	    wDS->sqlConstraint);
    dyStringPrintf(query, " order by ");
    if (!wDS->chrName)
	dyStringPrintf(query, " chrom ASC,");
    if (!wDS->spanLimit)
	dyStringPrintf(query, " span ASC,");
    dyStringPrintf(query, " chromStart ASC");

    verbose(VERBOSE_SQL_ROW_LEVEL, "#\t%s\n", query->string);
    if (!wDS->conn)
	wDS->conn = sqlConnect(wDS->db);
    wDS->sr = sqlGetResult(wDS->conn,query->string);
    }
}


static void setDbTable(struct wiggleDataStream *wDS, char * db, char *table)
/*	sets the DB and table name, determines if table is a file or not */
{
struct dyString *fileName = dyStringNew(256);

if (!table)
    errAbort("setDbTable: table specification missing");

/*	Check to see if there is a .wig file	*/
dyStringPrintf(fileName, "%s.wig", table);

/*	file present ignores db specification	*/
if (fileExists(fileName->string))
    wDS->isFile = TRUE;
else
    {
    if (db)			/*	db can be NULL	*/
	wDS->isFile = FALSE;	/*	assume it will be in the db */
    else
	errAbort("setDbTable: db is NULL and can not find file %s",
	    fileName->string);
    }
dyStringFree(&fileName);

freeMem(wDS->db);		/*	potentially previously existing	*/
if (!wDS->isFile && db)
	wDS->db = cloneString(db);
freeMem(wDS->tblName);		/*	potentially previously existing */
wDS->tblName = cloneString(table);
}

static void outputIdentification(struct wiggleDataStream *wDS, FILE *fh)
{
if (wDS->db)
    fprintf (fh, "#\tdb: '%s', track: '%s'\n", wDS->db, wDS->tblName);
if (wDS->isFile)
    fprintf (fh, "#\tfrom file input, track: '%s'\n", wDS->tblName);
}

static void showConstraints(struct wiggleDataStream *wDS, FILE *fh)
{
if (wDS->chrName)
    fprintf (fh, "#\tchrom specified: %s\n", wDS->chrName);
if (wDS->spanLimit)
    fprintf (fh, "#\tspan specified: %u\n", wDS->spanLimit);
if (wDS->winEnd)
    fprintf (fh, "#\tposition specified: %d-%d\n",
	wDS->winStart+1, wDS->winEnd);
if (wDS->bedConstrained && !wDS->chrName)
    fprintf (fh, "#\tconstrained by chr names and coordinates in bed list\n");
else if (wDS->bedConstrained)
    fprintf (fh, "#\tconstrained by coordinates in bed list\n");
if (wDS->useDataConstraint)
    {
    if ((wDS->dataConstraint) &&
	sameWord(wDS->dataConstraint,"in range"))
	    fprintf (fh, "#\tdata values in range [%g : %g]\n",
		    wDS->limit_0, wDS->limit_1);
    else
	    fprintf (fh, "#\tdata values %s %g\n",
		    wDS->dataConstraint, wDS->limit_0);
    }
}

static int arrayDataCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,winStart */
{
const struct wiggleArray *a = *((struct wiggleArray **)va);
const struct wiggleArray *b = *((struct wiggleArray **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->winStart - b->winStart;
return dif;
}

static int asciiDataCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,span,chromStart. */
{
const struct wigAsciiData *a = *((struct wigAsciiData **)va);
const struct wigAsciiData *b = *((struct wigAsciiData **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    {
    dif = a->span - b->span;
    if (dif == 0)
	dif = a->data->chromStart - b->data->chromStart;
    }
return dif;
}

static int statsDataCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,span,chromStart. */
{
const struct wiggleStats *a = *((struct wiggleStats **)va);
const struct wiggleStats *b = *((struct wiggleStats **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    {
    dif = a->span - b->span;
    if (dif == 0)
	dif = a->chromStart - b->chromStart;
    }
return dif;
}

/*	currently this dataArrayOut is private, but it may become public */
static void dataArrayOut(struct wiggleDataStream *wDS, char *fileName,
	boolean rawDataOut, boolean sort)
/*	print to fileName the ascii data values	*/
{
FILE * fh;
fh = mustOpen(fileName, "w");
if (wDS->array)
    {
    unsigned pointCount;
    unsigned chromPosition;
    unsigned inx;
    float *dataPtr;

    if (sort)
	slSort(&wDS->array, arrayDataCmp);

    dataPtr = wDS->array->data;
    /*	user visible coordinates are 1 relative	*/
    chromPosition = wDS->array->winStart + 1;
    pointCount = wDS->array->winEnd - wDS->array->winStart;
    for (inx = 0; inx < pointCount; ++inx)
	{
	if (!isnan(*dataPtr))
	    {
	    if (rawDataOut)
		fprintf (fh, "%g\n", *dataPtr);
	    else
		fprintf (fh, "%u\t%g\n", chromPosition, *dataPtr);
	    }
	++dataPtr;
	++chromPosition;
	}

    }
else
    {
    if (!rawDataOut)
	{
	showConstraints(wDS, fh);
	fprintf(fh, "#\tdataArray: no data points found\n");
	}
    }
carefulClose(&fh);
}	/*	static void dataArrayOut()	*/

/*	PUBLIC	METHODS   **************************************************/
static void setPositionConstraint(struct wiggleDataStream *wDS,
	int winStart, int winEnd)
/*	both 0 means no constraint	*/
{
if ((!wDS->isFile) && wDS->conn)
    {	/*	this should not happen	*/
    errAbort("setPositionConstraint: not allowed after openWigConn()");
    }
/*	keep them in proper order	*/
if (winStart > winEnd)
    {
    wDS->winStart = winEnd;
    wDS->winEnd = winStart;
    }
else if ((winEnd > 0) && (winStart == winEnd))
    errAbort(
	"setPositionConstraint: can not have winStart == winEnd (%d == %d)",
	    winStart, winEnd);
else
    {
    wDS->winStart = winStart;
    wDS->winEnd = winEnd;
    }
verbose(VERBOSE_CHR_LEVEL,"#\tsetPosition: %d - %d\n", wDS->winStart, wDS->winEnd);
}

static void setChromConstraint(struct wiggleDataStream *wDS, char *chr)
{
freeMem(wDS->chrName);
wDS->chrName = cloneString(chr);
}

static void setSpanConstraint(struct wiggleDataStream *wDS, unsigned span)
{
wDS->spanLimit = span;
}

static void freeConstraints(struct wiggleDataStream *wDS)
/*	free the position, span, chrName and data constraints */
{
wDS->spanLimit = 0;
wDS->setPositionConstraint(wDS, 0, 0);
freez(&wDS->chrName);
freez(&wDS->dataConstraint);
wDS->wigCmpSwitch = wigNoOp_e;
wDS->limit_0 = wDS->limit_1 = 0.0;
wDS->ucLowerLimit = wDS->ucUpperLimit = 0;
freez(&wDS->sqlConstraint);
wDS->useDataConstraint = FALSE;
wDS->bedConstrained = FALSE;
}

static void freeAscii(struct wiggleDataStream *wDS)
/*	free the wiggleAsciiData list	*/
{
if (wDS->ascii)
    {
    struct wigAsciiData *el, *next;

    for (el = wDS->ascii; el != NULL; el = next)
	{
	next = el->next;
	freeMem(el->chrom);
	freeMem(el->data);
	freeMem(el);
	}
    }
wDS->ascii = NULL;
}

static void freeBed(struct wiggleDataStream *wDS)
/*	free the wiggle bed list	*/
{
bedFreeList(&wDS->bed);
}

static void freeArray(struct wiggleDataStream *wDS)
/*	free the wiggleArray list	*/
{
if (wDS->array)
    {
    struct wiggleArray *wa;
    struct wiggleArray *next;

    for (wa = wDS->array; wa; wa = next)
	{
	next = wa->next;
	freeMem(wa->chrom);
	freeMem(wa->data);
	freeMem(wa);
	}
    }
wDS->array = NULL;
}

static void freeStats(struct wiggleDataStream *wDS)
/*	free the wiggleStats list	*/
{
if (wDS->stats)
    {
    struct wiggleStats *el, *next;

    for (el = wDS->stats; el != NULL; el = next)
	{
	next = el->next;
	freeMem(el->chrom);
	freeMem(el);
	}
    }
wDS->stats = NULL;
}

/*	the double comparison functions
 *	used to check the wiggle SQL rows which are a bucket of values
 *	between *lower and *upper.  Therefore, the value to be checked
 *	which is in wDS->limit_0 (and wDS->limit_1 in the case of
 *	a range) needs to be compared to the bucket of values.  If it
 *	falls within the specified range, then it is considered to be in
 *	that bucket.
 */
/* InRange means the SQL row begins before the limit_1 (lower<=limit_1)
 *	 and the row ends after the limit_0 (upper>=limit_0)
 *	i.e. there is at least some overlap of the range
 */
/* LessThan means:  the row begins before the limit_0 (value<limit_0)
 *	i.e. there are data values below the specified limit_0
 */
/* LessEqual means:  the row begins at or before the limit_0 (value<=limit_0)
 *	i.e. there are data values at or below the specified limit_0
 */
/* Equal means:  similar to InRange, the test value limit_0 can be found
 * in the SQL row, i.e. lower <= limit_0 <= upper
 */
/* NotEqual means:  the opposite of Equal, the test value limit_0 can not
 *	be found in the SQL row, i.e. (limit_0 < lower) or (upper < limit_0)
 */
/* GreaterEqual means:  the row ends at or after the limit_0 (limit_0<=upper)
 *	i.e. there are data values at or above the specified limit_0
 */
/* GreaterEqual means:  the row ends after the limit_0 (limit_0<upper)
 *	i.e. there are data values above the specified limit_0
 */
/*	the unsigned char comparison functions
 *	Unlike the above, these are straighforward, just compare the
 *	byte values
 */

static void wigSetCompareFunctions(struct wiggleDataStream *wDS)
{
if (!wDS->dataConstraint)
    return;

if (sameWord(wDS->dataConstraint,"<"))
    wDS->wigCmpSwitch = wigLessThan_e;
else if (sameWord(wDS->dataConstraint,"<="))
    wDS->wigCmpSwitch = wigLessEqual_e;
else if (sameWord(wDS->dataConstraint,"="))
    wDS->wigCmpSwitch = wigEqual_e;
else if (sameWord(wDS->dataConstraint,"!="))
    wDS->wigCmpSwitch = wigNotEqual_e;
else if (sameWord(wDS->dataConstraint,">="))
    wDS->wigCmpSwitch = wigGreaterEqual_e;
else if (sameWord(wDS->dataConstraint,">"))
    wDS->wigCmpSwitch = wigGreaterThan_e;
else if (sameWord(wDS->dataConstraint,"in range"))
    wDS->wigCmpSwitch = wigInRange_e;
else
    errAbort("wigSetCompareFunctions: unknown constraint: '%s'",
	wDS->dataConstraint);
verbose(VERBOSE_CHR_LEVEL, "#\twigSetCompareFunctions: set to '%s'\n", wDS->dataConstraint);
}

static void setDataConstraint(struct wiggleDataStream *wDS,
	char *dataConstraint, double lowerLimit, double upperLimit)
{
wDS->dataConstraint = cloneString(dataConstraint);
if (lowerLimit < upperLimit)
    {
    wDS->limit_0 = lowerLimit;
    wDS->limit_1 = upperLimit;
    }
else if (!(upperLimit < lowerLimit))
  errAbort("wigSetDataConstraint: upper and lower limits are equal: %g == %g",
	lowerLimit, upperLimit);
else
    {
    wDS->limit_0 = upperLimit;
    wDS->limit_1 = lowerLimit;
    }
wigSetCompareFunctions(wDS);
wDS->useDataConstraint = TRUE;
}

static unsigned long long getData(struct wiggleDataStream *wDS, char *db,
	char *table, int operations)
/* getData - read and return wiggle data	*/
{
char *row[WIGGLE_NUM_COLS];
unsigned long long rowCount = 0;
unsigned long long validData = 0;
unsigned long long valuesMatched = 0;
unsigned long long noDataBytes = 0;
unsigned long long bytesSkipped = 0;
boolean doStats = FALSE;
boolean doBed = FALSE;
boolean doAscii = FALSE;
boolean doDataArray = FALSE;
boolean doNoOp = FALSE;
boolean skipDataRead = FALSE;	/*	may need this later	*/
float lowerLimit = INFINITY;
float upperLimit = -1.0 * INFINITY;
float sumData = 0.0;
float sumSquares = 0.0;
unsigned statsCount = 0;
long int chromStart = -1;
long int chromEnd = 0;
boolean summaryOnly = TRUE;
unsigned bedElStart = 0;
unsigned bedElEnd = 0;
unsigned bedElCount = 0;
boolean firstSpanDone = FALSE;	/*	to prevent multiple bed lists */
float *dataArrayPtr = NULL;	/*	to access the data array values */
unsigned dataArrayPosition = 0;	/*  marches thru all from beginning to end */

doAscii = operations & wigFetchAscii;
doDataArray = operations & wigFetchDataArray;
doBed = operations & wigFetchBed;
doStats = operations & wigFetchStats;
doNoOp = operations & wigFetchNoOp;

/*	for timing purposes, allow the wigFetchNoOp to go through */
if (doNoOp)
    {
    doDataArray = doBed = doStats = doAscii = FALSE;
    }

if (! (doNoOp || doAscii || doDataArray || doBed || doStats) )
    {
	verbose(VERBOSE_CHR_LEVEL, "getData: no type of data fetch requested ?\n");
	return(valuesMatched);	/*	NOTHING ASKED FOR ?	*/
    }

/*	not going to do this without a range specified and a chr name */
if (doDataArray && wDS->winEnd && wDS->chrName)
    {
    struct wiggleArray *wa;
    size_t inx, size; 
    size_t winSize;

    AllocVar(wa);
    wa->chrom = cloneString(wDS->chrName);
    wa->winStart = wDS->winStart;
    wa->winEnd = wDS->winEnd;
    winSize = wa->winEnd - wa->winStart;
    size = sizeof(float) * winSize;
    /*	good enough for 500,000,000 bases	*/
    setMaxAlloc((size_t)2100000000);	/*2^31 = 2,147,483,648 */
    wa->data = (float *)needLargeMem(size);
    dataArrayPtr = wa->data;
    /*  something like this could best be done by a step wise memcpy */
    /*	this takes 30 seconds for 500,000,000 winSize on hgwdev */
    for (inx = 0; inx < winSize; ++inx)
	*dataArrayPtr++ = NAN;
    dataArrayPtr = wa->data;
    dataArrayPosition = wa->winStart;
    slAddHead(&wDS->array, wa);
    verbose(VERBOSE_CHR_LEVEL,
	"#\tgetData: created data array for %d values (%u b)\n",
	winSize, size);
    }
else if (doDataArray && !(doNoOp || doAscii || doBed || doStats))
    {
	verbose(VERBOSE_CHR_LEVEL,
		"getData: must specify a chrom range to FetchDataArray\n");
	return(valuesMatched);	/*	NOTHING ASKED FOR ?	*/
    }

setDbTable(wDS, db, table);
openWigConn(wDS);

if (doDataArray)
    summaryOnly = FALSE;
if (doAscii)
    summaryOnly = FALSE;
if (doBed)
    summaryOnly = FALSE;
if (!wDS->isFile && wDS->winEnd)
    summaryOnly = FALSE;

/*	nextRow produces next SQL row from either DB or file	*/
while (nextRow(wDS, row, WIGGLE_NUM_COLS))
    {
    struct wigAsciiData *wigAscii = NULL;
    struct wiggle *wiggle;
    struct asciiDatum *asciiOut = NULL;	/* to address data[] in wigAsciiData */
    unsigned chromPosition;

    ++rowCount;
    wiggle = wiggleLoad(row);
    /*	constraints have to be done manually for files	*/
    if (wDS->isFile)
	{
	/*	if chrName is not correct, or span is not correct,
	 *	or outside of window, go to next SQL row
	 */
	if (
	    ((wDS->chrName) && (differentString(wDS->chrName,wiggle->chrom))) ||
	    ((wDS->spanLimit) && (wDS->spanLimit != wiggle->span)) ||
	    ((wDS->winEnd) && ((wiggle->chromStart >= wDS->winEnd) ||
		(wiggle->chromEnd < wDS->winStart)) )
	   )
	    {
	    bytesSkipped += wiggle->count;
	    wiggleFree(&wiggle);
	    continue;	/*	next SQL row	*/
	    }
	}

    chromPosition = wiggle->chromStart;

    /*	this will be true the very first time for both reasons	*/
    if ( (wDS->currentSpan != wiggle->span) || 
	    (wDS->currentChrom &&
		differentString(wDS->currentChrom, wiggle->chrom)))
	{
	/*	if we have been working on one, then doBed	*/
	if (!firstSpanDone && doBed && wDS->currentChrom)
	    {
	    if (bedElEnd > bedElStart)
		{
		struct bed *bedEl;
		bedEl = bedElement(wDS->currentChrom,
			bedElStart, bedElEnd, ++bedElCount);
		slAddHead(&wDS->bed, bedEl);
		}
	    bedElStart = 0;
	    bedElEnd = 0;
	    bedElCount = 0;
	    }
	if (wDS->currentSpan && (wDS->currentSpan |= wiggle->span))
	    firstSpanDone = TRUE;
	if (wDS->currentChrom &&
		differentString(wDS->currentChrom, wiggle->chrom))
	    firstSpanDone = FALSE;
	/*	if we have been working on one, then doStats	*/
	if (doStats && wDS->currentChrom)
	    {
	    accumStats(wDS, lowerLimit, upperLimit, sumData, sumSquares,
		statsCount, chromStart, chromEnd);
	    resetStats(&lowerLimit, &upperLimit, &sumData, &sumSquares,
		&statsCount, &chromStart, &chromEnd);
	    }
	freeMem(wDS->currentChrom);
	/*	set them whether they are changing or not, doesn't matter */
	wDS->currentChrom = cloneString(wiggle->chrom);
	wDS->currentSpan = wiggle->span;
	}

    /*	it may seem inefficient to make one of these for each SQL row,
     *	but the alternative would require expanding the data area for
     *	each row and thus a re-alloc - probably more expensive than
     *	just making one of these for each row.
     */
    if (doAscii)
	{
	AllocVar(wigAscii);
	wigAscii->chrom = cloneString(wiggle->chrom);
	wigAscii->span = wiggle->span;
	wigAscii->count = 0;	/* will count up as values added */
	wigAscii->data = (struct asciiDatum *) needMem((size_t)
	    (sizeof(struct asciiDatum) * wiggle->validCount));
	asciiOut = wigAscii->data;
	slAddHead(&wDS->ascii,wigAscii);
	}

    verbose(VERBOSE_PER_VALUE_LEVEL,
	    "#\trow: %llu, start: %u, data range: %g: [%g:%g]\n",
	    rowCount, wiggle->chromStart, wiggle->dataRange,
	    wiggle->lowerLimit, wiggle->lowerLimit+wiggle->dataRange);
    verbose(VERBOSE_SQL_ROW_LEVEL, "#\tresolution: %g per bin\n",
	    wiggle->dataRange/(double)MAX_WIG_VALUE);
    if (wDS->useDataConstraint)
	{
	boolean takeIt = FALSE;
	switch (wDS->wigCmpSwitch)
	    {
	    case wigNoOp_e:
		takeIt = TRUE;
		break;
	    case wigInRange_e:
		takeIt = (wiggle->lowerLimit <= wDS->limit_1) &&
		    ((wiggle->lowerLimit + wiggle->dataRange) >=
			    wDS->limit_0);
		break;
	    case wigLessThan_e:
		takeIt = wiggle->lowerLimit < wDS->limit_0;
		break;
	    case wigLessEqual_e:
		takeIt = wiggle->lowerLimit <= wDS->limit_0;
		break;
	    case wigEqual_e:
		takeIt = (wiggle->lowerLimit <= wDS->limit_0) &&
		    (wDS->limit_0 <=
			(wiggle->lowerLimit + wiggle->dataRange));
		break;
	    case wigNotEqual_e:
		takeIt = (wDS->limit_0 < wiggle->lowerLimit) ||
		    ((wiggle->lowerLimit + wiggle->dataRange) <
			wDS->limit_0);
		break;
	    case wigGreaterEqual_e:
	       takeIt = wDS->limit_0 <=
		    (wiggle->lowerLimit + wiggle->dataRange);
		break;
	    case wigGreaterThan_e:
	       takeIt = wDS->limit_0 <
		    (wiggle->lowerLimit + wiggle->dataRange);
		break;
	    default:
		errAbort("wig_getData: illegal wigCmpSwitch ? %#x",
		    wDS->wigCmpSwitch);
		break;
	    }	/*	switch (wDS->wigCmpSwitch)	*/
	if (takeIt)
	    setCompareByte(wDS, wiggle->lowerLimit, wiggle->dataRange);
	else
	    {
	    bytesSkipped += wiggle->count;
	    wiggleFree(&wiggle);
	    verbose(VERBOSE_HIGHEST,
		"#\tthis row fails compare, next SQL row\n");
	    continue;	/*	next SQL row	*/
	    }
	}
    if (summaryOnly && doStats)
	{
	/*	record maximum limits of data values	*/
	if (wiggle->lowerLimit < lowerLimit)
	    lowerLimit = wiggle->lowerLimit;
	if (wiggle->lowerLimit+wiggle->dataRange > upperLimit)
	    upperLimit = wiggle->lowerLimit+wiggle->dataRange;
	sumData += wiggle->sumData;
	sumSquares += wiggle->sumSquares;
	/*	record maximum extents	*/
	if ((chromStart < 0)||(chromStart > wiggle->chromStart))
	    chromStart = wiggle->chromStart;
	if (chromEnd < wiggle->chromEnd)
	    chromEnd = wiggle->chromEnd;
	statsCount += wiggle->validCount;
	wiggleFree(&wiggle);
	continue;	/*	next SQL row	*/
	}
    if (!skipDataRead)
	{
	int j;	/*	loop counter through readData	*/
	unsigned char *datum;    /* to walk through readData bytes */
	unsigned char *readData;    /* the bytes read in from the file */

	openWibFile(wDS, wiggle->file);
		    /* possibly open a new wib file */
	readData = (unsigned char *) needMem((size_t) (wiggle->count + 1));
	wDS->bytesRead += wiggle->count;
	lseek(wDS->wibFH, wiggle->offset, SEEK_SET);
	read(wDS->wibFH, readData,
	    (size_t) wiggle->count * (size_t) sizeof(unsigned char));

	verbose(VERBOSE_PER_VALUE_LEVEL,
		"#\trow: %llu, reading: %u bytes\n", rowCount, wiggle->count);

	datum = readData;
	for (j = 0; j < wiggle->count; ++j)
	    {
	    if (*datum != WIG_NO_DATA)
		{
		boolean takeIt = FALSE;

		if (wDS->winEnd)  /* non-zero means a range is in effect */
		    {	/*	do not allow item (+span) to run over winEnd */
		    unsigned span = wiggle->span;

			/*	if only doing data array, span is 1 	*/
		    if (doDataArray && !(doStats || doBed || doStats))
			span = 1;

		    if ( (chromPosition < wDS->winStart) ||
			((chromPosition+span) > wDS->winEnd) )
			{
			++datum;	/*	out of range	*/
			chromPosition += wiggle->span;
			continue;	/*	next *datum	*/
			}
		    }
		++validData;
		switch (wDS->wigCmpSwitch)
		    {
		    case wigNoOp_e:
			takeIt = TRUE;
			break;
		    case wigInRange_e:
			takeIt = (*datum <= wDS->ucUpperLimit) &&
			    (*datum >= wDS->ucLowerLimit);
			break;
		    case wigLessThan_e:
			takeIt = *datum < wDS->ucLowerLimit;
			break;
		    case wigLessEqual_e:
			takeIt = *datum <= wDS->ucLowerLimit;
			break;
		    case wigEqual_e:
			takeIt = *datum == wDS->ucLowerLimit;
			break;
		    case wigNotEqual_e:
			takeIt = *datum != wDS->ucLowerLimit;
			break;
		    case wigGreaterEqual_e:
			takeIt = *datum >= wDS->ucLowerLimit;
			break;
		    case wigGreaterThan_e:
			takeIt = *datum > wDS->ucLowerLimit;
			break;
		    default:
			errAbort("wig_getData: illegal wigCmpSwitch ? %#x",
			    wDS->wigCmpSwitch);
			break;
		    }	/*	switch (wDS->wigCmpSwitch)	*/
		if (takeIt)
		    {
		    float value = 0.0;
		    ++valuesMatched;
		    if (doAscii || doStats || doDataArray)
			value = BIN_TO_VALUE(*datum,
					wiggle->lowerLimit,wiggle->dataRange);
		    if (doAscii)
			{
			asciiOut->value = value;
			asciiOut->chromStart = chromPosition;
			++asciiOut;
			++wigAscii->count;
			}
		    if (!firstSpanDone && doDataArray)
			{
			unsigned span = wiggle->span;
			unsigned inx;
			unsigned gap = chromPosition - dataArrayPosition;

			dataArrayPosition += gap;

			/*	special case squeezing into the end	*/
			if ((dataArrayPosition + span) > wDS->winEnd)
				span = wDS->winEnd - dataArrayPosition;
				
			dataArrayPtr += gap;	/* move up, leave NaN's */
			verbose(VERBOSE_PER_VALUE_LEVEL, "#\t%u-%u <- %g\n",
				dataArrayPosition,
				dataArrayPosition + span, value);
			/*	fill all chrom positions for this element */
			for (inx = 0; inx < span; ++inx)
				*dataArrayPtr++ = value;
			dataArrayPosition += span;
			}
		    /*	beware, coords must come in sequence for this to
		     *	work.  The original SQL query should be ordering
		     *	things properly.  We only do the first span
		     *	encountered, it should be the lowest due to the
		     *	SQL query.
		     */
		    if (!firstSpanDone && doBed)
			{
			/*	as long as they are continuous, keep
			 *	extending the end
			 */
			if (chromPosition == bedElEnd)
			    bedElEnd += wiggle->span;
			else
			    {	/*	if one was collected, output it */
			    if (bedElEnd > bedElStart)
				{
				struct bed *bedEl;
				bedEl = bedElement(wiggle->chrom,
					bedElStart, bedElEnd, ++bedElCount);
				slAddHead(&wDS->bed, bedEl);
				}
			    /*	start a new element here	*/
			    bedElStart = chromPosition;
			    bedElEnd = bedElStart + wiggle->span;
			    }
			}
		    if (doStats)
			{
			/*	record maximum data limits	*/
			if (value < lowerLimit)
			    lowerLimit = value;
			if (value > upperLimit)
			    upperLimit = value;
			sumData += value;
			sumSquares += value * value;
			/*	record maximum extents	*/
			if ((chromStart < 0)||(chromStart > chromPosition))
			    chromStart = chromPosition;
			if (chromEnd < (chromPosition + wiggle->span))
			    chromEnd = chromPosition + wiggle->span;
			++statsCount;
			}
		    }
		}	/*	if (*datum != WIG_NO_DATA)	*/
	    else
		{
		++noDataBytes;
		}
	    ++datum;
	    chromPosition += wiggle->span;
	    }	/*	for (j = 0; j < wiggle->count; ++j)	*/
	freeMem(readData);
	}	/*	if (!skipDataRead)	*/
    wiggleFree(&wiggle);
    }		/*	while (nextRow())	*/

/*	there may be one last bed element to output	*/
if (!firstSpanDone && doBed)
    {
    if (bedElEnd > bedElStart)
	{
	struct bed *bedEl;
	bedEl = bedElement(wDS->currentChrom,
		bedElStart, bedElEnd, ++bedElCount);
	slAddHead(&wDS->bed, bedEl);
	}
    }

/*	there are accumulated stats to complete	*/
if (doStats)
    {
    accumStats(wDS, lowerLimit, upperLimit, sumData, sumSquares,
	statsCount, chromStart, chromEnd);
    resetStats(&lowerLimit, &upperLimit, &sumData, &sumSquares,
	&statsCount, &chromStart, &chromEnd);
    }

/*	The proper in chrom ordering was already done by the original SQL
 *	select statement.  To finish off, all results need to be reversed
 *	since they went in via AddHead which is an inverse insertion.
 *	More properly, we need a real sort routine on these specialized
 *	lists to make sure they really get into order.  The special
 *	thing on bedConstrained is a one-time test since getData is
 *	being called multiple times before it is finished and there is
 *	no need to try and reverse all accumulating results before they
 *	are done.  In fact it gets them all mixed up.  Thus again, a
 *	real need for an actual sort routine that would do everything
 *	properly no matter what happened during the result assembly.
if (!wDS->bedConstrained)
    {
    if (doAscii)
	slReverse(&wDS->ascii);
    if (doBed)
	slReverse(&wDS->bed);
    if (doStats)
	slReverse(&wDS->stats);
    }
	A sortResults method has been added.  It can be used by the caller,
 *	or the *Out() methods will sort results before output.
 */

wDS->rowsRead += rowCount;
wDS->validPoints += validData;
wDS->valuesMatched += valuesMatched;
wDS->noDataPoints += noDataBytes;
wDS->bytesSkipped += bytesSkipped;

closeWigConn(wDS);

return(valuesMatched);
}	/*	unsigned long long getData()	*/

static unsigned long long getDataViaBed(struct wiggleDataStream *wDS, char *db,
	char *table, int operations, struct bed **bedList)
/* getDataViaBed - constrained by the bedList	*/
{
unsigned long long valuesMatched = 0;

if (bedList && *bedList)
    {
    struct bed *filteredBed = NULL;
    boolean chromConstraint = ((char *)NULL != wDS->chrName);
    boolean positionConstraints = (0 != wDS->winEnd);
    int saveWinStart = 0;
    int saveWinEnd = 0;
    char * saveChrName = NULL;
    struct slName *chromList = NULL;
    struct slName *chr;
    struct hash *chromSizes = NULL;
    struct { unsigned chrStart; unsigned chrEnd; } *chrStartEnd;
    unsigned long long valuesFound = 0;

    /* remember these constraints so we can reset them afterwards
     * because we are going to use them for our own internal uses here.
     */
    saveWinStart = wDS->winStart;
    saveWinEnd = wDS->winEnd;
    saveChrName = wDS->chrName;

    chromSizes = newHash(0);

    /*	Determine what will control which chrom to do.  If a chrom
     *	constraint has been set then that is the one to do.  If it has
     *	not been set, then allow the bedList to be the list of chroms to
     *	do.
     */
    if (chromConstraint)
	{
	chr = newSlName(wDS->chrName);
	slAddHead(&chromList, chr);
	AllocVar(chrStartEnd);
	chrStartEnd->chrStart = wDS->winStart;
	chrStartEnd->chrEnd = wDS->winEnd;
	hashAdd(chromSizes, wDS->chrName, chrStartEnd);
	}
    else
	{
	char *chrName = NULL;
	struct bed *bed;
	unsigned bedStart = 0;
	unsigned bedEnd = 0;

	/*	no need to clone the name, it remains there, we are just
	 *	pointing to it.
	 */
	bedStart = (*bedList)->chromStart;
	chrName = (*bedList)->chrom;
	for (bed = *bedList; bed; bed = bed->next)
	    {
	    if (differentString(chrName, bed->chrom))
		{
		chr = newSlName(chrName);
		slAddHead(&chromList, chr);
		AllocVar(chrStartEnd);
		chrStartEnd->chrStart = bedStart;
		chrStartEnd->chrEnd = bedEnd;
		hashAdd(chromSizes, chrName, chrStartEnd);
		chrName = bed->chrom;
		bedStart = bed->chromStart;
		}
	    bedEnd = bed->chromEnd;
	    }
	    /*	and the last one remains to be done	*/
	    chr = newSlName(chrName);
	    slAddHead(&chromList, chr);
	    AllocVar(chrStartEnd);
	    chrStartEnd->chrStart = bedStart;
	    chrStartEnd->chrEnd = bedEnd;
	    hashAdd(chromSizes, chrName, chrStartEnd);
	    slReverse(&chromList);
	}

    /*	OK, the chromList and chromSizes hash have been created, run
     *	through that list.  This may seem unusual, but if the caller has
     *	set position constraints but no chr constraint, allow those
     *	constraints to override
     *	the bedList coordinates, even though we may be running through a
     *	number of chroms.  That's what they wanted, that's what they get.
     *	e.g. they wanted the first 10,000 bases of each chrom,
     *		winStart=0, winEnd = 10,000, no chrName specified
     */
    for (chr = chromList; chr; chr = chr->next)
	{
	float *fptr;
	boolean doAscii = FALSE;
	boolean doBed = FALSE;
	boolean doStats = FALSE;
	boolean doNoOp = FALSE;
	boolean doDataArray = FALSE;
	struct bed *bed;
	unsigned bedExtent = 0;	/* from first thru last element of bedList*/
	unsigned bedStart = 0;
	unsigned bedEnd = 0;
	unsigned bedPosition = 0;
	char *bedArray;	/*	meaning is boolean but	*/
	char *boolPtr;	/*	boolean is 4 bytes, char is 1	*/
	unsigned bedArraySize = 0;
	unsigned winStart = 0;
	unsigned winEnd = 0;
	unsigned winExtent = 0;
	unsigned bedMarked = 0;
	unsigned chromPosition = 0;
	unsigned bedElStart = 0;
	unsigned bedElEnd = 0;
	unsigned bedElCount = 0;
	float lowerLimit = INFINITY;		/*	stats accumulators */
	float upperLimit = -1.0 * INFINITY;	/*	stats accumulators */
	float sumData = 0.0;			/*	stats accumulators */
	float sumSquares = 0.0;			/*	stats accumulators */
	unsigned statsCount = 0;		/*	stats accumulators */
	long int chromStart = -1;		/*	stats accumulators */
	long int chromEnd = 0;			/*	stats accumulators */

	doDataArray = operations & wigFetchDataArray;

	/*	set chrom name constraint	*/
	wDS->setChromConstraint(wDS, (char *)&chr->name);

	chrStartEnd = hashFindVal(chromSizes, wDS->chrName); 
	if (NULL == chrStartEnd)
	    errAbort("getDataViaBed: constructed hash of bed coords is broken");
	/* This may seem unusual, but if the caller has
	 *	set position constraints, allow those constraints to override
	 *	the bedList coordinates, even though we may be running through
	 *	a number of chroms.
	 */
	if (saveWinEnd)
	    {
	    winStart = saveWinStart;
	    winEnd = saveWinEnd;
	    }
	else
	    {
	    winStart = chrStartEnd->chrStart;
	    winEnd = chrStartEnd->chrEnd;
	    }
	/*	winExtent could still be 0 here	*/
	/*	remember this window extent	*/
	winExtent = winEnd - winStart;
	/*	set window position constraint	*/
	wDS->setPositionConstraint(wDS, winStart, winEnd);

	/*	Going to create a TRUE/FALSE marker array from the bedList.
	 *	Each chrom position will be marked TRUE if it belongs to
 	 *	a bed element.  Overlapping bed elements will be
 	 *	redundant and a waste of time, but no real problem.
	 *	Filter by the chr name and the win limits.
	 */
	for (bed = *bedList; bed; bed = bed->next)
	    {
	    if (sameString(wDS->chrName, bed->chrom))
		{
		/*	if (no limits) OR (within limits) use it	*/
		if (!(winEnd) || ( (bed->chromStart < winEnd) &&
					(bed->chromEnd >= winStart)) )
		    {
		    struct bed *newBed = cloneBed(bed);
		    slAddHead(&filteredBed, newBed);
		    }
		}
	    }
	if (NULL == filteredBed)
	    {
	    verbose(VERBOSE_CHR_LEVEL,
		"#\tfilter found nothing in bed file: %s:%u-%u\n",
		wDS->chrName, winStart, winEnd);
	    continue;	/*	next bed element	*/
	    }
	else
	    slSort(&filteredBed,bedCmp);	/*	in proper order */

	if (verboseLevel() >= VERBOSE_CHR_LEVEL)
	    {
	    int count=slCount(filteredBed);
	    verbose(VERBOSE_CHR_LEVEL,
		"#\tfiltered by chr,pos constraints, bed el count: %d\n",
		    count);
	    }

	/*	determine actual limits loaded by the filter	*/
	bedStart = min(winStart,filteredBed->chromStart);
	bedEnd = filteredBed->chromEnd;
	for (bed = filteredBed; bed; bed = bed->next)
	    {
	    if (bed->chromStart < bedStart)
		bedStart = bed->chromStart;
	    if (bed->chromEnd > bedEnd)
		{
		bedEnd = bed->chromEnd;
		}
	    }
	bedExtent = bedEnd - bedStart;

	/*	must use the larger of the two sizes	*/
	bedArraySize = max(winExtent,bedExtent);
	bedArraySize *= sizeof(char);	/*	in bytes	*/

	/*	good enough for 2,000,000,000 bases (bedArray elem 1 byte) */
	setMaxAlloc((size_t)2100000000);	/*2^31 = 2,147,483,648 */
	bedArray = (char *)needLargeMem(bedArraySize);

	/*	set them all to FALSE to begin with	*/
	memset((void *)bedArray, FALSE, bedArraySize);

	/*	use the bed definition to fill this marker array */
	/*	start at the lesser of the two start positions	*/
	bedPosition = min(winStart, bedStart);
	bedStart = bedPosition;
	boolPtr = bedArray;
	for (bed = filteredBed; bed; bed = bed->next)
	    {
	    unsigned gap = bed->chromStart - bedPosition;
	    unsigned elSize = bed->chromEnd - bed->chromStart;

	    boolPtr += gap;
	    memset((void *)boolPtr, TRUE, elSize);
	    boolPtr += elSize;
	    bedPosition += gap + elSize;
	    bedMarked += elSize;	/*	number of bases marked	*/
	    }
	bedEnd = bedPosition;
	bedExtent = bedEnd - bedStart;
	verbose(VERBOSE_CHR_LEVEL,
		"#\tbed range %u = %u - %u, allocated %u bytes\n",
		bedExtent, bedEnd, bedStart, bedArraySize);
	verbose(VERBOSE_CHR_LEVEL, "#\tbed marked %u bases, winSize %u\n",
		bedMarked, winExtent);

	/*set window position constraint, this eliminates the 0,0 possibility*/
	if (winEnd)
	    {
	    winStart = max(bedStart, winStart);
	    winEnd = min(bedEnd, winEnd);
	    }
	else
	    {
	    winStart = bedStart;
	    winEnd = bedEnd;
	    }

	/*	must have actual positions for constraints for getData
	 *	to do this wigFetchDataArray function.
	 */
	wDS->setPositionConstraint(wDS, winStart, winEnd);
	/*	now fetch all the wiggle data for this position,
	 *	constraints have been set at the top of this loop
	 */

	valuesFound = wDS->getData(wDS, db, table, wigFetchDataArray);

	verbose(VERBOSE_CHR_LEVEL,
	    "#\tback from getData, found %llu valid points\n", valuesFound);
	doAscii = operations & wigFetchAscii;
	doBed = operations & wigFetchBed;
	doStats = operations & wigFetchStats;
	doNoOp = operations & wigFetchNoOp;

	/*	if ((something found) and (something to do))	*/
	if ((valuesFound > 0) && (doAscii || doBed || doStats || doNoOp))
	    {
	    struct wigAsciiData *wigAscii = NULL;
	    struct asciiDatum *asciiOut = NULL;
		/* to address data[] in wigAsciiData */

	    if (verboseLevel() > VERBOSE_PER_VALUE_LEVEL)
		dataArrayOut(wDS, "stdout", FALSE, TRUE);

	    /*	OK, ready to scan the bedArray in concert with the data
	     *	array and pick out those values that are masked by the bed.
	     *	The starting position will be the greater of the
	     *	two different possible windows.
	     */
	    if (doAscii)
		{
		AllocVar(wigAscii);
		wigAscii->chrom = cloneString(wDS->chrName);
		wigAscii->span = 1;	/* span information has been lost */
		wigAscii->count = 0;	/* will count up as values added */
		wigAscii->data = (struct asciiDatum *) needMem((size_t)
		    (sizeof(struct asciiDatum) * valuesFound));
			/*	maximum area needed, may use less 	*/
		asciiOut = wigAscii->data;	/* ptr to data area	*/
		slAddHead(&wDS->ascii,wigAscii);
		}

	    chromPosition = max(bedStart, winStart);
	    boolPtr = bedArray + (chromPosition - bedStart);

	    fptr = wDS->array->data + (chromPosition - bedStart);

	    while (chromPosition < winEnd)
		{
		if (*boolPtr && (!isnan(*fptr)))
		    {
		    float value = *fptr;

		    ++valuesMatched;
		    /*	construct output listings here	*/
		    if (doAscii)
			{
			asciiOut->value = value;
			asciiOut->chromStart = chromPosition;
			++asciiOut;
			++wigAscii->count;
			}
		    if (doBed)
			{
			/*	have we started a bed element yet ?	*/
			if (0 == bedElEnd)
			    bedElEnd = bedElStart = chromPosition;
			/*	That will get it started right here	*/

			/*	as long as they are continuous, keep
			 *	extending the end
			 */
			if (chromPosition == bedElEnd)
			    bedElEnd += 1; /* span information has been lost */
			else
			    {	/*	if one was collected, output it */
			    if (bedElEnd > bedElStart)
				{
				struct bed *bedEl;
				bedEl = bedElement(wDS->chrName,
					bedElStart, bedElEnd, ++bedElCount);
				slAddHead(&wDS->bed, bedEl);
				}
			    /*	start a new element here	*/
			    bedElStart = chromPosition;
			    bedElEnd = bedElStart + 1; /* span = 1	*/
			    }
			}	/*	if (doBed)	*/
		    if (doStats)
			{
			/*	record maximum data limits	*/
			if (value < lowerLimit)
			    lowerLimit = value;
			if (value > upperLimit)
			    upperLimit = value;
			sumData += value;
			sumSquares += value * value;
			/*	record maximum extents	*/
			if ((chromStart < 0)||(chromStart > chromPosition))
			    chromStart = chromPosition;
			if (chromEnd < (chromPosition + 1))	/* span = 1 */
			    chromEnd = chromPosition + 1;	/* span = 1 */
			++statsCount;
			}
		    }	/*	if (*boolPtr && (!isnan(*fptr)))	*/

		++chromPosition;
		++boolPtr;
		++fptr;
		}
	    /*	if we can save more than 100,000 data locations, move
	     *	the results to a smaller area.  Could be huge savings on
	     *	very sparse result sets.
	     */
	    if (doAscii && ((valuesFound - wigAscii->count) > 100000))
		{
		struct asciiDatum *smallerDataArea;
		size_t newSize;
		newSize = sizeof(struct asciiDatum) * wigAscii->count;

		smallerDataArea = (struct asciiDatum *) needMem(newSize);
		memcpy(smallerDataArea, wigAscii->data, newSize);
		freeMem(wigAscii->data);
		wigAscii->data = smallerDataArea;
		}
	    if (doBed)	/*	there may be one last element	*/
		{
		if (bedElEnd > bedElStart)
		    {
		    struct bed *bedEl;
		    bedEl = bedElement(wDS->chrName,
			    bedElStart, bedElEnd, ++bedElCount);
		    slAddHead(&wDS->bed, bedEl);
		    }
		}
	    /*	there are accumulated stats to complete	*/
	    if (doStats)
		{
		accumStats(wDS, lowerLimit, upperLimit, sumData, sumSquares,
		    statsCount, chromStart, chromEnd);
		resetStats(&lowerLimit, &upperLimit, &sumData, &sumSquares,
		    &statsCount, &chromStart, &chromEnd);
		}


	    verbose(VERBOSE_CHR_LEVEL,
		    "#\tviaBed found: %llu valid data points\n", valuesMatched);
	    }	/*	if ((something found) and (something to do))	*/

	/*	did they want the data array returned ?  No, then release it */
	if (!doDataArray)
	    wDS->freeArray(wDS);
	/*	we are certainly done with the bedArray	*/
	bedFreeList(&filteredBed);
	freeMem(bedArray);
	}	/*	for (chr = chromList; chr; chr = chr->next)	*/

    /*	restore these constraints we used here
     */
    if (chromConstraint)
	wDS->chrName = saveChrName;
    else
	freeMem(wDS->chrName);
    if (positionConstraints)
	wDS->setPositionConstraint(wDS, saveWinStart, saveWinEnd);
    else
	wDS->setPositionConstraint(wDS, 0, 0);

    /*	The bedConstrained flag will provide a printout indication of
     *	what happened to the *Out() routines.
     */
    wDS->bedConstrained = TRUE;	/* to signal the *Out() displays */

    freeHashAndVals(&chromSizes);
    }	/*	if (bedList && *bedList)	*/

return(valuesMatched);
}

static void sortResults(struct wiggleDataStream *wDS)
/*	sort any results that exist	*/
{
if (wDS->bed)
    slSort(&wDS->bed, bedCmp);
if (wDS->ascii)
    slSort(&wDS->ascii, asciiDataCmp);
if (wDS->stats)
    slSort(&wDS->stats, statsDataCmp);
if (wDS->array)
    slSort(&wDS->array, arrayDataCmp);
}

static void bedOut(struct wiggleDataStream *wDS, char *fileName, boolean sort)
/*	print to fileName the bed list */
{
FILE * fh;
fh = mustOpen(fileName, "w");
if (wDS->bed)
    {
    struct bed *bedEl, *next;

    outputIdentification(wDS, fh);
    showConstraints(wDS, fh);

    if (sort)
	slSort(&wDS->bed, bedCmp);

    for (bedEl = wDS->bed; bedEl; bedEl = next )
	{
	fprintf(fh,"%s\t%u\t%u\t%s\n", bedEl->chrom, bedEl->chromStart,
	    bedEl->chromEnd, bedEl->name);
	next = bedEl->next;
	}
    }
else
    {
    showConstraints(wDS, fh);
    fprintf(fh, "#\tbed: no data points found for bed format output\n");
    }
carefulClose(&fh);
}	/*	static void bedOut()	*/

static void statsOut(struct wiggleDataStream *wDS, char *fileName, boolean sort)
/*	print to fileName the statistics */
{
FILE * fh;
fh = mustOpen(fileName, "w");
if (wDS->stats)
    {
    struct wiggleStats *stats, *next;
    int itemsDisplayed = 0;

    if (sort)
	slSort(&wDS->stats, statsDataCmp);

    fprintf (fh, "<TABLE COLS=12 ALIGN=CENTER HSPACE=0><TR><TH COLSPAN=6 ALIGN=CENTER> ");
    if (wDS->db)
	fprintf(fh, "Database: %s </TH><TH COLSPAN=6 ALIGN=CENTER> Table: %s </TH></TR></TABLE>\n", wDS->db, wDS->tblName);
    if (wDS->isFile)
	fprintf(fh, "from file </TH><TH COLSPAN=6 ALIGN=CENTER> Table: %s </TH></TR></TABLE>\n", wDS->tblName);

    fprintf(fh,"<TABLE COLS=12 ALIGN=CENTER HSPACE=0>\n");
    fprintf(fh,"<TR><TH> Chrom </TH><TH> Data <BR> start </TH>");
    fprintf(fh,"<TH> Data <BR> end </TH>");
    fprintf(fh,"<TH> # of Data <BR> values </TH><TH> Data <BR> span </TH>");
    fprintf(fh,"<TH> Bases <BR> covered </TH><TH> Minimum </TH>");
    fprintf(fh,"<TH> Maximum </TH><TH> Range </TH><TH> Mean </TH>");
    fprintf(fh,"<TH> Variance </TH><TH> Standard <BR> deviation </TH></TR>\n");

    for (stats = wDS->stats; stats; stats = next )
	{
	fprintf(fh,"<TR><TH ALIGN=LEFT> %s </TH>\n", stats->chrom);
	fprintf(fh,"\t<TD ALIGN=RIGHT> %u </TD>\n", stats->chromStart+1);
                                        	/* display closed coords */
	fprintf(fh,"\t<TD ALIGN=RIGHT> %u </TD>\n", stats->chromEnd);
	fprintf(fh,"\t<TD ALIGN=RIGHT> %u </TD>\n", stats->count);
	fprintf(fh,"\t<TD ALIGN=RIGHT> %d </TD>\n", stats->span);
	fprintf(fh,"\t<TD ALIGN=RIGHT> %u </TD>\n", stats->count*stats->span);
	fprintf(fh,"\t<TD ALIGN=RIGHT> %g </TD>\n", stats->lowerLimit);
	fprintf(fh,"\t<TD ALIGN=RIGHT> %g </TD>\n", stats->lowerLimit+stats->dataRange);
	fprintf(fh,"\t<TD ALIGN=RIGHT> %g </TD>\n", stats->dataRange);
	fprintf(fh,"\t<TD ALIGN=RIGHT> %g </TD>\n", stats->mean);
	fprintf(fh,"\t<TD ALIGN=RIGHT> %g </TD>\n", stats->variance);
	fprintf(fh,"\t<TD ALIGN=RIGHT> %g </TD>\n", stats->stddev);
	fprintf(fh,"<TR>\n");

	++itemsDisplayed;
	next = stats->next;
	}
    if (!itemsDisplayed)
	fprintf(fh,"<TR><TH ALIGN=CENTER COLSPAN=12> No data found matching this request </TH></TR>");
    fprintf(fh,"</TABLE>\n");
    }
else
    {
    showConstraints(wDS, fh);
    fprintf(fh, "#\tstats: no data points found\n");
    }
carefulClose(&fh);
}	/*	static void statsOut()	*/

static void asciiOut(struct wiggleDataStream *wDS, char *fileName, boolean sort,
	boolean rawDataOut)
/*	print to fileName the ascii data values	*/
{
FILE * fh;
fh = mustOpen(fileName, "w");
if (wDS->ascii)
    {
    struct wigAsciiData *asciiData;
    char *chrom = NULL;
    unsigned span = 0;

    if (sort)
	slSort(&wDS->ascii, asciiDataCmp);

    for (asciiData = wDS->ascii; asciiData; asciiData = asciiData->next)
	{
	unsigned i;
	struct asciiDatum *data;

	if (asciiData->count)
	    {
	    /*	will be true the first time for both reasons	*/
	    if ( (span != asciiData->span) ||
		(chrom && differentString(asciiData->chrom, chrom)) )
		{
		freeMem(chrom);
		chrom = cloneString(asciiData->chrom);
		span = asciiData->span;
		if (!rawDataOut)
		    {
		    showConstraints(wDS, fh);
		    fprintf (fh, "variableStep chrom=%s span=%u\n",
			chrom, span);
		    }
		}
	    data = asciiData->data;
	    for (i = 0; i < asciiData->count; ++i)
		{
		/*	user visible coordinates are 1 relative	*/
		if (rawDataOut)
		    fprintf (fh, "%g\n", data->value);
		else
		    fprintf (fh, "%u\t%g\n", data->chromStart + 1, data->value);
		++data;
		}
	    }
	}
    }
else
    {
    if (!rawDataOut)
	{
	showConstraints(wDS, fh);
	fprintf(fh, "#\tasciiOut: no data points found\n");
	}
    }
carefulClose(&fh);
}	/*	static void asciiOut()	*/

void destroyWigDataStream(struct wiggleDataStream **wDS)
/*	free all structures and zero the callers' structure pointer	*/
{
if (wDS)
    {
    struct wiggleDataStream *wds;
    wds=*wDS;
    if (wds)
	{
	closeWigConn(wds);
	wds->freeAscii(wds);
	wds->freeBed(wds);
	wds->freeStats(wds);
	wds->freeArray(wds);
	wds->freeConstraints(wds);
	freeMem(wds->currentChrom);
	}
    freez(wDS);
    }
}

struct wiggleDataStream *newWigDataStream()
{
struct wiggleDataStream *wds;

AllocVar(wds);
/*	everything is zero which is good since that is NULL for all the
 *	strings and lists.  A few items should have some initial values
 *	which are not necessarily NULL
 */
wds->isFile = FALSE;
wds->useDataConstraint = FALSE;
wds->wibFH = -1;
wds->limit_0 = -1 * INFINITY;
wds->limit_1 = INFINITY;
wds->wigCmpSwitch = wigNoOp_e;
wds->freeConstraints = freeConstraints;
wds->freeAscii = freeAscii;
wds->freeBed = freeBed;
wds->freeStats = freeStats;
wds->freeArray = freeArray;
wds->setPositionConstraint = setPositionConstraint;
wds->setChromConstraint = setChromConstraint;
wds->setSpanConstraint = setSpanConstraint;
wds->setDataConstraint = setDataConstraint;
wds->bedOut = bedOut;
wds->statsOut = statsOut;
wds->asciiOut = asciiOut;
wds->sortResults = sortResults;
wds->getDataViaBed = getDataViaBed;
wds->getData = getData;
return wds;
}

