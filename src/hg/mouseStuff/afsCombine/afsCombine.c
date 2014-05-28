/* afsCombine - Combine output from multiple runs of aliFragScore. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "afsCombine - Combine output from multiple runs of aliFragScore\n"
  "usage:\n"
  "   afsCombine inDir outFile\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct scoredFrag
/* A structure that holds scores of a bunch of frags. */
    {
    struct scoredFrag *next;	/* Next in list. */
    int perfectCount;/* Number of perfect matches */
    int posCount;    /* Number of matches with positive total. */
    double total;    /* Total score of all hits to target dna. */
    double posTotal; /* Total score of all positive scoring hits. */
    char frag[1];    /* Sequence associated with fragment, dynamically allocated to right size */
    };

void foldIn(char *fileName, struct hash *hash, struct scoredFrag **pList)
/* Read file and add contents to hash/list. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[5];
struct scoredFrag *frag;

while (lineFileRow(lf, row))
    {
    char *name = row[0];
    if ((frag = hashFindVal(hash, name)) == NULL)
        {
	frag = needMem(sizeof(*frag) + strlen(name));
	strcpy(frag->frag, name);
	hashAdd(hash, name, frag);
	slAddHead(pList, frag);
	}
    frag->perfectCount += lineFileNeedNum(lf, row, 1);
    frag->posCount += lineFileNeedNum(lf, row, 2);
    frag->posTotal += atof(row[3]);
    frag->total += atof(row[4]);
    }
lineFileClose(&lf);
}

void afsCombine(char *inDir, char *outFile)
/* afsCombine - Combine output from multiple runs of aliFragScore. */
{
struct hash *hash = newHash(16);
struct scoredFrag *fragList = NULL, *frag;
struct fileInfo *fi, *fiList = listDirX(inDir, "*", TRUE);
FILE *f;

for (fi = fiList; fi != NULL; fi = fi->next)
   foldIn(fi->name, hash, &fragList);
slReverse(&fragList);
f = mustOpen(outFile, "w");
for (frag = fragList; frag != NULL; frag = frag->next)
    {
    fprintf(f, "%s\t%d\t%d\t%f\t%f\n", frag->frag, frag->perfectCount, frag->posCount,
    	frag->posTotal, frag->total);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
afsCombine(argv[1], argv[2]);
return 0;
}
