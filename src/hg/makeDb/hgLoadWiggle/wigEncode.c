/* wigEncode - Convert wiggle ascii to wiggle binary format */

static char const rcsid[] = "$Id: wigEncode.c,v 1.5 2005/08/01 21:20:05 hiram Exp $";

#include "common.h"
#include "wiggle.h"
#include "options.h"

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
    {NULL, 0}
};

static int lift = 0;		/*	offset to lift positions on input */

void wigEncode(char *bedFile, char *wigFile, char *wibFile)
/* Convert BED file to wiggle binary representation */
{
double upper, lower;
if (lift != 0)
    {
    struct wigEncodeOptions options;
    options.lift = lift;
    wigAsciiToBinary(bedFile, wigFile, wibFile, &upper, &lower, &options);
    }
else
    wigAsciiToBinary(bedFile, wigFile, wibFile, &upper, &lower, NULL);
fprintf(stderr, "Converted %s, upper limit %.2f, lower limit %.2f\n",
                        bedFile, upper, lower);
}

int main( int argc, char *argv[] )
/* Process command line */
{
optionInit(&argc, argv, optionSpecs);

lift = optionInt("lift", 0);

if (argc < 4)
    usage();

if (lift != 0)
    verbose(2,"option lift=%d to lift all positions by %d\n", lift, lift);
verbose(2,"input ascii data file: %s\n", argv[1]);
verbose(2,"output .wig file: %s\n", argv[2]);
verbose(2,"output .wib file: %s\n", argv[3]);

wigEncode(argv[1], argv[2], argv[3]);
exit(0);
}
