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

static char const rcsid[] = "$Id: hgWiggle.c,v 1.13 2004/08/10 21:00:54 hiram Exp $";

/* Command line switches. */
static boolean noAscii = FALSE;	/*	do not output ascii data */
static boolean doStats = FALSE;	/*	perform stats measurement */
static boolean silent = FALSE;	/*	no data points output */
static boolean fetchNothing = FALSE;	/*  no ascii, bed, or stats returned */
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
    {"noAscii", OPTION_BOOLEAN},
    {"doStats", OPTION_BOOLEAN},
    {"silent", OPTION_BOOLEAN},
    {"fetchNothing", OPTION_BOOLEAN},
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
  "   -noAscii - do *not* perform the default ascii output\n"
  "   -doStats - perform stats measurement\n"
  "   -silent - no output, scanning data only and prepares result\n"
  "   -fetchNothing - scanning data only, *NOT* preparing result\n"
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

verbose(2, "#\texamining tracks:");
for (i=0; i<trackCount; ++i)
    verbose(2, " %s", tracks[i]);
verbose(2, "\n");

startClock = clock1000();
for (i=0; i<trackCount; ++i)
    {
    int once = 1;	/*	at least once through even on null chromList */
    struct slName *chromPtr;
    unsigned long long chrBytesStart = 0;
    unsigned long long chrNoDataBytesStart = 0;
    unsigned long long chrRowsStart = 0;
    unsigned long long chrValuesMatchedStart = 0;

    for (chromPtr=chromList;  (once == 1) || (chromPtr != NULL); )
	{
	long chrStartClock = clock1000();
	int whatToDo = wigFetchAscii;

	if (chromPtr)
	    {
	    wDS->setChromConstraint(wDS, chromPtr->name);
	    verbose(2,"#\tchrom: %s\n", chromPtr->name);
	    }

	wDS->openWigConn(wDS, tracks[i]);

	if (noAscii)
		whatToDo &= ~wigFetchAscii;
	if (doStats)
		whatToDo |= wigFetchStats;
	if (fetchNothing)
		whatToDo |= wigFetchNoOp;

	wDS->getData(wDS, whatToDo);
	wDS->closeWigConn(wDS);
	if (!silent)
	    {
	    if (doStats)
		wDS->statsOut(wDS, "stdout");
	    if (!noAscii)
		wDS->asciiOut(wDS, "stdout");
	    }
	wDS->freeAscii(wDS);
	wDS->freeStats(wDS);
	if (timing)
	    {
	    long et;
	    long chrEndClock;
	    unsigned long long chrBytesRead = wDS->validPoints - chrBytesStart;
	    unsigned long long chrNoDataBytes =
		wDS->noDataPoints - chrNoDataBytesStart;
	    unsigned long long chrRowsRead = wDS->rowsRead - chrRowsStart;
	    unsigned long long chrValuesMatched =
		wDS->valuesMatched - chrValuesMatchedStart;

	    chrEndClock = clock1000();
	    et = chrEndClock - chrStartClock;
	    verbose(1,"#\t%s.%s %llu data bytes, %llu no-data bytes, %ld ms, %llu rows, %llu matched\n",
			tracks[i], wDS->currentChrom, chrBytesRead,
			chrNoDataBytes, et, chrRowsRead, chrValuesMatched);
	    chrNoDataBytes = chrBytesRead = 0;
	    chrBytesStart = wDS->validPoints;
	    chrNoDataBytesStart = wDS->noDataPoints;
	    chrRowsStart = wDS->rowsRead;
	    chrValuesMatchedStart = wDS->valuesMatched;
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
    if (wDS->validPoints > 0 )
	{
    verbose(1,"#\ttotal %llu valid bytes, %llu no-data bytes, %ld ms, %llu rows\n#\t%llu matched = %% %.2f, %llu wib bytes, %llu bytes skipped\n",
	wDS->validPoints, wDS->noDataPoints, et, wDS->rowsRead,
	wDS->valuesMatched,
	100.0 * (float)wDS->valuesMatched / (float)wDS->validPoints,
	wDS->bytesRead, wDS->bytesSkipped);
	}
    else
	{
    verbose(1,"#\ttotal %llu valid bytes, %lu no-data bytes, %ld ms, %llu rows\n#\t%llu matched = %% 0.00, %llu wib bytes, %llu bytes skipped\n",
	wDS->validPoints, wDS->noDataPoints, et, wDS->rowsRead,
	wDS->valuesMatched, wDS->bytesRead, wDS->bytesSkipped);
	}
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
noAscii = optionExists("noAscii");
doStats = optionExists("doStats");
silent = optionExists("silent");
fetchNothing = optionExists("fetchNothing");
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
if (noAscii)
    verbose(2, "#\tnoAscii option on, do not perform the default ascii output\n");
if (doStats)
    verbose(2, "#\tdoStats option on, perform statistics measurements\n");
if (silent)
    verbose(2, "#\tsilent option on, no data points output, data is scanned\n");
if (fetchNothing)
    verbose(2, "#\tfetchNothing option on, data will be scanned, no results prepared\n");
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
