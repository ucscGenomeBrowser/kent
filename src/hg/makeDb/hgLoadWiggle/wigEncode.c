/* wigEncode - Convert wiggle ascii to wiggle binary format */

static char const rcsid[] = "$Id: wigEncode.c,v 1.12 2009/01/14 18:59:18 hiram Exp $";

#include "common.h"
#include "wiggle.h"
#include "options.h"
#include "obscure.h"

void usage()
/* Explain usage and exit */
{
errAbort("wigEncode - convert Wiggle ascii data to binary format\n\n"
    "usage:\n"
    "    wigEncode [options] wigInput wigFile wibFile\n"
    "\twigInput - wiggle ascii data input file (stdin OK)\n"
    "\twigFile - .wig output file to be used with hgLoadWiggle\n"
    "\twibFile - .wib output file to be symlinked into /gbdb/<db>/wib/\n"
    "\n"
    "This processes the three data input format types described at:\n"
    "\thttp://genome.ucsc.edu/encode/submission.html#WIG\n"
    "\t(track and browser lines are tolerated, i.e. ignored)\n"
    "options:\n"
    "    -lift=<D> - lift all input coordinates by D amount, default 0\n"
    "              - can be negative as well as positive\n"
    "    -allowOverlap - allow overlapping data, default: overlap not allowed\n"
    "              - only effective for fixedStep and if fixedStep declarations\n"
    "              - are in order by chromName,chromStart\n"
    "    -noOverlapSpanData - check for overlapping span data\n"
    "    -wibSizeLimit=<N> - ignore rest of input when wib size is >= N\n"
    "\n"
    "Example:\n"
    "    hgGcPercent -wigOut -doGaps -file=stdout -win=5 xenTro1 \\\n"
    "        /cluster/data/xenTro1 | "
    "wigEncode stdin gc5Base.wig gc5Base.wib\n"
    "load the resulting .wig file with hgLoadWiggle:\n"
    "    hgLoadWiggle -pathPrefix=/gbdb/xenTro1/wib xenTro1 gc5Base gc5Base.wig\n"
    "    ln -s `pwd`/gc5Base.wib /gbdb/xenTro1/wib"
    );
}

static struct optionSpec optionSpecs[] = {
    {"lift", OPTION_INT},
    {"noOverlap", OPTION_BOOLEAN},
    {"allowOverlap", OPTION_BOOLEAN},
    {"noOverlapSpanData", OPTION_BOOLEAN},
    {"wibSizeLimit", OPTION_LONG_LONG},
    {NULL, 0}
};

static int lift = 0;		/*	offset to lift positions on input */
static boolean noOverlap = TRUE;	/*	check for overlapping data */
static boolean allowOverlap = FALSE;	/* do not check for overlapping data */
static boolean noOverlapSpanData = FALSE; /* verify span data doesn't overlap */
static long long wibSizeLimit = 0;	/*	governor on ct trash sizes */

void wigEncode(char *bedFile, char *wigFile, char *wibFile)
/* Convert BED file to wiggle binary representation */
{
double upper=wigEncodeStartingUpperLimit, lower=wigEncodeStartingLowerLimit;
if ((lift != 0) || noOverlap || noOverlapSpanData || (wibSizeLimit > 0))
    {
    struct wigEncodeOptions options;

    ZeroVar(&options);	/*	make sure everything is zero	*/
    options.lift = lift;
    options.noOverlap = noOverlap;
    options.flagOverlapSpanData = noOverlapSpanData;
    options.wibSizeLimit = wibSizeLimit;
    wigAsciiToBinary(bedFile, wigFile, wibFile, &upper, &lower, &options);
    if ((wibSizeLimit > 0) && (options.wibSizeLimit >= wibSizeLimit))
	verbose(1,"#\twarning, reached wiggle size limits, %lld vs. %lld\n",
		wibSizeLimit, options.wibSizeLimit);
    }
else
    wigAsciiToBinary(bedFile, wigFile, wibFile, &upper, &lower, NULL);

if ( (wigEncodeStartingUpperLimit == upper) &&
	(wigEncodeStartingLowerLimit == lower) )
    errAbort("ERROR: wigEncode: empty input file: '%s'", bedFile );

verbose(1, "Converted %s, upper limit %.2f, lower limit %.2f\n",
                        bedFile, upper, lower);
}

int main( int argc, char *argv[] )
/* Process command line */
{
optionInit(&argc, argv, optionSpecs);

lift = optionInt("lift", 0);
wibSizeLimit = optionLongLong("wibSizeLimit", 0);
allowOverlap = optionExists("allowOverlap");
noOverlapSpanData = optionExists("noOverlapSpanData");
if (wibSizeLimit < 0)	/*	protect against negative limits	*/
    {
    wibSizeLimit = 0;
    verbose(1,"warning: negative wibSizeLimit specified, becomes zero\n");
    }

if (argc < 4)
    usage();
if (optionExists("noOverlap") && allowOverlap)
    {
    verbose(1,"wigEncode: can not specify -noOverlap and -allowOverlap together\n");
    usage();
    }
if (optionExists("noOverlap"))
    {
    verbose(1,"warning: -noOverlap is now the default, no need to specify that\n");
    verbose(1,"\tsee also: -allowOverlap\n");
    }

if (lift != 0)
    verbose(2,"option lift=%d to lift all positions by %d\n", lift, lift);
if (wibSizeLimit > 0)
    verbose(2,"option wibSizeLimit=%lld\n", wibSizeLimit);
if (noOverlapSpanData)
    {
    verbose(2,"option noOverlapSpanData on, will *not* allow overlapping spanned data\n" );
    }
if (allowOverlap)
    {
    verbose(2,"option allowOverlap on, will allow overlapping data\n" );
    noOverlap = FALSE;
    }
verbose(2,"input ascii data file: %s\n", argv[1]);
verbose(2,"output .wig file: %s\n", argv[2]);
verbose(2,"output .wib file: %s\n", argv[3]);

wigEncode(argv[1], argv[2], argv[3]);
if (verboseLevel() > 1)
    printVmPeak();
exit(0);
}
