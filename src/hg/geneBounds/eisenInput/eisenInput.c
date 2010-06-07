/* eisenInput - Create input for Eisen-style cluster program. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "binRange.h"
#include "hdb.h"
#include "bed.h"
#include "genePred.h"
#include "refLink.h"
#include "expRecord.h"

static char const rcsid[] = "$Id: eisenInput.c,v 1.5 2008/09/03 19:18:35 markd Exp $";

/* Some variables we should probably let people set from the
 * command line. */
char *expTrack = "affyRatio";	/* Values positioned on chromosome. */
char *expRecordTable = "affyExps";  /* Extra expression info . */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eisenInput - Create input for Eisen-style cluster program\n"
  "usage:\n"
  "   eisenInput database output.txt\n"
  );
}

struct expRecord * loadExpRecord(char *table, char *database)
/* Load everything from an expRecord table in the
   specified database, usually hgFixed instead of hg7, hg8, etc. */
{
struct sqlConnection *conn = sqlConnect(database);
char query[256];
struct expRecord *erList = NULL;
snprintf(query, sizeof(query), "select * from %s", table);
erList = expRecordLoadByQuery(conn, query);
sqlDisconnect(&conn);
return erList;
}


struct bed *loadBed(char *database, char *chrom, char *track, int bedN, struct binKeeper *bk)
/* Load in info from a bed track to bk. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
struct bed *list = NULL, *el;

sr = hChromQuery(conn, track, chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = bedLoadN(row + rowOffset, bedN);
    binKeeperAdd(bk, el->chromStart, el->chromEnd, el);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
return list;
}

struct genePred *loadGenePred(char *database, char *chrom, char *track, struct binKeeper *bk)
/* Load in a gene prediction track to bk. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
struct genePred *list = NULL, *el;

sr = hChromQuery(conn, track, chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = genePredLoad(row + rowOffset);
    binKeeperAdd(bk, el->txStart, el->txEnd, el);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
return list;
}

struct refLink *loadRefLink(char *database, struct hash *hash)
/* Load refLink into hash keyed by mrna accession and
 * return list too. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct refLink *list = NULL, *el;

sr = sqlGetResult(conn, "select * from refLink");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = refLinkLoad(row);
    hashAdd(hash, el->mrnaAcc, el);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
return list;
}

struct rangeInfo
/* Information on a range. */
    {
    struct rangeInfo *next;	/* Next in list. */
    char *id;			/* Unique id string, no allocated here. */
    char *commonName;		/* Associated gene name if any. Not allocated here. */
    struct bed *range;	/* Underlying range. Not allocated here.*/
    boolean isSplit;		/* True if one exp hits this and another. */
    };

char *findCommonName(struct bed *range, struct binKeeper *knownBk,
	struct hash *refLinkHash)
/* Try and find a common name for range based on overlap with
 * known genes. */
{
struct binElement *beList=NULL, *be;
struct refLink *link = NULL;
struct genePred *gp;
int matchCount = 0;

beList = binKeeperFind(knownBk, range->chromStart, range->chromEnd);
for (be = beList; be != NULL; be = be->next)
    {
    gp = be->val;
    if (gp->strand[0] == range->strand[0])
        {
	++matchCount;
	link = hashFindVal(refLinkHash, gp->name);
	}
    }
slFreeList(&beList);
if (matchCount == 1 && link != 0)
    return link->name;
else
    return range->name;
}

void writeHeader(FILE *f, struct bed *exp, struct hash *erHash)
/* Write out expression data header. */
{
int i;
struct expRecord *er;
fprintf(f, "YORF\tNAME\tGWEIGHT");
for (i=0; i< exp->expCount; ++i)
    {
    char sid[16];
    snprintf(sid, sizeof(sid), "%u", exp->expIds[i]);
    er = hashMustFindVal(erHash, sid);
    fprintf(f, "\t%s", er->name);
    }
fprintf(f, "\n");
}

void outputAveraged(FILE *f, struct rangeInfo *ri, struct hash *erHash,
	struct binElement *beList)
/* Write out average expresssion value. */
{
static FILE *oldFile = NULL;
static int colCount = 0;
float *scores = NULL;
struct binElement *be;
struct bed *exp;
int elCount = 0;
float elScale;
int i;

if (beList == NULL)
    return;


/* Calculate score averages. */
for (be = beList; be != NULL; be = be->next)
    {
    ++elCount;
    exp = be->val;

    /* Write header if this is the first time through. */
    if (f != oldFile)
	{
	writeHeader(f, exp, erHash);
	oldFile = f;
	colCount = exp->expCount;
	}

    if (scores == NULL)
        AllocArray(scores, colCount);
	
    /* Make sure number of columns are consistent. */
    if (colCount != exp->expCount)
	errAbort("Not all column counts are the same (%d vs %d)", 
	    colCount, exp->expCount);

    for (i=0; i<colCount; ++i)
	scores[i] += exp->expScores[i];
    }

/* Write line of output. */
fprintf(f, "%s\t%s\t1", ri->id, ri->commonName);
elScale = 1.0/elCount;
for (i=0; i<colCount; ++i)
    fprintf(f, "\t%f", scores[i] * elScale);
fprintf(f, "\n");

/* Clean up. */
freez(&scores);
}

void oneChromInput(char *database, char *chrom, int chromSize, 	
	char *rangeTrack, char *expTrack, 
	struct hash *refLinkHash, struct hash *erHash, FILE *f)
/* Read in info for one chromosome. */
{
struct binKeeper *rangeBk = binKeeperNew(0, chromSize);
struct binKeeper *expBk = binKeeperNew(0, chromSize);
struct binKeeper *knownBk = binKeeperNew(0, chromSize);
struct bed *rangeList = NULL, *range;
struct bed *expList = NULL;
struct genePred *knownList = NULL;
struct rangeInfo *riList = NULL, *ri;
struct hash *riHash = hashNew(0); /* rangeInfo values. */
struct binElement *rangeBeList = NULL, *rangeBe, *beList = NULL, *be;

/* Load up data from database. */
rangeList = loadBed(database, chrom, rangeTrack, 12, rangeBk);
expList = loadBed(database, chrom, expTrack, 15, expBk);
knownList = loadGenePred(database, chrom, "refGene", knownBk);

/* Build range info basics. */
rangeBeList = binKeeperFindAll(rangeBk);
for (rangeBe = rangeBeList; rangeBe != NULL; rangeBe = rangeBe->next)
    {
    range = rangeBe->val;
    AllocVar(ri);
    slAddHead(&riList, ri);
    hashAddSaveName(riHash, range->name, ri, &ri->id);
    ri->range = range;
    ri->commonName = findCommonName(range, knownBk, refLinkHash);
    }
slReverse(&riList);

/* Mark split ones. */
beList = binKeeperFindAll(expBk);
for (be = beList; be != NULL; be = be->next)
    {
    struct bed *exp = be->val;
    struct binElement *subList = binKeeperFind(rangeBk, 
    	exp->chromStart, exp->chromEnd);
    if (slCount(subList) > 1)
        {
	struct binElement *sub;
	for (sub = subList; sub != NULL; sub = sub->next)
	    {
	    struct bed *range = sub->val;
	    struct rangeInfo *ri = hashMustFindVal(riHash, range->name);
	    ri->isSplit = TRUE;
	    }
	}
    slFreeList(&subList);
    }

/* Output the nice ones: not split and having some expression info. */
for (ri = riList; ri != NULL; ri = ri->next)
    {
    if (!ri->isSplit)
        {
	struct bed *range =  ri->range;
	beList = binKeeperFind(expBk, range->chromStart, range->chromEnd);
	if (beList != NULL)
	    outputAveraged(f, ri, erHash, beList);
	slFreeList(&beList);
	}
    }

/* Clean up time! */
freeHash(&riHash);
genePredFreeList(&knownList);
bedFree(&rangeList);
bedFree(&expList);
slFreeList(&rangeBeList);
slFreeList(&beList);
slFreeList(&riList);
binKeeperFree(&rangeBk);
binKeeperFree(&expBk);
binKeeperFree(&knownBk);
}

void eisenInput(char *database, char *outFile)
/* eisenInput - Create input for Eisen-style cluster program. */
{
struct slName *chromList = NULL, *chromEl;
FILE *f = mustOpen(outFile, "w");
char *chrom;
struct hash *refLinkHash = hashNew(0);
struct refLink *refLinkList;
struct hash *erHash = hashNew(0);
struct expRecord *erList = NULL, *er;


/* Load info good for all chromosomes. */
refLinkList = loadRefLink(database, refLinkHash);
erList = loadExpRecord(expRecordTable, "hgFixed");
for (er = erList; er != NULL; er = er->next)
    {
    char sid[16];
    snprintf(sid, sizeof(sid), "%u", er->id);
    hashAdd(erHash, sid, er);
    }

/* Do it chromosome by chromosome. */
chromList = hAllChromNames(database);
for (chromEl = chromList; chromEl != NULL; chromEl = chromEl->next)
    {
    chrom = chromEl->name;
    uglyf("%s\n", chrom);
    oneChromInput(database, chrom, hChromSize(database, chrom), "rnaCluster", expTrack, 
    	refLinkHash, erHash, f);
    }

/* Cleanup time! */
expRecordFreeList(&erList);
freeHash(&erHash);
refLinkFreeList(&refLinkList);
freeHash(&refLinkHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
eisenInput(argv[1], argv[2]);
return 0;
}
