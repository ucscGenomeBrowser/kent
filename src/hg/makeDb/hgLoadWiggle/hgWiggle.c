/* hgWiggle - fetch wiggle data from database or .wig file */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "dystring.h"
#include "wiggle.h"
#include "hdb.h"
#include "portable.h"

static char const rcsid[] = "$Id: hgWiggle.c,v 1.7 2004/08/03 23:21:49 hiram Exp $";

/* Command line switches. */
static boolean silent = FALSE;	/*	no data points output */
static boolean timing = FALSE;	/*	turn timing on	*/
static boolean skipDataRead = FALSE;	/*	do not read the wib data */
static char *dataConstraint;	/*	one of < = >= <= == != 'in range' */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"chr", OPTION_STRING},
    {"dataConstraint", OPTION_STRING},
    {"silent", OPTION_BOOLEAN},
    {"timing", OPTION_BOOLEAN},
    {"skipDataRead", OPTION_BOOLEAN},
    {"span", OPTION_INT},
    {"ll", OPTION_FLOAT},
    {"ul", OPTION_FLOAT},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgWiggle - fetch wiggle data from data base or file\n"
  "usage:\n"
  "   hgWiggle [options] <track names ...>\n"
  "options:\n"
  "   -db=<database> - use specified database\n"
  "   -chr=chrN - examine data only on chrN\n"
  "   -silent - no output, scanning data only\n"
  "   -timing - display timing statistics\n"
  "   -skipDataRead - do not read the .wib data (for no-read speed check)\n"
  "   -dataConstraint='DC' - where DC is one of < = >= <= == != 'in range'\n"
  "   -ll=<F> - lowerLimit compare data values to F (float) (all but 'in range')\n"
  "   -ul=<F> - upperLimit compare data values to F (float)\n\t\t(need both ll and ul when 'in range')\n"
  "   When no database is specified, track names will refer to .wig files\n"
  "   example using the file gc5Base.wig:\n"
  "                hgWiggle -chr=chrM gc5Base\n"
  "   example using the database table hg17.gc5Base:\n"
  "                hgWiggle -chr=chrM -db=hg17 gc5Base"
  );
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
return wds;
}

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

static void addChromConstraint(struct wiggleDataStream *wDS, char *chr)
{
addConstraint(wDS, "chrom =", chr);
freeMem(wDS->chrName);
wDS->chrName = cloneString(chr);
}

static void addSpanConstraint(struct wiggleDataStream *wDS, unsigned span)
{
struct dyString *dyTmp = dyStringNew(256);
dyStringPrintf(dyTmp, "%u", span);
addConstraint(wDS, "span =", dyTmp->string);
wDS->spanLimit = span;
dyStringFree(&dyTmp);
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
static boolean wigInRange_D(struct wiggleDataStream *wDS, double lower,
	double upper)
{
return ((lower <= wDS->limit_1) && (upper >= wDS->limit_0));
}
/* LessThan means:  the row begins before the limit_0 (value<limit_0)
 *	i.e. there are data values below the specified limit_0
 */
static boolean wigLessThan_D(struct wiggleDataStream *wDS, double value,
	double dummy)
{
return (value < wDS->limit_0);
}
/* LessEqual means:  the row begins at or before the limit_0 (value<=limit_0)
 *	i.e. there are data values at or below the specified limit_0
 */
static boolean wigLessEqual_D(struct wiggleDataStream *wDS, double value,
	double dummy)
{
return (value <= wDS->limit_0);
}
/* Equal means:  similar to InRange, the test value limit_0 can be found
 * in the SQL row, i.e. lower <= limit_0 <= upper
 */
static boolean wigEqual_D(struct wiggleDataStream *wDS, double lower,
	double upper)
{
return ((lower <= wDS->limit_0) && (wDS->limit_0 <= upper));
}
/* NotEqual means:  the opposite of Equal, the test value limit_0 can not
 *	be found in the SQL row, i.e. (limit_0 < lower) or (upper < limit_0)
 */
static boolean wigNotEqual_D(struct wiggleDataStream *wDS, double lower,
	double upper)
{
return ((wDS->limit_0 < lower) || (upper < wDS->limit_0));
}
/* GreaterEqual means:  the row ends at or after the limit_0 (limit_0<=upper)
 *	i.e. there are data values at or above the specified limit_0
 */
static boolean wigGreaterEqual_D(struct wiggleDataStream *wDS, double dummy,
	double upper)
{
return (wDS->limit_0 <= upper);
}
/* GreaterEqual means:  the row ends after the limit_0 (limit_0<upper)
 *	i.e. there are data values above the specified limit_0
 */
static boolean wigGreaterThan_D(struct wiggleDataStream *wDS, double dummy,
	double upper)
{
return (wDS->limit_0 < upper);
}
/*	the unsigned char comparison functions
 *	Unlike the above, these are straighforward, just compare the
 *	byte values
 */
static boolean wigInRange(struct wiggleDataStream *wDS, unsigned char *value)
{
return ((*value <= wDS->ucUpperLimit) && (*value >= wDS->ucLowerLimit));
}
static boolean wigLessThan(struct wiggleDataStream *wDS, unsigned char *value)
{
return (*value < wDS->ucLowerLimit);
}
static boolean wigLessEqual(struct wiggleDataStream *wDS, unsigned char *value)
{
return (*value <= wDS->ucLowerLimit);
}
static boolean wigEqual(struct wiggleDataStream *wDS, unsigned char *value)
{
return (*value == wDS->ucLowerLimit);
}
static boolean wigNotEqual(struct wiggleDataStream *wDS, unsigned char *value)
{
return (*value != wDS->ucLowerLimit);
}
static boolean wigGreaterEqual(struct wiggleDataStream *wDS, unsigned char *value)
{
return (*value >= wDS->ucLowerLimit);
}
static boolean wigGreaterThan(struct wiggleDataStream *wDS, unsigned char *value)
{
return (*value > wDS->ucLowerLimit);
}


static void wigSetCompareFunctions(struct wiggleDataStream *wDS)
{
if (!wDS->dataConstraint)
    return;

if (sameWord(wDS->dataConstraint,"<"))
    {
    wDS->cmpDouble = wigLessThan_D;
    wDS->cmpByte = wigLessThan;
    }
else if (sameWord(wDS->dataConstraint,"<="))
    {
    wDS->cmpDouble = wigLessEqual_D;
    wDS->cmpByte = wigLessEqual;
    }
else if (sameWord(wDS->dataConstraint,"="))
    {
    wDS->cmpDouble = wigEqual_D;
    wDS->cmpByte = wigEqual;
    }
else if (sameWord(wDS->dataConstraint,"!="))
    {
    wDS->cmpDouble = wigNotEqual_D;
    wDS->cmpByte = wigNotEqual;
    }
else if (sameWord(wDS->dataConstraint,">="))
    {
    wDS->cmpDouble = wigGreaterEqual_D;
    wDS->cmpByte = wigGreaterEqual;
    }
else if (sameWord(wDS->dataConstraint,">"))
    {
    wDS->cmpDouble = wigGreaterThan_D;
    wDS->cmpByte = wigGreaterThan;
    }
else if (sameWord(wDS->dataConstraint,"in range"))
    {
    wDS->cmpDouble = wigInRange_D;
    wDS->cmpByte = wigInRange;
    }
else
    errAbort("wigSetCompareFunctions: unknown constraint: '%s'",
	wDS->dataConstraint);
verbose(2, "#\twigSetCompareFunctions: set to '%s'\n", wDS->dataConstraint);
}

static void wigSetDataConstraint(struct wiggleDataStream *wDS,
	char *dataConstraint, double lowerLimit, double upperLimit)
{
wDS->dataConstraint = cloneString(dataConstraint);
if (lowerLimit < upperLimit)
    {
    wDS->limit_0 = lowerLimit;
    wDS->limit_1 = upperLimit;
    }
else if (!(upperLimit < lowerLimit))
  errAbort("wigSetDataConstraint: upper and lower limits are equal: %.g == %.g",
	lowerLimit, upperLimit);
else
    {
    wDS->limit_0 = upperLimit;
    wDS->limit_1 = lowerLimit;
    }
wigSetCompareFunctions(wDS);
wDS->useDataConstraint = TRUE;
}

static void wigSetCompareByte(struct wiggleDataStream *wDS,
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
verbose(2, "#\twigSetCompareByte: [%g : %g] becomes [%d : %d]\n",
	lower, lower+range, wDS->ucLowerLimit, wDS->ucUpperLimit);
}

static void closeWibFile(struct wiggleDataStream *wDS)
/*	if there is a Wib file open, close it	*/
{
if (wDS->wibFH > 0)
    close(wDS->wibFH);
wDS->wibFH = -1;
freez(&wDS->wibFile);
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

static void closeWigConn(struct wiggleDataStream *wDS)
{
closeWibFile(wDS);	/*	closes only if it is open	*/
if (wDS->conn)
    {
    sqlFreeResult(&wDS->sr);
    sqlDisconnect(&wDS->conn);
    }
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
    if (wDS->sqlConstraint)
	dyStringPrintf(query, " where (%s) order by chromStart",
	    wDS->sqlConstraint);
    verbose(2, "#\t%s\n", query->string);
    if (!wDS->conn)
	wDS->conn = sqlConnect(wDS->db);
    wDS->sr = sqlGetResult(wDS->conn,query->string);
    }
}

static boolean wigNextRow(struct wiggleDataStream *wDS, char *row[],
	int maxRow)
/*	read next wig row from sql query or lineFile	*/
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

static void hgWiggle(struct wiggleDataStream *wDS, int trackCount,
	char *tracks[])
/* hgWiggle - dump wiggle data from database or .wig file */
{
int i;
struct wiggle *wiggle;
unsigned long long totalValidData = 0;
unsigned long fileNoDataBytes = 0;
long fileEt = 0;
unsigned long long totalRows = 0;
unsigned long long valuesMatched = 0;
unsigned long long bytesRead = 0;
unsigned long long bytesSkipped = 0;


verbose(2, "#\texamining tracks:");
for (i=0; i<trackCount; ++i)
    verbose(2, " %s", tracks[i]);
verbose(2, "\n");

for (i=0; i<trackCount; ++i)
    {
    char *row[WIGGLE_NUM_COLS];
    unsigned long long rowCount = 0;
    unsigned long long chrRowCount = 0;
    long startClock = clock1000();
    long endClock;
    long chrStartClock = clock1000();
    long chrEndClock;
    unsigned long long validData = 0;
    unsigned long chrBytesRead = 0;
    unsigned long noDataBytes = 0;
    unsigned long chrNoDataBytes = 0;

    openWigConn(wDS, tracks[i]);
    while (wigNextRow(wDS, row, WIGGLE_NUM_COLS))
	{
	++rowCount;
	++chrRowCount;
	wiggle = wiggleLoad(row);
	if (wDS->isFile)
	    {
	    if (wDS->chrName)
		if (differentString(wDS->chrName,wiggle->chrom))
		    continue;
	    if (wDS->spanLimit)
		if (wDS->spanLimit != wiggle->span)
		    continue;
	    }

	if ( (wDS->currentSpan != wiggle->span) || 
		(wDS->currentChrom && differentString(wDS->currentChrom, wiggle->chrom)))
	    {
	    if (wDS->currentChrom && differentString(wDS->currentChrom, wiggle->chrom))
		{
		long et;
		chrEndClock = clock1000();
		et = chrEndClock - chrStartClock;
		if (timing)
 verbose(1,"#\t%s.%s %lu data bytes, %lu no-data bytes, %ld ms, %llu rows\n",
		    tracks[i], wDS->currentChrom, chrBytesRead, chrNoDataBytes, et,
			chrRowCount);
    		chrRowCount = chrNoDataBytes = chrBytesRead = 0;
		chrStartClock = clock1000();
		}
	    freeMem(wDS->currentChrom);
	    wDS->currentChrom=cloneString(wiggle->chrom);
	    if (!silent)
		printf ("variableStep chrom=%s", wDS->currentChrom);
	    wDS->currentSpan = wiggle->span;
	    if (!silent && (wDS->currentSpan > 1))
		printf (" span=%u\n", wDS->currentSpan);
	    else if (!silent)
		printf ("\n");
	    }

	verbose(3, "#\trow: %llu, start: %u, data range: %g: [%g:%g]\n",
		rowCount, wiggle->chromStart, wiggle->dataRange,
		wiggle->lowerLimit, wiggle->lowerLimit+wiggle->dataRange);
	verbose(3, "#\tresolution: %g per bin\n",
		wiggle->dataRange/(double)MAX_WIG_VALUE);
	if (wDS->useDataConstraint)
	    {
	    if (!wDS->cmpDouble(wDS, wiggle->lowerLimit,
		    (wiggle->lowerLimit + wiggle->dataRange)))
		{
		bytesSkipped += wiggle->count;
		continue;
		}
	    wigSetCompareByte(wDS, wiggle->lowerLimit, wiggle->dataRange);
	    }
	if (!skipDataRead)
	    {
	    int j;	/*	loop counter through ReadData	*/
	    unsigned char *datum;    /* to walk through ReadData bytes */
	    unsigned char *ReadData;    /* the bytes read in from the file */
	    openWibFile(wDS, wiggle->file);/* possibly open a new wib file */
	    ReadData = (unsigned char *) needMem((size_t) (wiggle->count + 1));
	    bytesRead += wiggle->count;
	    lseek(wDS->wibFH, wiggle->offset, SEEK_SET);
	    read(wDS->wibFH, ReadData,
		(size_t) wiggle->count * (size_t) sizeof(unsigned char));

    verbose(3, "#\trow: %llu, reading: %u bytes\n", rowCount, wiggle->count);

	    datum = ReadData;
	    for (j = 0; j < wiggle->count; ++j)
		{
		if (*datum != WIG_NO_DATA)
		    {
		    ++validData;
		    ++chrBytesRead;
		    if (wDS->useDataConstraint)
			{
			if (wDS->cmpByte(wDS, datum))
			    {
			    ++valuesMatched;
			    if (!silent)
				{
				double datumOut =
 wiggle->lowerLimit+(((double)*datum/(double)MAX_WIG_VALUE)*wiggle->dataRange);
				if (verboseLevel() > 2)
				    printf("%d\t%g\t%d\n", 1 +
					wiggle->chromStart + (j * wiggle->span),
						datumOut, *datum);
				else
				    printf("%d\t%g\n", 1 +
					wiggle->chromStart + (j * wiggle->span),
						datumOut);
				}
			    }
			}
		    else
			{
			if (!silent)
			    {
			    double datumOut =
 wiggle->lowerLimit+(((double)*datum/(double)MAX_WIG_VALUE)*wiggle->dataRange);
			    if (verboseLevel() > 2)
				printf("%d\t%g\t%d\n",
				  1 + wiggle->chromStart + (j * wiggle->span),
					    datumOut, *datum);
			    else
				printf("%d\t%g\n",
				  1 + wiggle->chromStart + (j * wiggle->span),
					    datumOut);
			    }
			}
		    }
		else
		    {
		    ++noDataBytes;
		    ++chrNoDataBytes;
		    }
		++datum;
		}
	    freeMem(ReadData);
	    }	/*	if (!skipDataRead)	*/
	wiggleFree(&wiggle);
	}	/*	while (wigNextRow())	*/
    endClock = clock1000();
    totalRows += rowCount;
    if (timing)
	{
	long et;
	chrEndClock = clock1000();
	et = chrEndClock - chrStartClock;
	/*	file per chrom output already happened above except for
	 *	the last one */
	if (!wDS->isFile || (wDS->isFile && ((i+1)==trackCount)))
 verbose(1,"#\t%s.%s %lu data bytes, %lu no-data bytes, %ld ms, %llu rows, %llu matched\n",
		    tracks[i], wDS->currentChrom, chrBytesRead, chrNoDataBytes, et,
			rowCount, valuesMatched);
    	chrNoDataBytes = chrBytesRead = 0;
	chrStartClock = clock1000();
	et = endClock - startClock;
	fileEt += et;
 verbose(1,"#\t%s %llu valid bytes, %lu no-data bytes, %ld ms, %llu rows, %llu matched\n",
	tracks[i], validData, noDataBytes, et, totalRows, valuesMatched);
	}
    totalValidData += validData;
    fileNoDataBytes += noDataBytes;
    }	/*	for (i=0; i<trackCount; ++i)	*/

if (wDS->isFile)
    {
    lineFileClose(&wDS->lf);
    if (timing)
 verbose(1,"#\ttotal %llu valid bytes, %lu no-data bytes, %ld ms, %llu rows\n#\t%llu matched = %% %.2f, wib bytes: %llu, bytes skipped: %llu\n",
	totalValidData, fileNoDataBytes, fileEt, totalRows, valuesMatched,
	100.0 * (float)valuesMatched / (float)totalValidData, bytesRead, bytesSkipped);
    }

closeWigConn(wDS);

}	/*	void hgWiggle()	*/


int main(int argc, char *argv[])
/* Process command line. */
{
struct wiggleDataStream *wDS = NULL;
float lowerLimit = -1 * INFINITY;  /*	for constraint comparison	*/
float upperLimit = INFINITY;	/*	for constraint comparison	*/
unsigned span = 0;	/*	select for this span only	*/
char *chr = NULL;		/* work on this chromosome only */

optionInit(&argc, argv, optionSpecs);

wDS = newWigDataStream();

wDS->db = optionVal("db", NULL);
chr = optionVal("chr", NULL);
dataConstraint = optionVal("dataConstraint", NULL);
silent = optionExists("silent");
timing = optionExists("timing");
skipDataRead = optionExists("skipDataRead");
span = optionInt("span", 0);
lowerLimit = optionFloat("ll", -1 * INFINITY);
upperLimit = optionFloat("ul", INFINITY);

if (wDS->db)
    {
    wDS->isFile = FALSE;
    verbose(2, "#\tdatabase: %s\n", wDS->db);
    }
else
    {
    wDS->isFile = TRUE;
    verbose(2, "#\tno database specified, using .wig files\n");
    }
if (chr)
    {
    addChromConstraint(wDS, chr);
    verbose(2, "#\tchrom constraint: (%s)\n", wDS->sqlConstraint);
    }
if (silent)
    verbose(2, "#\tsilent option on, no data points output\n");
if (timing)
    verbose(2, "#\ttiming option on\n");
if (skipDataRead)
    verbose(2, "#\tskipDataRead option on, do not read .wib data\n");
if (span)
    {
    addSpanConstraint(wDS, span);
    verbose(2, "#\tspan constraint: (%s)\n", wDS->sqlConstraint);
    }
if (dataConstraint)
    {
    if (sameString(dataConstraint, "in range"))
	{
	if (!(optionExists("ll") && optionExists("ul")))
	    {
	    warn("ERROR: dataConstraint 'in range' specified without both -ll=<F> and -ul=<F>");
	    usage();
	    }
	}
    else if (!optionExists("ll"))
	{
	warn("ERROR: dataConstraint specified without -ll=<F>");
	usage();
	}

    wigSetDataConstraint(wDS, dataConstraint, lowerLimit, upperLimit);

    if (sameString(dataConstraint, "in range"))
	verbose(2, "#\tdataConstraint: %s [%f : %f]\n", wDS->dataConstraint,
		wDS->limit_0, wDS->limit_1);
    else
	verbose(2, "#\tdataConstraint: data values %s %f\n",
		wDS->dataConstraint, wDS->limit_0);
    }
else if (optionExists("ll") || optionExists("ul"))
    {
    warn("ERROR: ll or ul options specified without -dataConstraint");
    usage();
    }

if (argc < 2)
    usage();

hgWiggle(wDS, argc-1, argv+1);
return 0;
}
