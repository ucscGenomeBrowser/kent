/*	wigDataStream - an object to access wiggle data values from
 *	either a DB access or from a .wig text file (==custom track)
 */
#include "common.h"
#include "wiggle.h"

static char const rcsid[] = "$Id: wigDataStream.c,v 1.8 2004/08/09 23:06:06 hiram Exp $";

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

/*	PUBLIC	METHODS   **************************************************/

static void setPositionConstraint(struct wiggleDataStream *wDS,
	int winStart, int winEnd)
/*	both 0 means no constraint	*/
{
if ((!wDS->isFile) && wDS->conn)
    {
    errAbort("setPositionConstraint: need to openWigConn() before setting winStart, winEnd");
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

static void getData(struct wiggleDataStream *wDS, enum wigDataFetchType type)
/* getData - read and return wiggle data	*/
{
char *row[WIGGLE_NUM_COLS];
unsigned long rowCount = 0;
unsigned long long validData = 0;
unsigned long long valuesMatched = 0;
unsigned long long noDataBytes = 0;
unsigned long long bytesSkipped = 0;
boolean doStats = FALSE;
boolean doBed = FALSE;
boolean doAscii = FALSE;
boolean skipDataRead = FALSE;	/*	may need this later	*/

doAscii = type & wigFetchAscii;
doBed = type & wigFetchBed;
doStats = type & wigFetchStats;

if (! (doAscii || doBed || doStats) )
    {
	verbose(2, "wigGetData: no type of data fetch requested ?\n");
	return;	/*	NOTHING ASKED FOR	*/
    }

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
		continue;
	if (wDS->spanLimit)
	    if (wDS->spanLimit != wiggle->span)
		continue;
	if (wDS->winEnd)	/* non-zero means a range is in effect */
	    if ( (wiggle->chromStart > wDS->winEnd) ||
		(wiggle->chromEnd < wDS->winStart) )
		continue;	/* entire block is out of range */
	}

    chromPosition = wiggle->chromStart;

    /*	this will be true the very first time for both reasons	*/
    if ( (wDS->currentSpan != wiggle->span) || 
	    (wDS->currentChrom &&
		differentString(wDS->currentChrom, wiggle->chrom)))
	{
	freeMem(wDS->currentChrom);
	wDS->currentChrom = cloneString(wiggle->chrom);
	wDS->currentSpan = wiggle->span;
	}

    /*	it is a bit inefficient to make one of these for each SQL row,
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
	    continue;
	    }
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
		    {
		    if ( (chromPosition < wDS->winStart) ||
			(chromPosition >= wDS->winEnd) )
			{
			++datum;
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
		    ++valuesMatched;
		    if (doAscii)
			{
			asciiOut->value =
		BIN_TO_VALUE(*datum,wiggle->lowerLimit,wiggle->dataRange);
			asciiOut->chromStart = chromPosition;
			++asciiOut;
			++wigAscii->count;
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

if (doAscii)
    slReverse(&wDS->ascii);

wDS->rowsRead += rowCount;
wDS->validPoints += validData;
wDS->valuesMatched += valuesMatched;
wDS->noDataPoints += noDataBytes;
wDS->bytesSkipped += bytesSkipped;

}	/*	void getData()	*/

static void asciiOut(struct wiggleDataStream *wDS, char *fileName)
/*	print to fileName the ascii data values	*/
{
FILE * fh;
fh = mustOpen(fileName, "w");
if (wDS->ascii)
    {
    struct wigAsciiData *asciiData;
    struct wigAsciiData *next;
    char *chrom = NULL;
    unsigned span = 0;

    for (asciiData = wDS->ascii; asciiData; asciiData = next )
	{
	unsigned i;
	struct asciiDatum *data;

	/*	will be true the first time for both reasons	*/
	if ( (span != asciiData->span) ||
	    (chrom && differentString(asciiData->chrom, chrom)) )
	    {
	    freeMem(chrom);
	    chrom = cloneString(asciiData->chrom);
	    span = asciiData->span;
	    if (wDS->useDataConstraint)
		{
		fprintf (fh, "#\tdata constraint in use: %s\n",
			wDS->dataConstraint );
		if ((wDS->dataConstraint) &&
		    sameWord(wDS->dataConstraint,"in range"))
			fprintf (fh, "#\tdata values in range [%g : %g]\n",
				wDS->limit_0, wDS->limit_1);
		else
			fprintf (fh, "#\tdata values %s %g\n",
				wDS->dataConstraint, wDS->limit_0);
		}
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
	next = asciiData->next;
	}

    }
else
    {
    fprintf(fh, "#\tno data points found\n");
    }
carefulClose(&fh);
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
freez(&wDS->sqlConstraint);
}

static void openWigConn(struct wiggleDataStream *wDS, char *tableName)
/*	open connection to db or to a file, prepare SQL result for db */
{
if (wDS->isFile)
    {
    struct dyString *fileName = dyStringNew(256);
    lineFileClose(&wDS->lf);	/*	possibly a previous file */
    dyStringPrintf(fileName, "%s.wig", tableName);
    wDS->lf = lineFileOpen(fileName->string, TRUE);
    dyStringFree(&fileName);
    }
else
    {
    struct dyString *query = dyStringNew(256);
    dyStringPrintf(query, "select * from %s", tableName);
    if (wDS->chrName)
	addConstraint(wDS, "chrom =", wDS->chrName);
    if (wDS->spanLimit)
	{
	struct dyString *dyTmp = dyStringNew(256);
	dyStringPrintf(dyTmp, "%u", wDS->spanLimit);
	addConstraint(wDS, "span =", dyTmp->string);
	dyStringFree(&dyTmp);
	}
    if (wDS->sqlConstraint)
	dyStringPrintf(query, " where (%s) order by chromStart",
	    wDS->sqlConstraint);
    verbose(2, "#\t%s\n", query->string);
    if (!wDS->conn)
	wDS->conn = sqlConnect(wDS->db);
    wDS->sr = sqlGetResult(wDS->conn,query->string);
    }
freeMem(wDS->tblName);
wDS->tblName = cloneString(tableName);
}

void destroyWigDataStream(struct wiggleDataStream **wDS)
/*	free all structures and zero the callers' structure pointer	*/
{
if (wDS)
    {
    struct wiggleDataStream *wds;
    wds=*wDS;
    if (wds)
	{
	wds->closeWigConn(wds);
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
wds->asciiOut = asciiOut;
wds->getData = getData;
wds->closeWigConn = closeWigConn;
wds->openWigConn = openWigConn;
return wds;
}

