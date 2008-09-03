/* mafFrag - Extract maf sequences for a region from database. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "maf.h"
#include "hdb.h"
#include "hgMaf.h"

static char const rcsid[] = "$Id: mafFrag.c,v 1.5 2008/09/03 19:21:15 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafFrag - Extract maf sequences for a region from database\n"
  "usage:\n"
  "   mafFrag database mafTrack chrom start end strand out.maf\n"
  "options:\n"
  "   -outName=XXX  Use XXX instead of database.chrom for the name\n"
  );
}

char *outName = NULL;

static struct optionSpec options[] = {
   {"outName", OPTION_STRING},
   {NULL, 0},
};

void mafFragCheck(char *database, char *track, 
	char *chrom, char *startString, char *endString,
	char *strand, char *outMaf)
/* mafFragCheck - Check parameters and convert to binary.
 * Call mafFrag. */
{
int start, end, chromSize;
struct mafAli *maf;
if (!isdigit(startString[0]) || !isdigit(endString[0]))
    errAbort("%s %s not numbers", startString, endString);
start = atoi(startString);
end = atoi(endString);
if (end <= start)
    errAbort("end before start!");
chromSize = hChromSize(database, chrom);
if (end > chromSize)
   errAbort("End past chromSize (%d > %d)", end, chromSize);
maf = hgMafFrag(database, track, 
	chrom, start, end, *strand, 
	outName, NULL);
if (maf != NULL)
    {
    FILE *f = mustOpen(outMaf, "w");
    mafWriteStart(f, "zero");
    mafWrite(f, maf);
    mafWriteEnd(f);
    carefulClose(&f);
    mafAliFree(&maf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionInit(&argc, argv, options);
outName = optionVal("outName", NULL);
if (argc != 8)
    usage();
mafFragCheck(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
return 0;
}
