/* testIntersect - Test some ideas on intersections. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "hdb.h"

static char const rcsid[] = "$Id: testIntersect.c,v 1.1 2004/11/19 17:04:52 kent Exp $";

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

void intersectOnChrom(char *db, struct sqlConnection *conn, char *chrom, char *track1, char *track2)
/* Do intersection on one chromosome. */
{
struct lm *lm = lmInit(0);
struct bed *bedList1, *bedList2;
int fieldCount1, fieldCount2;
uglyTime(NULL);
scanChromTable(conn, chrom, track1);
uglyTime("Scan track 1");
scanChromTable(conn, chrom, track2);
uglyTime("Scan track 2");
bedList1 = getChromAsBed(conn, db, track1, chrom, lm, &fieldCount1);
uglyTime("Track1 as bed");
bedList2 = getChromAsBed(conn, db, track2, chrom, lm, &fieldCount2);
uglyTime("Track2 as bed");
uglyf("%d items with %d fields in %s\n", slCount(bedList1), fieldCount1, track1);
uglyf("%d items with %d fields in %s\n", slCount(bedList2), fieldCount2, track2);
uglyTime("Print time");
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
