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

static char const rcsid[] = "$Id: hgWiggle.c,v 1.20 2004/08/17 19:36:55 hiram Exp $";

/* Command line switches. */
static boolean noAscii = FALSE;	/*	do not output ascii data */
static boolean doStats = FALSE;	/*	perform stats measurement */
static boolean doBed = FALSE;	/*	output bed format */
static boolean silent = FALSE;	/*	no data points output */
static boolean fetchNothing = FALSE;	/*  no ascii, bed, or stats returned */
static boolean timing = FALSE;	/*	turn timing on	*/
static boolean skipDataRead = FALSE;	/*	do not read the wib data */
static boolean rawDataOut = FALSE;	/*	just the values, no positions */
static char *db = NULL;			/* database specification	*/
static char *chr = NULL;		/* work on this chromosome only */
static char *chromLst = NULL;	/*	file with list of chroms to process */
static char *position = NULL;	/*	to specify a range on a chrom */
static char *bedFile = NULL;	/*	to constrain via bedFile */
static unsigned winStart = 0;	/*	from the position specification */
static unsigned winEnd = 0;	/*	from the position specification */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"chr", OPTION_STRING},
    {"chromLst", OPTION_STRING},
    {"position", OPTION_STRING},
    {"bedFile", OPTION_STRING},
    {"dataConstraint", OPTION_STRING},
    {"noAscii", OPTION_BOOLEAN},
    {"doStats", OPTION_BOOLEAN},
    {"doBed", OPTION_BOOLEAN},
    {"silent", OPTION_BOOLEAN},
    {"fetchNothing", OPTION_BOOLEAN},
    {"timing", OPTION_BOOLEAN},
    {"skipDataRead", OPTION_BOOLEAN},
    {"rawDataOut", OPTION_BOOLEAN},
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
  "   -position=[chrN:]start-end - examine data in window start-end (1-relative)\n"
  "             (the chrN: is optional)\n"
  "   -chromLst=<file> - file with list of chroms to examine\n"
  "   -noAscii - do *not* perform the default ascii output\n"
  "   -rawDataOut - output just the data values, nothing else ( | textHistogram )\n"
  "   -doStats - perform stats measurement\n"
  "   -doBed - output bed format\n"
  "   -silent - no output, scanning data only and prepares result\n"
  "   -fetchNothing - scanning data only, *NOT* preparing result\n"
  "   -timing - display timing statistics\n"
/*"   -skipDataRead - do not read the .wib data (for no-read speed check)\n"*/
  "   -bedFile=<file> - constrain output to ranges specified in bed <file>\n"
  "   -dataConstraint='DC' - where DC is one of < = >= <= == != 'in range'\n"
  "   -ll=<F> - lowerLimit compare data values to F (float) (all but 'in range')\n"
  "   -ul=<F> - upperLimit compare data values to F (float)\n\t\t(need both ll and ul when 'in range')\n"
  "   When no database is specified, track names will refer to .wig files\n"
  "   example using the file gc5Base.wig:\n"
  "                hgWiggle -chr=chrM gc5Base\n"
  "   example using the database table hg17.gc5Base:\n"
  "                hgWiggle -chr=chrM -db=hg17 gc5Base\n"
  "   the case of multiple track names is most appropriate with .wig files\n"
  "        it will work with a database, but doesn't make much sense."
  );
}

static void hgWiggle(struct wiggleDataStream *wDS, int trackCount,
	char *tracks[])
/* hgWiggle - dump wiggle data from database or .wig file */
{
struct slName *chromList = NULL;	/*	list of chroms to process */
int i;
long startClock;
long endClock;
unsigned long long totalMatched = 0;

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
    if (db)
	chromList = hAllChromNamesDb(db);
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

    for (chromPtr=chromList;  (once == 1) || (chromPtr != NULL); --once )
	{
	long chrStartClock = clock1000();
	int operations = wigFetchAscii;	/*	default operation */
	unsigned long long valuesMatched;

	if (chromPtr)
	    {
	    wDS->setChromConstraint(wDS, chromPtr->name);
	    verbose(2,"#\tchrom: %s\n", chromPtr->name);
	    }

	if (noAscii)
		operations &= ~wigFetchAscii;
	if (doStats)
		operations |= wigFetchStats;
	if (doBed)
		operations |= wigFetchBed;
	if (fetchNothing)
		operations |= wigFetchNoOp;

	if (bedFile)
	    {
	    struct bed *bedList = bedLoadNAllChrom(bedFile, 3, wDS->chrName);
	    valuesMatched = wDS->getDataViaBed(wDS, db, tracks[i],
		operations, &bedList);
	    bedFreeList(&bedList);
	    }
	else
	    valuesMatched = wDS->getData(wDS, db, tracks[i], operations);

	totalMatched += valuesMatched;

	if ((valuesMatched > 0) && !silent)
	    {
	    /*	the TRUE means sort the results.  The FALSE case is
	     *	possible if you are accumulating results via numerous calls
	     *	and perhaps sorted them all at once with the
	     *	sortResults() method elsewhere, then came to printout.
	     *	No need to sort again.
	     *	When working through a list of chroms, no need to print
	     *	stats until all done.
	     */
	    if (doStats && !chromPtr)
		wDS->statsOut(wDS, "stdout", TRUE);
	    if (doBed)
		wDS->bedOut(wDS, "stdout", TRUE);
	    if (!noAscii)
		wDS->asciiOut(wDS, "stdout", TRUE, rawDataOut);
	    }
	wDS->freeBed(wDS);
	wDS->freeAscii(wDS);
	if (doStats && !chromPtr)
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
	}
	/*	when working through a chrom list, stats only at the end */
	if (doStats && chromList)
	    {
	    wDS->statsOut(wDS, "stdout", TRUE);
	    wDS->freeStats(wDS);
	    }
    }
endClock = clock1000();

if ((!doStats) && (0 == totalMatched))
    verbose(1,"#\tno values found with these constraints\n");

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


db = optionVal("db", NULL);
chr = optionVal("chr", NULL);
chromLst = optionVal("chromLst", NULL);
position = optionVal("position", NULL);
bedFile = optionVal("bedFile", NULL);
dataConstraint = optionVal("dataConstraint", NULL);
noAscii = optionExists("noAscii");
doStats = optionExists("doStats");
doBed = optionExists("doBed");
silent = optionExists("silent");
fetchNothing = optionExists("fetchNothing");
timing = optionExists("timing");
skipDataRead = optionExists("skipDataRead");
rawDataOut = optionExists("rawDataOut");
span = optionInt("span", 0);
lowerLimit = optionFloat("ll", -1 * INFINITY);
upperLimit = optionFloat("ul", INFINITY);

if (db)
    verbose(2, "#\tdatabase: %s\n", db);
else
    verbose(2, "#\tno database specified, using .wig files\n");

if (chromLst && chr)
    {
    warn("ERROR: both specified: -chr=%s and -chromLst=%s", chr, chromLst);
    errAbort("ERROR: specify only one of -chr or -chromLst but not both");
    }

/*	create the object here to allow constraint settings	*/
wDS = newWigDataStream();

if (chr)
    {
    wDS->setChromConstraint(wDS, chr);
    verbose(2, "#\tchrom constraint: %s\n", wDS->chrName);
    }
if (position)
    {
    char *startEnd[2];
    char *stripped = stripCommas(position);
    
    /*	allow chrN: or not	*/
    if (2 == chopByChar(stripped, ':', startEnd, 2))
	{
	char *freeThis;
	if (chr)
	    {
	    if (differentString(chr, startEnd[0]))
		{
	warn("different chroms specified via -chr and -position arguments\n");
	warn("uncertain which one it should be: %s vs. %s\n", chr, startEnd[0]);
		usage();
		}
	    }
	    else
		chr = cloneString(startEnd[0]);
	freeThis = stripped;
	stripped = stripCommas(startEnd[1]);
	freeMem(freeThis);
	}
    if (2 != chopByChar(stripped, '-', startEnd, 2))
	errAbort("can not parse position: '%s' '%s'", position, stripped);
    winStart = sqlUnsigned(startEnd[0]) - 1;	/* !!! 1-relative coming in */
    winEnd = sqlUnsigned(startEnd[1]);
    freeMem(stripped);
    wDS->setPositionConstraint(wDS, winStart, winEnd);
    verbose(2, "#\tposition specified: %u-%u\n", wDS->winStart+1, wDS->winEnd);
    }
if (noAscii)
    verbose(2, "#\tnoAscii option on, do not perform the default ascii output\n");
if (doBed)
    verbose(2, "#\tdoBed option on, output bed format\n");
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
if (rawDataOut)
    verbose(2, "#\trawDataOut option on, only data values are output\n");
if (span)
    {
    wDS->setSpanConstraint(wDS, span);
    verbose(2, "#\tspan constraint: %u\n", wDS->spanLimit);
    }

if (bedFile)
    verbose(2, "#\twill constrain to ranges specified in bed file: %s\n",
	bedFile);

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
