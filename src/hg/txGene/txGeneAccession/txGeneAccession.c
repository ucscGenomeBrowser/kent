/* txGeneAccession - Assign permanent accession number to genes.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "bed.h"
#include "binRange.h"
#include "rangeTree.h"
#include "sqlNum.h"
#include "minChromSize.h"
#include "txCommon.h"

static char const rcsid[] = "$Id: txGeneAccession.c,v 1.14 2008/04/28 12:25:36 kent Exp $";


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneAccession - Assign permanent accession number to genes.\n"
  "usage:\n"
  "   txGeneAccession old.bed txLastId new.bed txToAcc.tab oldToNew.tab\n"
  "where:\n"
  "   old.bed is a bed file with the previous build with accessions for names\n"
  "   txLastId is a file with the current highest txId\n"
  "   new.bed is a bed file with the current build with temp IDs for names\n"
  "   txToAcc.tab is the output mapping temp IDs to accessions\n"
  "   oldToNew.tab gives information about the relationship between the accessions\n"
  "                in old.bed and the new accessions\n"
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


struct bed *findExact(struct bed *newBed, struct hash *oldHash, struct hash *usedHash)
/* Try and find an old bed identical with new bed. */
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
	if (!hashLookup(usedHash, oldBed->name))
	    {
	    if (bedExactMatch(oldBed, newBed))
		{
		matchingBed = oldBed;
		break;
		}
	    }
	}
    }
slFreeList(&binList);
return matchingBed;
}

boolean bedIntronsIdentical(struct bed *a, struct bed *b)
/* Return TRUE if the introns of the bed are the same. */
{
int blockCount = a->blockCount;
if (blockCount != b->blockCount)
    return FALSE;
int aStart = a->chromStart, bStart = b->chromStart;
int i;
for (i=1; i<a->blockCount; ++i)
    {
    int prev = i-1;
    int iStart = aStart + a->chromStarts[prev] + a->blockSizes[prev];
    if (iStart != bStart + b->chromStarts[prev] + b->blockSizes[prev])
        return FALSE;
    int iEnd = aStart + a->chromStarts[i];
    if (iEnd != bStart + b->chromStarts[i])
        return FALSE;
    }
return TRUE;
}

boolean endUtrChange(struct bed *oldBed, struct bed *newBed)
/* Returns TRUE if newBed has same CDS as old BED, and same number of exons,
 * with only difference being length of UTR */
{
if (oldBed->thickStart != newBed->thickStart || oldBed->thickEnd != newBed->thickEnd)
    return FALSE;
return bedIntronsIdentical(oldBed, newBed);
}
   

struct bed *findCompatible(struct bed *newBed, struct hash *oldHash, struct hash *usedHash)
/* Try and find an old bed compatible with new bed. */
{
struct binKeeper *bk = hashFindVal(oldHash, newBed->chrom);
int bestDiff = BIGNUM;
struct bed *bestBed = NULL;
if (bk == NULL)
    return NULL;
struct binElement *bin, *binList = binKeeperFind(bk, newBed->chromStart, newBed->chromEnd);
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct bed *oldBed = bin->val;
    if (oldBed->strand[0] == newBed->strand[0])
	{
	if (!hashLookup(usedHash, oldBed->name))
	    {
	    if (bedCompatibleExtension(oldBed, newBed) || endUtrChange(oldBed, newBed))
		{
		int diff = bedTotalBlockSize(oldBed) - bedTotalBlockSize(newBed);
		if (diff < 0) diff = -diff;
		if (diff < bestDiff)
		    {
		    bestDiff = diff;
		    bestBed = oldBed;
		    }
		}
	    }
	}
    }
slFreeList(&binList);
return bestBed;
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

void txGeneAccession(char *oldBedFile, char *lastIdFile, char *newBedFile, char *txToAccFile,
	char *oldToNewFile)
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

/* Record our decisions in hash as well as file. */
struct hash *idToAccHash = hashNew(16);

/* Loop through new list first looking for exact matches. Record
 * exact matches in hash so we don't look for them again during
 * the next, "compatable" match phase. */
struct hash *oldExactHash = hashNew(16), *newExactHash = hashNew(16);
struct bed *oldBed, *newBed;
FILE *f = mustOpen(txToAccFile, "w");
FILE *fOld = mustOpen(oldToNewFile, "w");
for (newBed = newList; newBed != NULL; newBed = newBed->next)
    {
    oldBed = findExact(newBed, oldHash, usedHash);
    if (oldBed != NULL)
        {
	hashAdd(oldExactHash, oldBed->name, oldBed);
	hashAdd(newExactHash, newBed->name, newBed);
	hashAdd(usedHash, oldBed->name, NULL);
        fprintf(f, "%s\t%s\n", newBed->name, oldBed->name);
	hashAdd(idToAccHash, newBed->name, oldBed->name);
	fprintf(fOld, "%s:%d-%d\t%s\t%s\t%s\n", oldBed->chrom, oldBed->chromStart, oldBed->chromEnd,
		oldBed->name, oldBed->name, "exact");
	}
    }

/* Loop through new bed looking for compatible things.  If
 * we can't find anything compatable, make up a new accession. */
for (newBed = newList; newBed != NULL; newBed = newBed->next)
    {
    if (!hashLookup(newExactHash, newBed->name))
	{
	oldBed = findCompatible(newBed, oldHash, usedHash);
	if (oldBed == NULL)
	    {
	    char newAcc[16];
	    txGeneAccFromId(++txId, newAcc);
	    strcat(newAcc, ".1");
	    fprintf(f, "%s\t%s\n", newBed->name, newAcc);
	    hashAdd(idToAccHash, newBed->name, cloneString(newAcc));
	    oldBed = findMostOverlapping(newBed, oldHash);
	    char *oldAcc = (oldBed == NULL ? "" : oldBed->name);
	    fprintf(fOld, "%s:%d-%d\t%s\t%s\t%s\n", newBed->chrom, newBed->chromStart, newBed->chromEnd,
	    	oldAcc, newAcc, "new");
	    }
	else
	    {
	    char *acc = cloneString(oldBed->name);
	    char *ver = strchr(acc, '.');
	    if (ver == NULL)
	        errAbort("No version found in %s", oldBed->name);
	    *ver++ = 0;
	    int version = sqlUnsigned(ver);
	    char newAcc[16];
	    safef(newAcc, sizeof(newAcc), "%s.%d", acc, version+1);
	    hashAdd(usedHash, oldBed->name, NULL);
	    fprintf(f, "%s\t%s\n", newBed->name, newAcc);
	    hashAdd(idToAccHash, newBed->name, newAcc);
	    fprintf(fOld, "%s:%d-%d\t%s\t%s\t%s\n", newBed->chrom, newBed->chromStart, newBed->chromEnd,
	    	oldBed->name, newAcc, "compatible");
	    }
	}
    }
carefulClose(&f);

/* Make a random-access data structure for old list. */
struct hash *newHash = bedsIntoKeeperHash(newList);

/* Write record of ones that don't map. */
for (oldBed = oldList; oldBed != NULL; oldBed = oldBed->next)
    {
    if (!hashLookup(usedHash, oldBed->name))
	{
	char *newAcc = "";
	struct bed *newBed = findMostOverlapping(oldBed, newHash);
	if (newBed != NULL)
	    newAcc = hashMustFindVal(idToAccHash, newBed->name);
	fprintf(fOld, "%s:%d-%d\t%s\t%s\t%s\n", oldBed->chrom, oldBed->chromStart, oldBed->chromEnd,
		oldBed->name, newAcc, "lost");
	}
    }
carefulClose(&fOld);

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
if (argc != 6)
    usage();
txGeneAccession(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
