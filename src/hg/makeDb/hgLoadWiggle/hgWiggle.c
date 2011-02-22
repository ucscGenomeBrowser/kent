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
#include "memalloc.h"

static char const rcsid[] = "$Id: hgWiggle.c,v 1.39 2008/09/03 19:19:45 markd Exp $";

/* Command line switches. */
static boolean doAscii = TRUE;	/*	do not output ascii data */
static boolean doStats = FALSE;	/*	perform stats measurement */
static boolean doBed = FALSE;	/*	output bed format */
static boolean doHistogram = FALSE;	/*	output histogram of the data */
static boolean silent = FALSE;	/*	no data points output */
static boolean fetchNothing = FALSE;	/*  no ascii, bed, or stats returned */
static boolean timing = FALSE;	/*	turn timing on	*/
static boolean skipDataRead = FALSE;	/*	do not read the wib data */
static boolean rawDataOut = FALSE;	/*	just the values, no positions */
static boolean htmlOut = FALSE;	/*	stats and histogram output in HTML */
static boolean help = FALSE;	/*	extended help message */
static char *db = NULL;			/* database specification	*/
static char *chr = NULL;		/* work on this chromosome only */
static char *chromLst = NULL;	/*	file with list of chroms to process */
static char *position = NULL;	/*	to specify a range on a chrom */
static char *bedFile = NULL;	/*	to constrain via bedFile */
static unsigned winStart = 0;	/*	from the position specification */
static unsigned winEnd = 0;	/*	from the position specification */
static float hBinSize = 1.0;	/*	histoGram bin size	*/
static unsigned hBinCount = 25;	/*	histoGram bin count	*/
static float hMinVal = 0.0;	/*	histoGram minimum value	*/
static float hRange = 0.0;	/*	hRange = hBinSize * hBinCount; */
static float hMax = 0.0;	/*	hMax = hMinVal + hRange;	*/
static int lift = 0;		/*	offset to lift ascii positions out */

#if defined(DEBUG)
struct cart *cart;
#endif

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"chr", OPTION_STRING},
    {"chrom", OPTION_STRING},
    {"chromLst", OPTION_STRING},
    {"position", OPTION_STRING},
    {"bedFile", OPTION_STRING},
    {"dataConstraint", OPTION_STRING},
    {"doAscii", OPTION_BOOLEAN},
    {"doStats", OPTION_BOOLEAN},
    {"doBed", OPTION_BOOLEAN},
    {"doHistogram", OPTION_BOOLEAN},
    {"silent", OPTION_BOOLEAN},
    {"fetchNothing", OPTION_BOOLEAN},
    {"timing", OPTION_BOOLEAN},
    {"skipDataRead", OPTION_BOOLEAN},
    {"rawDataOut", OPTION_BOOLEAN},
    {"htmlOut", OPTION_BOOLEAN},
    {"help", OPTION_BOOLEAN},
    {"span", OPTION_INT},
    {"ll", OPTION_FLOAT},
    {"ul", OPTION_FLOAT},
    {"hBinSize", OPTION_STRING},
    {"hBinCount", OPTION_INT},
    {"hMinVal", OPTION_STRING},
    {"lift", OPTION_INT},
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
  "   -chrom=chrN - same as -chr option above\n"
  "   -position=[chrN:]start-end - examine data in window start-end (1-relative)\n"
  "             (the chrN: is optional)\n"
  "   -chromLst=<file> - file with list of chroms to examine\n"
  "   -doAscii - perform the default ascii output, in addition to other outputs\n"
  "            - Any of the other -do outputs turn off the default ascii output\n"
  "   -rawDataOut - output just the data values, nothing else\n"
  "   -htmlOut - output stats or histogram in HTML instead of plain text\n"
  "   -doStats - perform stats measurement, default output text, see -htmlOut\n"
  "   -doBed - output bed format\n"
  "   -lift=<D> - lift ascii output positions by D (0 default)\n"
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
  "\thgWiggle -span=5 -doStats `ls *.wig | sed -e 's/.wig//'`\n"
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
  "   -doHistogram - perform histogram on data, turns off all other outputs\n"
  "                - see also -htmlOut to output the histogram in html\n"
  "   -hBinSize=<F> - sets histogram bin size to <F> (float, default 1.0)\n"
  "   -hBinCount=<D> - sets histogram bin count to <D> (int, default 25)\n"
  "   -hMinVal=<F> - sets histogram minimum value to <F> (float, default 0.0)\n"
  "   -timing - display timing statistics to stderr\n"
  "   -silent - no output, scanning data only and prepares result (timing check)\n"
  "   -fetchNothing - scanning data only, *NOT* preparing result (timing check)\n"
  "   -verbose=2 - generic and per chrom information to stderr\n"
  "   -verbose=3 - per SQL row information to stderr\n"
  "   -verbose=4 - data block information to stderr\n"
  );

exit(255);
}

static void hgWiggle(struct wiggleDataStream *wds, int trackCount,
	char *tracks[])
/* hgWiggle - dump wiggle data from database or .wig file */
{
struct slName *chromList = NULL;	/*	list of chroms to process */
int i;
long startClock;
long endClock;
unsigned long long totalMatched = 0;
struct histoResult *histoGramResult = NULL;
float *valuesArray = NULL;
size_t valueCount = 0;

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
	{
	chromList = hAllChromNames(db);
	slReverse(&chromList);
	}
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
	    wds->setChromConstraint(wds, chromPtr->name);
	    verbose(VERBOSE_CHR_LEVEL,"#\tchrom: %s\n", chromPtr->name);
	    }

	if (!rawDataOut && !doAscii)
		operations &= ~wigFetchAscii;
	if (doStats)
		operations |= wigFetchStats;
	if (doBed)
		operations |= wigFetchBed;
	if (fetchNothing)
		operations |= wigFetchNoOp;
	if (doHistogram)
		operations |= wigFetchAscii;

	if (bedFile)
	    {
	    struct bed *bedList=bedLoadNAllChrom(bedFile, 3, wds->chrName);
	    valuesMatched = wds->getDataViaBed(wds, db, tracks[i],
		operations, &bedList);
	    bedFreeList(&bedList);
	    }
	else
	    valuesMatched = wds->getData(wds, db, tracks[i], operations);

	if (doHistogram && (valuesMatched > 0))
	    {
	    /*  convert the ascii data listings to one giant float array */
	    valuesArray=wds->asciiToDataArray(wds, valuesMatched, &valueCount);
	    /* first time starts the histogram, next times accumulate into it */
	    if (histoGramResult == NULL)
		histoGramResult = histoGram(valuesArray, valueCount,
		    hBinSize, hBinCount+1, hMinVal, hMinVal, hMax,
			(struct histoResult *)NULL);
	    else
		(void) histoGram(valuesArray, valueCount,
		    hBinSize, hBinCount+1, hMinVal, hMinVal, hMax,
			histoGramResult);
	    freeMem(valuesArray);
	    }

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
		wds->statsOut(wds, db, "stdout", TRUE, htmlOut, TRUE, FALSE);
	    if (doBed)
		wds->bedOut(wds, "stdout", TRUE);
	    if (rawDataOut || doAscii)
		wds->asciiOut(wds, db, "stdout", TRUE, rawDataOut);
	    }
	wds->freeBed(wds);
	wds->freeAscii(wds);
	if (doStats && (!chromPtr) && (trackCount == 1))
	    wds->freeStats(wds);
	if (timing)
	    {
	    long et;
	    long chrEndClock;
	    unsigned long long chrBytesRead = wds->validPoints - chrBytesStart;
	    unsigned long long chrNoDataBytes =
		wds->noDataPoints - chrNoDataBytesStart;
	    unsigned long long chrRowsRead = wds->rowsRead - chrRowsStart;
	    unsigned long long chrValuesMatched =
		wds->valuesMatched - chrValuesMatchedStart;

	    chrEndClock = clock1000();
	    et = chrEndClock - chrStartClock;
	    verbose(VERBOSE_ALWAYS_ON,"#\t%s.%s %llu data bytes, %llu no-data bytes, %ld ms, %llu rows, %llu matched\n",
			tracks[i], wds->currentChrom, chrBytesRead,
			chrNoDataBytes, et, chrRowsRead, chrValuesMatched);
	    chrNoDataBytes = chrBytesRead = 0;
	    chrBytesStart = wds->validPoints;
	    chrNoDataBytesStart = wds->noDataPoints;
	    chrRowsStart = wds->rowsRead;
	    chrValuesMatchedStart = wds->valuesMatched;
	    chrStartClock = clock1000();
	    }
	if (chromPtr)
	    chromPtr = chromPtr->next;
	}
    }

if (doHistogram)
    {
    printHistoGram(histoGramResult, htmlOut);
    freeHistoGram(&histoGramResult);
    }

/* when working through a chrom list, or track list, stats only at the end */
if (doStats && (chromList || (trackCount > 1)))
    {
    wds->statsOut(wds, db, "stdout", TRUE, htmlOut, TRUE, FALSE);
    wds->freeStats(wds);
    }
endClock = clock1000();

if ((!doStats) && (0 == totalMatched))
    verbose(VERBOSE_ALWAYS_ON,"#\tno values found with these constraints\n");

if (timing)
    {
    long et;
    et = endClock - startClock;
    if (wds->validPoints > 0 )
	{
    verbose(VERBOSE_ALWAYS_ON,"#\ttotal %llu valid bytes, %llu no-data bytes, %ld ms, %llu rows\n#\t%llu matched = %% %.2f, %llu wib bytes, %llu bytes skipped\n",
	wds->validPoints, wds->noDataPoints, et, wds->rowsRead,
	wds->valuesMatched,
	100.0 * (float)wds->valuesMatched / (float)wds->validPoints,
	wds->bytesRead, wds->bytesSkipped);
	}
    else
	{
    verbose(VERBOSE_ALWAYS_ON,"#\ttotal %llu valid bytes, %llu no-data bytes, %ld ms, %llu rows\n#\t%llu matched = %% 0.00, %llu wib bytes, %llu bytes skipped\n",
	wds->validPoints, wds->noDataPoints, et, wds->rowsRead,
	wds->valuesMatched, wds->bytesRead, wds->bytesSkipped);
	}
    }
}	/*	void hgWiggle()	*/

int main(int argc, char *argv[])
/* Process command line. */
{
struct wiggleDataStream *wds = NULL;
float lowerLimit = -1 * INFINITY;  /*	for constraint comparison	*/
float upperLimit = INFINITY;	/*	for constraint comparison	*/
unsigned span = 0;	/*	select for this span only	*/
static char *dataConstraint;	/*	one of < = >= <= == != 'in range' */

pushCarefulMemHandler(2147483647 * ((sizeof(size_t)/4)*(sizeof(size_t)/4)));
	/*	== 2 Gb for 32 bit machines, 8 Gb for 64 bit machines */

optionInit(&argc, argv, optionSpecs);


db = optionVal("db", NULL);
chr = optionVal("chr", NULL);
if (NULL == chr)
    chr = optionVal("chrom", NULL);
chromLst = optionVal("chromLst", NULL);
position = optionVal("position", NULL);
bedFile = optionVal("bedFile", NULL);
dataConstraint = optionVal("dataConstraint", NULL);
doStats = optionExists("doStats");
doBed = optionExists("doBed");
doHistogram = optionExists("doHistogram");
silent = optionExists("silent");
fetchNothing = optionExists("fetchNothing");
timing = optionExists("timing");
skipDataRead = optionExists("skipDataRead");
rawDataOut = optionExists("rawDataOut");
htmlOut = optionExists("htmlOut");
help = optionExists("help");
span = optionInt("span", 0);
lowerLimit = optionFloat("ll", -1 * INFINITY);
upperLimit = optionFloat("ul", INFINITY);
hBinSize = optionFloat("hBinSize", 1.0);
hMinVal = optionFloat("hMinVal", 0.0);
hBinCount = optionInt("hBinCount", 25);
lift = optionInt("lift", 0);

if (help)
    usage(TRUE);

if (doStats || doBed)
    doAscii = optionExists("doAscii");

if (doHistogram)	/*	histogram turns off everything else */
    {
    doAscii = FALSE;
    doBed = FALSE;
    doStats = FALSE;
    rawDataOut = FALSE;
    hRange = hBinSize * hBinCount;
    if ( !(hRange > 0.0))
	errAbort("ERROR: histogram range is not > 0.0. binSize: %g, binCount: %u, range: %g", hBinSize, hBinCount, hRange);
    hMax = hMinVal + hRange;
    }

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
wds = wiggleDataStreamNew();

if (chr)
    {
    wds->setChromConstraint(wds, chr);
    verbose(VERBOSE_CHR_LEVEL, "#\tchrom constraint: %s\n", wds->chrName);
    }
if (position)
    {
    char *startEnd[2];
    char *clonePosition = cloneString(position);
    char *stripped = stripCommas(clonePosition);
    
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
    if (sqlSigned(startEnd[0]) < 1)
	errAbort("ERROR: illegal chromStart: %d in specified position: %s", sqlSigned(startEnd[0]), position);
    if (sqlSigned(startEnd[1]) < 1)
	errAbort("ERROR: illegal chromEnd: %d in specified position: %s", sqlSigned(startEnd[1]), position);
    winStart = BASE_0(sqlUnsigned(startEnd[0])); /* !!! 1-relative coming in */
    winEnd = sqlUnsigned(startEnd[1]);
    freeMem(stripped);
    wds->setPositionConstraint(wds, winStart, winEnd);
    verbose(VERBOSE_CHR_LEVEL, "#\tposition specified: %u-%u\n", BASE_1(wds->winStart), wds->winEnd);
    freeMem(clonePosition);
    }
wds->offset = lift;
if (lift != 0)
    verbose(VERBOSE_CHR_LEVEL, "#\tlifting ascii positions on output by %d\n", lift);
if (doAscii)
    verbose(VERBOSE_CHR_LEVEL, "#\tdoAscii option on, perform the default ascii output\n");
if (doHistogram)
    verbose(VERBOSE_CHR_LEVEL, "#\tdoHistogram: min,max: %g:%g, range: %g, binCount: %u, binSize: %g\n", hMinVal, hMax, hRange, hBinCount, hBinSize);

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
if (htmlOut)
    verbose(VERBOSE_CHR_LEVEL, "#\thtmlOut option on, output stats or histogram in HTML format\n");
if (span)
    {
    wds->setSpanConstraint(wds, span);
    verbose(VERBOSE_CHR_LEVEL, "#\tspan constraint: %u\n", wds->spanLimit);
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

    wds->setDataConstraint(wds, dataConstraint, lowerLimit, upperLimit);

    if (sameString(dataConstraint, "in range"))
	verbose(VERBOSE_CHR_LEVEL, "#\tdataConstraint: %s [%f : %f]\n", wds->dataConstraint,
		wds->limit_0, wds->limit_1);
    else
	verbose(VERBOSE_CHR_LEVEL, "#\tdataConstraint: data values %s %f\n",
		wds->dataConstraint, wds->limit_0);
    }
else if (optionExists("ll") || optionExists("ul"))
    {
    warn("ERROR: ll or ul options specified without -dataConstraint");
    usage(FALSE);
    }

if (argc < 2)
    usage(FALSE);

hgWiggle(wds, argc-1, argv+1);

wiggleDataStreamFree(&wds);
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
