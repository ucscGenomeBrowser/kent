/* iriToControlTable - Convert improbizer run to simple list of control scores. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "improbRunInfo.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "iriToControlTable - Convert improbizer run to simple list of control scores\n"
  "usage:\n"
  "   iriToControlTable inFile outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void iriToControlTable(char *inName, char *outDir)
/* iriToControlTable - Convert improbizer run to simple list of control scores. */
{
char outName[512];
struct lineFile *lf = lineFileOpen(inName, TRUE);
char *row[improbRunInfoRowSize];

makeDir(outDir);
while (lineFileRow(lf, row))
    {
    struct improbRunInfo *iri = improbRunInfoLoad(row);
    int i;
    FILE *f = NULL;
    sprintf(outName, "%s/%s", outDir, iri->name);
    f = mustOpen(outName, "w");
    for (i=0; i<iri->controlCount; ++i)
	fprintf(f, "%f\n", iri->controlScores[i]);
    carefulClose(&f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
iriToControlTable(argv[1], argv[2]);
return 0;
}
