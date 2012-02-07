/* gfPcr - In silico PCR using gfServer index...
 * Copyright 2004 Jim Kent.  All rights reserved. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genoFind.h"
#include "gfPcrLib.h"


/* Command line overridable variables. */
int maxSize = 4000; /* corresponds to maxSize= parameter. */
int minPerfect = 15;
int minGood = 18;
char *clName = NULL;  /* corresponds to name= parameter */
char *clOut = "fa";  /* corresponds to out= parameter */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gfPcr - In silico PCR version %s using gfServer index.\n"
  "usage:\n"
  "   gfPcr host port seqDir fPrimer rPrimer output\n"
  "or\n"
  "   gfPcr host port seqDir batch output\n"
  "Where:\n"
  "   host is the name of the machine running the gfServer\n"
  "   port is the gfServer port (usually 17779)\n"
  "   seqDir is where the nib or 2bit files for the genome database are\n"
  "   fPrimer is the forward strand primer\n"
  "   rPrimer is the reverse strand primer\n"
  "   output is the output file.  Use 'stdout' for output to standard output\n"
  "   batch is a space or tab delimited file with the following fields on each line\n"
  "       name/fPrimer/rPrimer/maxProductSize\n"
  "options:\n"
  "   -maxSize=N - Maximum size of PCR product (default %d)\n"
  "   -minPerfect=N - Minimum size of perfect match at 3' end of primer (default %d)\n"
  "   -minGood=N - Minimum size where there must be 2 matches for each mismatch (default %d)\n"
  "   -out=XXX - Output format.  Either\n"
  "      fa - fasta with position, primers in header (default)\n"
  "      bed - tab delimited format. Fields: chrom/start/end/name/score/strand\n"
  "      psl - blat format.\n"
  "   -name=XXX - Name to use in bed output.\n"
  , gfVersion, maxSize, minPerfect, minGood
  );
}

static struct optionSpec options[] = {
   {"maxSize", OPTION_INT},
   {"minPerfect", OPTION_INT},
   {"minGood", OPTION_INT},
   {"out", OPTION_STRING},
   {"name", OPTION_STRING},
   {NULL, 0},
};


int main(int argc, char *argv[])
/* Process command line. */
{
struct gfPcrInput *inList = NULL;
struct gfPcrOutput *outList = NULL;
char *host, *port, *seqDir, *outFile = NULL;
optionInit(&argc, argv, options);
maxSize = optionInt("maxSize", maxSize);
minPerfect = optionInt("minPerfect", minPerfect);
minGood = optionInt("minGood", minGood);
if (minGood < minPerfect)
    minGood = minPerfect;
clOut = optionVal("out", clOut);
clName = optionVal("name", clName);
if (argc == 6)	/* Batch */
    {
    inList = gfPcrInputLoadAll(argv[4]);
    outFile = argv[5];
    }
else if (argc == 7) /* One primer pair */
    {
    AllocVar(inList);
    inList->name = clName;
    inList->fPrimer = cloneString(argv[4]);
    inList->rPrimer = cloneString(argv[5]);
    outFile = argv[6];
    }
else
    usage();
host = argv[1];
port = argv[2];
seqDir = argv[3];
outList = gfPcrViaNet(host, port, seqDir, inList, maxSize, minPerfect, minGood);
gfPcrOutputWriteAll(outList, clOut, NULL, outFile);
gfPcrOutputFreeList(&outList);
gfPcrInputFreeList(&inList);
return 0;
}
