/* nibFrag - Extract part of a nib file as .fa. */
#include "common.h"
#include "dnaseq.h"
#include "nib.h"
#include "options.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "nibFrag - Extract part of a nib file as .fa\n"
  "usage:\n"
  "   nibFrag [options] file.nib start end strand out.fa\n"
  "options:\n"
  "   -masked - use lower case characters for masked-out bases\n"
  "   -upper - use uppper case characters for all bases\n"
  "   -name=name Use given name after '>' in output sequence\n"
  );
}

void nibFrag(int options, char *nibFile, int start, int end, char strand, char *faFile, int optUpper)
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
seq = nibLoadPartMasked(options, nibFile, start, end-start);
if (strand == '-')
    reverseComplement(seq->dna, seq->size);
if (optUpper == 1)
    touppers(seq->dna);
faWrite(faFile, optionVal("name", seq->name), seq->dna, seq->size);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int options = 0;
int optUpper = 0;
optionHash(&argc, argv);
if (optionExists("masked"))
    options = NIB_MASK_MIXED;
if (optionExists("upper"))
    optUpper = 1;

if (argc != 6)
    {
    usage();
    }
if (!isdigit(argv[2][0]) || !isdigit(argv[3][0]))
    {
    usage();
    }
nibFrag(options, argv[1], atoi(argv[2]), atoi(argv[3]), argv[4][0], argv[5], optUpper);
return 0;
}
