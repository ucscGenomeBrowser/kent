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

static char const rcsid[] = "$Id: hgWiggle.c,v 1.6 2004/07/30 22:40:35 hiram Exp $";

/* Command line switches. */
static char *db = NULL;		/* database to read from */
static char *chr = NULL;		/* work on this chromosome only */
static boolean silent = FALSE;	/*	no data points output */
static boolean timing = FALSE;	/*	turn timing on	*/
static boolean skipDataRead = FALSE;	/*	do not read the wib data */
static unsigned span = 0;	/*	select for this span only	*/
static float dataValue = 0.0;	/*	dataValue to compare	*/

/*	global flags	*/
static boolean file = FALSE;	/*	no database specified, use file */
static struct lineFile *wigFile = NULL;
static boolean useDataValue = FALSE;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"chr", OPTION_STRING},
    {"silent", OPTION_BOOLEAN},
    {"timing", OPTION_BOOLEAN},
    {"skipDataRead", OPTION_BOOLEAN},
    {"span", OPTION_INT},
    {"dataValue", OPTION_FLOAT},
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
  "   -skipDataRead - do not read the .wib data\n"
  "   -dataValue=<F> - compare data values to F (float) \n"
  "   When no database is specified, track names will refer to .wig files\n"
  "   example using the file gc5Base.wig:\n"
  "                hgWiggle -chr=chrM gc5Base\n"
  "   example using the database table hg17.gc5Base:\n"
  "                hgWiggle -chr=chrM -db=hg17 gc5Base"
  );
}

static boolean wigNextRow(struct sqlResult *sr, struct lineFile *lf,
	char *row[], int maxRow)
/*	read next wig row from sql query or lineFile	*/
{
int numCols;

if (file)
    {
    numCols = lineFileChopNextTab(wigFile, row, maxRow);
    if (numCols != maxRow) return FALSE;
    verbose(3, "#\tnumCols = %d, row[0]: %s, row[1]: %s, row[%d]: %s\n",
	numCols, row[0], row[1], maxRow-1, row[maxRow-1]);
    }
else
    {
    int i;
    char **sqlRow;
    sqlRow = sqlNextRow(sr);
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

void hgWiggle(int trackCount, char *tracks[])
/* hgWiggle - dump wiggle data from database or .wig file */
{
struct sqlConnection *conn;
#if defined(FORW)
FILE *f = (FILE *) NULL;
#else
int f = 0;
#endif
int i;
struct wiggle *wiggle;
char *wibFile = NULL;
struct sqlResult *sr;
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

if (db)
    {
    conn = hAllocConn();
    conn = sqlConnect(db);
    }

for (i=0; i<trackCount; ++i)
    {
    char *row[WIGGLE_NUM_COLS];
    unsigned long long rowCount = 0;
    unsigned long long chrRowCount = 0;
    unsigned int currentSpan = 0;
    char *currentChrom = NULL;
    long startClock = clock1000();
    long endClock;
    long chrStartClock = clock1000();
    long chrEndClock;
    unsigned long long validData = 0;
    unsigned long chrBytesRead = 0;
    unsigned long noDataBytes = 0;
    unsigned long chrNoDataBytes = 0;

    if (file)
	{
	struct dyString *fileName = dyStringNew(256);
	lineFileClose(&wigFile);	/*	possibly a previous file */
	dyStringPrintf(fileName, "%s.wig", tracks[i]);
	wigFile = lineFileOpen(fileName->string, TRUE);
	dyStringFree(&fileName);
	}
    else
	{
	struct dyString *query = dyStringNew(256);
	dyStringPrintf(query, "select * from %s", tracks[i]);
	if ((chr != NULL) || (span != 0))
	    {
	    dyStringPrintf(query, " where (");
	    if (chr != NULL)
		dyStringPrintf(query, "chrom = \"%s\"", chr);
	    if ((chr != NULL) && (span != 0))
		dyStringPrintf(query, " AND ");
	    if (span != 0)
		dyStringPrintf(query, "span = \"%d\"", span);
	    dyStringPrintf(query, ")");
	    }
	verbose(2, "#\t%s\n", query->string);
	sr = sqlGetResult(conn,query->string);
	}
    while (wigNextRow(sr, wigFile, row, WIGGLE_NUM_COLS))
	{
	unsigned char compareValue;	/*	dataValue compare	*/
	++rowCount;
	++chrRowCount;
	wiggle = wiggleLoad(row);
	if (file)
	    {
	    if (chr)
		if (differentString(chr,wiggle->chrom))
		    continue;
	    if (span)
		if (span != wiggle->span)
		    continue;
	    }

	if ( (currentSpan != wiggle->span) || 
		(currentChrom && differentString(currentChrom, wiggle->chrom)))
	    {
	    if (currentChrom && differentString(currentChrom, wiggle->chrom))
		{
		long et;
		chrEndClock = clock1000();
		et = chrEndClock - chrStartClock;
		if (timing)
 verbose(1,"#\t%s.%s %lu data bytes, %lu no-data bytes, %ld ms, %llu rows\n",
		    tracks[i], currentChrom, chrBytesRead, chrNoDataBytes, et,
			chrRowCount);
    		chrRowCount = chrNoDataBytes = chrBytesRead = 0;
		chrStartClock = clock1000();
		}
	    freeMem(currentChrom);
	    currentChrom=cloneString(wiggle->chrom);
	    if (!silent)
		printf ("variableStep chrom=%s", currentChrom);
	    currentSpan = wiggle->span;
	    if (!silent && (currentSpan > 1))
		printf (" span=%d\n", currentSpan);
	    else if (!silent)
		printf ("\n");
	    }

	verbose(3, "#\trow: %llu, start: %u, data range: %g: [%g:%g]\n",
		rowCount, wiggle->chromStart, wiggle->dataRange,
		wiggle->lowerLimit, wiggle->lowerLimit+wiggle->dataRange);
	verbose(3, "#\tresolution: %g per bin\n",
		wiggle->dataRange/(double)MAX_WIG_VALUE);
	if (useDataValue)
	    {
	    if (dataValue > (wiggle->lowerLimit + wiggle->dataRange))
		{
		bytesSkipped += wiggle->count;
		continue;
		}
	    if (dataValue < wiggle->lowerLimit)
		compareValue = 0;
	    else
		compareValue = MAX_WIG_VALUE *
		    ((dataValue - wiggle->lowerLimit)/wiggle->dataRange);
	    }
	if (!skipDataRead)
	    {
	    int j;	/*	loop counter through ReadData	*/
	    unsigned char *datum;    /* to walk through ReadData bytes */
	    unsigned char *ReadData;    /* the bytes read in from the file */
	    if (wibFile)
		{		/*	close and open only if different */
		if (differentString(wibFile,wiggle->file))
		    {
#if defined(FORW)
		    carefulClose(&f);
#else
		    if (f > 0)
			close(f);
#endif
		    freeMem(wibFile);
		    wibFile = cloneString(wiggle->file);
#if defined(FORW)
		    f = mustOpen(wibFile, "r");
#else
		    f = open(wibFile, O_RDONLY);
		    if (f == -1)
			errAbort("failed to open %s", wibFile);
#endif
		    }
		}
	    else
		{
		wibFile = cloneString(wiggle->file);	/* first time */
#if defined(FORW)
		f = mustOpen(wibFile, "r");
#else
		f = open(wibFile, O_RDONLY);
		if (f == -1)
		    errAbort("failed to open %s", wibFile);
#endif
		}
	    ReadData = (unsigned char *) needMem((size_t) (wiggle->count + 1));
	    bytesRead += wiggle->count;
#if defined(FORW)
	    fseek(f, wiggle->offset, SEEK_SET);
	    fread(ReadData, (size_t) wiggle->count,
		(size_t) sizeof(unsigned char), f);
#else
	    lseek(f, wiggle->offset, SEEK_SET);
	    read(f, ReadData,
		(size_t) wiggle->count * (size_t) sizeof(unsigned char));
#endif
    verbose(3, "#\trow: %llu, reading: %u bytes\n", rowCount, wiggle->count);
	    datum = ReadData;
	    for (j = 0; j < wiggle->count; ++j)
		{
		if (*datum != WIG_NO_DATA)
		    {
		    ++validData;
		    ++chrBytesRead;
		    if (useDataValue)
			{
			if (*datum >= compareValue)
			    {
			    ++valuesMatched;
			    if (!silent)
				{
				double datumOut =
 wiggle->lowerLimit+(((double)*datum/(double)MAX_WIG_VALUE)*wiggle->dataRange);
				printf("%d\t%g\n",
				  1 + wiggle->chromStart + (j * wiggle->span),
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
	if (!file || (file && ((i+1)==trackCount)))
 verbose(1,"#\t%s.%s %lu data bytes, %lu no-data bytes, %ld ms, %llu rows, %llu matched\n",
		    tracks[i], currentChrom, chrBytesRead, chrNoDataBytes, et,
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

if (file)
    {
    lineFileClose(&wigFile);
    if (timing)
 verbose(1,"#\ttotal %llu valid bytes, %lu no-data bytes, %ld ms, %llu rows\n#\t%llu matched = %% %.2f, wib bytes: %llu, bytes skipped: %llu\n",
	totalValidData, fileNoDataBytes, fileEt, totalRows, valuesMatched,
	100.0 * (float)valuesMatched / (float)totalValidData, bytesRead, bytesSkipped);
    }
if (wibFile)
    {
#if defined(FORW)
    carefulClose(&f);
#else
    if (f > 0)
	close(f);
#endif
    freeMem(wibFile);
    }

if (db)
    {
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
}	/*	void hgWiggle(int trackCount, char *tracks[])	*/


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);

db = optionVal("db", NULL);
chr = optionVal("chr", NULL);
silent = optionExists("silent");
timing = optionExists("timing");
skipDataRead = optionExists("skipDataRead");
span = optionInt("span", 0);
useDataValue = optionExists("dataValue");
if (useDataValue)
    dataValue = optionFloat("dataValue", 0.0);

if (db)
    verbose(2, "#\tdatabase: %s\n", db);
else
    {
    file = TRUE;
    verbose(2, "#\tno database specified, using .wig files\n");
    }
if (chr)
    verbose(2, "#\tselect chrom: %s\n", chr);
if (silent)
    verbose(2, "#\tsilent option on, no data points output\n");
if (timing)
    verbose(2, "#\ttiming option on\n");
if (skipDataRead)
    verbose(2, "#\tskipDataRead option on, do not read .wib data\n");
if (span)
    verbose(2, "#\tselect for span=%u only\n", span);
if (useDataValue)
    verbose(2, "#\tcomparing to data value: %f\n", dataValue);

if (argc < 2)
    usage();

hgWiggle(argc-1, argv+1);
return 0;
}
