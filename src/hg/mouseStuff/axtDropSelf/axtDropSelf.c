/* axtDropSelf - Drop alignments that just align same thing to itself. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "axt.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtDropSelf - Drop alignments that just align same thing to itself\n"
  "usage:\n"
  "   axtDropSelf in.axt out.axt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtDropSelf(char *inFile, char *outFile)
/* axtDropSelf - Drop alignments that just align same thing to itself. */
{
FILE *f = mustOpen(outFile, "w");
struct lineFile *lf = lineFileOpen(inFile, TRUE);
struct axt *axt;
while ((axt = axtRead(lf)) != NULL)
    {
    if (axt->qStart != axt->tStart || axt->qEnd != axt->tEnd ||
        axt->qStrand != axt->tStrand || !sameString(axt->qName, axt->tName))
	{
	axtWrite(axt,f);
	}
    axtFree(&axt);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
axtDropSelf(argv[1], argv[2]);
return 0;
}
