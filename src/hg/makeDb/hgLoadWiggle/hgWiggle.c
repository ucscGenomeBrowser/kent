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

static char const rcsid[] = "$Id: hgWiggle.c,v 1.26 2004/09/03 21:58:01 hiram Exp $";

/* Command line switches. */
static boolean noAscii = FALSE;	/*	do not output ascii data */
static boolean doStats = FALSE;	/*	perform stats measurement */
static boolean doBed = FALSE;	/*	output bed format */
static boolean silent = FALSE;	/*	no data points output */
static boolean fetchNothing = FALSE;	/*  no ascii, bed, or stats returned */
static boolean timing = FALSE;	/*	turn timing on	*/
static boolean skipDataRead = FALSE;	/*	do not read the wib data */
static boolean rawDataOut = FALSE;	/*	just the values, no positions */
static boolean statsHTML = FALSE;	/*	stats output in HTML */
static boolean help = FALSE;	/*	extended help message */
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
    {"statsHTML", OPTION_BOOLEAN},
    {"help", OPTION_BOOLEAN},
    {"span", OPTION_INT},
    {"ll", OPTION_FLOAT},
    {"ul", OPTION_FLOAT},
    {NULL, 0}
};

void usage(boolean moreHelp)
/* Explain usage and exit. */
{
verbose(VERBOSE_ALWAYS_ON,
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
  "   -rawDataOut - output just the data values, nothing else\n"
  "   -statsHTML - output stats in HTML instead of plain text (sets doStats too)\n"
  "   -doStats - perform stats measurement, default output text, see -statsHTML\n"
  "   -doBed - output bed format\n"
  "   -bedFile=<file> - constrain output to ranges specified in bed <file>\n"
  "   -dataConstraint='DC' - where DC is one of < = >= <= == != 'in range'\n"
  "   -ll=<F> - lowerLimit compare data values to F (float) (all but 'in range')\n"
  "   -ul=<F> - upperLimit compare data values to F (float)\n\t\t(need both ll and ul when 'in range')\n"
  "\n"
  "   -help - display more examples and extra options (to stderr)\n"
  "\n"
  "   When no database is specified, track names will refer to .wig files\n\n"
  "   example using the file chrM.wig:\n"
  "\thgWiggle chrM\n"
  "   example using the database table hg17.gc5Base:\n"
  "\thgWiggle -chr=chrM -db=hg17 gc5Base\n"
  );

if (moreHelp)
  verbose(VERBOSE_ALWAYS_ON,
  "   example, stats on all .wig files in this directory:\n"
  "\thgWiggle -span=5 -noAscii -doStats `ls *.wig | sed -e 's/.wig//'`\n"
  "   example using dataCompare limits, show values over 35.0:\n"
  "\thgWiggle -span=5 -dataConstraint='>' -ll=35.0 -chr=chrM -db=hg17 gc5Base\n"
  "   example, show values in range [ 50.0 : 80.0 ]\n"
  "\thgWiggle -span=5 -dataConstraint='in range' -ll=50.0 -ul=80.0 \\\n"
  "\t\t-chr=chrM -db=hg17 gc5Base\n"
  "\n"
  "   the case of multiple track names is most appropriate with .wig files\n"
  "\tit will work with a database, but doesn't make much sense.\n"
  "\n"
  "   -rawDataOut - output just the data values, nothing else, useful for\n"
  "\t\tsending data to other processes, e.g:\n"
  "\thgWiggle -span=5 -rawDataOut -chr=chrM -db=hg17 gc5Base | \\\n"
  "\t\ttextHistogram -real -binSize=20 -maxBinCount=13 -pValues stdin\n"
/*"   -skipDataRead - do not read the .wib data (for no-read speed check)\n"*/
  "   -timing - display timing statistics to stderr\n"
  "   -silent - no output, scanning data only and prepares result (timing check)\n"
  "   -fetchNothing - scanning data only, *NOT* preparing result (timing check)\n"
  "   -verbose=2 - generic and per chrom information to stderr\n"
  "   -verbose=3 - per SQL row information to stderr\n"
  "   -verbose=4 - data block information to stderr\n"
  );

exit(255);
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
    if (db && !bedFile)
	chromList = hAllChromNamesDb(db);
    else
	verbose(VERBOSE_CHR_LEVEL, "#\tno chrom specified for file read, do them all\n");
    }

verbose(VERBOSE_CHR_LEVEL, "#\texamining tracks:");
for (i=0; i<trackCount; ++i)
    verbose(VERBOSE_CHR_LEVEL, " %s", tracks[i]);
verbose(VERBOSE_CHR_LEVEL, "\n");

startClock = clock1000();
for (i=0; i<trackCount; ++i)
    {
    int once = 1;	/*	at least once through even on null chromList */
    struct slName *chromPtr;
    unsigned long long chrBytesStart = 0;
    unsigned long long chrNoDataBytesStart = 0;
    unsigned long long chrRowsStart = 0;
    unsigned long long chrValuesMatchedStart = 0;

    /*	chromList may be empty, perform loop once in that case	*/
    for (chromPtr=chromList;  (once == 1) || (chromPtr != NULL); --once )
	{
	long chrStartClock = clock1000();
	int operations = wigFetchAscii;	/*	default operation */
	unsigned long long valuesMatched;

	if (chromPtr)
	    {
	    wDS->setChromConstraint(wDS, chromPtr->name);
	    verbose(VERBOSE_CHR_LEVEL,"#\tchrom: %s\n", chromPtr->name);
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

	if ((!silent && (valuesMatched > 0)) || (!silent && doStats))
	    {
	    /*	the TRUE means sort the results.  The FALSE case is
	     *	possible if you are accumulating results via numerous calls
	     *	and perhaps sorted them all at once with the
	     *	sortResults() method elsewhere, then came to printout.
	     *	No need to sort again.
	     *	When working through a list of chroms or tracks,
	     *	no need to print stats until all done.
	     */
	    if (doStats && (!chromPtr) && (trackCount == 1))
		wDS->statsOut(wDS, "stdout", TRUE, statsHTML);
	    if (doBed)
		wDS->bedOut(wDS, "stdout", TRUE);
	    if (!noAscii)
		wDS->asciiOut(wDS, "stdout", TRUE, rawDataOut);
	    }
	wDS->freeBed(wDS);
	wDS->freeAscii(wDS);
	if (doStats && (!chromPtr) && (trackCount == 1))
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
	    verbose(VERBOSE_ALWAYS_ON,"#\t%s.%s %llu data bytes, %llu no-data bytes, %ld ms, %llu rows, %llu matched\n",
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
    }
/* when working through a chrom list, or track list, stats only at the end */
if (doStats && (chromList || (trackCount > 1)))
    {
    wDS->statsOut(wDS, "stdout", TRUE, statsHTML);
    wDS->freeStats(wDS);
    }
endClock = clock1000();

if ((!doStats) && (0 == totalMatched))
    verbose(VERBOSE_ALWAYS_ON,"#\tno values found with these constraints\n");

if (timing)
    {
    long et;
    et = endClock - startClock;
    if (wDS->validPoints > 0 )
	{
    verbose(VERBOSE_ALWAYS_ON,"#\ttotal %llu valid bytes, %llu no-data bytes, %ld ms, %llu rows\n#\t%llu matched = %% %.2f, %llu wib bytes, %llu bytes skipped\n",
	wDS->validPoints, wDS->noDataPoints, et, wDS->rowsRead,
	wDS->valuesMatched,
	100.0 * (float)wDS->valuesMatched / (float)wDS->validPoints,
	wDS->bytesRead, wDS->bytesSkipped);
	}
    else
	{
    verbose(VERBOSE_ALWAYS_ON,"#\ttotal %llu valid bytes, %llu no-data bytes, %ld ms, %llu rows\n#\t%llu matched = %% 0.00, %llu wib bytes, %llu bytes skipped\n",
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
statsHTML = optionExists("statsHTML");
help = optionExists("help");
span = optionInt("span", 0);
lowerLimit = optionFloat("ll", -1 * INFINITY);
upperLimit = optionFloat("ul", INFINITY);

if (help)
    usage(TRUE);

if (statsHTML)
    doStats = TRUE;

if (db)
    verbose(VERBOSE_CHR_LEVEL, "#\tdatabase: %s\n", db);
else
    verbose(VERBOSE_CHR_LEVEL, "#\tno database specified, using .wig files\n");

if (chromLst && chr)
    {
    warn("ERROR: both specified: -chr=%s and -chromLst=%s", chr, chromLst);
    errAbort("ERROR: specify only one of -chr or -chromLst but not both");
    }

/*	create the object here to allow constraint settings	*/
wDS = wiggleDataStreamNew();

if (chr)
    {
    wDS->setChromConstraint(wDS, chr);
    verbose(VERBOSE_CHR_LEVEL, "#\tchrom constraint: %s\n", wDS->chrName);
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
		usage(FALSE);
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
    verbose(VERBOSE_CHR_LEVEL, "#\tposition specified: %u-%u\n", wDS->winStart+1, wDS->winEnd);
    }
if (noAscii)
    verbose(VERBOSE_CHR_LEVEL, "#\tnoAscii option on, do not perform the default ascii output\n");
if (doBed)
    verbose(VERBOSE_CHR_LEVEL, "#\tdoBed option on, output bed format\n");
if (doStats)
    verbose(VERBOSE_CHR_LEVEL, "#\tdoStats option on, perform statistics measurements\n");
if (silent)
    verbose(VERBOSE_CHR_LEVEL, "#\tsilent option on, no data points output, data is scanned\n");
if (fetchNothing)
    verbose(VERBOSE_CHR_LEVEL, "#\tfetchNothing option on, data will be scanned, no results prepared\n");
if (timing)
    verbose(VERBOSE_CHR_LEVEL, "#\ttiming option on\n");
if (skipDataRead)
    verbose(VERBOSE_CHR_LEVEL, "#\tskipDataRead option on, do not read .wib data\n");
if (rawDataOut)
    verbose(VERBOSE_CHR_LEVEL, "#\trawDataOut option on, only data values are output\n");
if (statsHTML)
    verbose(VERBOSE_CHR_LEVEL, "#\tstatsHTML option on, output stats in HTML format\n");
if (span)
    {
    wDS->setSpanConstraint(wDS, span);
    verbose(VERBOSE_CHR_LEVEL, "#\tspan constraint: %u\n", wDS->spanLimit);
    }

if (bedFile)
    verbose(VERBOSE_CHR_LEVEL, "#\twill constrain to ranges specified in bed file: %s\n",
	bedFile);

if (dataConstraint)
    {
    if (sameString(dataConstraint, "in range"))
	{
	if (!(optionExists("ll") && optionExists("ul")))
	    {
	    warn("ERROR: dataConstraint 'in range' specified without both -ll=<F> and -ul=<F>");
	    usage(FALSE);
	    }
	}
    else if (!optionExists("ll"))
	{
	warn("ERROR: dataConstraint specified without -ll=<F>");
	usage(FALSE);
	}

    wDS->setDataConstraint(wDS, dataConstraint, lowerLimit, upperLimit);

    if (sameString(dataConstraint, "in range"))
	verbose(VERBOSE_CHR_LEVEL, "#\tdataConstraint: %s [%f : %f]\n", wDS->dataConstraint,
		wDS->limit_0, wDS->limit_1);
    else
	verbose(VERBOSE_CHR_LEVEL, "#\tdataConstraint: data values %s %f\n",
		wDS->dataConstraint, wDS->limit_0);
    }
else if (optionExists("ll") || optionExists("ul"))
    {
    warn("ERROR: ll or ul options specified without -dataConstraint");
    usage(FALSE);
    }

if (argc < 2)
    usage(FALSE);

hgWiggle(wDS, argc-1, argv+1);

wiggleDataStreamFree(&wDS);

return 0;
}
