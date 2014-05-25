/* txGeneExplainUpdate2 - Make table explaining correspondence between versions of UCSC genes. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "binRange.h"



char *unmapped = NULL;
char *oldAsm = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneExplainUpdate2 - Make table explaining correspondence between versions of UCSC genes.\n"
  "usage:\n"
  "   txGeneExplainUpdate2 oldKg.bed newKg.bed out.tab\n"
  "options:\n"
  "   -unmapped=unmapped.bed - file containing beds from previous assembly that didn't map well\n"
  "   -oldAsm=hg18 - name of old database where unmapped ones used to live.\n"
  );
}

static struct optionSpec options[] = {
   {"unmapped", OPTION_STRING},
   {"oldAsm", OPTION_STRING},
   {NULL, 0},
};

struct bed *findExact(struct bed *oldBed, struct hash *newKeeperHash)
/* Try and find a new bed identical with old bed. */
{
struct binKeeper *bk = hashFindVal(newKeeperHash, oldBed->chrom);
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

struct bed *findCompatible(struct bed *oldBed, struct hash *newAccHash)
/* Find bed in newHash with same accession (but maybe different version) */
{
char accOnly[64];
safef(accOnly, sizeof(accOnly), oldBed->name);
chopSuffix(accOnly);
return hashFindVal(newAccHash, accOnly);
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

void explainMissing(struct bed *oldBed, char *type, FILE *f)
/* Write out a line explaining why old gene has gone missing. */
{
char *newName = "";
char *note = ""; 	/* Some day would be nice to elaborate on note. */
fprintf(f, "%s\t%s\t%d\t%d\t%s\t%s\t%s\n", oldBed->name, oldBed->chrom, 
	oldBed->chromStart, oldBed->chromEnd, newName, type, note);
}

boolean intronsDifferent(struct bed *a, struct bed *b)
/* Return TRUE if introns of a and b are same. */
{
int blockCount = a->blockCount;
if (b->blockCount != blockCount)
    return TRUE;
int i;
for (i=1; i<blockCount; ++i)
    {
    int aStart = a->chromStart + a->chromStarts[i-1] + a->blockSizes[i-1];
    int bStart = b->chromStart + b->chromStarts[i-1] + b->blockSizes[i-1];
    int aEnd = a->chromStart + a->chromStarts[i];
    int bEnd = b->chromStart + b->chromStarts[i];
    if (aStart != bStart || aEnd != bEnd)
        return TRUE;
    }
return FALSE;
}

static void addCode(struct dyString *dy, char *code, int *numNotes)
{
(*numNotes)++;

dyStringPrintf(dy, "%s,", code);
}

char *diffNote(struct bed *oldBed, struct bed *newBed)
/* Return note explaining difference. */
{
boolean oldIsCoding = (oldBed->thickStart < oldBed->thickEnd);
boolean newIsCoding = (newBed->thickStart < newBed->thickEnd);
boolean bothCoding = (oldIsCoding && newIsCoding);
struct dyString *dy = newDyString(64);
int numNotes = 0;

if (oldIsCoding != newIsCoding)
    {
    if (oldIsCoding)
	addCode(dy, "becameNonCoding", &numNotes);
    else
	addCode(dy, "becameCoding", &numNotes);
    }

if (oldBed->blockCount != newBed->blockCount)
    addCode(dy, "differentNumberOfBlocks", &numNotes);

if (bothCoding && (oldBed->thickStart < newBed->thickStart || oldBed->thickEnd > newBed->thickEnd))
    addCode(dy, "basesRemovedFromCoding", &numNotes);

if (bothCoding && (oldBed->thickStart > newBed->thickStart || oldBed->thickEnd < newBed->thickEnd))
    addCode(dy, "basesAddedToCoding", &numNotes);

if (intronsDifferent(oldBed, newBed))
    addCode(dy, "intronsChanged", &numNotes);

if (oldBed->chromStart < newBed->chromStart || oldBed->chromEnd > newBed->chromEnd)
    addCode(dy, "basesRemovedFromUTR", &numNotes);

if (oldBed->chromStart > newBed->chromStart || oldBed->chromEnd < newBed->chromEnd)
    addCode(dy, "basesAddedToUTR", &numNotes);

if (numNotes == 0)
    addCode(dy, "noChange", &numNotes);

char *note = dyStringCannibalize(&dy);

/* remove final comma */
if (note != NULL)
    {
    int noteLen = strlen(note);

    note[noteLen - 1] = 0;
    }

return note;
}

void explainOverlap(struct bed *oldBed, struct bed *newBed, char *type, FILE *f)
/* Write out a line explaining why old gene has gone missing. */
{
char *note = diffNote(oldBed, newBed);
fprintf(f, "%s\t%s\t%d\t%d\t%s\t%s\t%s\n", oldBed->name, oldBed->chrom, 
	oldBed->chromStart, oldBed->chromEnd, newBed->name, type, note);
}

struct hash *makeAccHash(struct bed *list)
/* Make hash keyed by accession (without version) with beds for values. */
{
struct bed *bed;
struct hash *hash = hashNew(16);
for (bed = list; bed != NULL; bed = bed->next)
    {
    char accOnly[64];
    safef(accOnly, sizeof(accOnly), bed->name);
    chopSuffix(accOnly);
    hashAdd(hash, accOnly, bed);
    }
return hash;
}

struct hash *hashBedList(struct bed *list)
/* Make hash keyed by bed->name with bed values. */
{
struct bed *bed;
struct hash *hash = hashNew(16);
for (bed = list; bed != NULL; bed = bed->next)
    hashAdd(hash, bed->name, bed);
return hash;
}

void txGeneExplainUpdate2(char *oldKgFile, char *newKgFile, char *outFile)
/* txGeneExplainUpdate2 - Make table explaining correspondence between versions of UCSC genes. */
{
struct bed *unmappedList = NULL;
struct bed *oldBed, *oldList = bedLoadNAll(oldKgFile, 12);
struct bed *bed, *newList = bedLoadNAll(newKgFile, 12);
struct hash *newAccHash = makeAccHash(newList);
struct hash *newAccVerHash = hashBedList(newList);
struct hash *newKeeperHash = bedsIntoKeeperHash(newList);
if (unmapped)
    unmappedList = bedLoadNAll(unmapped, 12);
FILE *f = mustOpen(outFile, "w");

for (oldBed = oldList; oldBed != NULL; oldBed = oldBed->next)
    {
    if ((bed = hashFindVal(newAccVerHash, oldBed->name)) != NULL)
        fprintf(f, "%s\t%s\t%d\t%d\t%s\texact\t\n", oldBed->name, oldBed->chrom,
		oldBed->chromStart, oldBed->chromEnd, bed->name);
    else if ((bed = findCompatible(oldBed, newAccHash)) != NULL)
        fprintf(f, "%s\t%s\t%d\t%d\t%s\tcompatible\t%s\n", oldBed->name, oldBed->chrom,
		oldBed->chromStart, oldBed->chromEnd, bed->name, diffNote(oldBed, bed));
    else if ((bed = findMostOverlapping(oldBed, newKeeperHash)) != NULL)
	explainOverlap(oldBed, bed, "overlap", f);
    else
        explainMissing(oldBed, "none", f);
    }
for (oldBed = unmappedList; oldBed != NULL; oldBed = oldBed->next)
    {
    fprintf(f, "%s\t%s\t%d\t%d\t%s\tunmapped\tunmapped from %s\n", oldBed->name, oldBed->chrom,
	    oldBed->chromStart, oldBed->chromEnd, "", oldAsm);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
unmapped = optionVal("unmapped", NULL);
oldAsm = optionVal("oldAsm", NULL);
if (unmapped != NULL)
    {
    if (oldAsm == NULL)
        errAbort("You have to specify oldAsm option to say what database unmapped come from.");
    }
txGeneExplainUpdate2(argv[1], argv[2], argv[3]);
return 0;
}
