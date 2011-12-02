/* testIntersect - Test some ideas on intersections. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "featureBits.h"
#include "hdb.h"
#include "binRange.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "testIntersect - Test some ideas on intersections\n"
  "usage:\n"
  "   testIntersect database track1 track2\n"
  "options:\n"
  "   -chrom=chrN - Restrict to a single chromosome\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {NULL, 0},
};

boolean htiIsPsl(struct hTableInfo *hti)
/* Return TRUE if table looks to be in psl format. */
{
return sameString("tStarts", hti->startsField);
}

void bedSqlFieldsExceptForChrom(struct hTableInfo *hti,
	int *retFieldCount, char **retFields)
/* Given tableInfo figure out what fields are needed to
 * add to a database query to have information to create
 * a bed. The chromosome is not one of these fields - we
 * assume that is already known since we're processing one
 * chromosome at a time.   Return a comma separated (no last
 * comma) list of fields that can be freeMem'd, and the count
 * of fields (this *including* the chromosome). */
{
struct dyString *fields = dyStringNew(128);
int fieldCount = fieldCount = hTableInfoBedFieldCount(hti);

dyStringPrintf(fields, "%s,%s", hti->startField, hti->endField);
if (fieldCount >= 4)
    {
    if (hti->nameField[0] != 0)
	dyStringPrintf(fields, ",%s", hti->nameField);
    else /* Put in . as placeholder. */
	dyStringPrintf(fields, ",'.'");  
    }
if (fieldCount >= 5)
    {
    if (hti->scoreField[0] != 0)
	dyStringPrintf(fields, ",%s", hti->scoreField);
    else
	dyStringPrintf(fields, ",0", hti->startField);  
    }
if (fieldCount >= 6)
    {
    if (hti->strandField[0] != 0)
	dyStringPrintf(fields, ",%s", hti->strandField);
    else
	dyStringPrintf(fields, ",'.'");  
    }
if (fieldCount >= 8)
    {
    if (hti->cdsStartField[0] != 0)
	dyStringPrintf(fields, ",%s,%s", hti->cdsStartField, hti->cdsEndField);
    else
	dyStringPrintf(fields, ",%s,%s", hti->startField, hti->endField);  
    }
if (fieldCount >= 12)
    {
    dyStringPrintf(fields, ",%s,%s,%s", hti->countField, 
        hti->endsSizesField, hti->startsField);
    }
if (htiIsPsl(hti))
    {
    /* For psl format we need to know chrom size as well
     * to handle reverse strand case. */
    dyStringPrintf(fields, ",tSize");
    }
*retFieldCount = fieldCount;
*retFields = dyStringCannibalize(&fields);
}

struct bed *bedFromRow(
	char *chrom, 		  /* Chromosome bed is on. */
	char **row,  		  /* Row with other data for bed. */
	int fieldCount,		  /* Number of fields in final bed. */
	boolean isPsl, 		  /* True if in PSL format. */
	boolean isGenePred,	  /* True if in GenePred format. */
	boolean isBedWithBlocks,  /* True if BED with block list. */
	boolean *pslKnowIfProtein,/* Have we figured out if psl is protein? */
	boolean *pslIsProtein,    /* True if we know psl is protien. */
	struct lm *lm)		  /* Local memory pool */
/* Create bed from a database row when we already understand
 * the format pretty well.  The bed is allocated inside of
 * the local memory pool lm.  Generally use this in conjunction
 * with the results of a SQL query constructed with the aid
 * of the bedSqlFieldsExceptForChrom function. */
{
char *strand, tStrand, qStrand;
struct bed *bed;
int i, blockCount;

lmAllocVar(lm, bed);
bed->chrom = chrom;
bed->chromStart = sqlUnsigned(row[0]);
bed->chromEnd = sqlUnsigned(row[1]);

if (fieldCount < 4)
    return bed;
bed->name = lmCloneString(lm, row[2]);
if (fieldCount < 5)
    return bed;
bed->score = atoi(row[3]);
if (fieldCount < 6)
    return bed;
strand = row[4];
qStrand = strand[0];
tStrand = strand[1];
if (tStrand == 0)
    bed->strand[0] = qStrand;
else
    {
    /* psl: use XOR of qStrand,tStrand if both are given. */
    if (tStrand == qStrand)
	bed->strand[0] = '+';
    else
	bed->strand[0] = '-';
    }
if (fieldCount < 8)
    return bed;
bed->thickStart = sqlUnsigned(row[5]);
bed->thickEnd   = sqlUnsigned(row[6]);
if (fieldCount < 12)
    return bed;
bed->blockCount = blockCount = sqlUnsigned(row[7]);
lmAllocArray(lm, bed->blockSizes, blockCount);
sqlUnsignedArray(row[8], bed->blockSizes, blockCount);
lmAllocArray(lm, bed->chromStarts, blockCount);
sqlUnsignedArray(row[9], bed->chromStarts, blockCount);
if (isGenePred)
    {
    /* Translate end coordinates to sizes. */
    for (i=0; i<bed->blockCount; ++i)
	bed->blockSizes[i] -= bed->chromStarts[i];
    }
else if (isPsl)
    {
    if (!*pslKnowIfProtein)
	{
	/* Figure out if is protein using a rather elaborate but
	 * working test I think Angie or Brian must have figured out. */
	if (tStrand == '-')
	    {
	    int tSize = sqlUnsigned(row[10]);
	    *pslIsProtein = 
		   (bed->chromStart == 
		    tSize - (3*bed->blockSizes[bed->blockCount - 1]  + 
		    bed->chromStarts[bed->blockCount - 1]));
	    }
	else
	    {
	    *pslIsProtein = (bed->chromEnd == 
		    3*bed->blockSizes[bed->blockCount - 1]  + 
		    bed->chromStarts[bed->blockCount - 1]);
	    }
	*pslKnowIfProtein = TRUE;
	}
    if (*pslIsProtein)
	{
	/* if protein then blockSizes are in protein space */
	for (i=0; i<blockCount; ++i)
	    bed->blockSizes[i] *= 3;
	}
    if (tStrand == '-')
	{
	/* psl: if target strand is '-', flip the coords.
	 * (this is the target part of pslRcBoth from src/lib/psl.c) */
	int tSize = sqlUnsigned(row[10]);
	for (i=0; i<blockCount; ++i)
	    {
	    bed->chromStarts[i] = tSize - 
		    (bed->chromStarts[i] + bed->blockSizes[i]);
	    }
	reverseInts(bed->chromStarts, bed->blockCount);
	reverseInts(bed->blockSizes, bed->blockCount);
	}
    }
if (!isBedWithBlocks)
    {
    /* non-bed: translate absolute starts to relative starts */
    for (i=0;  i < bed->blockCount;  i++)
	bed->chromStarts[i] -= bed->chromStart;
    }
return bed;
}


static struct bed *getChromAsBed(
	struct sqlConnection *conn, 
	char *db, char *table, 	/* Database and table. */
	char *chrom,
	struct lm *lm,
	int *retFieldCount)		/* Where to allocate memory. */
/* Return a bed list of all items in the given range in table.
 * Cleanup result via lmCleanup(&lm) rather than bedFreeList.  */
{
char *fields = NULL;
struct sqlResult *sr;
struct hTableInfo *hti;
struct bed *bedList=NULL, *bed;
char **row;
int fieldCount;
boolean isPsl, isGenePred, isBedWithBlocks;
boolean pslKnowIfProtein = FALSE, pslIsProtein = FALSE;

hti = hFindTableInfoDb(db, chrom, table);
if (hti == NULL)
    errAbort("Could not find table info for table %s", table);

bedSqlFieldsExceptForChrom(hti, &fieldCount, &fields);
isPsl = htiIsPsl(hti);
isGenePred = sameString("exonEnds", hti->endsSizesField);
isBedWithBlocks = (
    (sameString("chromStarts", hti->startsField) ||
     sameString("blockStarts", hti->startsField))
	 && sameString("blockSizes", hti->endsSizesField));

/* All beds have at least chrom,start,end.  We omit the chrom
 * from the query since we already know it. */
sr = hExtendedChromQuery(conn, table, chrom, NULL, FALSE, fields, NULL);
while (sr != NULL && (row = sqlNextRow(sr)) != NULL)
    {
    bed = bedFromRow(chrom, row, fieldCount, isPsl, isGenePred,
		     isBedWithBlocks, &pslKnowIfProtein, &pslIsProtein, lm);
    slAddHead(&bedList, bed);
    }
freez(&fields);
sqlFreeResult(&sr);
slReverse(&bedList);
*retFieldCount = fieldCount;
return(bedList);
}

void scanChromTable(struct sqlConnection *conn, char *chrom, char *table)
/* Scan chromosome table, don't do anything with data. */
{
struct sqlResult *sr;
int rowOffset;
char **row;

sr = hChromQuery(conn, table, chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    ;
sqlFreeResult(&sr);
}

struct featureBits *fbList(char *db, char *chrom, char *table, 
	struct bed *bedList, int chromSize)
/* Get feature bits list from bed list. */
{
struct hTableInfo *hti;
hti = hFindTableInfoDb(db, chrom, table);
return fbFromBed(table, hti, bedList, 0, chromSize, FALSE, FALSE);
}

static struct bed *bitsToBed4List(Bits *bits, int bitSize, 
	char *chrom, int minSize, int rangeStart, int rangeEnd,
	struct lm *lm)
/* Translate ranges of set bits to bed 4 items. */
{
struct bed *bedList = NULL, *bed;
boolean thisBit, lastBit;
int start = 0;
int end = 0;
int id = 0;
char name[128];

if (rangeStart < 0)
    rangeStart = 0;
if (rangeEnd > bitSize)
    rangeEnd = bitSize;
end = rangeStart;

/* We depend on extra zero BYTE at end in case bitNot was used on bits. */
for (;;)
    {
    start = bitFindSet(bits, end, rangeEnd);
    if (start >= rangeEnd)
        break;
    end = bitFindClear(bits, start, rangeEnd);
    if (end - start >= minSize)
	{
	lmAllocVar(lm, bed);
	bed->chrom = chrom;
	bed->chromStart = start;
	bed->chromEnd = end;
	snprintf(name, sizeof(name), "%s.%d", chrom, ++id);
	bed->name = lmCloneString(lm, name);
	slAddHead(&bedList, bed);
	}
    }
slReverse(&bedList);
return(bedList);
}

static int bitCountBasesOverlap(struct bed *bedItem, Bits *bits, boolean hasBlocks)
/* Return the number of bases belonging to bedItem covered by bits. */
{
int count = 0;
int i, j;
if (hasBlocks)
    {
    for (i=0;  i < bedItem->blockCount;  i++)
	{
	int start = bedItem->chromStart + bedItem->chromStarts[i];
	int end   = start + bedItem->blockSizes[i];
	for (j=start;  j < end;  j++)
	    if (bitReadOne(bits, j))
		count++;
	}
    }
else
    {
    for (i=bedItem->chromStart;  i < bedItem->chromEnd;  i++)
	if (bitReadOne(bits, i))
	    count++;
    }
    return(count);
}

int bitCountAllOverlaps(struct bed *bedList, Bits *bits, int fieldCount)
/* Count all overlapping. */
{
struct bed *bed;
int total = 0, one;
boolean hasBlocks = (fieldCount >= 12);

for (bed = bedList;  bed != NULL; bed = bed->next)
     {
     one = bitCountBasesOverlap(bed, bits, hasBlocks);
     total += one;
     }
return total;
}

struct binKeeper *fbToBinKeeper(struct featureBits *fbList, int chromSize)
/* Make a binKeeper filled with fbList. */
{
struct binKeeper *bk = binKeeperNew(0, chromSize);
struct featureBits *fb;
for (fb = fbList; fb != NULL; fb = fb->next)
    binKeeperAdd(bk, fb->start, fb->end, fb);
return bk;
}

int bkCountOverlappingRange(struct binKeeper *bk, int start, int end)
/* Return biggest overlap of anything in binKeeper with given range. */
{
struct binElement *el, *list = binKeeperFind(bk, start, end);
int overlap, bestOverlap = 0;

for (el = list; el != NULL; el = el->next)
    {
    overlap = rangeIntersection(el->start, el->end, start, end);
    if (overlap > bestOverlap)
        bestOverlap = overlap;
    }
return bestOverlap;
}

static int bkCountBasesOverlap(struct bed *bedItem, struct binKeeper *bk, boolean hasBlocks)
/* Return the number of bases belonging to bedItem covered by bits. */
{
int count = 0;
int i;
if (hasBlocks)
    {
    for (i=0;  i < bedItem->blockCount;  i++)
	{
	int start = bedItem->chromStart + bedItem->chromStarts[i];
	int end   = start + bedItem->blockSizes[i];
	count += bkCountOverlappingRange(bk, start, end);
	}
    }
else
    {
    count += bkCountOverlappingRange(bk, bedItem->chromStart, bedItem->chromEnd);
    }
return(count);
}

int bkCountAllOverlaps(struct bed *bedList, struct binKeeper *bk, int fieldCount)
/* Count all overlapping. */
{
struct bed *bed;
int total = 0, one;
boolean hasBlocks = (fieldCount >= 12);

for (bed = bedList;  bed != NULL; bed = bed->next)
     {
     one = bkCountBasesOverlap(bed, bk, hasBlocks);
     total += one;
     }
return total;
}


void intersectOnChrom(char *db, struct sqlConnection *conn, char *chrom, 
	char *track1, char *track2)
/* Do intersection on one chromosome. */
{
int chromSize = hChromSize(chrom);
struct lm *lm = lmInit(0);
struct bed *bedList1, *bedList2, *andBed;
struct featureBits *fb1, *fb2;
Bits *bit1, *bit2;
int fieldCount1, fieldCount2;
struct binKeeper *bk2;

uglyTime(NULL);
scanChromTable(conn, chrom, track1);
scanChromTable(conn, chrom, track2);
uglyTime("Scan tracks");
bedList1 = getChromAsBed(conn, db, track1, chrom, lm, &fieldCount1);
bedList2 = getChromAsBed(conn, db, track2, chrom, lm, &fieldCount2);
uglyTime("Tracks as bed");
uglyf("%d items with %d fields in %s, ", slCount(bedList1), fieldCount1, track1);
uglyf("%d items with %d fields in %s\n", slCount(bedList2), fieldCount2, track2);
bit1 = bitAlloc(chromSize+8);
bit2 = bitAlloc(chromSize+8);
uglyTime("bitAlloc");

fb1 = fbList(db, chrom, track1,  bedList1, chromSize);
fb2 = fbList(db, chrom, track1,  bedList1, chromSize);
uglyTime("bed to featureBits list");

fbOrBits(bit1, chromSize, fb1, 0);
fbOrBits(bit2, chromSize, fb2, 0);
uglyTime("or into bits");

bitAnd(bit1, bit2, chromSize);
uglyTime("Anding bitfields");

andBed = bitsToBed4List(bit1, chromSize, chrom, 0, 0, chromSize, lm);
uglyTime("Converting bitfield to bed 4");

bitCountAllOverlaps(bedList1, bit2, fieldCount2);
uglyTime("Counting overlaps in track1 with bitfield of track2");

bk2 = fbToBinKeeper(fb2, chromSize);
uglyTime("Adding featureBits list from track 2 into binKeeper.");

bkCountAllOverlaps(bedList1, bk2, fieldCount2);
uglyTime("Count overlaps in track1 with binKeeper of track2");

featureBitsFreeList(&fb1);
featureBitsFreeList(&fb2);
uglyTime("free featureBits");


bitFree(&bit1);
bitFree(&bit2);
uglyTime("bitFree");
}

void testIntersect(char *db, char *track1, char *track2)
/* testIntersect - Test some ideas on intersections. */
{
struct slName *chromList = NULL, *chrom;
struct sqlConnection *conn;

hSetDb(db);
if (optionExists("chrom"))
    chromList = slNameNew(optionVal("chrom", NULL));
else
    chromList = hAllChromNames();
conn = hAllocConn();
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
     intersectOnChrom(db, conn, chrom->name, track1, track2);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
testIntersect(argv[1], argv[2], argv[3]);
return 0;
}
