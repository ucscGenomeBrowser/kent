/* faToNib - Convert from .fa to .nib format. */
#include "common.h"
#include "nib.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faToNib - Convert from .fa to .nib format\n"
  "usage:\n"
  "   faToNib in.fa out.nib\n");
}

void faToNib(char *in, char *out)
/* faToNib - Convert from .fa to .nib format. */
{
struct dnaSeq *seq = faReadDna(in);
nibWrite(seq, out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
faToNib(argv[1], argv[2]);
return 0;
}
