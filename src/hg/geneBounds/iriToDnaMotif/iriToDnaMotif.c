/* iriToDnaMotif - Convert improbRunInfo to dnaMotif. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "improbRunInfo.h"
#include "dnaMotif.h"
#include "dnaMotifSql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "iriToDnaMotif - Convert improbRunInfo to dnaMotif\n"
  "usage:\n"
  "   iriToDnaMotif improbRunInfo.txt dnaMotif.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void iriToDnaMotif(char *inName, char *outName)
/* iriToDnaMotif - Convert improbRunInfo to dnaMotif. */
{
FILE *f = mustOpen(outName, "w");
static struct dnaMotif motif;
struct improbRunInfo *iriList = improbRunInfoLoadAll(inName);
struct improbRunInfo *iri;

for (iri = iriList; iri != NULL; iri = iri->next)
    {
    motif.name = iri->name;
    motif.columnCount = iri->columnCount;
    motif.aProb = iri->aProb;
    motif.cProb = iri->cProb;
    motif.gProb = iri->gProb;
    motif.tProb = iri->tProb;
    dnaMotifTabOut(&motif, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
iriToDnaMotif(argv[1], argv[2]);
return 0;
}
