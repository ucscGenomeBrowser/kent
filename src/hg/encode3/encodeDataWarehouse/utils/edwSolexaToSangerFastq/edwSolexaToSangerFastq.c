/* edwSolexaToSangerFastq - Subtract 31 from each quality to get from solexe to sanger format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fq.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwSolexaToSangerFastq - Subtract 31 from each quality to get from solexe to sanger format.\n"
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

struct fq3 *fq3ReadNext(struct lineFile *lf)
/* Read next record, return it as fq3. */
{
struct fq3 *fq3;
AllocVar(fq3);
char *line;

/* Deal with initial line starting with '@' */
if (!lineFileNextReal(lf, &line))
    return FALSE;
if (line[0] != '@')
    {
    errAbort("Expecting line starting with '@' got %s line %d of %s", 
	line, lf->lineIx, lf->fileName);
    }
fq3->atLine = cloneString(line);

/* Deal with line containing sequence. */
if (!lineFileNext(lf, &line,  NULL))
    errAbort("%s truncated in middle of record", lf->fileName);
fq3->seqLine = cloneString(line);

/* Check for + line */
if (!lineFileNext(lf, &line,  NULL))
    errAbort("%s truncated in middle of record", lf->fileName);
if (line[0] != '+')
    errAbort("Expecting + line %d of %s", lf->lineIx, lf->fileName);

/* Get quality line */
if (!lineFileNext(lf, &line,  NULL))
    errAbort("%s truncated in middle of record", lf->fileName);
fq3->qualLine = cloneString(line);
return fq3;
}

void fq3Free(struct fq3 **pFq3)
/* Read next record, return it as fq3. */
{
struct fq3 *fq3 = *pFq3;
if (fq3 != NULL)
    {
    freeMem(fq3->atLine);
    freeMem(fq3->seqLine);
    freeMem(fq3->qualLine);
    freez(pFq3);
    }
}

void fixSolexaQual(char *qual)
/* Subtract 31 from solexa qual to get it into our range */
{
char q;
while ((q = *qual) != 0)
    {
    *qual = q - 31;
    ++qual;
    }
}

boolean isAllSolexa(char *qual)
/* Return TRUE if plausibly all solexa format quals. */
{
char q;
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
struct fq3  *fq3;
while ((fq3 = fq3ReadNext(lf)) != NULL)
    {
    if (!isAllSolexa(fq3->qualLine))
        errAbort("Quality line %d of %s has non-Solexa scores", lf->lineIx, lf->fileName);
    else
        fixSolexaQual(fq3->qualLine);
    fprintf(f, "%s\n%s\n+\n%s\n", fq3->atLine, fq3->seqLine, fq3->qualLine);
    if (ferror(f))
	errAbort("Write error %s", outSanger);
    fq3Free(&fq3);
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
