/* maskOutFa - Produce a masked .fa file given an unmasked .fa and a RepeatMasker .out file. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "maskOutFa - Produce a masked .fa file given an unmasked .fa and a RepeatMasker .out file\n"
  "usage:\n"
  "   maskOutFa XXX\n");
}

void maskOutFa(char *XXX)
/* maskOutFa - Produce a masked .fa file given an unmasked .fa and a RepeatMasker .out file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
maskOutFa(argv[1]);
return 0;
}
