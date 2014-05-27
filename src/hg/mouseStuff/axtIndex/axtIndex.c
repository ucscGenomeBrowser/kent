/* axtIndex - index axt file by range. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtIndex - build index of axt file\n"
  "usage:\n"
  "   axtIndex in.axt out.axt.ix\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtIndex(char *in, char *out)
/* axtIndex - Create summary file for axt. */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f = mustOpen(out, "w");
struct axt *axt;

for (;;)
    {
    off_t pos = lineFileTell(lf);
    axt = axtRead(lf);
    if (axt == NULL)
        break;
    fprintf(f, "%d %d %lld\n", axt->tStart, axt->tEnd - axt->tStart, (unsigned long long) pos); 
    axtFree(&axt);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
axtIndex(argv[1], argv[2]);
return 0;
}
