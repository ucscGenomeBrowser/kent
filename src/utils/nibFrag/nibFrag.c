/* nibFrag - Extract part of a nib file as .fa. */
#include "common.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "nibFrag - Extract part of a nib file as .fa\n"
  "usage:\n"
  "   nibFrag file.nib start end strand out.fa\n");
}

void nibFrag(char *nibFile, int start, int end, char strand, char *faFile)
/* nibFrag - Extract part of a nib file as .fa. */
{
struct dnaSeq *seq;
if (strand != '+' && strand != '-')
   {
   usage();
   }
if (start >= end)
   {
   usage();
   }
seq = nibLoadPart(nibFile, start, end-start);
if (strand == '-')
    reverseComplement(seq->dna, seq->size);
faWrite(faFile, seq->name, seq->dna, seq->size);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 6)
    {
    usage();
    }
if (!isdigit(argv[2][0]) || !isdigit(argv[3][0]))
    {
    usage();
    }
nibFrag(argv[1], atoi(argv[2]), atoi(argv[3]), argv[4][0], argv[5]);
return 0;
}
