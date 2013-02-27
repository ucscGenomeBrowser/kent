/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "basicBed.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen val desiredVal\n");
}

void freen(char *input, char *desiredOutput)
/* Test some hair-brained thing. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
char *row[12];
struct bed *bed;
AllocVar(bed);
while (lineFileNextRow(lf, row, ArraySize(row)))
    {
    printf("%d %s\n", lf->lineIx, row[3]);
    // bed = bedLoad12(row);
    loadAndValidateBed(row, 12, 12, lf, bed, NULL, FALSE);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
freen(argv[1], argv[2]);
return 0;
}
