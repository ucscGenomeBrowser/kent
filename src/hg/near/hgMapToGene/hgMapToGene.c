/* hgMapToGene - Map a track to a genePred track.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "bed.h"
#include "binRange.h"
#include "hdb.h"
#include "hgConfig.h"


static char const rcsid[] = "$Id: hgMapToGene.c,v 1.14.68.2 2008/07/31 05:21:39 markd Exp $";

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
  "           This must be bed12 psl or genePred itself\n"
  "   geneTrack - a genePred format track\n"
  "   newMapTable - a new table to create with two columns\n"
  "           <geneTrackId><trackId>\n"
  "For each gene in geneTrack this will select the most\n"
  "overlapping item in track to associate\n"
  "Options:\n"
  "   -type=xxx - Set the type line here rather than looking it up in\n"
  "              the trackDb table.\n"
  "   -geneTableType=xxx - Set the gene table type (bed or genePred)\n"
  "                       rather than look it up in trackDb table.\n" 
  "   -all - Put all elements of track that intersect with geneTrack\n"
  "          into mapTable, not just the best for each gene.\n"
  "   -prefix=XXX - Only consider elements of track that begin with prefix\n"
  "   -cds - Only consider coding portions of gene.\n"
  "   -noLoad - Don't load database, just create mapTable.tab file\n"
  "   -verbose=N - Print intermediate status info if N > 0\n"
  "   -intronsToo - Include introns\n"
  "   -createOnly - Just create mapTable, don't populate it\n"
  "   -tempDb - Database to look for genes track and where to put result\n"
  "   -lookup=lookup.txt - Lookup.txt is a 2 column file\n"
  "            <trackId><lookupId>\n"
  "           The trackId from the geneTrack gets replaced with lookupId\n"
  );
}

boolean cdsOnly = FALSE;
boolean intronsToo = FALSE;
boolean createOnly = FALSE;
char *prefix = NULL;
char *trackDb = NULL;

static struct optionSpec options[] = {
   {"type", OPTION_STRING},
   {"all", OPTION_BOOLEAN},
   {"prefix", OPTION_STRING},
   {"cds", OPTION_BOOLEAN},
   {"intronsToo", OPTION_BOOLEAN},
   {"noLoad", OPTION_BOOLEAN},
   {"createOnly", OPTION_BOOLEAN},
   {"lookup", OPTION_STRING},
   {"geneTableType", OPTION_STRING},
   {"tempDb", OPTION_STRING},
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

void outTwo(FILE *f, struct hash *lookupHash, char *key, char *val)
/* Output key/val to tab separated file.  If lookupHash is non-null
 * then lookup val through it before outputting. */
{
if (lookupHash != NULL)
    val = hashFindVal(lookupHash, val);
if (val != NULL)
    fprintf(f, "%s\t%s\n", key, val);
}

void oneChromStrandTrackToGene(char *database, struct sqlConnection *conn, struct sqlConnection *tConn,
			     char *chrom, char strand,
			     char *geneTable, char *geneTableType, 
			     char *otherTable,  char *otherType,
			     struct hash *dupeHash, boolean doAll, struct hash *lookupHash,
			     FILE *f)
/* Find most overlapping bed for each genePred in one
 * strand of a chromosome, and write it to file.  */
{
int chromSize = hChromSize(database, chrom);
struct binKeeper *bk = binKeeperNew(0, chromSize);
struct sqlResult *sr;
char extraBuf[256], *extra = NULL, **row;
int rowOffset;
struct bed *bedList = NULL, *bed;
int bedNum;	/* Number of bed fields*/
boolean geneTableBed = FALSE;
int numCols;
if(startsWith("genePred", geneTableType)) 
    geneTableBed = FALSE;
else if(startsWith("bed", geneTableType))
    geneTableBed = TRUE;
else
    errAbort("%s table type doesn't appear to be bed or genePred (%s instead).", geneTable, geneTableType);
    
/* Read data into binKeeper. */
if (startsWith("bed", otherType))
    {
    char *numString = otherType + 3;
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
    sr = hChromQuery(conn, otherTable, chrom, extra, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, bedNum);
	if (prefix == NULL || startsWith(prefix, bed->name))
	    {
	    slAddHead(&bedList, bed);
	    binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
	    }
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
	if (prefix == NULL || startsWith(prefix, gp->name))
	    {
	    bed = bedFromGenePred(gp);
	    genePredFree(&gp);
	    slAddHead(&bedList, bed);
	    binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
	    }
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
	if (prefix == NULL || startsWith(prefix, psl->qName))
	    {
	    bed = bedFromPsl(psl);
	    pslFree(&psl);
	    slAddHead(&bedList, bed);
	    binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
	    }
	}
    }
else
    {
    errAbort("%s: unrecognized track type %s", otherTable, otherType);
    }
sqlFreeResult(&sr);

/* Scan through gene preds looking for best overlap if any. */
sr = hChromQuery(tConn, geneTable, chrom, extra, &rowOffset);
numCols = sqlCountColumns(sr);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gp = NULL;
    struct bed *bed = NULL;
    char *name = NULL;
    if(geneTableBed) 
	{
	bed = bedLoadN(row+rowOffset, numCols-rowOffset);
	gp = bedToGenePred(bed);
	bedFree(&bed);
	}
    else
	gp = genePredLoad(row+rowOffset);
    name = gp->name;
    if (!hashLookup(dupeHash, name))	/* Only take first occurrence. */
	{
	if (doAll)
	    {
	    struct binElement *el, *elList = binKeeperFind(bk, gp->txStart, gp->txEnd);
	    for (el = elList; el != NULL; el = el->next)
		{
		bed = el->val;
		if (gpBedOverlap(gp, cdsOnly, intronsToo, bed) > 0)
		    {
		    outTwo(f, lookupHash, gp->name, bed->name);
		    hashAdd(dupeHash, name, NULL);
		    }
		}
	    }
	else
	    {
	    if ((bed = mostOverlappingBed(bk, gp)) != NULL)
		{
		outTwo(f, lookupHash, gp->name, bed->name);
		hashAdd(dupeHash, name, NULL);
		}
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

void hgMapTableToGene(char *database, struct sqlConnection *conn, struct sqlConnection *tConn,
	char *geneTable, char *geneTableType,
	char *otherTable, char *otherType, char *outTable,
	struct hash *lookupHash)
/* hgMapTableToGene - Create a table that maps geneTable to otherTable, 
 * choosing the best single item in otherTable for each genePred. */
{
/* Open tab file and database loop through each chromosome writing to it. */
struct slName *chromList, *chrom;
char *tempDir = ".";
FILE *f;
boolean doAll = optionExists("all");

/* Process each strand of each chromosome into tab-separated file. */
if (!createOnly)
    {
    struct hash *dupeHash = newHash(16);
    f = hgCreateTabFile(tempDir, outTable);
    chromList = hAllChromNamesDb(database);
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
	{
	verbose(2, "%s\n", chrom->name);
	oneChromStrandTrackToGene(database, conn, tConn, chrom->name, '+', geneTable, geneTableType,  
	    otherTable, otherType, dupeHash, doAll, lookupHash, f);
	oneChromStrandTrackToGene(database, conn, tConn, chrom->name, '-', geneTable, geneTableType,
	    otherTable, otherType, dupeHash, doAll, lookupHash, f);
	}
    hashFree(&dupeHash);
    }

/* Create and load table from tab file. */
if (createOnly)
    {
    createTable(tConn, outTable, !doAll);
    }
else if (!optionExists("noLoad"))
    {
    createTable(tConn, outTable, !doAll);
    hgLoadTabFile(tConn, tempDir, outTable, &f);
    hgRemoveTabFile(tempDir, outTable);
    }

}

char *tdbType(struct sqlConnection *conn, char *track)
/* Return track field from TDB. */
{
char query[512];
char *type, typeBuf[128];
safef(query, sizeof(query), "select type from %s where tableName = '%s'", trackDb, track);
type = sqlQuickQuery(conn, query, typeBuf, sizeof(typeBuf));
if (type == NULL)
    errAbort("Can't find track %s in trackDb", track);
return cloneString(type);
}

struct hash *hashTwoColumns(char *fileName)
/* Make up a hash out of a two column file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct hash *hash = hashNew(17);
while (lineFileRow(lf, row))
    hashAdd(hash, row[0], cloneString(row[1]));
lineFileClose(&lf);
return hash;
}

void hgMapToGene(char *database, char *track, char *geneTrack, char *newTable)
/* hgMapToGene - Map a track to a genePred track.. */
{
char *tempDb = optionVal("tempDb", database);
struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *tConn = sqlConnect(tempDb);
char *type = optionVal("type", NULL);
char *lookupFile = optionVal("lookup", NULL);
struct hash *lookupHash = NULL;
char *geneTableType = optionVal("geneTableType", NULL);
if (lookupFile != NULL)
    lookupHash = hashTwoColumns(lookupFile);
if (type == NULL)
    type = tdbType(conn, track);

/* If geneTableType not specified query database. */
if(geneTableType == NULL)
    geneTableType = tdbType(conn, geneTrack);
    
if (!startsWith("genePred", geneTableType) && !startsWith("bed", geneTableType))
    errAbort("%s is neither a genePred or bed type track", geneTrack);
hgMapTableToGene(database, conn, tConn, geneTrack, geneTableType, track, type, newTable, lookupHash);
sqlDisconnect(&conn);
sqlDisconnect(&tConn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
cdsOnly = optionExists("cds");
intronsToo = optionExists("intronsToo");
createOnly = optionExists("createOnly");
prefix = optionVal("prefix", NULL);
trackDb = cfgOption("db.trackDb");
if(trackDb == NULL)
    trackDb = "trackDb";

if (argc != 5)
    usage();
hgMapToGene(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
