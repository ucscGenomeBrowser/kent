/* This program finds an alignment between a short and a long
 * .fa file and and prints the result as a .html file. */

#include "common.h"         /* String, list, file utilities. */
#include "dnautil.h"       /* Handy utilities on DNA. */
#include "dnaseq.h"         /* DNA sequence - byte per nucleotide. */
#include "fa.h"             /* Read/write fasta files. */
#include "fuzzyFind.h"      /* Local aligner for similar sequences. */
#include "patSpace.h"       /* Fast homology finder for similar sequences. */

void usage()
/* Print usage instructions and exit. */
{
    errAbort("forChuck - Aligns a short (8 kb or less) query sequence to a\n"
             "long target sequence and prints some stats about the alignment.\n"
             "Usage:\n"
             "    forChuck query.fa target.fa\n");
}

int main(int argc, char *argv[])
{
char *queryName, *targetName;
struct dnaSeq *query, *target;
struct patSpace *ps;
struct patClump *clumpList, *clump;

/* Check command line arguments and assign to local variables. */
if (argc != 3)
    usage();
queryName = argv[1];
targetName = argv[2];

/* Read in DNA from fasta files and check query not too big.
 * The 8000 query limit is soft.  Performance gradually degrades 
 * on longer sequences.  It's starts hurting around 8000. */
query = faReadDna(queryName);
if (query->size > 8000)
    errAbort("Query sequence is %d bases, can only handle 8000", query->size);
target = faReadDna(targetName);

/* Make a pattern space query structure and use it once.
 * (This is really optimized for multiple queries to the
 * same target). */
ps = makePatSpace(&target, 1, NULL, 4, 500);
clumpList = patSpaceFindOne(ps, query->dna, query->size);
freePatSpace(&ps);

/* For each homology clump patSpace finds, do a fuzzyFinder
 * alignment of it and print the results. */
printf("Found %d homologous regions\n", slCount(clumpList));
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    struct ffAli *ali, *a;
    boolean isRc;
    int score;
    struct dnaSeq *t = clump->seq; /* There could be multiple seq's in patSpace. */

    if (ffFindAndScore(query->dna, query->size,
                       t->dna + clump->start, clump->size,
                       ffCdna, &ali, &isRc, &score))
        {
        printf("Found alignment score %d\n", score);
        printf("Blocks are:\n");
        for (a = ali; a != NULL; a = a->right)
            {
            printf("Q %4d - %4d\t T %4d -%4d\n", 
                a->nStart - query->dna, a->nEnd - query->dna,
                a->hStart - t->dna, a->hEnd - t->dna);
            }
        printf("\n");
        ffFreeAli(&ali);
        }
    else
        {
        printf("Couldn't align clump at %s %d-%d\n",
            t->name, clump->start, clump->start + clump->size);
        }
    }
slFreeList(&clumpList);
return 0;
}
