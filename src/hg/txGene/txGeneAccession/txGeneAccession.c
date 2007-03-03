/* txGeneAccession - Assign permanent accession number to genes.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "binRange.h"
#include "rangeTree.h"
#include "minChromSize.h"

static char const rcsid[] = "$Id: txGeneAccession.c,v 1.4 2007/03/03 17:24:00 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneAccession - Assign permanent accession number to genes.\n"
  "usage:\n"
  "   txGeneAccession old.bed txLastId new.bed txToAcc.tab\n"
  "where:\n"
  "   old.bed is a bed file with the previous build with accessions for names\n"
  "   txLastId is a file with the current highest txId\n"
  "   new.bed is a bed file with the current build with temp IDs for names\n"
  "   txToAcc.tab is the output mapping temp IDs to accessions\n"
  "Use old.bed from previous build for new builds on same assembly.  For new\n"
  "assemblies first liftOver old.bed, then use this.\n"
  "options:\n"
  "   -test - Don't update txLastId file, so testing is reproducible\n"
  );
}

static struct optionSpec options[] = {
   {"test", OPTION_BOOLEAN},
   {NULL, 0},
};

int readNumberFromFile(char *fileName)
/* Read exactly one number from file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[1];
if (!lineFileRow(lf, row))
    errAbort("%s is empty", fileName);
int number = lineFileNeedNum(lf, row, 0);
lineFileClose(&lf);
return number;
}


boolean compatibleExtension(struct bed *oldBed, struct bed *newBed)
/* Return TRUE if newBed is a compatible extension of oldBed, meaning
 * all internal exons and all introns of old bed are contained, in the 
 * same order in the new bed. */
{
/* New bed must have at least as many exons as old bed... */
if (oldBed->blockCount > newBed->blockCount)
    return 0;

/* Look for an exact match */
int oldSize = bedTotalBlockSize(oldBed);
int newSize = bedTotalBlockSize(newBed);
int overlap = bedSameStrandOverlap(oldBed, newBed);
if (oldSize == newSize && oldSize == overlap)
    return TRUE;

/* Next handle case where old bed is a single exon.  For this
 * just require that old bed is a proper superset of new bed. */
if (oldBed->blockCount == 0)
    return overlap == oldSize;

/* Otherwise we look for first intron start in old bed, and then
 * flip through new bed until we find an intron that starts at the
 * same place. */
int oldFirstIntronStart = oldBed->chromStart + oldBed->chromStarts[0] + oldBed->blockSizes[0];
int newLastBlock = newBed->blockCount-1, oldLastBlock = oldBed->blockCount-1;
int newIx, oldIx;
for (newIx=0; newIx < newLastBlock; ++newIx)
    {
    int iStartNew = newBed->chromStart + newBed->chromStarts[newIx] + newBed->blockSizes[newIx];
    if (iStartNew == oldFirstIntronStart)
        break;
    }
if (newIx == newLastBlock)
    return FALSE;

/* Now we go through all introns in old bed, and make sure they match. */
for (oldIx=0; oldIx < oldLastBlock; ++oldIx, ++newIx)
    {
    int iStartOld = oldBed->chromStart + oldBed->chromStarts[oldIx] + oldBed->blockSizes[oldIx];
    int iEndOld = oldBed->chromStart + oldBed->chromStarts[oldIx+1];
    int iStartNew = newBed->chromStart + newBed->chromStarts[newIx] + newBed->blockSizes[newIx];
    int iEndNew = newBed->chromStart + newBed->chromStarts[newIx+1];
    if (iStartOld != iStartNew || iEndOld != iEndNew)
        return FALSE;
    }
return TRUE;
}

struct bed *findCompatible(struct bed *newBed, struct hash *oldHash)
/* Try and find an old bed compatible with new bed. */
{
struct binKeeper *bk = hashFindVal(oldHash, newBed->chrom);
if (bk == NULL)
    return NULL;
struct bed *matchingBed = NULL;
struct binElement *bin, *binList = binKeeperFind(bk, newBed->chromStart, newBed->chromEnd);
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct bed *oldBed = bin->val;
    if (oldBed->strand[0] == newBed->strand[0])
        {
	if (compatibleExtension(oldBed, newBed))
	    {
	    matchingBed = oldBed;
	    break;
	    }
	}
    }
slFreeList(&binList);
return matchingBed;
}

void txGeneAccession(char *oldBedFile, char *lastIdFile, char *newBedFile, char *outTab)
/* txGeneAccession - Assign permanent accession number to genes. */
{
/* Read in all input. */
struct bed *oldList = bedLoadNAll(oldBedFile, 12);
verbose(2, "Read %d from %s\n", slCount(oldList), oldBedFile);
struct bed *newList = bedLoadNAll(newBedFile, 12);
verbose(2, "Read %d from %s\n", slCount(newList), newBedFile);
int txId = readNumberFromFile(lastIdFile);
verbose(2, "Last txId used was %d (from %s)\n", txId, lastIdFile);

/* Make a random-access data structure for old list. */
struct hash *oldHash = bedsIntoKeeperHash(oldList);

/* Make a little hash to help prevent us from reusing an
 * old accession twice (which might happen if we extend it
 * in two incompatible ways). */
struct hash *usedHash = hashNew(16);

/* Loop through new bed looking for compatible things.  If
 * we can't find anything compatable, make up a new accession. */
FILE *f = mustOpen(outTab, "w");
struct bed *oldBed, *newBed;
for (newBed = newList; newBed != NULL; newBed = newBed->next)
    {
    oldBed = findCompatible(newBed, oldHash);
    if (oldBed == NULL || hashLookup(usedHash, oldBed->name))
        {
	char newAcc[16];
	safef(newAcc, sizeof(newAcc), "TX%08d", ++txId);
	fprintf(f, "%s\t%s\n", newBed->name, newAcc);
	}
    else
	{
	hashAdd(usedHash, oldBed->name, NULL);
        fprintf(f, "%s\t%s\n", newBed->name, oldBed->name);
	}
    }
carefulClose(&f);

if (!optionExists("test"))
    {
    FILE *fId = mustOpen(lastIdFile, "w");
    fprintf(fId, "%d\n", txId);
    carefulClose(&fId);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
txGeneAccession(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
