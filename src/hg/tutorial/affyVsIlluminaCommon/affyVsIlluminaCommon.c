/* affyVsIlluminaCommon - Compare SNPs shared in Affy 250k and Illumina 300k platforms.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: affyVsIlluminaCommon.c,v 1.2 2007/08/27 21:45:04 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "affyVsIlluminaCommon - Compare SNPs shared in Affy 250k and Illumina 300k platforms.\n"
  "usage:\n"
  "   affyVsIlluminaCommon XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void affyVsIlluminaCommon(char *XXX)
/* affyVsIlluminaCommon - Compare SNPs shared in Affy 250k and Illumina 300k platforms.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
affyVsIlluminaCommon(argv[1]);
return 0;
}
