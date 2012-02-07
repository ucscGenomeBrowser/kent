/* ultraPcrRegions - Get regions to PCR up and some surrounding sequence. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "bed.h"
#include "hdb.h"
#include "nib.h"
#include "dnaseq.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ultraPcrRegions - Get regions to PCR up and some surrounding sequence\n"
  "usage:\n"
  "   ultraPcrRegions database ultraBed outputPcr.fa\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void ultraPcrRegions(char *database, char *bedFile, char *outFa)
/* ultraPcrRegions - Get regions to PCR up and some surrounding sequence. */
{
int extraSize = 1000;
FILE *f = mustOpen(outFa, "w");
struct bed *bed, *bedList = bedLoadNAll(bedFile, 4);
hSetDb(database);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    int bedSize = bed->chromEnd - bed->chromStart;
    int chromSize = hChromSize(bed->chrom);
    int seqSize;
    int seqStart = bed->chromStart - extraSize;
    int seqEnd = bed->chromEnd + extraSize;
    int firstParenPos, secondParenPos;
    struct dyString *dy;
    char fileName[512];
    struct dnaSeq *seq;
    if (seqStart < 0)
        seqStart = 0;
    if (seqEnd > chromSize)
        seqEnd = chromSize;
    seqSize = seqEnd - seqStart;
    firstParenPos = bed->chromStart - seqStart;
    secondParenPos = firstParenPos + bedSize;
    seq = hChromSeqMixed(bed->chrom, seqStart, seqEnd);
    dy = dyStringNew(seqSize+2);
    dyStringAppendN(dy, seq->dna, firstParenPos);
    dyStringAppendC(dy, '(');
    dyStringAppendN(dy, seq->dna+firstParenPos, secondParenPos-firstParenPos);
    dyStringAppendC(dy, ')');
    dyStringAppendN(dy, seq->dna+secondParenPos, seqSize - secondParenPos);
    faWriteNext(f, bed->name, dy->string, dy->stringSize);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
ultraPcrRegions(argv[1], argv[2], argv[3]);
return 0;
}
