/* stitcher.c - Stitches together multiple short seven state alignments into one big
 * alignment.
 */
#include "common.h"
#include "dnautil.h"
#include "xenalign.h"

void usage()
{
errAbort("stitcher - third pass of genomic/genomic alignment. Stitches\n"
         "together 2000x5000 base 7-state alignments into longer contigs.\n"
         "usage:\n"
         "      stitcher in.dyn out.sti\n"
         "      stitcher in.dyn out.st compact\n");
}

int main(int argc, char *argv[])
{
boolean compact = FALSE;
FILE *out;

if (argc < 3)
    {
    usage();
    }
if (argc > 3)
    {
    if (sameString(argv[3], "compact"))
        compact = TRUE;
    else
        usage();
    }
out = mustOpen(argv[2], "w");
uglyf("Stitching %s to %s\n", argv[1], argv[2]);
xenStitcher(argv[1], out, 150000, compact);
return 0;
}