/* wigEncode - Convert wiggle ascii to wiggle binary format */

static char const rcsid[] = "$Id: wigEncode.c,v 1.4 2004/11/02 22:09:34 hiram Exp $";

#include "common.h"
#include "wiggle.h"

void usage()
/* Explain usage and exit */
{
errAbort("wigEncode - convert Wiggle ascii data to binary format\n\n"
    "usage:\n"
    "    wigEncode wigInput wigFile wibFile\n"
    "\twigInput - wiggle ascii data input file (stdin OK)\n"
    "\twigFile - .wig output file to be used with hgLoadWiggle\n"
    "\twibFile - .wib output file to be symlinked into /gbdb/<db>/wib/\n"
    "\n"
    "This processes the three data input format types described at:\n"
    "\thttp://genome.ucsc.edu/encode/submission.html#WIG\n"
    "\t(track and browser lines are tolerated, i.e. ignored)\n"
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

void wigEncode(char *bedFile, char *wigFile, char *wibFile)
/* Convert BED file to wiggle binary representation */
{
double upper, lower;
wigAsciiToBinary(bedFile, wigFile, wibFile, &upper, &lower);
fprintf(stderr, "Converted %s, upper limit %.2f, lower limit %.2f\n",
                        bedFile, upper, lower);
}

int main( int argc, char *argv[] )
/* Process command line */
{
if (argc < 4)
    usage();
wigEncode(argv[1], argv[2], argv[3]);
exit(0);
}
