/* wigBedToBinary - Convert a BED file, formatted <chrom><start><end><value> to
 *                      wiggle binary format */

static char const rcsid[] = "$Id: wigBedToBinary.c,v 1.1 2004/10/19 20:23:23 kate Exp $";

#include "common.h"
#include "wiggle.h"

void usage()
/* Explain usage and exit */
{
errAbort("wigBedToBinary - convert BED Wiggle data to binary file\n\n"
        "usage:\n"
        "    wigBedToBinary bedFile wigFile wibFile\n");
}

void wigBedToBinary(char *bedFile, char *wigFile, char *wibFile)
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
wigBedToBinary(argv[1], argv[2], argv[3]);
exit(0);
}
