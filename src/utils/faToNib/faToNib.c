/* faToNib - Convert from .fa to .nib format. */
#include "common.h"
#include "options.h"
#include "nib.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faToNib - Convert from .fa to .nib format\n"
  "usage:\n"
  "   faToNib [options] in.fa out.nib\n"
  "options:\n"
  "   -masked - use lower-case characters masked-out based\n");
}

void faToNib(int options, char *in, char *out)
/* faToNib - Convert from .fa to .nib format. */
{
struct dnaSeq *seq = faReadAllMixed(in);
nibWriteMasked(options, seq, out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int options = 0;
optionHash(&argc, argv);
if (optionExists("masked"))
    options = NIB_MASK_MIXED;
if (argc != 3)
    usage();
faToNib(options, argv[1], argv[2]);
return 0;
}
