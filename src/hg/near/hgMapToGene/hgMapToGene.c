/* hgMapToGene - Map a track to a genePred track.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "bed.h"
#include "binRange.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgMapToGene.c,v 1.1 2003/09/12 11:07:12 kent Exp $";

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
  "   type=xxx - Set the type line here rather than looking it up in\n"
  "              the trackDb table.\n"
  );
}

boolean verbose = FALSE;

static struct optionSpec options[] = {
   {"type", OPTION_STRING},
   {"verbose", OPTION_BOOLEAN},
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
	char *genePredTable, char *otherTable,  char *otherType,
	struct hash *dupeHash, FILE *f)
/* Find most overlapping bed for each genePred in one
 * strand of a chromosome, and write it to file.  */
{
int chromSize = hChromSize(chrom);
struct binKeeper *bk = binKeeperNew(0, chromSize);
struct sqlResult *sr;
char extra[256], **row;
int rowOffset;
struct bed *bedList = NULL, *bed;

/* Read data into binKeeper. */
safef(extra, sizeof(extra), "strand = '%c'", strand);
sr = hChromQuery(conn, otherTable, chrom, extra, &rowOffset);
if (startsWith("bed 12", otherType))
    {
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoad12(row+rowOffset);
	slAddHead(&bedList, bed);
	binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
	}
    }
else if (startsWith("genePred", otherType))
    {
    struct genePred *gp;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	gp = genePredLoad(row + rowOffset);
	bed = bedFromGenePred(gp);
	genePredFree(&gp);
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
"    PRIMARY KEY(name(16)),\n"
"    INDEX(value(16))\n"
")\n",  tableName);
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
struct hash *dupeHash = newHash(16);

/* Process each strand of each chromosome into tab-separated file. */
f = hgCreateTabFile(tempDir, outTable);
chromList = hAllChromNames();
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    oneChromStrandBedToGene(conn, chrom->name, '+', genePredTable, 
    	otherTable, otherType, dupeHash, f);
    oneChromStrandBedToGene(conn, chrom->name, '-', genePredTable, 
    	otherTable, otherType, dupeHash, f);
    }

/* Create and load table from tab file. */
createTable(conn, outTable);
hgLoadTabFile(conn, tempDir, outTable, &f);

/* Clean up and go home. */
// hgRemoveTabFile(tempDir, outTable);
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
if (argc != 5)
    usage();
hgMapToGene(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
