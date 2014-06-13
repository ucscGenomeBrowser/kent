/* edwSolexaToSangerFastq - Subtract 31 from each quality to get from solexe to sanger format.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fq.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwSolexaToSangerFastq v1 - Subtract 31 from each quality to get from solexe to sanger format.\n"
  "usage:\n"
  "   edwSolexaToSangerFastq inSolexa.fq outSanger.fq\n"
  "The in.fq can be gzipped.  Use 'stdin' or 'stdout' as a file name to use in a pipe.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void fixSolexaQual(unsigned char *qual)
/* Subtract 31 from solexa qual to get it into our range */
{
unsigned char q;
while ((q = *qual) != 0)
    {
    *qual = q - 31;
    ++qual;
    }
}

boolean isAllSolexa(unsigned char *qual)
/* Return TRUE if plausibly all solexa format quals. */
{
unsigned char q;
while ((q = *qual++) != 0)
    if (q <= 58)
        return FALSE;
return TRUE;
}
    
void edwSolexaToSangerFastq(char *inSolexa, char *outSanger)
/* edwSolexaToSangerFastq - Subtract 31 from each quality to get from solexa to sanger format.. */
{
struct lineFile *lf = lineFileOpen(inSolexa, TRUE);
FILE *f = mustOpen(outSanger, "w");
struct fq  *fq;
while ((fq = fqReadNext(lf)) != NULL)
    {
    if (!isAllSolexa(fq->quality))
        errAbort("Quality line %d of %s has non-Solexa scores", lf->lineIx, lf->fileName);
    else
        fixSolexaQual(fq->quality);
    fprintf(f, "%s\n%s\n+\n%s\n", fq->header, fq->dna, fq->quality);
    if (ferror(f))
	errAbort("Write error %s", outSanger);
    fqFree(&fq);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwSolexaToSangerFastq(argv[1], argv[2]);
return 0;
}
