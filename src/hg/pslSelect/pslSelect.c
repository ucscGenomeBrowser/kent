/* pslSelect - select records from a PSL file  */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "localmem.h"
#include "psl.h"

static char const rcsid[] = "$Id: pslSelect.c,v 1.1 2003/12/27 22:22:47 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"qtPairs", OPTION_STRING},
    {NULL, 0}
};

/* global data from command line */
static char* qtPairsFile;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslSelect - select records from a PSL file.\n"
  "\n"
  "usage:\n"
  "   pslSelect [options] inPsl outPsl\n"
  "\n"
  "Must specify a selection option, currently -qtPairs is the only option\n"
  "\n"
  "Options:\n"
  "   -qtPairs=file - file is tab-seperated query and targets to select\n"
  "\n"
  );
}

struct hash *loadSelect(char *selectFile)
/* load select file. */
{
struct hash *hash = hashNew(20);
char *row[2];
struct lineFile *lf = lineFileOpen(selectFile, TRUE);
while (lineFileNextRowTab(lf, row, ArraySize(row)))
    hashAdd(hash, row[0], lmCloneString(hash->lm, row[1]));
lineFileClose(&lf);
return hash;
}

void pslSelect(char *inPsl, char *outPsl)
/* select psl */
{
struct hash* selectHash = loadSelect(qtPairsFile);
struct lineFile *inPslLf = pslFileOpen(inPsl);
FILE *outPslFh = mustOpen(outPsl, "w");
struct psl* psl;

while ((psl = pslNext(inPslLf)) != NULL)
    {
    char *target = hashFindVal(selectHash, psl->qName);
    if ((target != NULL) && sameString(target, psl->tName))
        pslTabOut(psl, outPslFh);
    pslFree(&psl);
    }

carefulClose(&outPslFh);
lineFileClose(&inPslLf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
qtPairsFile = optionVal("qtPairs", NULL);
if (qtPairsFile == NULL)
    errAbort("must specify -qtPairs");

pslSelect(argv[1], argv[2]);

return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

