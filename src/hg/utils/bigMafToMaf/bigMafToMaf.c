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
  );
}

/* chunking keeps memory down */
static int chunkSizeBases = 1048576;

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static void writeMafAli(char *ptr, FILE *f)
/* output one block, change ';'  back to newline */
{
for(; *ptr; ptr++)
    {
    if (*ptr == ';')
        fputc('\n', f);
    else
        fputc(*ptr, f);
    }
fputc('\n', f);

}

static void processChromChunk(struct bbiFile *bbi, struct bbiChromInfo *chrom,
                              int start, int end, FILE *f)
/* Output one chunk.  Only blocks where start is in the range will be written
 * to avoid outputting a block multiple tines.  */
{
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom->name,
                                                    start, end, 0, lm);
for(; bbList; bbList = bbList->next)
    {
    if ((start <= bbList->start) && (bbList->start < end))
        writeMafAli(bbList->rest, f);
    }
lmCleanup(&lm);
}

static void processChrom(struct bbiFile *bbi, struct bbiChromInfo *chrom,
                         FILE *f)
/* output MAF blocks from one chrom */
{
int start = 0;
while (start < chrom->size)
    {
    int end = min(start + chunkSizeBases, chrom->size);
    processChromChunk(bbi, chrom, start, end, f);
    start = end;
    }
}

static void bigMafToMaf(char *bigBed, char *mafFile)
/* bigMafToMaf - convert bigMaf to maf file. */
{
struct bbiFile *bbi = bigBedFileOpen(bigBed);
FILE *f = mustOpen(mafFile, "w");
fprintf(f, "##maf version=1\n");
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    processChrom(bbi, chrom, f);
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
