/* pairedTagToBed12 - Convert pairedTagAlign format to bed 12 + 2. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed12wSeq.h"
#include "pairedTagAlign.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "pairedTagToBed12 - Convert pairedTagAlign format to bed 12 + 2\n"
  "     where bed12 + 2 is bed12 with two more fields having the"
  "     pair sequences in them."
  "usage:\n"
  "   pairedTagToBed12 file.pairedTagAlign file.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void pairedTagToBed12(char *pairTagFile, char *bedFile)
/* pairedTagToBed12 - Convert pairedTagAlign format to bed 12 + 2. */
{
struct lineFile *lf = lineFileOpen(pairTagFile, TRUE);
FILE *f = mustOpen(bedFile, "w");
struct pairedTagAlign *pt;
struct bed12wSeq bed;
char *row[8];

int blockSizes[2];
int chromStarts[2];
chromStarts[0] = 0;
bed.chromStarts = chromStarts;
bed.blockSizes = blockSizes;
bed.blockCount = 2;

while (lineFileRow(lf, row))
    {
    pt = pairedTagAlignLoad(row);

    bed.chrom = pt->chrom;
    bed.chromStart = pt->chromStart;
    bed.thickStart = pt->chromStart;
    bed.chromEnd = pt->chromEnd;
    bed.thickEnd = pt->chromEnd;
    bed.name = pt->name;
    bed.score = pt->score;
    bed.strand[0] = pt->strand[0];
    bed.strand[1] = pt->strand[1];
    bed.seq1 = pt->seq1;
    bed.seq2 = pt->seq2;
    bed.reserved = 0;

    blockSizes[0] = strlen(pt->seq1);
    blockSizes[1] = strlen(pt->seq2);

    chromStarts[1] = bed.chromEnd - blockSizes[1] - bed.chromStart;

    bed12wSeqOutput(&bed, f, '\t', '\n');

    pairedTagAlignFree(&pt);
    }

lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
pairedTagToBed12(argv[1], argv[2]);
return 0;
}
