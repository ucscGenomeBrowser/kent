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

static char const rcsid[] = "$Id: hgWiggle.c,v 1.11 2004/08/06 22:47:16 hiram Exp $";

/* Command line switches. */
static boolean silent = FALSE;	/*	no data points output */
static boolean timing = FALSE;	/*	turn timing on	*/
static boolean skipDataRead = FALSE;	/*	do not read the wib data */
static char *chr = NULL;		/* work on this chromosome only */
static char *chromLst = NULL;	/*	file with list of chroms to process */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"chr", OPTION_STRING},
    {"chromLst", OPTION_STRING},
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
  "   -chromLst=<file> - file with list of chroms to examine\n"
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

/*	between hgWiggle() and getData()	*/
static unsigned long long totalValidData = 0;
static unsigned long fileNoDataBytes = 0;
static long fileEt = 0;
static unsigned long long totalRows = 0;
static unsigned long long valuesMatched = 0;
static unsigned long long bytesRead = 0;
static unsigned long long bytesSkipped = 0;

/*	between getData() and hgWiggle()	*/

static unsigned long long rowCount = 0;
static long chrStartClock;
static unsigned long chrBytesRead = 0;
static unsigned long chrNoDataBytes = 0;

#define	ASCII_DATA	0
#define	BED_DATA	1
#define	STATS_DATA	2

static void getData(struct wiggleDataStream *wDS, int type)
/* getData - read and return wiggle data	*/
{
char *row[WIGGLE_NUM_COLS];
unsigned long long chrRowCount = 0;
unsigned long long validData = 0;
unsigned long noDataBytes = 0;
boolean doStats = FALSE;
boolean doBed = FALSE;
boolean doAscii = FALSE;


switch (type)
    {
    case ASCII_DATA:
	doAscii = TRUE;
	break;
    case BED_DATA:
	doBed = TRUE;
	break;
    case STATS_DATA:
	doStats = TRUE;
	break;
    default:
	verbose(2, "wigGetData: no type of data fetch requested ?\n");
	return;	/*	NOTHING ASKED FOR	*/
    }

rowCount = 0;
chrStartClock = clock1000();
chrBytesRead = 0;
chrNoDataBytes = 0;

while (wDS->nextRow(wDS, row, WIGGLE_NUM_COLS))
    {
    struct wiggle *wiggle;
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
	    long chrEndClock;
	    chrEndClock = clock1000();
	    et = chrEndClock - chrStartClock;
	    if (timing)
verbose(1,"#\t%s.%s %lu data bytes, %lu no-data bytes, %ld ms, %llu rows\n",
		wDS->tblName, wDS->currentChrom, chrBytesRead,
		chrNoDataBytes, et, chrRowCount);
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
	wDS->setCompareByte(wDS, wiggle->lowerLimit, wiggle->dataRange);
	}
    if (!skipDataRead)
	{
	int j;	/*	loop counter through ReadData	*/
	unsigned char *datum;    /* to walk through ReadData bytes */
	unsigned char *ReadData;    /* the bytes read in from the file */
	wDS->openWibFile(wDS, wiggle->file);
		    /* possibly open a new wib file */
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
    }	/*	while (nextRow())	*/

totalRows += rowCount;
totalValidData += validData;
fileNoDataBytes += noDataBytes;

}	/*	void getData()	*/

static void getAscii(struct wiggleDataStream *wDS)
{
getData(wDS, ASCII_DATA);
}

static void hgWiggle(struct wiggleDataStream *wDS, int trackCount,
	char *tracks[])
/* hgWiggle - dump wiggle data from database or .wig file */
{
struct slName *chromList = NULL;	/*	 list of chroms to process */
int i;
long startClock;
long endClock;

if (chromLst)
    {
    struct lineFile *clf = lineFileOpen(chromLst, TRUE);
    char *row[1];
    while (lineFileRow(clf, row))
        {
	struct slName *el;
	el = newSlName(row[0]);
	slAddHead(&chromList, el);
        }
    lineFileClose(&clf);
    slReverse(&chromList);
    }
else if (chr)
    {
    struct slName *el;
    el = newSlName(chr);
    slAddHead(&chromList, el);
    }
else
    {
    if (wDS->db)
	chromList = hAllChromNamesDb(wDS->db);
    else
	verbose(2, "#\tno chrom specified for file read, do them all\n");
    }

totalValidData = 0;
fileNoDataBytes = 0;
fileEt = 0;
totalRows = 0;
valuesMatched = 0;
bytesRead = 0;
bytesSkipped = 0;

verbose(2, "#\texamining tracks:");
for (i=0; i<trackCount; ++i)
    verbose(2, " %s", tracks[i]);
verbose(2, "\n");

startClock = clock1000();
for (i=0; i<trackCount; ++i)
    {
    int once = 1;	/*	at least once through even on null chromList */
    struct slName *chromPtr;

    for (chromPtr=chromList;  (once == 1) || (chromPtr != NULL); )
	{
	if (chromPtr)
	    {
	    wDS->setChromConstraint(wDS, chromPtr->name);
	    verbose(2,"#\tchrom: %s\n", chromPtr->name);
	    }

	wDS->openWigConn(wDS, tracks[i]);
	getAscii(wDS);
	wDS->closeWigConn(wDS);
	if (timing)
	    {
	    long et;
	    long chrEndClock;
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
	    }
	if (chromPtr)
	    chromPtr = chromPtr->next;
	--once;
	}
    }
endClock = clock1000();

if (timing)
    {
    long et;
    et = endClock - startClock;
    verbose(1,"#\ttotal %llu valid bytes, %lu no-data bytes, %ld ms, %llu rows\n#\t%llu matched = %% %.2f, wib bytes: %llu, bytes skipped: %llu\n",
	totalValidData, fileNoDataBytes, et, totalRows, valuesMatched,
	100.0 * (float)valuesMatched / (float)totalValidData,
	bytesRead, bytesSkipped);
    }
}	/*	void hgWiggle()	*/

int main(int argc, char *argv[])
/* Process command line. */
{
struct wiggleDataStream *wDS = NULL;
float lowerLimit = -1 * INFINITY;  /*	for constraint comparison	*/
float upperLimit = INFINITY;	/*	for constraint comparison	*/
unsigned span = 0;	/*	select for this span only	*/
static char *dataConstraint;	/*	one of < = >= <= == != 'in range' */

optionInit(&argc, argv, optionSpecs);

wDS = newWigDataStream();

wDS->db = optionVal("db", NULL);
chr = optionVal("chr", NULL);
chromLst = optionVal("chromLst", NULL);
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
if (chromLst && chr)
    {
    warn("ERROR: both specified: -chr=%s and -chromLst=%s", chr, chromLst);
    errAbort("ERROR: specify only one of -chr or -chromLst but not both");
    }
if (chr)
    {
    wDS->setChromConstraint(wDS, chr);
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
    wDS->setSpanConstraint(wDS, span);
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

    wDS->setDataConstraint(wDS, dataConstraint, lowerLimit, upperLimit);

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

destroyWigDataStream(&wDS);

return 0;
}
