/* mafToBigMaf - Put ucsc standard maf file into bigMaf format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafToBigMaf - Put ucsc standard maf file into bigMaf format\n"
  "usage:\n"
  "   mafToBigMaf referenceDb input.maf out.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void packageAli(FILE *out, struct mafAli *ali)
{
}

void mafToBigMaf(char *referenceDb, char *inputMaf, char *outBed)
/* mafToBigMaf - Put ucsc standard maf file into bigMaf format. */
{
struct mafFile *mf = mafOpen(inputMaf);
FILE *out = mustOpen(outBed, "w");

struct mafAli *ali;
char lastChrom[1024];
*lastChrom = 0;
unsigned lastEnd = 0;
while ((ali = mafNext(mf)) != NULL)
    {
    struct mafComp *master = ali->components;
    char *dot = strchr(master->src, '.');

    if (dot == NULL)
        errAbort("maf block on line %d missing properly formated reference sequence", mf->lf->lineIx);

    *dot = 0;
    char *chrom = dot + 1;

    if (differentString(referenceDb, master->src))
        errAbort("reference databases (%s) must be first component of every block on line %d", referenceDb, mf->lf->lineIx);

    if (master->strand != '+')
        errAbort("reference sequence has to be on positive strand on line %d", mf->lf->lineIx);

    if (sameString(lastChrom, chrom))
        {
        if (master->start < lastEnd)
            errAbort("reference sequence out of order on line %d (start address %d is less than last end address %d)", mf->lf->lineIx, master->start, lastEnd);
        }
    else
        safecpy(lastChrom, sizeof lastChrom, chrom);

    lastEnd = master->start + master->size;

    fprintf(out, "%s\t%d\t%d\t", chrom, master->start, lastEnd);
    *dot = '.';
    mafWriteDelimiter(out, ali, ';');
    fputc('\n', out);
    mafAliFree(&ali);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
mafToBigMaf(argv[1],argv[2],argv[3]);
return 0;
}
