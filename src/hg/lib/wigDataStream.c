/*	wigDataStream - an object to access wiggle data values from
 *	either a DB access or from a .wig text file (==custom track)
 */
#include "common.h"
#include "wiggle.h"

static char const rcsid[] = "$Id: wigDataStream.c,v 1.16 2004/08/12 20:59:01 hiram Exp $";

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
    verbose(3, "#\tnumCols = %d, row[0]: %s, row[1]: %s, row[%d]: %s\n",
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

verbose(3, "#\twigSetCompareByte: [%g : %g] becomes [%d : %d]\n",
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

    verbose(2, "#\t%s\n", query->string);
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
{
bedFreeList(&wDS->bed);
}

static void freeStats(struct wiggleDataStream *wDS)
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
verbose(2, "#\twigSetCompareFunctions: set to '%s'\n", wDS->dataConstraint);
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

static void getData(struct wiggleDataStream *wDS, char *db, char *table,
	 int operations)
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

doAscii = operations & wigFetchAscii;
doBed = operations & wigFetchBed;
doStats = operations & wigFetchStats;
doNoOp = operations & wigFetchNoOp;

/*	for timing purposes, allow the wigFetchNoOp to go through */
if (doNoOp)
    {
    doBed = doStats = doAscii = FALSE;
    }

if (! (doNoOp || doAscii || doBed || doStats) )
    {
	verbose(2, "getData: no type of data fetch requested ?\n");
	return;	/*	NOTHING ASKED FOR ?	*/
    }

setDbTable(wDS, db, table);
openWigConn(wDS);

if (doAscii)
    summaryOnly = FALSE;
if (doBed)
    summaryOnly = FALSE;
if (!wDS->isFile && wDS->winEnd)
    summaryOnly = FALSE;

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
	if (wDS->chrName)
	    if (differentString(wDS->chrName,wiggle->chrom))
		{
		wiggleFree(&wiggle);
		continue;	/*	next SQL row	*/
		}
	if (wDS->spanLimit)
	    if (wDS->spanLimit != wiggle->span)
		{
		wiggleFree(&wiggle);
		continue;	/*	next SQL row	*/
		}
	if (wDS->winEnd)	/* non-zero means a range is in effect */
	    if ( (wiggle->chromStart >= wDS->winEnd) ||
		(wiggle->chromEnd < wDS->winStart) )
		{		/* entire block is out of range */
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

    verbose(3, "#\trow: %llu, start: %u, data range: %g: [%g:%g]\n",
	    rowCount, wiggle->chromStart, wiggle->dataRange,
	    wiggle->lowerLimit, wiggle->lowerLimit+wiggle->dataRange);
    verbose(3, "#\tresolution: %g per bin\n",
	    wiggle->dataRange/(double)MAX_WIG_VALUE);
    if (wDS->useDataConstraint)
	{
	boolean takeIt = FALSE;
	switch (wDS->wigCmpSwitch)
	    {
		default:
		    errAbort("wig_getData: illegal wigCmpSwitch ? %#x",
			wDS->wigCmpSwitch);
		    break;
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
	    }	/*	switch (wDS->wigCmpSwitch)	*/
	if (takeIt)
	    setCompareByte(wDS, wiggle->lowerLimit, wiggle->dataRange);
	else
	    {
	    bytesSkipped += wiggle->count;
	    wiggleFree(&wiggle);
	    continue;	/*	next SQL row	*/
	    }
	}
    if (summaryOnly && doStats)
	{
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
	int j;	/*	loop counter through ReadData	*/
	unsigned char *datum;    /* to walk through ReadData bytes */
	unsigned char *ReadData;    /* the bytes read in from the file */

	openWibFile(wDS, wiggle->file);
		    /* possibly open a new wib file */
	ReadData = (unsigned char *) needMem((size_t) (wiggle->count + 1));
	wDS->bytesRead += wiggle->count;
	lseek(wDS->wibFH, wiggle->offset, SEEK_SET);
	read(wDS->wibFH, ReadData,
	    (size_t) wiggle->count * (size_t) sizeof(unsigned char));

    verbose(3, "#\trow: %llu, reading: %u bytes\n", rowCount, wiggle->count);

	datum = ReadData;
	for (j = 0; j < wiggle->count; ++j)
	    {
	    if (*datum != WIG_NO_DATA)
		{
		boolean takeIt = FALSE;

		if (wDS->winEnd)  /* non-zero means a range is in effect */
		    {	/*	do not allow item (+span) to run over winEnd */
		    if ( (chromPosition < wDS->winStart) ||
			((chromPosition+wiggle->span) > wDS->winEnd) )
			{
			++datum;	/*	out of range	*/
			chromPosition += wiggle->span;
			continue;	/*	next *datum	*/
			}
		    }
		++validData;
		switch (wDS->wigCmpSwitch)
		    {
		    default:
			errAbort("wig_getData: illegal wigCmpSwitch ? %#x",
			    wDS->wigCmpSwitch);
			break;
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
		    }	/*	switch (wDS->wigCmpSwitch)	*/
		if (takeIt)
		    {
		    float value = 0.0;
		    ++valuesMatched;
		    if (doAscii || doStats)
			value =
		BIN_TO_VALUE(*datum,wiggle->lowerLimit,wiggle->dataRange);
		    if (doAscii)
			{
			asciiOut->value = value;
			asciiOut->chromStart = chromPosition;
			++asciiOut;
			++wigAscii->count;
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
	freeMem(ReadData);
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
}	/*	void getData()	*/

static void getDataViaBed(struct wiggleDataStream *wDS, char *db, char *table,
	 int operations, struct bed **bedList)
/* getDataViaBed - constrained by the bedList	*/
{
struct bed *el;

if (bedList && *bedList)
    {
    boolean chromConstraint = ((char *)NULL != wDS->chrName);
    boolean positionConstraints = (0 != wDS->winEnd);
    boolean doStats = FALSE;
    boolean doBed = FALSE;
    boolean doAscii = FALSE;
    boolean doNoOp = FALSE;
    int saveWinStart = 0;
    int saveWinEnd = 0;

    if (positionConstraints)
	{
	saveWinStart = wDS->winStart;
	saveWinEnd = wDS->winEnd;
	}
    doAscii = operations & wigFetchAscii;
    doBed = operations & wigFetchBed;
    doStats = operations & wigFetchStats;
    doNoOp = operations & wigFetchNoOp;

    /*	for timing purposes, allow the wigFetchNoOp to go through */
    if (doNoOp)
	{
	doBed = doStats = doAscii = FALSE;
	}
    if (! (doNoOp || doAscii || doBed || doStats) )
	{
	    verbose(2, "getDataViaBed: no type of data fetch requested ?\n");
	    return;	/*	NOTHING ASKED FOR ?	*/
	}

    wDS->bedConstrained = TRUE;	/* to signal the *Out() displays */
    for (el = *bedList; el; el = el->next)
	{
	/*	if chrom constraint is in effect, allow it to filter the
	 *	bed file.  Otherwise the bed file is the filter.
	 */
	if (chromConstraint)
	    {
	    if (differentString(wDS->chrName,el->chrom))
		continue;
	    }
	else
	    wDS->setChromConstraint(wDS, el->chrom);
	/*	if position constraints are in effect, allow them to
	 *	filter the bed file.  Otherwise the bed file is the filter.
	 */
	if (positionConstraints)
	    {
	    int winStart;
	    int winEnd;
	    if ( (el->chromStart >= saveWinEnd) ||
		(el->chromEnd < saveWinStart))
		continue;
	    winStart = max(saveWinStart, el->chromStart);
	    winEnd = min(saveWinEnd, el->chromEnd);
	    wDS->setPositionConstraint(wDS, winStart, winEnd);
	    }
	else
	    wDS->setPositionConstraint(wDS, el->chromStart, el->chromEnd);
	wDS->getData(wDS, db, table, operations);
	}
    /*	restore these constraints we used here
     *	The bedConstrained flag will provide a printout indication of
     *	what happened to the *Out() routines.
     */
    if (!chromConstraint)
	freeMem(wDS->chrName);
    if (positionConstraints)
	wDS->setPositionConstraint(wDS, saveWinStart, saveWinEnd);
    else
	wDS->setPositionConstraint(wDS, 0, 0);
    /*	sorting is definately needed after this business, the caller can
     *	call the sortResults() method, or allow the *Out() methods to do
     *	that.
     */
    }
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
    fprintf(fh, "#\tno data points found for bed format output\n");
    }
carefulClose(&fh);
}

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
    fprintf(fh, "#\tno data points found\n");
    }
carefulClose(&fh);
}	/*	static void statsOut()	*/

static void asciiOut(struct wiggleDataStream *wDS, char *fileName, boolean sort)
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
		showConstraints(wDS, fh);
		fprintf (fh, "variableStep chrom=%s span=%u\n",
		    chrom, span);
		}
	    data = asciiData->data;
	    for (i = 0; i < asciiData->count; ++i)
		{
		/*	user visible coordinates are 1 relative	*/
		fprintf (fh, "%u\t%g\n", data->chromStart + 1, data->value);
		++data;
		}
	    }
	}
    }
else
    {
    showConstraints(wDS, fh);
    fprintf(fh, "#\tno data points found\n");
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

