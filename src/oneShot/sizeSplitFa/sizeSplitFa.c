/* sizeSplitFa - split .fa file into two based on size of sequences. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"

void writeLines50(FILE *f, DNA *dna)
/* Write DNA 50 bp to a line. */
{
int sizeLeft = strlen(dna);
int lineSize;
while (sizeLeft > 0)
    {
    lineSize = 50;
    if (lineSize > sizeLeft)
        lineSize = sizeLeft;
    mustWrite(f, dna, lineSize);
    fputc('\n', f);
    dna += lineSize;
    sizeLeft -= lineSize;
    }
}

int main(int argc, char *argv[])
{
char *origName, *smallName, *largeName;
int threshold, startSkip, endSkip;
FILE *orig, *small, *large;
char *comment;
struct dnaSeq *seq;
int seqCount = 0;

if (argc != 7 || !isdigit(argv[2][0]))
    {
    errAbort("sizeSplitFa - split .fa file into two based on size and skip some on either end\n"
             "usage:\n"
             "    sizeSplitFa orig.fa threshold small.fa large.fa startSkip endSkip");
    }
origName = argv[1];
threshold = atoi(argv[2]);
smallName = argv[3];
largeName = argv[4];
startSkip = atoi(argv[5]);
endSkip = atoi(argv[6]);

orig = mustOpen(origName, "r");
small = mustOpen(smallName, "w");
large = mustOpen(largeName, "w");

while (faReadNext(orig, NULL, TRUE, &comment, &seq) )
    {
    FILE *f = (seq->size <= threshold ? small : large);
    int size = seq->size;
    if ((++seqCount % 1000) == 0)
        printf("Processing sequence %d\n", seqCount);
    if (size > startSkip + endSkip)
        {
        DNA *dna = seq->dna;
        dna[size-endSkip] = 0;
        dna += startSkip;
        fputs(comment, f);
        writeLines50(f, dna);
        freez(&comment);
        freeDnaSeq(&seq);
        }
    } 
return 0;
}