/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "jksql.h"
#include "xAli.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen output");
}

void freen(char *output)
{
FILE *f = NULL;
struct lineFile *lf = NULL;
char *line;
struct xAli xa;
struct xAli *x;

xa.matches = 20;
xa.misMatches = 0;
xa.repMatches = 0;
xa.nCount = 0;
xa.qNumInsert = 1;
xa.qBaseInsert = 10;
xa.tNumInsert = 1;
xa.tBaseInsert = 20;
strcpy(xa.strand, "++");
xa.qName = "query";
xa.qSize = 100;
xa.qStart = 10;
xa.qEnd = 40;
xa.tName = "target";
xa.tSize = 10;
xa.tEnd = 50;
xa.blockCount = 2;
AllocArray(xa.blockSizes,2);
xa.blockSizes[0] = 10;
xa.blockSizes[1] = 10;
AllocArray(xa.qStarts,2);
xa.qStarts[0] = 10;
xa.qStarts[1] = 30;
AllocArray(xa.tStarts,2);
xa.tStarts[0] = 10;
xa.tStarts[1] = 40;
AllocArray(xa.qSeq, 2);
xa.qSeq[0] = "AAAAATTTTT";
xa.qSeq[1] = "GGGGGCCCCC";
AllocArray(xa.tSeq, 2);
xa.tSeq[0] = "AAAAATTTTT";
xa.tSeq[1] = "GGGGGCCCCC";

f = mustOpen(output, "w");
xAliTabOut(&xa, f);
carefulClose(&f);

x = xAliLoadAll(output);

f = mustOpen(output, "w");
xAliCommaOut(x, f);
fprintf(f,"\n");
carefulClose(&f);

lf = lineFileOpen(output, TRUE);
while (lineFileNext(lf, &line, NULL))
    {
    x = xAliCommaIn(&line, NULL);
    xAliTabOut(x, stdout);
    }

}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
}
