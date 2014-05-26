/* wigBedToBinary - Convert a BED file, formatted <chrom><start><end><value> to
 *                      wiggle binary format */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


#include "common.h"
#include	"options.h"
#include "wiggle.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"obsolete", OPTION_BOOLEAN},
    {NULL, 0}
};

static boolean obsolete = FALSE;/*use this program even though it is obsolete*/

void usage()
/* Explain usage and exit */
{
errAbort("wigBedToBinary - convert BED Wiggle data to binary file\n\n"
    "usage: wigBedToBinary [-obsolete] bedFile wigFile wibFile\n"
    "\toptions:\n"
    "\t-obsolete - Use this program even though it is obsolete.");
}

void wigBedToBinary(char *bedFile, char *wigFile, char *wibFile)
/* Convert BED file to wiggle binary representation */
{
double upper, lower;
wigAsciiToBinary(bedFile, wigFile, wibFile, &upper, &lower, NULL);
fprintf(stderr, "Converted %s, upper limit %.2f, lower limit %.2f\n",
                        bedFile, upper, lower);
}

int main( int argc, char *argv[] )
/* Process command line */
{
optionInit(&argc, argv, optionSpecs);

obsolete = optionExists("obsolete");
if (! obsolete)
    {
    verbose(1,"ERROR: This loader is obsolete.  Please use: 'wigEncode'\n");
    verbose(1,"\t(use -obsolete flag to run this encoder despite it being obsolete)\n");
    errAbort("ERROR: wigBedToBinary is obsolete, use 'wigEncode' instead");
    }

if (argc < 4)
    usage();
wigBedToBinary(argv[1], argv[2], argv[3]);
exit(0);
}
