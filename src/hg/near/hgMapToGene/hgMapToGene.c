/* hgMapToGene - Map a track to a genePred track.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "bed.h"
#include "binRange.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgMapToGene.c,v 1.3 2003/09/21 05:04:22 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgMapToGene - Map a track to a genePred track.\n"
  "usage:\n"
  "   hgMapToGene database track geneTrack mapTable\n"
  "where:\n"
  "   database - is a browser database (ex. hg16)\n"
  "   track - the track to map to the genePred track\n"
  "           This must be bed12 or genePred itself\n"
  "   geneTrack - a genePred format track\n"
  "   newMapTable - a new table to create with two columns\n"
  "           <geneTrackId><trackId>\n"
  "For each gene in geneTrack this will select the most\n"
  "overlapping item in track to associate\n"
  "Options:\n"
  "   -type=xxx - Set the type line here rather than looking it up in\n"
  "              the trackDb table.\n"
  "   -all - Put all elements of track that intersect with geneTrack\n"
  "          into mapTable, not just the best for each gene.\n"
  "   -cds - Only consider coding portions of gene.\n"
  "   -noLoad - Don't load database, just create mapTable.tab file\n"
  "   -verbose - Print intermediate status info\n"
  "   -intronsToo - Include introns\n"
  "   -createOnly - Just create mapTable, don't populate it\n"
  );
}

boolean verbose = FALSE;
boolean cdsOnly = FALSE;
boolean intronsToo = FALSE;
boolean createOnly = FALSE;

static struct optionSpec options[] = {
   {"type", OPTION_STRING},
   {"all", OPTION_BOOLEAN},
   {"cds", OPTION_BOOLEAN},
   {"intronsToo", OPTION_BOOLEAN},
   {"noLoad", OPTION_BOOLEAN},
   {"createOnly", OPTION_BOOLEAN},
   {"verbose", OPTION_BOOLEAN},
   {NULL, 0},
};


int bedIntersectRange(struct bed *bed, int rStart, int rEnd)
/* Return intersection of bed with range rStart-rEnd */
{
int bedStart = bed->chromStart;
int overlap = 0;

if (bed->blockCount > 1)
    {
    int bedIx;
    for (bedIx = 0; bedIx < bed->blockCount; ++bedIx)
	{
	int start = bedStart + bed->chromStarts[bedIx];
	int end = start + bed->blockSizes[bedIx];
	overlap += positiveRangeIntersection(rStart, rEnd, start, end);
	}
    }
else
    {
    overlap = positiveRangeIntersection(rStart, rEnd, bed->chromStart, bed->chromEnd);
    }
return overlap;
}

int gpBedOverlap(struct genePred *gp, boolean cdsOnly, boolean intronsToo, struct bed *bed)
/* Return total amount of overlap between gp and bed12. */
{
/* This could be implemented more efficiently, but this
 * way is really simple. */
int gpIx;
int overlap = 0;
int geneStart, geneEnd;
if (cdsOnly)
    {
    geneStart = gp->cdsStart;
    geneEnd = gp->cdsEnd;
    }
else
    {
    geneStart = gp->txStart;
    geneEnd = gp->txEnd;
    }
if (intronsToo)
    {
    overlap = bedIntersectRange(bed, geneStart, geneEnd);
    }
else
    {
    for (gpIx = 0; gpIx < gp->exonCount; ++gpIx)
	{
	int gpStart = gp->exonStarts[gpIx];
	int gpEnd = gp->exonEnds[gpIx];
	if (cdsOnly)
	    {
	    if (gpStart < geneStart) gpStart = geneStart;
	    if (gpEnd > geneEnd) gpEnd = geneEnd;
	    if (gpStart >= gpEnd)
	        continue;
	    }
	overlap += bedIntersectRange(bed, gpStart, gpEnd);
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
    overlap = gpBedOverlap(gp, cdsOnly, intronsToo, bed);
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
	char *genePredTable, char *otherTable,  char *otherType,
	struct hash *dupeHash, boolean doAll, FILE *f)
/* Find most overlapping bed for each genePred in one
 * strand of a chromosome, and write it to file.  */
{
int chromSize = hChromSize(chrom);
struct binKeeper *bk = binKeeperNew(0, chromSize);
struct sqlResult *sr;
char extraBuf[256], *extra = NULL, **row;
int rowOffset;
struct bed *bedList = NULL, *bed;
int bedNum;	/* Number of bed fields*/

/* Read data into binKeeper. */
if (startsWith("bed", otherType))
    {
    char *numString = otherType + 4;
    bedNum = atoi(numString);
    if (bedNum < 6)	/* Just one strand in bed. */
        {
	if (strand == '-')
	    {
	    binKeeperFree(&bk);
	    return;
	    }
	}
    else
	{
	safef(extraBuf, sizeof(extraBuf), "strand = '%c'", strand);
	extra = extraBuf;
	}
    sr = hChromQuery(conn, otherTable, chrom, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, bedNum);
	slAddHead(&bedList, bed);
	binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
	}
    }
else if (startsWith("genePred", otherType))
    {
    struct genePred *gp;
    bedNum = 12;
    safef(extraBuf, sizeof(extraBuf), "strand = '%c'", strand);
    extra = extraBuf;
    sr = hChromQuery(conn, otherTable, chrom, extra, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	gp = genePredLoad(row + rowOffset);
	bed = bedFromGenePred(gp);
	genePredFree(&gp);
	slAddHead(&bedList, bed);
	binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
	}
    }
else if (startsWith("psl", otherType))
    {
    struct psl *psl;
    bedNum = 12;
    safef(extraBuf, sizeof(extraBuf), "strand = '%c'", strand);
    extra = extraBuf;
    sr = hChromQuery(conn, otherTable, chrom, extra, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	psl = pslLoad(row + rowOffset);
	bed = bedFromPsl(psl);
	pslFree(&psl);
	slAddHead(&bedList, bed);
	binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
	}
    }
else
    {
    errAbort("%s: unrecognized track type %s", otherTable, otherType);
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
	if (doAll)
	    {
	    struct binElement *el, *elList = binKeeperFind(bk, gp->txStart, gp->txEnd);
	    for (el = elList; el != NULL; el = el->next)
		{
		bed = el->val;
		if (gpBedOverlap(gp, cdsOnly, intronsToo, bed) > 0)
		    fprintf(f, "%s\t%s\n", gp->name, bed->name);
		}
	    }
	else
	    {
	    if ((bed = mostOverlappingBed(bk, gp)) != NULL)
		fprintf(f, "%s\t%s\n", gp->name, bed->name);
	    }
	}
    genePredFree(&gp);
    }
bedFreeList(&bedList);
binKeeperFree(&bk);
}

void createTable(struct sqlConnection *conn, char *tableName, boolean unique)
/* Create our name/value table, dropping if it already exists. */
{
char *indexType =  (unique ? "UNIQUE" : "INDEX");
struct dyString *dy = dyStringNew(512);
dyStringPrintf(dy, 
"CREATE TABLE  %s (\n"
"    name varchar(255) not null,\n"
"    value varchar(255) not null,\n"
"              #Indices\n"
"    %s(name(16)),\n"
"    INDEX(value(16))\n"
")\n",  tableName, indexType);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}

void hgMapTableToGene(struct sqlConnection *conn, char *genePredTable, char *otherTable, 
	char *otherType, char *outTable)
/* hgMapTableToGene - Create a table that maps genePredTable to otherTable, 
 * choosing the best single item in otherTable for each genePred. */
{
/* Open tab file and database loop through each chromosome writing to it. */
struct slName *chromList, *chrom;
char *tempDir = ".";
FILE *f;
int fieldIx;
boolean doAll = optionExists("all");

/* Process each strand of each chromosome into tab-separated file. */
if (!createOnly)
    {
    struct hash *dupeHash = newHash(16);
    f = hgCreateTabFile(tempDir, outTable);
    chromList = hAllChromNames();
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
	{
	if (verbose)
	    printf("%s\n", chrom->name);
	oneChromStrandBedToGene(conn, chrom->name, '+', genePredTable, 
	    otherTable, otherType, dupeHash, doAll, f);
	oneChromStrandBedToGene(conn, chrom->name, '-', genePredTable, 
	    otherTable, otherType, dupeHash, doAll, f);
	}
    hashFree(&dupeHash);
    }

/* Create and load table from tab file. */
if (createOnly)
    {
    createTable(conn, outTable, !doAll);
    }
else if (!optionExists("noLoad"))
    {
    createTable(conn, outTable, !doAll);
    hgLoadTabFile(conn, tempDir, outTable, &f);
    hgRemoveTabFile(tempDir, outTable);
    }

}

char *tdbType(struct sqlConnection *conn, char *track)
/* Return track field from TDB. */
{
char query[512];
char *type, typeBuf[128];
safef(query, sizeof(query), "select type from trackDb where tableName = '%s'", track);
type = sqlQuickQuery(conn, query, typeBuf, sizeof(typeBuf));
if (type == NULL)
    errAbort("Can't find track %s in trackDb", track);
return cloneString(type);
}

void hgMapToGene(char *database, char *track, char *geneTrack, char *newTable)
/* hgMapToGene - Map a track to a genePred track.. */
{
struct sqlConnection *conn = sqlConnect(database);
char query[512];
char typeBuf[128];
char *type = optionVal("type", NULL);
hSetDb(database);
if (type == NULL)
    type = tdbType(conn, track);
if (!startsWith("genePred", tdbType(conn, geneTrack)))
    errAbort("%s is not a genePred type track", geneTrack);
hgMapTableToGene(conn, geneTrack, track, type, newTable);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
verbose = optionExists("verbose");
cdsOnly = optionExists("cds");
intronsToo = optionExists("intronsToo");
createOnly = optionExists("createOnly");

if (argc != 5)
    usage();
hgMapToGene(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
