/* hgMapBedToGene - Create a table that maps a genePred table to a bed table, choosing the best single bed item. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "hdb.h"
#include "genePred.h"
#include "bed.h"
#include "binRange.h"

static char const rcsid[] = "$Id: hgMapBedToGene.c,v 1.1 2003/06/21 00:51:57 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgMapBedToGene - Create a table that maps a genePred table to a bed table, choosing the best single bed item\n"
  "usage:\n"
  "   hgMapBedToGene database genePred bed12 newMapTable\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int gpBedOverlap(struct genePred *gp, struct bed *bed)
/* Return total amount of overlap between gp and bed12. */
{
/* This could be implemented more efficiently, but this
 * way is really simple. */
int bedStart = bed->chromStart;
int gpIx, bedIx;
int overlap = 0;
for (gpIx = 0; gpIx < gp->exonCount; ++gpIx)
    {
    int gpStart = gp->exonStarts[gpIx];
    int gpEnd = gp->exonEnds[gpIx];
    for (bedIx = 0; bedIx < bed->blockCount; ++bedIx)
        {
	int start = bedStart + bed->chromStarts[bedIx];
	int end = start + bed->blockSizes[bedIx];
	overlap += positiveRangeIntersection(gpStart, gpEnd, start, end);
	}
    }
return overlap;
}

struct bed *mostOverlappingBed(struct binKeeper *bk, struct genePred *gp)
/* Find bed in bk that overlaps most with gp.  Return NULL if no overlap. */
{
struct bed *bed, *bestBed = NULL;
int overlap, bestOverlap = 0;
struct binElement *el, *elList = binKeeperFind(bk, gp->txStart, gp->txEnd);

for (el = elList; el != NULL; el = el->next)
    {
    bed = el->val;
    overlap = gpBedOverlap(gp, bed);
    if (overlap > bestOverlap)
        {
	bestOverlap = overlap;
	bestBed = bed;
	}
    }
return bestBed;
}

void oneChromStrandBedToGene(struct sqlConnection *conn, 
	char *chrom, char strand,
	char *genePredTable, char *bedTable, struct hash *dupeHash, 
	FILE *f)
/* Find most overlapping bed for each genePred in one
 * strand of a chromosome, and write it to file.  */
{
int chromSize = hChromSize(chrom);
struct binKeeper *bk = binKeeperNew(0, chromSize);
struct sqlResult *sr;
char extra[256], **row;
int rowOffset;
struct bed *bedList = NULL, *bed;

/* Read bed into binKeeper. */
safef(extra, sizeof(extra), "strand = '%c'", strand);
sr = hChromQuery(conn, bedTable, chrom, extra, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoad12(row+rowOffset);
    slAddHead(&bedList, bed);
    binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
    }
sqlFreeResult(&sr);

/* Scan through gene preds looking for best overlap if any. */
sr = hChromQuery(conn, genePredTable, chrom, extra, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gp = genePredLoad(row+rowOffset);
    char *name = gp->name;
    if (!hashLookup(dupeHash, name))	/* Only take first occurrence. */
	{
	hashAdd(dupeHash, name, NULL);
	if ((bed = mostOverlappingBed(bk, gp)) != NULL)
	    fprintf(f, "%s\t%s\n", gp->name, bed->name);
	}
    genePredFree(&gp);
    }
bedFreeList(&bedList);
binKeeperFree(&bk);
}

void createTable(struct sqlConnection *conn, char *tableName)
/* Create our name/value table, dropping if it already exists. */
{
struct dyString *dy = dyStringNew(512);
dyStringPrintf(dy, 
"CREATE TABLE  %s (\n"
"    name varchar(255) not null,\n"
"    value varchar(255) not null,\n"
"              #Indices\n"
"    PRIMARY KEY(name(16))\n"
")\n",  tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}

void hgMapBedToGene(char *database, char *genePredTable, char *bedTable, 
	char *outTable)
/* hgMapBedToGene - Create a table that maps a genePred table to a bed table, 
 * choosing the best single bed item. */
{
/* Open tab file and database loop through each chromosome writing to it. */
struct slName *chromList, *chrom;
struct sqlConnection *conn;
char *tempDir = ".";
FILE *f;
int fieldIx;
struct hash *dupeHash = newHash(16);

/* Set up connection to database. */
hSetDb(database);
conn = hAllocConn();

/* Check that bed table is really likely to be bed12 or more. 
 * and that genePred table is really genePred. */
fieldIx = sqlFieldIndex(conn, bedTable, "blockStarts");
if (fieldIx != 11 && fieldIx != 12)
    errAbort("%s doesn't seem to be a bed12 table", bedTable);
fieldIx = sqlFieldIndex(conn, genePredTable, "exonEnds");
if (fieldIx != 9 && fieldIx != 10)
    errAbort("%s doesn't seem to be a genePred table", genePredTable);

/* Process each strand of each chromosome into tab-separated file. */
f = hgCreateTabFile(tempDir, outTable);
chromList = hAllChromNames();
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    oneChromStrandBedToGene(conn, chrom->name, '+', genePredTable, bedTable, dupeHash, f);
    oneChromStrandBedToGene(conn, chrom->name, '-', genePredTable, bedTable, dupeHash, f);
    }

/* Create and load table from tab file. */
createTable(conn, outTable);
hgLoadTabFile(conn, tempDir, outTable, &f);

/* Clean up and go home. */
// hgRemoveTabFile(tempDir, outTable);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
hgMapBedToGene(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
