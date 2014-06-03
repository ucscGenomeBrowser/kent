/* txGeneAltProt - Figure out statistics on number of alternative proteins produced by alt-splicing.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneAltProt - Figure out statistics on number of alternative proteins produced by alt-splicing.\n"
  "usage:\n"
  "   txGeneAltProt in.pep in.isoforms out.protCluster\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void outputCluster(int id, struct slName *list, FILE *f)
/* Output cluster. */
{
if (list != NULL)
    {
    fprintf(f, "%d\t%d\t", id, slCount(list));
    struct slName *el;
    for (el = list; el != NULL; el = el->next)
        fprintf(f, "%s,", el->name);
    fprintf(f, "\n");
    }
}

void txGeneAltProt(char *pepFile, char *isoformsFile, char *outFile)
/* txGeneAltProt - Figure out statistics on number of alternative proteins produced by alt-splicing.. */
{
struct hash *pepHash = faReadAllIntoHash(pepFile, dnaUpper);
struct hash *totalUniqHash = hashNew(18);
uglyf("Read %d from %s\n", pepHash->elCount, pepFile);
int lastClusterId = -1;
struct hash *uniqHash = NULL;
struct slName *clusterList = NULL;
FILE *f = mustOpen(outFile, "w");
struct lineFile *lf = lineFileOpen(isoformsFile, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    {
    int clusterId = lineFileNeedNum(lf, row, 0);
    char *tx = row[1];
    if (clusterId != lastClusterId)
        {
	if (uniqHash != NULL)
	    {
	    outputCluster(lastClusterId, clusterList, f);
	    hashFree(&uniqHash);
	    slFreeList(&clusterList);
	    }
	uniqHash = hashNew(0);
	}
    lastClusterId = clusterId;
    struct dnaSeq *pep = hashFindVal(pepHash, tx);
    if (pep != NULL)
         {
	 if (!hashLookup(uniqHash, pep->dna))
	     {
	     hashAdd(uniqHash, pep->dna, NULL);
	     slNameAddTail(&clusterList, tx);
	     }
	 if (!hashLookup(totalUniqHash, pep->dna))
	     hashAdd(totalUniqHash, pep->dna, NULL);
	 }
    }
outputCluster(lastClusterId, clusterList, f);
verbose(1, "%d total unique proteins\n", totalUniqHash->elCount);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
txGeneAltProt(argv[1], argv[2], argv[3]);
return 0;
}
