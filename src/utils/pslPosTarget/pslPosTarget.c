/* pslPosTarget - flip psl strands so target is positive. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslPosTarget - flip psl strands so target is positive and implicit\n"
  "usage:\n"
  "   pslPosTarget inPsl outPsl\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void pslPosTarget(char *pslFileName, char *outFileName)
/* pslPosTarget - flip psl strands so target is positive. */
{
struct lineFile *pslLf = pslFileOpen(pslFileName);
FILE *out = mustOpen(outFileName, "w");
struct psl *psl;

while ((psl = pslNext(pslLf)) != NULL)
    {
    if (psl->strand[1] == '-')
        pslRc(psl);
    psl->strand[1] = '\0';
    pslTabOut(psl, out);
    pslFree(&psl);
    }
lineFileClose(&pslLf);
fclose(out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
pslPosTarget(argv[1], argv[2]);
return 0;
}
