/* faFrag - Extract a piece of DNA from a .fa file.. */
#include "common.h"
#include "dnaseq.h"
#include "fa.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "faFrag - Extract a piece of DNA from a .fa file.\n"
  "usage:\n"
  "   faFrag in.fa start end out.fa\n"
  "options:\n"
  "   -mixed - preserve mixed-case in FASTA file\n");
}

void faFrag(char *inName, int start, int end, char *outName, boolean mixed)
/* faFrag - Extract a piece of DNA from a .fa file.. */
{
FILE *outF = mustOpen(outName, "w");
if (start >= end)
    usage();
struct dnaSeq *seqList, *seq;
if (mixed)
    seqList = faReadAllMixed(inName);
else
    seqList = faReadAllDna(inName);
int seqCount = 0;
for (seq = seqList;  seq != NULL;  seq = seq->next)
    {
    int clippedEnd = end;
    if (end > seq->size)
        {
        clippedEnd = seq->size;
        if (start >= clippedEnd)
            warn("Sorry, %s is too short (%d bases), skipping", seq->name, seq->size);
        else
            warn("%s only has %d bases, truncating", seq->name, seq->size);
        }
    if (start < clippedEnd)
        {
        char name[512];
        safef(name, sizeof(name), "%s:%d-%d", seq->name, start, clippedEnd);
        faWriteNext(outF, name, seq->dna + start, clippedEnd-start);
        seqCount++;
        }
    }
carefulClose(&outF);
verbose(2, "Wrote %d bases from %d sequences to %s\n", end-start, seqCount, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
if (!isdigit(argv[2][0]) || !isdigit(argv[3][0]))
    usage();
faFrag(argv[1], atoi(argv[2]), atoi(argv[3]), argv[4], optionExists("mixed"));
return 0;
}
