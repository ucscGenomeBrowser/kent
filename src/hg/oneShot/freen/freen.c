/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "jksql.h"
#include "genePred.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen in out");
}

void genePredOffset(struct genePred *gp, int offset)
/* Add offset to gene prediction. */
{
int i;
gp->txStart += offset;
gp->txEnd += offset;
gp->cdsStart += offset;
gp->cdsEnd += offset;
for (i=0; i<gp->exonCount; ++i)
    {
    gp->exonStarts[i] += offset;
    gp->exonEnds[i] += offset;
    }
}

void freen(char *input, char *output)
{
FILE *f = mustOpen(output, "w");
struct lineFile *lf = NULL;
char *row[50];
struct genePred *gp;

lf = lineFileOpen(input, TRUE);
while (lineFileChop(lf, row))
    {
    gp = genePredLoad(row);
    genePredOffset(gp, 100000);
    genePredTabOut(gp, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
   usage();
freen(argv[1], argv[2]);
}
