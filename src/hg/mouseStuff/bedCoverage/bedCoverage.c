/* bedCoverage - Analyse coverage by bed files - chromosome by chromosome 
 * and genome-wide.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "chromInfo.h"
#include "agpFrag.h"
#include "memalloc.h"

static char const rcsid[] = "$Id: bedCoverage.c,v 1.8 2008/09/03 19:20:33 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedCoverage - Analyse coverage by bed files - chromosome by \n"
  "chromosome and genome-wide.\n"
  "usage:\n"
  "   bedCoverage database bedFile\n"
  "Note bed file must be sorted by chromosome\n"
  "   -restrict=restrict.bed Restrict to parts in restrict.bed\n"
  );
}

struct chromInfo *getAllChromInfo(char *database)
/* Return list of info for all chromosomes. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct chromInfo *ci, *ciList = NULL;

sr = sqlGetResult(conn, "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    ci = chromInfoLoad(row);
    slAddHead(&ciList, ci);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&ciList);
return ciList;
}

#define maxCover 100    /* Maximum coverage we track. */
#define restricted 255	/* Special value for masked out. */

struct simpleRange
/* A range inside of a chromosome. */
   {
   struct simpleRange *next;
   int start;	/* Start (zero based) */
   int end;	/* End (not included) */
   };

struct chromSizes
/* Sizes of each chromosome, with and without gaps. */
   {
   struct chromSizes *next;
   char *name;	/* Allocated in hash */
   double seqSize;   /* Size without N's. */
   double totalSize; /* Size with N's. */
   double unrestrictedSize;	/* Size unrestricted. */
   double totalDepth;  /* Sum of coverage of all bases. */
   double totalCov;    /* Sum of bases covered at least once. */

   double histogram[maxCover+1]; /* Coverage histogram. */
   boolean completed;   /* True if completed. */
   struct simpleRange *restrictList;	/* List of ranges to restrict to. */
   };


void addRestrictions(struct hash *hash, char *fileName)
/* Add restrictions from bed file */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];
int count = 0;
while (lineFileRow(lf, row))
    {
    struct chromSizes *cs = hashMustFindVal(hash, row[0]);
    struct simpleRange *r;
    AllocVar(r);
    r->start = lineFileNeedNum(lf, row, 1);
    r->end = lineFileNeedNum(lf, row, 2);
    if (r->start < 0 || r->end > cs->totalSize)
        errAbort("%d-%d doesn't fit into %s size %ld line %d of %s",
		 r->start, r->end, row[0], (long)(cs->totalSize),
		 lf->lineIx, lf->fileName);
    slAddHead(&cs->restrictList, r);
    ++count;
    }
printf("Added %d restrictions from %s\n", count, fileName);
}

void showStats(struct chromSizes *cs)
/* Print out stats. */
{
int i, j;
printf("%-6s %9.0f depth %10.0f %5.2f anyCov %10.0f %5.2f%%\n", 
	cs->name, cs->unrestrictedSize, 
	cs->totalDepth, 100.0 * cs->totalDepth/cs->unrestrictedSize,
	cs->totalCov, 100.0 * cs->totalCov/cs->unrestrictedSize);
for (i=1; i<10; ++i)
    {
    double sum = cs->histogram[i];
    printf("%2d  %10.0f %5.3f%%\n", i, sum, 100.0 * sum/cs->unrestrictedSize);
    }
for (i=0; i<100; i += 10)
    {
    double sum = 0;
    for (j=0; j<10; ++j)
        {
	int ix = i+j;
	sum += cs->histogram[ix];
	}
    printf("%2d to %2d:  %10f %5.3f%%\n", i, i+9, sum, 100.0 * sum/cs->unrestrictedSize);
    }
printf(">=100  %10f %5.3f%%\n", cs->histogram[100], 
	100.0 * cs->histogram[100]/cs->unrestrictedSize);
printf("\n");
}

void shortStats(struct chromSizes *cs)
/* Display short form of stats. */
{
int i;
double totalCov, twoOrMore, tenOrMore=0, hundredOrMore=0;
totalCov = cs->totalCov;
twoOrMore = totalCov - cs->histogram[1];
for (i=10; i<=100; ++i)
    tenOrMore += cs->histogram[i];
hundredOrMore = cs->histogram[100];
printf("%s\t%1.2f\t%1.2f\t%1.2f\t%1.2f\n", 
	cs->name, 
	totalCov * 100.0 / cs->unrestrictedSize, 
	twoOrMore * 100.0 / cs->unrestrictedSize, 
	tenOrMore * 100.0 / cs->unrestrictedSize, 
	hundredOrMore * 100.0 / cs->unrestrictedSize);
}

void getChromSizes(char *database, struct hash **retHash, 
	struct chromSizes **retList)
/* Return hash of chromSizes. */
{
struct sqlConnection *conn = hAllocConn(database);
struct chromInfo *ci, *ciList = getAllChromInfo(database);
struct sqlResult *sr;
char **row;
struct chromSizes *cs, *csList = NULL;
struct hash *hash = newHash(8);
int rowOffset;

for (ci = ciList; ci != NULL; ci = ci->next)
    {
    AllocVar(cs);
    hashAddSaveName(hash, ci->chrom, cs, &cs->name);
    slAddHead(&csList, cs);
    cs->totalSize = ci->size;
    sr = hChromQuery(conn, "gold", ci->chrom, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	struct agpFrag frag;
	agpFragStaticLoad(row + rowOffset, &frag);
	cs->seqSize += frag.chromEnd - frag.chromStart;
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
slReverse(&csList);
*retHash = hash;
*retList = csList;
}

void incNoOverflow(UBYTE *cov, int size)
/* Add one to each member of cov, so long as it's
 * not over maxCover. */
{
UBYTE c;
while (--size >= 0)
   {
   c = *cov;
   if (c < maxCover)
       {
       ++c;
       *cov = c;
       }
   ++cov;
   }
}

void closeChromCov(struct lineFile *lf, struct chromSizes *cs, UBYTE **pCov)
/* Fill in cs->histogram from cov, and free cov. */
{
UBYTE *cov = *pCov, c;
int i, size = cs->totalSize;
for (i=0; i<size; ++i)
   {
   c = cov[i];
   if (c > 0 && c != restricted)
       {
       cs->histogram[c] += 1;
       cs->totalCov += 1;
       }
   }
freez(pCov);
if (cs->completed)
     errAbort("Bed file not sorted by chromosome line %d of %s.", lf->lineIx, lf->fileName);
cs->completed = TRUE;
showStats(cs);
}


void restrictCov(UBYTE *cov, int size, struct simpleRange *restrictList)
/* Set areas that are restricted (not in restrict list) to restricted
 * value. Assumes cov is all zero to begin with. */
{
struct simpleRange *r;
memset(cov, restricted, size);
for (r = restrictList; r != NULL; r = r->next)
    {
    assert(r->start >= 0);
    assert(r->end <= size);
    memset(cov + r->start, 0, r->end - r->start);
    }
}

void restrictGaps(char *database, UBYTE *cov, int size, char *chrom)
/* Mark gaps as off-limits. */
{
int rowOffset;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = hChromQuery(conn, "gap", chrom, NULL, &rowOffset);
char **row;
int s,e;

while ((row = sqlNextRow(sr)) != NULL)
    {
    s = sqlUnsigned(row[1+rowOffset]);
    e = sqlUnsigned(row[2+rowOffset]);
    assert(s >= 0);
    assert(e <= size);
    memset(cov + s, restricted, e - s);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

int calcUnrestrictedSize(UBYTE *cov, int size)
/* Figure out number of bases not restricted. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (cov[i] != restricted)
        ++count;
return count;
}


void scanBed(char *database, char *fileName, struct hash *chromHash, boolean restrict)
/* Scan through bed file (which must be sorted by
 * chromosome) and fill in coverage histograms on 
 * each chromosome. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct chromSizes *lastCs = NULL, *cs = NULL;
char *chrom;
int start, end, size;
char *row[3];
UBYTE *cov = NULL;

while (lineFileRow(lf, row))
    {
    chrom = row[0];
    start = lineFileNeedNum(lf, row, 1);
    end = lineFileNeedNum(lf, row, 2);
    size = end - start;
    cs = hashMustFindVal(chromHash, chrom);
    if (cs != lastCs)
        {
	if (lastCs != NULL)
	    closeChromCov(lf, lastCs, &cov);
	AllocArray(cov, cs->totalSize);
	if (restrict)
	    restrictCov(cov, cs->totalSize, cs->restrictList);
	restrictGaps(database, cov, cs->totalSize, chrom);
	cs->unrestrictedSize = calcUnrestrictedSize(cov, cs->totalSize);
	lastCs = cs;
	}
    if (end > cs->totalSize)
        errAbort("End %d past end %ld of %s", end, (long)(cs->totalSize), cs->name);
    incNoOverflow(cov+start, size);
    cs->totalDepth += size;
    }
closeChromCov(lf, cs, &cov);
lineFileClose(&lf);
}

struct chromSizes *genoSize(struct chromSizes *chromSizes)
/* Sum up all chromSizes and return result. */
{
struct chromSizes *cs, *g;
int i;
AllocVar(g);
g->name = "all";
for (cs = chromSizes; cs != NULL; cs = cs->next)
    {
    g->unrestrictedSize += cs->unrestrictedSize;
    g->seqSize += cs->seqSize;
    g->totalSize += cs->totalSize;
    g->totalDepth += cs->totalDepth;
    g->totalCov += cs->totalCov;
    for (i=0; i<=maxCover; ++i)
        g->histogram[i] += cs->histogram[i];
    }
return g;
}

void bedCoverage(char *database, char *bedFile, char *restrictFile)
/* bedCoverage - Analyse coverage by bed files - chromosome by 
 * chromosome and genome-wide.. */
{
struct chromSizes *cs, *csList = NULL;
struct hash *chromHash = NULL;
struct chromSizes *genome;

getChromSizes(database, &chromHash, &csList);
if (restrictFile != NULL)
    addRestrictions(chromHash, restrictFile);
scanBed(database, bedFile, chromHash, restrictFile != NULL);
genome = genoSize(csList);
showStats(genome);

printf("\n");
shortStats(genome);
for (cs = csList; cs != NULL; cs = cs->next)
    shortStats(cs);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(350000000);
optionHash(&argc, argv);
if (argc != 3)
    usage();
bedCoverage(argv[1], argv[2], optionVal("restrict", NULL));
return 0;
}
