/* wigEncode - Convert wiggle ascii to wiggle binary format */

static char const rcsid[] = "$Id: wigEncode.c,v 1.1 2004/11/01 18:59:46 hiram Exp $";

#include "common.h"
#include "wiggle.h"

void usage()
/* Explain usage and exit */
{
errAbort("wigEncode - convert Wiggle ascii data to binary format\n\n"
        "usage:\n"
        "    wigEncode bedFile wigFile wibFile\n"
	"\n"
	"This processes the three data input format types described at:\n"
	"\thttp://genome.ucsc.edu/encode/submission.html#WIG\n"
	"*without* the track type= lines\n"
	"\t(future improvements will tolerate the track lines)");
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
