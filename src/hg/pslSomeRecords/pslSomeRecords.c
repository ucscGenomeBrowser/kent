/* pslSomeRecords - Extract multiple psl records. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslSomeRecords - Extract multiple psl records\n"
  "usage:\n"
  "   pslSomeRecords pslIn listOfQNames pslOut\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *hashLines(char *fileName)
/* Read all lines in file and put them in a hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[1];
struct hash *hash = newHash(0);
while (lineFileRow(lf, row))
    hashAdd(hash, row[0], NULL);
lineFileClose(&lf);
return hash;
}

void pslSomeRecords(char *pslIn, char *listName, char *pslOut)
/* pslSomeRecords - Extract multiple psl records. */
{
struct hash *hash = hashLines(listName);
struct lineFile *lf = pslFileOpen(pslIn);
FILE *f = mustOpen(pslOut, "w");
struct psl *psl;

while ((psl = pslNext(lf) ) != NULL)
    {
    if (hashLookup(hash, psl->qName))
	pslTabOut(psl, f);
    pslFree(&psl);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
pslSomeRecords(argv[1], argv[2], argv[3]);
return 0;
}
