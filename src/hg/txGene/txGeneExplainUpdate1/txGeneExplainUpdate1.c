/* txGeneExplainUpdate1 - Make table explaining correspondence between older known genes 
 * and ucsc genes.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "binRange.h"



void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneExplainUpdate1 - Make table explaining correspondence between older known genes \n"
  "and ucsc genes.\n"
  "usage:\n"
  "   txGeneExplainUpdate1 oldKg.bed newKg.bed in.bed txWalk.bed out.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct bed *findExact(struct bed *oldBed, struct hash *newHash)
/* Try and find a new bed identical with old bed. */
{
struct binKeeper *bk = hashFindVal(newHash, oldBed->chrom);
if (bk == NULL)
    return NULL;
struct bed *matchingBed = NULL;
struct binElement *bin, *binList = binKeeperFind(bk, oldBed->chromStart, oldBed->chromEnd);
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct bed *newBed = bin->val;
    if (oldBed->strand[0] == newBed->strand[0])
        {
	if (bedExactMatch(oldBed, newBed))
	    {
	    matchingBed = newBed;
	    break;
	    }
	}
    }
slFreeList(&binList);
return matchingBed;
}

struct bed *findCompatible(struct bed *oldBed, struct hash *newHash)
/* Try and find a new bed compatible with old bed. */
{
struct bed *matchingBed = NULL;
struct binKeeper *bk = hashFindVal(newHash, oldBed->chrom);
if (bk == NULL)
    return NULL;
struct binElement *bin, *binList = binKeeperFind(bk, oldBed->chromStart, oldBed->chromEnd);
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct bed *bed = bin->val;
    if (bed->strand[0] == oldBed->strand[0])
	{
	if (bedCompatibleExtension(bed, oldBed))
	    {
	    matchingBed = bed;
	    break;
	    }
	}
    }
slFreeList(&binList);
return matchingBed;
}

struct bed *findMostOverlapping(struct bed *bed, struct hash *keeperHash)
/* Try find most overlapping thing to bed in keeper hash. */
{
struct bed *bestBed = NULL;
int bestOverlap = 0;
struct binKeeper *bk = hashFindVal(keeperHash, bed->chrom);
if (bk == NULL)
    return NULL;
struct binElement *bin, *binList = binKeeperFind(bk, bed->chromStart, bed->chromEnd);
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct bed *bed2 = bin->val;
    if (bed2->strand[0] == bed->strand[0])
	{
	int overlap = bedSameStrandOverlap(bed2, bed);
	if (overlap > bestOverlap)
	    {
	    bestOverlap = overlap;
	    bestBed = bed2;
	    }
	}
    }
slFreeList(&binList);
return bestBed;
}

void explainMissing(struct bed *oldBed, char *newName, char *type,
	struct hash *txHash, struct hash *inHash, struct hash *accHash, FILE *f)
/* Write out a line explaining why old gene has gone missing. */
{
fprintf(f, "%s\t%s\t%d\t%d\t%s\t%s\t", oldBed->name, oldBed->chrom, 
	oldBed->chromStart, oldBed->chromEnd, newName, type);
if (!hashLookup(accHash, oldBed->name))
    fprintf(f, "No alignment at current increased stringency\n");
else if (!findCompatible(oldBed, inHash))
    fprintf(f, "No longer maps to this position, but maps elsewhere\n");
else if (!findCompatible(oldBed, txHash))
    fprintf(f, "Not enough supporting evidence\n");
else
    fprintf(f, "Appears incompletely processed\n");
}

void txGeneExplainUpdate1(char *oldKgFile, char *newKgFile, char *inBedFile, char *txWalkFile,
	char *outFile)
/* txGeneExplainUpdate1 - Make table explaining correspondence between older known genes 
 * and ucsc genes.. */
{
struct hash *accHash = hashNew(20);
struct bed *oldBed, *oldList = bedLoadNAll(oldKgFile, 12);
struct bed *bed, *newList = bedLoadNAll(newKgFile, 12);
struct bed *inList = bedLoadNAll(inBedFile, 12);
struct bed *txList = bedLoadNAll(txWalkFile, 12);
struct hash *newHash = bedsIntoKeeperHash(newList);
struct hash *inHash = bedsIntoKeeperHash(inList);

for (bed = inList; bed != NULL; bed = bed->next)
    {
    char *dupe = cloneString(bed->name);
    chopSuffix(dupe);
    hashAdd(accHash, dupe, bed);
    }
struct hash *txHash = bedsIntoKeeperHash(txList);
FILE *f = mustOpen(outFile, "w");
for (oldBed = oldList; oldBed != NULL; oldBed = oldBed->next)
    {
    if ((bed = findExact(oldBed, newHash)) != NULL)
        fprintf(f, "%s\t%s\t%d\t%d\t%s\texact\t\n", oldBed->name, oldBed->chrom,
		oldBed->chromStart, oldBed->chromEnd, bed->name);
    else if ((bed = findCompatible(oldBed, newHash)) != NULL)
        fprintf(f, "%s\t%s\t%d\t%d\t%s\tcompatible\t\n", oldBed->name, oldBed->chrom,
		oldBed->chromStart, oldBed->chromEnd, bed->name);
    else if ((bed = findMostOverlapping(oldBed, newHash)) != NULL)
	explainMissing(oldBed, bed->name, "overlap", txHash, inHash, accHash, f);
    else
        explainMissing(oldBed, "", "none", txHash, inHash, accHash, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
txGeneExplainUpdate1(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
