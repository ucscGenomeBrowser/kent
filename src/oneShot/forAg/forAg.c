/* Example program for Alessandro Guffanti that shows how
 * to use PatSpace and FuzzyFinder to make  EST/genomic 
 * alignments. */

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
    errAbort("forAg - Aligns a series of EST sequences to a genomic sequence.\n"
             "Usage:\n"
             "    forAg EST.fa genomic.fa celegans.ooc\n"
             "The EST.fa will ideally have a lot of ESTs in it.  The\n"
             "genomic .fa file can have several clones.  It is sadly\n"
             "limited to 8 million bases long.  You'll have to break\n"
             "up larger contigs.   celegans.ooc contains a list of\n"
             "11-mers which occur too frequently in the C. elegans\n"
             "genome to be useful for locating things.");
}

int totalSequenceSize(struct dnaSeq *seqList)
/* Returns total size of all sequences in list. */
{
struct dnaSeq *seq;
int total = 0;

for (seq = seqList; seq != NULL; seq = seq->next)
    total += seq->size;
return total;
}

void freeSeqList(struct dnaSeq **pSeqList)
/* Free an entire list of sequences */
{
struct dnaSeq *seq, *next;
for (seq = *pSeqList; seq != NULL; seq = next)
    {
    next = seq->next;
    freeDnaSeq(&seq);
    }
*pSeqList = NULL;
}

int main(int argc, char *argv[])
{
char *estName, *targetName, *oocName;
FILE *estFile;
struct dnaSeq *target;
struct dnaSeq *est;
struct patSpace *ps;
struct patClump *clumpList, *clump;
int estIx = 0;

/* Check command line arguments and assign to local variables. */
if (argc != 4)
    usage();
estName = argv[1];
estFile = mustOpen(estName, "rb");
targetName = argv[2];
oocName = argv[3];

/* Read in target DNA from fasta files and check not too big. */
fprintf(stderr, "Reading %s\n", targetName);
target = faReadAllDna(targetName);
if (totalSequenceSize(target) > 8000000)
    {
    errAbort("Can only handle 8000000 bases of genomic sequence at once, %s has %d.", 
        targetName, totalSequenceSize(target));
    }

/* Make a pattern space index structure. */
fprintf(stderr, "Making Pattern Space index\n");
ps = makePatSpace(&target, 1, oocName, 4, 32000);

/* Loop through each EST in query list. */
printf("Searching for hits\n\n");
while (faReadNext(estFile, NULL, TRUE, NULL, &est))
    {
    boolean isRc;   /* Reverse complemented? */

    if (++estIx % 5000 == 0)
        fprintf(stderr, "Processing EST %d\n", estIx);
    if (est->size > 20000)
        {
        warn("Very large EST sequence %s.\n"
             "Maybe you mixed up the EST and genomic parameters?", est->name);
        usage();
        }
    
    for (isRc = 0; isRc <= 1; ++isRc)   /* Search both strands. */
        {
        if (isRc)
            reverseComplement(est->dna, est->size);
        clumpList = patSpaceFindOne(ps, est->dna, est->size);

        /* For each homology clump patSpace finds, do a fuzzyFinder
         * alignment of it and print the results. */
        for (clump = clumpList; clump != NULL; clump = clump->next)
            {
            struct ffAli *ali, *a;
            boolean isRc;
            int score;
            struct dnaSeq *t = clump->seq;
            DNA *tStart = t->dna + clump->start;

            ali = ffFind(est->dna, est->dna+est->size, tStart, tStart + clump->size, ffCdna);
            if (ali != NULL)
                {
                score = ffScoreCdna(ali);
                printf("%s hits %s strand %c score %d\n", 
                    est->name, t->name, (isRc ? '+' : '-'), score);
                for (a = ali; a != NULL; a = a->right)
                    {
                    printf("  Q %4d - %4d\t T %4d -%4d\n", 
                        a->nStart - est->dna, a->nEnd - est->dna,
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
        }
    freeDnaSeq(&est);
    }
/* Clean up time. */
freePatSpace(&ps);
freeSeqList(&target);
return 0;
}
