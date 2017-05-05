/* bigMafToMaf - convert bigMaf to maf file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "bigBed.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigMafToMaf - convert bigMaf to maf file\n"
  "usage:\n"
  "   bigMafToMaf bigMaf.bb file.maf\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void bigMafToMaf(char *bigBed, char *mafFile)
/* bigMafToMaf - convert bigMaf to maf file. */
{
struct bbiFile *bbi = bigBedFileOpen(bigBed);
FILE *f = mustOpen(mafFile, "w");
fprintf(f, "##maf version=1\n");
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    int start = 0, end = chrom->size;
    int itemsLeft = 0;
    char *chromName = chrom->name;

    struct lm *lm = lmInit(0);
    struct bigBedInterval  *bbList = bigBedIntervalQuery(bbi, chromName,
            start, end, itemsLeft, lm);
    
    for(; bbList; bbList = bbList->next)
        {
        char *ptr = bbList->rest;

        for(; *ptr; ptr++)
            if (*ptr == ';')
                fputc('\n', f);
            else
                fputc(*ptr, f);
        fputc('\n', f);
        }

    lmCleanup(&lm);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bigMafToMaf(argv[1], argv[2]);
return 0;
}
