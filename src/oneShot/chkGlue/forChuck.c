/* This program finds an alignment between a short and a long
 * .fa file and and prints the result as a .html file. */

#include "common.h"
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "patSpace.h"

void usage()
/* Print usage instructions and exit. */
{
    errAbort("alignTwo - Aligns a short (8 kb or less) query sequence to a\n"
             "long target sequence and prints some stats about the alignment.\n"
             "Usage:\n"
             "    alignTwo query.fa target.fa\n");
}

int main(int argc, char *argv[])
{
char *queryName, *targetName, *outName;
struct dnaSeq *query, *target;
struct patSpace *ps;
struct patClump *clumpList, *clump;

/* Check command line arguments and assign to local variables. */
if (argc != 4)
    usage();
queryName = argv[1];
targetName = argv[2];
outName = argv[3];

/* Read in DNA from fasta files. */
query = faReadDna(queryName);
target = faReadDna(targetName);

/* Make a pattern space query structure and use it once.
 * (This is really optimized for multiple queries to the
 * same target). */
ps = makePatSpace(&target, 1, NULL, 4, 500);
clumpList = patSpaceFindOne(ps, query->dna, query->size);
freePatSpace(&ps);

/* For each homology clump patSpace finds, do a fuzzyFinder
 * alignment of it and print the results. */
printf("Found %d homologous regions\n", slCount(clumpList);
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    struct ffAli *ali, *a;
    boolean isRc;
    int score;
    DNA *needle = query->dna + clump->qStart;
    DNA *haystack = target->dna + clump->tStart;

    if (ffFindAndScore(needle, clump->qSize,
                       haystack, clump->tSize,
                       ffCdna, &ali, &score))
        {
        printf("Found alignment score %d\n", score);
        printf("Blocks are:\n");
        for (a = ali; a != NULL; a = a->right)
            {
            printf("Q %d-%d\t T %d-%d\n", 
                a->nStart - query->dna, a->nEnd - query->dna,
                a->hStart - target->dna, a->hEnd - target->dna);
            }
        printf("\n");
        ffFreeAli(&ali);
        }
    else
        {
        printf("Couldn't align clump Q %d-%d T %d-%d\n",
            clump->qStart, clump->qStart + clump->qSize,
            clump->tStart, clump->tStart + clump->tSize);
        }
    }
slFreeList(&clump);
}
