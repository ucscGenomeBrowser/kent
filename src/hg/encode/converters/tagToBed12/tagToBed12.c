/* tagToBed12 - Convert tagAlign format to bed 12 + 2. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed12wSeq.h"
#include "tagAlign.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagToBed12 - Convert tagAlign format to bed 12 + 2\n"
  "     where bed12 + 2 is bed12 with two more fields having the"
  "     pair sequences in them ( the second is blank for tagAligns)"
  "usage:\n"
  "   tagToBed12 file.tagAlign file.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void tagToBed12(char *pairTagFile, char *bedFile)
/* tagToBed12 - Convert tagAlign format to bed 12 + 2. */
{
struct lineFile *lf = lineFileOpen(pairTagFile, TRUE);
FILE *f = mustOpen(bedFile, "w");
struct tagAlign *pt;
struct bed12wSeq bed;
char *row[6];

int blockSizes[1];
int chromStarts[1];
chromStarts[0] = 0;
bed.chromStarts = chromStarts;
bed.blockSizes = blockSizes;
bed.blockCount = 1;
bed.strand[1] = 0;

while (lineFileRow(lf, row))
    {
    pt = tagAlignLoad(row);

    bed.chrom = pt->chrom;
    bed.chromStart = pt->chromStart;
    bed.thickStart = pt->chromStart;
    bed.chromEnd = pt->chromEnd;
    bed.thickEnd = pt->chromEnd;
    bed.name = pt->sequence;
    bed.score = pt->score;
    bed.strand[0] = pt->strand;
    bed.seq1 = pt->sequence;
    bed.seq2 = "X";
    bed.reserved = 0;

    blockSizes[0] = strlen(pt->sequence);

    assert(bed.chromEnd = bed.chromStart + blockSizes[0]);

    bed12wSeqOutput(&bed, f, '\t', '\n');

    tagAlignFree(&pt);
    }

lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tagToBed12(argv[1], argv[2]);
return 0;
}
